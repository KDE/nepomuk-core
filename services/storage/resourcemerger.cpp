/*
    This file is part of the Nepomuk KDE project.
    Copyright (C) 2011-13  Vishesh Handa <handa.vish@gmail.com>

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

    template<typename T> QStringList urlsToN3( const T &urls ) {
        QStringList list;
        foreach( const QUrl& uri, urls ) {
            list << Soprano::Node::resourceToN3(uri);
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
                                         const Nepomuk2::StoreResourcesFlags& flags, bool discardable )
{
    m_app = app;
    m_discardbale = discardable;
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

        QList<QUrl> propertiesToRemove;
        QList<KUrl> properties = res.uniqueKeys();
        foreach( const QUrl& prop, properties ) {
            QList<Soprano::Node> values = res.values( prop );
            const QString propN3 = Soprano::Node::resourceToN3( prop );

            if( lazy || overwrite || overwriteAll ) {
                if( tree->maxCardinality( prop ) == 1 || overwriteAll ) {
                    QString query = QString::fromLatin1("select ?o where { %1 %2 ?o . }")
                                    .arg( resN3, propN3 );

                    Soprano::QueryResultIterator it = m_model->executeQuery(query, Soprano::Query::QueryLanguageSparqlNoInference);
                    while (it.next()) {
                        m_resRemoveHash[ res.uri() ].insert( prop, it[0] );
                    }
                    propertiesToRemove << prop;

                    // In LazyCardinalities we don't care about the cardinality
                    if( lazy )
                        values = QList<Soprano::Node>() << values.first();
                }
            }

            query += QString::fromLatin1(" %1 %2 ;").arg( propN3, nodesToN3(values).join(QString(", ")) );
        }

        query[ query.length() - 1 ] = '.';

        // Remove the properties
        if( propertiesToRemove.count() ) {
            QString query = QString::fromLatin1("sparql delete { graph ?g { %1 ?p ?o.} } where { "
                                                " graph ?g { %1 ?p ?o. } FILTER(?p in(%2)) . }")
                            .arg( resN3, urlsToN3(propertiesToRemove).join(",") );

            m_model->executeQuery( query, Soprano::Query::QueryLanguageUser, QLatin1String("sql") );
        }

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

        res.setUri( resolveBlankNode( res.uriNode() ) );

        QMutableHashIterator<KUrl, Soprano::Node> it( res );
        while( it.hasNext() ) {
            it.next();
            it.setValue( resolveBlankNode(it.value()) );
        }

        resHash.insert( res.uri(), res );
    }

    return resHash;
}


bool Nepomuk2::ResourceMerger::merge(const Nepomuk2::Sync::ResourceHash& resHash_)
{
    //
    // 1. Resolve all the mapped statements
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

    Soprano::LiteralValue currentDateTime( QDateTime::currentDateTime() );

    //
    // 2. Move the metadata from resHash to the metadataHash
    //
    Sync::ResourceHash resMetadataHash;
    QStringList resN3List;

    QMutableHashIterator<KUrl, Sync::SyncResource> it( resHash );
    while( it.hasNext() ) {
        Sync::SyncResource& res = it.next().value();

        Sync::SyncResource metadataRes( res.uri() );

        // Remove the metadata properties - nao:lastModified and nao:created
        QHash<KUrl, Soprano::Node>::iterator fit = res.find( NAO::lastModified() );
        if( fit != res.end() ) {
            metadataRes.insert( NAO::lastModified(), fit.value() );
            res.erase( fit );
        }
        else {
            metadataRes.insert( NAO::lastModified(), currentDateTime );
        }

        if( !res.isBlank() )
            resN3List << Soprano::Node::resourceToN3(res.uri());

        fit = res.find( NAO::created() );
        if( fit != res.end() ) {
            // We do not allow the nao:created to be changed
            if( res.isBlank() )
                metadataRes.insert( NAO::created(), fit.value() );

            res.erase( fit );
        }
        else {
            if( res.isBlank() )
                metadataRes.insert( NAO::created(), currentDateTime );
        }

        // Also move the nie:url
        fit = res.find( NIE::url() );
        if( fit != res.end() ) {
            metadataRes.insert( NIE::url(), fit.value() );
            res.erase( fit );
        }

        resMetadataHash.insert( metadataRes.uri(), metadataRes );

        if( !hasValidData( resHash, res ) )
            return false;

    }

    // nao:lastModified
    if( resN3List.count() ) {
        QString removeCommand = QString::fromLatin1("sparql delete from %1 { ?r nao:lastModified ?m . } where"
                                                    "{ graph %1 { ?r nao:lastModified ?m . FILTER(?r in (%2)) .} }")
                                .arg( Soprano::Node::resourceToN3(m_model->nepomukGraph()),
                                    resN3List.join(",") );
        m_model->executeQuery( removeCommand, Soprano::Query::QueryLanguageUser, QLatin1String("sql") );
    }
    resN3List.clear();

    // Create the main graph, if they are any statements to merge
    if( !resHash.isEmpty() ) {
        m_graph = m_model->fetchGraph(m_app, m_discardbale);
        if( m_graph.isEmpty() || m_model->lastError() )
            return false;
    }


    // 6. Create all the blank nodes
    resHash = resolveBlankNodes( resHash );
    resMetadataHash = resolveBlankNodes( resMetadataHash );

    //
    // Actual statement pushing
    //

    // Push the data in one go
    if( !push( m_graph, resHash ) )
        return false;

    if( !push( m_model->nepomukGraph(), resMetadataHash ) )
        return false;

    //
    // Resource Watcher
    //

    // Inform the ResourceWatcherManager of the new resources
    QSetIterator<QUrl> newUriIt( m_newUris );
    while( newUriIt.hasNext() ) {
        const QUrl newUri = newUriIt.next();

        QList<Soprano::Node> types = resHash[ newUri ].values( RDF::type() );
        QSet<QUrl> allTypes = ClassAndPropertyTree::self()->allParents( nodeListToUriList(types) );
        m_rvm->createResource( newUri, allTypes );
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

void Nepomuk2::ResourceMerger::clearError()
{
    m_error = Soprano::Error::Error();
}

Soprano::Error::Error Nepomuk2::ResourceMerger::lastError()
{
    return m_error;
}

void Nepomuk2::ResourceMerger::setError(const Soprano::Error::Error& error)
{
    m_error = error;
}

void Nepomuk2::ResourceMerger::setError(const QString& errorMessage, int code)
{
    m_error = Soprano::Error::Error( errorMessage, code );
}


