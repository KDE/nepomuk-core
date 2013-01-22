/*
    This file is part of the Nepomuk KDE project.
    Copyright (C) 2011  Vishesh Handa <handa.vish@gmail.com>
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


#include "resourcewatchermanager.h"
#include "resourcewatcherconnection.h"
#include "datamanagementmodel.h"
#include "typecache.h"

#include <Soprano/Statement>
#include <Soprano/StatementIterator>
#include <Soprano/NodeIterator>
#include <Soprano/Vocabulary/RDF>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>

#include <KUrl>
#include <KDebug>

#include <QtCore/QStringList>
#include <QtCore/QSet>


using namespace Soprano::Vocabulary;

namespace {
QVariant nodeToVariant(const Soprano::Node& node) {
    if(node.isResource()) {
        return QVariant(node.uri().toString());
    }
    else {
        return QVariant(node.literal().variant());
    }
}

template<typename T> QVariantList nodeListToVariantList(const T &nodes) {
    QVariantList list;
    list.reserve(nodes.size());
    foreach( const Soprano::Node &n, nodes ) {
        list << nodeToVariant(n);
    }

    return list;
}

QString convertUri(const QUrl& uri) {
    return KUrl(uri).url();
}

template<typename T> QStringList convertUris(const T& uris) {
    QStringList sl;
    foreach(const QUrl& uri, uris)
        sl << convertUri(uri);
    return sl;
}

QUrl convertUri(const QString& s) {
    return KUrl(s);
}

QList<QUrl> convertUris(const QStringList& uris) {
    QList<QUrl> sl;
    foreach(const QString& uri, uris)
        sl << convertUri(uri);
    return sl;
}

/**
 * Returns true if the given hash contains at least one of the possible combinations of con and "c in candidates".
 */
bool hashContainsAtLeastOneOf(Nepomuk2::ResourceWatcherConnection* con, const QSet<QUrl>& candidates, const QMultiHash<QUrl, Nepomuk2::ResourceWatcherConnection*>& hash) {
    for(QSet<QUrl>::const_iterator it = candidates.constBegin();
        it != candidates.constEnd(); ++it) {
        if(hash.contains(*it, con)) {
            return true;
        }
    }
    return false;
}
}


Nepomuk2::ResourceWatcherManager::ResourceWatcherManager(DataManagementModel* parent)
    : QObject(parent),
      m_model(parent),
      m_mutex( QMutex::Recursive ),
      m_connectionCount(0)
{
    QDBusConnection::sessionBus().registerObject("/resourcewatcher", this, QDBusConnection::ExportScriptableSlots|QDBusConnection::ExportScriptableSignals);
}

Nepomuk2::ResourceWatcherManager::~ResourceWatcherManager()
{
    // the connections call removeConnection() from their descrutors. Thus,
    // we need to clean them up before we are deleted ourselves
    QMutexLocker locker( &m_mutex );
    QSet<ResourceWatcherConnection*> allConnections
            = QSet<ResourceWatcherConnection*>::fromList(m_resHash.values())
            + QSet<ResourceWatcherConnection*>::fromList(m_propHash.values())
            + QSet<ResourceWatcherConnection*>::fromList(m_typeHash.values())
            + m_watchAllConnections;
    qDeleteAll(allConnections);
}


void Nepomuk2::ResourceWatcherManager::changeProperty(const QUrl &res, const QUrl &property, const QList<Soprano::Node> &addedValues, const QList<Soprano::Node> &removedValues)
{
    QMutexLocker locker( &m_mutex );
//    kDebug() << res << property << addedValues << removedValues;

    //
    // We only need the resource types if any connections are watching types.
    //
    QSet<QUrl> types;
    if(!m_typeHash.isEmpty()) {
        types = m_model->typeCache()->types( res ).toSet();
    }


    //
    // special case: rdf:type
    //
    if(property == RDF::type()) {
        QSet<QUrl> addedTypes, removedTypes;
        for(QList<Soprano::Node>::const_iterator it = addedValues.constBegin();
            it != addedValues.constEnd(); ++it) {
            addedTypes << it->uri();
        }
        for(QList<Soprano::Node>::const_iterator it = removedValues.constBegin();
            it != removedValues.constEnd(); ++it) {
            removedTypes << it->uri();
        }
        changeTypes(res, types, addedTypes, removedTypes);
    }


    // first collect all the connections we need to emit the signals for
    QSet<ResourceWatcherConnection*> connections(m_watchAllConnections);

    //
    // Emit signals for all the connections that are only watching specific resources
    //
    foreach( ResourceWatcherConnection* con, m_resHash.values( res ) ) {
        if( m_propHash.contains(property, con) ||
            !m_propHash.values().contains(con) ) {
            connections << con;
        }
    }


    //
    // Emit signals for the connections that are watching specific resources and properties
    //
    foreach( ResourceWatcherConnection* con, m_propHash.values( property ) ) {
        //
        // Emit for those connections which watch the property and either no
        // type or once of the types of the resource.
        // Only query the types if we have any type watching connections.
        //
        bool conIsWatchingResType = !m_typeHash.values().contains(con);
        foreach(const QUrl& type, types) {
            if(m_typeHash.contains(type, con)) {
                conIsWatchingResType = true;
                break;
            }
        }

        if( !m_resHash.values().contains(con) && conIsWatchingResType ) {
            connections << con;
        }
    }



    //
    // Emit signals for all connections which watch one of the types of the resource
    // but no properties (that is handled above).
    //
    foreach(const QUrl& type, types) {
        foreach(ResourceWatcherConnection* con, m_typeHash.values(type)) {
            if(!m_propHash.values(property).contains(con)) {
                connections << con;
            }
        }
    }


    //
    // Finally emit the signals for all connections
    //
    foreach(ResourceWatcherConnection* con, connections) {
        // make sure we emit from the correct thread through a queued connection
        QMetaObject::invokeMethod(con,
                                  "propertyChanged",
                                  Q_ARG(QString, convertUri(res)),
                                  Q_ARG(QString, convertUri(property)),
                                  Q_ARG(QVariantList, nodeListToVariantList(addedValues)),
                                  Q_ARG(QVariantList, nodeListToVariantList(removedValues)));
    }
}

void Nepomuk2::ResourceWatcherManager::changeProperty(const QMultiHash< QUrl, Soprano::Node >& oldValues,
                                                     const QUrl& property,
                                                     const QList<Soprano::Node>& nodes)
{
    QMutexLocker locker( &m_mutex );
    QList<QUrl> uniqueKeys = oldValues.keys();
    foreach( const QUrl resUri, uniqueKeys ) {
        const QList<Soprano::Node> old = oldValues.values( resUri );
        changeProperty(resUri, property, old, nodes);
    }
}

void Nepomuk2::ResourceWatcherManager::createResource(const QUrl &uri, const QList<QUrl> &types)
{
    QMutexLocker locker( &m_mutex );
    QSet<ResourceWatcherConnection*> connections(m_watchAllConnections);
    foreach(const QUrl& type, types) {
        foreach(ResourceWatcherConnection* con, m_typeHash.values( type )) {
            connections += con;
        }
    }

    foreach(ResourceWatcherConnection* con, connections) {
        // make sure we emit from the correct thread through a queued connection
        QMetaObject::invokeMethod(con,
                                  "resourceCreated",
                                  Q_ARG(QString, convertUri(uri)),
                                  Q_ARG(QStringList, convertUris(types)));
    }
}

void Nepomuk2::ResourceWatcherManager::removeResource(const QUrl &res, const QList<QUrl>& _types)
{
    QMutexLocker locker( &m_mutex );
    QList<QUrl> types(_types);
    if(!m_typeHash.isEmpty()) {
        types = m_model->typeCache()->types( res );
    }

    QSet<ResourceWatcherConnection*> connections(m_watchAllConnections);
    foreach(const QUrl& type, types) {
        foreach(ResourceWatcherConnection* con, m_typeHash.values( type )) {
            connections += con;
        }
    }
    foreach(ResourceWatcherConnection* con, m_resHash.values( res )) {
        connections += con;
    }

    foreach(ResourceWatcherConnection* con, connections) {
        // make sure we emit from the correct thread through a queued connection
        QMetaObject::invokeMethod(con,
                                  "resourceRemoved",
                                  Q_ARG(QString, convertUri(res)),
                                  Q_ARG(QStringList, convertUris(types)));
    }
}

void Nepomuk2::ResourceWatcherManager::changeSomething()
{
    QMutexLocker locker( &m_mutex );
    // make sure we emit from the correct thread through a queued connection
    QMetaObject::invokeMethod(this, "somethingChanged");
}

Nepomuk2::ResourceWatcherConnection* Nepomuk2::ResourceWatcherManager::createConnection(const QList<QUrl> &resources,
                                                                                      const QList<QUrl> &properties,
                                                                                      const QList<QUrl> &types)
{
    QMutexLocker locker( &m_mutex );
    kDebug() << resources << properties << types;

    ResourceWatcherConnection* con = new ResourceWatcherConnection( this );
    foreach( const QUrl& res, resources ) {
        m_resHash.insert(res, con);
    }

    foreach( const QUrl& prop, properties ) {
        m_propHash.insert(prop, con);
    }

    foreach( const QUrl& type, types ) {
        m_typeHash.insert(type, con);
    }

    if(resources.isEmpty() && properties.isEmpty() && types.isEmpty()) {
        m_watchAllConnections.insert(con);
    }

    return con;
}

QDBusObjectPath Nepomuk2::ResourceWatcherManager::watch(const QStringList& resources,
                                                       const QStringList& properties,
                                                       const QStringList& types)
{
    QMutexLocker locker( &m_mutex );
    kDebug() << resources << properties << types;

    if(ResourceWatcherConnection* con = createConnection(convertUris(resources), convertUris(properties), convertUris(types))) {
        return con->registerDBusObject(message().service(), ++m_connectionCount);
    }
    else {
        QDBusConnection::sessionBus().send(message().createErrorReply(QDBusError::InvalidArgs, QLatin1String("Failed to create watch for given arguments.")));
        return QDBusObjectPath();
    }
}

namespace {
    void removeConnectionFromHash( QMultiHash<QUrl, Nepomuk2::ResourceWatcherConnection*> & hash,
                 const Nepomuk2::ResourceWatcherConnection * con )
    {
        QMutableHashIterator<QUrl, Nepomuk2::ResourceWatcherConnection*> it( hash );
        while( it.hasNext() ) {
            if( it.next().value() == con )
                it.remove();
        }
    }
}

void Nepomuk2::ResourceWatcherManager::removeConnection(Nepomuk2::ResourceWatcherConnection *con)
{
    QMutexLocker locker( &m_mutex );
    removeConnectionFromHash( m_resHash, con );
    removeConnectionFromHash( m_propHash, con );
    removeConnectionFromHash( m_typeHash, con );
    m_watchAllConnections.remove(con);
}

void Nepomuk2::ResourceWatcherManager::setResources(Nepomuk2::ResourceWatcherConnection *conn, const QStringList &resources)
{
    QMutexLocker locker( &m_mutex );
    const QSet<QUrl> newRes = convertUris(resources).toSet();
    const QSet<QUrl> oldRes = m_resHash.keys(conn).toSet();

    foreach(const QUrl& res, newRes - oldRes) {
        m_resHash.insert(res, conn);
    }
    foreach(const QUrl& res, oldRes - newRes) {
        m_resHash.remove(res, conn);
    }

    if(resources.isEmpty()) {
        if(!m_propHash.values().contains(conn) &&
           !m_typeHash.values().contains(conn)) {
            m_watchAllConnections << conn;
        }
    }
    else {
        m_watchAllConnections.remove(conn);
    }
}

void Nepomuk2::ResourceWatcherManager::addResource(Nepomuk2::ResourceWatcherConnection *conn, const QString &resource)
{
    QMutexLocker locker( &m_mutex );
    m_resHash.insert(convertUri(resource), conn);
    m_watchAllConnections.remove(conn);
}

void Nepomuk2::ResourceWatcherManager::removeResource(Nepomuk2::ResourceWatcherConnection *conn, const QString &resource)
{
    QMutexLocker locker( &m_mutex );
    m_resHash.remove(convertUri(resource), conn);
    if(!m_resHash.values().contains(conn) &&
       !m_propHash.values().contains(conn) &&
       !m_typeHash.values().contains(conn)) {
        m_watchAllConnections << conn;
    }
}

void Nepomuk2::ResourceWatcherManager::setProperties(Nepomuk2::ResourceWatcherConnection *conn, const QStringList &properties)
{
    QMutexLocker locker( &m_mutex );
    const QSet<QUrl> newprop = convertUris(properties).toSet();
    const QSet<QUrl> oldprop = m_propHash.keys(conn).toSet();

    foreach(const QUrl& prop, newprop - oldprop) {
        m_propHash.insert(prop, conn);
    }
    foreach(const QUrl& prop, oldprop - newprop) {
        m_propHash.remove(prop, conn);
    }

    if(properties.isEmpty()) {
        if(!m_resHash.values().contains(conn) &&
           !m_typeHash.values().contains(conn)) {
            m_watchAllConnections << conn;
        }
    }
    else {
        m_watchAllConnections.remove(conn);
    }
}

void Nepomuk2::ResourceWatcherManager::addProperty(Nepomuk2::ResourceWatcherConnection *conn, const QString &property)
{
    QMutexLocker locker( &m_mutex );
    m_propHash.insert(convertUri(property), conn);
    m_watchAllConnections.remove(conn);
}

void Nepomuk2::ResourceWatcherManager::removeProperty(Nepomuk2::ResourceWatcherConnection *conn, const QString &property)
{
    QMutexLocker locker( &m_mutex );
    m_propHash.remove(convertUri(property), conn);
    if(!m_resHash.values().contains(conn) &&
       !m_propHash.values().contains(conn) &&
       !m_typeHash.values().contains(conn)) {
        m_watchAllConnections << conn;
    }
}

void Nepomuk2::ResourceWatcherManager::setTypes(Nepomuk2::ResourceWatcherConnection *conn, const QStringList &types)
{
    QMutexLocker locker( &m_mutex );
    const QSet<QUrl> newtype = convertUris(types).toSet();
    const QSet<QUrl> oldtype = m_typeHash.keys(conn).toSet();

    foreach(const QUrl& type, newtype - oldtype) {
        m_typeHash.insert(type, conn);
    }
    foreach(const QUrl& type, oldtype - newtype) {
        m_typeHash.remove(type, conn);
    }

    if(types.isEmpty()) {
        if(!m_resHash.values().contains(conn) &&
           !m_propHash.values().contains(conn)) {
            m_watchAllConnections << conn;
        }
    }
    else {
        m_watchAllConnections.remove(conn);
    }
}

void Nepomuk2::ResourceWatcherManager::addType(Nepomuk2::ResourceWatcherConnection *conn, const QString &type)
{
    QMutexLocker locker( &m_mutex );
    m_typeHash.insert(convertUri(type), conn);
    m_watchAllConnections.remove(conn);
}

void Nepomuk2::ResourceWatcherManager::removeType(Nepomuk2::ResourceWatcherConnection *conn, const QString &type)
{
    QMutexLocker locker( &m_mutex );
    m_typeHash.remove(convertUri(type), conn);
    if(!m_resHash.values().contains(conn) &&
       !m_propHash.values().contains(conn) &&
       !m_typeHash.values().contains(conn)) {
        m_watchAllConnections << conn;
    }
}

// FIXME: also take super-classes into account
void Nepomuk2::ResourceWatcherManager::changeTypes(const QUrl &res, const QSet<QUrl>& resTypes, const QSet<QUrl> &addedTypes, const QSet<QUrl> &removedTypes)
{
    QMutexLocker locker( &m_mutex );
    // first collect all the connections we need to emit the signals for
    QSet<ResourceWatcherConnection*> addConnections(m_watchAllConnections), removeConnections(m_watchAllConnections);

    // all connections watching the resource and not a special property
    // and no special type or one of the changed types
    foreach( ResourceWatcherConnection* con, m_resHash.values( res ) ) {
        if( m_propHash.contains(RDF::type(), con) ||
            !m_propHash.values().contains(con) ) {
            if(!addedTypes.isEmpty() &&
               connectionWatchesOneType(con, addedTypes)) {
                addConnections << con;
            }
            if(!removedTypes.isEmpty() &&
               connectionWatchesOneType(con, removedTypes)) {
                removeConnections << con;
            }
        }
    }

    // all connections watching one of the types and no special resource or property
    if(!addedTypes.isEmpty()) {
        foreach(const QUrl& type, addedTypes + resTypes) {
            foreach(ResourceWatcherConnection* con, m_typeHash.values(type)) {
                if(!m_resHash.values().contains(con) &&
                   !m_propHash.values().contains(con)) {
                    addConnections << con;
                }
            }
        }
    }
    if(!removedTypes.isEmpty()) {
        foreach(const QUrl& type, removedTypes + resTypes) {
            foreach(ResourceWatcherConnection* con, m_typeHash.values(type)) {
                if(!m_resHash.values().contains(con) &&
                   !m_propHash.values().contains(con)) {
                    removeConnections << con;
                }
            }
        }
    }

    // all connections watching rdf:type
    foreach(ResourceWatcherConnection* con, m_propHash.values(RDF::type())) {
        if(!m_resHash.values().contains(con) ) {
            if(connectionWatchesOneType(con, addedTypes + resTypes)) {
                addConnections << con;
            }
            if(connectionWatchesOneType(con, removedTypes + resTypes)) {
                removeConnections << con;
            }
        }
    }

    // finally emit the actual signals
    if(!addedTypes.isEmpty()) {
        foreach(ResourceWatcherConnection* con, addConnections) {
            // make sure we emit from the correct thread through a queued connection
            QMetaObject::invokeMethod(con,
                                      "resourceTypesAdded",
                                      Q_ARG(QString, convertUri(res)),
                                      Q_ARG(QStringList, convertUris(addedTypes)));
        }
    }
    if(!removedTypes.isEmpty()) {
        foreach(ResourceWatcherConnection* con, removeConnections) {
            // make sure we emit from the correct thread through a queued connection
            QMetaObject::invokeMethod(con,
                                      "resourceTypesRemoved",
                                      Q_ARG(QString, convertUri(res)),
                                      Q_ARG(QStringList, convertUris(removedTypes)));
        }
    }
}

bool Nepomuk2::ResourceWatcherManager::connectionWatchesOneType(Nepomuk2::ResourceWatcherConnection *con, const QSet<QUrl> &types) const
{
    QMutexLocker locker( &m_mutex );
    return !m_typeHash.values().contains(con) || hashContainsAtLeastOneOf(con, types, m_typeHash);
}

#include "resourcewatchermanager.moc"
