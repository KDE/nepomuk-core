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

#include "statementgenerator.h"

#include <QtCore/QTimer>
#include <QtCore/QFile>

#include <KDebug>

#include <Soprano/QueryResultIterator>
#include <Soprano/PluginManager>
#include <Soprano/Serializer>
#include <Soprano/Util/SimpleStatementIterator>
#include <Soprano/Vocabulary/NAO>

#include "nfo.h"

using namespace Nepomuk2;
using namespace Nepomuk2::Backup;
using namespace Nepomuk2::Vocabulary;
using namespace Soprano::Vocabulary;

StatementGenerator::StatementGenerator(Soprano::Model* model, const QString& inputFile,
                                       const QString& outputFile, QObject* parent)
    : KJob(parent)
    , m_model(model)
    , m_inputFile(inputFile)
    , m_outputFile(outputFile)
    , m_filter(Filter_None)
    , m_statementCount(0)
{
}

void StatementGenerator::start()
{
    QTimer::singleShot( 0, this, SLOT(doJob()) );
}

void StatementGenerator::setFilter(StatementGenerator::Filter filter)
{
    m_filter = filter;
}

void StatementGenerator::doJob()
{
    Soprano::PluginManager* pg = Soprano::PluginManager::instance();
    const Soprano::Serializer* serializer = pg->discoverSerializerForSerialization( Soprano::SerializationNQuads );

    QFile input( m_inputFile );
    kDebug() << "INPUT: " << m_inputFile;
    if( !input.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
        setError(1);
        setErrorText( QString::fromLatin1("Could not open file %1").arg( m_inputFile ) );
        emitResult();
        return;
    }

    QFile output( m_outputFile );
    if( !output.open( QIODevice::ReadWrite | QIODevice::Append | QIODevice::Text ) ) {
        setError(1);
        setErrorText( QString::fromLatin1("Could not open file %1").arg( m_outputFile ) );
        emitResult();
        return;
    }

    QTextStream inputStream( &input );
    QTextStream outputStream( &output );

    int count = 0;
    while( !inputStream.atEnd() ) {
        const QUrl uri( inputStream.readLine() );
        kDebug() << "Processing" << uri;
        QList<Soprano::Statement> stList;

        if( m_filter == Filter_None ) {
            stList << fetchNonDiscardableStatements(uri);
            if( stList.count() ) {
                QStringList properties;
                properties << "nao:lastModified" << "nao:created" << "rdf:type" << "nie:url";

                stList << fetchProperties(uri, properties);
            }
        }
        else if( m_filter == Filter_TagsAndRatings ) {
            if( hasType(uri, NFO::FileDataObject()) ) {
                QList<QString> fileProperties;
                fileProperties << "rdf:type" << "nao:lastModified" << "nao:created"
                               << "nie:url" << "nao:numericRating" << "nao:hasTag" << "nao:description";

                stList << fetchProperties(uri, fileProperties);
            }
            else {
                bool hasPrefLabel = false;
                Soprano::Statement identifierSt;

                QList<Soprano::Statement> tagStatements = fetchAllStatements(uri);
                foreach(const Soprano::Statement& st, tagStatements) {
                    // Do not link to other Nepmouk objects
                    // FIXME: What about the symbols for tags?
                    if( st.object().isResource() && st.object().uri().scheme() == QLatin1String("nepomuk") )
                        continue;

                    if( st.predicate().uri() == NAO::prefLabel() )
                        hasPrefLabel = true;
                    else if( st.predicate().uri() == NAO::identifier() )
                        identifierSt = st;

                    stList << st;
                }

                if( !hasPrefLabel ) {
                    identifierSt.setPredicate( NAO::prefLabel() );
                    stList << identifierSt;
                }
            }
        }

        if( !stList.isEmpty() ) {
            kDebug() << stList;
            Soprano::Util::SimpleStatementIterator iter( stList );
            serializer->serialize( iter, outputStream, Soprano::SerializationNQuads );

            m_statementCount += stList.size();
        }

        if( m_resourceCount ) {
            count++;
            emitPercent( count, m_resourceCount );
        }
    }

    emitResult();
}

namespace {
    /**
     * A very comprehensive validity check
     */
    bool isValid(const Soprano::Statement& st) {
        if( !st.isValid() )
            return false;

        if( !st.subject().isResource() )
            return false;

        if( !st.context().isResource() )
            return false;

        Soprano::Node object = st.object();
        if( !object.isResource() && !object.isLiteral() )
            return false;

        if( object.isLiteral() ) {
            Soprano::LiteralValue lv = object.literal();
            if( lv.dataTypeUri().isEmpty() )
                return false;

            if( lv.isString() && lv.toString().isEmpty() )
                return false;
        }

        return true;
    }
}

bool StatementGenerator::hasType(const QUrl& uri, const QUrl& type)
{
    QString query = QString::fromLatin1("ask where { %1 a %2 . }")
                    .arg( Soprano::Node::resourceToN3(uri),
                          Soprano::Node::resourceToN3(type) );

    return m_model->executeQuery( query, Soprano::Query::QueryLanguageSparql ).boolValue();
}

QList<Soprano::Statement> StatementGenerator::fetchProperties(const QUrl& uri, QStringList properties)
{
    QList<Soprano::Statement> stList;
    QString query = QString::fromLatin1("select distinct ?p ?o ?g where { graph ?g { %1 ?p ?o. } "
                                        " FILTER(?p in (%2)) . }")
                    .arg( Soprano::Node::resourceToN3(uri),
                          properties.join(",") );

    Soprano::QueryResultIterator it = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
    while( it.next() ) {
        Soprano::Statement st( uri, it[0], it[1], it[2] );
        if( isValid(st) )
            stList << st;
    }

    return stList;
}

QList< Soprano::Statement > StatementGenerator::fetchNonDiscardableStatements(const QUrl& uri)
{
    QString query = QString::fromLatin1("select distinct ?p ?o ?g where { graph ?g { %1 ?p ?o. } "
                                        " FILTER(!(?p in (nao:lastModified, nao:created, rdf:type, nie:url))) ."
                                        " ?g a nrl:InstanceBase ."
                                        " FILTER NOT EXISTS { ?g a nrl:DiscardableInstanceBase . } }")
                    .arg( Soprano::Node::resourceToN3(uri) );

    QList<Soprano::Statement> stList;
    Soprano::QueryResultIterator it = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
    while( it.next() ) {
        Soprano::Statement st( uri, it[0], it[1], it[2] );
        if( isValid(st) )
            stList << st;
    }

    return stList;
}

QList< Soprano::Statement > StatementGenerator::fetchAllStatements(const QUrl& uri)
{
    QString query = QString::fromLatin1("select distinct ?p ?o ?g where { graph ?g { %1 ?p ?o. } }")
                    .arg( Soprano::Node::resourceToN3(uri) );

    QList<Soprano::Statement> stList;
    Soprano::QueryResultIterator it = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
    while( it.next() ) {
        Soprano::Statement st( uri, it[0], it[1], it[2] );
        if( isValid(st) )
            stList << st;
    }

    return stList;
}
