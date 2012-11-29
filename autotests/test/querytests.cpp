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
#include "storeresourcesjob.h"
#include "resultiterator.h"

#include <Soprano/LiteralValue>
#include <Soprano/Node>
#include <Soprano/Model>
#include <Soprano/Vocabulary/NAO>
#include <Soprano/Vocabulary/RDFS>
#include <Soprano/Vocabulary/RDF>
#include <Soprano/Vocabulary/XMLSchema>
#include <Soprano/QueryResultIterator>
#include <Soprano/StatementIterator>
#include <Soprano/NodeIterator>

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

    QList<Result> fetchResults( const Query::Query& query ) {
        Query::ResultIterator it( query );
        QList<Result> list;
        while( it.next() )
            list << it.result();

        return list;
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
{
    // Do nothing - we want the data to be preserved across tests
}


void QueryTests::literalTerm_data()
{
    QTest::addColumn< QList<Nepomuk2::Query::Result> >( "actualResults" );
    QTest::addColumn< QList<Nepomuk2::Query::Result> >( "expectedResults" );

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
            << fetchResults( query )
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
            << fetchResults( query )
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
            << fetchResults( query )
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
            << fetchResults( query )
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

}

void QueryTests::literalTerm()
{
    QFETCH( QList<Result>, actualResults );
    QFETCH( QList<Result>, expectedResults );

    QSet<QUrl> actualUris = resultListToUriSet( actualResults );
    QSet<QUrl> expectedUris = resultListToUriSet( expectedResults );

//    kDebug() << "Actual Results: " << actualResults;
//    kDebug() << "Expected Results: " << expectedResults;

    // What about duplicates?
    QCOMPARE( actualUris, expectedUris );
}

void QueryTests::resourceTypeTerm_data()
{
    QTest::addColumn< QList<Nepomuk2::Query::Result> >( "actualResults" );
    QTest::addColumn< QList<Nepomuk2::Query::Result> >( "expectedResults" );

    Test::DataGenerator gen;

    // Inject the data
    for(int i=0; i<10; i++)
        gen.createTag( QString("Tag_") + QString::number(i) );

    // Resource Type Term
    {
        ResourceTypeTerm term( NAO::Tag() );
        Query::Query query( term );

        QSet<QUrl> uris;
        NepomukStatementIterator it( RDF::type() );
        while( it.next() ) {
            if( it.object() == NAO::Tag() ) {
                uris << it.subject().uri();
            }
        }

        QTest::newRow( "type query" )
            << fetchResults( query )
            << toResultList( uris );
    }

    // Negated Resource Type Term
    {
        Query::Query query( NegationTerm::negateTerm( ResourceTypeTerm( NAO::Tag() ) ) );

        QSet<QUrl> uris;
        QSet<QUrl> tags;
        NepomukStatementIterator it( RDF::type() );
        while( it.next() ) {
            uris << it.subject().uri();
            if( it.object() == NAO::Tag() ) {
                tags << it.subject().uri();
            }
        }
        uris.subtract( tags );

        QTest::newRow( "negated type query" )
            << fetchResults( query )
            << toResultList( uris );
    }
}

void QueryTests::resourceTypeTerm()
{
    QEXPECT_FAIL( "negated type query", "Negated Terms are broken - We get graphs in the results", Continue );
    literalTerm();
}

void QueryTests::comparisonTerm_comparators()
{
    literalTerm();
}

void QueryTests::comparisonTerm_comparators_data()
{
    QTest::addColumn< QList<Nepomuk2::Query::Result> >( "actualResults" );
    QTest::addColumn< QList<Nepomuk2::Query::Result> >( "expectedResults" );

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
            << fetchResults( query )
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
            << fetchResults( query )
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
            << fetchResults( query )
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
            << fetchResults( query )
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
            << fetchResults( query )
            << toResultList( uris );
    }
}

void QueryTests::comparisonTerm_data()
{
    QTest::addColumn< QList<Nepomuk2::Query::Result> >( "actualResults" );
    QTest::addColumn< QList<Nepomuk2::Query::Result> >( "expectedResults" );

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
            << fetchResults( query )
            << toResultList( uris );
    }

    // Comparison Term with literal Term - String
    {
        Query::Query query( ComparisonTerm( NMM::performer(), LiteralTerm(artist) ) );

        QSet<QUrl> artists;
        NepomukStatementIterator it2;
        while( it2.next() ) {
            if( it2.hasStringLiteral() && it2.stringLiteral().contains( artist, Qt::CaseInsensitive ) )
                artists << it2.subject().uri();
        }

        QSet<QUrl> uris;
        NepomukStatementIterator it3( NMM::performer() );
        while( it3.next() ) {
            if( artists.contains( it3.object().uri() ) ) {
                uris << it3.subject().uri();
            }
        }

        QTest::newRow( "comparison term with string" )
            << fetchResults( query )
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
            << fetchResults( query )
            << toResultList( uris );
    }

    // Add some more artists
    {
        QLatin1String artist("Floname Rida");
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
        QSet<QUrl> negatedUris;
        NepomukStatementIterator it3;
        while( it3.next() ) {
            uris << it3.subject().uri();
            if( it3.predicate() == NMM::performer() && it3.object().uri() == artistUri ) {
                negatedUris << it3.subject().uri();
            }
        }
        uris.subtract( negatedUris );

        QTest::newRow( "negated comparison term with resource" )
            << fetchResults( query )
            << toResultList( uris );
    }

    // Double Comparison Term
    {
        Query::Query query( ComparisonTerm(NMM::performer(),
                                           ComparisonTerm( NCO::fullname(), LiteralTerm("Floname") )) );

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
            << fetchResults( query )
            << toResultList( uris );
    }
}

void QueryTests::comparisonTerm()
{
    QEXPECT_FAIL( "negated comparison term with resource",
                  "Negated Terms are broken - We get graphs in the results", Continue );
    QEXPECT_FAIL( "comparison term with datetime",
                  "We need to filter out the ontology results", Continue );
    literalTerm();
}

void QueryTests::comparisonTerm_withInvalid_data()
{
    QTest::addColumn< QList<Nepomuk2::Query::Result> >( "actualResults" );
    QTest::addColumn< QList<Nepomuk2::Query::Result> >( "expectedResults" );

    // Add some music
    QUrl whistleSongUri;
    {
        QLatin1String artist("Floname Rida");
        QLatin1String album("Wild Ones");

        Test::DataGenerator gen;
        whistleSongUri = gen.createMusicFile( QLatin1String("Whistle"), artist, album );
        gen.createMusicFile( QLatin1String("Wild Ones"), artist, album );
        gen.createMusicFile( QLatin1String("Let it Roll"), artist, album );
        gen.createMusicFile( QLatin1String("Good Feeling"), artist, album );
        gen.createMusicFile( QLatin1String("Sweet Spot"), artist, album );
    }

    // Comparsion Term with invalid property
    {
        Query::Query query;
        query.setTerm( ComparisonTerm(QUrl(), ResourceTerm(whistleSongUri)) );

        QSet<QUrl> uris;
        NepomukStatementIterator it;
        while( it.next() ) {
            if( it.object().uri() == whistleSongUri )
                uris << it.subject().uri();
        }

        QTest::newRow( "comparsion term with invalid property" )
            << fetchResults( query )
            << toResultList( uris );
    }

    // Comparsion Term with invalid term
    {
        Query::Query query( ComparisonTerm(NMM::performer(), Term()) );

        QSet<QUrl> uris;
        NepomukStatementIterator it( NMM::performer() );
        while( it.next() ) {
            uris << it.subject().uri();
        }

        QTest::newRow( "comparsion term with invalid term" )
            << fetchResults( query )
            << toResultList( uris );
    }

    // Comparsion Term with invalid term and property
    {
        Query::Query query;
        query.setTerm( ComparisonTerm( QUrl(), Term() ) );

        QSet<QUrl> uris;
        NepomukStatementIterator it;
        while( it.next() ) {
            uris << it.subject().uri();
        }

        QTest::newRow( "comparsion term with invalid term and property" )
            << fetchResults( query )
            << toResultList( uris );
    }
}

void QueryTests::comparisonTerm_withInvalid()
{
    // We seem to get ontology results as well. Even though they are expressly forbidden
    QEXPECT_FAIL( "comparsion term with invalid term and property",
                  "Negated Terms are broken - We get graph results and virtuoso is freaking crazy", Continue );
    literalTerm();
}


void QueryTests::invertedComparisonTerm_data()
{
    QTest::addColumn< QList<Nepomuk2::Query::Result> >( "actualResults" );
    QTest::addColumn< QList<Nepomuk2::Query::Result> >( "expectedResults" );

    // Add some music
    QUrl whistleSongUri;
    {
        QLatin1String artist("Floname Rida");
        QLatin1String album("Wild Ones");

        Test::DataGenerator gen;
        whistleSongUri = gen.createMusicFile( QLatin1String("Whistle"), artist, album );
        gen.createMusicFile( QLatin1String("Wild Ones"), artist, album );
        gen.createMusicFile( QLatin1String("Let it Roll"), artist, album );
        gen.createMusicFile( QLatin1String("Good Feeling"), artist, album );
        gen.createMusicFile( QLatin1String("Sweet Spot"), artist, album );
    }

    // property inversion with term
    {
        Query::Query query( ComparisonTerm(NMM::performer(), ResourceTerm(whistleSongUri)).inverted() );

        QSet<QUrl> uris;
        NepomukStatementIterator it( NMM::performer() );
        while( it.next() ) {
            if( it.subject().uri() == whistleSongUri )
                uris << it.object().uri();
        }


        QTest::newRow( "property inversion with term" )
            << fetchResults( query )
            << toResultList( uris );
    }

    // property inversion without term
    {
        Query::Query query( ComparisonTerm(NMM::performer(), Term()).inverted() );

        QSet<QUrl> uris;
        NepomukStatementIterator it( NMM::performer() );
        while( it.next() ) {
            uris << it.object().uri();
        }

        QTest::newRow( "property inversion without term" )
            << fetchResults( query )
            << toResultList( uris );
    }
}

void QueryTests::invertedComparisonTerm()
{
    literalTerm();
}

void QueryTests::optionalTerm()
{
    literalTerm();
}

void QueryTests::optionalTerm_data()
{
    QTest::addColumn< QList<Nepomuk2::Query::Result> >( "actualResults" );
    QTest::addColumn< QList<Nepomuk2::Query::Result> >( "expectedResults" );

    QString mainArtist;
    QString mainAlbum;

    // Add some music
    {
        QLatin1String artist("Floname Rida");
        QLatin1String album("Wild Ones");

        Test::DataGenerator gen;
        gen.createMusicFile( QLatin1String("Whistle"), artist, album );
        gen.createMusicFile( QLatin1String("Wild Ones"), artist, album );
        gen.createMusicFile( QLatin1String("Let it Roll"), artist, album );
        gen.createMusicFile( QLatin1String("Good Feeling"), artist, album );
        gen.createMusicFile( QLatin1String("Sweet Spot"), artist, album );

        QLatin1String artist2("Outlandish");
        QLatin1String album2("Bread & Barrels of Water");

        gen.createMusicFile( QLatin1String("Peelo"), artist2, album2 );
        gen.createMusicFile( QLatin1String("Walou"), artist2, album2 );
        gen.createMusicFile( QLatin1String("Aicha"), artist2, album2 );
        gen.createMusicFile( QLatin1String("If Only"), artist2, album2 );

        gen.createMusicFile( QLatin1String("Random Song"), artist, album2 );

        mainAlbum = album2;
        mainArtist = artist;
    }

    QUrl albumUri;
    QUrl artistUri;
    {
        Soprano::Model* model = ResourceManager::instance()->mainModel();
        QList< Soprano::Node > nodes = model->listStatements( QUrl(), NIE::title(), Soprano::LiteralValue(mainAlbum) ).iterateSubjects().allNodes();

        QCOMPARE( nodes.size(), 1 );
        albumUri = nodes.first().uri();

        nodes = model->listStatements( QUrl(), NCO::fullname(), Soprano::LiteralValue(mainArtist) ).iterateSubjects().allNodes();
        QCOMPARE( nodes.size(), 1 );
        artistUri = nodes.first().uri();
    }

    {
        Query::ComparisonTerm ct1( NMM::performer(), ResourceTerm(artistUri) );
        Query::ComparisonTerm ct2( NMM::musicAlbum(), ResourceTerm(albumUri) );
        Query::Query query( AndTerm( ct1,
                                     OptionalTerm::optionalizeTerm( ct2 ) ) );

        QSet<QUrl> uris;
        NepomukStatementIterator it( NMM::performer() );
        while( it.next() ) {
            if( it.object().uri() == artistUri )
                uris << it.subject().uri();
        }

        NepomukStatementIterator it2( NMM::musicAlbum() );
        while( it.next() ) {
            if( it2.object().uri() == albumUri )
                uris << it.subject().uri();
        }

        QTest::newRow( "optional term" )
            << fetchResults( query )
            << toResultList( uris );
    }
}

void QueryTests::variableNames_data()
{
    QTest::addColumn< QList<QUrl> >( "actualResults" );
    QTest::addColumn< QList<QUrl> >( "expectedResults" );

    // Add some music
    {
        QLatin1String artist("Floname Rida");
        QLatin1String album("Wild Ones");

        Test::DataGenerator gen;
        gen.createMusicFile( QLatin1String("Whistle"), artist, album );
        gen.createMusicFile( QLatin1String("Wild Ones"), artist, album );
        gen.createMusicFile( QLatin1String("Let it Roll"), artist, album );
        gen.createMusicFile( QLatin1String("Good Feeling"), artist, album );
        gen.createMusicFile( QLatin1String("Sweet Spot"), artist, album );
    }

    {
        QString varName("artist");

        Query::ComparisonTerm ct1( NMM::performer(), Term() );
        ct1.setVariableName( varName );

        Query::Query query( ct1 );

        QSet<QUrl> uris;
        NepomukStatementIterator it( NMM::performer() );
        while( it.next() ) {
            uris << it.object().uri();
        }

        QList<Query::Result> queryResults = fetchResults( query );
        QList<QUrl> actualResults;
        foreach( const Query::Result& res, queryResults ) {
            actualResults << res.additionalBinding( varName ).toUrl();
        }

        QTest::newRow( "variable names" )
            << actualResults
            << uris.toList();
    }
}

void QueryTests::variableNames()
{
    QFETCH( QList<QUrl>, actualResults );
    QFETCH( QList<QUrl>, expectedResults );

    QSet<QUrl> actualUris = actualResults.toSet();
    QSet<QUrl> expectedUris = expectedResults.toSet();

    //kDebug() << "Actual Results: " << actualResults;
    //kDebug() << "Expected Results: " << expectedResults;

    // What about duplicates?
    QCOMPARE( actualUris, expectedUris );
}


void QueryTests::orderByTerm_data()
{
    resetRepository();

    QTest::addColumn< QList<QUrl> >( "actualResults" );
    QTest::addColumn< QList<QUrl> >( "expectedResults" );

    // Generate a bunch of text files with numeric ratings
    QList<QUrl> expectedResults;

    Test::DataGenerator gen;

    QUrl res1 = gen.createPlainTextFile( QLatin1String("ABC") );
    QUrl res2 = gen.createPlainTextFile( QLatin1String("ABCD") );
    QUrl res3 = gen.createPlainTextFile( QLatin1String("ABCE") );
    QUrl res4 = gen.createPlainTextFile( QLatin1String("ABD") );
    QUrl res5 = gen.createPlainTextFile( QLatin1String("AB") );
    QUrl res6 = gen.createPlainTextFile( QLatin1String("A") );
    QUrl res7 = gen.createPlainTextFile( QLatin1String("NO MATTER") );

    SimpleResourceGraph graph;
    graph.addStatement( res1, NAO::numericRating(), Soprano::LiteralValue(1) );
    graph.addStatement( res2, NAO::numericRating(), Soprano::LiteralValue(2) );
    graph.addStatement( res3, NAO::numericRating(), Soprano::LiteralValue(3) );
    graph.addStatement( res4, NAO::numericRating(), Soprano::LiteralValue(3) );
    graph.addStatement( res5, NAO::numericRating(), Soprano::LiteralValue(2) );
    graph.addStatement( res6, NAO::numericRating(), Soprano::LiteralValue(1) );
    graph.addStatement( res7, NAO::numericRating(), Soprano::LiteralValue(6) );

    KJob* job = graph.save();
    job->exec();
    QVERIFY( !job->error() );

    {
        Query::ComparisonTerm ct1( NAO::numericRating(), LiteralTerm(4), ComparisonTerm::Smaller );
        Query::ComparisonTerm ct2( NIE::plainTextContent(), Term() );

        ct1.setSortWeight( 10, Qt::DescendingOrder );
        ct2.setSortWeight( 2, Qt::AscendingOrder );

        Query::Query query( AndTerm( ct1, ct2 ) );
        kDebug() << query.toSparqlQuery();

        QList<Query::Result> queryResults = fetchResults( query );
        QList<QUrl> actualResults;
        foreach( const Query::Result& res, queryResults ) {
            actualResults << res.resource().uri();
        }

        // The correct order will be -
        expectedResults << res3 << res4;
        expectedResults << res5 << res2;
        expectedResults << res6 << res1;

        QTest::newRow( "order by" )
            << actualResults
            << expectedResults;
    }

}

void QueryTests::orderByTerm()
{
    QFETCH( QList<QUrl>, actualResults );
    QFETCH( QList<QUrl>, expectedResults );

    QCOMPARE( actualResults, expectedResults );
}


void QueryTests::fileQueries()
{
    literalTerm();
}

void QueryTests::fileQueries_data()
{
    QTest::addColumn< QList<Nepomuk2::Query::Result> >( "actualResults" );
    QTest::addColumn< QList<Nepomuk2::Query::Result> >( "expectedResults" );

    // Create the data
    // includeFolder/
    // includeFolder/files
    // includeFolder/excludeFolder/
    // includeFolder/excludeFolder/files
    KTempDir dir;
    dir.setAutoRemove( false );

    QString includeDir( dir.name() + "includeFolder/" );
    QString excludeDir( includeDir + "excludeFolder/" );
    SimpleResourceGraph graph;
    graph += Test::DataGenerator::createFolder( QUrl::fromLocalFile(includeDir) );
    for( int i=0; i<10; i++ ) {
        QString fileName( "abc" + QString::number(i) );
        QUrl url = QUrl::fromLocalFile( includeDir + fileName );

        graph += Test::DataGenerator::createPlainTextFile( url, QString::number(i) );
    }

    graph += Test::DataGenerator::createFolder( QUrl::fromLocalFile(excludeDir) );
    for( int i=0; i<10; i++ ) {
        QString fileName( "fgh" + QString::number(i) );
        QUrl url = QUrl::fromLocalFile( excludeDir + fileName );
        graph += Test::DataGenerator::createPlainTextFile( url, QString::number(i*10) );
    }

    KJob* job = graph.save();
    job->exec();
    if( job->error() ) {
        kError() << job->errorString();
        QVERIFY(!job->error());
    }

    // Empty FileQuery - Should query for all files
    {
        FileQuery query;

        QSet<QUrl> uris;
        NepomukStatementIterator it( NIE::url() );
        while( it.next() ) {
            if( it.object().uri().isLocalFile() ) {
                uris << it.subject().uri();
            }
        }

        QTest::newRow( "empty file query" )
            << fetchResults( query )
            << toResultList( uris );
    }

    // Query files with tags
    {
/*    FileQuery fileQuery( ComparisonTerm( Soprano::Vocabulary::NAO::hasTag(), ResourceTerm(QUrl("nepomuk:/res/foobar")) ) );
    QTest::newRow( "file query" )
        << Query(fileQuery)
        << QString::fromLatin1("select distinct ?r where { { ?r %1 <nepomuk:/res/foobar> . ?r a ?v1 . FILTER(?v1 in (%2,%3)) . } . }")
        .arg( Soprano::Node::resourceToN3(Soprano::Vocabulary::NAO::hasTag()),
              Soprano::Node::resourceToN3(Nepomuk2::Vocabulary::NFO::Folder()),
              Soprano::Node::resourceToN3(Nepomuk2::Vocabulary::NFO::FileDataObject()) );
*/
    }


    // Query files
    {
        FileQuery query;
        query.setFileMode( FileQuery::QueryFiles );

        QSet<QUrl> uris;
        QSet<QUrl> discards;
        NepomukStatementIterator it( NIE::url() );
        while( it.next() ) {
            const QUrl uri = it.subject().uri();

            Soprano::Model* model = ResourceManager::instance()->mainModel();
            if( model->containsAnyStatement(uri, RDF::type(), NFO::FileDataObject()) )
                uris << uri;

            if( model->containsAnyStatement(uri, RDF::type(), NFO::Folder()) )
                discards << uri;
        }
        uris.subtract( discards );

        QTest::newRow( "file query (only files)" )
            << fetchResults( query )
            << toResultList( uris );
    }

    // Query Folders
    {
        FileQuery query;
        query.setFileMode( FileQuery::QueryFolders );
        kDebug() << query.toSparqlQuery();

        QSet<QUrl> uris;
        NepomukStatementIterator it( NIE::url() );
        while( it.next() ) {
            const QUrl uri = it.subject().uri();

            Soprano::Model* model = ResourceManager::instance()->mainModel();
            if( model->containsAnyStatement(uri, RDF::type(), NFO::Folder()) )
                uris << it.subject().uri();
        }

        QTest::newRow( "file query (only folders)" )
            << fetchResults( query )
            << toResultList( uris );
    }

    // Include Exclude Folder
    {
        FileQuery query;
        query.addIncludeFolder( QUrl::fromLocalFile(includeDir) );
        query.addExcludeFolder( QUrl::fromLocalFile(excludeDir) );

        QSet<QUrl> uris;
        NepomukStatementIterator it( NIE::url() );
        while( it.next() ) {
            const QUrl url = it.object().uri();
            const QString urlString = url.toLocalFile();

            if( !urlString.startsWith(excludeDir) && urlString.startsWith(includeDir) ) {
                uris << it.subject().uri();
            }
        }

        QTest::newRow( "file query with include folder" )
            << fetchResults( query )
            << toResultList( uris );
    }
}



}

QTEST_KDEMAIN(Nepomuk2::QueryTests, NoGUI)
