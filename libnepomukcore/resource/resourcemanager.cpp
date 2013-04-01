/*
 * This file is part of the Nepomuk KDE project.
 * Copyright (C) 2006-2012 Sebastian Trueg <trueg@kde.org>
 * Copyright (C) 2010-2012 Vishesh Handa <handa.vish@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "resourcemanager.h"
#include "resourcemanager_p.h"
#include "resourcedata.h"
#include "tools.h"
#include "nepomukmainmodel.h"
#include "resource.h"
#include "class.h"
#include "nie.h"
#include "dbustypes.h"
#include "resourcewatcher.h"

#include <kglobal.h>
#include <kdebug.h>
#include <krandom.h>

#include <Soprano/Node>
#include <Soprano/Statement>
#include <Soprano/Vocabulary/RDF>
#include <Soprano/StatementIterator>
#include <Soprano/QueryResultIterator>

#include <QtCore/QFileInfo>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QUuid>
#include <QtCore/QMutableHashIterator>
#include <QtCore/QCoreApplication>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusServiceWatcher>
#include <QtDBus/QDBusMetaType>

using namespace Soprano;
using namespace Soprano::Vocabulary;
using namespace Nepomuk2::Vocabulary;

Nepomuk2::ResourceManager* Nepomuk2::ResourceManager::s_instance = 0;

Nepomuk2::ResourceManagerPrivate::ResourceManagerPrivate( ResourceManager* manager )
    : mainModel( 0 ),
      overrideModel( 0 ),
      mutex(QMutex::Recursive),
      m_manager( manager ),
      m_watcher( 0 )
{
    Nepomuk2::DBus::registerDBusTypes();
}


Nepomuk2::ResourceData* Nepomuk2::ResourceManagerPrivate::data( const QUrl& uri, const QUrl& type )
{
    if ( uri.isEmpty() ) {
        // return an invalid resource which may be activated by calling setProperty
        return new ResourceData( QUrl(), QUrl(), type, this );
    }

    QUrl newUri = uri;
    if( uri.scheme().isEmpty() ) {
        const QString uriString = uri.toString();
        if( uriString[0] == '/' && QFile::exists(uriString) ) {
            newUri.setScheme(QLatin1String("file"));
        }
    }

    if( ResourceData* data = findData( newUri ) ) {
        return data;
    }
    else {
        if( uri.scheme() != QLatin1String("nepomuk") )
            return new ResourceData( QUrl(), newUri, type, this );
        else
            return new ResourceData( newUri, QUrl(), type, this );
    }
}


Nepomuk2::ResourceData* Nepomuk2::ResourceManagerPrivate::data( const QString& uriOrId, const QUrl& type )
{
    if ( !uriOrId.isEmpty() ) {
        KUrl url( uriOrId );
        if( uriOrId[0] == '/' && QFile::exists(uriOrId) ) {
            url.setScheme("file");
        }
        return data( url, type );
    }

    return new ResourceData( QUrl(), QUrl(), type, this );
}


//FIXME: Streamline this function. It's supposed to be faster than the rest
Nepomuk2::ResourceData* Nepomuk2::ResourceManagerPrivate::dataForResourceUri( const QUrl& uri, const QUrl& type )
{
    if ( uri.isEmpty() ) {
        // return an invalid resource which may be activated by calling setProperty
        return new ResourceData( QUrl(), QUrl(), type, this );
    }

    if( ResourceData* data = findData( uri ) ) {
        return data;
    }
    else {
        return new ResourceData( uri, QUrl(), type, this );
    }
}


QSet<Nepomuk2::ResourceData*> Nepomuk2::ResourceManagerPrivate::allResourceData()
{
    return m_identifierKickOff.values().toSet() + m_urlKickOff.values().toSet() + m_initializedData.values().toSet();
}

bool Nepomuk2::ResourceManagerPrivate::shouldBeDeleted( ResourceData * rd ) const
{
    // We only delete ResourceData objects if no other Resource is accessing them
    return !rd->cnt();
}


void Nepomuk2::ResourceManagerPrivate::_k_storageServiceInitialized( bool success )
{
    if( success ) {
        kDebug() << "Nepomuk Storage service up and initialized.";
        m_manager->init();
        emit m_manager->nepomukSystemStarted();
    }
}


void Nepomuk2::ResourceManagerPrivate::_k_dbusServiceUnregistered( const QString& serviceName )
{
    if( serviceName == QLatin1String("org.kde.NepomukStorage") ) {
        kDebug() << "Nepomuk Storage service went down.";
        if( m_manager->d->mainModel && m_manager->d->mainModel->isValid() ){
            m_manager->d->mainModel->disconnect();
        }
        emit m_manager->nepomukSystemStopped();
    }
}



Nepomuk2::ResourceData* Nepomuk2::ResourceManagerPrivate::findData( const QUrl& uri )
{
    if ( !uri.isEmpty() ) {
        QMutexLocker lock( &mutex );

        if( uri.scheme() == QLatin1String("nepomuk") ) {
            ResourceDataHash::iterator it = m_initializedData.find( uri );
            if( it != m_initializedData.end() )
                return it.value();
        }
        else if( uri.scheme().isEmpty() ) {
            QString identifier = uri.toString();
            QHash<QString, ResourceData*>::iterator it = m_identifierKickOff.find( identifier );
            if( it != m_identifierKickOff.end() )
                return it.value();
        }
        else {
            QHash<QUrl, ResourceData*>::iterator it = m_urlKickOff.find( uri );
            if( it != m_urlKickOff.end() )
                return it.value();
        }
    }

    return 0;
}


Nepomuk2::ResourceManager::ResourceManager()
    : QObject(),
      d( new ResourceManagerPrivate( this ) )
{
    // connect to the storage service's initialized signal to be able to emit
    // the nepomukSystemStarted signal
    QDBusConnection::sessionBus().connect( QLatin1String("org.kde.NepomukStorage"),
                                           QLatin1String("/servicecontrol"),
                                           QLatin1String("org.kde.nepomuk.ServiceControl"),
                                           QLatin1String("serviceInitialized"),
                                           this,
                                           SLOT(_k_storageServiceInitialized(bool)) );

    // connect to the serviceUnregistered signal to be able to connect the nepomukSystemStopped
    // signal once the storage service goes away
    QDBusServiceWatcher *watcher = new QDBusServiceWatcher( QLatin1String("org.kde.NepomukStorage"),
                                                            QDBusConnection::sessionBus(),
                                                            QDBusServiceWatcher::WatchForUnregistration,
                                                            this );
    connect( watcher, SIGNAL(serviceUnregistered(QString)),
             this, SLOT(_k_dbusServiceUnregistered(QString)) );

    // now let's see if the service is already running and init, if so
    QDBusMessage result = QDBusConnection::sessionBus().call(
            QDBusMessage::createMethodCall(
                    QLatin1String("org.kde.NepomukStorage"),
                    QLatin1String("/servicecontrol"),
                    QLatin1String("org.kde.nepomuk.ServiceControl"),
                    QLatin1String("isInitialized")
                )
            );
    if ( result.type() == QDBusMessage::ReplyMessage ) {
        if ( result.arguments().count() > 0 ) {
            const bool initialized = result.arguments()[0].toBool();
            d->_k_storageServiceInitialized( initialized );
        }
    }

}

void Nepomuk2::ResourceManager::cleanupResources()
{
    //
    // Ideally, all three caches should be empty when the ResourceManager is being destroyed
    // But that isn't always the case. There could be a Resource in a static object which gets
    // deleted after the ResourceManager.
    // In order to counter that case, we mark all Resource classes as invalid.
    // The cache should be empty when the ResourceManager is getting destroyed
    // See bug 292996 - https://bugs.kde.org/show_bug.cgi?id=292996
    //
    QMutexLocker lock( &d->mutex );
    QList<ResourceData*> rdList = d->m_identifierKickOff.values() + d->m_urlKickOff.values() +
                                  d->m_initializedData.values();
    foreach( const ResourceData* rd, rdList.toSet() ) {
        foreach(Resource* res, rd->resources())
            res->m_data = 0;
        delete rd;
    }
}

Nepomuk2::ResourceManager::~ResourceManager()
{
    // The cache should be empty when the ResourceManager is getting destroyed
    Q_ASSERT( d->m_identifierKickOff.isEmpty() );
    Q_ASSERT( d->m_urlKickOff.isEmpty() );
    Q_ASSERT( d->m_initializedData.isEmpty() );

    delete d->mainModel;
    delete d;

    if(s_instance == this) {
        s_instance = 0;
    }
}


Nepomuk2::ResourceManager* Nepomuk2::ResourceManager::instance()
{
    if(!s_instance) {
        //
        // This happens when QCoreApplication::quit has been called
        //
        QCoreApplication* app = QCoreApplication::instance();
        if( !app )
            return 0;

        s_instance = new ResourceManager();
        s_instance->setParent( app );
        connect( app, SIGNAL(aboutToQuit()), s_instance, SLOT(cleanupResources()) );
    }
    return s_instance;
}


int Nepomuk2::ResourceManager::init()
{
    QMutexLocker lock( &d->initMutex );

    if( d->overrideModel ) {
        return true;
    }

    if( !d->mainModel ) {
        d->mainModel = new MainModel( this );
    }

    d->mainModel->init();

    return d->mainModel->isValid() ? 0 : -1;
}


bool Nepomuk2::ResourceManager::initialized() const
{
    QMutexLocker lock( &d->initMutex );
    if( d->overrideModel )
        return true;

    return d->mainModel && d->mainModel->isValid();
}


void Nepomuk2::ResourceManager::removeResource( const QString& uri )
{
    Resource res( uri );
    res.remove();
}

void Nepomuk2::ResourceManager::notifyError( const QString& uri, int errorCode )
{
    kDebug() << "(Nepomuk2::ResourceManager) error: " << uri << " " << errorCode;
    emit error( uri, errorCode );
}


QUrl Nepomuk2::ResourceManager::generateUniqueUri( const QString& name )
{
    // default to res URIs
    QString type = QLatin1String("res");

    // ctx is the only used value for name
    if(name == QLatin1String("ctx")) {
        type = name;
    }

    Soprano::Model* model = mainModel();

    while( 1 ) {
        QString uuid = QUuid::createUuid().toString();
        uuid = uuid.mid(1, uuid.length()-2);
        QString uriString = QLatin1String("nepomuk:/") + type + QLatin1String("/") + uuid;
        QString query = QString::fromLatin1("select iri_to_id( '%1', 0 )").arg( uriString );

        Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageUser,
                                                               QLatin1String("sql") );

        // don't loop forever if there was an error executing the query
        if( model->lastError() ) {
            return QUrl();
        }
        if( it.next() ) {
            if( it[0].literal().toString().isEmpty() ) {
                return QUrl( uriString );
            }
        }
    }
}


Soprano::Model* Nepomuk2::ResourceManager::mainModel()
{
    // make sure we are initialized
    if ( !d->overrideModel && !initialized() ) {
        init();
    }

    if( d->overrideModel )
        return d->overrideModel;

    return d->mainModel;
}


void Nepomuk2::ResourceManager::slotPropertyAdded(const Resource &res, const Types::Property &prop, const QVariant &value)
{
    QMutexLocker lock( &d->mutex );
    ResourceDataHash::iterator it = d->m_initializedData.find(res.uri());
    if(it != d->m_initializedData.end()) {
        ResourceData* data = *it;
        data->propertyAdded(prop, value);
    }
}

void Nepomuk2::ResourceManager::slotPropertyRemoved(const Resource &res, const Types::Property &prop, const QVariant &value_)
{
    QMutexLocker lock( &d->mutex );
    ResourceDataHash::iterator it = d->m_initializedData.find(res.uri());
    if(it != d->m_initializedData.end()) {
        ResourceData* data = *it;
        data->propertyRemoved(prop, value_);
    }
}

void Nepomuk2::ResourceManager::setOverrideMainModel( Soprano::Model* model )
{
    QMutexLocker lock( &d->mutex );

    if( model != d->mainModel ) {
        d->overrideModel = model;

        // clear cache to make sure we do not mix data
        Q_FOREACH( ResourceData* data, d->allResourceData()) {
            data->invalidateCache();
        }
    }
}

void Nepomuk2::ResourceManagerPrivate::addToWatcher( const QUrl& uri )
{
    if( uri.isEmpty() )
        return;
    // Lock the mutex, because the ResourceWatcher is not thread-safe.
    // This should have a small impact, because a) it is not taking it for very long.
    // b) the calling function in ResourceData will have to lock the mutex soon after this routine returns 
    // to edit m_initializedData, so there is limited opportunities for parallelism.
    QMutexLocker lock( &mutex );
    if( !m_watcher ) {
        m_watcher = new ResourceWatcher(m_manager);
        QObject::connect( m_watcher, SIGNAL(propertyAdded(Nepomuk2::Resource, Nepomuk2::Types::Property, QVariant)),
                  m_manager, SLOT(slotPropertyAdded(Nepomuk2::Resource, Nepomuk2::Types::Property, QVariant)) );
        QObject::connect( m_watcher, SIGNAL(propertyRemoved(Nepomuk2::Resource, Nepomuk2::Types::Property, QVariant)),
                  m_manager, SLOT(slotPropertyRemoved(Nepomuk2::Resource, Nepomuk2::Types::Property, QVariant)) );
    }
    m_watcher->addResource( uri );
    // (re-)start the watcher in case this resource is the only one in the list of watched
    if( m_watcher->resourceCount() <= 1 ) {
        m_watcher->start();
    }
}

void Nepomuk2::ResourceManagerPrivate::removeFromWatcher( const QUrl& uri )
{
    if( uri.isEmpty())
        return;

    QMutexLocker lock( &mutex );
    if( !m_watcher )
        return;

    m_watcher->removeResource( uri );
    // stop the watcher since we do not want to watch all changes in case there is no ResourceData left
    if( !m_watcher->resourceCount() ) {
        m_watcher->stop();
    }
}

#include "resourcemanager.moc"
