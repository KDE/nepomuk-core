/*
    This file is part of the Nepomuk KDE project.
    Copyright (C) 2011  Vishesh Handa <handa.vish@gmail.com>

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


#include "resourcemerger.h"
#include "datamanagementmodel.h"
#include "classandpropertytree.h"
#include "nepomuktools.h"

#include <Soprano/Vocabulary/NRL>
#include <Soprano/Vocabulary/RDF>
#include <Soprano/Vocabulary/RDFS>
#include <Soprano/Vocabulary/NAO>
#include <Soprano/Vocabulary/XMLSchema>

#include <Soprano/StatementIterator>
#include <Soprano/QueryResultIterator>
#include <Soprano/FilterModel>
#include <Soprano/NodeIterator>
#include <Soprano/LiteralValue>
#include <Soprano/Node>

#include "nie.h"

#include <KDebug>
#include <Soprano/Graph>
#include "resourcewatchermanager.h"
#include "backupsync/lib/syncresource.h"

using namespace Soprano::Vocabulary;
using namespace Nepomuk2::Vocabulary;


Nepomuk2::ResourceMerger::ResourceMerger(Nepomuk2::DataManagementModel* model, const QString& app,
                                        const QHash< QUrl, QVariant >& additionalMetadata,
                                        const StoreResourcesFlags& flags )
{
    m_app = app;
    m_additionalMetadata = additionalMetadata;
    m_model = model;
    m_flags = flags;
    m_rvm = model->resourceWatcherManager();

    //setModel( m_model );
}


Nepomuk2::ResourceMerger::~ResourceMerger()
{
}

void Nepomuk2::ResourceMerger::setMappings(const QHash< QUrl, QUrl >& mappings)
{
    m_mappings = mappings;
}

QHash< QUrl, QUrl > Nepomuk2::ResourceMerger::mappings() const
{
    return m_mappings;
}

void Nepomuk2::ResourceMerger::setAdditionalGraphMetadata(const QHash<QUrl, QVariant>& additionalMetadata)
{
    m_additionalMetadata = additionalMetadata;
}

QHash< QUrl, QVariant > Nepomuk2::ResourceMerger::additionalMetadata() const
{
    return m_additionalMetadata;
}

Soprano::Statement Nepomuk2::ResourceMerger::resolveStatement(const Soprano::Statement& st)
{
    if( !st.isValid() ) {
        QString error = QString::fromLatin1("Invalid statement encountered");
        setError( error, Soprano::Error::ErrorInvalidStatement );
        return Soprano::Statement();
    }

    Soprano::Node resolvedSubject = resolveMappedNode( st.subject() );
    if( lastError() )
        return Soprano::Statement();

    Soprano::Statement newSt( st );
    newSt.setSubject( resolvedSubject );

    Soprano::Node object = st.object();
    if( ( object.isResource() && object.uri().scheme() == QLatin1String("nepomuk") ) || object.isBlank() ) {
        Soprano::Node resolvedObject = resolveMappedNode( object );
        if( lastError() )
            return Soprano::Statement();
        newSt.setObject( resolvedObject );
    }

    return newSt;
}


bool Nepomuk2::ResourceMerger::push(const Soprano::Statement& st)
{
    ClassAndPropertyTree *tree = ClassAndPropertyTree::self();
    if( tree->maxCardinality(  st.predicate().uri() ) == 1 ) {
        const bool lazy = ( m_flags & LazyCardinalities );
        const bool overwrite = (m_flags & OverwriteProperties) &&
        tree->maxCardinality( st.predicate().uri() ) == 1;

        if( lazy || overwrite ) {
            Soprano::StatementIterator it = m_model->listStatements( st.subject(), st.predicate(), Soprano::Node() );
            while(it.next()) {
                m_removedStatements << *it;
                m_trailingGraphCandidates.insert(it.current().context().uri());
            }
            m_model->removeAllStatements( st.subject(), st.predicate(), Soprano::Node() );
        }
    }

    Soprano::Statement statement( st );
    if( statement.context().isEmpty() )
        statement.setContext( m_graph );


    return m_model->addStatement( statement );
}


QUrl Nepomuk2::ResourceMerger::createGraph()
{
    return m_model->createGraph( m_app, m_additionalMetadata );
}

QMultiHash< QUrl, Soprano::Node > Nepomuk2::ResourceMerger::getPropertyHashForGraph(const QUrl& graph) const
{
    // trueg: this is more a hack than anything else: exclude the inference types
    // a real solution would either ignore supertypes of nrl:Graph in checkGraphMetadata()
    // or only check the new metadata for consistency
    Soprano::QueryResultIterator it
            = m_model->executeQuery(QString::fromLatin1("select ?p ?o where { graph ?g { %1 ?p ?o . } . FILTER(?g!=<urn:crappyinference2:inferredtriples>) . }")
                                    .arg(Soprano::Node::resourceToN3(graph)),
                                    Soprano::Query::QueryLanguageSparql);
    //Convert to prop hash
    QMultiHash<QUrl, Soprano::Node> propHash;
    while(it.next()) {
        propHash.insert( it["p"].uri(), it["o"] );
    }
    return propHash;
}


bool Nepomuk2::ResourceMerger::areEqual(const QMultiHash<QUrl, Soprano::Node>& oldPropHash,
                                       const QMultiHash<QUrl, Soprano::Node>& newPropHash)
{
    //
    // When checking if two graphs are equal, certain stuff needs to be considered
    //
    // 1. The nao:created might not be the same
    // 2. One graph may contain more rdf:types than the other, but still be the same
    // 3. The newPropHash does not contain the nao:maintainedBy statement

    QSet<QUrl> oldTypes;
    QSet<QUrl> newTypes;

    QHash< QUrl, Soprano::Node >::const_iterator it = oldPropHash.constBegin();
    for( ; it != oldPropHash.constEnd(); it++ ) {
        const QUrl & propUri = it.key();
        if( propUri == NAO::maintainedBy() || propUri == NAO::created() )
            continue;

        if( propUri == RDF::type() ) {
            oldTypes << it.value().uri();
            continue;
        }

        //kDebug() << " --> " << it.key() << " " << it.value();
        if( !newPropHash.contains( it.key(), it.value() ) ) {
            //kDebug() << "False value : " << newPropHash.value( it.key() );
            return false;
        }
    }

    it = newPropHash.constBegin();
    for( ; it != newPropHash.constEnd(); it++ ) {
        const QUrl & propUri = it.key();
        if( propUri == NAO::maintainedBy() || propUri == NAO::created() )
            continue;

        if( propUri == RDF::type() ) {
            newTypes << it.value().uri();
            continue;
        }

        //kDebug() << " --> " << it.key() << " " << it.value();
        if( !oldPropHash.contains( it.key(), it.value() ) ) {
            //kDebug() << "False value : " << oldPropHash.value( it.key() );
            return false;
        }
    }

    //
    // Check the types
    //
    newTypes << NRL::InstanceBase();
    if( !sameTypes(oldTypes, newTypes) ) {
        return false;
    }

    // Check nao:maintainedBy
    it = oldPropHash.find( NAO::maintainedBy() );
    if( it == oldPropHash.constEnd() )
        return false;

    if( it.value().uri() != m_model->findApplicationResource(m_app, false) )
        return false;

    return true;
}


bool Nepomuk2::ResourceMerger::sameTypes(const QSet< QUrl >& t1, const QSet< QUrl >& t2)
{
    QSet<QUrl> types1;
    QSet<QUrl> types2;

    ClassAndPropertyTree* tree = m_model->classAndPropertyTree();
    foreach(const QUrl& type, t1) {
        types1 << type;
        types1.unite(tree->allParents(type));
    }

    foreach(const QUrl& type, t2) {
        types2 << type;
        types2.unite(tree->allParents(type));
    }

    return types1 == types2;
}


// Graph Merge rules
// 1. If old graph is of type discardable and new is non-discardable
//    -> Then update the graph
// 2. Otherwsie
//    -> Keep the old graph

QUrl Nepomuk2::ResourceMerger::mergeGraphs(const QUrl& oldGraph)
{
    //
    // Check if mergeGraphs has already been called for oldGraph
    //
    QHash< QUrl, QUrl >::const_iterator fit = m_graphHash.constFind( oldGraph );
    if( fit != m_graphHash.constEnd() ) {
        //kDebug() << "Already merged once, just returning";
        return fit.value();
    }

    QMultiHash<QUrl, Soprano::Node> oldPropHash = getPropertyHashForGraph( oldGraph );
    QMultiHash<QUrl, Soprano::Node> newPropHash = m_additionalMetadataHash;

    // Compare the old and new property hash
    // If both have the same properties then there is no point in creating a new graph.
    // vHanda: This check is very expensive. Is it worth it?
    if( areEqual( oldPropHash, newPropHash ) ) {
        //kDebug() << "SAME!!";
        // They are the same - Don't do anything
        m_graphHash.insert( oldGraph, QUrl() );
        return QUrl();
    }

    QMultiHash<QUrl, Soprano::Node> finalPropHash;
    //
    // Graph type nrl:DiscardableInstanceBase is a special case.
    // Only If both the old and new graph contain nrl:DiscardableInstanceBase then
    // will the new graph also be discardable.
    //
    if( oldPropHash.contains( RDF::type(), NRL::DiscardableInstanceBase() ) &&
        newPropHash.contains( RDF::type(), NRL::DiscardableInstanceBase() ) )
        finalPropHash.insert( RDF::type(), NRL::DiscardableInstanceBase() );

    oldPropHash.remove( RDF::type(), NRL::DiscardableInstanceBase() );
    newPropHash.remove( RDF::type(), NRL::DiscardableInstanceBase() );

    finalPropHash.unite( oldPropHash );
    finalPropHash.unite( newPropHash );

    // Add app uri
    if( m_appUri.isEmpty() )
        m_appUri = m_model->findApplicationResource( m_app );
    if( !finalPropHash.contains( NAO::maintainedBy(), m_appUri ) )
        finalPropHash.insert( NAO::maintainedBy(), m_appUri );

    //kDebug() << "Creating : " << finalPropHash;
    QUrl graph = m_model->createGraph( m_app, finalPropHash );

    m_graphHash.insert( oldGraph, graph );
    return graph;
}

QMultiHash< QUrl, Soprano::Node > Nepomuk2::ResourceMerger::toNodeHash(const QHash< QUrl, QVariant >& hash)
{
    QMultiHash<QUrl, Soprano::Node> propHash;
    ClassAndPropertyTree *tree = ClassAndPropertyTree::self();

    QHash< QUrl, QVariant >::const_iterator it = hash.constBegin();
    QHash< QUrl, QVariant >::const_iterator constEnd = hash.constEnd();
    for( ; it != constEnd; ++it ) {
        Soprano::Node n = tree->variantToNode( it.value(), it.key() );
        if( tree->lastError() ) {
            setError( tree->lastError().message() ,tree->lastError().code() );
            return QMultiHash< QUrl, Soprano::Node >();
        }

        propHash.insert( it.key(), n );
    }

    return propHash;
}

bool Nepomuk2::ResourceMerger::checkGraphMetadata(const QMultiHash< QUrl, Soprano::Node >& hash)
{
    ClassAndPropertyTree* tree = m_model->classAndPropertyTree();

    QList<QUrl> types;
    types << NRL::Graph();

    QHash<QUrl, int> propCardinality;

    QHash< QUrl, Soprano::Node >::const_iterator it = hash.constBegin();
    for( ; it != hash.constEnd(); it++ ) {
        const QUrl& propUri = it.key();
        if( propUri == RDF::type() ) {
            Soprano::Node object = it.value();
            if( !object.isResource() ) {
                setError(QString::fromLatin1("rdf:type has resource range. '%1' does not have a resource type.").arg(object.toN3()), Soprano::Error::ErrorInvalidArgument);
                return false;
            }

            // All the types should be a sub-type of nrl:Graph
            // FIXME: there could be multiple types in the old graph from inferencing. all superclasses of nrl:Graph. However, it would still be valid.
            if( !tree->isChildOf( object.uri(), NRL::Graph() ) ) {
                setError( QString::fromLatin1("Any rdf:type specified in the additional metadata should be a subclass of nrl:Graph. '%1' is not.").arg(object.uri().toString()),
                                   Soprano::Error::ErrorInvalidArgument );
                return false;
            }
            types << object.uri();
        }

        // Save the cardinality of each property
        QHash< QUrl, int >::iterator propIter = propCardinality.find( propUri );
        if( propIter == propCardinality.end() ) {
            propCardinality.insert( propUri, 1 );
        }
        else {
            propIter.value()++;
        }
    }

    it = hash.constBegin();
    for( ; it != hash.constEnd(); it++ ) {
        const QUrl & propUri = it.key();
        // Check the cardinality
        int maxCardinality = tree->maxCardinality( propUri );
        int curCardinality = propCardinality.value( propUri );

        if( maxCardinality != 0 ) {
            if( curCardinality > maxCardinality ) {
                setError( QString::fromLatin1("%1 has a max cardinality of %2").arg(propUri.toString()).arg(maxCardinality), Soprano::Error::ErrorInvalidArgument );
                return false;
            }
        }

        //
        // Check the domain and range
        const QUrl domain = tree->propertyDomain( propUri );
        const QUrl range = tree->propertyRange( propUri );

        // domain
        if( !domain.isEmpty() && !tree->isChildOf( types, domain ) ) {
            setError( QString::fromLatin1("%1 has a rdfs:domain of %2").arg( propUri.toString(), domain.toString() ), Soprano::Error::ErrorInvalidArgument);
            return false;
        }

        // range
        if( !range.isEmpty() ) {
            const Soprano::Node& object = it.value();
            if( object.isResource() ) {
                if( !isOfType( object.uri(), range ) ) {
                    setError( QString::fromLatin1("%1 has a rdfs:range of %2").arg( propUri.toString(), range.toString() ), Soprano::Error::ErrorInvalidArgument);
                    return false;
                }
            }
            else if( object.isLiteral() ) {
                const Soprano::LiteralValue lv = object.literal();
                if( lv.dataTypeUri() != range ) {
                    setError( QString::fromLatin1("%1 has a rdfs:range of %2").arg( propUri.toString(), range.toString() ), Soprano::Error::ErrorInvalidArgument);
                    return false;
                }
            }
        } // range
    }

    //kDebug() << hash;
    return true;
}

QUrl Nepomuk2::ResourceMerger::createResourceUri()
{
    return m_model->createUri( DataManagementModel::ResourceUri );
}

QUrl Nepomuk2::ResourceMerger::createGraphUri()
{
    return m_model->createUri( DataManagementModel::GraphUri );
}

QList< QUrl > Nepomuk2::ResourceMerger::existingTypes(const QUrl& uri) const
{
    QList<QUrl> types;

    QString query = QString::fromLatin1("select ?t where { %1 rdf:type ?t . }")
                    .arg( Soprano::Node::resourceToN3( uri ) );
    Soprano::QueryResultIterator it = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
    while( it.next() )
        types << it[0].uri();

    // all resources have rdfs:Resource type by default
    types << RDFS::Resource();

    return types;
}

bool Nepomuk2::ResourceMerger::isOfType(const Soprano::Node & node, const QUrl& type, const QList<QUrl> & newTypes) const
{
    //kDebug() << "Checking " << node << " for type " << type;
    ClassAndPropertyTree * tree = m_model->classAndPropertyTree();

    QList<QUrl> types( newTypes );
    if( !node.isBlank() ) {
        types << existingTypes( node.uri() );
    }
    types += newTypes;

    if( types.isEmpty() ) {
        kDebug() << node << " does not have a type!!";
        return false;
    }

    foreach( const QUrl & uri, types ) {
        if( uri == type || tree->isChildOf( uri, type ) ) {
            return true;
        }
    }

    return false;
}

Soprano::Node Nepomuk2::ResourceMerger::resolveMappedNode(const Soprano::Node& node)
{
    // Find in mappings
    const QUrl uri = node.isBlank() ? node.toN3() : node.uri();
    QHash< QUrl, QUrl >::const_iterator it = m_mappings.constFind( uri );
    if( it != m_mappings.constEnd() ) {
        return it.value();
    }

    // Do not resolve the blank nodes which need to be created
    if( node.isBlank() )
        return node;

    if( uri.scheme() == QLatin1String("nepomuk") &&
        !m_model->containsAnyStatement( uri, Soprano::Node(), Soprano::Node() ) ) {
        QString error = QString::fromLatin1("Could not resolve %1. "
                                            "You cannot create nepomuk uris using this method")
                        .arg( Soprano::Node::resourceToN3( uri ) );
        setError( error, Soprano::Error::ErrorInvalidArgument );
        return Soprano::Node();
    }

    return node;
}

Soprano::Node Nepomuk2::ResourceMerger::resolveUnmappedNode(const Soprano::Node& node)
{
    if( !node.isBlank() )
        return node;

    const QUrl nodeN3( node.toN3() );
    QHash< QUrl, QUrl >::const_iterator it = m_mappings.constFind( nodeN3 );
    if( it != m_mappings.constEnd() ) {
        return it.value();
    }

    const QUrl newUri = createResourceUri();
    m_mappings.insert( nodeN3, newUri );

    // FIXME: trueg: IMHO these statements should instead be added to the list of all statements so there is only one place where anything is actually added to the model
    Soprano::LiteralValue dateTime( QDateTime::currentDateTime() );

    // OPTIMIZATION: Use a hand made query instead of the generic addStatement.
    // They way we avoid the extra resourceToN3 for the properties, uri and graph
    QString addQuery = QString::fromLatin1("sparql insert into %1 { %2 nao:created %3 ; nao:lastModified %3 . }")
                        .arg( Soprano::Node::resourceToN3( m_graph ),
                              Soprano::Node::resourceToN3( newUri ),
                              Soprano::Node::literalToN3( dateTime ) );
    // We use sql instead of sparql so that we can avoid any changes done by any of the other models
    m_model->executeQuery( addQuery, Soprano::Query::QueryLanguageUser, QLatin1String("sql") );

    return newUri;
}


void Nepomuk2::ResourceMerger::resolveBlankNodes(Nepomuk2::Sync::SyncResource& res)
{
    res.setUri( resolveUnmappedNode( res.uriNode() ) );

    QMutableHashIterator<KUrl, Soprano::Node> it( res );
    while( it.hasNext() ) {
        it.next();
        it.setValue( resolveUnmappedNode(it.value()) );
    }
}


void Nepomuk2::ResourceMerger::removeDuplicates(Nepomuk2::Sync::SyncResource& res)
{
    QMutableHashIterator<KUrl, Soprano::Node> it( res );

    while( it.hasNext() ) {
        it.next();

        if( res.isBlank() || it.value().isBlank() )
            continue;

        // FIXME: Maybe half of this query could be constructed in advance
        const QString query = QString::fromLatin1("select ?g where { graph ?g { %1 %2 %3 . } . } LIMIT 1")
                              .arg( Soprano::Node::resourceToN3( res.uri() ),
                                    Soprano::Node::resourceToN3( it.key() ),
                                    it.value().toN3() );

        Soprano::QueryResultIterator qit = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparql);
        if(qit.next()) {
            const QUrl oldGraph = qit[0].uri();
            qit.close();

            if(!m_model->isProtectedProperty(it.key())) {
                Soprano::Statement st( res.uri(), it.key(), it.value() );
                m_duplicateStatements.insert( oldGraph, st );
            }
            it.remove();
        }
    }
}

void Nepomuk2::ResourceMerger::removeDuplicatesInList(QSet<Soprano::Statement> *stList)
{
    QMutableSetIterator<Soprano::Statement> it( *stList );
    while( it.hasNext() ) {
        const Soprano::Statement &st = it.next();
        if( st.subject().isBlank() || st.object().isBlank() )
            continue;

        const QString query = QString::fromLatin1("select ?g where { graph ?g { %1 %2 %3 . } . } LIMIT 1")
        .arg(st.subject().toN3(),
             st.predicate().toN3(),
             st.object().toN3());

        Soprano::QueryResultIterator qit = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparql);
        if(qit.next()) {
            const QUrl oldGraph = qit[0].uri();
            qit.close();

            if(!m_model->isProtectedProperty(st.predicate().uri())) {
                m_duplicateStatements.insert( oldGraph, st );
            }
            it.remove();
        }
    }
}

namespace {
    QUrl getBlankOrResourceUri( const Soprano::Node & n ) {
        if( n.isResource() ) {
            return n.uri();
        }
        else if( n.isBlank() ) {
            return QString( QLatin1String("_:") + n.identifier() );
        }
        return QUrl();
    }

    QUrl xsdDuration() {
        return QUrl( Soprano::Vocabulary::XMLSchema::xsdNamespace().toString() + QLatin1String("duration") );
    }

    template<typename T> QStringList nodesToN3( const T &nodes ) {
        QStringList list;
        foreach( const Soprano::Node& node, nodes ) {
            list << node.toN3();
        }
        return list;
    }

    QList<QUrl> nodeListToUriList( const QList<Soprano::Node>& nodeList ) {
        QList<QUrl> urls;
        urls.reserve( nodeList.size() );
        foreach( const Soprano::Node& node, nodeList )
            urls << node.uri();
        return urls;
    }
}

/*
 Rough algorithm -

 - - Validity checks --
 1. Check the graph's additional metadata for validity
 2. Resolve all the identified uris - Do not create new nodes
 3. Check statement validity -
    a. Max cardinality checks
    - Take OverwriteProperties and LazyCardinalities into account
    b. Domain/Range checks

 -- Graph Handling --
 4. Get all the statements which already exist in the model, but in a different graph.
 5. Create new graphs which are the result of the merging the present graph and the new one
    a.) In case the old and new graph are the same then forget about those statements
 6. Create the main graph
 7. Iterate through all the remaining statements and make a list of the resources that
    we will be modifying

 -- Actual Statement pushing --
 8. Create new resources for all the unidentified blank uris
    - Notify the RWM
 9. Push all the statements
    a.) Push <nao:lastModified, currentDateTime()> for all the resources that will be modified
    b.) Push all the type statements
        - Inform the RWM about these new types
    c.) Push other statements
        - Inform the RWM
        - Remove existing statements if OverwriteProperties or LazyCardinalities
    d.) Update nao:lastModified for all modified resources
    e.) Push extra metadata statements, so that the specified nao:lastModified, nao:created
        are taken into considerations
 10. You're done!

 */
bool Nepomuk2::ResourceMerger::merge(const Nepomuk2::Sync::ResourceHash& resHash_)
{
    //
    // 1. Check if the additional metadata is valid
    //
    if( !additionalMetadata().isEmpty() ) {
        QMultiHash<QUrl, Soprano::Node> additionalMetadata = toNodeHash(m_additionalMetadata);
        if( lastError() )
            return false;

        if( !checkGraphMetadata( additionalMetadata ) ) {
            return false;
        }
    }

    //
    // 2. Resolve all the mapped statements
    //
    QHash<KUrl, Sync::SyncResource> resHash;
    QHashIterator<KUrl, Sync::SyncResource> it_( resHash_ );
    while( it_.hasNext() ) {
        Sync::SyncResource res = it_.next().value();
        res.setUri( resolveMappedNode(res.uri()) );
        if( lastError() )
            return false;

        QMutableHashIterator<KUrl, Soprano::Node> iter( res );
        while( iter.hasNext() ) {
            Soprano::Node& object = iter.next().value();
            if( ( object.isResource() && object.uri().scheme() == QLatin1String("nepomuk") ) || object.isBlank() )
                object = resolveMappedNode( object );

            if( lastError() )
                return false;
        }

        resHash.insert( res.uri(), res );
    }

    //
    // 3 Move the metadata from resHash to the metadataHash
    //
    Sync::ResourceHash resMetadataHash;

    QMutableHashIterator<KUrl, Sync::SyncResource> it( resHash );
    while( it.hasNext() ) {
        Sync::SyncResource& res = it.next().value();

        if( !res.isBlank() ) {
            Sync::SyncResource metadataRes( res.uri() );

            // Remove the metadata properties - nao:lastModified and nao:created
            QHash< KUrl, Soprano::Node >::iterator fit = res.find( NAO::lastModified() );
            if( fit != res.end() ) {
                metadataRes.insert( NAO::lastModified(), fit.value() );
                res.erase( fit );
            }

            fit = res.find( NAO::created() );
            if( fit != res.end() ) {
                metadataRes.insert( NAO::created(), fit.value() );
                res.erase( fit );
            }

            if( !metadataRes.isEmpty() )
                resMetadataHash.insert( metadataRes.uri(), metadataRes );
        }

        if( !hasValidData( resHash, res ) )
            return false;
    }

    // Graph Handling

    //
    // 4. Remove duplicate statemets
    //
    it.toFront();
    while( it.hasNext() ) {
        Sync::SyncResource& res = it.next().value();

        removeDuplicates( res );
        if( res.isEmpty() )
            it.remove();
    }

    //
    // 5. Create all the graphs
    //
    QMutableHashIterator<QUrl, Soprano::Statement> hit( m_duplicateStatements );
    while( hit.hasNext() ) {
        hit.next();
        const QUrl& oldGraph = hit.key();
        const QUrl newGraph = mergeGraphs( oldGraph );

        // The newGraph is invalid when the oldGraph and the newGraph are the same
        // In that case those statements can just be ignored.
        if( !newGraph.isValid() ) {
            hit.remove();
        }
    }

    // Create the main graph, if they are any statements to merge
    if( !resHash.isEmpty() ) {
        m_graph = createGraph();
    }


    //
    // Apply the Metadata properties back onto the resHash
    // Also modify the model to remove previous values
    //
    QSet<QUrl> trailingGraphCandidates = m_graphHash.keys().toSet();
    Soprano::Node currentDateTime = Soprano::LiteralValue( QDateTime::currentDateTime() );

    it.toFront();
    while( it.hasNext() ) {
        Sync::SyncResource& res = it.next().value();
        //TODO: What about the case when a blank node is created with a specific datetime?
        if( res.isBlank() )
            continue;

        const Sync::SyncResource metadataRes = resMetadataHash.value( it.key() );
        const QUrl resUri = res.uri();

        // Remove previous nao:lastModified
        QString query = QString::fromLatin1("select distinct ?g where { graph ?g { "
                                            " %1 nao:lastModified ?o . } }")
                        .arg( Soprano::Node::resourceToN3( resUri ) );

        Soprano::QueryResultIterator qit = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
        while(qit.next()) {
            const Soprano::Node g = qit[0];
            m_model->removeAllStatements( resUri, NAO::lastModified(), Soprano::Node(), g );
            trailingGraphCandidates << g.uri();
        }

        // Add nao:lastModified with currentDateTime (unless provided)
        QHash< KUrl, Soprano::Node >::const_iterator fit = metadataRes.constFind( NAO::lastModified() );
        if( fit == metadataRes.constEnd() )
            res.insert( NAO::lastModified(), currentDateTime );
        else
            res.insert( NAO::lastModified(), fit.value() );

        // Check for nao:created
        fit = metadataRes.constFind( NAO::created() );
        if( fit != metadataRes.constEnd() ) {
            query = QString::fromLatin1("ask where { %1 nao:created ?o . }")
                    .arg( Soprano::Node::resourceToN3( resUri ) );

            // in this case the value of nao:created is not changed
            bool naoCreatedInRepo = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparql ).boolValue();
            if( !naoCreatedInRepo )
                res.insert( NAO::created(), fit.value() );
        }
    }
    resMetadataHash.clear();

    //
    // Node Resolution + ResourceWatcher stuff
    //
    //FIXME: Inform RWM about typeAdded()
    QMultiHash< QUrl, QList<QUrl> > typeHash; // For Blank Nodes

    it.toFront();
    while( it.hasNext() ) {
        Sync::SyncResource& res = it.next().value();
        if( res.isBlank() ) {
            QList<QUrl> types = nodeListToUriList( res.values(RDF::type()) );
            typeHash.insert( res.uri(), types );
        }

        // 6. Create all the blank nodes
        resolveBlankNodes( res );
    }

    //
    // Actual statement pushing
    //
    QHashIterator<KUrl, Sync::SyncResource> iter( resHash );
    QList<Soprano::Statement> allStatements;
    while( iter.hasNext() ) {
        iter.next();

        // FIXME: Find a more efficient way of pushing and avoid this 'allStatements'
        allStatements << iter.value().toStatementList();
        foreach( const Soprano::Statement& st, iter.value().toStatementList() ) {
            push( st );
            m_rvm->addStatement( st );
        }
    }

    // make sure we do not leave trailing empty graphs
    m_model->removeTrailingGraphs(m_trailingGraphCandidates);

    // Inform the ResourceWatcherManager of these new types
    QHashIterator< QUrl, QList<QUrl> > typeIt( typeHash );
    while( typeIt.hasNext() ) {
        const QUrl blankUri = typeIt.next().key();

        // Get its resource uri
        const QUrl resUri = m_mappings.value( blankUri );
        m_rvm->createResource( resUri, typeIt.value() );
    }

    // Inform the ResourceWatcherManager of the changed properties
    QHash<QUrl, QHash<QUrl, QList<Soprano::Node> > > addedProperties;
    QHash<QUrl, QHash<QUrl, QList<Soprano::Node> > > removedProperties;
    foreach(const Soprano::Statement& s, m_removedStatements) {
        removedProperties[s.subject().uri()][s.predicate().uri()].append(s.object());
    }
    foreach(const Soprano::Statement& s, allStatements) {
        addedProperties[s.subject().uri()][s.predicate().uri()].append(s.object());
    }
    foreach(const QUrl& res, addedProperties.keys().toSet() + removedProperties.keys().toSet()) {
        foreach(const QUrl& prop, addedProperties[res].keys().toSet() + removedProperties[res].keys().toSet()) {
            m_rvm->changeProperty(res, prop, addedProperties[res][prop], removedProperties[res][prop]);
        }
    }

    // Push all the duplicateStatements
    QHashIterator<QUrl, Soprano::Statement> hashIter( m_duplicateStatements );
    while( hashIter.hasNext() ) {
        hashIter.next();
        Soprano::Statement st = hashIter.value();

        m_model->removeAllStatements( st.subject(), st.predicate(), st.object(), hashIter.key() );
        const QUrl newGraph( m_graphHash[hashIter.key()] );
        st.setContext( newGraph );

        // No need to inform the RVM, we're just changing the graph.
        m_model->addStatement( st );
    }


    m_model->removeTrailingGraphs(trailingGraphCandidates);

    return true;
}

bool Nepomuk2::ResourceMerger::hasValidData(const QHash<KUrl, Nepomuk2::Sync::SyncResource>& resHash,
                                            Nepomuk2::Sync::SyncResource& res)
{
    // Remove the types for now, will add again later
    // There is no point in checking the domain and range of rdf:type
    QList<Soprano::Node> resNodeTypes = res.values( RDF::type() );
    QList<QUrl> resTypes = nodeListToUriList( resNodeTypes );
    res.remove( RDF::type() );

    // FIXME: Optimize iteration over a multi hash
    QList<KUrl> properties = res.uniqueKeys();
    foreach( const KUrl& propUri, properties ) {
        QList<Soprano::Node> objectValues = res.values( propUri );

        //
        // 3.a Check the max cardinality
        //
        int maxCardinality = ClassAndPropertyTree::self()->maxCardinality( propUri );
        if( maxCardinality > 0 ) {

            QStringList filterStringList;
            QStringList objectN3 = nodesToN3( objectValues );
            foreach( const QString &n3, objectN3 )
                filterStringList << QString::fromLatin1("?v!=%1").arg( n3 );

            const QString query = QString::fromLatin1("select count(distinct ?v) where {"
                                                      " %1 %2 ?v ."
                                                      "FILTER( %3 ) . }")
                                  .arg( Soprano::Node::resourceToN3( res.uri() ),
                                        Soprano::Node::resourceToN3( propUri ),
                                        filterStringList.join( QLatin1String(" && ") ) );

            int existingCardinality = 0;
            Soprano::QueryResultIterator exCarIt
            = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
            if( exCarIt.next() ) {
                existingCardinality = exCarIt[0].literal().toInt();
            }

            const int newCardinality = objectValues.size() + existingCardinality;

            // TODO: This can be made faster by not calculating all these values when flags are set
            if( newCardinality > maxCardinality ) {
                // Special handling for max Cardinality == 1
                if( maxCardinality == 1 ) {
                    // If the difference is 1, then that is okay, as the OverwriteProperties flag
                    // has been set
                    if( (m_flags & OverwriteProperties) && (newCardinality-maxCardinality) == 1 ) {
                        continue;
                    }
                }

                // The LazyCardinalities flag has been set, we don't care about cardinalities any more
                if( (m_flags & LazyCardinalities) ) {
                    continue;
                }

                //
                // Display a very informative error message
                //
                QString query = QString::fromLatin1("select distinct ?v where {" " %1 %2 ?v ."
                                                    "FILTER( %3 ) . }")
                                .arg( Soprano::Node::resourceToN3( res.uri() ),
                                      Soprano::Node::resourceToN3( propUri ),
                                      filterStringList.join( QLatin1String(" && ") ) );
                QList< Soprano::Node > existingValues = m_model->executeQuery( query,
                               Soprano::Query::QueryLanguageSparql ).iterateBindings(0).allNodes();

                QString error = QString::fromLatin1("%1 has a max cardinality of %2. Provided "
                                                    "%3 values - %4. Existing - %5")
                                .arg( propUri.url(),
                                      QString::number(maxCardinality),
                                      QString::number(objectN3.size()),
                                      objectN3.join(QLatin1String(", ")),
                                      nodesToN3(existingValues).join(QLatin1String(", ")) );
                setError( error, Soprano::Error::ErrorInvalidStatement );
                return false;
            }
        } // if ( maxCardinality > 0 )

        //
        // 3.b Check the domain and range
        //
        ClassAndPropertyTree* tree = ClassAndPropertyTree::self();
        QUrl domain = tree->propertyDomain( propUri );
        QUrl range = tree->propertyRange( propUri );

        //        kDebug() << "Domain : " << domain;
        //        kDebug() << "Range : " << range;

        // domain
        if( !domain.isEmpty() && !isOfType( res.uri(), domain, resTypes ) ) {
            // Error
            QList<QUrl> allTypes = ( resTypes + existingTypes(res.uri()) );

            QString error = QString::fromLatin1("%1 has a rdfs:domain of %2. "
            "%3 only has the following types %4" )
            .arg( Soprano::Node::resourceToN3( propUri ),
                    Soprano::Node::resourceToN3( domain ),
                    Soprano::Node::resourceToN3( res.uri() ),
                    Nepomuk2::resourcesToN3( allTypes ).join(", ") );
            setError( error, Soprano::Error::ErrorInvalidArgument);
            return false;
        }

        // range
        if( range.isEmpty() )
            continue;

        foreach( const Soprano::Node& object, objectValues ) {
            if( object.isResource() || object.isBlank() ) {
                const QUrl objUri = getBlankOrResourceUri( object );
                QList<QUrl> objectNewTypes =  nodeListToUriList( resHash[ objUri ].values( RDF::type() ) );

                if( !isOfType( objUri, range, objectNewTypes ) ) {
                    // Error
                    QList<QUrl> allTypes = ( objectNewTypes + existingTypes(objUri) );

                    QString error = QString::fromLatin1("%1 has a rdfs:range of %2. "
                                                        "%3 only has the following types %4" )
                                    .arg( Soprano::Node::resourceToN3( propUri ),
                                          Soprano::Node::resourceToN3( range ),
                                          Soprano::Node::resourceToN3( objUri ),
                                          resourcesToN3( allTypes ).join(", ") );
                    setError( error, Soprano::Error::ErrorInvalidArgument );
                    return false;
                }
            }
            else if( object.isLiteral() ) {
                const Soprano::LiteralValue lv = object.literal();
                // Special handling for xsd:duration
                if( lv.isUnsignedInt() && range == xsdDuration() ) {
                    continue;
                }
                if( (!lv.isPlain() && lv.dataTypeUri() != range) || (lv.isPlain() && range != RDFS::Literal()) ) {
                    // Error
                    QString error = QString::fromLatin1("%1 has a rdfs:range of %2. Provided %3")
                                    .arg( Soprano::Node::resourceToN3( propUri ),
                                          Soprano::Node::resourceToN3( range ),
                                          Soprano::Node::literalToN3(lv) );
                    setError( error, Soprano::Error::ErrorInvalidArgument);
                    return false;
                }
            }
        } // range

    } // SyncResource contents iteration

    // Insert the removed types
    foreach( const Soprano::Node& node, resNodeTypes )
        res.insert( RDF::type(), node );

    return true;
}
