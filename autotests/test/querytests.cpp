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


#include "querytests.h"
#include "../lib/datagenerator.h"

#include "query.h"
#include "filequery.h"
#include "literalterm.h"
#include "resourceterm.h"
#include "andterm.h"
#include "orterm.h"
#include "negationterm.h"
#include "comparisonterm.h"
#include "resourcetypeterm.h"
#include "optionalterm.h"
#include "nie.h"
#include "nfo.h"
#include "nco.h"
#include "pimo.h"
#include "property.h"
#include "variant.h"
#include "resource.h"
#include "resourcemanager.h"
#include "result.h"
#include "dbusoperators_p.h"
#include "queryserviceclient.h"

#include <Soprano/LiteralValue>
#include <Soprano/Node>
#include <Soprano/Model>
#include <Soprano/Vocabulary/NAO>
#include <Soprano/Vocabulary/RDFS>
#include <Soprano/Vocabulary/RDF>
#include <Soprano/Vocabulary/XMLSchema>
#include <Soprano/QueryResultIterator>
#include <Soprano/StatementIterator>

#include <KDebug>
#include <KTemporaryFile>
#include <KJob>
#include <KTempDir>
#include <qtest_kde.h>

using namespace Nepomuk2::Query;
using namespace Nepomuk2::Vocabulary;
using namespace Soprano::Vocabulary;

Q_DECLARE_METATYPE(Nepomuk2::Query::Query)

QDebug operator<<(QDebug debug, const Result & res) {
    debug << "(" << res.resource().uri() << ", " << res.score() << ")";
    return debug;
}

namespace Nepomuk2 {

namespace {
    QList<Result> fetchResults(const QString& query) {
        Soprano::Model* model = ResourceManager::instance()->mainModel();
        Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );

        QList<Result> results;
        while( it.next() ) {
            const QUrl resUri = it[0].uri();
            results << Result( resUri );
        }

        return results;
    }

    QList<Result> setToResultList(const QSet<QUrl>& uriSet) {
        QList<Result> results;
        foreach(const QUrl& uri, uriSet)
            results << Result( uri );
        return results;
    }
}

void QueryTests::cleanup()
{
    // Do nothing - we want the data to be preserved across tests
}


void QueryTests::literalTerm_data()
{
    Soprano::Model* model = ResourceManager::instance()->mainModel();

    QTest::addColumn< Nepomuk2::Query::Query >( "query" );
    QTest::addColumn< QList<Query::Result> >( "results" );

    // Create some test data
    Test::DataGenerator gen;
    gen.createPlainTextFile("Hello");
    gen.createPlainTextFile("Hello World");
    gen.createPlainTextFile("Hellfire");
    gen.createPlainTextFile("hellfire");

    // Simple word - Hello
    {
        Query::Query query( LiteralTerm( "Hello" ) );

        QSet<QUrl> uris;
        Soprano::StatementIterator it = model->listStatements();
        while( it.next() ) {
            const Soprano::Statement& st = it.current();
            if( st.subject().uri().scheme() != QLatin1String("nepomuk") )
                continue;

            if( st.object().isLiteral() ) {
                Soprano::LiteralValue lv = st.object().literal();
                if( lv.isString() ) {
                    if( lv.toString().contains("Hello") )
                        uris << st.subject().uri();
                }
            }
        }

        QTest::newRow( "simple literal query" )
            << query
            << setToResultList( uris );
    }

    // Words with spaces
    {
        Query::Query query( LiteralTerm( "Hello World" ) );

        QSet<QUrl> uris;
        Soprano::StatementIterator it = model->listStatements();
        while( it.next() ) {
            const Soprano::Statement& st = it.current();
            if( st.subject().uri().scheme() != QLatin1String("nepomuk") )
                continue;

            if( st.object().isLiteral() ) {
                Soprano::LiteralValue lv = st.object().literal();
                if( lv.isString() ) {
                    QString str = lv.toString();
                    if( str.contains("Hello") && str.contains("World") )
                        uris << st.subject().uri();
                }
            }
        }

        QTest::newRow( "simple literal query with space" )
            << query
            << setToResultList( uris );
    }

    // Simple literal query with spaces and double quotes
    {
        Query::Query query( LiteralTerm( "'Hello World'" ) );

        QSet<QUrl> uris;
        Soprano::StatementIterator it = model->listStatements();
        while( it.next() ) {
            const Soprano::Statement& st = it.current();
            if( st.subject().uri().scheme() != QLatin1String("nepomuk") )
                continue;

            if( st.object().isLiteral() ) {
                Soprano::LiteralValue lv = st.object().literal();
                if( lv.isString() ) {
                    QString str = lv.toString();
                    if( str.contains("Hello World") )
                        uris << st.subject().uri();
                }
            }
        }

        QTest::newRow( "simple literal query with space and double quotes" )
            << query
            << setToResultList( uris );
    }

    // Simple literal query with wildcards
    {
        Query::Query query( LiteralTerm( "Hell*" ) );

        QSet<QUrl> uris;
        Soprano::StatementIterator it = model->listStatements();
        while( it.next() ) {
            const Soprano::Statement& st = it.current();
            if( st.subject().uri().scheme() != QLatin1String("nepomuk") )
                continue;

            if( st.object().isLiteral() ) {
                Soprano::LiteralValue lv = st.object().literal();
                if( lv.isString() ) {
                    QString str = lv.toString();
                    QRegExp regex("Hell*", Qt::CaseInsensitive, QRegExp::Wildcard);

                    if( str.contains(regex) )
                        uris << st.subject().uri();
                }
            }
        }

        QTest::newRow( "simple literal wildcards1" )
            << query
            << setToResultList( uris );
    }

    // hasTag with literalTerm
    // hasTag with resource
    // hasTag with negated resource

    // comparators < <= > >= ==

    // optional term

    // set variable name term

    // order by term
}


namespace {
    QSet<QUrl> resultListToUriSet( const QList<Result>& results ) {
        QSet<QUrl> urls;
        foreach( const Result& r, results )
            urls << r.resource().uri();
        return urls;
    }

    QList<Result> fetchResults( const Query::Query& query ) {
        QueryServiceClient client;
        QEventLoop loop;
        QObject::connect( &client, SIGNAL(finishedListing()), &loop, SLOT(quit()) );
        QSignalSpy spy( &client, SIGNAL(newEntries(QList<Nepomuk2::Query::Result>)) );
        client.query( query );
        loop.exec();

        QList<Result> list;
        while( spy.count() ) {
            list += spy.takeFirst().first().value< QList<Result> >();
        }

        return list;
    }
}

void QueryTests::literalTerm()
{
    QFETCH( Query::Query, query );
    QFETCH( QList<Result>, results );

    kDebug() << query.toSparqlQuery();
    QList<Result> actualResults = fetchResults( query );

    QSet<QUrl> actualUris = resultListToUriSet( actualResults );
    QSet<QUrl> expectedUris = resultListToUriSet( results );

    kDebug() << "Actual Results: " << actualResults;
    kDebug() << "Expected Results: " << results;

    // What about duplicates?
    QCOMPARE( actualUris, expectedUris );
}

}

QTEST_KDEMAIN(Nepomuk2::QueryTests, NoGUI)