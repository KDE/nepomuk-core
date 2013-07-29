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

#include "queryparsertest.h"
#include "qtest_querytostring.h"
#include "class.h"
#include "queryparser.h"
#include "query.h"
#include "literalterm.h"
#include "resourceterm.h"
#include "andterm.h"
#include "orterm.h"
#include "negationterm.h"
#include "comparisonterm.h"
#include "resourcetypeterm.h"
#include "nie.h"
#include "nfo.h"
#include "nmo.h"

#include <QDateTime>
#include <QtTest>
#include <qtest_kde.h>

#include "ktempdir.h"

#include <Soprano/LiteralValue>
#include <Soprano/Node>
#include <Soprano/StorageModel>
#include <Soprano/Backend>
#include <Soprano/PluginManager>
#include <Soprano/Vocabulary/NAO>
#include <Soprano/Vocabulary/NRL>
#include <Soprano/Vocabulary/RDF>
#include <Soprano/Vocabulary/RDFS>
#include <Soprano/Vocabulary/XMLSchema>

#include "property.h"
#include "resourcemanager.h"

Q_DECLARE_METATYPE( Nepomuk2::Query::Query )

using namespace Nepomuk2::Query;

void QueryParserTest::initTestCase()
{
}

void QueryParserTest::cleanupTestCase()
{
}

void QueryParserTest::testQueryParser_data()
{
    QTest::addColumn<QString>("queryString");
    QTest::addColumn<Nepomuk2::Query::Query>("query");

    // the invalid query
    QTest::newRow("empty query string")
        << QString()
        << Query();

    // simple literal queries
    QTest::newRow("simple literal query")
        << QString("Hello")
        << Query(LiteralTerm("Hello"));

    QTest::newRow("literal with spaces without quotes")
        << QString("Hello World")
        << Query(AndTerm(LiteralTerm("Hello"), LiteralTerm("World")));

    QTest::newRow("literal with spaces with quotes")
        << QString("\"Hello World\"")
        << Query(LiteralTerm("Hello World"));

    // type hints
    QTest::newRow("type hint")
        << QString("mails")
        << Query(ResourceTypeTerm(Nepomuk2::Vocabulary::NMO::Message()));

    // Properties
    QTest::newRow("simple property")
        << QString("named Kile")
        << Query(ComparisonTerm(Soprano::Vocabulary::RDFS::label(), LiteralTerm("Kile")));

    QTest::newRow("file sizes")
        << QString("size < 2 KiB")
        << Query(ComparisonTerm(Nepomuk2::Vocabulary::NIE::contentSize(), LiteralTerm(2048LL), ComparisonTerm::Smaller));

    QTest::newRow("comparators")
        << QString("size < 2KB size greater than 2KB")
        << Query(AndTerm(
            ComparisonTerm(Nepomuk2::Vocabulary::NIE::contentSize(), LiteralTerm(2000LL), ComparisonTerm::Smaller),
            ComparisonTerm(Nepomuk2::Vocabulary::NIE::contentSize(), LiteralTerm(2000LL), ComparisonTerm::Greater)));

    // logical operators
    QTest::newRow("logical operators")
        << QString("Vim or Emacs and Kate")
        << Query(AndTerm(OrTerm(LiteralTerm("Vim"), LiteralTerm("Emacs")), LiteralTerm("Kate")));

    QTest::newRow("inversion")
        << QString("-KWrite")
        << Query(NegationTerm::negateTerm(LiteralTerm("KWrite")));

    // Date-time values
    QTest::newRow("absolute date-time")
        << QString("before June 5, 2013")
        << Query(ComparisonTerm(Nepomuk2::Types::Property(), LiteralTerm(QDateTime(QDate(2013, 6, 5), QTime(0, 0, 0, 4))), ComparisonTerm::Smaller));

    // Nested queries
    QTest::newRow("nested queries")
        << QString("related to mails title Title, size > 2K")
        << Query(AndTerm(
            ComparisonTerm(Nepomuk2::Vocabulary::NIE::relatedTo(), AndTerm(
                ResourceTypeTerm(Nepomuk2::Vocabulary::NMO::Message()),
                ComparisonTerm(Nepomuk2::Vocabulary::NMO::messageSubject(), LiteralTerm("Title"))
            ), ComparisonTerm::Equal),
            ComparisonTerm(Nepomuk2::Vocabulary::NIE::contentSize(), LiteralTerm(2048LL), ComparisonTerm::Greater)));
}

void QueryParserTest::testQueryParser()
{
    QFETCH( QString, queryString );
    QFETCH( Nepomuk2::Query::Query, query );

    Query q = QueryParser::parseQuery( queryString );

    QCOMPARE( q, query );
}

QTEST_KDEMAIN_CORE( QueryParserTest )

#include "queryparsertest.moc"
