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
#include "datamanagement.h"
#include "simpleresource.h"
#include "simpleresourcegraph.h"
#include "storeresourcesjob.h"

#include "nco.h"

#include <Soprano/Vocabulary/NAO>
#include <Soprano/Model>
#include <Soprano/QueryResultIterator>
#include <Soprano/StatementIterator>

#include <KTemporaryFile>
#include <KDebug>

#include <qtest_kde.h>

using namespace Soprano::Vocabulary;
using namespace Nepomuk2::Vocabulary;

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

void QueryServiceTest::sparqlQueries()
{
    SimpleResource email;
    email.addType( NCO::EmailAddress() );
    email.addProperty( NCO::emailAddress(), QLatin1String("spiderman@kde.org") );

    SimpleResource contact;
    contact.addType( NCO::Contact() );
    contact.setProperty( NCO::fullname(), QLatin1String("Peter Parker") );
    contact.addProperty( NCO::hasEmailAddress(), email );

    SimpleResourceGraph graph;
    graph << contact << email;

    StoreResourcesJob* job = graph.save();
    job->exec();
    QVERIFY( !job->error() );

    QUrl emailUri = job->mappings().value( email.uri() );
    QUrl contactUri = job->mappings().value( contact.uri() );

    QString query = QString::fromLatin1("select distinct ?email ?fullname where {"
                                        "?r a %1 ; %2 ?fullname ; %3 ?v . ?v %4 ?email . }")
                    .arg( Soprano::Node::resourceToN3( NCO::Contact() ),
                          Soprano::Node::resourceToN3( NCO::fullname() ),
                          Soprano::Node::resourceToN3( NCO::hasEmailAddress() ),
                          Soprano::Node::resourceToN3( NCO::emailAddress() ) );

    Query::RequestPropertyMap map;
    map.insert( "email", NCO::emailAddress() );
    map.insert( "fullname", NCO::fullname() );

    Query::QueryServiceClient client;
    QSignalSpy spy( &client, SIGNAL(newEntries(QList<Nepomuk2::Query::Result>)) );

    QEventLoop loop;
    QObject::connect( &client, SIGNAL(finishedListing()), &loop, SLOT(quit()) );
    client.sparqlQuery( query, map );
    loop.exec();

    QCOMPARE( spy.count(), 1 );
    QList<QVariant> list = spy.takeFirst();
    QCOMPARE(list.size(), 1);
    QList<Query::Result> results = list.first().value< QList<Query::Result> >();
    QCOMPARE(results.size(), 1);

    Query::Result result = results.first();
    QVERIFY( !result.resource().isValid() );

    QHash< Types::Property, Soprano::Node > reqProp = result.requestProperties();
    kDebug() << reqProp;
    QCOMPARE( reqProp.size(), 2 );
    QVERIFY( reqProp.contains( NCO::emailAddress() ) );
    QVERIFY( reqProp.contains( NCO::fullname() ) );

    QCOMPARE( reqProp[NCO::emailAddress()].literal().toString(), QLatin1String("spiderman@kde.org") );
    QCOMPARE( reqProp[NCO::fullname()].literal().toString(), QLatin1String("Peter Parker") );
}

}

QTEST_KDEMAIN(Nepomuk2::QueryServiceTest, NoGUI)