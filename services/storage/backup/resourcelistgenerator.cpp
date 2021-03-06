/*
 * <one line to give the library's name and an idea of what it does.>
 * Copyright 2013  Vishesh Handa <me@vhanda.in>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "resourcelistgenerator.h"

#include <QtCore/QTimer>
#include <QtCore/QFile>

#include <KDebug>

#include <Soprano/QueryResultIterator>

using namespace Nepomuk2;
using namespace Nepomuk2::Backup;

ResourceListGenerator::ResourceListGenerator(Soprano::Model* model, const QString& outputFile, QObject* parent)
    : KJob(parent)
    , m_model(model)
    , m_outputFile(outputFile)
    , m_filter(Filter_None)
    , m_resourceCount(0)
{
}

void ResourceListGenerator::start()
{
    QTimer::singleShot( 0, this, SLOT(doJob()) );
}

void ResourceListGenerator::setFilter(ResourceListGenerator::Filter filter)
{
    m_filter = filter;
}

namespace {
    QUrl nepomukGraph(Soprano::Model* model) {
        QString query = QString::fromLatin1("select ?r where { ?r a nrl:Graph ; nao:maintainedBy ?app ."
                                            " ?app nao:identifier %1 . } LIMIT 1")
                        .arg( Soprano::Node::literalToN3(QLatin1String("nepomuk")) );

        Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
        if( it.next() )
            return it[0].uri();

        return QUrl();
    }
}

void ResourceListGenerator::doJob()
{
    // FIXME: Emit some kind of %?

    QFile file( m_outputFile );
    if( !file.open( QIODevice::ReadWrite | QIODevice::Append | QIODevice::Text ) ) {
        setError(1);
        setErrorText( QString::fromLatin1("Could not open file %1").arg( m_outputFile ) );
        emitResult();
        return;
    }

    QTextStream out( &file );

    if( m_filter == Filter_None ) {
        QUrl ng = nepomukGraph(m_model);
        QString query = QString::fromLatin1("select distinct ?r where { graph ?g { ?r ?p ?o }"
                                            " ?g a nrl:InstanceBase ."
                                            " FILTER( ?g!=%1 ) ."
                                            " FILTER NOT EXISTS { ?g a nrl:DiscardableInstanceBase . } }")
                        .arg( Soprano::Node::resourceToN3(ng) );

        kDebug() << "Fetching URI list";
        Soprano::QueryResultIterator rit = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );

        while( rit.next() ) {
            out << rit[0].uri().toString() << "\n";
            m_resourceCount++;
        }
    }
    else if( m_filter == Filter_FilesAndTags ) {
        QString query = QString::fromLatin1("select distinct ?r where { ?r a nao:Tag . ?f nao:hasTag ?r ."
                                            "?f a nfo:FileDataObject . }");
        Soprano::QueryResultIterator it = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );

        while( it.next() ) {
            out << it[0].uri().toString() << "\n";
            m_resourceCount++;
        }

        // file count
        QString countQuery = QString::fromLatin1("select count(distinct ?r) where { ?r a nfo:FileDataObject ;"
                                                 " nie:url ?url ; ?p ?t. "
                                                 "FILTER(?p in (nao:hasTag, nao:numericRating, nao:description)). }");
        Soprano::QueryResultIterator iter = m_model->executeQuery( countQuery, Soprano::Query::QueryLanguageSparqlNoInference );
        int approxCount = 0;
        if( iter.next() )
            approxCount = iter[0].literal().toInt() + m_resourceCount;

        query = QString::fromLatin1("select distinct ?r where { ?r a nfo:FileDataObject ;"
                                    " nie:url ?url ; ?p ?t ."
                                    " FILTER(?p in (nao:hasTag, nao:numericRating, nao:description)) ."
                                    " FILTER(REGEX(STR(?url), '^file:')) . }");
        it = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );

        while( it.next() ) {
            out << it[0].uri().toString() << "\n";
            m_resourceCount++;

            if( m_resourceCount < approxCount )
                emitPercent( m_resourceCount, approxCount );
        }
    }

    emitPercent( m_resourceCount, m_resourceCount );
    emitResult();
}
