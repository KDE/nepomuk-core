/*
    This file is part of the Nepomuk KDE project.
    Copyright (C) 2010  Vishesh Handa <handa.vish@gmail.com>

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


#include "resourceidentifier.h"
#include "resourceidentifier_p.h"
#include "syncresource.h"
#include "identificationsetgenerator_p.h"
#include "nrio.h"

#include <QtCore/QSet>

#include <Soprano/Statement>
#include <Soprano/Graph>
#include <Soprano/Node>
#include <Soprano/BindingSet>
#include <Soprano/StatementIterator>
#include <Soprano/QueryResultIterator>
#include <Soprano/Model>

#include <Soprano/Vocabulary/RDF>
#include <Soprano/Vocabulary/RDFS>
#include <Soprano/Vocabulary/NAO>
#include "nie.h"

#include "resource.h"
#include "resourcemanager.h"
#include "variant.h"

#include <KDebug>
#include <KUrl>

using namespace Nepomuk2::Vocabulary;
using namespace Soprano::Vocabulary;

Nepomuk2::Sync::ResourceIdentifier::ResourceIdentifier(Soprano::Model * model)
    : d( new Nepomuk2::Sync::ResourceIdentifier::Private(this) )
{
    d->m_model = model ? model : ResourceManager::instance()->mainModel();
}

Nepomuk2::Sync::ResourceIdentifier::Private::Private( ResourceIdentifier * parent )
    : q( parent ),
      m_model(0)
{
}

Nepomuk2::Sync::ResourceIdentifier::~ResourceIdentifier()
{
    delete d;
}


void Nepomuk2::Sync::ResourceIdentifier::addStatement(const Soprano::Statement& st)
{
    SyncResource res;
    res.setUri( st.subject() );

    QHash<KUrl, SyncResource>::iterator it = d->m_resourceHash.find( res.uri() );
    if( it != d->m_resourceHash.end() ) {
        SyncResource & res = it.value();
        res.insert( st.predicate().uri(), st.object() );
        return;
    }

   // Doesn't exist - Create it and insert it into the resourceHash
   res.insert( st.predicate().uri(), st.object() );

   d->m_resourceHash.insert( res.uri(), res );
   d->m_notIdentified.insert( res.uri() );
}

void Nepomuk2::Sync::ResourceIdentifier::addStatements(const Soprano::Graph& graph)
{
    ResourceHash resHash = ResourceHash::fromGraph( graph );

    KUrl::List uniqueKeys = resHash.uniqueKeys();
    foreach( const KUrl & resUri, uniqueKeys ) {
        QHash<KUrl, SyncResource>::iterator it = d->m_resourceHash.find( resUri );
        if( it != d->m_resourceHash.end() ) {
            it.value() += resHash.value( resUri );
        }
        else {
            d->m_resourceHash.insert( resUri, resHash.value( resUri ) );
        }
    }

    d->m_notIdentified += uniqueKeys.toSet();
}


void Nepomuk2::Sync::ResourceIdentifier::addStatements(const QList< Soprano::Statement >& stList)
{
    addStatements( Soprano::Graph( stList ) );
}


void Nepomuk2::Sync::ResourceIdentifier::addSyncResource(const Nepomuk2::Sync::SyncResource& res)
{
    Q_ASSERT( !res.uri().isEmpty() );
    QHash<KUrl, SyncResource>::iterator it = d->m_resourceHash.find( res.uri() );
    if( it == d->m_resourceHash.end() ) {
        d->m_resourceHash.insert( res.uri(), res );
        d->m_notIdentified.insert( res.uri() );
    }
    else {
        it.value().unite( res );
    }
}


//
// Identification
//

void Nepomuk2::Sync::ResourceIdentifier::identifyAll()
{
    int totalSize = d->m_notIdentified.size();
    kDebug() << totalSize;

    return identify( d->m_notIdentified.toList() );
}


bool Nepomuk2::Sync::ResourceIdentifier::identify(const KUrl& uri)
{
    // If already identified
    if( d->m_hash.contains( uri ) )
        return true;

    // Avoid recursive calls
    if( d->m_beingIdentified.contains( uri ) )
        return false;

    bool result = runIdentification( uri );
    d->m_beingIdentified.remove( uri );

    if( result )
        d->m_notIdentified.remove( uri );

    return result;
}


void Nepomuk2::Sync::ResourceIdentifier::identify(const KUrl::List& uriList)
{
    foreach( const KUrl & uri, uriList ) {
        identify( uri );
    }
}

bool Nepomuk2::Sync::ResourceIdentifier::runIdentification(const KUrl& uri)
{
    const Sync::SyncResource & res = simpleResource( uri );

    // Make sure that the res has some rdf:type statements
    if( !res.contains( RDF::type() ) ) {
        kDebug() << "No rdf:type statements - Not identifying";
        return false;
    }

    QStringList identifyingProperties;
    QHash<KUrl, Soprano::Node> identifyingPropertiesHash;

    QHash< KUrl, Soprano::Node >::const_iterator it = res.constBegin();
    QHash< KUrl, Soprano::Node >::const_iterator constEnd = res.constEnd();
    QList<Soprano::Node> requiredTypes;
    for( ; it != constEnd; it++ ) {
        const QUrl & prop = it.key();

        // Special handling for rdf:type
        if( prop == RDF::type() ) {
            requiredTypes << it.value().uri();
            continue;
        }

        if( !isIdentifyingProperty( prop ) ) {
            continue;
        }

        identifyingProperties << Soprano::Node::resourceToN3( prop );

        Soprano::Node object = it.value();
        if( object.isBlank()
            || ( object.isResource() && object.uri().scheme() == QLatin1String("nepomuk") ) ) {

            QUrl objectUri = object.isResource() ? object.uri() : QString( "_:" + object.identifier() );
            if( !identify( objectUri ) ) {
                //kDebug() << "Identification of object " << objectUri << " failed";
                continue;
            }

            object = mappedUri( objectUri );
        }

        identifyingPropertiesHash.insert(prop, object);
    }

    if( identifyingPropertiesHash.isEmpty() ) {
        //kDebug() << "No identification properties found!";
        return false;
    }


    // construct the identification query
    QString query = QLatin1String("select distinct ?r where { ");

    //
    // Optimization:
    // If there is only one identifying property using all that optional and filter stuff
    // slows the queries down incredibly. Thus, we make it a special case.
    //
    if(identifyingPropertiesHash.count() > 1) {
        int numIdentifyingProperties = 0;
        for(QHash<KUrl, Soprano::Node>::const_iterator it = identifyingPropertiesHash.constBegin();
            it != identifyingPropertiesHash.constEnd(); ++it) {
            // FIXME: What about optional properties?
            query += QString::fromLatin1(" optional { ?r %1 ?o%3 . } . filter(!bound(?o%3) || ?o%3=%2). ")
                     .arg( Soprano::Node::resourceToN3( it.key() ),
                           it.value().toN3(),
                           QString::number( numIdentifyingProperties++ ) );
        }

        // Make sure at least one of the identification properties has been matched
        // by adding filter( bound(?o1) || bound(?o2) ... )
        query += QString::fromLatin1("filter( ");
        for( int i=0; i<numIdentifyingProperties-1; i++ ) {
            query += QString::fromLatin1(" bound(?o%1) || ").arg( QString::number( i ) );
        }
        query += QString::fromLatin1(" bound(?o%1) ) . ").arg( QString::number( numIdentifyingProperties - 1 ) );
    }
    else {
        query += QString::fromLatin1("?r %1 %2 . ").arg(Soprano::Node::resourceToN3(identifyingPropertiesHash.constBegin().key()),
                                                         identifyingPropertiesHash.constBegin().value().toN3());
    }

    //
    // For performance reasons we add a limit even though this could mean that we
    // miss a resource to identify since we check the types below.
    //
    query += QLatin1String("} LIMIT 100");


    //
    // Fetch a score for each result.
    // We do this in a separate query for performance reasons.
    //
    QMultiHash<int, KUrl> resultsScoreHash;
    int maxScore = -1;
    Soprano::QueryResultIterator qit = d->m_model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
    while( qit.next() ) {
        const Soprano::Node r(qit["r"]);

        //
        // Check the type requirements. Experiments have shown this to mean a substantial
        // performance boost as compared to doing it in the main query.
        //
        if(!requiredTypes.isEmpty() ) {
            query = QLatin1String("ask where { ");
            foreach(const Soprano::Node& type, requiredTypes) {
                query += QString::fromLatin1("%1 a %2 . ").arg(r.toN3(), type.toN3());
            }
            query += QLatin1String("}");
            if(!d->m_model->executeQuery(query, Soprano::Query::QueryLanguageSparql).boolValue()) {
                continue;
            }
        }


        const int score = d->m_model->executeQuery(QString::fromLatin1("select count(?p) as ?cnt where { "
                                                                       "%1 ?p ?o. filter( ?p in (%2) ) . }")
                                                   .arg( r.toN3(),
                                                         identifyingProperties.join(",") ),
                                                   Soprano::Query::QueryLanguageSparqlNoInference)
                          .allBindings().first()["cnt"].literal().toInt();

        if( maxScore < score ) {
            maxScore = score;
        }

        resultsScoreHash.insert(score, r.uri());
    }

    //
    // Only get the results which have the maximum score
    //
    QSet<KUrl> results = QSet<KUrl>::fromList(resultsScoreHash.values(maxScore));


    //kDebug() << "Got " << results.size() << " results";
    if( results.empty() )
        return false;

    KUrl newUri;
    if( results.size() == 1 ) {
        newUri = *results.begin();
    }
    else {
        kDebug() << "DUPLICATE RESULTS!";
        newUri = duplicateMatch( res.uri(), results );
    }

    if( !newUri.isEmpty() ) {
        kDebug() << uri << " --> " << newUri;
        manualIdentification( uri, newUri );
        return true;
    }

    return false;
}


bool Nepomuk2::Sync::ResourceIdentifier::allIdentified() const
{
    return d->m_notIdentified.isEmpty();
}

//
// Getting the info
//

Soprano::Model* Nepomuk2::Sync::ResourceIdentifier::model()
{
    return d->m_model;
}

void Nepomuk2::Sync::ResourceIdentifier::setModel(Soprano::Model* model)
{
    d->m_model = model ? model : ResourceManager::instance()->mainModel();
}

KUrl Nepomuk2::Sync::ResourceIdentifier::mappedUri(const KUrl& resourceUri) const
{
    QHash< QUrl, QUrl >::iterator it = d->m_hash.find( resourceUri );
    if( it != d->m_hash.end() )
        return it.value();
    return KUrl();
}

KUrl::List Nepomuk2::Sync::ResourceIdentifier::mappedUris() const
{
    return d->m_hash.uniqueKeys();
}

QHash<QUrl, QUrl> Nepomuk2::Sync::ResourceIdentifier::mappings() const
{
    return d->m_hash;
}

Nepomuk2::Sync::SyncResource Nepomuk2::Sync::ResourceIdentifier::simpleResource(const KUrl& uri)
{
    QHash< KUrl, SyncResource >::const_iterator it = d->m_resourceHash.constFind( uri );
    if( it != d->m_resourceHash.constEnd() ) {
        return it.value();
    }

    return SyncResource();
}


Soprano::Graph Nepomuk2::Sync::ResourceIdentifier::statements(const KUrl& uri)
{
    return simpleResource( uri ).toStatementList();
}

QList< Soprano::Statement > Nepomuk2::Sync::ResourceIdentifier::identifyingStatements() const
{
    return d->m_resourceHash.toStatementList();
}


QSet< KUrl > Nepomuk2::Sync::ResourceIdentifier::unidentified() const
{
    return d->m_notIdentified;
}

QSet< QUrl > Nepomuk2::Sync::ResourceIdentifier::identified() const
{
    return d->m_hash.keys().toSet();
}


//
// Property settings
//

void Nepomuk2::Sync::ResourceIdentifier::addOptionalProperty(const QUrl& property)
{
    d->m_optionalProperties.append( property );
}

void Nepomuk2::Sync::ResourceIdentifier::clearOptionalProperties()
{
    d->m_optionalProperties.clear();
}

KUrl::List Nepomuk2::Sync::ResourceIdentifier::optionalProperties() const
{
    return d->m_optionalProperties;
}


namespace {

    QString stripFileName( const QString & url ) {
        //kDebug() << url;
        int lastIndex = url.lastIndexOf('/') + 1; // the +1 is because we want to keep the trailing /
        return QString(url).remove( lastIndex, url.size() );
    }
}


void Nepomuk2::Sync::ResourceIdentifier::forceResource(const KUrl& oldUri, const Nepomuk2::Resource& res)
{
    d->m_hash[ oldUri ] = res.uri();
    d->m_notIdentified.remove( oldUri );

    if( res.isFile() ) {
        const QUrl nieUrlProp = Nepomuk2::Vocabulary::NIE::url();

        Sync::SyncResource & simRes = d->m_resourceHash[ oldUri ];
        KUrl oldNieUrl = simRes.nieUrl();
        KUrl newNieUrl = res.property( nieUrlProp ).toUrl();

        //
        // Modify resourceUri's nie:url
        //
        simRes.remove( nieUrlProp );
        simRes.insert( nieUrlProp, Soprano::Node( newNieUrl ) );

        // Remove from list. Insert later
        d->m_notIdentified.remove( oldUri );

        //
        // Modify other non identified resources with similar nie:urls
        //
        QString oldString;
        QString newString;

        if( !simRes.isFolder() ) {
            oldString = stripFileName( oldNieUrl.url( KUrl::RemoveTrailingSlash ) );
            newString = stripFileName( newNieUrl.url( KUrl::RemoveTrailingSlash ) );

            kDebug() << oldString;
            kDebug() << newString;
        }
        else {
            oldString = oldNieUrl.url( KUrl::AddTrailingSlash );
            newString = newNieUrl.url( KUrl::AddTrailingSlash );
        }

        foreach( const KUrl & uri, d->m_notIdentified ) {
            // Ignore If already identified
            if( d->m_hash.contains( uri ) )
                continue;

            Sync::SyncResource& simpleRes = d->m_resourceHash[ uri ];
            // Check if it has a nie:url
            QString nieUrl = simpleRes.nieUrl().url();
            if( nieUrl.isEmpty() )
                return;

            // Modify the existing nie:url
            if( nieUrl.startsWith(oldString) ) {
                nieUrl.replace( oldString, newString );

                simpleRes.remove( nieUrlProp );
                simpleRes.insert( nieUrlProp, Soprano::Node( KUrl(nieUrl) ) );
            }
        }

        d->m_notIdentified.insert( oldUri );
    }
}


bool Nepomuk2::Sync::ResourceIdentifier::ignore(const KUrl& resUri, bool ignoreSub)
{
    kDebug() << resUri;
    kDebug() << "Ignore Sub : " << ignoreSub;

    if( d->m_hash.contains( resUri ) ) {
        kDebug() << d->m_hash;
        return false;
    }

    // Remove the resource
    const Sync::SyncResource & res = d->m_resourceHash.value( resUri );
    d->m_resourceHash.remove( resUri );
    d->m_notIdentified.remove( resUri );

    kDebug() << "Removed!";

    // Remove all the statements that contain the resoruce
    QList<KUrl> allUris = d->m_resourceHash.uniqueKeys();
    foreach( const KUrl & uri, allUris ) {
        SyncResource res = d->m_resourceHash[ uri ];
        res.removeObject( resUri );
    }

    if( ignoreSub || !res.isFolder() )
        return true;

    //
    // Remove all the subfolders
    //
    const QUrl nieUrlProp = Nepomuk2::Vocabulary::NIE::url();
    QList<Soprano::Node> nieUrlNodes = res.values( nieUrlProp );
    if( nieUrlNodes.size() != 1 )
        return false;

    KUrl mainNieUrl = nieUrlNodes.first().uri();

    foreach( const KUrl & uri, d->m_notIdentified ) {
        Sync::SyncResource res = d->m_resourceHash[ uri ];

        // If already identified
        if( d->m_hash.contains(uri) )
            continue;

        // Check if it has a nie:url
        QList<Soprano::Node> nieUrls = res.values( nieUrlProp );
        if( nieUrls.empty() )
            continue;

        QString nieUrl = nieUrls.first().uri().toString();

        // Check if the nie url contains the mainNieUrl
        if( nieUrl.contains( mainNieUrl.toLocalFile( KUrl::AddTrailingSlash ) ) ) {
            d->m_resourceHash.remove( uri );
            d->m_notIdentified.remove( uri );
        }
    }
    return true;
}


KUrl Nepomuk2::Sync::ResourceIdentifier::duplicateMatch(const KUrl& uri, const QSet< KUrl >& matchedUris)
{
    Q_UNUSED( uri );
    Q_UNUSED( matchedUris );

    // By default - Identification fails
    return KUrl();
}

// static
Soprano::Graph Nepomuk2::Sync::ResourceIdentifier::createIdentifyingStatements(const KUrl::List& uriList)
{
    IdentificationSetGenerator gen( uriList.toSet(), ResourceManager::instance()->mainModel(), QSet<KUrl>() );
    return gen.generate();
}

void Nepomuk2::Sync::ResourceIdentifier::manualIdentification(const KUrl& oldUri, const KUrl& newUri)
{
    d->m_hash[ oldUri ] = newUri;
    d->m_notIdentified.remove( oldUri );
}

bool Nepomuk2::Sync::ResourceIdentifier::isIdentifyingProperty(const QUrl& uri)
{
    if( uri == NAO::created()
        || uri == NAO::creator()
        || uri == NAO::lastModified()
        || uri == NAO::userVisible() ) {
        return false;
    }

    // TODO: Hanlde nxx:FluxProperty and nxx:resourceRangePropWhichCanIdentified
    const QString query = QString::fromLatin1("ask { %1 %2 ?range . "
                                                " %1 a %3 . "
                                                "{ FILTER( regex(str(?range), '^http://www.w3.org/2001/XMLSchema#') ) . }"
                                                " UNION { %1 a rdf:Property . } }") // rdf:Property should be nxx:resourceRangePropWhichCanIdentified
                            .arg( Soprano::Node::resourceToN3( uri ),
                                Soprano::Node::resourceToN3( RDFS::range() ),
                                Soprano::Node::resourceToN3( RDF::Property() ) );

    return model()->executeQuery( query, Soprano::Query::QueryLanguageSparql ).boolValue();
}