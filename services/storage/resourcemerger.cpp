/*
    This file is part of the Nepomuk KDE project.
    Copyright (C) 2011-12  Vishesh Handa <handa.vish@gmail.com>

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
#include "typecache.h"

using namespace Soprano::Vocabulary;
using namespace Nepomuk2::Vocabulary;


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
    // Resource Metadata
    metadataProperties.reserve( 4 );
    metadataProperties.insert( NAO::lastModified() );
    metadataProperties.insert( NAO::userVisible() );
    metadataProperties.insert( NAO::created() );
    metadataProperties.insert( NAO::creator() );
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

bool Nepomuk2::ResourceMerger::push(const QUrl& graph, const Nepomuk2::Sync::ResourceHash& resHash)
{
    if( resHash.isEmpty() || graph.isEmpty() )
        return true; /* Silently fail, nothing bad has happened */

    ClassAndPropertyTree *tree = ClassAndPropertyTree::self();

    const bool lazy = (m_flags & LazyCardinalities);
    const bool overwrite = (m_flags & OverwriteProperties);
    const bool overwriteAll = (m_flags & OverwriteAllProperties);

    const QString preQuery = QString::fromLatin1("sparql insert into %1 { ")
                             .arg( Soprano::Node::resourceToN3( graph ) );

    QString query;
    QHashIterator<KUrl, Sync::SyncResource> it( resHash );
    while( it.hasNext() ) {
        const Sync::SyncResource& res = it.next().value();

        const QString resN3 = Soprano::Node::resourceToN3( res.uri() );
        query += resN3;

        QList<KUrl> properties = res.uniqueKeys();
        foreach( const QUrl& prop, properties ) {
            QList<Soprano::Node> values = res.values( prop );
            const QString propN3 = Soprano::Node::resourceToN3( prop );

            if( lazy || overwrite || overwriteAll ) {
                if( tree->maxCardinality( prop ) == 1 || overwriteAll ) {
                    QString query = QString::fromLatin1("select ?o ?g where { graph ?g { %1 %2 ?o . } }")
                                    .arg( resN3, propN3 );

                    Soprano::QueryResultIterator it = m_model->executeQuery(query, Soprano::Query::QueryLanguageSparqlNoInference);
                    while (it.next()) {
                        m_resRemoveHash[ res.uri() ].insert( prop, it[0] );
                        m_trailingGraphCandidates.insert( it[1].uri() );
                    }
                    m_model->removeAllStatements( res.uri(), prop, Soprano::Node() );

                    // In LazyCardinalities we don't care about the cardinality
                    if( lazy )
                        values = QList<Soprano::Node>() << values.first();
                }
            }

            QStringList n3;
            foreach( const Soprano::Node& node, values )
                n3 << node.toN3();
            query += QString::fromLatin1(" %1 %2 ;").arg( propN3, n3.join(QString(", ")) );
        }

        query[ query.length() - 1 ] = '.';

        // Virtuoso does not like commands that are too long. So we use an arbitary limit of 500 characters.
        if( query.size() >= 500 ) {
            QString command = QString::fromLatin1("%1 %2 }").arg( preQuery, query );
            m_model->executeQuery( command, Soprano::Query::QueryLanguageUser, QLatin1String("sql") );
            if( m_model->lastError() ) {
                setError( m_model->lastError() );
                return false;
            }
            query.clear();
        }
    }

    if( !query.isEmpty() ) {
        QString command = QString::fromLatin1("%1 %2 }").arg( preQuery, query );

        // We use sql instead of sparql so that we can avoid any changes done by any of the other models
        m_model->executeQuery( command, Soprano::Query::QueryLanguageUser, QLatin1String("sql") );
        if( m_model->lastError() ) {
            setError( m_model->lastError() );
            return false;
        }
    }

    return true;
}


QMultiHash< QUrl, Soprano::Node > Nepomuk2::ResourceMerger::getPropertyHashForGraph(const QUrl& graph) const
{
    Soprano::QueryResultIterator it
            = m_model->executeQuery(QString::fromLatin1("select ?p ?o where { %1 ?p ?o . }")
                                    .arg(Soprano::Node::resourceToN3(graph)),
                                    Soprano::Query::QueryLanguageSparqlNoInference);
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

    QMultiHash<QUrl, Soprano::Node> oldHash( oldPropHash );
    oldHash.remove( NAO::created() );

    oldTypes = nodeListToUriList(oldHash.values( RDF::type() )).toSet();
    oldHash.remove( RDF::type() );

    // Maintainership
    // No nao:maintainedBy => legacy data, not the same
    QHash< QUrl, Soprano::Node >::ConstIterator it = oldHash.constFind( NAO::maintainedBy() );
    if( it == oldHash.constEnd() )
        return false;
    else if( it.value().uri() != m_model->findApplicationResource(m_app, false) )
        return false;

    oldHash.remove( NAO::maintainedBy() );

    QMultiHash<QUrl, Soprano::Node> newHash( newPropHash );
    newHash.remove( NAO::created() );
    newHash.remove( NAO::maintainedBy() );

    newTypes = nodeListToUriList(newHash.values( RDF::type() )).toSet();
    newHash.remove( RDF::type() );

    if( oldHash != newHash )
        return false;

    //
    // Check the types
    //
    newTypes << NRL::InstanceBase();
    if( !sameTypes(oldTypes, newTypes) ) {
        return false;
    }

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

    QHash< QUrl, Soprano::Node >::const_iterator fit = hash.constFind( RDF::type() );
    if( fit != hash.constEnd() ) {
        Soprano::Node object = fit.value();
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

    QList<QUrl> properties = hash.uniqueKeys();
    properties.removeAll( RDF::type() );

    foreach( const QUrl& propUri, properties ) {
        QList<Soprano::Node> objects = hash.values( propUri );

        int curCardinality = objects.size();
        int maxCardinality = tree->maxCardinality( propUri );

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
            foreach(const Soprano::Node& object, objects ) {
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
            }
        } // range
    }

    return true;
}


bool Nepomuk2::ResourceMerger::isOfType(const Soprano::Node & node, const QUrl& type, const QList<QUrl> & newTypes) const
{
    //kDebug() << "Checking " << node << " for type " << type;
    ClassAndPropertyTree * tree = m_model->classAndPropertyTree();

    QList<QUrl> types( newTypes );
    if( !node.isBlank() ) {
        types << m_model->typeCache()->types( node.uri() );
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

Soprano::Node Nepomuk2::ResourceMerger::resolveBlankNode(const Soprano::Node& node)
{
    if( !node.isBlank() )
        return node;

    const QUrl nodeN3( node.toN3() );
    QHash< QUrl, QUrl >::const_iterator it = m_mappings.constFind( nodeN3 );
    if( it != m_mappings.constEnd() ) {
        return it.value();
    }

    const QUrl newUri = m_model->createUri( DataManagementModel::ResourceUri );
    m_mappings.insert( nodeN3, newUri );
    m_newUris.insert( newUri );

    return newUri;
}


Nepomuk2::Sync::ResourceHash Nepomuk2::ResourceMerger::resolveBlankNodes(const Nepomuk2::Sync::ResourceHash& resHash_)
{
    Sync::ResourceHash resHash;

    QHashIterator<KUrl, Sync::SyncResource> it( resHash_ );
    while( it.hasNext() ) {
        Sync::SyncResource res = it.next().value();
        const bool wasBlank = res.isBlank();

        res.setUri( resolveBlankNode( res.uriNode() ) );

        QMutableHashIterator<KUrl, Soprano::Node> it( res );
        while( it.hasNext() ) {
            it.next();
            it.setValue( resolveBlankNode(it.value()) );
        }

        //
        // Add the metadata properties for new nodes
        //
        if( wasBlank ) {
            Soprano::LiteralValue dateTime( QDateTime::currentDateTime() );

            // Check if already exists?
            if( !res.contains( NAO::lastModified() ) )
                res.insert( NAO::lastModified(), dateTime );

            if( !res.contains( NAO::created() ) )
                res.insert( NAO::created(), dateTime );
        }

        resHash.insert( res.uri(), res );
    }

    return resHash;
}


void Nepomuk2::ResourceMerger::removeDuplicates(Nepomuk2::Sync::SyncResource& res)
{
    QString baseQuery = QString::fromLatin1("select ?g where { graph ?g { %1 ")
                        .arg( Soprano::Node::resourceToN3( res.uri() ) );

    QMutableHashIterator<KUrl, Soprano::Node> it( res );
    while( it.hasNext() ) {
        const Soprano::Node& object = it.next().value();

        if( res.isBlank() || object.isBlank() )
            continue;

        const QString query = QString::fromLatin1("%1 %2 %3 . } . } LIMIT 1")
                              .arg( baseQuery, Soprano::Node::resourceToN3( it.key() ),
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


bool Nepomuk2::ResourceMerger::merge(const Nepomuk2::Sync::ResourceHash& resHash_)
{
    //
    // 1. Check if the additional metadata is valid
    //
    if( !additionalMetadata().isEmpty() ) {
        m_additionalMetadataHash = toNodeHash(m_additionalMetadata);
        if( lastError() )
            return false;

        if( !checkGraphMetadata( m_additionalMetadataHash ) ) {
            return false;
        }
    }

    //
    // 2. Resolve all the mapped statements
    //
    Sync::ResourceHash resHash;
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
        if( !newGraph.isValid() )
            hit.remove();
        else
            m_trailingGraphCandidates << oldGraph;
    }

    // Create the main graph, if they are any statements to merge
    if( !resHash.isEmpty() ) {
        m_graph = m_model->createGraph( m_app, m_additionalMetadata );
        if( m_graph.isEmpty() )
            return false;
    }


    //
    // Apply the Metadata properties back onto the resHash
    // Also modify the model to remove previous values
    //
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

        Soprano::QueryResultIterator qit = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
        while(qit.next()) {
            const Soprano::Node g = qit[0];
            m_model->removeAllStatements( resUri, NAO::lastModified(), Soprano::Node(), g );
            m_trailingGraphCandidates << g.uri();
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
            bool naoCreatedInRepo = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference ).boolValue();
            if( !naoCreatedInRepo )
                res.insert( NAO::created(), fit.value() );
        }
    }
    resMetadataHash.clear();


    // 6. Create all the blank nodes
    resHash = resolveBlankNodes( resHash );

    //
    // Actual statement pushing
    //

    // Push the data in one go
    if( !push( m_graph, resHash ) )
        return false;

    // Push all the duplicateStatements
    QList<QUrl> graphs = m_duplicateStatements.uniqueKeys();
    foreach( const QUrl& graph, graphs ) {
        QList<Soprano::Statement> stList = m_duplicateStatements.values( graph );
        Sync::ResourceHash resHash = Sync::ResourceHash::fromStatementList( stList );

        //
        // Remove all these statements with the graph
        //
        QString stPattern;
        foreach( const Soprano::Statement& st, stList ) {
            stPattern += QString::fromLatin1("%1 %2 %3 . ")
                          .arg( st.subject().toN3(),
                                st.predicate().toN3(),
                                st.object().toN3() );
        }


        QString query = QString::fromLatin1("sparql delete { graph %1 { %2 } }")
                        .arg( Soprano::Node::resourceToN3( graph ), stPattern );

        m_model->executeQuery( query, Soprano::Query::QueryLanguageUser, QLatin1String("sql") );
        if( m_model->lastError() ) {
            setError( m_model->lastError() );
            return false;
        }

        // Push all these statements
        if( !push( m_graphHash[graph], resHash ) )
            return false;
    }
    m_duplicateStatements.clear();

    // make sure we do not leave trailing empty graphs
    m_model->removeTrailingGraphs(m_trailingGraphCandidates);

    //
    // Resource Watcher
    //

    // Inform the ResourceWatcherManager of the new resources
    QSetIterator<QUrl> newUriIt( m_newUris );
    while( newUriIt.hasNext() ) {
        const QUrl newUri = newUriIt.next();

        QList<Soprano::Node> types = resHash[ newUri ].values( RDF::type() );
        m_rvm->createResource( newUri, nodeListToUriList( types ) );
    }

    // Inform the ResourceWatcherManager of the changed properties
    QHashIterator<KUrl, Sync::SyncResource> iter( resHash );
    while( iter.hasNext() ) {
        const Sync::SyncResource& res = iter.next().value();
        const Sync::SyncResource& removedRes = m_resRemoveHash.value( res.uri() );

        // FIXME: More efficient way of traversing the multi hash?
        const QList<KUrl> properties = res.uniqueKeys();
        foreach( const KUrl& propUri, properties ) {
            if( metadataProperties.contains( propUri ) )
                continue;

            const QList<Soprano::Node> added = res.values( propUri );
            const QList<Soprano::Node> removed = removedRes.values( propUri );

            m_rvm->changeProperty( res.uri(), propUri, added, removed );
        }
    }

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
        bool overwriteProperties = (m_flags & OverwriteProperties) | (m_flags & OverwriteAllProperties);
        bool lazyCardinalities = (m_flags & LazyCardinalities);

        if( !lazyCardinalities ) {
            int maxCardinality = ClassAndPropertyTree::self()->maxCardinality( propUri );
            if( maxCardinality > 0 ) {

                QStringList filterStringList;
                QStringList objectN3 = nodesToN3( objectValues );
                foreach( const QString &n3, objectN3 )
                    filterStringList << QString::fromLatin1("?v!=%1").arg( n3 );

                int existingCardinality = 0;

                if( !res.isBlank() && !overwriteProperties ) {
                    if( maxCardinality > 1 ) {
                        const QString query = QString::fromLatin1("select count(distinct ?v) where {"
                                                                " %1 %2 ?v . FILTER( %3 ) . }")
                                            .arg( Soprano::Node::resourceToN3( res.uri() ),
                                                  Soprano::Node::resourceToN3( propUri ),
                                                  filterStringList.join( QLatin1String(" && ") ) );

                        Soprano::QueryResultIterator exCarIt =
                                        m_model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
                        if( exCarIt.next() ) {
                            existingCardinality = exCarIt[0].literal().toInt();
                        }
                    }
                    else {
                        QString query = QString::fromLatin1("ask where { %1 %2 ?v . FILTER(%3) .}")
                                        .arg( Soprano::Node::resourceToN3( res.uri() ),
                                              Soprano::Node::resourceToN3( propUri ),
                                              filterStringList.first() );

                        bool e = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference ).boolValue();
                        if( e )
                            existingCardinality = 1;
                    }
                }

                const int newCardinality = objectValues.size() + existingCardinality;
                if( newCardinality > maxCardinality ) {
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
        }

        //
        // 3.b Check the domain and range
        //
        ClassAndPropertyTree* tree = ClassAndPropertyTree::self();
        QUrl domain = tree->propertyDomain( propUri );
        QUrl range = tree->propertyRange( propUri );

        //        kDebug() << "Domain : " << domain;
        //        kDebug() << "Range : " << range;

        // domain
        if( !domain.isEmpty() && !isOfType( res.uriNode(), domain, resTypes ) ) {
            // Error
            QList<QUrl> allTypes = ( resTypes + m_model->typeCache()->types(res.uri()) );

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

                if( !isOfType( object, range, objectNewTypes ) ) {
                    // Error
                    QList<QUrl> allTypes = ( objectNewTypes + m_model->typeCache()->types(objUri) );

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
