/*
    This file is part of the Nepomuk KDE project.
    Copyright (C) 2011  Vishesh Handa <handa.vish@gmail.com>
    Copyright (C) 2011-2012 Sebastian Trueg <trueg@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/


#include "resourcewatcher.h"
#include "resourcewatcherconnectioninterface.h"
#include "resourcewatchermanagerinterface.h"
#include "resourcemanager.h"

#include "resource.h"
#include "variant.h"
#include "property.h"
#include "literal.h"

#include <QtDBus>

#include <KUrl>
#include <KDebug>
#include <Soprano/Vocabulary/RDFS>

using namespace Soprano::Vocabulary;

namespace {
    QString convertUri(const QUrl& uri) {
        return KUrl(uri).url();
    }

    QStringList convertUris(const QList<QUrl>& uris) {
        QStringList cs;
        foreach(const QUrl& uri, uris) {
            cs << convertUri(uri);
        }
        return cs;
    }

    QList<QUrl> convertUris(const QStringList& uris) {
        QList<QUrl> us;
        foreach(const QString& uri, uris) {
            us << KUrl(uri);
        }
        return us;
    }
}

class Nepomuk2::ResourceWatcher::Private {
public:
    QList<QUrl> m_types;
    QList<QUrl> m_resources;
    QList<QUrl> m_properties;

    org::kde::nepomuk::ResourceWatcherConnection * m_connectionInterface;
    org::kde::nepomuk::ResourceWatcher * m_watchManagerInterface;
};

Nepomuk2::ResourceWatcher::ResourceWatcher(QObject* parent)
    : QObject(parent),
      d(new Private)
{
    d->m_watchManagerInterface
            = new org::kde::nepomuk::ResourceWatcher( "org.kde.nepomuk.DataManagement",
                                                      "/resourcewatcher",
                                                      QDBusConnection::sessionBus() );
    d->m_connectionInterface = 0;
}

Nepomuk2::ResourceWatcher::~ResourceWatcher()
{
    stop();
    delete d;
}

bool Nepomuk2::ResourceWatcher::start()
{
    stop();

    //
    // Convert to list of strings
    //
    QList<QString> uris = convertUris(d->m_resources);
    QList<QString> props = convertUris(d->m_properties);
    QList<QString> types_ = convertUris(d->m_types);

    //
    // Watch for the RW service to (re-)appear and then re-connect to make sure we always get updates
    // We create this watcher even if we fail to connect below. Thus, once the rw service comes up we
    // can re-attach.
    //
    connect(ResourceManager::instance(), SIGNAL(nepomukSystemStarted()), this, SLOT(start()));

    //
    // Create the dbus object to watch
    //
    QDBusPendingReply<QDBusObjectPath> reply = d->m_watchManagerInterface->watch( uris, props, types_ );
    QDBusObjectPath path = reply.value();

    if(!path.path().isEmpty()) {
        d->m_connectionInterface = new org::kde::nepomuk::ResourceWatcherConnection( "org.kde.nepomuk.DataManagement",
                                                                                     path.path(),
                                                                                     QDBusConnection::sessionBus() );
        connect( d->m_connectionInterface, SIGNAL(propertyChanged(QString,QString,QVariantList,QVariantList)),
                 this, SLOT(slotPropertyChanged(QString,QString,QVariantList,QVariantList)) );
        connect( d->m_connectionInterface, SIGNAL(resourceCreated(QString,QStringList)),
                 this, SLOT(slotResourceCreated(QString,QStringList)) );
        connect( d->m_connectionInterface, SIGNAL(resourceRemoved(QString,QStringList)),
                 this, SLOT(slotResourceRemoved(QString,QStringList)) );
        connect( d->m_connectionInterface, SIGNAL(resourceTypesAdded(QString,QStringList)),
                 this, SLOT(slotResourceTypesAdded(QString,QStringList)) );
        connect( d->m_connectionInterface, SIGNAL(resourceTypesRemoved(QString,QStringList)),
                 this, SLOT(slotResourceTypesRemoved(QString,QStringList)) );

        //kDebug() << "Successfully connected to watch service";
        return true;
    }
    else {
        kDebug() << "Failed to connect to watch service" << reply.error().message();
        return false;
    }
}

void Nepomuk2::ResourceWatcher::stop()
{
    if (d->m_connectionInterface) {
        d->m_connectionInterface->close();
        delete d->m_connectionInterface;
        d->m_connectionInterface = 0;
    }

    // This is for the case when ~ResourceManager removes its ResourceWatcher,
    // which in turn calls stop().
    if( ResourceManager::instance() )
        disconnect(ResourceManager::instance(), SIGNAL(nepomukSystemStarted()), this, SLOT(start()));
}

void Nepomuk2::ResourceWatcher::addProperty(const Nepomuk2::Types::Property& property)
{
    d->m_properties << property.uri();
    if(d->m_connectionInterface) {
        d->m_connectionInterface->addProperty(convertUri(property.uri()));
    }
}

void Nepomuk2::ResourceWatcher::addResource(const Nepomuk2::Resource& res)
{
    addResource(res.uri());
}

void Nepomuk2::ResourceWatcher::addResource(const QUrl& resUri)
{
    d->m_resources << resUri;
    if(d->m_connectionInterface) {
        d->m_connectionInterface->addResource(convertUri(resUri));
    }
}


void Nepomuk2::ResourceWatcher::addType(const Nepomuk2::Types::Class& type)
{
    d->m_types << type.uri();
    if(d->m_connectionInterface) {
        d->m_connectionInterface->addType(convertUri(type.uri()));
    }
}

void Nepomuk2::ResourceWatcher::removeProperty(const Nepomuk2::Types::Property& property)
{
    d->m_properties.removeAll(property.uri());
    if(d->m_connectionInterface) {
        d->m_connectionInterface->removeProperty(convertUri(property.uri()));
    }
}

void Nepomuk2::ResourceWatcher::removeResource(const Nepomuk2::Resource& res)
{
    removeResource(res.uri());
}

void Nepomuk2::ResourceWatcher::removeResource(const QUrl& resUri)
{
    d->m_resources.removeAll(resUri);
    if(d->m_connectionInterface) {
        d->m_connectionInterface->removeResource(convertUri(resUri));
    }
}

void Nepomuk2::ResourceWatcher::removeType(const Nepomuk2::Types::Class& type)
{
    d->m_types.removeAll(type.uri());
    if(d->m_connectionInterface) {
        d->m_connectionInterface->removeType(convertUri(type.uri()));
    }
}

QList< Nepomuk2::Types::Property > Nepomuk2::ResourceWatcher::properties() const
{
    QList< Nepomuk2::Types::Property > props;
    foreach(const QUrl& uri, d->m_properties)
        props << Types::Property(uri);
    return props;
}

QList<Nepomuk2::Resource> Nepomuk2::ResourceWatcher::resources() const
{
    QList<Nepomuk2::Resource> resources;
    foreach(const QUrl& uri, d->m_resources)
        resources << Resource::fromResourceUri(uri);
    return resources;
}

QList< Nepomuk2::Types::Class > Nepomuk2::ResourceWatcher::types() const
{
    QList<Nepomuk2::Types::Class> types;
    foreach(const QUrl& uri, d->m_types)
        types << Types::Class(uri);
    return types;
}

int Nepomuk2::ResourceWatcher::propertyCount() const
{
    return d->m_properties.size();
}

int Nepomuk2::ResourceWatcher::resourceCount() const
{
    return d->m_resources.size();
}

int Nepomuk2::ResourceWatcher::typeCount() const
{
    return d->m_types.size();
}


void Nepomuk2::ResourceWatcher::setProperties(const QList< Nepomuk2::Types::Property >& properties_)
{
    d->m_properties.clear();
    foreach(const Nepomuk2::Types::Property& p, properties_) {
        d->m_properties << p.uri();
    }

    if(d->m_connectionInterface) {
        d->m_connectionInterface->setProperties(convertUris(d->m_properties));
    }
}

void Nepomuk2::ResourceWatcher::setResources(const QList< Nepomuk2::Resource >& resources_)
{
    d->m_resources.clear();
    foreach(const Nepomuk2::Resource& res, resources_) {
        d->m_resources << res.uri();
    }

    if(d->m_connectionInterface) {
        d->m_connectionInterface->setResources(convertUris(d->m_resources));
    }
}

void Nepomuk2::ResourceWatcher::setTypes(const QList< Nepomuk2::Types::Class >& types_)
{
    d->m_types.clear();
    foreach(const Nepomuk2::Types::Class& t, types_) {
        d->m_types << t.uri();
    }

    if(d->m_connectionInterface) {
        d->m_connectionInterface->setTypes(convertUris(d->m_types));
    }
}

void Nepomuk2::ResourceWatcher::slotResourceCreated(const QString &res, const QStringList &types)
{
    emit resourceCreated(Nepomuk2::Resource::fromResourceUri(KUrl(res)), convertUris(types));
}

void Nepomuk2::ResourceWatcher::slotResourceRemoved(const QString &res, const QStringList &types)
{
    emit resourceRemoved(KUrl(res), convertUris(types));
}

void Nepomuk2::ResourceWatcher::slotResourceTypesAdded(const QString &res, const QStringList &types)
{
    foreach(const QString& type, types) {
        emit resourceTypeAdded(KUrl(res), KUrl(type));
    }
}

void Nepomuk2::ResourceWatcher::slotResourceTypesRemoved(const QString &res, const QStringList &types)
{
    foreach(const QString& type, types) {
        emit resourceTypeRemoved(KUrl(res), KUrl(type));
    }
}

namespace {
    QVariant convertType(const Nepomuk2::Types::Property& prop, const QVariant& v) {
        QVariant var(v);
        if( !prop.literalRangeType().isValid() && var.type() == QVariant::String ) {
            var.setValue<QUrl>(QUrl(var.toString()));
        }

        return var;
    }
}

void Nepomuk2::ResourceWatcher::slotPropertyChanged(const QString& res_, const QString& prop_, const QVariantList& addedObjs, const QVariantList& removedObjs)
{
    const Resource res = Resource::fromResourceUri(KUrl(res_));
    const Types::Property prop = KUrl(prop_);

    foreach( const QVariant& v, addedObjs ) {
        emit propertyAdded( res, prop, convertType(prop, v) );
    }

    foreach( const QVariant& v, removedObjs ) {
        emit propertyRemoved( res, prop, convertType(prop, v) );
    }

    emit propertyChanged( res, prop, addedObjs, removedObjs );
}

#include "resourcewatcher.moc"

