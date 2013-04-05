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
{
}

void ResourceListGenerator::start()
{
    QTimer::singleShot( 0, this, SLOT(doJob()) );
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

    QUrl ng = nepomukGraph(m_model);
    QString query = QString::fromLatin1("select distinct ?r where { graph ?g { ?r ?p ?o }"
                                        " ?g a nrl:InstanceBase ."
                                        " FILTER( ?g!=%1 ) ."
                                        " FILTER NOT EXISTS { ?g a nrl:DiscardableInstanceBase . } }")
                    .arg( Soprano::Node::resourceToN3(ng) );

    kDebug() << "Fetching URI list";
    Soprano::QueryResultIterator rit = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );

    QFile file( m_outputFile );
    if( !file.open( QIODevice::ReadWrite | QIODevice::Append | QIODevice::Text ) ) {
        setErrorText( QString::fromLatin1("Could not open file %1").arg( m_outputFile ) );
        return;
    }

    QTextStream out( &file );

    while( rit.next() ) {
        out << rit[0].uri().toString() << "\n";
    }

    emitResult();
}
