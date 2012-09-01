/*
   Copyright (c) 2008-2012 Sebastian Trueg <trueg@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "folder.h"
#include "folderconnection.h"
#include "queryservice.h"
#include "countqueryrunnable.h"
#include "searchrunnable.h"

#include "resource.h"
#include "resourcemanager.h"

#include "andterm.h"
#include "orterm.h"
#include "resourcetypeterm.h"
#include "optionalterm.h"
#include "comparisonterm.h"
#include "negationterm.h"
#include "resourcewatcher.h"

#include <Soprano/Model>

#include <KDebug>

#include <QtCore/QThreadPool>
#include <QtCore/QMutexLocker>
#include <QtDBus/QDBusConnection>


Nepomuk2::Query::Folder::Folder( Soprano::Model* model, const Query& query, QObject* parent )
    : QObject( parent ),
      m_isSparqlQueryFolder( false ),
      m_query( query ),
      m_model( model ),
      m_currentSearchRunnable( 0 ),
      m_currentCountQueryRunnable( 0 ),
      m_runnableMutex(QMutex::Recursive)
{
    init();
}


Nepomuk2::Query::Folder::Folder( Soprano::Model* model, const QString& query,
                                 const RequestPropertyMap& requestProps, QObject* parent )
    : QObject( parent ),
      m_isSparqlQueryFolder( true ),
      m_sparqlQuery( query ),
      m_requestProperties( requestProps ),
      m_model( model ),
      m_currentSearchRunnable( 0 ),
      m_currentCountQueryRunnable( 0 ),
      m_runnableMutex(QMutex::Recursive)
{
    init();
}

namespace {
    using namespace Nepomuk2::Query;
    using namespace Nepomuk2;

    void initWatcherForGroupTerms(ResourceWatcher* watcher, const GroupTerm& groupTerm, bool& emptyProperty);

    void initWatcherForTerm(ResourceWatcher* watcher, const Term& term, bool &emptyProperty ) {
        if( term.isAndTerm() )
            initWatcherForGroupTerms( watcher, term.toAndTerm(), emptyProperty );
        else if( term.isOrTerm() )
            initWatcherForGroupTerms( watcher, term.toOrTerm(), emptyProperty );
        else if( term.isOptionalTerm() )
            initWatcherForTerm( watcher, term.toOptionalTerm().subTerm(), emptyProperty );
        else if( term.isNegationTerm() )
            initWatcherForTerm( watcher, term.toNegationTerm().subTerm(), emptyProperty );
        else if( term.isResourceTypeTerm() )
            watcher->addType( term.toResourceTypeTerm().type() );
        else if( term.isComparisonTerm() ) {
            const QUrl prop = term.toComparisonTerm().property().uri();
            if( prop.isEmpty() ) {
                emptyProperty = true;
            }
            else {
                watcher->addProperty( term.toComparisonTerm().property().uri() );
            }
        }
    }

    void initWatcherForGroupTerms(ResourceWatcher* watcher, const GroupTerm& groupTerm, bool &emptyProperty) {
        QList<Term> terms = groupTerm.subTerms();
        foreach(const Term& term, terms) {
            initWatcherForTerm( watcher, term, emptyProperty );
        }
    }

    void intiWatcherForQuery(ResourceWatcher* watcher, const Nepomuk2::Query::Query& query) {
        // The empty property is for comparison terms which do not have a property
        // in that case we want to monitor all properties
        bool emptyProperty = false;
        initWatcherForTerm( watcher, query.term(), emptyProperty );
        if( emptyProperty )
            watcher->setProperties( QList<Types::Property>() );
    }
}


void Nepomuk2::Query::Folder::init()
{
    m_resultCount = -1;
    m_initialListingDone = false;
    m_storageChanged = false;

    m_updateTimer.setSingleShot( true );
    m_updateTimer.setInterval( 2000 );

    ResourceWatcher* watcher = new ResourceWatcher( this );
    intiWatcherForQuery( watcher, m_query );

    connect( watcher, SIGNAL(propertyAdded(Nepomuk2::Resource,Nepomuk2::Types::Property,QVariant)),
             this, SLOT(slotStorageChanged()) );
    connect( watcher, SIGNAL(propertyRemoved(Nepomuk2::Resource,Nepomuk2::Types::Property,QVariant)),
             this, SLOT(slotStorageChanged()) );
    connect( watcher, SIGNAL(resourceCreated(Nepomuk2::Resource,QList<QUrl>)),
             this, SLOT(slotStorageChanged()) );
    connect( watcher, SIGNAL(resourceRemoved(QUrl,QList<QUrl>)),
             this, SLOT(slotStorageChanged()) );
    connect( watcher, SIGNAL(resourceTypeAdded(Nepomuk2::Resource,Nepomuk2::Types::Class)),
             this, SLOT(slotStorageChanged()) );
    connect( watcher, SIGNAL(resourceTypeRemoved(Nepomuk2::Resource,Nepomuk2::Types::Class)),
             this, SLOT(slotStorageChanged()) );
    watcher->start();

    connect( &m_updateTimer, SIGNAL( timeout() ),
             this, SLOT( slotUpdateTimeout() ) );
}


Nepomuk2::Query::Folder::~Folder()
{
    QMutexLocker lock(&m_runnableMutex);
    if( m_currentSearchRunnable )
        m_currentSearchRunnable->cancel();
    if( m_currentCountQueryRunnable )
        m_currentCountQueryRunnable->cancel();

    // cannot use qDeleteAll since deleting a connection changes m_connections
    while ( !m_connections.isEmpty() )
        delete m_connections.first();
}


void Nepomuk2::Query::Folder::update()
{
    QMutexLocker lock(&m_runnableMutex);
    if ( !m_currentSearchRunnable ) {
        m_currentSearchRunnable = new SearchRunnable( m_model, this );
        QueryService::searchThreadPool()->start( m_currentSearchRunnable, 1 );

        // we only need the count for initialListingDone
        // count with a limit is pointless since Virtuoso will ignore the limit
        if ( !m_initialListingDone &&
             !m_isSparqlQueryFolder &&
             m_query.limit() == 0 ) {
            m_currentCountQueryRunnable = new CountQueryRunnable( m_model, this );
            QueryService::searchThreadPool()->start( m_currentCountQueryRunnable, 0 );
        }
    }
}


QList<Nepomuk2::Query::Result> Nepomuk2::Query::Folder::entries() const
{
    QMutexLocker lock(&m_runnableMutex);
    return m_results.values();
}


bool Nepomuk2::Query::Folder::initialListingDone() const
{
    QMutexLocker lock(&m_runnableMutex);
    return m_initialListingDone;
}


QString Nepomuk2::Query::Folder::sparqlQuery() const
{
    if ( !m_isSparqlQueryFolder )
        return m_query.toSparqlQuery();
    else
        return m_sparqlQuery;
}


Nepomuk2::Query::RequestPropertyMap Nepomuk2::Query::Folder::requestPropertyMap() const
{
    if ( !m_isSparqlQueryFolder )
        return m_query.requestPropertyMap();
    else
        return m_requestProperties;
}


// called from SearchRunnable in the main thread
void Nepomuk2::Query::Folder::addResults( const QList<Nepomuk2::Query::Result>& results )
{
    QMutexLocker lock(&m_runnableMutex);

    QSet<Result> newResults;
    Q_FOREACH( const Result& result, results ) {
        if ( !m_results.contains( result.resource().uri() ) ) {
            newResults.insert( result );
        }
    }

    Q_FOREACH(const Result& result, results) {
        if ( !m_newResults.contains( result.resource().uri() ) ) {
            m_newResults.insert(result.resource().uri(), result);
        }
    }

    if( !newResults.isEmpty() ) {
        emit newEntries( newResults.values() );
    }
}


// called from SearchRunnable in the main thread
void Nepomuk2::Query::Folder::listingFinished()
{
    QMutexLocker lock(&m_runnableMutex);

    m_currentSearchRunnable = 0;

    // inform about removed items
    QList<Result> removedResults;

    // legacy removed results
    foreach( const Result& result, m_results ) {
        if ( !m_newResults.contains( result.resource().uri() ) ) {
            removedResults << result;
            emit entriesRemoved( QList<QUrl>() << KUrl(result.resource().uri()).url() );
        }
    }

    // new removed results which include all the details to be used for optimizations
    if( !removedResults.isEmpty() ) {
        emit entriesRemoved( removedResults );
    }

    // reset
    m_results = m_newResults;
    m_newResults.clear();

    if ( !m_initialListingDone ) {
        kDebug() << "Listing done. Total:" << m_results.count();
        m_initialListingDone = true;
        emit finishedListing();
    }

    // make sure we do not update again right away
    // but we need to do it from the main thread but this
    // method is called sync from the SearchRunnable
    QMetaObject::invokeMethod( &m_updateTimer, "start", Qt::QueuedConnection );
}


void Nepomuk2::Query::Folder::slotStorageChanged()
{
    QMutexLocker lock(&m_runnableMutex);
    if ( !m_updateTimer.isActive() && !m_currentSearchRunnable ) {
        update();
    }
    else {
        m_storageChanged = true;
    }
}


// if there was a change in the nepomuk store we update
void Nepomuk2::Query::Folder::slotUpdateTimeout()
{
    QMutexLocker lock(&m_runnableMutex);
    if ( m_storageChanged && !m_currentSearchRunnable ) {
        m_storageChanged = false;
        update();
    }
}


// called from CountQueryRunnable in the search thread
void Nepomuk2::Query::Folder::countQueryFinished( int count )
{
    QMutexLocker lock(&m_runnableMutex);

    m_currentCountQueryRunnable = 0;

    m_resultCount = count;
    kDebug() << m_resultCount;
    if( count >= 0 )
        emit resultCount( m_resultCount );
}


void Nepomuk2::Query::Folder::addConnection( FolderConnection* conn )
{
    Q_ASSERT( conn != 0 );
    Q_ASSERT( !m_connections.contains( conn ) );

    m_connections.append( conn );
}


void Nepomuk2::Query::Folder::removeConnection( FolderConnection* conn )
{
    Q_ASSERT( conn != 0 );
    Q_ASSERT( m_connections.contains( conn ) );

    m_connections.removeAll( conn );

    if ( m_connections.isEmpty() ) {
        kDebug() << "Folder unused. Deleting.";
        emit aboutToBeDeleted( this );
        deleteLater();
    }
}


QList<Nepomuk2::Query::FolderConnection*> Nepomuk2::Query::Folder::openConnections() const
{
    return m_connections;
}


uint Nepomuk2::Query::qHash( const Result& result )
{
    // we only use this to ensure that we do not emit duplicates
    return qHash(result.resource().uri());
}

#include "folder.moc"
