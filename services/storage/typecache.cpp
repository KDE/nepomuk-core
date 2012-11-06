/*
    <one line to give the library's name and an idea of what it does.>
    Copyright (C) 2012  Vishesh Handa <me@vhanda.in>

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


#include "typecache.h"

#include <Soprano/Model>
#include <Soprano/Node>
#include <Soprano/QueryResultIterator>

#include <Soprano/Vocabulary/RDFS>

using namespace Nepomuk2;
using namespace Soprano::Vocabulary;

TypeCache::TypeCache(Soprano::Model* model)
    : m_model(model)
{
    // Cache the last 20 resources that were accessed
    m_cache.setMaxCost( 20 );
}

TypeCache::~TypeCache()
{

}

QList< QUrl > TypeCache::types(const QUrl& uri)
{
    QMutexLocker locker( &m_mutex );

    QList<QUrl>* obj = m_cache.object( uri );
    if( obj ) {
        return *obj;
    }

    obj = new QList<QUrl>;

    QString query = QString::fromLatin1("select ?t where { %1 rdf:type ?t . }")
                    .arg( Soprano::Node::resourceToN3( uri ) );
    Soprano::QueryResultIterator it = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
    while( it.next() )
        obj->append( it[0].uri() );

    obj->append( RDFS::Resource() );

    m_cache.insert( uri, obj );

    return *obj;
}

void TypeCache::clear()
{
    QMutexLocker locker( &m_mutex );
    m_cache.clear();
}



