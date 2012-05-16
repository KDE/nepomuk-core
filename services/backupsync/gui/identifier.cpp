/*
    This file is part of the Nepomuk KDE project.
    Copyright (C) 2010  Vishesh Handa <handa.vish@gmail.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) version 3, or any
   later version accepted by the membership of KDE e.V. (or its
   successor approved by the membership of KDE e.V.), which shall
   act as a proxy defined in Section 6 of version 3 of the license.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "changelog.h"
#include "identifier.h"

#include <QtDBus/QDBusConnection>

#include <QtCore/QMutexLocker>
#include <QtCore/QHashIterator>
#include <QtCore/QFile>

#include <Soprano/Model>
#include <Soprano/Graph>
#include <Soprano/QueryResultIterator>
#include <Soprano/Vocabulary/RDF>
#include <Soprano/Serializer>
#include <Soprano/PluginManager>
#include <Soprano/Util/SimpleStatementIterator>

#include "resourcemanager.h"
#include "resource.h"
#include "variant.h"
#include "nie.h"

#include <KDebug>

Nepomuk2::Identifier::Identifier(QObject* parent): QThread(parent)
{
    //Register DBus interface
    //new IdentifierAdaptor( this );
    //QDBusConnection dbus = QDBusConnection::sessionBus();
    //dbus.registerObject( QLatin1String("/identifier"), this );

    start();
}

Nepomuk2::Identifier* Nepomuk2::Identifier::instance()
{
    static Identifier ident;
    return &ident;
}

Nepomuk2::Identifier::~Identifier()
{
    stop();
    quit();
}


int Nepomuk2::Identifier::process(const Nepomuk2::SyncFile& sf)
{
    m_queueMutex.lock();

    SyncFileIdentifier* identifier = new SyncFileIdentifier( sf );
    int id = identifier->id();
    m_queue.enqueue( identifier );

    m_queueMutex.unlock();
    m_queueWaiter.wakeAll();

    kDebug() << "Processing ID : " << id;
    return id;
}


void Nepomuk2::Identifier::stop()
{
    m_stopped = true;
    m_queueWaiter.wakeAll();
}



void Nepomuk2::Identifier::run()
{
    m_stopped = false;

    while( !m_stopped ) {

        // lock for initial iteration
        m_queueMutex.lock();

        while( !m_queue.isEmpty() ) {

            SyncFileIdentifier* identifier = m_queue.dequeue();

            // unlock after queue utilization
            m_queueMutex.unlock();

            identifier->load();
            identifyAllWithCompletedSignals( identifier );

            emit identificationDone( identifier->id(), identifier->unidentified().size() );

            m_processMutex.lock();
            m_processes[ identifier->id() ] = identifier;
            m_processMutex.unlock();

            // Send the sigals
            foreach( const KUrl & uri, identifier->unidentified() ) {
                emitNotIdentified( identifier->id(), identifier->statements( uri ).toList() );
            }

            foreach( const KUrl & uri, identifier->mappings().uniqueKeys() ) {
                emit identified( identifier->id(), uri.url(), identifier->mappedUri( uri ).url() );
            }

//             if( identifier->allIdentified() ) {
//                 m_processes.remove( identifier->id() );
//                 delete identifier;
//             }

            m_queueMutex.lock();
        }

        // wait for more input
        kDebug() << "Waiting...";
        m_queueWaiter.wait( &m_queueMutex );
        m_queueMutex.unlock();
        kDebug() << "Woke up.";
    }
}


bool Nepomuk2::Identifier::identify(int id, const QString& oldUriString, const QString& newUriString)
{
    QUrl oldUri( oldUriString );
    QUrl newUri( newUriString );

    kDebug() << newUri;
    // Lock the mutex and all
    QMutexLocker lock ( &m_processMutex );

    QHash<int, SyncFileIdentifier*>::iterator it = m_processes.find( id );
    if( it == m_processes.end() )
        return false;

    SyncFileIdentifier* ip = *it;

    if ( oldUri.scheme() != QLatin1String("nepomuk") )
        return false;

    if( newUri.scheme() == QLatin1String("nepomuk") ) {
        ip->forceResource( oldUri, Nepomuk2::Resource(newUri) );
    }
    else if( newUri.scheme() == "file" ) {
        ip->forceResource( oldUri, Nepomuk2::Resource(newUri) );
    }

    m_queueMutex.lock();
    m_queue.enqueue( ip );
    m_queueMutex.unlock();
    m_queueWaiter.wakeAll();

    return true;
}


bool Nepomuk2::Identifier::ignore(int id, const QString& urlString, bool ignoreSub)
{
    KUrl url( urlString );
    // Lock the mutex and all
    QMutexLocker lock ( &m_processMutex );

    QHash<int, SyncFileIdentifier*>::iterator it = m_processes.find( id );
    if( it == m_processes.end() )
        return false;

    SyncFileIdentifier* identifier = *it;
    return identifier->ignore( url, ignoreSub );
}

void Nepomuk2::Identifier::ignoreAll(int id)
{
    QMutexLocker lock ( &m_processMutex );

    QHash<int, SyncFileIdentifier*>::iterator it = m_processes.find( id );
    if( it == m_processes.end() )
        return;

    SyncFileIdentifier* identifier = *it;
    foreach( const KUrl & url, identifier->unidentified() ) {
        identifier->ignore( url, true );
    }
}

void Nepomuk2::Identifier::emitNotIdentified(int id, const QList< Soprano::Statement >& stList)
{
    const Soprano::Serializer* serializer = Soprano::PluginManager::instance()->discoverSerializerForSerialization( Soprano::SerializationNQuads );

    Soprano::Util::SimpleStatementIterator it( stList );
    QString ser;
    QTextStream stream( &ser );
    serializer->serialize( it, stream, Soprano::SerializationNQuads );

    emit notIdentified( id, ser );
}

void Nepomuk2::Identifier::test()
{
    kDebug() << "Test!";
}

void Nepomuk2::Identifier::completeIdentification(int id)
{
    kDebug() << id;

    QMutexLocker lock ( &m_processMutex );

    QHash<int, SyncFileIdentifier*>::iterator it = m_processes.find( id );
    if( it == m_processes.end() )
        return;

    SyncFileIdentifier* identifier = *it;
    m_processes.remove( id );

    ChangeLog log = identifier->convertedChangeLog();
    kDebug() << "ChangeLog of size " << log.size() << " has been converted";
    if( !log.empty() ) {
        kDebug() << "sending ChangeLog of size : " << log.size();
        emit processed( log );
    }

    delete identifier;
}


void Nepomuk2::Identifier::identifyAllWithCompletedSignals(Nepomuk2::SyncFileIdentifier* ident)
{
    int unidentified = ident->unidentified().size();
    float step = 100.0/unidentified;
    float progress = 0;

    emit completed( ident->id(), 0 );
    foreach( const KUrl & url, ident->unidentified() ) {
        ident->identify( url );

        progress += step;
        emit completed( ident->id(), (int)progress );
    }
    emit completed( ident->id(), 100 );
}


#include "identifier.moc"
