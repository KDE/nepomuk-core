/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2011 Sebastian Trueg <trueg@kde.org>

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

#include "simpleresourcegraph.h"
#include "simpleresource.h"
#include "datamanagement.h"
#include "storeresourcesjob.h"

#include <QtCore/QSharedData>
#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtCore/QDebug>
#include <QtCore/QDataStream>

#include <Soprano/Graph>

#include <KRandom>

class Nepomuk2::SimpleResourceGraph::Private : public QSharedData
{
public:
    QHash<QUrl, SimpleResource> resources;
};


Nepomuk2::SimpleResourceGraph::SimpleResourceGraph()
    : d(new Private)
{
}

Nepomuk2::SimpleResourceGraph::SimpleResourceGraph(const SimpleResource& resource)
    : d(new Private)
{
    insert(resource);
}

Nepomuk2::SimpleResourceGraph::SimpleResourceGraph(const QList<SimpleResource>& resources)
    : d(new Private)
{
    Q_FOREACH(const SimpleResource& res, resources) {
        insert(res);
    }
}

Nepomuk2::SimpleResourceGraph::SimpleResourceGraph(const QSet<SimpleResource>& resources)
    : d(new Private)
{
    Q_FOREACH(const SimpleResource& res, resources) {
        insert(res);
    }
}

Nepomuk2::SimpleResourceGraph::SimpleResourceGraph(const SimpleResourceGraph& other)
    : d(other.d)
{
}

Nepomuk2::SimpleResourceGraph::~SimpleResourceGraph()
{
}

Nepomuk2::SimpleResourceGraph & Nepomuk2::SimpleResourceGraph::operator=(const Nepomuk2::SimpleResourceGraph &other)
{
    d = other.d;
    return *this;
}

void Nepomuk2::SimpleResourceGraph::insert(const SimpleResource &res)
{
    d->resources.insert(res.uri(), res);
}

Nepomuk2::SimpleResourceGraph& Nepomuk2::SimpleResourceGraph::operator<<(const SimpleResource &res)
{
    insert(res);
    return *this;
}

void Nepomuk2::SimpleResourceGraph::remove(const QUrl &uri)
{
    d->resources.remove(uri);
}

void Nepomuk2::SimpleResourceGraph::remove(const SimpleResource &res)
{
    if( contains( res ) )
        remove( res.uri() );
}

void Nepomuk2::SimpleResourceGraph::add(const QUrl &uri, const QUrl &property, const QVariant &value)
{
    if(!uri.isEmpty()) {
        d->resources[uri].setUri(uri);
        d->resources[uri].addProperty(property, value);
    }
}

void Nepomuk2::SimpleResourceGraph::set(const QUrl &uri, const QUrl &property, const QVariant &value)
{
    removeAll(uri, property);
    add(uri, property, value);
}

void Nepomuk2::SimpleResourceGraph::remove(const QUrl &uri, const QUrl &property, const QVariant &value)
{
    QHash< QUrl, SimpleResource >::iterator it = d->resources.find( uri );
    if( it != d->resources.end() ) {
        it.value().remove(property, value);
    }
}

void Nepomuk2::SimpleResourceGraph::removeAll(const QUrl &uri, const QUrl &property, const QVariant &value)
{
    if(!uri.isEmpty()) {
        QHash< QUrl, SimpleResource >::iterator it = d->resources.find( uri );
        if( it != d->resources.end() ) {
            it.value().removeAll(property, value);
        }
    }
    else {
        for(QHash<QUrl, SimpleResource>::iterator it = d->resources.begin();
            it != d->resources.end(); ++it) {
            it->removeAll(property, value);
        }
    }
}

bool Nepomuk2::SimpleResourceGraph::contains(const QUrl &uri) const
{
    return d->resources.contains(uri);
}

bool Nepomuk2::SimpleResourceGraph::containsAny(const QUrl &res, const QUrl &property) const
{
    QHash< QUrl, SimpleResource >::const_iterator it = d->resources.constFind( res );
    if( it == d->resources.constEnd() )
        return false;

    return it.value().contains(property);
}

bool Nepomuk2::SimpleResourceGraph::contains(const SimpleResource &res) const
{
    QHash< QUrl, SimpleResource >::const_iterator it = d->resources.find( res.uri() );
    if( it == d->resources.constEnd() )
        return false;

    return res == it.value();
}

Nepomuk2::SimpleResource Nepomuk2::SimpleResourceGraph::operator[](const QUrl &uri) const
{
    return d->resources[uri];
}

Nepomuk2::SimpleResource& Nepomuk2::SimpleResourceGraph::operator[](const QUrl &uri)
{
    QHash<QUrl, SimpleResource>::iterator it = d->resources.find(uri);
    if( it == d->resources.end() ) {
        // One expects the uri to be set if one uses operator[]
        SimpleResource res( uri );
        it = d->resources.insert( uri, res );
    }

    return it.value();
}

QSet<Nepomuk2::SimpleResource> Nepomuk2::SimpleResourceGraph::toSet() const
{
    return QSet<SimpleResource>::fromList(toList());
}

QList<Nepomuk2::SimpleResource> Nepomuk2::SimpleResourceGraph::toList() const
{
    return d->resources.values();
}

QList<QUrl> Nepomuk2::SimpleResourceGraph::allResourceUris() const
{
    return d->resources.keys();
}

void Nepomuk2::SimpleResourceGraph::clear()
{
    d->resources.clear();
}

bool Nepomuk2::SimpleResourceGraph::isEmpty() const
{
    return d->resources.isEmpty();
}

int Nepomuk2::SimpleResourceGraph::count() const
{
    return d->resources.count();
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

Nepomuk2::SimpleResourceGraph &
Nepomuk2::SimpleResourceGraph::operator+=( const SimpleResourceGraph & graph )
{
    if ( this == &graph )
        return *this;

    if ( d->resources.size() == 0 ) {
        d->resources = graph.d->resources;
    }
    else {
        QHash<QUrl, SimpleResource>::const_iterator it;
        QHash<QUrl, SimpleResource>::iterator fit;
        for (it = graph.d->resources.begin();
              it!= graph.d->resources.end();
            ++it
            )
        {
            fit = d->resources.find(it.key());
            if ( fit == d->resources.end() ) {
                // Not found
                d->resources[it.key()] = it.value();
            }
            else {
                // Found. Should merge
                fit.value().addProperties(it.value().properties());
            }
        }
    }

    return *this;
}


void Nepomuk2::SimpleResourceGraph::addStatement(const Soprano::Statement &s)
{
    const QUrl uri = nodeToVariant(s.subject()).toUrl();
    const QVariant value = nodeToVariant(s.object());
    d->resources[uri].setUri(uri);
    d->resources[uri].addProperty(s.predicate().uri(), value);
}

void Nepomuk2::SimpleResourceGraph::addStatement(const Soprano::Node& subject, const Soprano::Node& predicate, const Soprano::Node& object)
{
    addStatement( Soprano::Statement( subject, predicate, object ) );
}

Soprano::Graph Nepomuk2::SimpleResourceGraph::toStatementGraph() const
{
    Soprano::Graph g;
    for(QHash<QUrl, SimpleResource>::const_iterator it = d->resources.constBegin();
        it != d->resources.constEnd(); ++it) {
        g += it.value().toStatementList();
    }
    return g;
}

Nepomuk2::StoreResourcesJob* Nepomuk2::SimpleResourceGraph::save(const KComponentData& component) const
{
    return Nepomuk2::storeResources(*this, Nepomuk2::IdentifyNew, Nepomuk2::NoStoreResourcesFlags, QHash<QUrl, QVariant>(), component);
}

QDebug Nepomuk2::operator<<(QDebug dbg, const Nepomuk2::SimpleResourceGraph& graph)
{
    dbg.nospace() << "SimpleResourceGraph(" << endl;
    foreach(const SimpleResource& res, graph.toList()) {
        dbg << res << endl;
    }
    dbg.nospace() << ")" << endl;
    return dbg;
}

QDataStream & Nepomuk2::operator<<(QDataStream & stream, const Nepomuk2::SimpleResourceGraph& graph)
{
    stream << graph.toList();
    return stream;
}

QDataStream & Nepomuk2::operator>>(QDataStream & stream, Nepomuk2::SimpleResourceGraph& graph)
{
    QList<SimpleResource> l;
    stream >> l;
    graph = SimpleResourceGraph(l);
    return stream;
}


bool Nepomuk2::SimpleResourceGraph::operator!=( const SimpleResourceGraph & rhs) const
{
    return !(*this == rhs);
}


bool Nepomuk2::SimpleResourceGraph::operator==( const SimpleResourceGraph & rhs) const
{
    return (d->resources == rhs.d->resources);
}
