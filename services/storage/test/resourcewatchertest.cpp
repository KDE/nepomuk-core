/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2011 Sebastian Trueg <trueg@kde.org>
   Copyright (C) 2011-12 Vishesh Handa <handa.vish@gmail.com>

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

#include "resourcewatchertest.h"
#include "../datamanagementmodel.h"
#include "../virtuosoinferencemodel.h"
#include "../classandpropertytree.h"
#include "../resourcewatcherconnection.h"
#include "../resourcewatchermanager.h"

#include "simpleresource.h"
#include "simpleresourcegraph.h"

#include <QtTest>
#include "qtest_kde.h"
#include "qtest_dms.h"
#include <QStringList>
#include <Soprano/Soprano>
#include <Soprano/Graph>
#define USING_SOPRANO_NRLMODEL_UNSTABLE_API
#include <Soprano/NRLModel>

#include <KTemporaryFile>
#include <KTempDir>
#include <KTempDir>
#include <KDebug>

#include "nfo.h"
#include "nmm.h"
#include "nco.h"
#include "nie.h"
#include "pimo.h"
#include "resourcemanager.h"

using namespace Soprano;
using namespace Soprano::Vocabulary;
using namespace Nepomuk2;
using namespace Nepomuk2::Vocabulary;


void ResourceWatcherTest::resetModel()
{
    // remove all the junk from previous tests
    m_model->removeAllStatements();

    // add some classes and properties
    QUrl graph("graph:/onto");
    Nepomuk2::insertOntologies( m_model, graph );

    // rebuild the internals of the data management model
    m_classAndPropertyTree->rebuildTree(m_dmModel);
}


void ResourceWatcherTest::initTestCase()
{
    const Soprano::Backend* backend = Soprano::PluginManager::instance()->discoverBackendByName( "virtuosobackend" );
    QVERIFY( backend );
    m_storageDir = new KTempDir();
    m_model = backend->createModel( Soprano::BackendSettings() << Soprano::BackendSetting(Soprano::BackendOptionStorageDir, m_storageDir->name()) );
    QVERIFY( m_model );

    // DataManagementModel relies on the ussage of a NRLModel in the storage service
    m_nrlModel = new Soprano::NRLModel(m_model);
    Nepomuk2::insertNamespaceAbbreviations(m_model);

    m_classAndPropertyTree = new Nepomuk2::ClassAndPropertyTree(this);
    m_inferenceModel = new Nepomuk2::VirtuosoInferenceModel(m_nrlModel);
    m_dmModel = new Nepomuk2::DataManagementModel(m_classAndPropertyTree, m_inferenceModel);
}

void ResourceWatcherTest::cleanupTestCase()
{
    delete m_dmModel;
    delete m_nrlModel;
    delete m_model;
    delete m_storageDir;
    delete m_classAndPropertyTree;
}

void ResourceWatcherTest::init()
{
    resetModel();
}

void ResourceWatcherTest::testPropertyAddedSignal()
{
    // create a dummy resource which we will use
    const QUrl resA = m_dmModel->createResource(QList<QUrl>() << QUrl("class:/typeA"), QString(), QString(), QLatin1String("A"));

    // no error should be generated after the above method is executed.
    QVERIFY(!m_dmModel->lastError());

    // create a connection which listens to changes in res:/A
    Nepomuk2::ResourceWatcherConnection* con = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA, QList<QUrl>(), QList<QUrl>());
    QVERIFY(!m_dmModel->lastError());

    // spy for the propertyAdded signal
    QSignalSpy spy(con, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // change the resource
    m_dmModel->setProperty(QList<QUrl>() << resA, NAO::prefLabel(), QVariantList() << QLatin1String("foobar"), QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    // check that we actually got one signal
    QCOMPARE( spy.count(), 1 );

    // check that we got the correct values
    QList<QVariant> args = spy.takeFirst();

    // 1 param: the resource
    QCOMPARE(args[0].toString(), resA.toString());

    // 2 param: the property
    QCOMPARE(args[1].toString(), NAO::prefLabel().toString());

    // 3 param: the value
    QCOMPARE(args[2].value<QVariantList>().count(), 1);
    QCOMPARE(args[2].value<QVariantList>().first(), QVariant(QString(QLatin1String("foobar"))));

    // cleanup
    con->deleteLater();
}

void ResourceWatcherTest::testPropertyRemovedSignal()
{
    const QUrl resA = m_dmModel->createResource(QList<QUrl>() << QUrl("class:/typeA"), QLatin1String("foobar"), QString(), QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    Nepomuk2::ResourceWatcherConnection* con = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA, QList<QUrl>(), QList<QUrl>());

    QSignalSpy spy(con, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    m_dmModel->removeProperty(QList<QUrl>() << resA, NAO::prefLabel(), QVariantList() << QLatin1String("foobar"), QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE( spy.count(), 1 );

    QList<QVariant> args = spy.takeFirst();

    QCOMPARE(args[0].toString(), resA.toString());
    QCOMPARE(args[1].toString(), NAO::prefLabel().toString());
    QCOMPARE(args[3].value<QVariantList>().count(), 1);
    QCOMPARE(args[3].value<QVariantList>().first(), QVariant(QString(QLatin1String("foobar"))));

    con->deleteLater();
}

void ResourceWatcherTest::testResourceRemovedSignal()
{
    const QUrl resA = m_dmModel->createResource(QList<QUrl>() << QUrl("class:/typeA"), QString(), QString(), QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    Nepomuk2::ResourceWatcherConnection* con = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA, QList<QUrl>(), QList<QUrl>());
    QVERIFY(!m_dmModel->lastError());

    QSignalSpy spy(con, SIGNAL(resourceRemoved(QString, QStringList)));

    m_dmModel->removeResources(QList<QUrl>() << resA, Nepomuk2::RemovalFlags() , QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE( spy.count(), 1 );

    QList<QVariant> args = spy.takeFirst();

    QCOMPARE(args[0].toString(), resA.toString());

    con->deleteLater();
}

void ResourceWatcherTest::testStoreResources_createResources()
{
    ResourceWatcherManager *rvm = m_dmModel->resourceWatcherManager();
    ResourceWatcherConnection* con = rvm->createConnection( QList<QUrl>(), QList<QUrl>(),
                                                            QList<QUrl>() << NCO::Contact() );
    QVERIFY(!m_dmModel->lastError());

    SimpleResource res;
    res.addType( NCO::Contact() );
    res.addProperty( NCO::fullname(), QLatin1String("Haruki Murakami") );

    QSignalSpy spy(con, SIGNAL(resourceCreated(QString, QStringList)));

    m_dmModel->storeResources( SimpleResourceGraph() << res, QLatin1String("testApp") );
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE( spy.count(), 1 );

    QList<QVariant> args = spy.takeFirst();

    QList< Statement > stList = m_model->listStatements( Node(), RDF::type(), NCO::Contact() ).allStatements();
    QCOMPARE( stList.size(), 1 );

    const QUrl resUri = stList.first().subject().uri();
    QCOMPARE(args[0].toString(), resUri.toString());
    QCOMPARE(args[1].toStringList(), QStringList() << NCO::Contact().toString() );

    con->deleteLater();
}

void ResourceWatcherTest::testSetProperty()
{
    // create a dummy resource which we will use
    const QUrl resA = m_dmModel->createResource(QList<QUrl>() << QUrl("class:/typeA"), QString(), QString(), QLatin1String("A"));

    // no error should be generated after the above method is executed.
    QVERIFY(!m_dmModel->lastError());


    // a watcher for the resource
    Nepomuk2::ResourceWatcherConnection* resW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA, QList<QUrl>(), QList<QUrl>());
    QSignalSpy resWpChSpy(resW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the property
    Nepomuk2::ResourceWatcherConnection* propW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>());
    QSignalSpy propWpChSpy(propW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the resource and the property
    Nepomuk2::ResourceWatcherConnection* resPropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA, QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>());
    QSignalSpy resPropWpChSpy(resPropW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the type
    Nepomuk2::ResourceWatcherConnection* typeW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>(), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typeWpChSpy(typeW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the type and the property
    Nepomuk2::ResourceWatcherConnection* typePropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typePropWpChSpy(typePropW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));


    // set a new property
    // ===============================================================
    m_dmModel->setProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 42, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    // check all the signals
    QCOMPARE( resWpChSpy.count(), 1 );
    QList<QVariant> pChArgs = resWpChSpy.takeFirst();
    QCOMPARE(pChArgs[0].toString(), resA.toString());
    QCOMPARE(pChArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pChArgs[2].value<QVariantList>(), QVariantList() << QVariant(42));
    QCOMPARE(pChArgs[3].value<QVariantList>(), QVariantList());

    QCOMPARE( propWpChSpy.count(), 1 );
    QCOMPARE( propWpChSpy.takeFirst(), pChArgs);

    QCOMPARE( resPropWpChSpy.count(), 1 );
    QCOMPARE( resPropWpChSpy.takeFirst(), pChArgs);

    QCOMPARE( typeWpChSpy.count(), 1 );
    QCOMPARE( typeWpChSpy.takeFirst(), pChArgs);

    QCOMPARE( typePropWpChSpy.count(), 1 );
    QCOMPARE( typePropWpChSpy.takeFirst(), pChArgs);


    // set an existing property
    // ===============================================================
    m_dmModel->setProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 12, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE( resWpChSpy.count(), 1 );
    pChArgs = resWpChSpy.takeFirst();
    QCOMPARE(pChArgs[0].toString(), resA.toString());
    QCOMPARE(pChArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pChArgs[2].value<QVariantList>(), QVariantList() << QVariant(12));
    QCOMPARE(pChArgs[3].value<QVariantList>(), QVariantList() << QVariant(42));

    QCOMPARE(propWpChSpy.count(), 1);
    QCOMPARE(propWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(resPropWpChSpy.count(), 1);
    QCOMPARE(resPropWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typeWpChSpy.count(), 1);
    QCOMPARE(typeWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typePropWpChSpy.count(), 1);
    QCOMPARE(typePropWpChSpy.takeFirst(), pChArgs);


    // add a property
    // ===============================================================
    m_dmModel->setProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 12 << 42, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE( resWpChSpy.count(), 1 );
    pChArgs = resWpChSpy.takeFirst();
    QCOMPARE(pChArgs[0].toString(), resA.toString());
    QCOMPARE(pChArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pChArgs[2].value<QVariantList>(), QVariantList() << QVariant(42));
    QCOMPARE(pChArgs[3].value<QVariantList>().count(), 0);

    QCOMPARE(propWpChSpy.count(), 1);
    QCOMPARE(propWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(resPropWpChSpy.count(), 1);
    QCOMPARE(resPropWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typeWpChSpy.count(), 1);
    QCOMPARE(typeWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typePropWpChSpy.count(), 1);
    QCOMPARE(typePropWpChSpy.takeFirst(), pChArgs);


    // remove an existing property
    // ===============================================================
    m_dmModel->setProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 12, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE( resWpChSpy.count(), 1 );
    pChArgs = resWpChSpy.takeFirst();
    QCOMPARE(pChArgs[0].toString(), resA.toString());
    QCOMPARE(pChArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pChArgs[2].value<QVariantList>().count(), 0);
    QCOMPARE(pChArgs[3].value<QVariantList>(), QVariantList() << QVariant(42));

    QCOMPARE(propWpChSpy.count(), 1);
    QCOMPARE(propWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(resPropWpChSpy.count(), 1);
    QCOMPARE(resPropWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typeWpChSpy.count(), 1);
    QCOMPARE(typeWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typePropWpChSpy.count(), 1);
    QCOMPARE(typePropWpChSpy.takeFirst(), pChArgs);


    // set the already existing values
    // ===============================================================
    m_dmModel->setProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 12, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE(resWpChSpy.count(), 0);
    QCOMPARE(propWpChSpy.count(), 0);
    QCOMPARE(resPropWpChSpy.count(), 0);
    QCOMPARE(typeWpChSpy.count(), 0);
    QCOMPARE(typePropWpChSpy.count(), 0);

    // cleanup
    delete resW;
    delete propW;
    delete resPropW;
    delete typeW;
    delete typePropW;
}

void ResourceWatcherTest::testAddProperty()
{
    // create a dummy resource which we will use
    const QUrl resA = m_dmModel->createResource(QList<QUrl>() << QUrl("class:/typeA"), QString(), QString(), QLatin1String("A"));

    // no error should be generated after the above method is executed.
    QVERIFY(!m_dmModel->lastError());


    // a watcher for the resource
    Nepomuk2::ResourceWatcherConnection* resW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA, QList<QUrl>(), QList<QUrl>());
    QSignalSpy resWpChSpy(resW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the property
    Nepomuk2::ResourceWatcherConnection* propW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>());
    QSignalSpy propWpChSpy(propW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the resource and the property
    Nepomuk2::ResourceWatcherConnection* resPropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA, QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>());
    QSignalSpy resPropWpChSpy(resPropW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the type
    Nepomuk2::ResourceWatcherConnection* typeW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>(), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typeWpChSpy(typeW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the type and the property
    Nepomuk2::ResourceWatcherConnection* typePropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typePropWpChSpy(typePropW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));


    // add a new property
    // ===============================================================
    m_dmModel->addProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 42, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    // check all the signals
    QCOMPARE( resWpChSpy.count(), 1 );
    QVariantList pChArgs = resWpChSpy.takeFirst();
    QCOMPARE(pChArgs[0].toString(), resA.toString());
    QCOMPARE(pChArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pChArgs[2].value<QVariantList>(), QVariantList() << QVariant(42));
    QCOMPARE(pChArgs[3].value<QVariantList>(), QVariantList());

    QCOMPARE( propWpChSpy.count(), 1 );
    QCOMPARE( propWpChSpy.takeFirst(), pChArgs );

    QCOMPARE( resPropWpChSpy.count(), 1 );
    QCOMPARE( resPropWpChSpy.takeFirst(), pChArgs );

    QCOMPARE( typeWpChSpy.count(), 1 );
    QCOMPARE( typeWpChSpy.takeFirst(), pChArgs );

    QCOMPARE( typePropWpChSpy.count(), 1 );
    QCOMPARE( typePropWpChSpy.takeFirst(), pChArgs );


    // add another value to an existing property
    // ===============================================================
    m_dmModel->addProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 12, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE( resWpChSpy.count(), 1 );
    pChArgs = resWpChSpy.takeFirst();
    QCOMPARE(pChArgs[0].toString(), resA.toString());
    QCOMPARE(pChArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pChArgs[2].value<QVariantList>(), QVariantList() << QVariant(12));
    QCOMPARE(pChArgs[3].value<QVariantList>().count(), 0);

    QCOMPARE(propWpChSpy.count(), 1);
    QCOMPARE(propWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(resPropWpChSpy.count(), 1);
    QCOMPARE(resPropWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typeWpChSpy.count(), 1);
    QCOMPARE(typeWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typePropWpChSpy.count(), 1);
    QCOMPARE(typePropWpChSpy.takeFirst(), pChArgs);


    // add an already existing value
    // ===============================================================
    m_dmModel->addProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 12, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE(resWpChSpy.count(), 0 );
    QCOMPARE(propWpChSpy.count(), 0);
    QCOMPARE(resPropWpChSpy.count(), 0);
    QCOMPARE(typeWpChSpy.count(), 0);
    QCOMPARE(typePropWpChSpy.count(), 0);


    // cleanup
    delete resW;
    delete propW;
    delete resPropW;
    delete typeW;
    delete typePropW;
}

void ResourceWatcherTest::testRemoveProperty()
{
    // create a dummy resource which we will use
    const QUrl resA = m_dmModel->createResource(QList<QUrl>() << QUrl("class:/typeA"), QString(), QString(), QLatin1String("A"));
    m_dmModel->setProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 42 << 12, QLatin1String("A"));

    // no error should be generated after the above method is executed.
    QVERIFY(!m_dmModel->lastError());


    // a watcher for the resource
    Nepomuk2::ResourceWatcherConnection* resW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA, QList<QUrl>(), QList<QUrl>());
    QSignalSpy resWpChSpy(resW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the property
    Nepomuk2::ResourceWatcherConnection* propW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>());
    QSignalSpy propWpChSpy(propW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the resource and the property
    Nepomuk2::ResourceWatcherConnection* resPropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA, QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>());
    QSignalSpy resPropWpChSpy(resPropW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the type
    Nepomuk2::ResourceWatcherConnection* typeW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>(), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typeWpChSpy(typeW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the type and the property
    Nepomuk2::ResourceWatcherConnection* typePropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typePropWpChSpy(typePropW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));


    // remove a property
    // ===============================================================
    m_dmModel->removeProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 42, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE( resWpChSpy.count(), 1 );
    QVariantList pChArgs = resWpChSpy.takeFirst();
    QCOMPARE(pChArgs[0].toString(), resA.toString());
    QCOMPARE(pChArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pChArgs[2].value<QVariantList>(), QVariantList());
    QCOMPARE(pChArgs[3].value<QVariantList>(), QVariantList() << QVariant(42));

    QCOMPARE(propWpChSpy.count(), 1);
    QCOMPARE(propWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(resPropWpChSpy.count(), 1);
    QCOMPARE(resPropWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typeWpChSpy.count(), 1);
    QCOMPARE(typeWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typePropWpChSpy.count(), 1);
    QCOMPARE(typePropWpChSpy.takeFirst(), pChArgs);


    // remove another property (to check that we get an empty list in the changed signal)
    // ===============================================================
    m_dmModel->removeProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 12, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE( resWpChSpy.count(), 1 );
    pChArgs = resWpChSpy.takeFirst();
    QCOMPARE(pChArgs[0].toString(), resA.toString());
    QCOMPARE(pChArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pChArgs[2].value<QVariantList>(), QVariantList());
    QCOMPARE(pChArgs[3].value<QVariantList>(), QVariantList() << QVariant(12));

    QCOMPARE(propWpChSpy.count(), 1);
    QCOMPARE(propWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(resPropWpChSpy.count(), 1);
    QCOMPARE(resPropWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typeWpChSpy.count(), 1);
    QCOMPARE(typeWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typePropWpChSpy.count(), 1);
    QCOMPARE(typePropWpChSpy.takeFirst(), pChArgs);


    // remove non-existing property values
    // ===============================================================
    m_dmModel->setProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 42 << 12, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());
    resWpChSpy.clear();
    propWpChSpy.clear();
    resPropWpChSpy.clear();
    typeWpChSpy.clear();
    typePropWpChSpy.clear();
    m_dmModel->removeProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 2, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE(resWpChSpy.count(), 0);
    QCOMPARE(propWpChSpy.count(), 0);
    QCOMPARE(resPropWpChSpy.count(), 0);
    QCOMPARE(typeWpChSpy.count(), 0);
    QCOMPARE(typePropWpChSpy.count(), 0);

    // cleanup
    delete resW;
    delete propW;
    delete resPropW;
    delete typeW;
    delete typePropW;
}

void ResourceWatcherTest::testRemoveProperties()
{
    // create a dummy resource which we will use
    const QUrl resA = m_dmModel->createResource(QList<QUrl>() << QUrl("class:/typeA"), QString(), QString(), QLatin1String("A"));
    m_dmModel->setProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 42 << 12, QLatin1String("A"));

    // no error should be generated after the above method is executed.
    QVERIFY(!m_dmModel->lastError());


    // a watcher for the resource
    Nepomuk2::ResourceWatcherConnection* resW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA, QList<QUrl>(), QList<QUrl>());
    QSignalSpy resWpChSpy(resW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the property
    Nepomuk2::ResourceWatcherConnection* propW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>());
    QSignalSpy propWpChSpy(propW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the resource and the property
    Nepomuk2::ResourceWatcherConnection* resPropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA, QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>());
    QSignalSpy resPropWpChSpy(resPropW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the type
    Nepomuk2::ResourceWatcherConnection* typeW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>(), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typeWpChSpy(typeW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the type and the property
    Nepomuk2::ResourceWatcherConnection* typePropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typePropWpChSpy(typePropW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));


    // remove a property
    // ===============================================================
    m_dmModel->removeProperties(QList<QUrl>() << resA, QList<QUrl>() << QUrl("prop:/int"), QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE( resWpChSpy.count(), 1 );
    QVariantList pChArgs = resWpChSpy.takeFirst();
    QCOMPARE(pChArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pChArgs[2].value<QVariantList>(), QVariantList());
    QCOMPARE(pChArgs[3].value<QVariantList>().count(), 2);
    QVERIFY(pChArgs[3].value<QVariantList>().first() == QVariant(12) || *(++pChArgs[3].value<QVariantList>().constBegin()) == QVariant(12) );
    QVERIFY(pChArgs[3].value<QVariantList>().first() == QVariant(42) || *(++pChArgs[3].value<QVariantList>().constBegin()) == QVariant(42) );

    QCOMPARE(propWpChSpy.count(), 1);
    QCOMPARE(propWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(resPropWpChSpy.count(), 1);
    QCOMPARE(resPropWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typeWpChSpy.count(), 1);
    QCOMPARE(typeWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typePropWpChSpy.count(), 1);
    QCOMPARE(typePropWpChSpy.takeFirst(), pChArgs);


    // remove non-existing property values
    // ===============================================================
    m_dmModel->removeProperties(QList<QUrl>() << resA, QList<QUrl>() << QUrl("prop:/int"), QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE(resWpChSpy.count(), 0);
    QCOMPARE(propWpChSpy.count(), 0);
    QCOMPARE(resPropWpChSpy.count(), 0);
    QCOMPARE(typeWpChSpy.count(), 0);
    QCOMPARE(typePropWpChSpy.count(), 0);

    // cleanup
    delete resW;
    delete propW;
    delete resPropW;
    delete typeW;
    delete typePropW;
}

void ResourceWatcherTest::testRemoveResources()
{
    // create some resources for testing
    const QUrl resA = m_dmModel->createResource(QList<QUrl>() << QUrl("class:/typeA"), QString(), QString(), QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());
    const QUrl resB = m_dmModel->createResource(QList<QUrl>() << QUrl("class:/typeA"), QString(), QString(), QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());
    const QUrl resC = m_dmModel->createResource(QList<QUrl>() << QUrl("class:/typeC"), QString(), QString(), QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());


    // create a watcher by resource
    Nepomuk2::ResourceWatcherConnection* resW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA << resC, QList<QUrl>(), QList<QUrl>());
    QSignalSpy resWpRemSpy(resW, SIGNAL(resourceRemoved(QString, QString, QVariantList)));
}

void ResourceWatcherTest::testCreateResources()
{
    // TODO: do we want the propertyChanged signal for the label and description here?
}

void ResourceWatcherTest::testRemoveDataByApplication()
{
    // TODO: if a resource is removed completely no property change signals should be emitted but the resourceRemoved signal!
}

void ResourceWatcherTest::testRemoveAllDataByApplication()
{
}

void ResourceWatcherTest::testStoreResources_resourceCreated()
{
    // create one resource for testing
    const QUrl resAUri = m_dmModel->createResource(QList<QUrl>() << QUrl("class:/typeA"), QString(), QString(), QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    // a watcher for the type
    Nepomuk2::ResourceWatcherConnection* typeW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>(), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typeWrCreateSpy(typeW, SIGNAL(resourceCreated(QString,QStringList)));


    // now create a resource via storeResources
    SimpleResource resB;
    resB.addType(QUrl("class:/typeA"));
    resB.addProperty(QUrl("prop:/int"), 42);
    m_dmModel->storeResources(SimpleResourceGraph() << resB, QLatin1String("A"));

    // test that we got the resourceCreated signal
    QCOMPARE(typeWrCreateSpy.count(), 1);
    QVariantList args = typeWrCreateSpy.takeFirst();
    QCOMPARE(args[1].toStringList().count(), 1);
    QCOMPARE(args[1].toStringList().first(), QString::fromLatin1("class:/typeA"));


    // now add some new information to an existing resource
    SimpleResource resA(resAUri);
    resA.addProperty(QUrl("prop:/int"), 2);
    m_dmModel->storeResources(SimpleResourceGraph() << resA, QLatin1String("A"));

    // test that we did not get any resourceCreated signal
    QCOMPARE(typeWrCreateSpy.count(), 0);
}

void ResourceWatcherTest::testStoreResources_propertyChanged()
{
    // create one resource for testing
    const QUrl resAUri = m_dmModel->createResource(QList<QUrl>() << QUrl("class:/typeA"), QString(), QString(), QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());


    // a watcher for the resource
    Nepomuk2::ResourceWatcherConnection* resW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resAUri, QList<QUrl>(), QList<QUrl>());
    QSignalSpy resWpChSpy(resW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the property
    Nepomuk2::ResourceWatcherConnection* propW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>());
    QSignalSpy propWpChSpy(propW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the resource and the property
    Nepomuk2::ResourceWatcherConnection* resPropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resAUri, QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>());
    QSignalSpy resPropWpChSpy(resPropW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the type
    Nepomuk2::ResourceWatcherConnection* typeW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>(), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typeWpChSpy(typeW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the type and the property
    Nepomuk2::ResourceWatcherConnection* typePropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typePropWpChSpy(typePropW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));


    // now add some new information to the existing resource
    SimpleResource resA(resAUri);
    resA.addProperty(QUrl("prop:/int"), 12);
    resA.addProperty(QUrl("prop:/int"), 42);
    resA.addProperty(QUrl("prop:/string"), QLatin1String("Hello World"));
    m_dmModel->storeResources(SimpleResourceGraph() << resA, QLatin1String("A"));


    // check the signals
    QCOMPARE( resWpChSpy.count(), 2 );
    QVariantList pChArgs1, pChArgs2;
    if(resWpChSpy[0][1].toString() == QLatin1String("prop:/int")) {
        pChArgs1 = resWpChSpy[0];
        pChArgs2 = resWpChSpy[1];
    }
    else {
        pChArgs1 = resWpChSpy[1];
        pChArgs2 = resWpChSpy[0];
    }
    QCOMPARE(pChArgs1[0].toString(), resAUri.toString());
    QCOMPARE(pChArgs1[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pChArgs1[2].value<QVariantList>().count(), 2);
    QVERIFY(pChArgs1[2].value<QVariantList>().contains(QVariant(12)));
    QVERIFY(pChArgs1[2].value<QVariantList>().contains(QVariant(42)));
    QCOMPARE(pChArgs1[3].value<QVariantList>().count(), 0);
    QCOMPARE(pChArgs2[0].toString(), resAUri.toString());
    QCOMPARE(pChArgs2[1].toString(), QLatin1String("prop:/string"));
    QCOMPARE(pChArgs2[2].value<QVariantList>().count(), 1);
    QVERIFY(pChArgs2[2].value<QVariantList>().contains(QVariant(QLatin1String("Hello World"))));
    QCOMPARE(pChArgs2[3].value<QVariantList>().count(), 0);

    QCOMPARE(propWpChSpy.count(), 1);
    QCOMPARE(propWpChSpy.takeFirst(), pChArgs1);

    QCOMPARE(resPropWpChSpy.count(), 1);
    QCOMPARE(resPropWpChSpy.takeFirst(), pChArgs1);

    QCOMPARE(typeWpChSpy.count(), 2);
    QVERIFY(typeWpChSpy.contains(pChArgs1));
    QVERIFY(typeWpChSpy.contains(pChArgs2));

    QCOMPARE(typePropWpChSpy.count(), 2);
    QVERIFY(typePropWpChSpy.contains(pChArgs1));
    QVERIFY(typePropWpChSpy.contains(pChArgs2));
}

void ResourceWatcherTest::testStoreResources_typeAdded()
{
    // create one resource for testing
    const QUrl resAUri = m_dmModel->createResource(QList<QUrl>() << QUrl("class:/typeA"), QString(), QString(), QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());


    // a watcher for the resource
    Nepomuk2::ResourceWatcherConnection* resW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resAUri, QList<QUrl>(), QList<QUrl>());
    QSignalSpy resWpAddSpy(resW, SIGNAL(resourceTypesAdded(QString, QStringList)));

    // a watcher for the property
    Nepomuk2::ResourceWatcherConnection* propW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << RDF::type(), QList<QUrl>());
    QSignalSpy propWpAddSpy(propW, SIGNAL(resourceTypesAdded(QString, QStringList)));

    // a watcher for the resource and the property
    Nepomuk2::ResourceWatcherConnection* resPropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resAUri, QList<QUrl>() << RDF::type(), QList<QUrl>());
    QSignalSpy resPropWpAddSpy(resPropW, SIGNAL(resourceTypesAdded(QString, QStringList)));

    // a watcher for the resource type
    Nepomuk2::ResourceWatcherConnection* typeW1 = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>(), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy type1WpAddSpy(typeW1, SIGNAL(resourceTypesAdded(QString, QStringList)));

    // a watcher for the added type
    Nepomuk2::ResourceWatcherConnection* typeW2 = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>(), QList<QUrl>() << QUrl("class:/typeB"));
    QSignalSpy type2WpAddSpy(typeW2, SIGNAL(resourceTypesAdded(QString, QStringList)));

    // a watcher for the resource type and the property
    Nepomuk2::ResourceWatcherConnection* typePropW1 = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << RDF::type(), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typeProp1WpAddSpy(typePropW1, SIGNAL(resourceTypesAdded(QString, QStringList)));

    // a watcher for the added type and the property
    Nepomuk2::ResourceWatcherConnection* typePropW2 = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << RDF::type(), QList<QUrl>() << QUrl("class:/typeB"));
    QSignalSpy typeProp2WpAddSpy(typePropW2, SIGNAL(resourceTypesAdded(QString, QStringList)));


    // now add some new information to the existing resource
    SimpleResource resA(resAUri);
    resA.addProperty(RDF::type(), QUrl("class:/typeA"));
    resA.addProperty(RDF::type(), QUrl("class:/typeB"));
    m_dmModel->storeResources(SimpleResourceGraph() << resA, QLatin1String("A"));


    // verify the signals
    QCOMPARE( resWpAddSpy.count(), 1 );
    QVariantList args = resWpAddSpy.takeFirst();
    QCOMPARE(args[0].toString(), resAUri.toString());
    QCOMPARE(args[1].toStringList().count(), 1);
    QCOMPARE(args[1].toStringList().first(), QString::fromLatin1("class:/typeB"));

    QCOMPARE(propWpAddSpy.count(), 1);
    QCOMPARE(args, propWpAddSpy.takeFirst());

    QCOMPARE(resPropWpAddSpy.count(), 1);
    QCOMPARE(args, resPropWpAddSpy.takeFirst());

    QCOMPARE(type1WpAddSpy.count(), 1);
    QCOMPARE(args, type1WpAddSpy.takeFirst());

    QCOMPARE(type2WpAddSpy.count(), 1);
    QCOMPARE(args, type2WpAddSpy.takeFirst());

    QCOMPARE(typeProp1WpAddSpy.count(), 1);
    QCOMPARE(args, typeProp1WpAddSpy.takeFirst());

    QCOMPARE(typeProp2WpAddSpy.count(), 1);
    QCOMPARE(args, typeProp2WpAddSpy.takeFirst());
}

void ResourceWatcherTest::testRemoveProperty_typeRemoved()
{
    // create one resource for testing
    const QUrl resAUri = m_dmModel->createResource(QList<QUrl>() << QUrl("class:/typeA") << QUrl("class:/typeB"), QString(), QString(), QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());


    // a watcher for the resource
    Nepomuk2::ResourceWatcherConnection* resW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resAUri, QList<QUrl>(), QList<QUrl>());
    QSignalSpy resWpAddSpy(resW, SIGNAL(resourceTypesRemoved(QString, QStringList)));

    // a watcher for the property
    Nepomuk2::ResourceWatcherConnection* propW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << RDF::type(), QList<QUrl>());
    QSignalSpy propWpAddSpy(propW, SIGNAL(resourceTypesRemoved(QString, QStringList)));

    // a watcher for the resource and the property
    Nepomuk2::ResourceWatcherConnection* resPropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resAUri, QList<QUrl>() << RDF::type(), QList<QUrl>());
    QSignalSpy resPropWpAddSpy(resPropW, SIGNAL(resourceTypesRemoved(QString, QStringList)));

    // a watcher for the resource type
    Nepomuk2::ResourceWatcherConnection* typeW1 = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>(), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy type1WpAddSpy(typeW1, SIGNAL(resourceTypesRemoved(QString, QStringList)));

    // a watcher for the removed type
    Nepomuk2::ResourceWatcherConnection* typeW2 = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>(), QList<QUrl>() << QUrl("class:/typeB"));
    QSignalSpy type2WpAddSpy(typeW2, SIGNAL(resourceTypesRemoved(QString, QStringList)));

    // a watcher for the resource type and the property
    Nepomuk2::ResourceWatcherConnection* typePropW1 = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << RDF::type(), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typeProp1WpAddSpy(typePropW1, SIGNAL(resourceTypesRemoved(QString, QStringList)));

    // a watcher for the removed type and the property
    Nepomuk2::ResourceWatcherConnection* typePropW2 = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << RDF::type(), QList<QUrl>() << QUrl("class:/typeB"));
    QSignalSpy typeProp2WpAddSpy(typePropW2, SIGNAL(resourceTypesRemoved(QString, QStringList)));


    // remove a type
    m_dmModel->removeProperty(QList<QUrl>() << resAUri, RDF::type(), QVariantList() << QUrl("class:/typeB"), QLatin1String("A"));


    // verify the signals
    QCOMPARE( resWpAddSpy.count(), 1 );
    QVariantList args = resWpAddSpy.takeFirst();
    QCOMPARE(args[0].toString(), resAUri.toString());
    QCOMPARE(args[1].toStringList().count(), 1);
    QCOMPARE(args[1].toStringList().first(), QString::fromLatin1("class:/typeB"));

    QCOMPARE(propWpAddSpy.count(), 1);
    QCOMPARE(args, propWpAddSpy.takeFirst());

    QCOMPARE(resPropWpAddSpy.count(), 1);
    QCOMPARE(args, resPropWpAddSpy.takeFirst());

    QEXPECT_FAIL("", "No super proper handling support in ResourceWatcher", Continue);
    QCOMPARE(type1WpAddSpy.count(), 1);
    //QCOMPARE(args, type1WpAddSpy.takeFirst());

    QCOMPARE(type2WpAddSpy.count(), 1);
    QCOMPARE(args, type2WpAddSpy.takeFirst());

    QEXPECT_FAIL("", "No super proper handling support in ResourceWatcher", Continue);
    QCOMPARE(typeProp1WpAddSpy.count(), 1);
    //QCOMPARE(args, typeProp1WpAddSpy.takeFirst());

    QCOMPARE(typeProp2WpAddSpy.count(), 1);
    QCOMPARE(args, typeProp2WpAddSpy.takeFirst());
}

void ResourceWatcherTest::testMergeResources()
{
    SimpleResource tom;
    tom.addType( NCO::Contact() );
    tom.addProperty( NCO::fullname(), QLatin1String("Tom Marvolo Riddle") );

    SimpleResource voldemort;
    voldemort.addType( NCO::Contact() );
    voldemort.addProperty( NCO::fullname(), QLatin1String("Lord Voldemort") );
    voldemort.addProperty( NCO::gender(), NCO::male() );

    SimpleResource person;
    person.addType( PIMO::Person() );
    person.addProperty( PIMO::groundingOccurrence(), tom );
    person.addProperty( PIMO::groundingOccurrence(), voldemort );

    SimpleResourceGraph graph;
    graph << tom << voldemort << person;

    QHash<QUrl, QUrl> mappings = m_dmModel->storeResources( graph, QLatin1String("app") );
    QVERIFY(!m_dmModel->lastError());

    ResourceWatcherConnection* cWatcher = m_dmModel->resourceWatcherManager()->createConnection(
                                     QList<QUrl>(), QList<QUrl>(), QList<QUrl>() << NCO::Contact() );
    QSignalSpy wChSpy(cWatcher, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    ResourceWatcherConnection* pWatcher = m_dmModel->resourceWatcherManager()->createConnection(
                                    QList<QUrl>(), QList<QUrl>(), QList<QUrl>() << PIMO::Person() );
    QSignalSpy personCreateSpy(pWatcher, SIGNAL(resourceCreated(QString, QStringList)));
    QSignalSpy personPropChangeSpy(pWatcher, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    const QUrl tomUri = mappings.value( tom.uri() );
    const QUrl voldUri = mappings.value( voldemort.uri() );
    const QUrl personUri = mappings.value( person.uri() );

    m_dmModel->mergeResources( QList<QUrl>()<< tomUri << voldUri, QLatin1String("app") );
    QTest::qSleep( 100 );

    QCOMPARE( wChSpy.count(), 1 );
    QVariantList args = wChSpy.takeFirst();
    QCOMPARE( args[0].toUrl(), tomUri );
    QCOMPARE( args[1].toUrl(), NCO::gender() );
    QCOMPARE( args[2].toList().size(), 1 );
    QCOMPARE( args[2].toList().first().toUrl(), NCO::male() );
    QCOMPARE( args[3].toList().size(), 0 );

    QCOMPARE( personCreateSpy.count(), 0 );

    QCOMPARE( personPropChangeSpy.count(), 1 );
    args = personPropChangeSpy.takeFirst();
    QCOMPARE( args[0].toUrl(), personUri );
    QCOMPARE( args[1].toUrl(), PIMO::groundingOccurrence() );
    QCOMPARE( args[2].toList().size(), 0 );
    QCOMPARE( args[3].toList().size(), 1 );
    QCOMPARE( args[3].toList().first().toUrl(), voldUri );
}


QTEST_KDEMAIN_CORE(ResourceWatcherTest)

#include "resourcewatchertest.moc"
