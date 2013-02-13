/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2010-2012 Sebastian Trueg <trueg@kde.org>
   Copyright (C) 2011-2013 Vishesh Handa <handa.vish@gmail.com>

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

#include "datamanagementmodel.h"
#include "classandpropertytree.h"
#include "resourcemerger.h"
#include "resourceidentifier.h"
#include "simpleresourcegraph.h"
#include "simpleresource.h"
#include "resourcewatchermanager.h"
#include "syncresource.h"
#include "nepomuktools.h"
#include "typecache.h"

#include <Soprano/Vocabulary/NRL>
#include <Soprano/Vocabulary/NAO>
#include <Soprano/Vocabulary/RDF>
#include <Soprano/Vocabulary/RDFS>

#include <Soprano/Graph>
#include <Soprano/QueryResultIterator>
#include <Soprano/StatementIterator>
#include <Soprano/NodeIterator>
#include <Soprano/Error/ErrorCode>
#include <Soprano/Parser>
#include <Soprano/Serializer>
#include <Soprano/PluginManager>
#include <Soprano/Util/SimpleStatementIterator>

#include <QtCore/QHash>
#include <QtCore/QCache>
#include <QtCore/QUrl>
#include <QtCore/QVariant>
#include <QtCore/QDateTime>
#include <QtCore/QUuid>
#include <QtCore/QSet>
#include <QtCore/QPair>
#include <QtCore/QFileInfo>

#include "nie.h"
#include "nfo.h"
#include "pimo.h"

#include <KDebug>
#include <KService>
#include <KServiceTypeTrader>
#include <KProtocolInfo>

#include <KIO/NetAccess>

#define STRIGI_INDEX_GRAPH_FOR "http://www.strigi.org/fields#indexGraphFor"

using namespace Nepomuk2::Vocabulary;
using namespace Soprano::Vocabulary;

//// TODO: do not allow to create properties or classes through the "normal" methods. Instead provide methods for it.
//// IDEAS:
//// 1. Somehow handle nie:hasPart (at least in describeResources - compare text annotations where we only want to annotate part of a text)

namespace {
    /// convert a hash of URL->URI mappings to N3, omitting empty URIs.
    /// This is a helper for the return type of DataManagementModel::resolveUrls
    QStringList resourceHashToN3(const QHash<QUrl, QUrl>& urls) {
        QStringList n3;
        QHash<QUrl, QUrl>::const_iterator end = urls.constEnd();
        for(QHash<QUrl, QUrl>::const_iterator it = urls.constBegin(); it != end; ++it) {
            if(!it.value().isEmpty())
                n3 << Soprano::Node::resourceToN3(it.value());
        }
        return n3;
    }

    QStringList nodesToN3(const QSet<Soprano::Node>& nodes) {
        QStringList n3;
        Q_FOREACH(const Soprano::Node& node, nodes) {
            n3 << node.toN3();
        }
        return n3;
    }

    QStringList nodesToN3(const QList<Soprano::Node>& nodes) {
        QStringList n3;
        Q_FOREACH(const Soprano::Node& node, nodes) {
            n3 << node.toN3();
        }
        return n3;
    }

    QStringList urlListToN3(const QList<QUrl>& uris) {
        QStringList n3;
        Q_FOREACH(const QUrl& uri, uris) {
            n3 << Soprano::Node::resourceToN3(uri);
        }
        return n3;
    }

    QStringList urlSetToN3(const QSet<QUrl>& uris) {
        QStringList n3;
        Q_FOREACH(const QUrl& uri, uris) {
            n3 << Soprano::Node::resourceToN3(uri);
        }
        return n3;
    }

    template<typename T> QString createResourceFilter(const T& resources, const QString& var, bool exclude = true) {
        QString filter = QString::fromLatin1("%1 in (%2)").arg(var, Nepomuk2::resourcesToN3(resources).join(QLatin1String(",")));
        if(exclude) {
            filter = QString::fromLatin1("!(%1)").arg(filter);
        }
        return filter;
    }

    /*
     * Creates a filter (without the "FILTER" keyword) which either excludes the \p propVar
     * as once of the metadata properties or forces it to be one.
     */
    QString createResourceMetadataPropertyFilter(const QString& propVar, bool exclude = true) {
        return createResourceFilter(QList<QUrl>()
                                    << NAO::created()
                                    << NAO::lastModified()
                                    << NAO::creator()
                                    << NAO::userVisible()
                                    << NIE::url(),
                                    propVar,
                                    exclude);
    }

    enum UriState {
        /// the URI is a URL which points to an existing local file
        ExistingFileUrl,
        /// the URI is a file URL which points to a non-existing local file
        NonExistingFileUrl,
        /// the URI is a URL which uses on of the protocols supported by KIO
        SupportedUrl,
        /// the URI is a nepomuk:/ URI
        NepomukUri,
        /// A uri of the form "_:id"
        BlankUri,
        /// A uri in the ontology
        OntologyUri,
        /// the URI is some other non-supported URI
        OtherUri
    };

    /// Check if a URL points to a local file. This should be the only place where the local file is stat'ed
    inline UriState uriState(const QUrl& uri, bool statLocalFiles = true) {
        if(uri.scheme() == QLatin1String("nepomuk")) {
            return NepomukUri;
        }
        else if(uri.scheme() == QLatin1String("file")) {
            if(!statLocalFiles ||
                    QFile::exists(uri.toLocalFile())) {
                return ExistingFileUrl;
            }
            else {
                return NonExistingFileUrl;
            }
        }
        else if(Nepomuk2::ClassAndPropertyTree::self()->contains(uri)) {
            return OntologyUri;
        }
        // if supported by kio
        else if( KProtocolInfo::isKnownProtocol(uri) ) {
            return SupportedUrl;
        }
        else if(uri.toString().startsWith("_:") ) {
            return BlankUri;
        }
        else {
            return OtherUri;
        }
    }

    inline Soprano::Node convertIfBlankUri(const QUrl &uri) {
        if( uri.toString().startsWith(QLatin1String("_:")) )
            return Soprano::Node( uri.toString().mid(2) );
        else
            return Soprano::Node( uri );
    }
}

class Nepomuk2::DataManagementModel::Private
{
public:
    ClassAndPropertyTree* m_classAndPropertyTree;
    ResourceWatcherManager* m_watchManager;

    /// a set of properties that are maintained by the service and cannot be changed by clients
    QSet<QUrl> m_protectedProperties;

    QCache<QString, QUrl> m_appCache;
    QMutex m_appCacheMutex;

    typedef QPair<QString, bool> GraphCacheItem;
    QCache<GraphCacheItem, QUrl> m_graphCache;
    QMutex m_graphCacheMutex;

    TypeCache* m_typeCache;
    QUrl m_nepomukGraph;
};

Nepomuk2::DataManagementModel::DataManagementModel(Nepomuk2::ClassAndPropertyTree* tree, Soprano::Model* model, QObject *parent)
    : Soprano::FilterModel(model),
      d(new Private())
{
    d->m_classAndPropertyTree = tree;
    d->m_watchManager = new ResourceWatcherManager(this);
    d->m_typeCache = new TypeCache(this);
    d->m_appCache.setMaxCost( 10 );

    setParent(parent);

    // meta data properties are protected. This means they cannot be removed. But they
    // can be set.
    d->m_protectedProperties.insert(NAO::created());
    d->m_protectedProperties.insert(NAO::creator());
    d->m_protectedProperties.insert(NAO::lastModified());
    d->m_protectedProperties.insert(NAO::userVisible());
    d->m_protectedProperties.insert(NIE::url());

    // Create a "Nepomuk" application graph
    d->m_nepomukGraph = fetchGraph(QLatin1String("nepomuk"));

    // Specially add <nepomuk:/me> cause the clients cannot
    // TODO: Add the fullname, email and other details
    if( !containsAnyStatement( QUrl("nepomuk:/me"), Soprano::Node(), Soprano::Node() ) ) {
        addStatement( QUrl("nepomuk:/me"), RDF::type(), PIMO::Person(), d->m_nepomukGraph );
    }

    // Enable auto-commit after each statement change
    // This is required because multiple parallel calls to storeResources and removeResources
    // can often cause transaction failures due to deadlocks in virtuoso
    // function documentation - http://docs.openlinksw.com/virtuoso/fn_log_enable.html
    QLatin1String command("log_enable( 3 )");
    executeQuery( command, Soprano::Query::QueryLanguageUser, QLatin1String("sql") );
}

void Nepomuk2::DataManagementModel::clearCache()
{
    QMutexLocker lock2( &d->m_graphCacheMutex );
    d->m_graphCache.clear();
    lock2.unlock();

    QMutexLocker lock( &d->m_appCacheMutex );
    d->m_appCache.clear();
    lock.unlock();

    d->m_typeCache->clear();
    d->m_nepomukGraph = fetchGraph(QLatin1String("nepomuk"));
}


Nepomuk2::DataManagementModel::~DataManagementModel()
{
    delete d->m_typeCache;
    delete d;
}


Soprano::Error::ErrorCode Nepomuk2::DataManagementModel::updateModificationDate(const QUrl& resource,
                                                                                const QDateTime& date)
{
    return updateModificationDate(QSet<QUrl>() << resource, date);

}

Soprano::Error::ErrorCode Nepomuk2::DataManagementModel::updateModificationDate(const QSet<QUrl>& resources,
                                                                                const QDateTime& date)
{
    if(resources.isEmpty()) {
        return Soprano::Error::ErrorNone;
    }

    // TODO: Do this in one command?
    foreach(const QUrl& resource, resources) {
        Soprano::Error::ErrorCode c = removeAllStatements(resource, NAO::lastModified(), Soprano::Node());
        if (c != Soprano::Error::ErrorNone)
            return c;
        addStatement(resource, NAO::lastModified(), Soprano::LiteralValue( date ), d->m_nepomukGraph);
    }

    return Soprano::Error::ErrorNone;
}

void Nepomuk2::DataManagementModel::addProperty(const QList<QUrl> &resources, const QUrl &property, const QVariantList &values, const QString &app)
{
    //
    // Check parameters
    //
    if(app.isEmpty()) {
        setError(QLatin1String("addProperty: Empty application specified. This is not supported."), Soprano::Error::ErrorInvalidArgument);
        return;
    }
    if(resources.isEmpty()) {
        setError(QLatin1String("addProperty: No resource specified."), Soprano::Error::ErrorInvalidArgument);
        return;
    }
    foreach( const QUrl & res, resources ) {
        if(res.isEmpty()) {
            setError(QLatin1String("addProperty: Encountered empty resource URI."), Soprano::Error::ErrorInvalidArgument);
            return;
        }
    }
    if(property.isEmpty()) {
        setError(QLatin1String("addProperty: Property needs to be specified."), Soprano::Error::ErrorInvalidArgument);
        return;
    }
    if(values.isEmpty()) {
        setError(QLatin1String("addProperty: Values needs to be specified."), Soprano::Error::ErrorInvalidArgument);
        return;
    }
    if(d->m_protectedProperties.contains(property)) {
        setError(QString::fromLatin1("addProperty: %1 is a protected property which can only be set.").arg(property.toString()),
                 Soprano::Error::ErrorInvalidArgument);
        return;
    }


    //
    // Convert all values to RDF nodes. This includes the property range check and conversion of local file paths to file URLs
    //
    const QSet<Soprano::Node> nodes = d->m_classAndPropertyTree->variantListToNodeSet(values, property);
    if(nodes.isEmpty()) {
        setError(d->m_classAndPropertyTree->lastError());
        return;
    }


    //
    // We need to ensure that no client removes any ontology constructs or graphs,
    // we can check this before resolving file URLs since no protected resource will
    // ever have a nie:url
    //
    if(containsResourceWithProtectedType(QSet<QUrl>::fromList(resources))) {
        return;
    }


    clearError();


    //
    // Hash to keep mapping from provided URL/URI to resource URIs
    //
    QList<Soprano::Node> resolvedNodes;


    if(property == NIE::url()) {
        if(resources.count() != 1) {
            setError(QLatin1String("addProperty: no two resources can have the same nie:url."), Soprano::Error::ErrorInvalidArgument);
            return;
        }
        else if(nodes.count() > 1) {
            setError(QLatin1String("addProperty: One resource can only have one nie:url."), Soprano::Error::ErrorInvalidArgument);
            return;
        }

        if(!nodes.isEmpty()) {
            // check if another resource already uses the URL - no two resources can have the same URL at the same time
            // CAUTION: There is one theoretical situation in which this breaks (more than this actually):
            //          A file is moved and before the nie:url is updated data is added to the file in the new location.
            //          At this point the file is there twice and the data should ideally be merged. But how to decide that
            //          and how to distiguish between that situation and a file overwrite?
            if(containsAnyStatement(Soprano::Node(), NIE::url(), *nodes.constBegin())) {
                setError(QLatin1String("addProperty: No two resources can have the same nie:url at the same time."), Soprano::Error::ErrorInvalidArgument);
                return;
            }
            else if(containsAnyStatement(resources.first(), NIE::url(), Soprano::Node())) {
                // TODO: this can be removed as soon as nie:url gets a max cardinality of 1
                // vHanda: nie:url as of KDE 4.6 has a max cardinality of 1. This is due to the
                //         nepomuk resource identification ontology
                setError(QLatin1String("addProperty: One resource can only have one nie:url."), Soprano::Error::ErrorInvalidArgument);
                return;
            }

            // nie:url is the only property for which we do not want to resolve URLs
            resolvedNodes << *nodes.constBegin();
        }
    }
    else {
        resolvedNodes = resolveNodes(nodes, app);
        if(lastError()) {
            return;
        }
    }


    //
    // Resolve local file URLs (we need to hash the values since we do not want to write anything yet)
    //
    QList<QUrl> resolvedUris = resolveUrls(resources, app);
    if(lastError()) {
        return;
    }


    //
    // Check cardinality conditions
    //
    const int maxCardinality = d->m_classAndPropertyTree->maxCardinality(property);
    if( maxCardinality == 1 ) {
        // check if any of the resources already has a value set which differs from the one we want to add

        // an empty hashed value means that the resource for a file URL does not exist yet. Thus, there is
        // no need to filter it out. We basically only need to check if any value exists.
        QString valueFilter;
        if(resolvedNodes.first().isValid()) {
            valueFilter = QString::fromLatin1("FILTER(?v!=%3) . ")
                    .arg(resolvedNodes.first().toN3());
        }

        QStringList terms;
        Q_FOREACH(const QUrl& res, resolvedUris) {
            terms << QString::fromLatin1("%1 %2 ?v . %3")
                        .arg(Soprano::Node::resourceToN3(res),
                             Soprano::Node::resourceToN3(property),
                             valueFilter);
        }

        const QString cardinalityQuery = QString::fromLatin1("ask where { { %1 } }")
                .arg(terms.join(QLatin1String("} UNION {")));

        if(executeQuery(cardinalityQuery, Soprano::Query::QueryLanguageSparql).boolValue()) {
            setError(QString::fromLatin1("%1 has cardinality of 1. At least one of the resources already has a value set that differs from the new one.")
                     .arg(Soprano::Node::resourceToN3(property)), Soprano::Error::ErrorInvalidArgument);
            return;
        }
    }


    //
    // Do the actual work
    //
    addProperty(resolvedUris, property, resolvedNodes, app, true /* signal propertyChanged */);
}


// setting a property can be implemented by way of addProperty. All we have to do before calling addProperty is to remove all
// the values that are not in the setProperty call
void Nepomuk2::DataManagementModel::setProperty(const QList<QUrl> &resources, const QUrl &property, const QVariantList &values, const QString &app)
{
    //
    // Special case: setting to the empty list
    //
    if(values.isEmpty()) {
        removeProperties(resources, QList<QUrl>() << property, app);
        return;
    }

    //
    // Check parameters
    //
    if(app.isEmpty()) {
        setError(QLatin1String("setProperty: Empty application specified. This is not supported."), Soprano::Error::ErrorInvalidArgument);
        return;
    }
    if(resources.isEmpty()) {
        setError(QLatin1String("setProperty: No resource specified."), Soprano::Error::ErrorInvalidArgument);
        return;
    }
    foreach( const QUrl & res, resources ) {
        if(res.isEmpty()) {
            setError(QLatin1String("setProperty: Encountered empty resource URI."), Soprano::Error::ErrorInvalidArgument);
            return;
        }
    }
    if(property.isEmpty()) {
        setError(QLatin1String("setProperty: Property needs to be specified."), Soprano::Error::ErrorInvalidArgument);
        return;
    }


    //
    // Convert all values to RDF nodes. This includes the property range check and conversion of local file paths to file URLs
    //
    const QSet<Soprano::Node> nodes = d->m_classAndPropertyTree->variantListToNodeSet(values, property);
    if(nodes.isEmpty()) {
        setError(d->m_classAndPropertyTree->lastError());
        return;
    }


    //
    // We need to ensure that no client removes any ontology constructs or graphs,
    // we can check this before resolving file URLs since no protected resource will
    // ever have a nie:url
    //
    if(containsResourceWithProtectedType(QSet<QUrl>::fromList(resources))) {
        return;
    }


    clearError();


    //
    // Hash to keep mapping from provided URL/URI to resource URIs
    //
    QList<Soprano::Node> resolvedNodes;

    //
    // Setting nie:url on file resources also means updating nfo:fileName and possibly nie:isPartOf
    //
    if(property == NIE::url()) {
        if(resources.count() != 1) {
            setError(QLatin1String("setProperty: no two resources can have the same nie:url."), Soprano::Error::ErrorInvalidArgument);
            return;
        }
        else if(nodes.count() > 1) {
            setError(QLatin1String("setProperty: One resource can only have one nie:url."), Soprano::Error::ErrorInvalidArgument);
            return;
        }

        // check if another resource already uses the URL - no two resources can have the same URL at the same time
        // CAUTION: There is one theoretical situation in which this breaks (more than this actually):
        //          A file is moved and before the nie:url is updated data is added to the file in the new location.
        //          At this point the file is there twice and the data should ideally be merged. But how to decide that
        //          and how to distiguish between that situation and a file overwrite?
        QString query = QString::fromLatin1("select ?r where { ?r nie:url %1 . }")
                        .arg( nodes.constBegin()->toN3() );

        Soprano::QueryResultIterator it = executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
        if( it.next() ) {
            if( it[0] != resources.first() ) {
                setError(QLatin1String("setProperty: No two resources can have the same nie:url at the same time."), Soprano::Error::ErrorInvalidArgument);
                return;
            }
        }

        if(updateNieUrlOnLocalFile(resources.first(), nodes.constBegin()->uri())) {
            return;
        }

        // nie:url is the only property for which we do not want to resolve URLs
        resolvedNodes << *nodes.constBegin();
    }
    else {
        resolvedNodes = resolveNodes(nodes, app);
        if(lastError()) {
            return;
        }
    }

    //
    // Resolve local file URLs
    //
    QList<QUrl> resolvedUris = resolveUrls(resources, app);
    if(lastError()) {
        return;
    }

    //
    // Remove values that are not wanted anymore
    //
    const QStringList uriHashN3 = urlListToN3(resolvedUris);
    if(!uriHashN3.isEmpty()) {
        QHash<QUrl, QList<Soprano::Node> > valuesToRemove;
        const QSet<Soprano::Node> setValues = QSet<Soprano::Node>::fromList(resolvedNodes);
        QList<Soprano::BindingSet> existing
                = executeQuery(QString::fromLatin1("select ?r ?v where { ?r %1 ?v . FILTER(?r in (%2)) . }")
                               .arg(Soprano::Node::resourceToN3(property),
                                    uriHashN3.join(QLatin1String(","))),
                               Soprano::Query::QueryLanguageSparql).allBindings();
        Q_FOREACH(const Soprano::BindingSet& binding, existing) {
            if(!setValues.contains(binding["v"])) {
                removeAllStatements(binding["r"], property, binding["v"]);
                valuesToRemove[binding["r"].uri()].append(binding["v"]);
            }
        }

        // construct the lists of old and new values
        QHash<QUrl, QList<Soprano::Node> > addedValues;

        //
        // And finally add the rest of the statements (only if there is anything to add)
        //
        if(!nodes.isEmpty()) {
            addedValues = addProperty(resolvedUris, property, resolvedNodes, app);
        }

        //
        // Inform interested parties about the changes
        //
        foreach(const QUrl& res, resolvedUris) {
            const QList<Soprano::Node> added = addedValues.value(res);
            const QList<Soprano::Node> removed = valuesToRemove.value(res);
            if(!added.isEmpty() || !removed.isEmpty()) {
                d->m_watchManager->changeProperty(res,
                                                  property,
                                                  added,
                                                  removed);
            }
        }
        if(!resolvedUris.isEmpty()) {
            d->m_watchManager->changeSomething();
        }
    }
    else {
        addProperty(resolvedUris, property, resolvedNodes, app, true);
    }
}

void Nepomuk2::DataManagementModel::removeProperty(const QList<QUrl> &resources, const QUrl &property, const QVariantList &values, const QString &app)
{
    // 1. remove the triples
    // 2. remove trailing graphs
    // 3. update resource mtime only if we actually change anything on the resource

    //
    // Check arguments
    //
    if(app.isEmpty()) {
        setError(QLatin1String("removeProperty: Empty application specified. This is not supported."), Soprano::Error::ErrorInvalidArgument);
        return;
    }
    if(resources.isEmpty()) {
        setError(QLatin1String("removeProperty: No resource specified."), Soprano::Error::ErrorInvalidArgument);
        return;
    }
    foreach( const QUrl & res, resources ) {
        if(res.isEmpty()) {
            setError(QLatin1String("removeProperty: Encountered empty resource URI."), Soprano::Error::ErrorInvalidArgument);
            return;
        }
    }
    if(property.isEmpty()) {
        setError(QLatin1String("removeProperty: Property needs to be specified."), Soprano::Error::ErrorInvalidArgument);
        return;
    }
    if(values.isEmpty()) {
        setError(QLatin1String("removeProperty: Values needs to be specified."), Soprano::Error::ErrorInvalidArgument);
        return;
    }
    if(d->m_protectedProperties.contains(property)) {
        setError(QString::fromLatin1("removeProperty: %1 is a protected property which can only be changed by the data management service itself.").arg(property.toString()),
                 Soprano::Error::ErrorInvalidArgument);
        return;
    }

    const QSet<Soprano::Node> valueNodes = d->m_classAndPropertyTree->variantListToNodeSet(values, property);
    if(valueNodes.isEmpty()) {
        setError(d->m_classAndPropertyTree->lastError());
        return;
    }


    clearError();


    //
    // Resolve file URLs
    //
    QSet<QUrl> resolvedResources = QSet<QUrl>::fromList(resolveUrls(resources, app));
    if(resolvedResources.isEmpty() || lastError()) {
        return;
    }

    QList<Soprano::Node> resolvedNodes = resolveNodes(valueNodes, app);
    if(resolvedNodes.isEmpty() || lastError()) {
        return;
    }


    //
    // We need to ensure that no client removes any ontology constructs or graphs
    //
    if(containsResourceWithProtectedType(resolvedResources)) {
        return;
    }


    //
    // Actually change data
    //
    foreach( const QUrl & res, resolvedResources ) {
        QString query = QString::fromLatin1("select distinct ?v where { %1 %2 ?v . FILTER(?v in (%3)). }")
                        .arg( Soprano::Node::resourceToN3(res),
                              Soprano::Node::resourceToN3(property),
                              nodesToN3(resolvedNodes).join(QLatin1String(",")) );

        const QList<Soprano::BindingSet> valueGraphs
                = executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference ).allBindings();

        QList<Soprano::Node> removedValues;
        foreach(const Soprano::BindingSet& binding, valueGraphs) {
            const Soprano::Node v = binding["v"];
            removeAllStatements( res, property, v );
            removedValues << v;
        }

        if(!removedValues.isEmpty()) {
            d->m_watchManager->changeProperty( res, property, QList<Soprano::Node>(), removedValues );
        }

        // we only update the mtime in case we actually remove anything
        if(!valueGraphs.isEmpty()) {
            // If the resource is now empty we remove it completely
            if(!doesResourceExist(res)) {
                removeResources(QList<QUrl>() << res, NoRemovalFlags, app);
            }
            else {
                updateModificationDate(res);
            }
        }
    }
}

void Nepomuk2::DataManagementModel::removeProperties(const QList<QUrl> &resources, const QList<QUrl> &properties, const QString &app)
{
    // 1. remove the triples
    // 2. remove trailing graphs
    // 3. update resource mtime

    //
    // Check arguments
    //
    if(app.isEmpty()) {
        setError(QLatin1String("removeProperties: Empty application specified. This is not supported."), Soprano::Error::ErrorInvalidArgument);
        return;
    }
    if(resources.isEmpty()) {
        setError(QLatin1String("removeProperties: No resource specified."), Soprano::Error::ErrorInvalidArgument);
        return;
    }
    foreach( const QUrl & res, resources ) {
        if(res.isEmpty()) {
            setError(QLatin1String("removeProperties: Encountered empty resource URI."), Soprano::Error::ErrorInvalidArgument);
            return;
        }
    }
    if(properties.isEmpty()) {
        setError(QLatin1String("removeProperties: No properties specified."), Soprano::Error::ErrorInvalidArgument);
        return;
    }
    foreach(const QUrl& property, properties) {
        if(property.isEmpty()) {
            setError(QLatin1String("removeProperties: Encountered empty property URI."), Soprano::Error::ErrorInvalidArgument);
            return;
        }
        else if(d->m_protectedProperties.contains(property)) {
            setError(QString::fromLatin1("removeProperties: %1 is a protected property which can only be changed by the data management service itself.").arg(property.toString()),
                     Soprano::Error::ErrorInvalidArgument);
            return;
        }
    }


    clearError();


    //
    // Resolve file URLs, we can simply ignore the non-existing file resources which are reflected by empty resolved URIs
    //
    QSet<QUrl> resolvedResources = QSet<QUrl>::fromList(resolveUrls(resources, app));
    if(resolvedResources.isEmpty() || lastError()) {
        return;
    }


    //
    // We need to ensure that no client removes any ontology constructs or graphs
    //
    if(containsResourceWithProtectedType(resolvedResources)) {
        return;
    }


    //
    // Actually change data
    //
    foreach( const QUrl & res, resolvedResources ) {
        QSet<Soprano::Node> propertiesToRemove;
        QMultiHash<QUrl, Soprano::Node> propertyValues;
        Soprano::QueryResultIterator it
                = executeQuery(QString::fromLatin1("select distinct ?p ?v where { %1 ?p ?v . FILTER(?p in (%2)) . }")
                               .arg(Soprano::Node::resourceToN3(res),
                                    resourcesToN3(properties).join(QLatin1String(","))),
                               Soprano::Query::QueryLanguageSparql);
        while(it.next()) {
            propertiesToRemove.insert(it["p"]);
            propertyValues.insert( it["p"].uri(), it["v"] );
        }

        // remove the data
        foreach(const Soprano::Node& property, propertiesToRemove) {
            removeAllStatements( res, property, Soprano::Node() );
        }

        // inform interested parties
        for(QMultiHash<QUrl, Soprano::Node>::const_iterator it = propertyValues.constBegin();
            it != propertyValues.constEnd(); ) {
            QList<Soprano::Node> values;
            values << it.value();

            const QUrl property = it.key();
            for( ++it; it != propertyValues.constEnd() && it.key() == property; ++it ) {
                values << it.value();
            }

            d->m_watchManager->changeProperty(res, property, QList<Soprano::Node>(), values);
        }

        // we only update the mtime in case we actually remove anything
        if(!propertiesToRemove.isEmpty()) {
            // If the resource is now empty we remove it completely
            if(!doesResourceExist(res)) {
                removeResources(QList<QUrl>() << res, NoRemovalFlags, app);
            }
            else {
                updateModificationDate(res);
            }
        }
    }
}


QUrl Nepomuk2::DataManagementModel::createResource(const QList<QUrl> &types, const QString &label, const QString &description, const QString &app)
{
    // 1. create an new graph
    // 2. simplify the types
    // 3. check if the app exists, if not create it in the new graph
    // 4. create the new resource in the new graph
    // 5. return the resource's URI

    //
    // Check parameters
    //
    if(app.isEmpty()) {
        setError(QLatin1String("createResource: Empty application specified. This is not supported."), Soprano::Error::ErrorInvalidArgument);
        return QUrl();
    }
    QSet<QUrl> newTypes = types.toSet();
    QMutableSetIterator<QUrl> iterator( newTypes );
    while( iterator.hasNext() ) {
        const QUrl type = iterator.next();
        if(type.isEmpty()) {
            iterator.remove();
            continue;
        }

        if(!d->m_classAndPropertyTree->isKnownClass(type)) {
            QString error = QString::fromLatin1("createResource: Encountered invalid type URI %1").arg( Soprano::Node::resourceToN3(type) );
            setError(error, Soprano::Error::ErrorInvalidArgument);
            return QUrl();
        }
    }

    if( newTypes.isEmpty() ) {
        newTypes << RDFS::Resource();
    }

    clearError();

    // Simplify the types
    QSetIterator<QUrl> it( newTypes );
    while( it.hasNext() ) {
        const QUrl &type = it.next();
        QSet<QUrl> superTypes = d->m_classAndPropertyTree->allParents( type );

        foreach( const QUrl& parent, superTypes ) {
            QSet< QUrl >::iterator iter = newTypes.find( parent );
            if( iter != newTypes.end() ) {
                newTypes.erase( iter );
            }
        }
    }

    // create new URIs and a new graph
    const QUrl graph = fetchGraph(app);
    const QUrl resUri = createUri(ResourceUri);
    const QString resN3 = Soprano::Node::resourceToN3(resUri);

    // add provided metadata
    QString command = QString::fromLatin1("sparql insert into %1 { %2 ")
                      .arg( Soprano::Node::resourceToN3(graph),
                            resN3 );

    if(!label.isEmpty()) {
        Soprano::LiteralValue lv = Soprano::LiteralValue::createPlainLiteral(label);
        command += QString::fromLatin1(" nao:prefLabel %1 ;")
                   .arg( Soprano::Node::literalToN3(lv) );
    }
    if(!description.isEmpty()) {
        Soprano::LiteralValue lv = Soprano::LiteralValue::createPlainLiteral(description);
        command += QString::fromLatin1(" nao:description %1 ;")
                   .arg( Soprano::Node::literalToN3(lv) );
    }

    command += QString::fromLatin1("a %1 . }").arg( urlSetToN3(newTypes).join(",") );
    executeQuery( command, Soprano::Query::QueryLanguageUser, QLatin1String("sql") );

    // add basic metadata to the new resource in the nepomuk graph
    command = QString::fromLatin1("sparql insert into %1 { %2 ")
              .arg( Soprano::Node::resourceToN3(d->m_nepomukGraph), resN3 );

    Soprano::LiteralValue now( QDateTime::currentDateTime() );
    command += QString::fromLatin1(" nao:lastModified %1 ; nao:created %1 . }")
               .arg( Soprano::Node::literalToN3(now) );

    executeQuery( command, Soprano::Query::QueryLanguageUser, QLatin1String("sql") );

    // inform interested parties
    d->m_watchManager->createResource(resUri, newTypes.toList());
    d->m_watchManager->changeSomething();

    return resUri;
}

void Nepomuk2::DataManagementModel::removeResources(const QList<QUrl> &resources, RemovalFlags flags, const QString &app)
{
    kDebug() << resources << app << flags;

    // 1. get all sub-resources and check if they are used by some other resource (not in the list of resources to remove)
    //    for the latter one can use a bif:exists and a !filter(?s in <s>, <s>, ...) - based on the value of force
    // 2. remove the resources and the sub-resources
    // 3. drop trailing graphs (could be optimized by enumerating the graphs up front)

    //
    // Check parameters
    //
    if(app.isEmpty()) {
        setError(QLatin1String("removeResources: Empty application specified. This is not supported."), Soprano::Error::ErrorInvalidArgument);
        return;
    }
    if(resources.isEmpty()) {
        setError(QLatin1String("removeResources: No resource specified."), Soprano::Error::ErrorInvalidArgument);
        return;
    }
    foreach( const QUrl & res, resources ) {
        if(res.isEmpty()) {
            setError(QLatin1String("removeResources: Encountered empty resource URI."), Soprano::Error::ErrorInvalidArgument);
            return;
        }
    }


    //
    // Resolve file URLs, we can simply ignore the non-existing file resources which are reflected by empty resolved URIs
    //
    QSet<QUrl> resolvedResources = QSet<QUrl>::fromList(resolveUrls(resources, app, false));
    if(resolvedResources.isEmpty() || lastError()) {
        return;
    }


    //
    // We need to ensure that no client removes any ontology constructs or graphs
    //
    if(containsResourceWithProtectedType(resolvedResources)) {
        return;
    }


    clearError();


    //
    // Actually remove the data
    //
    removeAllResources(resolvedResources, flags, app);
}

void Nepomuk2::DataManagementModel::removeDataByApplication(const QList<QUrl> &resources, RemovalFlags flags, const QString &app)
{
    //
    // Check parameters
    //
    if(app.isEmpty()) {
        setError(QLatin1String("removeDataByApplication: Empty application specified. This is not supported."), Soprano::Error::ErrorInvalidArgument);
        return;
    }
    foreach( const QUrl & res, resources ) {
        if(res.isEmpty()) {
            setError(QLatin1String("removeDataByApplication: Encountered empty resource URI."), Soprano::Error::ErrorInvalidArgument);
            return;
        }
    }


    clearError();


    // we handle one special case below: legacy data from the file indexer
    const QUrl appRes = findApplicationResource(app, false);
    if(appRes.isEmpty() && app != QLatin1String("nepomukindexer")) {
        return;
    }


    //
    // Resolve file URLs, we can simply ignore the non-existing file resources which are reflected by empty resolved URIs
    //
    QSet<QUrl> resolvedResources = QSet<QUrl>::fromList(resolveUrls(resources, app, false));
    if(resolvedResources.isEmpty() || lastError()) {
        return;
    }


    //
    // Handle the sub-resources: we can delete all sub-resources of the deleted ones that are entirely defined by our app
    // and are not related by other resources.
    // this has to be done before deleting the resouces in resolvedResources. Otherwise the nao:hasSubResource relationships are already gone!
    //
    // Explanation of the query:
    // The query selects all subresources of the resources in resolvedResources.
    // It then filters out the sub-resources that have properties defined by other apps which are not metadata.
    //
    if(!appRes.isEmpty()) {
        QSet<QUrl> subResources;
        QSet<QUrl> currentResources = resolvedResources;
        int resCount = 0;
        do {
            resCount = resolvedResources.count();
            Soprano::QueryResultIterator it
                    = executeQuery(QString::fromLatin1("select ?r where { graph ?g { ?r ?p ?o . } . "
                                                       "?parent %1 ?r . "
                                                       "FILTER(?parent in (%2)) . "
                                                       "?g %3 %4 . "
                                                       "FILTER(!bif:exists((select (1) where { graph ?g2 { ?r ?p2 ?o2 . } . ?g2 %3 ?a2 . FILTER(?a2!=%4) . FILTER(%5) . }))) . "
                                                       "}")
                                   .arg(Soprano::Node::resourceToN3(NAO::hasSubResource()),
                                        resourcesToN3(currentResources).join(QLatin1String(",")),
                                        Soprano::Node::resourceToN3(NAO::maintainedBy()),
                                        Soprano::Node::resourceToN3(appRes),
                                        createResourceMetadataPropertyFilter(QLatin1String("?p2"))),
                                   Soprano::Query::QueryLanguageSparqlNoInference);
            currentResources.clear();
            while(it.next()) {
                currentResources << it[0].uri();
            }
            resolvedResources += currentResources;
            subResources += currentResources;
        } while(resCount < resolvedResources.count());

        //
        // Now subResources contains the list of all possible sub-resources to delete.
        // We now have to remove all those which have links from the "outside", ie. resources
        // that are not in our resolvedResources.
        // We do this iterative until nothing changes anymore by removing the sub-resources
        // have are linked from the "outside".
        //
        QSet<QUrl> excludedSubResources = resolvedResources;
        bool firstIteration = true;
        while(!subResources.isEmpty() && !excludedSubResources.isEmpty()) {
            Soprano::QueryResultIterator it
                    = executeQuery(QString::fromLatin1("select distinct ?r where { "
                                                       "?r2 ?p ?r ."
                                                       "FILTER(%1) . "
                                                       "FILTER(%2) . }" )
                                   .arg(createResourceFilter(subResources, QLatin1String("?r"), false),
                                        createResourceFilter(excludedSubResources, QLatin1String("?r2"), firstIteration)),
                                   Soprano::Query::QueryLanguageSparqlNoInference);
            excludedSubResources.clear();
            while(it.next()) {
                excludedSubResources << it[0].uri();
            }
            subResources -= excludedSubResources;
            resolvedResources -= excludedSubResources;

            // in all subsequent queries we are only interested in links coming from the excluded sub-resources
            // rather than coming from anything but our resolved resources.
            firstIteration = false;
        }
    }


    QString query = QString::fromLatin1("select distinct ?g where { ?g nao:maintainedBy %1 . }")
                          .arg( Soprano::Node::resourceToN3(appRes) );

    Soprano::QueryResultIterator it = executeQuery(query, Soprano::Query::QueryLanguageSparqlNoInference);
    QList<QUrl> graphs;
    while( it.next() ) {
        graphs << it[0].uri();
    }

    //
    // Shorten the list of resources to the ones that are actually in our graphs
    //
    QString graphN3 = urlListToN3(graphs).join(QLatin1String(","));
    QString resN3 = urlSetToN3(resolvedResources).join(QLatin1String(","));

    query = QString::fromLatin1("select distinct ?r where { graph ?g { ?r ?p ?o. } FILTER(?g in (%1)) ."
                                "FILTER(?r in (%2)) . }")
            .arg( graphN3, resN3 );

    QList<QUrl> finalResourcesList;
    it = executeQuery( query, Soprano::Query::QueryLanguageSparql );
    while( it.next() )
        finalResourcesList << it[0].uri();

    resolvedResources = finalResourcesList.toSet();
    resN3 = urlListToN3(finalResourcesList).join(QLatin1String(","));

    //
    // Get modified resources
    //
    QString notInGraphs = graphN3;
    notInGraphs += QString::fromLatin1(",%1").arg(Soprano::Node::resourceToN3(d->m_nepomukGraph));

    query = QString::fromLatin1("select distinct ?r where { "
                                " graph ?g1 { ?r ?p ?o. } "
                                " graph ?g2 { ?r ?p2 ?o2 . } "
                                " FILTER(?g1 in (%1)) . FILTER(?r in (%2)) . "
                                " FILTER(!(?g2 in (%3))) . }")
                    .arg( graphN3, resN3, notInGraphs );

    it = executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
    QSet<QUrl> modifiedResources;
    while( it.next() ) {
        modifiedResources << it[0].uri();
    }

    query = QString::fromLatin1("select distinct ?r2 where { "
                                " graph ?g { ?r2 ?p ?r . FILTER(?r in (%1)) .}"
                                " FILTER(?g in (%2))  }")
            .arg( resN3, graphN3 );

    it = executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
    while( it.next() ) {
        QUrl uri = it[0].uri();
        if( !resolvedResources.contains(uri) )
            modifiedResources << uri;
    }
    updateModificationDate(modifiedResources);

    foreach(const QUrl& graph, graphs) {
        QString deleteCommand = QString::fromLatin1("sparql delete from %1 { ?r ?p ?o. } where { "
                                                    "?r ?p ?o. FILTER(?r in (%2)). }")
                                .arg( Soprano::Node::resourceToN3(graph), resN3 );

        executeQuery( deleteCommand, Soprano::Query::QueryLanguageUser, QLatin1String("sql") );

        deleteCommand = QString::fromLatin1("sparql delete from %1 { ?o ?p ?r. } where { "
                                            "?o ?p ?r. FILTER(?r in (%2)). }")
                                .arg( Soprano::Node::resourceToN3(graph), resN3 );

        executeQuery( deleteCommand, Soprano::Query::QueryLanguageUser, QLatin1String("sql") );
    }

    // From the nepomuk graph only remove the resources which should be deleted completely
    resolvedResources.subtract( modifiedResources );

    if( resolvedResources.count() ) {
        QString deleteCommand = QString::fromLatin1("sparql delete from %1 { ?r ?p ?o. } where { "
                                                    "?r ?p ?o. FILTER(?r in (%2)). }")
                                .arg( Soprano::Node::resourceToN3(d->m_nepomukGraph),
                                      urlSetToN3(resolvedResources).join(",") );
        executeQuery( deleteCommand, Soprano::Query::QueryLanguageUser, QLatin1String("sql") );
    }
}


void Nepomuk2::DataManagementModel::removeDataByApplication(RemovalFlags flags, const QString &app)
{
    //
    // Check parameters
    //
    if(app.isEmpty()) {
        setError(QLatin1String("removeDataByApplication: Empty application specified. This is not supported."), Soprano::Error::ErrorInvalidArgument);
        return;
    }

    clearError();

    const QUrl appRes = findApplicationResource(app, false);
    if(appRes.isEmpty()) {
        return;
    }

    QString query = QString::fromLatin1("select ?g where { ?g nao:maintainedBy %1 . }")
                    .arg( Soprano::Node::resourceToN3(appRes) );

    Soprano::QueryResultIterator it = executeQuery(query, Soprano::Query::QueryLanguageSparqlNoInference);
    QList<QUrl> graphs;
    while( it.next() ) {
        graphs << it[0].uri();
    }

    // TODO: Find a faster way to update all the nao:lastModified of all the resources in the graph
    QSet<QUrl> modifiedResources;

    QString graphN3 = urlListToN3(graphs).join(QLatin1String(","));
    QString notInGraphs = graphN3;
    notInGraphs += QString::fromLatin1(",%1").arg(Soprano::Node::resourceToN3(d->m_nepomukGraph));

    // Fetch resources to modify
    query = QString::fromLatin1("select distinct ?r where { graph ?g1 { ?r ?p ?o. } "
                                " graph ?g2 { ?r ?p2 ?o2 . } "
                                " FILTER(?g1 in (%1)) . "
                                " FILTER(!(?g2 in (%2))) . }")
                    .arg( graphN3, notInGraphs );

    it = executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
    while( it.next() ) {
        modifiedResources << it[0].uri();
    }

    updateModificationDate( modifiedResources );

    QSet<QUrl> resourcesToRemove;
    foreach(const QUrl& graph, graphs) {
        QString query = QString::fromLatin1("select distinct ?r where { graph %1 { ?r ?p ?o. } }")
                        .arg( Soprano::Node::resourceToN3(graph) );

        Soprano::QueryResultIterator it = executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
        while( it.next() )
            resourcesToRemove << it[0].uri();

        QString command = QString::fromLatin1("clear graph %1").arg(Soprano::Node::resourceToN3(graph));
        executeQuery( command, Soprano::Query::QueryLanguageSparqlNoInference );
    }

    resourcesToRemove.subtract( modifiedResources );
    if( resourcesToRemove.count() ) {
        query = QString::fromLatin1("delete from %2 { ?r ?p ?o . } where { ?r ?p ?o. FILTER(?r in (%1)) . }")
                .arg( urlSetToN3(resourcesToRemove).join(","),
                      Soprano::Node::resourceToN3(d->m_nepomukGraph) );

        executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
    }
}


namespace {
    using namespace Nepomuk2;
    /**
     * Looks for pairs of SyncResources that are identical except for the uri and merges
     * then together into one SyncResource. If any other resources depend on them, they are
     * accordingly updated.
     */
    void mergeDuplicateSyncResources( QList<Sync::SyncResource>& syncResources ) {
        QHash<Soprano::Node, Soprano::Node> duplicateResources;
        do {
            duplicateResources.clear();

            QList<Sync::SyncResource> finalList;
            QHash< uint, const Sync::SyncResource* > syncResHash;
            foreach( const Sync::SyncResource& syncRes, syncResources ) {

                // Do not check for duplicates in non blank nodes
                if( syncRes.uri().url().startsWith("_:") ) {

                    //WARNING: The SyncResource qHash function does not use the uri while preparing the hash.
                    // If it did, then this would fail miserably.
                    uint hash = qHash( syncRes );
                    QHash< uint, const Sync::SyncResource* >::iterator it = syncResHash.find( hash );
                    if( it != syncResHash.end() ) {
                        // hash collision : Check the entire contents
                        const Sync::SyncResource r = *it.value();

                        // We can't use SyncResource::operator== cause that would check the uri as well
                        // and we don't care about the uri
                        if( r.QHash<KUrl,Soprano::Node>::operator==(syncRes) ) {
                            kDebug() << "Adding: " << syncRes.uri() << " " << it.value()->uri();
                            duplicateResources.insert( convertIfBlankUri( syncRes.uri() ),
                                                    convertIfBlankUri( it.value()->uri() ) );
                            continue;
                        }
                    }

                    syncResHash.insert( hash, &syncRes );
                }

                finalList << syncRes;
            }

            if( !duplicateResources.isEmpty() ) {
                // Resolve all duplicates in the finalList
                QMutableListIterator<Sync::SyncResource> it( finalList );
                while( it.hasNext() ) {
                    it.next();

                    QMutableHashIterator<KUrl, Soprano::Node> iter( it.value() );
                    while( iter.hasNext() ) {
                        iter.next();

                        const Soprano::Node node = iter.value();
                        if( node.isLiteral() || node.isEmpty() )
                            continue;
                        QHash< Soprano::Node, Soprano::Node >::const_iterator fit = duplicateResources.constFind( node );
                        if( fit != duplicateResources.constEnd() ) {
                            iter.setValue( fit.value() );
                        }
                    }
                }

                syncResources = finalList;
            }
        } while( !duplicateResources.isEmpty() );
    }
}

//// TODO: do not allow to create properties or classes this way
QHash<QUrl, QUrl> DataManagementModel::storeResources(const Nepomuk2::SimpleResourceGraph &resources,
                                                  const QString &app,
                                                  Nepomuk2::StoreIdentificationMode identificationMode,
                                                  Nepomuk2::StoreResourcesFlags flags,
                                                  const QHash<QUrl, QVariant> &additionalMetadata)
{
    bool discardable = false;
    if( QMultiHash<QUrl, QVariant>(additionalMetadata).contains(RDF::type(), NRL::DiscardableInstanceBase()) ) {
        discardable = true;
    }

    return storeResources(resources, app, discardable, identificationMode, flags);
}

QHash<QUrl, QUrl> DataManagementModel::storeResources(const SimpleResourceGraph& resources,
                                                      const QString& app,
                                                      bool discardable,
                                                      StoreIdentificationMode identificationMode,
                                                      StoreResourcesFlags flags)
{
    if(app.isEmpty()) {
        setError(QLatin1String("storeResources: Empty application specified. This is not supported."), Soprano::Error::ErrorInvalidArgument);
        return QHash<QUrl, QUrl>();
    }
    if(resources.isEmpty()) {
        clearError();
        return QHash<QUrl, QUrl>();
    }

    /// Holds the mapping of <file://url> to <nepomuk:/res/> uris
    QHash<QUrl, QUrl> resolvedNodes;

    //
    // Resolve the nie URLs which are present as resource uris
    //
    QSet<QUrl> blankResources;
    SimpleResourceGraph resGraph( resources );
    QList<SimpleResource> resGraphList = resGraph.toList();
    QMutableListIterator<SimpleResource> iter( resGraphList );
    while( iter.hasNext() ) {
        SimpleResource & res = iter.next();

        if( !res.isValid() ) {
            QString error = QString::fromLatin1("The resource with URI %1 is invalid.")
                            .arg( res.uri().toString() );
            setError(error, Soprano::Error::ErrorInvalidArgument);
            return QHash<QUrl, QUrl>();
        }

        const UriState state = uriState(res.uri());
        if(state == NepomukUri) {
            continue;
        }
        else if(state == BlankUri) {
            blankResources << res.uri();
        }
        // Handle nie urls
        else if(state == NonExistingFileUrl) {
            setError(QString::fromLatin1("Cannot store information about non-existing local files. File '%1' does not exist.").arg(res.uri().toLocalFile()), Soprano::Error::ErrorInvalidArgument);
            return QHash<QUrl, QUrl>();
        }
        else if(state == ExistingFileUrl || state == SupportedUrl) {
            const QUrl nieUrl = res.uri();
            QUrl newResUri = resolveUrl( nieUrl );
            if( lastError() )
                return QHash<QUrl, QUrl>();

            if( newResUri.isEmpty() ) {
                // Resolution of one url failed. Assign it a random blank uri
                newResUri = SimpleResource().uri(); // HACK: improveme

                if( state == ExistingFileUrl ) {
                    res.addProperty( RDF::type(), NFO::FileDataObject() );
                    if( QFileInfo( nieUrl.toLocalFile() ).isDir() )
                        res.addProperty( RDF::type(), NFO::Folder() );
                }
            }

            res.addProperty( NIE::url(), nieUrl );

            resolvedNodes.insert( nieUrl, newResUri );

            res.setUri( newResUri );
        }
        else if( state == OtherUri ) {
            // Legacy support - Sucks but we need it
            const QUrl legacyUri = resolveUrl( res.uri() );
            if( lastError() )
                return QHash<QUrl, QUrl>();
        }
        else if( state == OntologyUri ) {
            setError(QLatin1String("It is not allowed to add classes or properties through this API."), Soprano::Error::ErrorInvalidArgument);
            return QHash<QUrl, QUrl>();
        }
    }
    resGraph = resGraphList;

    ResourceIdentifier resIdent( identificationMode, this );
    QList<Sync::SyncResource> syncResources;

    //
    // Resolve URLs in property values and prepare the resource identifier
    //
    foreach( const SimpleResource& res, resGraph.toList() ) {
        // Convert to a Sync::SyncResource
        //
        Sync::SyncResource syncRes( res.uri() );
        QHashIterator<QUrl, QVariant> hit( res.properties() );
        while( hit.hasNext() ) {
            hit.next();

            Soprano::Node n = d->m_classAndPropertyTree->variantToNode( hit.value(), hit.key() );
            // The ClassAndPropertyTree returns blank nodes as URIs. It does not understand the
            // concept of blank nodes, and since only storeResources requires blank nodes,
            // it's easier to keep the blank node logic outside.
            if( n.isResource() )
                n = convertIfBlankUri( n.uri() );

            const Soprano::Error::Error error = d->m_classAndPropertyTree->lastError();
            if( error ) {
                setError( error );
                return QHash<QUrl, QUrl>();
            }
            syncRes.insert( hit.key(), n );
        }

        // Temporarily remove the nie:url, we will add it back later
        const QUrl nieUrl = syncRes.take( NIE::url() ).uri();

        QMutableHashIterator<KUrl, Soprano::Node> it( syncRes );
        while( it.hasNext() ) {
            it.next();

            const Soprano::Node object = it.value();
            if( object.isBlank() ) {
                //
                // Extra checks it is a blank node -
                // If it is a blank node make sure it was present as the subject
                QSet<QUrl>::const_iterator fit = blankResources.constFind( QUrl(object.toN3()) );
                if( fit == blankResources.constEnd() ) {
                    QString error = QString::fromLatin1("%1 does not exist in the graph. In statement (%2, %3, %4)")
                                    .arg( object.toN3(),
                                          syncRes.uri().url(),
                                          it.key().url(),
                                          it.value().toN3() );
                    setError( error, Soprano::Error::ErrorInvalidArgument );
                    return QHash<QUrl, QUrl>();
                }
                else {
                    continue;
                }
            }

            else if( object.isResource() ) {
                const UriState state = uriState(object.uri());
                if(state==NepomukUri || state == OntologyUri) {
                    continue;
                }
                else if(state == NonExistingFileUrl) {
                    setError(QString::fromLatin1("Cannot store information about non-existing local files. File '%1' does not exist.").arg(object.uri().toLocalFile()),
                                Soprano::Error::ErrorInvalidArgument);
                    return QHash<QUrl, QUrl>();
                }
                else if(state == ExistingFileUrl || state==SupportedUrl) {
                    const QUrl nieUrl = object.uri();
                    // Need to resolve it
                    QHash< QUrl, QUrl >::const_iterator findIter = resolvedNodes.constFind( nieUrl );
                    if( findIter != resolvedNodes.constEnd() ) {
                        it.setValue( convertIfBlankUri(findIter.value()) );
                    }
                    else {
                        Sync::SyncResource newRes;

                        // It doesn't exist, create it
                        QUrl resolvedUri = resolveUrl( nieUrl );
                        if( resolvedUri.isEmpty() ) {
                            resolvedUri = SimpleResource().uri(); // HACK: improveme

                            newRes.insert( NIE::url(), nieUrl );
                            if( state == ExistingFileUrl ) {
                                newRes.insert( RDF::type(), NFO::FileDataObject() );
                                if( QFileInfo( nieUrl.toLocalFile() ).isDir() )
                                    newRes.insert( RDF::type(), NFO::Folder() );
                            }

                            newRes.setUri( resolvedUri );
                            syncResources << newRes;
                        }

                        resolvedNodes.insert( nieUrl, resolvedUri );
                        it.setValue( convertIfBlankUri(resolvedUri) );
                    }
                }
                else if(state == OtherUri) {
                    // We use resolveUrl to check if the otherUri exists. If it doesn't exist,
                    // then resolveUrl which set the last error
                    // trueg: seems like a waste to not use the resolved uri here!
                    const QUrl legacyUri = resolveUrl( object.uri() );
                    if( lastError() )
                        return QHash<QUrl, QUrl>();

                    // It apparently exists, so we must support it
                }
            } // if object.isResurce
        } // while( it.hasNext() )

        // Check if the nie:url exists
        if( nieUrl.scheme() == QLatin1String("file") && !QFile::exists(nieUrl.toLocalFile()) ) {
            setError(QString::fromLatin1("Cannot store information about non-existing local files. File '%1' does not exist.").arg(nieUrl.toLocalFile()),
                        Soprano::Error::ErrorInvalidArgument);
            return QHash<QUrl, QUrl>();
        }

        // We had removed the nie:url in the begining, we must insert it again
        if( !nieUrl.isEmpty() )
            syncRes.insert( NIE::url(), nieUrl );

        // The resource is now ready.
        syncResources << syncRes;
    }

    blankResources.clear();

    if( flags & MergeDuplicateResources ) {
        mergeDuplicateSyncResources( syncResources );
    }

    // Push it into the Resource Identifier
    foreach( const Sync::SyncResource& syncRes, syncResources ) {
        QList< Soprano::Statement > stList = syncRes.toStatementList();

        if(stList.isEmpty()) {
            setError(QString::fromLatin1("storeResources: Encountered empty sync resource (%1). This is a bug. Please report.").arg(syncRes.uri().url()));
            return QHash<QUrl, QUrl>();
        }

        if( !syncRes.isValid() ) {
            setError(QLatin1String("storeResources: Contains invalid resources."), Soprano::Error::ErrorParsingFailed);
            return QHash<QUrl, QUrl>();
        }
        resIdent.addSyncResource( syncRes );
    }

    //
    // Perform the actual identification
    //
    resIdent.identifyAll();

    ResourceMerger merger( this, app, flags, discardable );
    merger.setMappings( resIdent.mappings() );

    if( !merger.merge( resIdent.resourceHash() ) ) {
        kDebug() << " MERGING FAILED! ";
        kDebug() << "Setting error!" << merger.lastError();
        setError( merger.lastError() );
        return QHash<QUrl, QUrl>();
    }

    return merger.mappings();
}

void Nepomuk2::DataManagementModel::importResources(const QUrl &url,
                                                   const QString &app,
                                                   Soprano::RdfSerialization serialization,
                                                   const QString &userSerialization,
                                                   Nepomuk2::StoreIdentificationMode identificationMode,
                                                   Nepomuk2::StoreResourcesFlags flags,
                                                   const QHash<QUrl, QVariant>& additionalMetadata)
{
    // download the file
    QString tmpFileName;
    if(!KIO::NetAccess::download(url, tmpFileName, 0)) {
        setError(QString::fromLatin1("Failed to download '%1'.").arg(url.toString()));
        return;
    }

    // guess the serialization
    if(serialization == Soprano::SerializationUnknown) {
        const QString extension = KUrl(url).fileName().section('.', -1).toLower();
        if(extension == QLatin1String("trig"))
            serialization = Soprano::SerializationTrig;
        else if(extension == QLatin1String("n3"))
            serialization = Soprano::SerializationNTriples;
        else if(extension == QLatin1String("xml"))
            serialization = Soprano::SerializationRdfXml;
    }

    const Soprano::Parser* parser = Soprano::PluginManager::instance()->discoverParserForSerialization(serialization, userSerialization);
    if(!parser) {
        setError(QString::fromLatin1("Failed to create parser for serialization '%1'").arg(Soprano::serializationMimeType(serialization, userSerialization)));
    }
    else {
        SimpleResourceGraph graph;
        Soprano::StatementIterator it = parser->parseFile(tmpFileName, QUrl(), serialization, userSerialization);
        while(it.next()) {
            graph.addStatement(*it);
        }
        if(parser->lastError()) {
            setError(parser->lastError());
        }
        else if(it.lastError()) {
            setError(it.lastError());
        }
        else {
            storeResources(graph, app, identificationMode, flags, additionalMetadata);
        }
    }

    KIO::NetAccess::removeTempFile(tmpFileName);
}

void Nepomuk2::DataManagementModel::mergeResources(const QList<QUrl>& resources, const QString& app)
{
    if(app.isEmpty()) {
        setError(QLatin1String("mergeResources: Empty application specified. This is not supported."),
                 Soprano::Error::ErrorInvalidArgument);
        return;
    }
    QSet<QUrl> resSet = resources.toSet();
    if(resSet.size() <= 1) {
        setError(QLatin1String("mergeResources: Need to provide more than 1 resource to merge"),
                 Soprano::Error::ErrorInvalidArgument);
        return;
    }

    // Virtuoso cannot handle very large number of resources cause of the complicated queries below
    // so, we do them in multiple sets of 10
    if( resources.size() > 10 ) {
        QList<QUrl> first10Resources = resources.mid( 0, 10 );
        QList<QUrl> remainingResources = resources.mid( 10 );

        mergeResources( first10Resources, app );
        if( lastError() )
            return;

        mergeResources( remainingResources, app );
        return;
    }

    foreach(const QUrl& uri, resSet) {
        if(uri.isEmpty()) {
            setError(QLatin1String("mergeResources: Encountered empty resource URI."),
                     Soprano::Error::ErrorInvalidArgument);
            return;
        }
    }

    //
    // We need to ensure that no client removes any ontology constructs or graphs,
    // we can check this before resolving file URLs since no protected resource will
    // ever have a nie:url
    //
    if(containsResourceWithProtectedType(resSet)) {
        return;
    }

    clearError();

    // TODO: Is it correct that all metadata stays the same?

    const QUrl resUri = resources.first();
    resSet.remove( resUri );
    //
    // Copy all property values of res2 that are not also defined for res1
    //
    QString query = QString::fromLatin1("select ?g ?p ?v (select count(distinct ?v2) where { %2 ?p ?v2 . }) as ?c"
                                        " where { graph ?g { ?r ?p ?v } "
                                        " FILTER(?r in (%1)) ."
                                        " FILTER NOT EXISTS { %2 ?p ?v .} }")
                    .arg( resourcesToN3(resSet).join(","),
                          Soprano::Node::resourceToN3(resUri) );

    const QList<Soprano::BindingSet> resProperties
                = executeQuery(query, Soprano::Query::QueryLanguageSparqlNoInference).allBindings();

    foreach(const Soprano::BindingSet& binding, resProperties) {
        const QUrl& prop = binding["p"].uri();
        // if the property has no cardinality of 1 or no value is defined yet we can simply convert the value to res1
        if(d->m_classAndPropertyTree->maxCardinality(prop) != 1 ||
                binding["c"].literal().toInt() == 0) {
            const Soprano::Node v = binding["v"];
            addStatement(resUri, prop, v, binding["g"]);
            d->m_watchManager->changeProperty( resUri, prop, QList<Soprano::Node>() << v,
                                               QList<Soprano::Node>() );
        }
    }


    //
    // Copy all backlinks from the other resources that are not valid for resUri
    //
    const QList<Soprano::BindingSet> res2Backlinks
            = executeQuery(QString::fromLatin1("select ?g ?p ?r where { graph ?g { ?r ?p ?r2 . } . "
                                               "FILTER(?r2 in (%1)) ."
                                               "FILTER(?r!=%2) . "
                                               "FILTER NOT EXISTS { ?r ?p %2. } }")
                           .arg(resourcesToN3(resSet).join(","),
                                Soprano::Node::resourceToN3(resUri)),
                           Soprano::Query::QueryLanguageSparqlNoInference).allBindings();
    foreach(const Soprano::BindingSet& binding, res2Backlinks) {
        addStatement(binding["r"], binding["p"], resUri, binding["g"]);
    }


    //
    // Finally delete the other resources as they have been merged
    //
    removeResources(resSet.toList(), NoRemovalFlags, app);
}



namespace {
QVariant nodeToVariant(const Soprano::Node& node) {
    if(node.isResource())
        return node.uri();
    else if(node.isBlank())
        return QUrl(QLatin1String("_:") + node.identifier());
    else
        return node.literal().variant();
}
}

Nepomuk2::SimpleResourceGraph Nepomuk2::DataManagementModel::describeResources(const QList<QUrl> &resources,
                                                                             DescribeResourcesFlags flags,
                                                                             const QList<QUrl>& targetParties)
{
    //
    // check parameters
    //
    foreach( const QUrl & res, resources ) {
        if(res.isEmpty()) {
            setError(QLatin1String("describeResources: Encountered empty resource URI."), Soprano::Error::ErrorInvalidArgument);
            return SimpleResourceGraph();
        }
    }

    if(!targetParties.isEmpty()) {
        setError(QLatin1String("Permission handling has not been implemented yet."));
        return SimpleResourceGraph();
    }


    clearError();


    //
    // Resolve all URLs - for now we ignore non-existing resources
    //
    QSet<QUrl> resolvedResources(QSet<QUrl>::fromList(resolveUrls(resources, QString(), false /* do not stat local files - it would be a waste of resources */)));
    if(resolvedResources.isEmpty()) {
        setError(QLatin1String("describeResources: No useful resource specified."), Soprano::Error::ErrorInvalidArgument);
        return SimpleResourceGraph();
    }


    //
    // In case we need to exclude discardable data we make sure that metadata properties are
    // exported in any case as those are maintained by us and are only in the specific graph
    // "by chance".
    //
    const QString discardableDataExcludeFilter
            = (flags & ExcludeDiscardableData
               ? QString::fromLatin1("FILTER(!bif:exists((select (1) where { ?g a %1 . })) || %2) . ")
                 .arg(Soprano::Node::resourceToN3(NRL::DiscardableInstanceBase()),
                      createResourceMetadataPropertyFilter(QLatin1String("?p"), false))
               : QString());

    //
    // Get all sub-resources and add them to the list of resources to describe
    //
    {
        QSet<QUrl> subResources = resolvedResources;
        int resCount = 0;
        do {
            resCount = resolvedResources.count();
            // FIXME: as soon as the issue with the empty result list is fixed change this loop back into a single query
            // (email sent to Virtuoso ml: http://sourceforge.net/mailarchive/forum.php?thread_name=4E36D5ED.9020904%40kde.org&forum_name=virtuoso-users)
            QSet<QUrl> tmp(subResources);
            subResources.clear();
            foreach(const QUrl& res, tmp) {
                Soprano::QueryResultIterator it
                        = executeQuery(QString::fromLatin1("select ?r where { "
                                                           "graph ?g { ?parent %2 ?r . "
                                                           "FILTER(?parent in (%1)) . } . "
                                                           "%3"
                                                           "}")
                                       .arg(/*resourcesToN3(subResources).join(QLatin1String(","))*/Soprano::Node::resourceToN3(res),
                                            Soprano::Node::resourceToN3(NAO::hasSubResource()),
                                            discardableDataExcludeFilter),
                                       Soprano::Query::QueryLanguageSparqlNoInference);
                subResources.clear();
                while(it.next()) {
                    subResources << it[0].uri();
                }
                resolvedResources += subResources;
            }
        } while(resCount < resolvedResources.count());
    }

    //
    // Build the basic graph consisting of the requested resources and all sub-resources
    //
    SimpleResourceGraph graph;
    QSet<QUrl> relatedResourcesToFetch;
    // FIXME: as soon as the issue with the empty result list is fixed change this loop back into a single query
    // (email sent to Virtuoso ml: http://sourceforge.net/mailarchive/forum.php?thread_name=4E36D5ED.9020904%40kde.org&forum_name=virtuoso-users)
    foreach(const QUrl& res, resolvedResources) {
        Soprano::QueryResultIterator it
                = executeQuery(QString::fromLatin1("select distinct ?s ?p ?o where { "
                                                   "?s ?p ?o . "
                                                   "FILTER(?s in (%1)) . "
                                                   "%2"
                                                   "}")
                               .arg(/*resourcesToN3(resolvedResources).join(QLatin1String(","))*/Soprano::Node::resourceToN3(res),
                                    discardableDataExcludeFilter),
                               Soprano::Query::QueryLanguageSparqlNoInference);
        while(it.next()) {
            const Soprano::Node r = it["s"];
            const Soprano::Node p = it["p"];
            const Soprano::Node o = it["o"];

            if(!o.isResource() ||
                    !(flags & ExcludeRelatedResources) ||
                    d->m_classAndPropertyTree->isDefiningProperty(p.uri()) ||
                    p == NIE::url()) {
                graph.addStatement(r, p, o);
            }

            if(o.isResource() &&
                    (!(flags & ExcludeRelatedResources) ||
                     d->m_classAndPropertyTree->isDefiningProperty(p.uri())) &&
                    !d->m_classAndPropertyTree->isChildOf(p.uri(), NAO::hasSubResource()) &&
                    p != NIE::url() &&
                    p != RDF::type()) {
                relatedResourcesToFetch << o.uri();
            }
        }
        if(it.lastError()) {
            setError(it.lastError());
            return SimpleResourceGraph();
        }
    }


    //
    // Now fetch related resources and their defining properties,
    // ignoring anything that comes from an ontology.
    //
    // For related resources we only take the defining properties since
    // that is all that is required to describe them.
    //
    QSet<QUrl> currentRelatedResources(relatedResourcesToFetch);
    while(!currentRelatedResources.isEmpty()) {
        // FIXME: as soon as the issue with the empty result list is fixed change this loop back into a single query
        // (email sent to Virtuoso ml: http://sourceforge.net/mailarchive/forum.php?thread_name=4E36D5ED.9020904%40kde.org&forum_name=virtuoso-users)
        QSet<QUrl> tmp(currentRelatedResources);
        currentRelatedResources.clear();
        foreach(const QUrl& res, tmp) {
            Soprano::QueryResultIterator it
                    = executeQuery(QString::fromLatin1("select distinct ?s ?p ?o where { "
                                                       "graph ?g { ?s ?p ?o . "
                                                       "FILTER(?s in (%1)) . "
                                                       "FILTER(!(?o in (%2))) . } . "
                                                       "FILTER(!bif:exists((select (1) where { ?g a %3 }))) . "
                                                       "%4"
                                                       "}")
                                   .arg(/*resourcesToN3(currentRelatedResources).join(QLatin1String(","))*/Soprano::Node::resourceToN3(res),
                                        resourcesToN3(graph.allResourceUris()).join(QLatin1String(",")),
                                        Soprano::Node::resourceToN3(NRL::Ontology()),
                                        discardableDataExcludeFilter),
                                   Soprano::Query::QueryLanguageSparqlNoInference);
            //currentRelatedResources.clear();
            while(it.next()) {
                const Soprano::Node r = it["s"];
                const Soprano::Node p = it["p"];
                const Soprano::Node o = it["o"];
                if(d->m_classAndPropertyTree->isDefiningProperty(p.uri())) {
                    graph.addStatement(r, p, o);
                    if(o.isResource()) {
                        currentRelatedResources << o.uri();
                    }
                }
            }
            if(it.lastError()) {
                setError(it.lastError());
                return SimpleResourceGraph();
            }
        }
    }


    //
    // Throw out all relations to related resources which were not fetched.
    // FIXME: do we really want this??
    //
    foreach(const QUrl& res, relatedResourcesToFetch + resolvedResources) {
        if(!graph.contains(res)) {
            graph.removeAll(QUrl(), QUrl(), res);
        }
    }


    //
    // Final cleanup: remove all resources that do only have metadata defined.
    //
    foreach(const QUrl& res, graph.allResourceUris()) {
        bool hasNonMetadataProps = false;
        foreach(const QUrl& prop, graph[res].properties().keys()) {
            if(!d->m_protectedProperties.contains(prop)) {
                hasNonMetadataProps = true;
                break;
            }
        }
        if(!hasNonMetadataProps) {
            graph.remove(res);
        }
    }


    return graph;
}

namespace {
    Soprano::Node anonymizeUri(const Soprano::Node& node, QHash<Soprano::Node, Soprano::Node>& blankNodes) {
        QHash<Soprano::Node, Soprano::Node>::const_iterator it = blankNodes.constFind(node);
        if(it == blankNodes.constEnd()) {
            Soprano::Node blank(QString::fromLatin1("b%1").arg(blankNodes.count()));
            blankNodes.insert(node, blank);
            return blank;
        }
        else {
            return it.value();
        }
    }
}

QString Nepomuk2::DataManagementModel::exportResources(const QList<QUrl> &resources,
                                                      Soprano::RdfSerialization serialization,
                                                      const QString &userSerialization,
                                                      Nepomuk2::DescribeResourcesFlags flags,
                                                      const QList<QUrl> &targetParties)
{
    // try to get a serializer. Without it there is no point in doing any other work
    const Soprano::Serializer* serializer = Soprano::PluginManager::instance()->discoverSerializerForSerialization(serialization, userSerialization);
    if(!serializer) {
        setError(QString::fromLatin1("Could not find serializer plugin for serialization '%1'").arg(Soprano::serializationMimeType(serialization, userSerialization)));
        return QString();
    }

    // fetch the actual data
    SimpleResourceGraph graph = describeResources(resources, flags, targetParties);
    if(lastError()) {
        return QString();
    }

    QList<Soprano::Statement> statements =  graph.toStatementGraph().toList();

    if(flags & AnonymizeNepomukUris) {
        QHash<Soprano::Node, Soprano::Node> blankNodes;
        for(QList<Soprano::Statement>::iterator it = statements.begin();
            it != statements.end(); ++it) {
            if(it->subject().uri().scheme() == QLatin1String("nepomuk")) {
                it->setSubject(anonymizeUri(it->subject(), blankNodes));
            }
            if(it->object().isResource() && it->object().uri().scheme() == QLatin1String("nepomuk")) {
                it->setObject(anonymizeUri(it->object(), blankNodes));
            }
        }
    }

    // serialilze the statements
    Soprano::Util::SimpleStatementIterator it(statements);
    QString result;
    QTextStream s(&result);
    if(serializer->serialize(it, s, serialization, userSerialization)) {
        clearError();
        return result;
    }
    else {
        setError(serializer->lastError());
        return QString();
    }
}


QUrl Nepomuk2::DataManagementModel::createGraph(const QString& app, const QMultiHash< QUrl, Soprano::Node >& additionalMetadata)
{
    QHash<QUrl, Soprano::Node> graphMetaData = additionalMetadata;

    // determine the graph type
    bool haveGraphType = false;
    bool hasNaoCreated = false;

    QHash< QUrl, Soprano::Node >::const_iterator it = additionalMetadata.constFind( RDF::type() );
    if( it != additionalMetadata.constEnd() ) {
        // check if it is a valid type
        if(!it.value().isResource()) {
            setError(QString::fromLatin1("rdf:type has resource range. '%1' does not have a resource type.").arg(it.value().toN3()), Soprano::Error::ErrorInvalidArgument);
            return QUrl();
        }
        else {
            if(d->m_classAndPropertyTree->isChildOf(it.value().uri(), NRL::Graph()))
                haveGraphType = true;
        }
    }

    it = additionalMetadata.constFind( NAO::created() );
    if( it != additionalMetadata.constEnd() ) {
        if(!it.value().literal().isDateTime()) {
            setError(QString::fromLatin1("nao:created has xsd:dateTime range. '%1' is not convertable to a dateTime.").arg(it.value().toN3()), Soprano::Error::ErrorInvalidArgument);
            return QUrl();
        }
        hasNaoCreated = true;
    }

    // FIXME: check property, domain, and range
    // Reuse code from ResourceMerger::checkGraphMetadata

    // add missing metadata
    if(!haveGraphType) {
        graphMetaData.insert(RDF::type(), NRL::InstanceBase());
    }
    if(!hasNaoCreated) {
        graphMetaData.insert(NAO::created(), Soprano::LiteralValue(QDateTime::currentDateTime()));
    }
    if(!graphMetaData.contains(NAO::maintainedBy()) && !app.isEmpty()) {
        graphMetaData.insert(NAO::maintainedBy(), findApplicationResource(app));
    }

    const QUrl graph = createUri(GraphUri);
    const QUrl metadatagraph = createUri(GraphUri);

    // add metadata graph itself
    const QString metaN3 = Soprano::Node::resourceToN3( metadatagraph );
    const QString graphN3 = Soprano::Node::resourceToN3( graph );

    QString query = QString::fromLatin1("sparql insert into %1 { %1 nrl:coreGraphMetadataFor %2 ;"
                                        " rdf:type nrl:GraphMetadata . %2 ")
                    .arg( metaN3, graphN3 );


    for(QHash<QUrl, Soprano::Node>::const_iterator it = graphMetaData.constBegin();
        it != graphMetaData.constEnd(); ++it) {

        query += QString::fromLatin1(" %1 %2 ;")
                 .arg( Soprano::Node::resourceToN3(it.key()), it.value().toN3() );
    }
    query[ query.length() - 1 ] = QChar::fromLatin1('.');
    query.append( QLatin1Char('}') );

    executeQuery( query, Soprano::Query::QueryLanguageUser, QLatin1String("sql") );

    return graph;
}

QUrl DataManagementModel::fetchGraph(const QString& app, bool discardable)
{
    QPair<QString, bool> cacheItem(app, discardable);
    QMutexLocker lock( &d->m_graphCacheMutex );
    QUrl* uri = d->m_graphCache.object( cacheItem );
    if( uri ) {
        return *uri;
    }

    QLatin1String type("nrl:InstanceBase");
    if( discardable )
        type = QLatin1String("nrl:DiscardableInstanceBase");

    const QString query = QString::fromLatin1("select ?g where { ?g a %1 ; nao:maintainedBy ?agent ."
                                              " ?agent nao:identifier %2 . } LIMIT 1")
                          .arg( type, Soprano::Node::literalToN3(app) );

    Soprano::QueryResultIterator it = executeQuery(query, Soprano::Query::QueryLanguageSparqlNoInference);
    if( it.next() ) {
        QUrl uri = it[0].uri();
        d->m_graphCache.insert( cacheItem, new QUrl(uri) );

        return uri;
    }
    else {
        QMultiHash<QUrl, Soprano::Node> hash;
        if( discardable )
            hash.insert( RDF::type(), NRL::DiscardableInstanceBase() );

        QUrl uri = createGraph(app, hash);
        d->m_graphCache.insert( cacheItem, new QUrl(uri) );

        return uri;
    }
}


QUrl Nepomuk2::DataManagementModel::findApplicationResource(const QString &app, bool create)
{
    QMutexLocker lock( &d->m_appCacheMutex );
    QUrl* uri = d->m_appCache.object( app );
    if( uri ) {
        return *uri;
    }

    Soprano::QueryResultIterator it =
            executeQuery(QString::fromLatin1("select ?r where { ?r a nao:Agent . ?r nao:identifier %1 . } LIMIT 1")
                         .arg( Soprano::Node::literalToN3(app) ),
                         Soprano::Query::QueryLanguageSparql);
    if(it.next()) {
        const QUrl newUri = it[0].uri();
        d->m_appCache.insert( app, new QUrl(newUri) );

        return newUri;
    }
    else if(create) {
        const QUrl graph = createGraph(QString(), QMultiHash<QUrl, Soprano::Node>());
        const QUrl uri = createUri(ResourceUri);

        // the app itself
        addStatement( uri, RDF::type(), NAO::Agent(), graph );
        addStatement( uri, NAO::identifier(), Soprano::LiteralValue(app), graph );

        KService::List services = KServiceTypeTrader::self()->query(QLatin1String("Application"),
                                                                    QString::fromLatin1("DesktopEntryName=='%1'").arg(app));
        if(services.count() == 1) {
            addStatement(uri, NAO::prefLabel(), Soprano::LiteralValue(services.first()->name()), graph);
        }

        d->m_appCache.insert( app, new QUrl(uri) );
        return uri;
    }
    else {
        return QUrl();
    }
}

QUrl Nepomuk2::DataManagementModel::createUri(Nepomuk2::DataManagementModel::UriType type)
{
    QString typeToken;
    if(type == GraphUri)
        typeToken = QLatin1String("ctx");
    else
        typeToken = QLatin1String("res");

    while( 1 ) {
        QString uuid = QUuid::createUuid().toString();
        uuid = uuid.mid(1, uuid.length()-2);

        QString uriString = QString::fromLatin1("nepomuk:/%1/%2").arg( typeToken, uuid );
        const QUrl uri( uriString );

        // The iri_to_id command returns an id of the form "#num" if the uri exists in the database
        // and NULL if it doesn't. This method is a LOT faster than checking if the uri exists
        // as a subject / predicate / object or graph.
        QString query = QString::fromLatin1("select iri_to_id( '%1', 0 )") .arg( uriString );
        Soprano::QueryResultIterator it = executeQuery( query, Soprano::Query::QueryLanguageUser, QLatin1String("sql") );
        //Don't loop forever if there was an error executing the query.
        if( lastError() ){
            return QUrl();
        }
        if( it.next() ) {
            if( it[0].literal().toString().isEmpty() )
                return uri;
        }
    }
}


QHash< QUrl, QList< Soprano::Node > > DataManagementModel::addProperty(const QList< QUrl >& resources, const QUrl& property, const QList<Soprano::Node>& nodes, const QString& app, bool signalPropertyChanged)
{
    kDebug() << resources << property << nodes << app;
    Q_ASSERT(!resources.isEmpty());
    Q_ASSERT(!nodes.isEmpty());
    Q_ASSERT(!property.isEmpty());

    //
    // Check cardinality conditions
    //
    const int maxCardinality = d->m_classAndPropertyTree->maxCardinality(property);
    if( maxCardinality == 1 ) {
        if( nodes.size() != 1 ) {
            setError(QString::fromLatin1("%1 has cardinality of 1. Cannot set more then one value.").arg(property.toString()), Soprano::Error::ErrorInvalidArgument);
            return QHash<QUrl, QList<Soprano::Node> >();
        }
    }


    clearError();

    //
    // Temporary duplication
    // FIXME: Remove all of this!
    //
    QList<QUrl> knownResources = resources;
    QList<Soprano::Node> resolvedNodes = nodes;
    QUrl graph = fetchGraph(app);
    if( property == NIE::url() )
        graph = d->m_nepomukGraph;

    const QUrl appRes = findApplicationResource(app);
    QSet<QPair<QUrl, Soprano::Node> > finalProperties;
    foreach(const QUrl& resUri, resources)
        foreach(const Soprano::Node& node, nodes)
            finalProperties << qMakePair(resUri, node);

    // add all the data
    // TODO: Add in one go, instead of multiple addStatement calls
    QHash<QUrl, QList<Soprano::Node> > finalValuesPerResource;
    foreach(const QUrl& resUri, resources) {
        foreach(const Soprano::Node& node, nodes) {
            addStatement(resUri, property, node, graph);
            finalValuesPerResource[resUri].append(node);
        }
    }

    // inform interested parties
    if(signalPropertyChanged) {
        for(QHash<QUrl, QList<Soprano::Node> >::const_iterator it = finalValuesPerResource.constBegin(); it != finalValuesPerResource.constEnd(); ++it) {
            d->m_watchManager->changeProperty(it.key(), property, it.value(), QList<Soprano::Node>());
        }
        if(!finalValuesPerResource.isEmpty()) {
            d->m_watchManager->changeSomething();
        }
    }

    // update modification date
    // TODO: This can be done when pushing the data above!
    QSet<QUrl> finalResources = finalValuesPerResource.keys().toSet();
    updateModificationDate( finalResources );

    return finalValuesPerResource;
}

bool Nepomuk2::DataManagementModel::doesResourceExist(const QUrl &res, const QUrl& graph) const
{
    if(graph.isEmpty()) {
        return executeQuery(QString::fromLatin1("ask where { %1 ?p ?v . FILTER(%2) . }")
                            .arg(Soprano::Node::resourceToN3(res),
                                 createResourceMetadataPropertyFilter(QLatin1String("?p"))),
                            Soprano::Query::QueryLanguageSparql).boolValue();
    }
    else {
        return executeQuery(QString::fromLatin1("ask where { graph %1 { %2 ?p ?v . FILTER(%3) . } . }")
                            .arg(Soprano::Node::resourceToN3(graph),
                                 Soprano::Node::resourceToN3(res),
                                 createResourceMetadataPropertyFilter(QLatin1String("?p"))),
                            Soprano::Query::QueryLanguageSparql).boolValue();
    }
}

QList<QUrl> DataManagementModel::resolveUrls(const QList<QUrl>& urls, const QString& app, bool statLocalFiles)
{
    QList<QUrl> nieUrls;
    QList<QUrl> finalUrls;
    finalUrls.reserve( urls.size() );

    Q_FOREACH(const QUrl& url, urls) {
        const QUrl resolved = resolveUrl(url, statLocalFiles);
        if( lastError() )
            return QList<QUrl>();

        if( resolved.isEmpty() )
            nieUrls << url;
        else
            finalUrls << resolved;
    }

    // Create the file urls
    if( !app.isEmpty() ) {
        const QUrl graph = fetchGraph( app );
        finalUrls.append( createFileResources(nieUrls, graph) );
    }

    return finalUrls;
}

QUrl Nepomuk2::DataManagementModel::resolveUrl(const QUrl &url, bool statLocalFiles)
{
    const UriState state = uriState( url, statLocalFiles );

    if( state == NepomukUri || state == OntologyUri ) {
        // nothing to resolve over here
        return url;
    }

    //
    // First check if the URL does exists as resource URI
    //
    else if( executeQuery(QString::fromLatin1("ask where { %1 ?p ?o . }")
                     .arg(Soprano::Node::resourceToN3(url)),
                     Soprano::Query::QueryLanguageSparql).boolValue() ) {
        return url;
    }

    //
    // we resolve all URLs except nepomuk:/ URIs. While the DMS does only use nie:url
    // on local files libnepomuk used to use it for everything but nepomuk:/ URIs.
    // Thus, we need to handle that legacy data by checking if url does exist as nie:url
    //
    else {
        Soprano::QueryResultIterator it
                = executeQuery(QString::fromLatin1("select ?r where { ?r %1 %2 . } limit 1")
                               .arg(Soprano::Node::resourceToN3(NIE::url()),
                                    Soprano::Node::resourceToN3(url)),
                               Soprano::Query::QueryLanguageSparql);

        // if the URL is used as a nie:url return the corresponding resource URI
        if(it.next()) {
            return it[0].uri();
        }

        // non-existing unsupported URL
        else if( state == OtherUri ) {
            QString error = QString::fromLatin1("Unknown protocol '%1' encountered in '%2'.")
                            .arg(url.scheme(), url.toString());
            setError(error, Soprano::Error::ErrorInvalidArgument);
            return QUrl();
        }

        // if there is no existing URI return an empty match for local files (since we do always want to use nie:url here)
        else {
            // we only throw an error if the file:/ URL points to a non-existing file AND it does not exist in the database.
            if(state == NonExistingFileUrl) {
                setError(QString::fromLatin1("Cannot store information about non-existing local files. File '%1' does not exist.").arg(url.toLocalFile()),
                         Soprano::Error::ErrorInvalidArgument);
            }

            return QUrl();
        }
    }

    // fallback
    return url;
}


QList<Soprano::Node> DataManagementModel::resolveNodes(const QSet< Soprano::Node >& nodes, const QString& app)
{
    QList<QUrl> nieUrls;
    QList<Soprano::Node> resolvedNodes;
    resolvedNodes.reserve( nodes.size() );

    Q_FOREACH(const Soprano::Node& node, nodes) {
        if(node.isResource()) {
            const QUrl resolved = resolveUrl(node.uri(), true);
            if(resolved.isEmpty() && lastError()) {
                return QList<Soprano::Node>();
            }

            if(resolved.isEmpty())
                nieUrls << node.uri();
            else
                resolvedNodes << resolved;
        }
        else {
            resolvedNodes << node;
        }
    }

    const QUrl graph = fetchGraph( app );
    QList<QUrl> fileResources = createFileResources( nieUrls, graph );
    foreach(const QUrl& resUri, fileResources)
        resolvedNodes << resUri;

    return resolvedNodes;
}

// TODO: emit resource watcher resource creation signals
QList<QUrl> DataManagementModel::createFileResources(const QList< QUrl >& nieUrls, const QUrl& graph)
{
    if(nieUrls.isEmpty())
        return QList<QUrl>();

    QList<QUrl> resUriList;

    QString query = QString::fromLatin1("sparql insert into %1 { ").arg( Soprano::Node::resourceToN3(graph) );
    QString query2 = QString::fromLatin1("sparql insert into %1 { ").arg( Soprano::Node::resourceToN3(d->m_nepomukGraph) );

    foreach( const QUrl& nieUrl, nieUrls ) {
        const QUrl resUri = createUri( ResourceUri );
        const QString resN3 = Soprano::Node::resourceToN3(resUri);
        query += resN3;
        query2 += resN3;

        resUriList << resUri;

        QFileInfo fileInfo( nieUrl.toLocalFile() );
        if( fileInfo.isDir() )
            query += QLatin1String(" a nfo:FileDataObject, nfo:Folder . ");
        else
            query += QLatin1String(" a nfo:FileDataObject . ");

        // Url, lastModified and created will be in the nepomuk graph
        query2 += QString::fromLatin1(" nie:url %1 ; ").arg( Soprano::Node::resourceToN3(nieUrl) );

        Soprano::LiteralValue dtNode( QDateTime::currentDateTime() );
        query2 += QString::fromLatin1("nao:lastModified %1 ; nao:created %1 . ")
                      .arg( Soprano::Node::literalToN3( dtNode ) );
    }
    query += QLatin1Char('}');
    query2 += QLatin1Char('}');

    executeQuery( query, Soprano::Query::QueryLanguageUser, QLatin1String("sql") );
    executeQuery( query2, Soprano::Query::QueryLanguageUser, QLatin1String("sql") );
    return resUriList;
}


bool Nepomuk2::DataManagementModel::updateNieUrlOnLocalFile(const QUrl &resource, const QUrl &nieUrl)
{
    kDebug() << resource << "->" << nieUrl;

    //
    // Since KDE 4.4 we use nepomuk:/res/<UUID> Uris for all resources including files. Thus, moving a file
    // means updating two things:
    // 1. the nie:url property
    // 2. the nie:isPartOf relation (only necessary if the file and not the whole folder was moved)
    //

    //
    // Now update the nie:url, nfo:fileName, and nie:isPartOf relations.
    //
    // We do NOT use setProperty to avoid the overhead and data clutter of creating
    // new metadata graphs for the changed data.
    //

    // remember for url, filename, and parent the graph they are defined in
    QUrl resUri, oldNieUrl, oldNieUrlGraph, oldParentResource, oldParentResourceGraph, oldFileNameGraph;
    QString oldFileName;

    // we do not use isLocalFileUrl() here since we also handle already moved files
    if(resource.scheme() == QLatin1String("file")) {
        oldNieUrl = resource;
        Soprano::QueryResultIterator it
                = executeQuery(QString::fromLatin1("select distinct ?gu ?gf ?gp ?r ?f ?p where { "
                                                   "graph ?gu { ?r %2 %1 . } . "
                                                   "OPTIONAL { graph ?gf { ?r %3 ?f . } . } . "
                                                   "OPTIONAL { graph ?gp { ?r %4 ?p . } . } . "
                                                   "} LIMIT 1")
                               .arg(Soprano::Node::resourceToN3(resource),
                                    Soprano::Node::resourceToN3(NIE::url()),
                                    Soprano::Node::resourceToN3(NFO::fileName()),
                                    Soprano::Node::resourceToN3(NIE::isPartOf())),
                               Soprano::Query::QueryLanguageSparql);
        if(it.next()) {
            resUri= it["r"].uri();
            oldNieUrlGraph = it["gu"].uri();
            oldParentResource = it["p"].uri();
            oldParentResourceGraph = it["gp"].uri();
            oldFileName = it["f"].toString();
            oldFileNameGraph = it["gf"].uri();
        }
    }
    else {
        resUri = resource;
        Soprano::QueryResultIterator it
                = executeQuery(QString::fromLatin1("select distinct ?gu ?gf ?gp ?u ?f ?p where { "
                                                   "graph ?gu { %1 %2 ?u . } . "
                                                   "OPTIONAL { graph ?gf { %1 %3 ?f . } . } . "
                                                   "OPTIONAL { graph ?gp { %1 %4 ?p . } . } . "
                                                   "} LIMIT 1")
                               .arg(Soprano::Node::resourceToN3(resource),
                                    Soprano::Node::resourceToN3(NIE::url()),
                                    Soprano::Node::resourceToN3(NFO::fileName()),
                                    Soprano::Node::resourceToN3(NIE::isPartOf())),
                               Soprano::Query::QueryLanguageSparql);
        if(it.next()) {
            oldNieUrl = it["u"].uri();
            oldNieUrlGraph = it["gu"].uri();
            oldParentResource = it["p"].uri();
            oldParentResourceGraph = it["gp"].uri();
            oldFileName = it["f"].toString();
            oldFileNameGraph = it["gf"].uri();
        }
    }

    if (!oldNieUrlGraph.isEmpty()) {
        const QString oldBasePath = KUrl(oldNieUrl).path(KUrl::AddTrailingSlash);
        const QString newBasePath = KUrl(nieUrl).path(KUrl::AddTrailingSlash);

        if( oldBasePath == newBasePath )
            return true;

        removeStatement(resUri, NIE::url(), oldNieUrl, oldNieUrlGraph);
        addStatement(resUri, NIE::url(), nieUrl, oldNieUrlGraph);

        if (!oldFileNameGraph.isEmpty()) {
            // we only update the filename if it actually changed
            if(KUrl(oldNieUrl).fileName() != KUrl(nieUrl).fileName()) {
                removeStatement(resUri, NFO::fileName(), Soprano::LiteralValue(oldFileName), oldFileNameGraph);
                addStatement(resUri, NFO::fileName(), Soprano::LiteralValue(KUrl(nieUrl).fileName()), oldFileNameGraph);
            }
        }

        if (!oldParentResourceGraph.isEmpty()) {
            // we only update the parent folder if it actually changed
            const KUrl nieUrlParent = KUrl(nieUrl).directory(KUrl::IgnoreTrailingSlash);
            const KUrl oldUrlParent = KUrl(oldNieUrl).directory(KUrl::IgnoreTrailingSlash);
            if(nieUrlParent != oldUrlParent) {
                removeStatement(resUri, NIE::isPartOf(), oldParentResource, oldParentResourceGraph);
                const QUrl newParentRes = resolveUrl(nieUrlParent);
                if (!newParentRes.isEmpty()) {
                    addStatement(resUri, NIE::isPartOf(), newParentRes, oldParentResourceGraph);
                }
            }
        }

        //
        // Update all children
        // We only need to update the nie:url properties. Filenames and nie:isPartOf relations cannot change
        // due to a renaming of the parent folder.
        //
        // CAUTION: The trailing slash on the from URL is essential! Otherwise we might match the newly added
        //          URLs, too (in case a rename only added chars to the name)
        //
        const QString query = QString::fromLatin1("select distinct ?r ?u ?g where { "
                                                  "graph ?g { ?r %1 ?u . } . "
                                                  "FILTER(REGEX(STR(?u),'^%2')) . "
                                                  "}")
                .arg(Soprano::Node::resourceToN3(NIE::url()),
                     KUrl(oldNieUrl).url(KUrl::AddTrailingSlash));

        //
        // We cannot use one big loop since our updateMetadata calls below can change the iterator
        // which could have bad effects like row skipping. Thus, we handle the urls in chunks of
        // cached items.
        //
        forever {
            const QList<Soprano::BindingSet> urls = executeQuery(query + QLatin1String( " LIMIT 500" ),
                                                                 Soprano::Query::QueryLanguageSparql)
                    .allBindings();
            if (urls.isEmpty())
                break;

            for (int i = 0; i < urls.count(); ++i) {
                const KUrl u = urls[i]["u"].uri();
                const QUrl r = urls[i]["r"].uri();
                const QUrl g = urls[i]["g"].uri();

                // now construct the new URL
                const QString oldRelativePath = u.path().mid(oldBasePath.length());
                const KUrl newUrl(newBasePath + oldRelativePath);

                removeStatement(r, NIE::url(), u, g);
                addStatement(r, NIE::url(), newUrl, g);
            }
        }

        return true;
    }
    else {
        // no old nie:url found
        return false;
    }
}

//void Nepomuk2::DataManagementModel::insertStatements(const QSet<QUrl> &resources, const QUrl &property, const QSet<Soprano::Node> &values, const QUrl &graph)
//{
//    const QString propertyString = Soprano::Node::resourceToN3(property);

//    QString query = QString::fromLatin1("insert into %1 { ")
//            .arg(Soprano::Node::resourceToN3(graph));

//    foreach(const QUrl& res, resources) {
//        foreach(const Soprano::Node& value, values) {
//            query.append(QString::fromLatin1("%1 %2 %3 . ")
//                        .arg(Soprano::Node::resourceToN3(res),
//                             propertyString,
//                             value.toN3()));
//        }
//    }
//    query.append(QLatin1String("}"));

//    executeQuery(query, Soprano::Query::QueryLanguageSparql);
//}

Nepomuk2::ClassAndPropertyTree* Nepomuk2::DataManagementModel::classAndPropertyTree()
{
    return d->m_classAndPropertyTree;
}

bool Nepomuk2::DataManagementModel::containsResourceWithProtectedType(const QSet<QUrl> &resources) const
{
    if(resources.isEmpty())
        return false;

    if(executeQuery(QString::fromLatin1("ask where { ?r a ?t . FILTER(?r in (%1)) . FILTER(?t in (%2,%3,%4)) . }")
            .arg(resourcesToN3(resources).join(QLatin1String(",")),
                 Soprano::Node::resourceToN3(RDFS::Class()),
                 Soprano::Node::resourceToN3(RDF::Property()),
                 Soprano::Node::resourceToN3(NRL::Graph())),
                    Soprano::Query::QueryLanguageSparql).boolValue()) {
        setError(QLatin1String("It is not allowed to remove classes, properties, or graphs through this API."), Soprano::Error::ErrorInvalidArgument);
        return true;
    }
    else {
        return false;
    }
}

bool Nepomuk2::DataManagementModel::isProtectedProperty(const QUrl &prop) const
{
    return d->m_protectedProperties.contains(prop);
}

Nepomuk2::ResourceWatcherManager* Nepomuk2::DataManagementModel::resourceWatcherManager() const
{
    return d->m_watchManager;
}

TypeCache* DataManagementModel::typeCache()
{
    return d->m_typeCache;
}

QUrl DataManagementModel::nepomukGraph()
{
    return d->m_nepomukGraph;
}


void Nepomuk2::DataManagementModel::removeAllResources(const QSet< QUrl >& resourceUris, RemovalFlags flags, const QString& app)
{
    Q_UNUSED(flags);

    QSet<QUrl> resolvedResources(resourceUris);

    //
    // Handle the sub-resources:
    // this has to be done before deleting the resouces in resolvedResources. Otherwise the nao:hasSubResource relationships are already gone!
    //
    // Explanation of the query:
    // The query selects all subresources of the resources in resolvedResources.
    // It then filters out the sub-resources that are related from other resources that are not the ones being deleted.
    //
    QSet<QUrl> subResources = resolvedResources;
    int resCount = 0;
    do {
        resCount = resolvedResources.count();
        Soprano::QueryResultIterator it
                = executeQuery(QString::fromLatin1("select ?r where { ?r ?p ?o . "
                                                   "?parent %1 ?r . "
                                                   "FILTER(?parent in (%2)) . "
                                                   "FILTER(!bif:exists((select (1) where { ?r2 ?p3 ?r . FILTER(%3) . FILTER(!bif:exists((select (1) where { ?x %1 ?r2 . FILTER(?x in (%2)) . }))) . }))) . "
                                                   "}")
                               .arg(Soprano::Node::resourceToN3(NAO::hasSubResource()),
                                    resourcesToN3(subResources).join(QLatin1String(",")),
                                    createResourceFilter(resolvedResources, QLatin1String("?r2"))),
                               Soprano::Query::QueryLanguageSparql);
        subResources.clear();
        while(it.next()) {
            subResources << it[0].uri();
        }
        resolvedResources += subResources;
    } while(resCount < resolvedResources.count());


    // get the graphs we need to check with removeTrailingGraphs later on
    QSet<QUrl> actuallyRemovedResources;
    Soprano::QueryResultIterator it
            = executeQuery(QString::fromLatin1("select distinct ?r where { ?r ?p ?o . FILTER(?r in (%1)) . }")
                           .arg(resourcesToN3(resolvedResources).join(QLatin1String(","))),
                           Soprano::Query::QueryLanguageSparqlNoInference);
    while(it.next()) {
        actuallyRemovedResources << it[0].uri();
    }

    // get the resources that we modify by removing relations to one of the deleted ones
    QSet<QUrl> modifiedResources;
    QList<Soprano::Statement> removedStatements;
    foreach(const QUrl& resUri, resolvedResources) {
        it = executeQuery(QString::fromLatin1("select distinct ?o ?p where { ?o ?p %1 . }")
                        .arg(Soprano::Node::resourceToN3(resUri)),
                        Soprano::Query::QueryLanguageSparqlNoInference);
        while(it.next()) {
            modifiedResources << it[0].uri();
            removedStatements << Soprano::Statement(it[0].uri(), it[1].uri(), resUri);
        }
    }
    modifiedResources -= actuallyRemovedResources;

    // remove the resources and inform interested parties
    foreach(const Soprano::Node& res, actuallyRemovedResources) {
        // The WatcherManaager fill automatically fetch the types
        d->m_watchManager->removeResource(res.uri(), QList<QUrl>());

        removeAllStatements(res, Soprano::Node(), Soprano::Node());
        removeAllStatements(Soprano::Node(), Soprano::Node(), res);
    }
    updateModificationDate(modifiedResources);

    foreach(const Soprano::Statement& st, removedStatements) {
        d->m_watchManager->changeProperty( st.subject().uri(), st.predicate().uri(),
                                           QList<Soprano::Node>(),
                                           QList<Soprano::Node>() << st.object() );
    }

    if(!actuallyRemovedResources.isEmpty()) {
        d->m_watchManager->changeSomething();
    }
}

#include "datamanagementmodel.moc"
