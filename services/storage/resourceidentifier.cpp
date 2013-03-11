/*
    <one line to give the library's name and an idea of what it does.>
    Copyright (C) 2011 Vishesh Handa <handa.vish@gmail.com>

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


#include "resourceidentifier.h"
#include "syncresource.h"
#include "classandpropertytree.h"

#include <QtCore/QDateTime>
#include <QtCore/QSet>

#include <Soprano/Node>
#include <Soprano/Model>
#include <Soprano/QueryResultIterator>
#include <Soprano/Vocabulary/NAO>
#include <Soprano/StatementIterator>
#include <Soprano/NodeIterator>
#include <Soprano/Vocabulary/RDFS>
#include <Soprano/Vocabulary/RDF>
#include "nie.h"

#include <KDebug>

using namespace Soprano::Vocabulary;
using namespace Nepomuk2::Vocabulary;

namespace {
    /// used to handle sets and lists of QUrls
    template<typename T> QStringList resourcesToN3(const T& urls) {
        QStringList n3;
        Q_FOREACH(const QUrl& url, urls) {
            n3 << Soprano::Node::resourceToN3(url);
        }
        return n3;
    }
}

Nepomuk2::ResourceIdentifier::ResourceIdentifier( Nepomuk2::StoreIdentificationMode mode,
                                                 Soprano::Model *model)
    : Nepomuk2::Sync::ResourceIdentifier( model ),
      m_mode( mode )
{
    m_metaProperties.insert( NAO::created() );
    m_metaProperties.insert( NAO::lastModified() );
    m_metaProperties.insert( NAO::userVisible() );
    m_metaProperties.insert( NAO::creator() );
}


bool Nepomuk2::ResourceIdentifier::exists(const KUrl& uri)
{
    // Special case for blank nodes
    if( uri.url().startsWith("_:") )
        return false;

    QString query = QString::fromLatin1("ask { %1 ?p ?o . } ").arg( Soprano::Node::resourceToN3(uri) );
    return m_model->executeQuery( query, Soprano::Query::QueryLanguageSparql ).boolValue();
}

KUrl Nepomuk2::ResourceIdentifier::duplicateMatch(const KUrl& origUri,
                                                 const QSet<KUrl>& matchedUris )
{
    Q_UNUSED( origUri );
    //
    // We return the uri that has the oldest nao:created
    // For backwards compatibility we keep in mind that three are resources which do not have nao:created defined.
    //
    Soprano::QueryResultIterator it
            = m_model->executeQuery(QString::fromLatin1("select ?r where { ?r %1 ?date . FILTER(?r in (%2)) . } ORDER BY ASC(?date) LIMIT 1")
                                    .arg(Soprano::Node::resourceToN3(NAO::created()),
                                         resourcesToN3(matchedUris).join(QLatin1String(","))),
                                    Soprano::Query::QueryLanguageSparql);
    if(it.next()) {
        return it[0].uri();
    }
    else {
        // FIXME: fallback to what? a random one from the set?
        return KUrl();
    }
}

bool Nepomuk2::ResourceIdentifier::isIdentifyingProperty(const QUrl& uri)
{
    if( m_metaProperties.contains( uri ) ) {
        return false;
    }
    else {
        return ClassAndPropertyTree::self()->isDefiningProperty(uri);
    }
}


bool Nepomuk2::ResourceIdentifier::runIdentification(const KUrl& uri)
{
    //kDebug() << "Identifying : " << uri;
    //
    // Check if a uri with the same name exists
    //
    if( exists( uri ) ) {
        manualIdentification( uri, uri );
        return true;
    }

    const Sync::SyncResource & res = simpleResource( uri );
    //kDebug() << res;

    //
    // Check if a uri with the same nie:url exists
    //
    QUrl nieUrl = res.nieUrl();
    if( !nieUrl.isEmpty() ) {
        QString query = QString::fromLatin1("select ?r where { ?r nie:url %1 . } LIMIT 1")
                        .arg( Soprano::Node::resourceToN3( nieUrl ) );
        Soprano::QueryResultIterator it = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
        if( it.next() ) {
            const QUrl newUri = it[0].uri();
            kDebug() << uri << " --> " << newUri;
            manualIdentification( uri, newUri );
            return true;
        }

        return false;
    }

    // If IdentifyNone mode, then we do not run the full identification
    if( m_mode == IdentifyNone )
        return false;

    // Never identify data objects
    foreach(const Soprano::Node& t, res.property(RDF::type())) {
        QSet<QUrl> allT = ClassAndPropertyTree::self()->allParents(t.uri());
        allT << t.uri();
        if( allT.contains(NIE::DataObject()) ) {
            kDebug() << "Not identifying" << res.uri() << " - DataObject";
            return false;
        }
    }

    // Run the normal identification procedure
    return Sync::ResourceIdentifier::runIdentification( uri );
}
