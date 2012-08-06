/*
 *
 * $Id: sourceheader 511311 2006-02-19 14:51:05Z trueg $
 *
 * This file is part of the Nepomuk KDE project.
 * Copyright (C) 2006-2009 Sebastian Trueg <trueg@kde.org>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING.LIB" for the exact licensing terms.
 */

#include "resourcetest.h"
#include "nie.h"

#include "resource.h"
#include "variant.h"
#include "resourcemanager.h"

#include <kdebug.h>
#include <ktemporaryfile.h>
#include <qtest_kde.h>

#include <Soprano/Soprano>

#include <QtCore/QTextStream>

using namespace Soprano;
using namespace Soprano::Vocabulary;
using namespace Nepomuk2;


void ResourceTest::testResourceStates()
{
    QUrl someUri = ResourceManager::instance()->generateUniqueUri( QString() );

    Resource r1( someUri );
    Resource r2( someUri );

    QCOMPARE( r1, r2 );

    QVERIFY( r1.isValid() );
    QVERIFY( !r1.exists() );

    r1.setProperty( QUrl("http://test/something"), 12 );

    QCOMPARE( r1, r2 );
    QVERIFY( r1.exists() );
}


void ResourceTest::testResourceRemoval()
{
    QString testiId( QLatin1String("testi") );

    Resource res( testiId );
    res.setProperty( QUrl("http://nepomuk.test.org/foo/bar"),  "foobar" );

    QUrl testiUri = res.uri();

    QVERIFY( !testiUri.isEmpty() );

    QVERIFY( ResourceManager::instance()->mainModel()->containsAnyStatement( Statement( testiUri, Node(), Node() ) ) );

    res.remove();

    QVERIFY( !res.exists() );

    QVERIFY( !ResourceManager::instance()->mainModel()->containsAnyStatement( Statement( testiUri, Node(), Node() ) ) );

    //
    // test recursive removal
    //
    Resource res2( "testi2" );

    res.setProperty( QUrl("http://nepomuk.test.org/foo/bar2"), res2 );

    QVERIFY( res.exists() );
    QVERIFY( res2.exists() );

    QVERIFY( ResourceManager::instance()->mainModel()->containsAnyStatement( Statement( res.uri(), QUrl("http://nepomuk.test.org/foo/bar2"), Node(res2.uri()) ) ) );

    res2.remove();

    QVERIFY( res.exists() );
    QVERIFY( !res2.exists() );

    QVERIFY( !ResourceManager::instance()->mainModel()->containsAnyStatement( Statement( res.uri(), QUrl("http://nepomuk.test.org/foo/bar2"), Node(res2.uri()) ) ) );

    //
    // Now make sure the relation between id and URI has actually be removed
    //
    Resource res3( testiId );
    QVERIFY( res3.uri() != testiUri );
}


void ResourceTest::testProperties()
{
    QUrl r1Uri, r2Uri;

    {
        Resource r1( "testi" );
        Resource r2( "testi2" );

        r1.setProperty( QUrl("http://nepomuk.test.org/int"), 17 );
        r1.setProperty( QUrl("http://nepomuk.test.org/bool1"), true );
        r1.setProperty( QUrl("http://nepomuk.test.org/bool2"), false );
        r1.setProperty( QUrl("http://nepomuk.test.org/double"), 2.2 );
        r1.setProperty( QUrl("http://nepomuk.test.org/string"), "test" );
        r1.setProperty( QUrl("http://nepomuk.test.org/date"), QDate::currentDate() );
        r1.setProperty( QUrl("http://nepomuk.test.org/Resource"), r2 );

        r1Uri = r1.uri();
        r2Uri = r2.uri();
    }

    QTextStream s(stdout);
    foreach( const Statement& st, ResourceManager::instance()->mainModel()->listStatements().allStatements() ) {
        s << st << endl;
    }

    {
        Resource r1( r1Uri );
        Resource r2( r2Uri );

        QVERIFY( r1.hasProperty( QUrl("http://nepomuk.test.org/int" ) ) );
        QVERIFY( r1.hasProperty( QUrl("http://nepomuk.test.org/bool1" ) ) );
        QVERIFY( r1.hasProperty( QUrl("http://nepomuk.test.org/bool2" ) ) );
        QVERIFY( r1.hasProperty( QUrl("http://nepomuk.test.org/double" ) ) );
        QVERIFY( r1.hasProperty( QUrl("http://nepomuk.test.org/string" ) ) );
        QVERIFY( r1.hasProperty( QUrl("http://nepomuk.test.org/date" ) ) );
        QVERIFY( r1.hasProperty( QUrl("http://nepomuk.test.org/Resource" ) ) );

        QCOMPARE( r1.property( QUrl("http://nepomuk.test.org/int" ) ).toInt(), 17 );
        QCOMPARE( r1.property( QUrl("http://nepomuk.test.org/bool1" ) ).toBool(), true );
        QCOMPARE( r1.property( QUrl("http://nepomuk.test.org/bool2" ) ).toBool(), false );
        QCOMPARE( r1.property( QUrl("http://nepomuk.test.org/double" ) ).toDouble(), 2.2 );
        QCOMPARE( r1.property( QUrl("http://nepomuk.test.org/string" ) ).toString(), QString("test") );
        QCOMPARE( r1.property( QUrl("http://nepomuk.test.org/date" ) ).toDate(), QDate::currentDate() );
        QCOMPARE( r1.property( QUrl("http://nepomuk.test.org/Resource" ) ).toResource(), r2 );

        QHash<QUrl, Variant> allProps = r1.properties();
        QCOMPARE( allProps.count(), 10 ); // properties + type + identifier + modification date
        QVERIFY( allProps.keys().contains( QUrl("http://nepomuk.test.org/int") ) );
        QVERIFY( allProps.keys().contains( QUrl("http://nepomuk.test.org/bool1") ) );
        QVERIFY( allProps.keys().contains( QUrl("http://nepomuk.test.org/bool2") ) );
        QVERIFY( allProps.keys().contains( QUrl("http://nepomuk.test.org/double") ) );
        QVERIFY( allProps.keys().contains( QUrl("http://nepomuk.test.org/string") ) );
        QVERIFY( allProps.keys().contains( QUrl("http://nepomuk.test.org/date") ) );
        QVERIFY( allProps.keys().contains( QUrl("http://nepomuk.test.org/Resource") ) );
    }
}


void ResourceTest::testRemoveProperty()
{
    QUrl p = QUrl("http://nepomuk.test.org/int");

    Resource r1;

    r1.setProperty( p, 17 );
    r1.removeProperty( p, 17 );

    QVERIFY( !r1.hasProperty( p ) );
    QVERIFY( !r1.property( p ).isValid() );
    QVERIFY( r1.property( p ).toVariantList().isEmpty() );
    QVERIFY( r1.property( p ).toNodeList().isEmpty() );

    QList<Soprano::Node> nodes =
        ResourceManager::instance()->mainModel()->listStatements(r1.uri(), p, Soprano::Node()).iterateObjects().allNodes();
    QCOMPARE(nodes.count(), 0);

    r1.setProperty( p, 17 );
    r1.addProperty( p, 18 );
    r1.removeProperty( p, 17 );

    QVERIFY( r1.hasProperty( p ) );
    QCOMPARE( r1.property( p ), Variant(18) );
    QCOMPARE( r1.property( p ).toVariantList().count(), 1 );
    QCOMPARE( r1.property( p ).toNodeList().count(), 1 );

    nodes =
        ResourceManager::instance()->mainModel()->listStatements(r1.uri(), p, Soprano::Node()).iterateObjects().allNodes();
    QCOMPARE(nodes.count(), 1);
    QCOMPARE(nodes[0], Soprano::Node(Soprano::LiteralValue(18)));

    r1.addProperty( p, 17 );
    Variant v;
    v.append(17);
    v.append(18);
    r1.removeProperty( p, v );

    QVERIFY( !r1.hasProperty( p ) );
    QVERIFY( !r1.property( p ).isValid() );
    QVERIFY( r1.property( p ).toVariantList().isEmpty() );
    QVERIFY( r1.property( p ).toNodeList().isEmpty() );

    nodes =
        ResourceManager::instance()->mainModel()->listStatements(r1.uri(), p, Soprano::Node()).iterateObjects().allNodes();
    QCOMPARE(nodes.count(), 0);
}


void ResourceTest::testResourceIdentifiers()
{
    QUrl theUri;
    {
        Resource r1( "wurst" );
        Resource r2( "wurst" );

        QVERIFY( r1 == r2 );

        QVERIFY( r1.uri() != QUrl("wurst") );

        r1.setProperty( QUrl("http://nepomuk.test.org/foo/bar"), "foobar" );

        theUri = r1.uri();

        QList<Statement> sl
            = ResourceManager::instance()->mainModel()->listStatements( Statement( r1.uri(), Node(), Node() ) ).allStatements();

        // rdf:type, nao:created, nao:lastModified, nao:identifier, and the property above
        QCOMPARE( sl.count(), 5 );

        QVERIFY( ResourceManager::instance()->mainModel()->containsAnyStatement( Statement( r1.uri(),
                                                                                            NAO::identifier(),
                                                                                            LiteralValue( "wurst" ) ) ) );
    }

    {
        Resource r1( theUri );
        Resource r2( "wurst" );

        QCOMPARE( r1, r2 );
    }
}


void ResourceTest::testResourceManager()
{
    {
        Resource r1( "res1", QUrl("http://test/mytype" ) );
        Resource r2( "res2", QUrl("http://test/mytype" ) );
        Resource r3( "res3", QUrl("http://test/myothertype" ) );
        Resource r4( "res4", QUrl("http://test/myothertype" ) );
        Resource r5( "res5", QUrl("http://test/myothertype" ) );
        Resource r6( "res6", QUrl("http://test/mythirdtype" ) );

        QList<Resource> rl = ResourceManager::instance()->allResourcesOfType( QUrl("http://test/mytype") );
        QCOMPARE( rl.count(), 2 );
        QVERIFY( rl.contains( r1 ) && rl.contains( r2 ) );

        rl = ResourceManager::instance()->allResourcesOfType( r6.type() );
        QCOMPARE( rl.count(), 1 );
        QCOMPARE( rl.first(), r6 );

        r1.setProperty( QUrl("http://test/prop1"), 42 );
        r3.setProperty( QUrl("http://test/prop1"), 42 );
        r4.setProperty( QUrl("http://test/prop1"), 41 );

        r3.setProperty( QUrl("http://test/prop2"), r6 );
        r4.setProperty( QUrl("http://test/prop2"), r6 );
        r5.setProperty( QUrl("http://test/prop2"), r6 );
        r6.setProperty( QUrl("http://test/prop2"), r1 );

        rl = ResourceManager::instance()->allResourcesWithProperty( QUrl("http://test/prop1"), 42 );
        QCOMPARE( rl.count(), 2 );
        QVERIFY( rl.contains( r1 ) && rl.contains( r3 ) );

        rl = ResourceManager::instance()->allResourcesWithProperty( QUrl("http://test/prop2"), r6 );
        QCOMPARE( rl.count(), 3 );
        QVERIFY( rl.contains( r3 ) && rl.contains( r4 ) && rl.contains( r5 ) );
    }

    {
        Resource r1( "res1", QUrl("http://test/mytype" ) );
        Resource r2( "res2", QUrl("http://test/mytype" ) );
        Resource r3( "res3", QUrl("http://test/myothertype" ) );
        Resource r4( "res4", QUrl("http://test/myothertype" ) );
        Resource r5( "res5", QUrl("http://test/myothertype" ) );
        Resource r6( "res6", QUrl("http://test/mythirdtype" ) );

        QList<Resource> rl = ResourceManager::instance()->allResourcesOfType( QUrl("http://test/mytype" ));

        QCOMPARE( rl.count(), 2 );
        QVERIFY( rl.contains( r1 ) && rl.contains( r2 ) );

        rl = ResourceManager::instance()->allResourcesOfType( r6.type() );
        QCOMPARE( rl.count(), 1 );
        QCOMPARE( rl.first(), r6 );

        rl = ResourceManager::instance()->allResourcesWithProperty( QUrl("http://test/prop1"), 42 );
        QCOMPARE( rl.count(), 2 );
        QVERIFY( rl.contains( r1 ) && rl.contains( r3 ) );

        rl = ResourceManager::instance()->allResourcesWithProperty( QUrl("http://test/prop2"), r6 );
        QCOMPARE( rl.count(), 3 );
        QVERIFY( rl.contains( r3 ) && rl.contains( r4 ) && rl.contains( r5 ) );

        QVERIFY( r1.hasProperty( QUrl("http://test/prop1" ) ) );
        QVERIFY( r3.hasProperty( QUrl("http://test/prop1" ) ) );
        QVERIFY( r4.hasProperty( QUrl("http://test/prop1" ) ) );

        QVERIFY( r3.hasProperty( QUrl("http://test/prop2" ) ) );
        QVERIFY( r4.hasProperty( QUrl("http://test/prop2" ) ) );
        QVERIFY( r5.hasProperty( QUrl("http://test/prop2" ) ) );
        QVERIFY( r6.hasProperty( QUrl("http://test/prop2" ) ) );

        QCOMPARE( r3.property( QUrl("http://test/prop2" )).toResource(), r6 );
        QCOMPARE( r4.property( QUrl("http://test/prop2" )).toResource(), r6 );
        QCOMPARE( r5.property( QUrl("http://test/prop2" )).toResource(), r6 );
        QCOMPARE( r6.property( QUrl("http://test/prop2" )).toResource(), r1 );
    }
}


void ResourceTest::testLocalFileUrls()
{
    // create a testfile
    KTemporaryFile tmpFile1;
    QVERIFY( tmpFile1.open() );

    QUrl tmpFile1ResUri;
    // create a new file resource. Resource should automatically save the nie:url property
    {
        Resource fileRes( KUrl(tmpFile1.fileName()) );
        fileRes.setRating( 4 );

        // make sure the nie:url is saved
        QVERIFY( ResourceManager::instance()->mainModel()->containsAnyStatement( fileRes.uri(), Nepomuk2::Vocabulary::NIE::url(), KUrl(tmpFile1.fileName()) ) );

        // make sure a proper nepomuk:/ uri has been created
        QVERIFY( fileRes.uri().scheme() == QLatin1String("nepomuk") );

        // make sure the local resource is reused with the file URL
        Resource fileRes2( KUrl(tmpFile1.fileName()) );
        QCOMPARE( fileRes.uri(), fileRes2.uri() );

        // make sure the local resource is reused with the resource URI
        Resource fileRes3( fileRes.uri() );
        QCOMPARE( fileRes.uri(), fileRes3.uri() );

        tmpFile1ResUri = fileRes.uri();

        // make sure even the string constructor will find the resource again with
        Resource fileRes4( KUrl(tmpFile1ResUri).url() );
        fileRes4.setRating(4);
        QCOMPARE( fileRes4.uri(), tmpFile1ResUri );

        // make sure the resource is reused with the local file path
        Resource fileRes5( tmpFile1.fileName() );
        fileRes4.setRating(5);
        QCOMPARE( fileRes5.uri(), tmpFile1ResUri );
    }

    // clear cache to be sure we call ResourceData::determineUri
    ResourceManager::instance()->clearCache();

    {
        // verify that the resource in question is found again
        Resource fileRes1( KUrl(tmpFile1.fileName()) );
        QCOMPARE( tmpFile1ResUri, fileRes1.uri() );

        // make sure the local resource is reused with the resource URI
        Resource fileRes2( tmpFile1ResUri );
        QCOMPARE( tmpFile1ResUri, fileRes2.uri() );

        // create a second test file
        KTemporaryFile tmpFile2;
        QVERIFY( tmpFile2.open() );

        // make sure the file:/ URL is reused as resource URI
        ResourceManager::instance()->mainModel()->addStatement( KUrl(tmpFile2.fileName()), Nepomuk2::Vocabulary::NIE::url(), KUrl(tmpFile2.fileName()) );

        Resource fileRes3( KUrl(tmpFile2.fileName()) );
        fileRes3.setRating( 4 );
        QCOMPARE( KUrl(fileRes3.uri()), KUrl(tmpFile2.fileName()) );

        // create a third test file
        KTemporaryFile tmpFile3;
        QVERIFY( tmpFile3.open() );

        // add a random bit of information about it
        ResourceManager::instance()->mainModel()->addStatement( KUrl(tmpFile3.fileName()), Soprano::Vocabulary::NAO::rating(), Soprano::LiteralValue(4) );

        Resource fileRes4( KUrl(tmpFile3.fileName()) );
        QCOMPARE( KUrl(fileRes4.uri()).url(), KUrl(tmpFile3.fileName()).url() );

        // make sure removing the resource results in us not reusing the URI
        QUrl fileRes1Uri = fileRes1.uri();
        fileRes1.remove();

        Resource fileRes5( KUrl(tmpFile1.fileName()) );
        QVERIFY( fileRes1Uri != fileRes5.uri() );
    }

    // clear cache to be sure we do not reuse the cache
    ResourceManager::instance()->clearCache();
}


void ResourceTest::testKickOffListRemoval()
{
    Soprano::Model * model = ResourceManager::instance()->mainModel();
    {
        KTemporaryFile tmpFile1;
        QVERIFY( tmpFile1.open() );

        Resource fileRes( KUrl(tmpFile1.fileName()) );
        fileRes.setRating( 4 );

        // make sure the nie:url is saved
        QVERIFY( model->containsAnyStatement( fileRes.uri(), Nepomuk2::Vocabulary::NIE::url(), KUrl(tmpFile1.fileName()) ) );

        // make sure a proper nepomuk:/ uri has been created
        QVERIFY( fileRes.uri().scheme() == QLatin1String("nepomuk") );

        // Remove the nie:url
        fileRes.removeProperty( Nepomuk2::Vocabulary::NIE::url() );

        Resource fileRes2( KUrl(tmpFile1.fileName()) );
        QVERIFY( fileRes.uri() != fileRes2.uri() );

        Resource r1( "res1" );
        r1.setProperty( QUrl("http://test/prop1"), 42 );
        r1.removeProperty( Soprano::Vocabulary::NAO::identifier() );

        Resource r2( "res1" );
        r2.setProperty( QUrl("http://test/prop1"), 46 );

        QVERIFY( r2.uri() != r1.uri() );
        QVERIFY( r1.property(QUrl("http://test/prop1")) != r2.property(QUrl("http://test/prop1")) );

    }
    {
        KTemporaryFile tmpFile;
        QVERIFY( tmpFile.open() );

        Resource fileRes( KUrl(tmpFile.fileName()) );
        fileRes.setRating( 4 );

        // make sure the nie:url is saved
        QVERIFY( model->containsAnyStatement( fileRes.uri(), Nepomuk2::Vocabulary::NIE::url(), KUrl(tmpFile.fileName()) ) );

        // make sure a proper nepomuk:/ uri has been created
        QVERIFY( fileRes.uri().scheme() == QLatin1String("nepomuk") );

        // Add a different nie:url
        KTemporaryFile tmpFile2;
        QVERIFY( tmpFile2.open() );
        fileRes.setProperty( Nepomuk2::Vocabulary::NIE::url(), KUrl(tmpFile2.fileName()) );

        // make sure the new nie:url is saved and the old one is gone
        QVERIFY( model->containsAnyStatement( fileRes.uri(), Nepomuk2::Vocabulary::NIE::url(), KUrl(tmpFile2.fileName()) ) );
        QVERIFY( !model->containsAnyStatement( fileRes.uri(), Nepomuk2::Vocabulary::NIE::url(), KUrl(tmpFile.fileName()) ) );

        // At this point the ResourceManager's kickOffUri's should contain
        // only tmpFile2 -> fileRes

        Resource fileRes2( KUrl(tmpFile.fileName()) );
        
        QVERIFY( fileRes.uri() != fileRes2.uri() );

        Resource fileRes3( KUrl(tmpFile2.fileName()) );
        QVERIFY( fileRes3.uri() == fileRes.uri() );
        
        Resource r1( "res1" );
        r1.setProperty( QUrl("http://test/prop1"), 42 );
        
        r1.setProperty( Soprano::Vocabulary::NAO::identifier(), "foo" );
        
        Resource r2( "res1" );
        r2.setProperty( QUrl("http://test/prop1"), 46 );

        QVERIFY( r2.uri() != r1.uri() );
        QVERIFY( r1.property(QUrl("http://test/prop1")) != r2.property(QUrl("http://test/prop1")) );

        Resource r3( "foo" );
        QVERIFY( r3.uri() == r1.uri() );
    }

}

QTEST_KDEMAIN(ResourceTest, NoGUI)

#include "resourcetest.moc"