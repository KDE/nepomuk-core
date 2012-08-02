/*
  Copyright (c) 2008-2010 Sebastian Trueg <trueg@kde.org>

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

#include "queryservice.h"
#include "folder.h"
#include "folderconnection.h"
#include "dbusoperators_p.h"

#include <QtCore/QThreadPool>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusObjectPath>
#include <QtDBus/QDBusMessage>

#include <KPluginFactory>
#include <KUrl>
#include <KDebug>

#include "resourcemanager.h"
#include "types/property.h"
#include "query/query.h"
#include "query/queryparser.h"

Q_DECLARE_METATYPE( QList<QUrl> )

static QThreadPool* s_searchThreadPool = 0;

Nepomuk2::Query::QueryService::QueryService( Soprano::Model* model, QObject* parent )
    : QObject( parent ),
      m_folderConnectionCnt( 0 ),
      m_model( model )
{
    // this looks wrong but there is only one QueryService instance at all time!
    s_searchThreadPool = new QThreadPool( this );
    s_searchThreadPool->setMaxThreadCount( 10 );

    // register types used in the DBus adaptor
    Nepomuk2::Query::registerDBusTypes();

    // register type used to communicate removeEntries between threads
    qRegisterMetaType<QList<QUrl> >();
    qRegisterMetaType<QList<Nepomuk2::Query::Result> >();

    //Register the service
    QLatin1String serviceName("nepomukqueryservice");
    QString dbusName = QString::fromLatin1("org.kde.nepomuk.services.%1").arg( serviceName );

    QDBusConnection bus = QDBusConnection::sessionBus();
    if( !bus.registerService( dbusName ) ) {
        kDebug() << "Failed to register the QueryService .. ";
    }

    // register the service
    // ====================================
    bus.registerObject( '/' + serviceName,
                        this,
                        QDBusConnection::ExportScriptableSlots |
                        QDBusConnection::ExportScriptableProperties |
                        QDBusConnection::ExportAdaptors);
}


Nepomuk2::Query::QueryService::~QueryService()
{
    // cannot use qDeleteAll since deleting a folder changes m_openQueryFolders
    while ( !m_openQueryFolders.isEmpty() )
        delete m_openQueryFolders.begin().value();
    while ( !m_openSparqlFolders.isEmpty() )
        delete m_openSparqlFolders.begin().value();
}


// static
QThreadPool* Nepomuk2::Query::QueryService::searchThreadPool()
{
    return s_searchThreadPool;
}


QDBusObjectPath Nepomuk2::Query::QueryService::query( const QString& query, const QDBusMessage& msg )
{
    Query q = Query::fromString( query );
    if ( !q.isValid() ) {
        // backwards compatibility: in KDE <= 4.5 query() was what desktopQuery() is now
        return desktopQuery( query, msg );
    }
    else {
        kDebug() << "Query request:" << q;
        Folder* folder = getFolder( q );
        return ( new FolderConnection( folder ) )->registerDBusObject( msg.service(), ++m_folderConnectionCnt );
    }
}


QDBusObjectPath Nepomuk2::Query::QueryService::desktopQuery( const QString& query, const QDBusMessage& msg )
{
    Query q = QueryParser::parseQuery( query );
    if( !q.isValid() ) {
        kDebug() << "Invalid desktop query:" << query;
        QDBusConnection::sessionBus().send( msg.createErrorReply( QDBusError::InvalidArgs, i18n("Invalid desktop query: '%1'", query) ) );
        return QDBusObjectPath(QLatin1String("/non/existing/path"));
    }
    else {
        kDebug() << "Query request:" << q;
        Folder* folder = getFolder( q );
        return ( new FolderConnection( folder ) )->registerDBusObject( msg.service(), ++m_folderConnectionCnt );
    }
}


namespace {
    Nepomuk2::Query::RequestPropertyMap decodeRequestPropertiesList( const RequestPropertyMapDBus& requestProps )
    {
        Nepomuk2::Query::RequestPropertyMap rpm;
        for ( RequestPropertyMapDBus::const_iterator it = requestProps.constBegin();
              it != requestProps.constEnd(); ++it )
            rpm.insert( it.key(), KUrl( it.value() ) );
        return rpm;
    }
}

QDBusObjectPath Nepomuk2::Query::QueryService::sparqlQuery( const QString& sparql, const RequestPropertyMapDBus& requestProps, const QDBusMessage& msg )
{
    kDebug() << "Query request:" << sparql << requestProps;

    if( sparql.isEmpty() ) {
        kDebug() << "Invalid SPARQL query:" << sparql;
        QDBusConnection::sessionBus().send( msg.createErrorReply( QDBusError::InvalidArgs, i18n("Invalid SPARQL query: '%1'", sparql) ) );
        return QDBusObjectPath(QLatin1String("/non/existing/path"));
    }
    else {
        // create query folder + connection
        Folder* folder = getFolder( sparql, decodeRequestPropertiesList( requestProps ) );
        return ( new FolderConnection( folder ) )->registerDBusObject( msg.service(), ++m_folderConnectionCnt );
    }
}


Nepomuk2::Query::Folder* Nepomuk2::Query::QueryService::getFolder( const Query& query )
{
    QHash<Query, Folder*>::const_iterator it = m_openQueryFolders.constFind( query );
    if ( it != m_openQueryFolders.constEnd() ) {
        kDebug() << "Recycling folder" << *it;
        return *it;
    }
    else {
        kDebug() << "Creating new search folder for query:" << query;
        Folder* newFolder = new Folder( m_model, query, this );
        connect( newFolder, SIGNAL( aboutToBeDeleted( Nepomuk2::Query::Folder* ) ),
                 this, SLOT( slotFolderAboutToBeDeleted( Nepomuk2::Query::Folder* ) ) );
        m_openQueryFolders.insert( query, newFolder );
        return newFolder;
    }
}


Nepomuk2::Query::Folder* Nepomuk2::Query::QueryService::getFolder( const QString& query, const Nepomuk2::Query::RequestPropertyMap& requestProps )
{
    QHash<QString, Folder*>::const_iterator it = m_openSparqlFolders.constFind( query );
    if ( it != m_openSparqlFolders.constEnd() ) {
        kDebug() << "Recycling folder" << *it;
        return *it;
    }
    else {
        kDebug() << "Creating new search folder for query:" << query;
        Folder* newFolder = new Folder( m_model, query, requestProps, this );
        connect( newFolder, SIGNAL( aboutToBeDeleted( Nepomuk2::Query::Folder* ) ),
                 this, SLOT( slotFolderAboutToBeDeleted( Nepomuk2::Query::Folder* ) ) );
        m_openSparqlFolders.insert( query, newFolder );
        return newFolder;
    }
}


void Nepomuk2::Query::QueryService::slotFolderAboutToBeDeleted( Folder* folder )
{
    kDebug() << folder;
    if ( folder->isSparqlQueryFolder() )
        m_openSparqlFolders.remove( folder->sparqlQuery() );
    else
        m_openQueryFolders.remove( folder->query() );
}

#include "queryservice.moc"
