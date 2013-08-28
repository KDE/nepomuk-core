/*
    Copyright (C) 2010-11 Vishesh handa <handa.vish@gmail.com>
    Copyright (C) 2011-2012 Sebastian Trueg <trueg@kde.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#include "resourcewatcherconnection.h"
#include "resourcewatcherconnectionadaptor.h"
#include "resourcewatchermanager.h"

#include <QtDBus/QDBusObjectPath>
#include <QtDBus/QDBusServiceWatcher>

#include <kdbusconnectionpool.h>

Nepomuk2::ResourceWatcherConnection::ResourceWatcherConnection( ResourceWatcherManager* parent )
    : QObject( parent ),
      m_manager(parent)
{
}

Nepomuk2::ResourceWatcherConnection::~ResourceWatcherConnection()
{
    m_manager->removeConnection(this);
}

QDBusObjectPath Nepomuk2::ResourceWatcherConnection::registerDBusObject( const QString& dbusClient, int id )
{
    // build the dbus object path from the id and register the connection as a Query dbus object
    new ResourceWatcherConnectionAdaptor( this );
    const QString dbusObjectPath = QString::fromLatin1( "/resourcewatcher/watch%1" ).arg( id );
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

void Nepomuk2::ResourceWatcherConnection::close()
{
    deleteLater();
}

void Nepomuk2::ResourceWatcherConnection::setResources(const QStringList &resources)
{
    m_manager->setResources(this, resources);
}

void Nepomuk2::ResourceWatcherConnection::addResource(const QString &resource)
{
    m_manager->addResource(this, resource);
}

void Nepomuk2::ResourceWatcherConnection::removeResource(const QString &resource)
{
    m_manager->removeResource(this, resource);
}

void Nepomuk2::ResourceWatcherConnection::setProperties(const QStringList &properties)
{
    m_manager->setProperties(this, properties);
}

void Nepomuk2::ResourceWatcherConnection::addProperty(const QString &property)
{
    m_manager->addProperty(this, property);
}

void Nepomuk2::ResourceWatcherConnection::removeProperty(const QString &property)
{
    m_manager->removeProperty(this, property);
}

void Nepomuk2::ResourceWatcherConnection::setTypes(const QStringList &types)
{
    m_manager->setTypes(this, types);
}

void Nepomuk2::ResourceWatcherConnection::addType(const QString &type)
{
    m_manager->addType(this, type);
}

void Nepomuk2::ResourceWatcherConnection::removeType(const QString &type)
{
    m_manager->removeType(this, type);
}

#include "resourcewatcherconnection.moc"
