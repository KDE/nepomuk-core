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

#include "folderconnection.h"
#include "folder.h"
#include "queryadaptor.h"

#include "resource.h"
#include "result.h"

#include <QtCore/QStringList>
#include <QtDBus/QDBusServiceWatcher>

#include <KDebug>
#include <kdbusconnectionpool.h>

Nepomuk2::Query::FolderConnection::FolderConnection( Folder* folder )
    : QObject( folder ),
      m_folder( folder )
{
    m_folder->addConnection( this );
}


Nepomuk2::Query::FolderConnection::~FolderConnection()
{
    m_folder->removeConnection( this );
}


void Nepomuk2::Query::FolderConnection::list()
{
    kDebug();

    m_folder->disconnect( this );
    connect( m_folder, SIGNAL( newEntries( QList<Nepomuk2::Query::Result> ) ),
             this, SIGNAL( newEntries( QList<Nepomuk2::Query::Result> ) ) );
    connect( m_folder, SIGNAL( entriesRemoved( QList<Nepomuk2::Query::Result> ) ),
             this, SLOT( slotEntriesRemoved( QList<Nepomuk2::Query::Result> ) ) );

    // report cached entries
    if ( !m_folder->entries().isEmpty() ) {
        emit newEntries( m_folder->entries() );
    }

    // report listing finished or connect to the folder
    if ( m_folder->initialListingDone() ) {
        emit finishedListing();
    }
    else {
        // We do NOT connect to slotFinishedListing!
        connect( m_folder, SIGNAL( finishedListing() ),
                 this, SIGNAL( finishedListing() ) );

        // make sure the search has actually been started
        m_folder->update();
    }

    // report the count or connect to the signal
    if( m_folder->getResultCount() >= 0 ) {
        emit resultCount( m_folder->getResultCount() );
    }
    else {
        connect( m_folder, SIGNAL( resultCount( int ) ),
                 this, SIGNAL( resultCount( int ) ) );
    }
}


void Nepomuk2::Query::FolderConnection::listen()
{
    m_folder->disconnect( this );

    // if the folder has already finished listing it will only emit
    // changed - exactly what we want
    if ( m_folder->initialListingDone() ) {
        connect( m_folder, SIGNAL( newEntries( QList<Nepomuk2::Query::Result> ) ),
                 this, SIGNAL( newEntries( QList<Nepomuk2::Query::Result> ) ) );
        connect( m_folder, SIGNAL( entriesRemoved( QList<Nepomuk2::Query::Result> ) ),
                 this, SLOT( slotEntriesRemoved( QList<Nepomuk2::Query::Result> ) ) );
        connect( m_folder, SIGNAL( resultCount( int ) ),
                 this, SIGNAL( resultCount( int ) ) );
    }

    // otherwise we need to wait for it finishing the listing
    else {
        connect( m_folder, SIGNAL( finishedListing() ),
                 this, SLOT( slotFinishedListing() ) );
    }
}


void Nepomuk2::Query::FolderConnection::slotEntriesRemoved( const QList<Nepomuk2::Query::Result>& entries )
{
    QStringList uris;
    for ( int i = 0; i < entries.count(); ++i ) {
        uris.append( entries[i].resource().uri().toString() );
    }
    emit entriesRemoved( uris );
    emit entriesRemoved( entries );
}


void Nepomuk2::Query::FolderConnection::slotFinishedListing()
{
    // this slot is only called in listen mode. Once the listing is
    // finished we can start listening for changes
    connect( m_folder, SIGNAL( newEntries( QList<Nepomuk2::Query::Result> ) ),
             this, SIGNAL( newEntries( QList<Nepomuk2::Query::Result> ) ) );
    connect( m_folder, SIGNAL( entriesRemoved( QList<Nepomuk2::Query::Result> ) ),
             this, SLOT( slotEntriesRemoved( QList<Nepomuk2::Query::Result> ) ) );
}


void Nepomuk2::Query::FolderConnection::close()
{
    kDebug();
    deleteLater();
}


bool Nepomuk2::Query::FolderConnection::isListingFinished() const
{
    return m_folder->initialListingDone();
}


QString Nepomuk2::Query::FolderConnection::queryString() const
{
    return m_folder->sparqlQuery();
}


QDBusObjectPath Nepomuk2::Query::FolderConnection::registerDBusObject( const QString& dbusClient, int id )
{
    // create the query adaptor on this connection
    ( void )new QueryAdaptor( this );

    // build the dbus object path from the id and register the connection as a Query dbus object
    const QString dbusObjectPath = QString( "/nepomukqueryservice/query%1" ).arg( id );
    QDBusConnection con = KDBusConnectionPool::threadConnection();
    con.registerObject( dbusObjectPath, this );

    // watch the dbus client for unregistration for auto-cleanup
    m_serviceWatcher = new QDBusServiceWatcher( dbusClient,
                                                con,
                                                QDBusServiceWatcher::WatchForUnregistration,
                                                this );
    connect( m_serviceWatcher, SIGNAL(serviceUnregistered(QString)),
             this, SLOT(close()) );

    // finally return the dbus object path this connection can be found on
    return QDBusObjectPath( dbusObjectPath );
}

#include "folderconnection.moc"
