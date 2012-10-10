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


#include "syncresource.h"

#include <Soprano/Node>
#include <Soprano/Graph>
#include <Soprano/Statement>
#include <Soprano/StatementIterator>

#include "nie.h"
#include "nfo.h"
#include <Soprano/Vocabulary/RDF>

#include <QtCore/QSharedData>

class Nepomuk2::Sync::SyncResource::Private : public QSharedData {
public:
    KUrl uri;
};


Nepomuk2::Sync::SyncResource::SyncResource()
    : d( new Nepomuk2::Sync::SyncResource::Private )
{
}

Nepomuk2::Sync::SyncResource::SyncResource(const KUrl& uri)
    : d( new Nepomuk2::Sync::SyncResource::Private )
{
    setUri( uri );
}

Nepomuk2::Sync::SyncResource::SyncResource(const Nepomuk2::Sync::SyncResource& rhs)
    : QMultiHash< KUrl, Soprano::Node >(rhs),
      d( rhs.d )
{
}

Nepomuk2::Sync::SyncResource::~SyncResource()
{
}

Nepomuk2::Sync::SyncResource& Nepomuk2::Sync::SyncResource::operator=(const Nepomuk2::Sync::SyncResource& rhs)
{
    d = rhs.d;
    return *this;
}

bool Nepomuk2::Sync::SyncResource::operator==(const Nepomuk2::Sync::SyncResource& res) const
{
    return d->uri == res.d->uri &&
    this->QHash<KUrl, Soprano::Node>::operator==( res );
}

QList< Soprano::Statement > Nepomuk2::Sync::SyncResource::toStatementList() const
{
    QList<Soprano::Statement> list;
    const QList<KUrl> & keys = uniqueKeys();
    foreach( const KUrl & key, keys ) {
        Soprano::Statement st;
        Soprano::Node sub = d->uri.url().startsWith("_:") ? Soprano::Node(d->uri.url().mid(2)) : d->uri;
        st.setSubject( sub );
        st.setPredicate( Soprano::Node( key ) );

        const QList<Soprano::Node>& objects = values( key );
        foreach( const Soprano::Node & node, objects ) {
            st.setObject( node );
            list.append( st );
        }
    }
    return list;
}


bool Nepomuk2::Sync::SyncResource::isFolder() const
{
    return values( Soprano::Vocabulary::RDF::type() ).contains( Soprano::Node( Nepomuk2::Vocabulary::NFO::Folder() ) );
}


bool Nepomuk2::Sync::SyncResource::isFileDataObject() const
{
    return values( Soprano::Vocabulary::RDF::type() ).contains( Soprano::Node( Nepomuk2::Vocabulary::NFO::FileDataObject() ) );
}


KUrl Nepomuk2::Sync::SyncResource::nieUrl() const
{
    const QHash<KUrl, Soprano::Node>::const_iterator it = constFind( Nepomuk2::Vocabulary::NIE::url() );
    if( it == constEnd() )
        return KUrl();
    else
        return it.value().uri();
}


void Nepomuk2::Sync::SyncResource::setUri(const Soprano::Node& node)
{
    if( node.isResource() ) {
        d->uri = node.uri();
    }
    else if( node.isBlank() ) {
        d->uri = KUrl( node.toN3() );
    }
}

KUrl Nepomuk2::Sync::SyncResource::uri() const
{
    return d->uri;
}

Soprano::Node Nepomuk2::Sync::SyncResource::uriNode() const
{
    return d->uri.url().startsWith("_:") ? Soprano::Node(d->uri.url().mid(2)) : d->uri;
}


QList< Soprano::Node > Nepomuk2::Sync::SyncResource::property(const KUrl& url) const
{
    return values(url);
}

void Nepomuk2::Sync::SyncResource::removeObject(const KUrl& uri)
{
    QMutableHashIterator<KUrl, Soprano::Node> iter( *this );
    while( iter.hasNext() ) {
        iter.next();

        if( iter.value().isResource() && iter.value().uri() == uri )
            iter.remove();
    }
}

namespace {
    // Blank nodes are stored as "_:identifier" in urls
    QUrl getUri( const Soprano::Node & n ) {
        if( n.isBlank() )
            return QUrl( n.toN3() );
        else
            return n.uri();
    }
}
// static
Nepomuk2::Sync::SyncResource Nepomuk2::Sync::SyncResource::fromStatementList(const QList< Soprano::Statement >& list)
{
    if( list.isEmpty() )
        return SyncResource();

    SyncResource res;
    Soprano::Node subject = list.first().subject();
    res.setUri( getUri(subject) );

    foreach( const Soprano::Statement & st, list ) {
        if( st.subject() != subject )
            continue;

        KUrl pred = st.predicate().uri();
        Soprano::Node obj = st.object();

        if( !res.contains( pred, obj ) )
            res.insert( pred, obj );
    }

    return res;
}

//
// ResourceHash
//

// static
Nepomuk2::Sync::ResourceHash Nepomuk2::Sync::ResourceHash::fromGraph(const Soprano::Graph& graph)
{
    return fromStatementList( graph.listStatements().allStatements() );
}

// static
Nepomuk2::Sync::ResourceHash Nepomuk2::Sync::ResourceHash::fromStatementList(const QList< Soprano::Statement >& allStatements)
{
    //
    // Convert into multi hash for easier look up
    //
    QMultiHash<KUrl, Soprano::Statement> stHash;
    stHash.reserve( allStatements.size() );
    foreach( const Soprano::Statement & st, allStatements ) {
        KUrl uri = getUri( st.subject() );
        stHash.insert( uri, st );
    }

    //
    // Convert them into a better format --> SyncResource
    //
    const QList<KUrl> & uniqueUris = stHash.uniqueKeys();

    ResourceHash resources;
    resources.reserve( uniqueUris.size() );

    foreach( const KUrl & resUri, uniqueUris ) {
        SyncResource res = SyncResource::fromStatementList( stHash.values( resUri ) );
        resources.insert( res.uri(), res );
    }

    return resources;
}


QList< Soprano::Statement > Nepomuk2::Sync::ResourceHash::toStatementList() const
{
    QList<Soprano::Statement> stList;
    Q_FOREACH( const KUrl& uri, uniqueKeys() ) {
        const SyncResource & res = value( uri );
        stList += res.toStatementList();
    }

    return stList;
}


bool Nepomuk2::Sync::SyncResource::isValid() const
{
    return !d->uri.isEmpty() && !isEmpty();
}

bool Nepomuk2::Sync::SyncResource::isBlank() const
{
    //TODO: Figure out a way to optimize this - Converting a url to a string is expensive
    return d->uri.url().startsWith(QLatin1String("_:"));
}


uint Nepomuk2::Sync::qHash(const Nepomuk2::Sync::SyncResource& res)
{
    // WARNING: Do not use the uri to generate the hash.
    // StoreResources depends on the fact that the uri is not used while generating the hash

    uint hash = 0;
    QHashIterator<KUrl, Soprano::Node> it( res );
    while( it.hasNext() ) {
        it.next();

        hash ^= qHash( it.key() ) & qHash( it.value() );
    }

    return hash;
}

QDebug Nepomuk2::Sync::operator<<(QDebug dbg, const Nepomuk2::Sync::SyncResource& res)
{
    return dbg << res.uri() << QMultiHash<KUrl, Soprano::Node>(res);
}
