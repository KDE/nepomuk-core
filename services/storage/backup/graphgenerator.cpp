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

#include "graphgenerator.h"

#include <QtCore/QTimer>
#include <QtCore/QFile>

#include <KDebug>

#include <Soprano/QueryResultIterator>
#include <Soprano/PluginManager>
#include <Soprano/Serializer>
#include <Soprano/Parser>
#include <Soprano/Util/SimpleStatementIterator>

using namespace Nepomuk2;
using namespace Nepomuk2::Backup;

GraphGenerator::GraphGenerator(Soprano::Model* model, const QString& inputFile,
                               const QString& outputFile, QObject* parent)
    : KJob(parent)
    , m_model(model)
    , m_inputFile(inputFile)
    , m_outputFile(outputFile)
    , m_numStatements(0)
{
}

void GraphGenerator::start()
{
    QTimer::singleShot( 0, this, SLOT(doJob()) );
}

namespace {
    QList<QUrl> fetchGraphApps(Soprano::Model* model, const QUrl& graph) {
        QString query = QString::fromLatin1("select ?a where { %1 nao:maintainedBy ?a . }")
                        .arg( Soprano::Node::resourceToN3( graph) );

        Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
        QList<QUrl> apps;
        while( it.next() )
            apps << it[0].uri();

        return apps;
    }

    QList<Soprano::Statement> fetchStatements(Soprano::Model* model, const QUrl& uri) {
        QString query = QString::fromLatin1("select ?p ?o ?g where { graph ?g { %1 ?p ?o. } }")
                        .arg( Soprano::Node::resourceToN3(uri) );

        QList<Soprano::Statement> stList;
        QSet<QUrl> graphs;
        Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
        while( it.next() ) {
            Soprano::Statement st( uri, it[0], it[1], it[2] );
            stList << st;

            graphs << st.context().uri();
        }

        graphs.remove( uri );
        foreach(const QUrl& g, graphs) {
            stList << fetchStatements( model, g );
        }

        return stList;
    }
}
namespace {
    QUrl fetchGraph(Soprano::Model* model, const QString& identifier) {
        QString query = QString::fromLatin1("select ?r where { ?r a nrl:Graph ; nao:maintainedBy ?app ."
                                            " ?app nao:identifier %1 . } LIMIT 1")
                        .arg( Soprano::Node::literalToN3( identifier ) );

        Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
        if( it.next() )
            return it[0].uri();

        return QUrl();
    }
}

void GraphGenerator::doJob()
{
    Soprano::PluginManager* pg = Soprano::PluginManager::instance();
    const Soprano::Serializer* serializer = pg->discoverSerializerForSerialization( Soprano::SerializationNQuads );
    const Soprano::Parser* parser = pg->discoverParserForSerialization( Soprano::SerializationNQuads );

    QFile input( m_inputFile );

    QFile output( m_outputFile );
    if( !output.open( QIODevice::ReadWrite | QIODevice::Append | QIODevice::Text ) ) {
        setError(1);
        setErrorText( QString::fromLatin1("Could not open file %1").arg( m_outputFile ) );
        emitResult();
        return;
    }

    QTextStream outputStream( &output );

    QHash<QUrl, QUrl> appGraphHash;

    kDebug() << "Input:" << m_inputFile;
    Soprano::StatementIterator it = parser->parseFile( m_inputFile, QUrl(), Soprano::SerializationNQuads );
    if( parser->lastError() ) {
        QString error = QString::fromLatin1("Failed to generate backup: %1").arg( parser->lastError().message() );
        setError(1);
        setErrorText( error );
        emitResult();
        return;
    }

    int count=0;
    QUrl nepomukGraph = fetchGraph( m_model, QLatin1String("nepomuk") );
    while( it.next() ) {
        Soprano::Statement st = it.current();
        const QUrl origGraph = st.context().uri();

        QList<Soprano::Statement> stList;
        QList<QUrl> apps = fetchGraphApps( m_model, origGraph );
        if( apps.isEmpty() ) {
            st.setContext( nepomukGraph );
        }
        foreach(const QUrl& app, apps) {
            QHash< QUrl, QUrl >::iterator fit = appGraphHash.find( app );
            if( fit == appGraphHash.end() ) {
                m_inputCount++;
                fit = appGraphHash.insert( app, origGraph );
            }

            st.setContext( fit.value() );
            stList << st;
        }

        Soprano::Util::SimpleStatementIterator iter( stList );
        serializer->serialize( iter, outputStream, Soprano::SerializationNQuads );

        m_numStatements += stList.size();
        if( m_inputCount ) {
            count++;
            emitPercent( count, m_inputCount );
        }
    }

    // Push all the graphs
    QHashIterator<QUrl, QUrl> hit( appGraphHash );
    while( hit.hasNext() ) {
        hit.next();

        QList<Soprano::Statement> stList;
        stList << fetchStatements( m_model, hit.key() );
        stList << fetchStatements( m_model, hit.value() );

        Soprano::Util::SimpleStatementIterator iter( stList );
        serializer->serialize( iter, outputStream, Soprano::SerializationNQuads );

        m_numStatements += stList.size();
        if( m_inputCount ) {
            count++;
            emitPercent( count, m_inputCount );
        }
    }

    emitResult();
}


int GraphGenerator::numStatements()
{
    return m_numStatements;
}
