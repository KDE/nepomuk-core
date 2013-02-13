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


#include "syncresourceidentifier.h"
#include "syncresource.h"

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
{
    m_model = model;
}

Nepomuk2::Sync::ResourceIdentifier::~ResourceIdentifier()
{

}


void Nepomuk2::Sync::ResourceIdentifier::addSyncResource(const Nepomuk2::Sync::SyncResource& res)
{
    Q_ASSERT( !res.uri().isEmpty() );
    QHash<KUrl, SyncResource>::iterator it = m_resourceHash.find( res.uri() );
    if( it == m_resourceHash.end() ) {
        m_resourceHash.insert( res.uri(), res );
        m_notIdentified.insert( res.uri() );
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
    return identify( m_notIdentified.toList() );
}


bool Nepomuk2::Sync::ResourceIdentifier::identify(const KUrl& uri)
{
    // If already identified
    if( m_hash.contains( uri ) )
        return true;

    // Avoid recursive calls
    if( m_beingIdentified.contains( uri ) )
        return false;

    bool result = runIdentification( uri );
    m_beingIdentified.remove( uri );

    if( result )
        m_notIdentified.remove( uri );

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
    Sync::SyncResource res = simpleResource( uri );

    // Make sure that the res has some rdf:type statements
    if( !res.contains( RDF::type() ) ) {
        kDebug() << "No rdf:type statements - Not identifying";
        return false;
    }

    // Remove the types
    QList<Soprano::Node> requiredTypes = res.values( RDF::type() );
    res.remove( RDF::type() );

    QStringList identifyingProperties;
    QHash<KUrl, Soprano::Node> identifyingPropertiesHash;

    QHash< KUrl, Soprano::Node >::const_iterator it = res.constBegin();
    QHash< KUrl, Soprano::Node >::const_iterator constEnd = res.constEnd();
    for( ; it != constEnd; it++ ) {
        const QUrl & prop = it.key();

        if( !isIdentifyingProperty( prop ) ) {
            continue;
        }

        identifyingProperties << Soprano::Node::resourceToN3( prop );

        // For the case when the property has a resource range, and is still identifying
        Soprano::Node object = it.value();
        // vHanda: Should we really be identifying nepomuk uris?
        if( object.isBlank()
            || ( object.isResource() && object.uri().scheme() == QLatin1String("nepomuk") ) ) {

            QUrl objectUri = object.isResource() ? object.uri() : QString( "_:" + object.identifier() );
            if( !identify( objectUri ) ) {
                //kDebug() << "Identification of object " << objectUri << " failed";
                continue;
            }

            object = m_hash.value( objectUri );
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
    Soprano::QueryResultIterator qit = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
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
            if(!m_model->executeQuery(query, Soprano::Query::QueryLanguageSparql).boolValue()) {
                continue;
            }
        }


        const int score = m_model->executeQuery(QString::fromLatin1("select count(?p) as ?cnt where { "
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


//
// Getting the info
//

QHash<QUrl, QUrl> Nepomuk2::Sync::ResourceIdentifier::mappings() const
{
    return m_hash;
}

Nepomuk2::Sync::SyncResource Nepomuk2::Sync::ResourceIdentifier::simpleResource(const KUrl& uri)
{
    QHash< KUrl, SyncResource >::const_iterator it = m_resourceHash.constFind( uri );
    if( it != m_resourceHash.constEnd() ) {
        return it.value();
    }

    return SyncResource();
}

Nepomuk2::Sync::ResourceHash Nepomuk2::Sync::ResourceIdentifier::resourceHash() const
{
    return m_resourceHash;
}

KUrl Nepomuk2::Sync::ResourceIdentifier::duplicateMatch(const KUrl& uri, const QSet< KUrl >& matchedUris)
{
    Q_UNUSED( uri );
    Q_UNUSED( matchedUris );

    // By default - Identification fails
    return KUrl();
}

void Nepomuk2::Sync::ResourceIdentifier::manualIdentification(const KUrl& oldUri, const KUrl& newUri)
{
    m_hash[ oldUri ] = newUri;
    m_notIdentified.remove( oldUri );
}
