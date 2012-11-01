/*
 * This file is part of the Nepomuk KDE project.
 * Copyright (C) 2006-2010 Sebastian Trueg <trueg@kde.org>
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

#include "resourcedata.h"
#include "resourcemanager.h"
#include "resourcemanager_p.h"
#include "resource.h"
#include "tools.h"
#include "nie.h"
#include "nfo.h"
#include "pimo.h"
#include "nepomukmainmodel.h"
#include "class.h"
#include "datamanagement.h"
#include "createresourcejob.h"
#include "resourcewatcher.h"
#include "dbustypes.h"

#include <Soprano/Statement>
#include <Soprano/StatementIterator>
#include <Soprano/QueryResultIterator>
#include <Soprano/NodeIterator>
#include <Soprano/Model>
#include <Soprano/Vocabulary/RDFS>
#include <Soprano/Vocabulary/RDF>
#include <Soprano/Vocabulary/Xesam>
#include <Soprano/Vocabulary/NAO>

#include <QtCore/QFile>
#include <QtCore/QDateTime>
#include <QtCore/QMutexLocker>
#include <QtCore/QFileInfo>

#include <kdebug.h>
#include <kurl.h>
#include <kcomponentdata.h>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>

using namespace Soprano;

#define MAINMODEL (m_rm->m_manager->mainModel())

using namespace Soprano::Vocabulary;
using namespace Nepomuk2::Vocabulary;

Nepomuk2::ResourceData::ResourceData( const QUrl& uri, const QUrl& kickOffUri, const QUrl& type, ResourceManagerPrivate* rm )
    : m_uri(uri),
      m_type(type.isEmpty() ? RDFS::Resource() : type),
      m_modificationMutex(QMutex::Recursive),
      m_cacheDirty(false),
      m_addedToWatcher(false),
      m_rm(rm)
{
    m_rm->dataCnt.ref();

    if( !uri.isEmpty() ) {
        m_cacheDirty = true;

        QMutexLocker locker(&m_rm->mutex);
        m_rm->m_initializedData.insert( uri, this );
    }

    if( !kickOffUri.isEmpty() ) {
        if( kickOffUri.scheme().isEmpty() ) {
            m_naoIdentifier = kickOffUri.toString();
            m_cache.insert( NAO::identifier(), m_naoIdentifier );

            QMutexLocker locker(&m_rm->mutex);
            m_rm->m_identifierKickOff.insert(m_naoIdentifier, this);
        }
        else {
            m_nieUrl = kickOffUri;
            m_cache.insert( NIE::url(), m_nieUrl );

            QMutexLocker locker(&m_rm->mutex);
            m_rm->m_urlKickOff.insert(m_nieUrl, this);
        }
    }
}


Nepomuk2::ResourceData::~ResourceData()
{
    resetAll(true);
    m_rm->dataCnt.deref();
}


bool Nepomuk2::ResourceData::isFile()
{
    return( m_uri.scheme() == QLatin1String("file") ||
            m_nieUrl.scheme() == QLatin1String("file") ||
            hasProperty( RDF::type(), NFO::FileDataObject() ) );
    // The hasProperty should be const - It shouldn't load the entire cache. Maybe
}


QUrl Nepomuk2::ResourceData::uri() const
{
    return m_uri;
}


QUrl Nepomuk2::ResourceData::type()
{
    QUrl mainType = RDFS::Resource();

    if( !load() )
        return mainType;

    QList<QUrl> types = m_cache[RDF::type()].toUrlList();
    foreach(const QUrl& t, types) {
        Types::Class currentTypeClass = mainType;
        Types::Class storedTypeClass = t;

        // Keep the type that is further down the hierarchy
        if ( storedTypeClass.isSubClassOf( currentTypeClass ) ) {
            mainType = storedTypeClass.uri();
        }
        else {
            // This is a little convenience hack since the user is most likely
            // more interested in the file content than the actual file
            // the same is true for nie:DataObject vs. nie:InformationElement
            Types::Class nieInformationElementClass( NIE::InformationElement() );
            Types::Class nieDataObjectClass( NIE::DataObject() );
            if( ( currentTypeClass == nieDataObjectClass ||
                  currentTypeClass.isSubClassOf( nieDataObjectClass ) ) &&
                ( storedTypeClass == nieInformationElementClass ||
                  storedTypeClass.isSubClassOf( nieInformationElementClass ) ) ) {
                mainType = storedTypeClass.uri();
            }
        }
    }

    return mainType;
}


void Nepomuk2::ResourceData::resetAll( bool isDelete )
{
    // remove us from all caches (store() will re-insert us later if necessary)
    m_rm->mutex.lock();

    // IMPORTANT:
    // Remove from the kickOffList before removing from the resource watcher
    // This is required cause otherwise the Resource::fromResourceUri creates a new
    // resource which is correctly identified to the ResourceData (this), and it is
    // then deleted, which calls resetAll and this cycle continues.
    const QString nao = m_cache[NAO::identifier()].toString();
    m_rm->m_identifierKickOff.remove( nao );
    const QUrl nieUrl = m_cache[NIE::url()].toUrl();
    m_rm->m_urlKickOff.remove( nieUrl );

    if( !m_uri.isEmpty() ) {
        m_rm->m_initializedData.remove( m_uri );
        if( m_rm->m_watcher && m_addedToWatcher ) {
            // See load() for an explanation of the QMetaObject call

            // stop the watcher since we do not want to watch all changes in case there is no ResourceData left
            if(m_rm->m_watcher->resourceCount() == 1) {
                QMetaObject::invokeMethod(m_rm->m_watcher, "stop", Qt::AutoConnection);
            }

            // remove this Resource from the list of watched resources
            QMetaObject::invokeMethod(m_rm->m_watcher, "removeResource", Qt::AutoConnection, Q_ARG(QUrl, m_uri));
            m_addedToWatcher = false;
        }
    }
    m_rm->mutex.unlock();

    // reset all variables
    QMutexLocker locker(&m_modificationMutex);
    m_uri.clear();
    m_nieUrl.clear();
    m_naoIdentifier.clear();
    m_cache.clear();
    m_cacheDirty = false;
    m_type = RDFS::Resource();
}


QHash<QUrl, Nepomuk2::Variant> Nepomuk2::ResourceData::allProperties()
{
    if( !load() )
        return QHash<QUrl, Nepomuk2::Variant>();
    else
        return m_cache;
}


bool Nepomuk2::ResourceData::hasProperty( const QUrl& uri )
{
    if( !load() )
        return false;

    QHash<QUrl, Variant>::const_iterator it = m_cache.constFind( uri );
    if( it == m_cache.constEnd() )
        return false;

    return true;
}


bool Nepomuk2::ResourceData::hasProperty( const QUrl& p, const Variant& v )
{
    if( !load() )
        return false;

    QHash<QUrl, Variant>::const_iterator it = m_cache.constFind( p );
    if( it == m_cache.constEnd() )
        return false;

    QList<Variant> thisVals = it.value().toVariantList();
    QList<Variant> vals = v.toVariantList();
    Q_FOREACH( const Variant& val, vals ) {
        if( !thisVals.contains(val) )
            return false;
    }
    return true;
}


Nepomuk2::Variant Nepomuk2::ResourceData::property( const QUrl& uri )
{
    if( !load() )
        return Variant();

    // we need to protect the reading, too. load my be triggered from another thread's
    // connection to a Soprano statement signal
    QMutexLocker lock(&m_modificationMutex);

    QHash<QUrl, Variant>::const_iterator it = m_cache.constFind( uri );
    if ( it == m_cache.constEnd() ) {
        return Variant();
    }
    else {
        return *it;
    }
}


bool Nepomuk2::ResourceData::store()
{
    QMutexLocker lock(&m_modificationMutex);

    if ( m_uri.isEmpty() ) {
        QMutexLocker rmlock(&m_rm->mutex);

        QList<QUrl> types;
        if ( m_nieUrl.isValid() && m_nieUrl.isLocalFile() ) {
            types << NFO::FileDataObject();
            if( QFileInfo(m_nieUrl.toLocalFile()).isDir() )
                types << NFO::Folder();
        }
        types << m_type;

        QDBusMessage msg = QDBusMessage::createMethodCall( QLatin1String("org.kde.nepomuk.DataManagement"),
                                                           QLatin1String("/datamanagement"),
                                                           QLatin1String("org.kde.nepomuk.DataManagement"),
                                                           QLatin1String("createResource") );

        msg.setArguments( QVariantList()
                          << DBus::convertUriList(types)
                          << QString() << QString()
                          << KGlobal::mainComponent().componentName() );

        QDBusConnection bus = QDBusConnection::sessionBus();
        QDBusMessage reply = bus.call( msg );
        if( reply.type() == QDBusMessage::ErrorMessage ) {
            //TODO: Set the error somehow
            kWarning() << reply.errorMessage();
            return false;
        }
        else {
            m_uri = reply.arguments().at(0).toUrl();

            QList<Soprano::Node> nodes = MAINMODEL->listStatements( m_uri, RDF::type(), QUrl() )
                                         .iterateObjects().allNodes();
            QList<QUrl> types;
            foreach(const Soprano::Node& node, nodes)
                types << node.uri();
            m_cache.insert(RDF::type(), types);
        }

        // Add us to the initialized data, i.e. make us "valid"
        m_rm->m_initializedData.insert( m_uri, this );

        if( !m_naoIdentifier.isEmpty() ) {
            setProperty( NAO::identifier(), m_naoIdentifier );
            m_naoIdentifier.clear();
        }
        if( !m_nieUrl.isEmpty() ) {
            setProperty( NIE::url(), m_nieUrl );
            m_nieUrl.clear();
        }

        addToWatcher();
    }

    return true;
}

// Caller must hold m_modificationMutex
void Nepomuk2::ResourceData::addToWatcher()
{
    if(!m_rm->m_watcher) {
        m_rm->m_watcher = new ResourceWatcher(m_rm->m_manager);
        //
        // The ResourceWatcher is not thread-safe. Thus, we need to ensure the safety ourselves.
        // We do that by simply handling all RW related operations in the manager thread.
        // This also means to invoke methods on the watcher through QMetaObject to make sure they
        // get queued in case of calls between different threads.
        //
        m_rm->m_watcher->moveToThread(m_rm->m_manager->thread());
        QObject::connect( m_rm->m_watcher, SIGNAL(propertyAdded(Nepomuk2::Resource, Nepomuk2::Types::Property, QVariant)),
                            m_rm->m_manager, SLOT(slotPropertyAdded(Nepomuk2::Resource, Nepomuk2::Types::Property, QVariant)) );
        QObject::connect( m_rm->m_watcher, SIGNAL(propertyRemoved(Nepomuk2::Resource, Nepomuk2::Types::Property, QVariant)),
                            m_rm->m_manager, SLOT(slotPropertyRemoved(Nepomuk2::Resource, Nepomuk2::Types::Property, QVariant)) );
        m_rm->m_watcher->addResource( m_uri );
    }
    else {
        QMetaObject::invokeMethod(m_rm->m_watcher, "addResource", Qt::AutoConnection, Q_ARG(QUrl, m_uri) );
    }
    // (re-)start the watcher in case this resource is the only one in the list of watched
    if(m_rm->m_watcher->resources().count() <= 1) {
        QMetaObject::invokeMethod(m_rm->m_watcher, "start", Qt::AutoConnection);
    }
    m_addedToWatcher = true;
}

bool Nepomuk2::ResourceData::load()
{
    QMutexLocker rmlock(&m_rm->mutex); // for updateKickOffLists, but must be locked first
    QMutexLocker lock(&m_modificationMutex);

    if ( m_cacheDirty ) {

        if ( !m_uri.isValid() )
            return false;

        const QString oldNaoIdentifier = m_cache[NAO::identifier()].toString();
        const QUrl oldNieUrl = m_cache[NIE::url()].toUrl();

        m_cache.clear();
        addToWatcher();

        //
        // We exclude properties that are part of the inference graph
        // It would only pollute the user interface
        //
        Soprano::QueryResultIterator it = MAINMODEL->executeQuery(QString("select distinct ?p ?o where { "
                                                                          "%1 ?p ?o . }").arg(Soprano::Node::resourceToN3(m_uri)),
                                                                  Soprano::Query::QueryLanguageSparqlNoInference);
        while ( it.next() ) {
            QUrl p = it["p"].uri();
            m_cache[p].append( Variant::fromNode( it["o"] ) );
        }

        const QString newNaoIdentifier = m_cache[NAO::identifier()].toString();
        const QUrl newNieUrl = m_cache[NIE::url()].toUrl();
        updateIdentifierLists( oldNaoIdentifier, newNaoIdentifier );
        updateUrlLists( oldNieUrl, newNieUrl );

        m_cacheDirty = false;
    }

    return true;
}


void Nepomuk2::ResourceData::setProperty( const QUrl& uri, const Nepomuk2::Variant& value )
{
    Q_ASSERT( uri.isValid() );

    if( store() ) {
        // step 0: make sure this resource is in the store
        QMutexLocker rmlock(&m_rm->mutex); // for updateKickOffLists, but must be locked first
        QMutexLocker lock(&m_modificationMutex);

        // update the store
        QVariantList varList;
        foreach( const Nepomuk2::Variant var, value.toVariantList() ) {
            // make sure resource values are in the store
            if( var.simpleType() == qMetaTypeId<Resource>() ) {
                var.toResource().m_data->store();
                varList << var.toUrl();
            }
            else {
                varList << var.variant();
            }
        }

        // update the store
        QDBusMessage msg = QDBusMessage::createMethodCall( QLatin1String("org.kde.nepomuk.DataManagement"),
                                                           QLatin1String("/datamanagement"),
                                                           QLatin1String("org.kde.nepomuk.DataManagement"),
                                                           QLatin1String("setProperty") );

        msg.setArguments( QVariantList()
                          << DBus::convertUriList(QList<QUrl>() << m_uri)
                          << DBus::convertUri(uri)
                          << QVariant(DBus::normalizeVariantList(varList))
                          << KGlobal::mainComponent().componentName() );

        QDBusConnection bus = QDBusConnection::sessionBus();
        QDBusMessage reply = bus.call( msg );
        if( reply.type() == QDBusMessage::ErrorMessage ) {
            //TODO: Set the error somehow
            kWarning() << reply.errorMessage();
            return;
        }

        // update the kickofflists
        // IMPORTANT: Must be called before the cache is updated. They use the value from the cache
        updateKickOffLists( uri, value );

        // update the cache for now
        if( value.isValid() )
            m_cache[uri] = value;
        else
            m_cache.remove(uri);
    }
}


void Nepomuk2::ResourceData::addProperty( const QUrl& uri, const Nepomuk2::Variant& value )
{
    Q_ASSERT( uri.isValid() );

    if( value.isValid() && store() ) {
        // step 0: make sure this resource is in the store
        QMutexLocker rmlock(&m_rm->mutex); // for updateKickOffLists, but must be locked first
        QMutexLocker lock(&m_modificationMutex);

        // update the store
        QVariantList varList;
        foreach( const Nepomuk2::Variant var, value.toVariantList() ) {
            // make sure resource values are in the store
            if( var.simpleType() == qMetaTypeId<Resource>() ) {
                var.toResource().m_data->store();
                varList << var.toUrl();
            }
            else {
                varList << var.variant();
            }
        }

        QDBusMessage msg = QDBusMessage::createMethodCall( QLatin1String("org.kde.nepomuk.DataManagement"),
                                                           QLatin1String("/datamanagement"),
                                                           QLatin1String("org.kde.nepomuk.DataManagement"),
                                                           QLatin1String("addProperty") );
        msg.setArguments( QVariantList()
                          << DBus::convertUriList(QList<QUrl>() << m_uri)
                          << DBus::convertUri(uri)
                          << QVariant(DBus::normalizeVariantList(varList))
                          << KGlobal::mainComponent().componentName() );

        QDBusConnection bus = QDBusConnection::sessionBus();
        QDBusMessage reply = bus.call( msg );
        if( reply.type() == QDBusMessage::ErrorMessage ) {
            //TODO: Set the error somehow
            kWarning() << reply.errorMessage();
            return;
        }

        // update the kickofflists
        // IMPORTANT: Must be called before the cache is updated. They use the value from the cache
        updateKickOffLists( uri, value );

        // update the cache for now
        if( value.isValid() )
            m_cache[uri].append(value);
    }
}


void Nepomuk2::ResourceData::removeProperty( const QUrl& uri )
{
    Q_ASSERT( uri.isValid() );
    QMutexLocker rmlock(&m_rm->mutex); // for updateKickOffLists, but must be locked first
    QMutexLocker lock(&m_modificationMutex);

    if( !m_uri.isEmpty() ) {
        QDBusMessage msg = QDBusMessage::createMethodCall( QLatin1String("org.kde.nepomuk.DataManagement"),
                                                           QLatin1String("/datamanagement"),
                                                           QLatin1String("org.kde.nepomuk.DataManagement"),
                                                           QLatin1String("removeProperties") );
        msg.setArguments( QVariantList()
                          << DBus::convertUri(m_uri)
                          << DBus::convertUri(uri)
                          << KGlobal::mainComponent().componentName() );

        QDBusConnection bus = QDBusConnection::sessionBus();
        QDBusMessage reply = bus.call( msg );
        if( reply.type() == QDBusMessage::ErrorMessage ) {
            //TODO: Set the error somehow
            kWarning() << reply.errorMessage();
            return;
        }

        // update the kickofflists
        // IMPORTANT: Must be called before the cache is updated. They use the value from the cache
        updateKickOffLists( uri, Variant() );

        // Update the cache
        m_cache.remove( uri );
    }
}


void Nepomuk2::ResourceData::remove( bool recursive )
{
    Q_UNUSED(recursive)
    QMutexLocker lock(&m_modificationMutex);

    if( !m_uri.isEmpty() ) {
        QDBusMessage msg = QDBusMessage::createMethodCall( QLatin1String("org.kde.nepomuk.DataManagement"),
                                                           QLatin1String("/datamanagement"),
                                                           QLatin1String("org.kde.nepomuk.DataManagement"),
                                                           QLatin1String("removeResources") );
        msg.setArguments( QVariantList()
                          << DBus::convertUriList(QList<QUrl>() << m_uri)
                          << 0 /* no flags */
                          << KGlobal::mainComponent().componentName());

        QDBusConnection bus = QDBusConnection::sessionBus();
        QDBusMessage reply = bus.call( msg );
        if( reply.type() == QDBusMessage::ErrorMessage ) {
            //TODO: Set the error somehow
            kWarning() << reply.errorMessage();
            return;
        }
    }

    resetAll();
}


bool Nepomuk2::ResourceData::exists()
{
    if( m_uri.isValid() ) {
        const QString query = QString::fromLatin1("ask { %1 ?p ?o . }")
                              .arg( Soprano::Node::resourceToN3(m_uri) );
        return MAINMODEL->executeQuery( query, Soprano::Query::QueryLanguageSparql ).boolValue();
    }
    else {
        return false;
    }
}


bool Nepomuk2::ResourceData::isValid() const
{
    return !m_uri.isEmpty() || !m_nieUrl.isEmpty() || !m_naoIdentifier.isEmpty();
}

// Caller must hold the ResourceManager mutex (since the RM owns the returned ResourceData pointer)
Nepomuk2::ResourceData* Nepomuk2::ResourceData::determineUri()
{
    QMutexLocker lock(&m_modificationMutex);

    // We have the following possible situations:
    // 1. m_uri is already valid
    //    -> simple, nothing to do
    //
    // 2. m_uri is not valid
    //    -> we need to determine the URI
    //
    // 2.1. m_kickoffUri is valid
    // 2.1.1. it is a file URL
    // 2.1.1.1. it is nie:url for r
    //          -> use r as m_uri
    // 2.1.1.2. it points to a file on a removable device for which we have a filex:/ URL
    //          -> use the r in r nie:url filex:/...
    // 2.1.1.3. it is a file which is not an object in some nie:url relation
    //          -> create new random m_uri and use kickoffUriOrId() as m_nieUrl
    // 2.1.2. it is a resource URI
    //          -> use it as m_uri
    //
    // 2.2. m_kickOffUri is not valid
    // 2.2.1. m_kickOffUri is a nao:identifier for r
    //        -> use r as m_uri
    //

    if( m_uri.isEmpty() ) {
        Soprano::Model* model = MAINMODEL;

        if( !m_naoIdentifier.isEmpty() ) {
            //
            // Not valid. Checking for nao:identifier
            //
            QString query = QString::fromLatin1("select distinct ?r where { ?r %1 %2. } LIMIT 1")
                            .arg( Soprano::Node::resourceToN3(Soprano::Vocabulary::NAO::identifier()) )
                            .arg( Soprano::Node::literalToN3( m_naoIdentifier ) );

            Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
            if( it.next() ) {
                m_uri = it["r"].uri();
                it.close();
            }
        }
        else {
            //
            // In one query determine if the URI is already used as resource URI or as
            // nie:url
            //
            QString query = QString::fromLatin1("select distinct ?r ?o where { "
                                                "{ ?r %1 %2 . FILTER(?r!=%2) . } "
                                                "UNION "
                                                "{ %2 ?p ?o . } "
                                                "} LIMIT 1")
                            .arg( Soprano::Node::resourceToN3( Nepomuk2::Vocabulary::NIE::url() ) )
                            .arg( Soprano::Node::resourceToN3( m_nieUrl ) );
            Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
            if( it.next() ) {
                QUrl uri = it["r"].uri();
                if( uri.isEmpty() ) {
                    // FIXME: Find a way to avoid this
                    // The url is actually the uri - legacy data
                    m_uri = m_nieUrl;
                }
                else {
                    m_uri = uri;
                }
            }
        }

        //
        // Move us to the final data hash now that the URI is known
        //
        if( !m_uri.isEmpty() ) {
            m_cacheDirty = true;
            ResourceDataHash::iterator it = m_rm->m_initializedData.find(m_uri);
            if( it == m_rm->m_initializedData.end() ) {
                m_rm->m_initializedData.insert( m_uri, this );
            }
            else {
                return it.value();
            }
        }
    }

    return this;
}


void Nepomuk2::ResourceData::invalidateCache()
{
    m_cacheDirty = true;
}


bool Nepomuk2::ResourceData::operator==( const ResourceData& other ) const
{
    if( this == &other )
        return true;

    // TODO: Maybe we shouldn't check if the identifier and url are same.
    return( m_uri == other.m_uri &&
            m_naoIdentifier == other.m_naoIdentifier &&
            m_nieUrl == other.m_nieUrl );
}


QDebug Nepomuk2::ResourceData::operator<<( QDebug dbg ) const
{
    dbg << QString::fromLatin1("[uri: %1; url: %2, identifier: %3, ref: %4]")
        .arg( m_uri.url(),
              m_nieUrl.url(),
              m_naoIdentifier )
        .arg( m_ref );

    return dbg;
}


QDebug operator<<( QDebug dbg, const Nepomuk2::ResourceData& data )
{
    return data.operator<<( dbg );
}

void Nepomuk2::ResourceData::updateUrlLists(const QUrl& oldUrl, const QUrl& newUrl)
{
    if ( !oldUrl.isEmpty() ) {
        m_rm->m_urlKickOff.remove( oldUrl );
    }

    if( !newUrl.isEmpty() ) {
        m_rm->m_urlKickOff.insert( newUrl, this );
    }
}

void Nepomuk2::ResourceData::updateIdentifierLists(const QString& oldIdentifier, const QString& newIdentifier)
{
    if ( !oldIdentifier.isEmpty() ) {
        m_rm->m_identifierKickOff.remove( oldIdentifier );
    }

    if( !newIdentifier.isEmpty() ) {
        m_rm->m_identifierKickOff.insert( newIdentifier, this );
    }
}

// Caller must lock RM mutex  first
void Nepomuk2::ResourceData::updateKickOffLists(const QUrl& uri, const Nepomuk2::Variant& variant)
{
    if( uri == NIE::url() )
        updateUrlLists( m_cache[NIE::url()].toUrl(), variant.toUrl() );
    else if( uri == NAO::identifier() )
        updateIdentifierLists( m_cache[NAO::identifier()].toString(), variant.toString() );
}

void Nepomuk2::ResourceData::propertyRemoved( const Types::Property &prop, const QVariant &value_ )
{
    QMutexLocker lock(&m_modificationMutex);
    QHash<QUrl, Variant>::iterator cacheIt = m_cache.find(prop.uri());
    if(cacheIt != m_cache.end()) {
        Variant v = *cacheIt;
        const Variant value(value_);
        QList<Variant> vl = v.toVariantList();
        if(vl.contains(value)) {
            vl.removeAll(value);
            if(vl.isEmpty()) {
                updateKickOffLists(prop.uri(), Variant());
                m_cache.erase(cacheIt);
            }
            else {
                // The kickoff properties (nao:identifier and nie:url) both have a cardinality of 1
                // If we have more than one value, then the properties must not be any of them
                if( vl.size() == 1 )
                    updateKickOffLists(prop.uri(), vl.first());
                cacheIt.value() = vl;
            }
        }
    }
}

void Nepomuk2::ResourceData::propertyAdded( const Types::Property &prop, const QVariant &value )
{
    QMutexLocker lock(&m_modificationMutex);
    const Variant var(value);
    updateKickOffLists(prop.uri(), var);
    m_cache[prop.uri()].append(var);
}
