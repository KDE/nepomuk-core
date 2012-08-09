/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) 2012 Vishesh Handa <me@vhanda.in>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "queryservicetest.h"

#include "resource.h"
#include "tag.h"
#include "resourcemanager.h"
#include "queryserviceclient.h"
#include "dbusoperators_p.h"
#include "comparisonterm.h"
#include "resourceterm.h"
#include "result.h"
#include "variant.h"
#include "property.h"
#include "class.h"

#include <Soprano/Vocabulary/NAO>
#include <Soprano/Model>
#include <Soprano/QueryResultIterator>
#include <Soprano/StatementIterator>

#include <KTemporaryFile>
#include <KDebug>

#include <qtest_kde.h>

using namespace Soprano::Vocabulary;

namespace Nepomuk2 {

namespace {
    void queryAndWaitTillFinishedListing(Query::QueryServiceClient* client,
                                         const Query::Query& query) {
        QEventLoop loop;
        QObject::connect( client, SIGNAL(finishedListing()), &loop, SLOT(quit()) );
        client->query( query );
        loop.exec();
    }
}
void QueryServiceTest::tagsUpdates()
{
    kDebug();
    KTemporaryFile file;
    QVERIFY(file.open());

    Tag tag("Test");
    QVERIFY(!tag.exists());

    Resource fileRes(file.fileName());
    QVERIFY(!fileRes.exists());

    fileRes.addTag( tag );
    QCOMPARE(fileRes.tags().size(), 1);
    QCOMPARE(fileRes.tags(), QList<Tag>() << tag);
    QVERIFY(tag.exists());

    Query::QueryServiceClient client;
    QSignalSpy spy( &client, SIGNAL(newEntries(QList<Nepomuk2::Query::Result>)) );

    Query::ComparisonTerm ct(NAO::hasTag(), Query::ResourceTerm(tag));
    Query::Query query( ct );

    queryAndWaitTillFinishedListing( &client, query );

    QCOMPARE(spy.count(), 1);
    QList<QVariant> list = spy.takeFirst();
    QCOMPARE(list.size(), 1);
    QList<Query::Result> results = list.first().value< QList<Query::Result> >();
    QCOMPARE(results.size(), 1);

    Query::Result result = results.first();
    QCOMPARE(result.resource(), fileRes);

    KTemporaryFile file2;
    QVERIFY(file2.open());

    Resource fileRes2(file2.fileName());
    fileRes2.addTag( tag );

    // We need a wait a minimum of 2000 msecs, cause the QueryService waits that long
    QTest::qWait( 5000 );

    QCOMPARE(spy.count(), 1);
    list = spy.takeFirst();
    QCOMPARE(list.size(), 1);
    results = list.first().value< QList<Query::Result> >();
    QCOMPARE(results.size(), 1);

    result = results.first();
    QCOMPARE(result.resource(), fileRes2);
}

}

QTEST_KDEMAIN(Nepomuk2::QueryServiceTest, NoGUI)