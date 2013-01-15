/*
 * Copyright (C) 2013  Vishesh Handa <me@vhanda.in>
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

#include "removablemediadatamigrator.h"
#include "resourcemanager.h"
#include "nie.h"
#include <KDebug>

#include <Soprano/Model>
#include <Soprano/QueryResultIterator>

using namespace Nepomuk2::Vocabulary;

namespace Nepomuk2 {

RemovableMediaDataMigrator::RemovableMediaDataMigrator(const RemovableMediaCache::Entry* entry)
{
    m_mountPoint = entry->mountPath();
    m_uuid = entry->url();
}

void RemovableMediaDataMigrator::run()
{
    QString query = QString::fromLatin1("select ?r ?url where { ?r a nfo:FileDataObject ;"
                                        " nie:url ?url ."
                                        " FILTER( REGEX(STR(?url), '^%1') ). }")
                    .arg( m_uuid );

    Soprano::Model* model = ResourceManager::instance()->mainModel();
    Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
    while( it.next() ) {
        const QUrl resUri = it[0].uri();
        const QUrl nieUrl = it[1].uri();

        const QString path = nieUrl.toString().mid( m_uuid.length() );
        const QUrl newUrl = QUrl::fromLocalFile( m_mountPoint + path );

        model->removeAllStatements( resUri, NIE::url(), nieUrl );
        kDebug() << nieUrl << newUrl;
        model->addStatement( resUri, NIE::url(), newUrl );
    }
}

}
