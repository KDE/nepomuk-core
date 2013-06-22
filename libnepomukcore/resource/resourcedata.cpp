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

#include <kdbusconnectionpool.h>
#include <QtDBus/QDBusMessage>

using namespace Soprano;

#define MAINMODEL (m_rm->m_manager->mainModel())

using namespace Soprano::Vocabulary;
using namespace Nepomuk2::Vocabulary;

Nepomuk2::ResourceData::ResourceData( const QUrl& uri, const QUrl& kickOffUri, const QUrl& type, ResourceManagerPrivate* rm )
    : m_uri(uri),
      m_type(type.isEmpty() ? RDFS::Resource() : type),
      m_dataMutex(QMutex::Recursive),
      m_cacheDirty(false),
      m_addedToWatcher(false),
      m_watchEnabled(false),
      m_rm(rm)
{
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
    resetAll();
}


bool Nepomuk2::ResourceData::isFile()
{
    QMutexLocker lock(&m_dataMutex);
    return( m_uri.scheme() == QLatin1String("file") ||
            m_nieUrl.scheme() == QLatin1String("file") ||
            hasProperty( RDF::type(), NFO::FileDataObject() ) );
    // The hasProperty should be const - It shouldn't load the entire cache. Maybe
}


QUrl Nepomuk2::ResourceData::uri() const
{
    QMutexLocker lock(&m_dataMutex);
    return m_uri;
}


QUrl Nepomuk2::ResourceData::type()
{
    QUrl mainType = RDFS::Resource();

    if( !load() )
        return mainType;

    QMutexLocker lock(&m_dataMutex);
    QList<QUrl> types = m_cache.value(RDF::type()).toUrlList();
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


void Nepomuk2::ResourceData::resetAll()
{
    QMutexLocker rmMutexLocker(&m_rm->mutex);
    QMutexLocker locker(&m_dataMutex);
    // remove us from all caches (store() will re-insert us later if necessary)

    // IMPORTANT:
    // Remove from the kickOffList before removing from the resource watcher
    // This is required cause otherwise the Resource::fromResourceUri creates a new
    // resource which is correctly identified to the ResourceData (this), and it is
    // then deleted, which calls resetAll and this cycle continues.
    const QString nao = m_cache.value(NAO::identifier()).toString();
    m_rm->m_identifierKickOff.remove( nao );
    const QUrl nieUrl = m_cache.value(NIE::url()).toUrl();
    m_rm->m_urlKickOff.remove( nieUrl );

    if( !m_uri.isEmpty() ) {
        m_rm->m_initializedData.remove( m_uri );
        removeFromWatcher();
    }

    // reset all variables
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
    else {
        QMutexLocker lock(&m_dataMutex);
        return m_cache;
    }
}


bool Nepomuk2::ResourceData::hasProperty( const QUrl& uri )
{
    if( !load() )
        return false;

    QMutexLocker lock(&m_dataMutex);
    QHash<QUrl, Variant>::const_iterator it = m_cache.constFind( uri );
    if( it == m_cache.constEnd() )
        return false;

    return true;
}


bool Nepomuk2::ResourceData::hasProperty( const QUrl& p, const Variant& v )
{
    if( !load() )
        return false;

    QMutexLocker lock(&m_dataMutex);
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
    QMutexLocker lock(&m_dataMutex);

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
    QMutexLocker lock(&m_dataMutex);

    if ( m_uri.isEmpty() ) {
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

        QDBusConnection bus = KDBusConnectionPool::threadConnection();
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
        if( !m_naoIdentifier.isEmpty() ) {
            setProperty( NAO::identifier(), m_naoIdentifier );
            setProperty( NAO::prefLabel(), m_naoIdentifier );
            m_naoIdentifier.clear();
        }
        if( !m_nieUrl.isEmpty() ) {
            setProperty( NIE::url(), m_nieUrl );
            m_nieUrl.clear();
        }

        addToWatcher();
        lock.unlock();
        QMutexLocker rmlock(&m_rm->mutex);
        // Add us to the initialized data, i.e. make us "valid"
        //Note: once m_uri is non-empty, it doesn't change
        m_rm->m_initializedData.insert( m_uri, this );

    }

    return true;
}

// Caller must hold m_dataMutex
void Nepomuk2::ResourceData::addToWatcher()
{
    if( m_watchEnabled && !m_addedToWatcher ) {
        //Obey the locking rules: the rm mutex gets locked before the dataMutex.
        m_dataMutex.unlock();
        //It is safe to use m_uri with dataMutex unlocked because it is only modified to set it, 
        //and we check it is non-empty before calling addToWatcher.
        m_rm->addToWatcher( m_uri );
        m_dataMutex.lock();
        m_addedToWatcher = true;
    }
}

void Nepomuk2::ResourceData::removeFromWatcher()
{
    if( m_addedToWatcher ) {
        //Obey the locking rules: the rm mutex gets locked before the dataMutex.
        m_dataMutex.unlock();
        m_rm->removeFromWatcher( m_uri );
        m_dataMutex.lock();
        m_addedToWatcher = false;
    }
}


bool Nepomuk2::ResourceData::load()
{
    QMutexLocker lock(&m_dataMutex);
    if (!m_cacheDirty) {
        // Fast path: nothing to do.
        return true;
    }
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

    const QString newNaoIdentifier = m_cache.value(NAO::identifier()).toString();
    const QUrl newNieUrl = m_cache.value(NIE::url()).toUrl();
    m_cacheDirty = false;

    lock.unlock();

    QMutexLocker rmlock(&m_rm->mutex); // for updating the lists. Must be locked first
    updateIdentifierLists( oldNaoIdentifier, newNaoIdentifier );
    updateUrlLists( oldNieUrl, newNieUrl );

    return true;
}


void Nepomuk2::ResourceData::setProperty( const QUrl& uri, const Nepomuk2::Variant& value )
{
    Q_ASSERT( uri.isValid() );

    if( store() ) {
        // step 0: make sure this resource is in the store

        // update the store
        QVariantList varList;
        foreach( const Variant& var, value.toVariantList() ) {
            // make sure resource values are identified and in the store
            if( var.simpleType() == qMetaTypeId<Resource>() ) {
                Resource res = var.toResource();
                res.determineFinalResourceData();
                res.m_data->store();

                varList << res.uri();
            }
            else {
                varList << var.variant();
            }
        }

        QMutexLocker lock(&m_dataMutex);
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

        QDBusConnection bus = KDBusConnectionPool::threadConnection();
        QDBusMessage reply = bus.call( msg );
        if( reply.type() == QDBusMessage::ErrorMessage ) {
            //TODO: Set the error somehow
            kWarning() << reply.errorMessage();
            return;
        }

        const Nepomuk2::Variant oldvalue = m_cache.value(uri);
        // update the cache for now
        if( value.isValid() )
            m_cache[uri] = value;
        else
            m_cache.remove(uri);
        lock.unlock();
        // update the kickofflists
        updateKickOffLists( uri, oldvalue, value );
    }
}


void Nepomuk2::ResourceData::addProperty( const QUrl& uri, const Nepomuk2::Variant& value )
{
    Q_ASSERT( uri.isValid() );

    if( value.isValid() && store() ) {
        // step 0: make sure this resource is in the store

        // update the store
        QVariantList varList;
        foreach( const Nepomuk2::Variant var, value.toVariantList() ) {
            // make sure resource values are in the store
            if( var.simpleType() == qMetaTypeId<Resource>() ) {
                Resource res = var.toResource();
                res.determineFinalResourceData();
                res.m_data->store();

                varList << res.uri();
            }
            else {
                varList << var.variant();
            }
        }

        QMutexLocker lock(&m_dataMutex);
        QDBusMessage msg = QDBusMessage::createMethodCall( QLatin1String("org.kde.nepomuk.DataManagement"),
                                                           QLatin1String("/datamanagement"),
                                                           QLatin1String("org.kde.nepomuk.DataManagement"),
                                                           QLatin1String("addProperty") );
        msg.setArguments( QVariantList()
                          << DBus::convertUriList(QList<QUrl>() << m_uri)
                          << DBus::convertUri(uri)
                          << QVariant(DBus::normalizeVariantList(varList))
                          << KGlobal::mainComponent().componentName() );

        QDBusConnection bus = KDBusConnectionPool::threadConnection();
        QDBusMessage reply = bus.call( msg );
        if( reply.type() == QDBusMessage::ErrorMessage ) {
            //TODO: Set the error somehow
            kWarning() << reply.errorMessage();
            return;
        }

        const Nepomuk2::Variant oldvalue = m_cache.value(uri);
        // update the cache for now
        if( value.isValid() )
            m_cache[uri].append(value);
        lock.unlock();
        // update the kickofflists
        updateKickOffLists( uri, oldvalue, value );
    }
}


void Nepomuk2::ResourceData::removeProperty( const QUrl& uri )
{
    Q_ASSERT( uri.isValid() );
    QMutexLocker lock(&m_dataMutex);

    if( !m_uri.isEmpty() ) {
        QDBusMessage msg = QDBusMessage::createMethodCall( QLatin1String("org.kde.nepomuk.DataManagement"),
                                                           QLatin1String("/datamanagement"),
                                                           QLatin1String("org.kde.nepomuk.DataManagement"),
                                                           QLatin1String("removeProperties") );
        msg.setArguments( QVariantList()
                          << DBus::convertUri(m_uri)
                          << DBus::convertUri(uri)
                          << KGlobal::mainComponent().componentName() );

        QDBusConnection bus = KDBusConnectionPool::threadConnection();
        QDBusMessage reply = bus.call( msg );
        if( reply.type() == QDBusMessage::ErrorMessage ) {
            //TODO: Set the error somehow
            kWarning() << reply.errorMessage();
            return;
        }

        const Nepomuk2::Variant oldvalue = m_cache.value(uri);
        // Update the cache
        m_cache.remove( uri );
        // update the kickofflists
        lock.unlock();
        updateKickOffLists( uri, oldvalue, Variant() );
    }
}


void Nepomuk2::ResourceData::remove( bool recursive )
{
    Q_UNUSED(recursive)
    QMutexLocker lock(&m_dataMutex);

    if( !m_uri.isEmpty() ) {
        QDBusMessage msg = QDBusMessage::createMethodCall( QLatin1String("org.kde.nepomuk.DataManagement"),
                                                           QLatin1String("/datamanagement"),
                                                           QLatin1String("org.kde.nepomuk.DataManagement"),
                                                           QLatin1String("removeResources") );
        msg.setArguments( QVariantList()
                          << DBus::convertUriList(QList<QUrl>() << m_uri)
                          << 0 /* no flags */
                          << KGlobal::mainComponent().componentName());

        QDBusConnection bus = KDBusConnectionPool::threadConnection();
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
    QMutexLocker lock(&m_dataMutex);
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
    QMutexLocker lock(&m_dataMutex);
    return !m_uri.isEmpty() || !m_nieUrl.isEmpty() || !m_naoIdentifier.isEmpty();
}

void Nepomuk2::ResourceData::determineUri()
{
    QMutexLocker lock(&m_dataMutex);
    if( !m_uri.isEmpty() ) {
        return;
    }

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
        lock.unlock(); // respect mutex order
        QMutexLocker locker(&m_rm->mutex);
        lock.relock();
        m_cacheDirty = true;
        ResourceDataHash::iterator it = m_rm->m_initializedData.find(m_uri);
        if( it == m_rm->m_initializedData.end() ) {
            m_rm->m_initializedData.insert( m_uri, this );
        }
        else {
            ResourceData* foundData = it.value();

            // in case we get an already existing one we update all instances
            // using the old ResourceData to avoid the overhead of calling
            // determineUri over and over
            Q_FOREACH (Resource* res, m_resources) {
                res->m_data = foundData; // this can include our caller
                this->deref( res );
                foundData->ref( res );
            }
        }
    }
}


void Nepomuk2::ResourceData::invalidateCache()
{
    QMutexLocker lock(&m_dataMutex);
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

// Be very careful never to call this when m_dataMutex is held,
// unless the RM mutex was taken first, because the RM mutex must be taken
// before the dataMutex.
void Nepomuk2::ResourceData::updateKickOffLists(const QUrl& uri, const Nepomuk2::Variant &oldvalue, const Nepomuk2::Variant& newvalue)
{
    if( uri == NIE::url() || uri == NAO::identifier() ){
        QMutexLocker rmlock(&m_rm->mutex);
        if( uri == NIE::url() )
            updateUrlLists( oldvalue.toUrl(), newvalue.toUrl() );
        else if( uri == NAO::identifier() )
            updateIdentifierLists( oldvalue.toString(), newvalue.toString() );
    }
}

// Since propertyRemoved and propertyAdded are called from the ResourceManager with the RM mutex already locked, 
// it is ok for them to call updateKickOffLists while the dataMutex is locked.
void Nepomuk2::ResourceData::propertyRemoved( const Types::Property &prop, const QVariant &value_ )
{
    QMutexLocker lock(&m_dataMutex);
    const Variant value(value_);
    QList<Variant> vl = m_cache.value(prop.uri()).toVariantList();
    if( vl.contains(value) ) {
        //
        // Remove that element and and also remove all empty elements
        // This is required because the value maybe have been a resource
        // which has now been deleted, and no longer has a value
        QMutableListIterator<Variant> it(vl);
        while( it.hasNext() ) {
            Variant var = it.next();
            if( (var.isResource() && var.toUrl().isEmpty()) || var == value )
                it.remove();
        }
        if(vl.isEmpty()) {
            updateKickOffLists(prop.uri(), m_cache.value(prop.uri()), Variant());
            m_cache.remove(prop.uri());
        }
        else {
            // The kickoff properties (nao:identifier and nie:url) both have a cardinality of 1
            // If we have more than one value, then the properties must not be any of them
            if( vl.size() == 1 )
                updateKickOffLists(prop.uri(), m_cache.value(prop.uri()), vl.first());
            m_cache[prop.uri()] = vl;
        }
    }
}

void Nepomuk2::ResourceData::propertyAdded( const Types::Property &prop, const QVariant &value )
{
    QMutexLocker lock(&m_dataMutex);
    const Variant var(value);
    const Variant oldvalue = m_cache.value(prop.uri());
    if( !oldvalue.toVariantList().contains(var) ) {
        m_cache[prop.uri()].append(var);
    }
    updateKickOffLists(prop.uri(), oldvalue, var);
}

void Nepomuk2::ResourceData::setWatchEnabled(bool status)
{
    QMutexLocker lock(&m_dataMutex);
    if( m_watchEnabled != status ) {
        if( status )
            addToWatcher();
        else
            removeFromWatcher();

        m_watchEnabled = status;
    }
}

bool Nepomuk2::ResourceData::watchEnabled()
{
    QMutexLocker lock(&m_dataMutex);
    return m_watchEnabled;
}
