/*
 * <one line to give the library's name and an idea of what it does.>
 * Copyright (C) 2012  Vishesh Handa <me@vhanda.in>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "uri.h"
#include <KUrl>

#include <Soprano/Node>
#include <Soprano/Model>
#include <Soprano/QueryResultIterator>
#include <Soprano/Vocabulary/NAO>

#include "resourcemanager.h"

using namespace Soprano::Vocabulary;

namespace Nepomuk2 {

QHash<QString, QString> s_prefixHash;

QHash<QString, QString> createPrefixHash() {
    QString query = QString::fromLatin1("select ?g ?abr where { ?r %1 ?g ; %2 ?abr . }")
                    .arg( Soprano::Node::resourceToN3( NAO::hasDefaultNamespace() ),
                          Soprano::Node::resourceToN3( NAO::hasDefaultNamespaceAbbreviation() ) );

    Soprano::Model* model = Nepomuk2::ResourceManager::instance()->mainModel();
    Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );

    QHash<QString, QString> hash;
    while( it.next() ) {
        hash.insert( it[0].toString(), it[1].toString() );
    }

    return hash;
}

Uri::Uri(const QUrl& uri)
{
    // FIXME: Not particularly thread safe
    if( s_prefixHash.isEmpty() )
        s_prefixHash = createPrefixHash();

    init( uri );
}

Uri::~Uri()
{

}

void Uri::init(const QUrl& uri)
{
    m_uri = uri;
    m_prefix.clear();
    m_suffix.clear();
    m_n3.clear();

    QString uriString = uri.toString();

    int index = uriString.indexOf('#');
    if( index != -1 ) {
        m_prefix = s_prefixHash.value( uriString.mid( 0, index+1 ) );
        m_suffix = uriString.mid( index+1 );

        m_n3 = m_prefix + QLatin1Char(':') + m_suffix;
        return;
    }

    if( uriString.startsWith("http://purl.org/dc/") ) {
        m_prefix = "purl";
        m_suffix = KUrl(m_uri).fileName();

        m_n3 = m_prefix + QLatin1Char(':') + m_suffix;
        return;
    }

    m_n3 = Soprano::Node::resourceToN3( m_uri );
}

QString Uri::prefix()
{
    return m_prefix;
}

QString Uri::suffix()
{
    return m_suffix;
}

QString Uri::toN3()
{
    return m_n3;
}

}
