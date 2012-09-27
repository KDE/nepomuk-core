/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2009-2010 Sebastian Trueg <trueg@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) version 3, or any
   later version accepted by the membership of KDE e.V. (or its
   successor approved by the membership of KDE e.V.), which shall
   act as a proxy defined in Section 6 of version 3 of the license.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "querylibtest.h"
#include "qtest_querytostring.h"

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

#include <QtTest>

#include <Soprano/LiteralValue>
#include <Soprano/Node>
#include <Soprano/Vocabulary/NAO>
#include <Soprano/Vocabulary/RDFS>
#include <Soprano/Vocabulary/RDF>
#include <Soprano/Vocabulary/XMLSchema>

#include <kdebug.h>
#include <qtest_kde.h>

Q_DECLARE_METATYPE( Nepomuk2::Query::Query )

using namespace Nepomuk2::Query;
using namespace Nepomuk2::Vocabulary;

void QueryLibTest::testOptimization()
{
    LiteralTerm literal("Hello World");
    AndTerm and1;
    and1.addSubTerm(literal);
    QCOMPARE( Query(and1).optimized(), Query(literal) );

    AndTerm and2;
    and2.addSubTerm(and1);
    QCOMPARE( Query(and2).optimized(), Query(literal) );

    Term invalidTerm;
    and2.addSubTerm(invalidTerm);
    QCOMPARE( Query(and2).optimized(), Query(literal) );

    and1.setSubTerms(QList<Term>() << invalidTerm);
    and2.setSubTerms(QList<Term>() << and1 << literal);
    QCOMPARE( Query(and2).optimized(), Query(literal) );

    // make sure duplicate negations are removed
    QCOMPARE( Query(
                  NegationTerm::negateTerm(
                      NegationTerm::negateTerm(
                          ResourceTypeTerm(Soprano::Vocabulary::NAO::Tag())))
                  ).optimized(),
              Query( ResourceTypeTerm(Soprano::Vocabulary::NAO::Tag()) ) );

    // make sure more than two negations are removed
    QCOMPARE( Query(
                  NegationTerm::negateTerm(
                      NegationTerm::negateTerm(
                          NegationTerm::negateTerm(
                              NegationTerm::negateTerm(
                                  ResourceTypeTerm(Soprano::Vocabulary::NAO::Tag())))))
                  ).optimized(),
              Query( ResourceTypeTerm(Soprano::Vocabulary::NAO::Tag()) ) );

    // test negation removal while keeping one
    QCOMPARE( Query(
                  NegationTerm::negateTerm(
                      NegationTerm::negateTerm(
                          NegationTerm::negateTerm(
                              ResourceTypeTerm(Soprano::Vocabulary::NAO::Tag()))))
                  ).optimized(),
              Query( NegationTerm::negateTerm(ResourceTypeTerm(Soprano::Vocabulary::NAO::Tag())) ) );


    // make sure duplicate optionals are removed
    QCOMPARE( Query(
                  OptionalTerm::optionalizeTerm(
                      OptionalTerm::optionalizeTerm(
                          OptionalTerm::optionalizeTerm(
                              ResourceTypeTerm(Soprano::Vocabulary::NAO::Tag()))))
                  ).optimized(),
              Query( OptionalTerm::optionalizeTerm(
                         ResourceTypeTerm(Soprano::Vocabulary::NAO::Tag()))) );
}


void QueryLibTest::testLogicalOperators()
{
    // test negation
    ComparisonTerm ct1( Soprano::Vocabulary::NAO::hasTag(), LiteralTerm("bla") );
    QCOMPARE( NegationTerm::negateTerm(ct1), !ct1 );

    // test logical and
    ComparisonTerm ct2( Soprano::Vocabulary::NAO::hasTag(), LiteralTerm("foo") );
    LiteralTerm lt1( "bar" );
    QCOMPARE( ct1 && ct2 && lt1, Term(AndTerm( ct1, ct2, lt1 )) );

    // test logical or
    QCOMPARE( ct1 || ct2 || lt1, Term(OrTerm( ct1, ct2, lt1 )) );
}


void QueryLibTest::testComparison_data()
{
    QTest::addColumn<Nepomuk2::Query::Query>( "q1" );
    QTest::addColumn<Nepomuk2::Query::Query>( "q2" );

    // invalid queries should always be similar - trivial but worth checking anyway
    Query q1, q2;
    QTest::newRow("invalid") << q1 << q2;

    // file queries with differently sorted folder lists
    FileQuery fq1, fq2;
    fq1.addIncludeFolder( KUrl("/tmp") );
    fq1.addIncludeFolder( KUrl("/wurst") );
    fq2.addIncludeFolder( KUrl("/wurst") );
    fq2.addIncludeFolder( KUrl("/tmp") );
    QTest::newRow("file query include sorting") << Query(fq1) << Query(fq2);
}


void QueryLibTest::testComparison()
{
    QFETCH( Nepomuk2::Query::Query, q1 );
    QFETCH( Nepomuk2::Query::Query, q2 );

    QCOMPARE( q1, q2 );
}


void QueryLibTest::testTermFromProperty()
{
    QCOMPARE( Term::fromProperty(Soprano::Vocabulary::NAO::hasTag(), Nepomuk2::Variant( QString::fromLatin1("Hello World") )),
              Term(
                  ComparisonTerm( Soprano::Vocabulary::NAO::hasTag(),
                                  LiteralTerm( QLatin1String("Hello World") ),
                                  ComparisonTerm::Equal ) )
        );

    QCOMPARE( Term::fromProperty(Soprano::Vocabulary::NAO::hasTag(), Nepomuk2::Variant( 42 )),
              Term(
                  ComparisonTerm( Soprano::Vocabulary::NAO::hasTag(),
                                  LiteralTerm( 42 ),
                                  ComparisonTerm::Equal ) )
        );

    Nepomuk2::Resource res( QUrl("nepomuk:/res/foobar") );
    QCOMPARE( Term::fromProperty(Soprano::Vocabulary::NAO::hasTag(), Nepomuk2::Variant( res )),
              Term(
                  ComparisonTerm( Soprano::Vocabulary::NAO::hasTag(),
                                  ResourceTerm( res ),
                                  ComparisonTerm::Equal ) )
        );

    Nepomuk2::Resource res2( QUrl("nepomuk:/res/foobar2") );
    QCOMPARE( Term::fromProperty(Soprano::Vocabulary::NAO::hasTag(), Nepomuk2::Variant( QList<Nepomuk2::Resource>() << res << res2 )),
              Term(
                  AndTerm(
                      ComparisonTerm( Soprano::Vocabulary::NAO::hasTag(),
                                      ResourceTerm( res ),
                                      ComparisonTerm::Equal ),
                      ComparisonTerm( Soprano::Vocabulary::NAO::hasTag(),
                                      ResourceTerm( res2 ),
                                      ComparisonTerm::Equal ) ) )
        );
}

QTEST_KDEMAIN_CORE( QueryLibTest )

#include "querylibtest.moc"
