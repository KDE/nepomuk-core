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
#include "nmm.h"
#include "pimo.h"
#include "property.h"
#include "variant.h"
#include "resource.h"
#include "tag.h"
#include "resourcemanager.h"
#include "result.h"
#include "dbusoperators_p.h"
#include "queryserviceclient.h"
#include "storeresourcesjob.h"

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

    template <typename T>
    QList<Result> toResultList(const T& container) {
        QList<Result> results;
        foreach(const QUrl& uri, container)
            results << Result( uri );
        return results;
    }

    class NepomukStatementIterator {
    public:
        NepomukStatementIterator(const QUrl& property = QUrl()) {
            QString propertyFilter;
            if( !property.isEmpty() ) {
                propertyFilter = QString("FILTER(?p=%1)").arg( Soprano::Node::resourceToN3(property) );
            }

            QString query = QString::fromLatin1("select ?r ?p ?o where { ?r ?p ?o. "
                                                "FILTER(REGEX(STR(?r), '^nepomuk:/res')) . %1 }")
                            .arg( propertyFilter );
            Soprano::Model* model = ResourceManager::instance()->mainModel();
            m_it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
        }

        Soprano::Statement current() const { return m_current; }
        bool next() {
            if( !m_it.next() )
                return false;

            m_current.setSubject( m_it["r"] );
            m_current.setPredicate( m_it["p"] );
            m_current.setObject( m_it["o"] );

            return true;
        }

        Soprano::Node subject() const { return m_current.subject(); }
        Soprano::Node predicate() const { return m_current.predicate(); }
        Soprano::Node object() const { return m_current.object(); }

        bool hasStringLiteral() const {
            return object().isLiteral() && object().literal().isString();
        }

        QString stringLiteral() const {
            return object().literal().toString();
        }
    private:
        Soprano::QueryResultIterator m_it;
        Soprano::Statement m_current;
    };
}

void QueryTests::cleanup()
{ // Do nothing - we want the data to be preserved across tests
}


void QueryTests::literalTerm_data()
{
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
        NepomukStatementIterator it;
        while( it.next() ) {
            if( it.hasStringLiteral() ) {
                QString str = it.stringLiteral();
                if( str.contains("Hello") )
                    uris << it.subject().uri();
            }
        }

        QTest::newRow( "simple literal query" )
            << query
            << toResultList( uris );
    }

    // Words with spaces
    {
        Query::Query query( LiteralTerm( "Hello World" ) );

        QSet<QUrl> uris;
        NepomukStatementIterator it;
        while( it.next() ) {
            if( it.hasStringLiteral() ) {
                QString str = it.stringLiteral();
                if( str.contains("Hello") && str.contains("World") )
                    uris << it.subject().uri();
            }
        }

        QTest::newRow( "simple literal query with space" )
            << query
            << toResultList( uris );
    }

    // Simple literal query with spaces and double quotes
    {
        Query::Query query( LiteralTerm( "'Hello World'" ) );

        QSet<QUrl> uris;
        NepomukStatementIterator it;
        while( it.next() ) {
            if( it.hasStringLiteral() ) {
                QString str = it.stringLiteral();
                if( str.contains("Hello World") )
                    uris << it.subject().uri();
            }
        }

        QTest::newRow( "simple literal query with space and double quotes" )
            << query
            << toResultList( uris );
    }

    // Simple literal query with wildcards
    {
        Query::Query query( LiteralTerm( "Hell*" ) );

        QSet<QUrl> uris;
        NepomukStatementIterator it;
        while( it.next() ) {
            if( it.hasStringLiteral() ) {
                QString str = it.stringLiteral();
                QRegExp regex("Hell*", Qt::CaseInsensitive, QRegExp::Wildcard);

                if( str.contains(regex) )
                    uris << it.subject().uri();
            }
        }

        QTest::newRow( "simple literal wildcards1" )
            << query
            << toResultList( uris );
    }




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

void QueryTests::resourceTypeTerm_data()
{
    QTest::addColumn< Nepomuk2::Query::Query >( "query" );
    QTest::addColumn< QList<Query::Result> >( "results" );

    Test::DataGenerator gen;

    // Inject the data
    for(int i=0; i<10; i++)
        gen.createTag( QString("Tag_") + QString::number(i) );

    // Resource Type Term
    {
        Query::Query query( ResourceTypeTerm( NAO::Tag() ) );

        QSet<QUrl> uris;
        NepomukStatementIterator it( RDF::type() );
        while( it.next() ) {
            if( it.object() == NAO::Tag() ) {
                uris << it.subject().uri();
            }
        }

        QTest::newRow( "type query" )
            << query
            << toResultList( uris );
    }

    // Negated Resource Type Term
    {
        Query::Query query( NegationTerm::negateTerm( ResourceTypeTerm( NAO::Tag() ) ) );

        QSet<QUrl> uris;
        NepomukStatementIterator it( RDF::type() );
        while( it.next() ) {
            if( it.object() != NAO::Tag() ) {
                uris << it.subject().uri();
            }
        }

        QTest::newRow( "negated type query" )
            << query
            << toResultList( uris );
    }
}

void QueryTests::resourceTypeTerm()
{
    literalTerm();
}

void QueryTests::comparisonTerm_comparators()
{
    literalTerm();
}

void QueryTests::comparisonTerm_comparators_data()
{
    QTest::addColumn< Nepomuk2::Query::Query >( "query" );
    QTest::addColumn< QList<Query::Result> >( "results" );

    //
    // comparators < <= > >= ==
    //

    // Inject data - Files with ratings
    {
        Test::DataGenerator gen;
        SimpleResourceGraph graph;
        for( int i=0; i<=10; i++ ) {
            const QUrl uri = gen.createPlainTextFile( QString("File Content") + QString::number(i) );

            SimpleResource res( uri );
            res.setProperty( NAO::numericRating(), i );
            graph << res;
        }

        KJob* job = graph.save();
        job->exec();
        QVERIFY( !job->error() );
    }

    // <
    {
        Query::Query query( ComparisonTerm( NAO::numericRating(), LiteralTerm(4), ComparisonTerm::Smaller ) );

        QSet<QUrl> uris;
        NepomukStatementIterator it( NAO::numericRating() );
        while( it.next() ) {
            if( it.object().isLiteral() && it.object().literal().isInt() ) {
                int rating = it.object().literal().toInt();
                if( rating < 4 )
                    uris << it.subject().uri();
            }
        }

        QTest::newRow( "comparsion term <" )
            << query
            << toResultList( uris );
    }

    // <=
    {
        Query::Query query( ComparisonTerm( NAO::numericRating(), LiteralTerm(4), ComparisonTerm::SmallerOrEqual ) );

        QSet<QUrl> uris;
        NepomukStatementIterator it( NAO::numericRating() );
        while( it.next() ) {
            if( it.object().isLiteral() && it.object().literal().isInt() ) {
                int rating = it.object().literal().toInt();
                if( rating <= 4 )
                    uris << it.subject().uri();
            }
        }

        QTest::newRow( "comparsion term <=" )
            << query
            << toResultList( uris );
    }


    // >
    {
        Query::Query query( ComparisonTerm( NAO::numericRating(), LiteralTerm(4), ComparisonTerm::Greater ) );

        QSet<QUrl> uris;
        NepomukStatementIterator it( NAO::numericRating() );
        while( it.next() ) {
            if( it.object().isLiteral() && it.object().literal().isInt() ) {
                int rating = it.object().literal().toInt();
                if( rating > 4 )
                    uris << it.subject().uri();
            }
        }

        QTest::newRow( "comparsion term >" )
            << query
            << toResultList( uris );
    }

    // >=
    {
        Query::Query query( ComparisonTerm( NAO::numericRating(), LiteralTerm(4), ComparisonTerm::GreaterOrEqual ) );

        QSet<QUrl> uris;
        NepomukStatementIterator it( NAO::numericRating() );
        while( it.next() ) {
            if( it.object().isLiteral() && it.object().literal().isInt() ) {
                int rating = it.object().literal().toInt();
                if( rating >= 4 )
                    uris << it.subject().uri();
            }
        }

        QTest::newRow( "comparsion term >=" )
            << query
            << toResultList( uris );
    }

    // ==
    {
        Query::Query query( ComparisonTerm( NAO::numericRating(), LiteralTerm(4), ComparisonTerm::Equal ) );

        QSet<QUrl> uris;
        NepomukStatementIterator it( NAO::numericRating() );
        while( it.next() ) {
            if( it.object().isLiteral() && it.object().literal().isInt() ) {
                int rating = it.object().literal().toInt();
                if( rating == 4 )
                    uris << it.subject().uri();
            }
        }

        QTest::newRow( "comparsion term ==" )
            << query
            << toResultList( uris );
    }
}

void QueryTests::comparisonTerm_data()
{
    QTest::addColumn< Nepomuk2::Query::Query >( "query" );
    QTest::addColumn< QList<Query::Result> >( "results" );

    // Inject some data
    Test::DataGenerator gen;

    QString artist("Coldplay");
    QString album("X&Y");
    for(int i=0; i<10; i++)
        gen.createMusicFile( QString("Title_") + QString::number(i), artist, album );

    // Comparison Term with literal Term - Date time
    {
        QDateTime now = QDateTime::currentDateTime();
        Query::Query query( ComparisonTerm( NAO::lastModified(), LiteralTerm(now),
                                            ComparisonTerm::SmallerOrEqual ) );

        QSet<QUrl> uris;
        NepomukStatementIterator it( NAO::lastModified() );
        while( it.next() ) {
            if( it.object().isLiteral() && it.object().literal().isDateTime() ) {
                QDateTime dt = it.object().literal().toDateTime();
                if( dt <= now )
                    uris << it.subject().uri();
            }
        }

        QTest::newRow( "comparison term with datetime" )
            << query
            << toResultList( uris );
    }

    // Comparison Term with literal Term - String
    {
        Query::Query query( ComparisonTerm( NMM::performer(), LiteralTerm(artist) ) );

        QSet<QUrl> contacts;
        NepomukStatementIterator it( RDF::type() );
        while( it.next() ) {
            if( it.object() == NCO::Contact() ) {
                contacts << it.subject().uri();
            }
        }

        QSet<QUrl> artists;
        NepomukStatementIterator it2;
        while( it2.next() ) {
            if( contacts.contains(it2.subject().uri()) ) {
                if( it2.hasStringLiteral() && it2.stringLiteral().contains( artist, Qt::CaseInsensitive ) )
                    artists << it2.subject().uri();
            }
        }

        QSet<QUrl> uris;
        NepomukStatementIterator it3( NMM::performer() );
        while( it3.next() ) {
            if( artists.contains( it3.object().uri() ) ) {
                uris << it3.subject().uri();
            }
        }

        QTest::newRow( "comparison term with string" )
            << query
            << toResultList( uris );
    }

    // ComparisonTerm with resource
    {
        QSet<QUrl> contacts;
        NepomukStatementIterator it( RDF::type() );
        while( it.next() ) {
            if( it.object() == NCO::Contact() ) {
                contacts << it.subject().uri();
            }
        }

        QSet<QUrl> artists;
        NepomukStatementIterator it2;
        while( it2.next() ) {
            if( contacts.contains(it2.subject().uri()) ) {
                if( it2.hasStringLiteral() && it2.stringLiteral() == artist )
                    artists << it2.subject().uri();
            }
        }

        QCOMPARE( artists.size(), 1 );
        const QUrl artistUri = *artists.begin();
        Query::Query query( ComparisonTerm( NMM::performer(), ResourceTerm(artistUri) ) );


        QSet<QUrl> uris;
        NepomukStatementIterator it3( NMM::performer() );
        while( it3.next() ) {
            if( artists.contains( it3.object().uri() ) ) {
                uris << it3.subject().uri();
            }
        }

        QTest::newRow( "comparison term with resource" )
            << query
            << toResultList( uris );
    }

    // Add some more artists
    {
        QLatin1String artist("Flo Rida");
        QLatin1String album("Wild Ones");

        gen.createMusicFile( QLatin1String("Whistle"), artist, album );
        gen.createMusicFile( QLatin1String("Wild Ones"), artist, album );
        gen.createMusicFile( QLatin1String("Let it Roll"), artist, album );
        gen.createMusicFile( QLatin1String("Good Feeling"), artist, album );
        gen.createMusicFile( QLatin1String("Sweet Spot"), artist, album );
    }

    // Negation ComparisonTerm with resource
    {
        QSet<QUrl> contacts;
        NepomukStatementIterator it( RDF::type() );
        while( it.next() ) {
            if( it.object() == NCO::Contact() ) {
                contacts << it.subject().uri();
            }
        }

        QSet<QUrl> artists;
        NepomukStatementIterator it2;
        while( it2.next() ) {
            if( contacts.contains(it2.subject().uri()) ) {
                if( it2.hasStringLiteral() && it2.stringLiteral() == artist )
                    artists << it2.subject().uri();
            }
        }

        const QUrl artistUri = *artists.begin();
        Query::Query query( NegationTerm::negateTerm( ComparisonTerm( NMM::performer(), ResourceTerm(artistUri) ) ) );

        QSet<QUrl> uris;
        NepomukStatementIterator it3;
        while( it3.next() ) {
            if( it3.predicate() != NMM::performer() || it3.object().uri() != artistUri ) {
                uris << it3.subject().uri();
            }
        }

        QTest::newRow( "negated comparison term with resource" )
            << query
            << toResultList( uris );
    }

    // Double Comparison Term
    {
        Query::Query query( ComparisonTerm(NMM::performer(),
                                           ComparisonTerm( NCO::fullname(), LiteralTerm("Flo") )) );

        QSet<QUrl> contacts;
        NepomukStatementIterator it( NCO::fullname() );
        while( it.next() ) {
            if( it.hasStringLiteral() ) {
                const QString str = it.stringLiteral();
                if( str.contains("Flo", Qt::CaseInsensitive) )
                    contacts << it.subject().uri();
            }
        }

        QSet<QUrl> uris;
        NepomukStatementIterator it2( NMM::performer() );
        while( it2.next() ) {
            if( contacts.contains(it2.object().uri()) ) {
                uris << it2.subject().uri();
            }
        }

        QTest::newRow( "double comparsion term" )
            << query
            << toResultList( uris );
    }
}

void QueryTests::comparisonTerm()
{
    literalTerm();
}





}

QTEST_KDEMAIN(Nepomuk2::QueryTests, NoGUI)