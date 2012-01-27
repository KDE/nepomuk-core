/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2011 Sebastian Trueg <trueg@kde.org>
   Copyright (C) 2011 Vishesh Handa <handa.vish@gmail.com>

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
#include "resourcemanager.h"

using namespace Soprano;
using namespace Soprano::Vocabulary;
using namespace Nepomuk;
using namespace Nepomuk::Vocabulary;


void ResourceWatcherTest::resetModel()
{
    // remove all the junk from previous tests
    m_model->removeAllStatements();

    // add some classes and properties
    QUrl graph("graph:/onto");
    Nepomuk::insertOntologies( m_model, graph );

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
    m_classAndPropertyTree = new Nepomuk::ClassAndPropertyTree(this);
    m_dmModel = new Nepomuk::DataManagementModel(m_classAndPropertyTree, m_nrlModel);
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
    Nepomuk::ResourceWatcherConnection* con = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA, QList<QUrl>(), QList<QUrl>());
    QVERIFY(!m_dmModel->lastError());

    // spy for the propertyAdded signal
    QSignalSpy spy(con, SIGNAL(propertyAdded(QString, QString, QVariantList)));

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

    Nepomuk::ResourceWatcherConnection* con = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA, QList<QUrl>(), QList<QUrl>());

    QSignalSpy spy(con, SIGNAL(propertyRemoved(QString, QString, QVariantList)));

    m_dmModel->removeProperty(QList<QUrl>() << resA, NAO::prefLabel(), QVariantList() << QLatin1String("foobar"), QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE( spy.count(), 1 );

    QList<QVariant> args = spy.takeFirst();

    QCOMPARE(args[0].toString(), resA.toString());
    QCOMPARE(args[1].toString(), NAO::prefLabel().toString());
    QCOMPARE(args[2].value<QVariantList>().count(), 1);
    QCOMPARE(args[2].value<QVariantList>().first(), QVariant(QString(QLatin1String("foobar"))));

    con->deleteLater();
}

void ResourceWatcherTest::testResourceRemovedSignal()
{
    const QUrl resA = m_dmModel->createResource(QList<QUrl>() << QUrl("class:/typeA"), QString(), QString(), QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    Nepomuk::ResourceWatcherConnection* con = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA, QList<QUrl>(), QList<QUrl>());
    QVERIFY(!m_dmModel->lastError());

    QSignalSpy spy(con, SIGNAL(resourceRemoved(QString, QStringList)));

    m_dmModel->removeResources(QList<QUrl>() << resA, Nepomuk::RemovalFlags() , QLatin1String("A"));
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
    Nepomuk::ResourceWatcherConnection* resW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA, QList<QUrl>(), QList<QUrl>());
    QSignalSpy resWpAddSpy(resW, SIGNAL(propertyAdded(QString, QString, QVariantList)));
    QSignalSpy resWpRemSpy(resW, SIGNAL(propertyRemoved(QString, QString, QVariantList)));
    QSignalSpy resWpChSpy(resW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the property
    Nepomuk::ResourceWatcherConnection* propW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>());
    QSignalSpy propWpAddSpy(propW, SIGNAL(propertyAdded(QString, QString, QVariantList)));
    QSignalSpy propWpRemSpy(propW, SIGNAL(propertyRemoved(QString, QString, QVariantList)));
    QSignalSpy propWpChSpy(propW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the resource and the property
    Nepomuk::ResourceWatcherConnection* resPropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA, QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>());
    QSignalSpy resPropWpAddSpy(resPropW, SIGNAL(propertyAdded(QString, QString, QVariantList)));
    QSignalSpy resPropWpRemSpy(resPropW, SIGNAL(propertyRemoved(QString, QString, QVariantList)));
    QSignalSpy resPropWpChSpy(resPropW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the type
    Nepomuk::ResourceWatcherConnection* typeW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>(), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typeWpAddSpy(typeW, SIGNAL(propertyAdded(QString, QString, QVariantList)));
    QSignalSpy typeWpRemSpy(typeW, SIGNAL(propertyRemoved(QString, QString, QVariantList)));
    QSignalSpy typeWpChSpy(typeW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the type and the property
    Nepomuk::ResourceWatcherConnection* typePropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typePropWpAddSpy(typePropW, SIGNAL(propertyAdded(QString, QString, QVariantList)));
    QSignalSpy typePropWpRemSpy(typePropW, SIGNAL(propertyRemoved(QString, QString, QVariantList)));
    QSignalSpy typePropWpChSpy(typePropW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));


    // set a new property
    // ===============================================================
    m_dmModel->setProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 42, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    // check all the signals
    QCOMPARE( resWpAddSpy.count(), 1 );
    QList<QVariant> pAddArgs = resWpAddSpy.takeFirst();
    QCOMPARE(pAddArgs[0].toString(), resA.toString());
    QCOMPARE(pAddArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pAddArgs[2].value<QVariantList>().count(), 1);
    QCOMPARE(pAddArgs[2].value<QVariantList>().first(), QVariant(42));
    QCOMPARE( resWpRemSpy.count(), 0 );
    QCOMPARE( resWpChSpy.count(), 1 );
    QList<QVariant> pChArgs = resWpChSpy.takeFirst();
    QCOMPARE(pChArgs[0].toString(), resA.toString());
    QCOMPARE(pChArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pChArgs[2].value<QVariantList>(), QVariantList() << QVariant(42));
    QCOMPARE(pChArgs[3].value<QVariantList>(), QVariantList());


    QCOMPARE( propWpAddSpy.count(), 1 );
    QCOMPARE( propWpAddSpy.takeFirst(), pAddArgs );
    QCOMPARE( propWpRemSpy.count(), 0 );
    QCOMPARE( propWpChSpy.count(), 1 );
    QCOMPARE( propWpChSpy.takeFirst(), pChArgs);

    QCOMPARE( resPropWpAddSpy.count(), 1 );
    QCOMPARE( resPropWpAddSpy.takeFirst(), pAddArgs );
    QCOMPARE( resPropWpRemSpy.count(), 0 );
    QCOMPARE( resPropWpChSpy.count(), 1 );
    QCOMPARE( resPropWpChSpy.takeFirst(), pChArgs);

    QCOMPARE( typeWpAddSpy.count(), 1 );
    QCOMPARE( typeWpAddSpy.takeFirst(), pAddArgs);
    QCOMPARE( typeWpRemSpy.count(), 0 );
    QCOMPARE( typeWpChSpy.count(), 1 );
    QCOMPARE( typeWpChSpy.takeFirst(), pChArgs);

    QCOMPARE( typePropWpAddSpy.count(), 1 );
    QCOMPARE( typePropWpAddSpy.takeFirst(), pAddArgs );
    QCOMPARE( typePropWpRemSpy.count(), 0 );
    QCOMPARE( typePropWpChSpy.count(), 1 );
    QCOMPARE( typePropWpChSpy.takeFirst(), pChArgs);


    // set an existing property
    // ===============================================================
    m_dmModel->setProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 12, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE( resWpAddSpy.count(), 1 );
    pAddArgs = resWpAddSpy.takeFirst();
    QCOMPARE(pAddArgs[0].toString(), resA.toString());
    QCOMPARE(pAddArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pAddArgs[2].value<QVariantList>().count(), 1);
    QCOMPARE(pAddArgs[2].value<QVariantList>().first(), QVariant(12));

    QCOMPARE( resWpRemSpy.count(), 1 );
    QVariantList pRemArgs = resWpRemSpy.takeFirst();
    QCOMPARE(pRemArgs[0].toString(), resA.toString());
    QCOMPARE(pRemArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pRemArgs[2].value<QVariantList>().count(), 1);
    QCOMPARE(pRemArgs[2].value<QVariantList>().first(), QVariant(42));

    QCOMPARE( resWpChSpy.count(), 1 );
    pChArgs = resWpChSpy.takeFirst();
    QCOMPARE(pChArgs[0].toString(), resA.toString());
    QCOMPARE(pChArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pChArgs[2].value<QVariantList>(), QVariantList() << QVariant(12));
    QCOMPARE(pChArgs[3].value<QVariantList>(), QVariantList() << QVariant(42));

    QCOMPARE(propWpAddSpy.count(), 1);
    QCOMPARE(propWpAddSpy.takeFirst(), pAddArgs);
    QCOMPARE(propWpRemSpy.count(), 1);
    QCOMPARE(propWpRemSpy.takeFirst(), pRemArgs);
    QCOMPARE(propWpChSpy.count(), 1);
    QCOMPARE(propWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(resPropWpAddSpy.count(), 1);
    QCOMPARE(resPropWpAddSpy.takeFirst(), pAddArgs);
    QCOMPARE(resPropWpRemSpy.count(), 1);
    QCOMPARE(resPropWpRemSpy.takeFirst(), pRemArgs);
    QCOMPARE(resPropWpChSpy.count(), 1);
    QCOMPARE(resPropWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typeWpAddSpy.count(), 1);
    QCOMPARE(typeWpAddSpy.takeFirst(), pAddArgs);
    QCOMPARE(typeWpRemSpy.count(), 1);
    QCOMPARE(typeWpRemSpy.takeFirst(), pRemArgs);
    QCOMPARE(typeWpChSpy.count(), 1);
    QCOMPARE(typeWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typePropWpAddSpy.count(), 1);
    QCOMPARE(typePropWpAddSpy.takeFirst(), pAddArgs);
    QCOMPARE(typePropWpRemSpy.count(), 1);
    QCOMPARE(typePropWpRemSpy.takeFirst(), pRemArgs);
    QCOMPARE(typePropWpChSpy.count(), 1);
    QCOMPARE(typePropWpChSpy.takeFirst(), pChArgs);


    // add a property
    // ===============================================================
    m_dmModel->setProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 12 << 42, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE( resWpAddSpy.count(), 1 );
    pAddArgs = resWpAddSpy.takeFirst();
    QCOMPARE(pAddArgs[0].toString(), resA.toString());
    QCOMPARE(pAddArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pAddArgs[2].value<QVariantList>().count(), 1);
    QCOMPARE(pAddArgs[2].value<QVariantList>().first(), QVariant(42));

    QCOMPARE( resWpRemSpy.count(), 0 );

    QCOMPARE( resWpChSpy.count(), 1 );
    pChArgs = resWpChSpy.takeFirst();
    QCOMPARE(pChArgs[0].toString(), resA.toString());
    QCOMPARE(pChArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pChArgs[2].value<QVariantList>(), QVariantList() << QVariant(42));
    QCOMPARE(pChArgs[3].value<QVariantList>().count(), 0);

    QCOMPARE(propWpAddSpy.count(), 1);
    QCOMPARE(propWpAddSpy.takeFirst(), pAddArgs);
    QCOMPARE(propWpRemSpy.count(), 0);
    QCOMPARE(propWpChSpy.count(), 1);
    QCOMPARE(propWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(resPropWpAddSpy.count(), 1);
    QCOMPARE(resPropWpAddSpy.takeFirst(), pAddArgs);
    QCOMPARE(resPropWpRemSpy.count(), 0);
    QCOMPARE(resPropWpChSpy.count(), 1);
    QCOMPARE(resPropWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typeWpAddSpy.count(), 1);
    QCOMPARE(typeWpAddSpy.takeFirst(), pAddArgs);
    QCOMPARE(typeWpRemSpy.count(), 0);
    QCOMPARE(typeWpChSpy.count(), 1);
    QCOMPARE(typeWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typePropWpAddSpy.count(), 1);
    QCOMPARE(typePropWpAddSpy.takeFirst(), pAddArgs);
    QCOMPARE(typePropWpRemSpy.count(), 0);
    QCOMPARE(typePropWpChSpy.count(), 1);
    QCOMPARE(typePropWpChSpy.takeFirst(), pChArgs);


    // remove an existing property
    // ===============================================================
    m_dmModel->setProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 12, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE( resWpAddSpy.count(), 0 );

    QCOMPARE( resWpRemSpy.count(), 1 );
    pRemArgs = resWpRemSpy.takeFirst();
    QCOMPARE(pRemArgs[0].toString(), resA.toString());
    QCOMPARE(pRemArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pRemArgs[2].value<QVariantList>().count(), 1);
    QCOMPARE(pRemArgs[2].value<QVariantList>().first(), QVariant(42));

    QCOMPARE( resWpChSpy.count(), 1 );
    pChArgs = resWpChSpy.takeFirst();
    QCOMPARE(pChArgs[0].toString(), resA.toString());
    QCOMPARE(pChArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pChArgs[2].value<QVariantList>().count(), 0);
    QCOMPARE(pChArgs[3].value<QVariantList>(), QVariantList() << QVariant(42));

    QCOMPARE(propWpAddSpy.count(), 0);
    QCOMPARE(propWpRemSpy.count(), 1);
    QCOMPARE(propWpRemSpy.takeFirst(), pRemArgs);
    QCOMPARE(propWpChSpy.count(), 1);
    QCOMPARE(propWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(resPropWpAddSpy.count(), 0);
    QCOMPARE(resPropWpRemSpy.count(), 1);
    QCOMPARE(resPropWpRemSpy.takeFirst(), pRemArgs);
    QCOMPARE(resPropWpChSpy.count(), 1);
    QCOMPARE(resPropWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typeWpAddSpy.count(), 0);
    QCOMPARE(typeWpRemSpy.count(), 1);
    QCOMPARE(typeWpRemSpy.takeFirst(), pRemArgs);
    QCOMPARE(typeWpChSpy.count(), 1);
    QCOMPARE(typeWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typePropWpAddSpy.count(), 0);
    QCOMPARE(typePropWpRemSpy.count(), 1);
    QCOMPARE(typePropWpRemSpy.takeFirst(), pRemArgs);
    QCOMPARE(typePropWpChSpy.count(), 1);
    QCOMPARE(typePropWpChSpy.takeFirst(), pChArgs);


    // set the already existing values
    // ===============================================================
    m_dmModel->setProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 12, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE(resWpAddSpy.count(), 0);
    QCOMPARE(resWpChSpy.count(), 0);
    QCOMPARE(resWpRemSpy.count(), 0);

    QCOMPARE(propWpAddSpy.count(), 0);
    QCOMPARE(propWpChSpy.count(), 0);
    QCOMPARE(propWpRemSpy.count(), 0);

    QCOMPARE(resPropWpAddSpy.count(), 0);
    QCOMPARE(resPropWpChSpy.count(), 0);
    QCOMPARE(resPropWpRemSpy.count(), 0);

    QCOMPARE(typeWpAddSpy.count(), 0);
    QCOMPARE(typeWpChSpy.count(), 0);
    QCOMPARE(typeWpRemSpy.count(), 0);

    QCOMPARE(typePropWpAddSpy.count(), 0);
    QCOMPARE(typePropWpChSpy.count(), 0);
    QCOMPARE(typePropWpRemSpy.count(), 0);


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
    Nepomuk::ResourceWatcherConnection* resW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA, QList<QUrl>(), QList<QUrl>());
    QSignalSpy resWpAddSpy(resW, SIGNAL(propertyAdded(QString, QString, QVariantList)));
    QSignalSpy resWpChSpy(resW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the property
    Nepomuk::ResourceWatcherConnection* propW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>());
    QSignalSpy propWpAddSpy(propW, SIGNAL(propertyAdded(QString, QString, QVariantList)));
    QSignalSpy propWpChSpy(propW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the resource and the property
    Nepomuk::ResourceWatcherConnection* resPropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA, QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>());
    QSignalSpy resPropWpAddSpy(resPropW, SIGNAL(propertyAdded(QString, QString, QVariantList)));
    QSignalSpy resPropWpChSpy(resPropW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the type
    Nepomuk::ResourceWatcherConnection* typeW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>(), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typeWpAddSpy(typeW, SIGNAL(propertyAdded(QString, QString, QVariantList)));
    QSignalSpy typeWpChSpy(typeW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the type and the property
    Nepomuk::ResourceWatcherConnection* typePropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typePropWpAddSpy(typePropW, SIGNAL(propertyAdded(QString, QString, QVariantList)));
    QSignalSpy typePropWpChSpy(typePropW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));


    // add a new property
    // ===============================================================
    m_dmModel->addProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 42, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    // check all the signals
    QCOMPARE( resWpAddSpy.count(), 1 );
    QList<QVariant> pAddArgs = resWpAddSpy.takeFirst();
    QCOMPARE(pAddArgs[0].toString(), resA.toString());
    QCOMPARE(pAddArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pAddArgs[2].value<QVariantList>().count(), 1);
    QCOMPARE(pAddArgs[2].value<QVariantList>().first(), QVariant(42));
    QCOMPARE( resWpChSpy.count(), 1 );
    QVariantList pChArgs = resWpChSpy.takeFirst();
    QCOMPARE(pChArgs[0].toString(), resA.toString());
    QCOMPARE(pChArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pChArgs[2].value<QVariantList>(), QVariantList() << QVariant(42));
    QCOMPARE(pChArgs[3].value<QVariantList>(), QVariantList());

    QCOMPARE( propWpAddSpy.count(), 1 );
    QCOMPARE( propWpAddSpy.takeFirst(), pAddArgs );
    QCOMPARE( propWpChSpy.count(), 1 );
    QCOMPARE( propWpChSpy.takeFirst(), pChArgs );

    QCOMPARE( resPropWpAddSpy.count(), 1 );
    QCOMPARE( resPropWpAddSpy.takeFirst(), pAddArgs );
    QCOMPARE( resPropWpChSpy.count(), 1 );
    QCOMPARE( resPropWpChSpy.takeFirst(), pChArgs );

    QCOMPARE( typeWpAddSpy.count(), 1 );
    QCOMPARE( typeWpAddSpy.takeFirst(), pAddArgs);
    QCOMPARE( typeWpChSpy.count(), 1 );
    QCOMPARE( typeWpChSpy.takeFirst(), pChArgs );

    QCOMPARE( typePropWpAddSpy.count(), 1 );
    QCOMPARE( typePropWpAddSpy.takeFirst(), pAddArgs );
    QCOMPARE( typePropWpChSpy.count(), 1 );
    QCOMPARE( typePropWpChSpy.takeFirst(), pChArgs );


    // add another value to an existing property
    // ===============================================================
    m_dmModel->addProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 12, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE( resWpAddSpy.count(), 1 );
    pAddArgs = resWpAddSpy.takeFirst();
    QCOMPARE(pAddArgs[0].toString(), resA.toString());
    QCOMPARE(pAddArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pAddArgs[2].value<QVariantList>().count(), 1);
    QCOMPARE(pAddArgs[2].value<QVariantList>().first(), QVariant(12));

    QCOMPARE( resWpChSpy.count(), 1 );
    pChArgs = resWpChSpy.takeFirst();
    QCOMPARE(pChArgs[0].toString(), resA.toString());
    QCOMPARE(pChArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pChArgs[2].value<QVariantList>(), QVariantList() << QVariant(12));
    QCOMPARE(pChArgs[3].value<QVariantList>().count(), 0);

    QCOMPARE(propWpAddSpy.count(), 1);
    QCOMPARE(propWpAddSpy.takeFirst(), pAddArgs);
    QCOMPARE(propWpChSpy.count(), 1);
    QCOMPARE(propWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(resPropWpAddSpy.count(), 1);
    QCOMPARE(resPropWpAddSpy.takeFirst(), pAddArgs);
    QCOMPARE(resPropWpChSpy.count(), 1);
    QCOMPARE(resPropWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typeWpAddSpy.count(), 1);
    QCOMPARE(typeWpAddSpy.takeFirst(), pAddArgs);
    QCOMPARE(typeWpChSpy.count(), 1);
    QCOMPARE(typeWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typePropWpAddSpy.count(), 1);
    QCOMPARE(typePropWpAddSpy.takeFirst(), pAddArgs);
    QCOMPARE(typePropWpChSpy.count(), 1);
    QCOMPARE(typePropWpChSpy.takeFirst(), pChArgs);


    // add an already existing value
    // ===============================================================
    m_dmModel->addProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 12, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE( resWpAddSpy.count(), 0 );
    QCOMPARE( resWpChSpy.count(), 0 );
    QCOMPARE(propWpAddSpy.count(), 0);
    QCOMPARE(propWpChSpy.count(), 0);
    QCOMPARE(resPropWpAddSpy.count(), 0);
    QCOMPARE(resPropWpChSpy.count(), 0);
    QCOMPARE(typeWpAddSpy.count(), 0);
    QCOMPARE(typeWpChSpy.count(), 0);
    QCOMPARE(typePropWpAddSpy.count(), 0);
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
    Nepomuk::ResourceWatcherConnection* resW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA, QList<QUrl>(), QList<QUrl>());
    QSignalSpy resWpRemSpy(resW, SIGNAL(propertyRemoved(QString, QString, QVariantList)));
    QSignalSpy resWpChSpy(resW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the property
    Nepomuk::ResourceWatcherConnection* propW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>());
    QSignalSpy propWpRemSpy(propW, SIGNAL(propertyRemoved(QString, QString, QVariantList)));
    QSignalSpy propWpChSpy(propW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the resource and the property
    Nepomuk::ResourceWatcherConnection* resPropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA, QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>());
    QSignalSpy resPropWpRemSpy(resPropW, SIGNAL(propertyRemoved(QString, QString, QVariantList)));
    QSignalSpy resPropWpChSpy(resPropW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the type
    Nepomuk::ResourceWatcherConnection* typeW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>(), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typeWpRemSpy(typeW, SIGNAL(propertyRemoved(QString, QString, QVariantList)));
    QSignalSpy typeWpChSpy(typeW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the type and the property
    Nepomuk::ResourceWatcherConnection* typePropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typePropWpRemSpy(typePropW, SIGNAL(propertyRemoved(QString, QString, QVariantList)));
    QSignalSpy typePropWpChSpy(typePropW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));


    // remove a property
    // ===============================================================
    m_dmModel->removeProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 42, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE( resWpRemSpy.count(), 1 );
    QVariantList pRemArgs = resWpRemSpy.takeFirst();
    QCOMPARE(pRemArgs[0].toString(), resA.toString());
    QCOMPARE(pRemArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pRemArgs[2].value<QVariantList>().count(), 1);
    QCOMPARE(pRemArgs[2].value<QVariantList>().first(), QVariant(42));

    QCOMPARE( resWpChSpy.count(), 1 );
    QVariantList pChArgs = resWpChSpy.takeFirst();
    QCOMPARE(pChArgs[0].toString(), resA.toString());
    QCOMPARE(pChArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pChArgs[2].value<QVariantList>(), QVariantList());
    QCOMPARE(pChArgs[3].value<QVariantList>(), QVariantList() << QVariant(42));

    QCOMPARE(propWpRemSpy.count(), 1);
    QCOMPARE(propWpRemSpy.takeFirst(), pRemArgs);
    QCOMPARE(propWpChSpy.count(), 1);
    QCOMPARE(propWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(resPropWpRemSpy.count(), 1);
    QCOMPARE(resPropWpRemSpy.takeFirst(), pRemArgs);
    QCOMPARE(resPropWpChSpy.count(), 1);
    QCOMPARE(resPropWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typeWpRemSpy.count(), 1);
    QCOMPARE(typeWpRemSpy.takeFirst(), pRemArgs);
    QCOMPARE(typeWpChSpy.count(), 1);
    QCOMPARE(typeWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typePropWpRemSpy.count(), 1);
    QCOMPARE(typePropWpRemSpy.takeFirst(), pRemArgs);
    QCOMPARE(typePropWpChSpy.count(), 1);
    QCOMPARE(typePropWpChSpy.takeFirst(), pChArgs);


    // remove another property (to check that we get an empty list in the changed signal)
    // ===============================================================
    m_dmModel->removeProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 12, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE( resWpRemSpy.count(), 1 );
    pRemArgs = resWpRemSpy.takeFirst();
    QCOMPARE(pRemArgs[0].toString(), resA.toString());
    QCOMPARE(pRemArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pRemArgs[2].value<QVariantList>().count(), 1);
    QCOMPARE(pRemArgs[2].value<QVariantList>().first(), QVariant(12));

    QCOMPARE( resWpChSpy.count(), 1 );
    pChArgs = resWpChSpy.takeFirst();
    QCOMPARE(pChArgs[0].toString(), resA.toString());
    QCOMPARE(pChArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pChArgs[2].value<QVariantList>(), QVariantList());
    QCOMPARE(pChArgs[3].value<QVariantList>(), QVariantList() << QVariant(12));

    QCOMPARE(propWpRemSpy.count(), 1);
    QCOMPARE(propWpRemSpy.takeFirst(), pRemArgs);
    QCOMPARE(propWpChSpy.count(), 1);
    QCOMPARE(propWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(resPropWpRemSpy.count(), 1);
    QCOMPARE(resPropWpRemSpy.takeFirst(), pRemArgs);
    QCOMPARE(resPropWpChSpy.count(), 1);
    QCOMPARE(resPropWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typeWpRemSpy.count(), 1);
    QCOMPARE(typeWpRemSpy.takeFirst(), pRemArgs);
    QCOMPARE(typeWpChSpy.count(), 1);
    QCOMPARE(typeWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typePropWpRemSpy.count(), 1);
    QCOMPARE(typePropWpRemSpy.takeFirst(), pRemArgs);
    QCOMPARE(typePropWpChSpy.count(), 1);
    QCOMPARE(typePropWpChSpy.takeFirst(), pChArgs);


    // remove non-existing property values
    // ===============================================================
    m_dmModel->setProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 42 << 12, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());
    resWpChSpy.clear();
    resWpRemSpy.clear();
    propWpChSpy.clear();
    propWpRemSpy.clear();
    resPropWpChSpy.clear();
    resPropWpRemSpy.clear();
    typeWpChSpy.clear();
    typeWpRemSpy.clear();
    typePropWpChSpy.clear();
    typePropWpRemSpy.clear();
    m_dmModel->removeProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 2, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE(resWpChSpy.count(), 0);
    QCOMPARE(resWpRemSpy.count(), 0);

    QCOMPARE(propWpChSpy.count(), 0);
    QCOMPARE(propWpRemSpy.count(), 0);

    QCOMPARE(resPropWpChSpy.count(), 0);
    QCOMPARE(resPropWpRemSpy.count(), 0);

    QCOMPARE(typeWpChSpy.count(), 0);
    QCOMPARE(typeWpRemSpy.count(), 0);

    QCOMPARE(typePropWpChSpy.count(), 0);
    QCOMPARE(typePropWpRemSpy.count(), 0);


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
    Nepomuk::ResourceWatcherConnection* resW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA, QList<QUrl>(), QList<QUrl>());
    QSignalSpy resWpRemSpy(resW, SIGNAL(propertyRemoved(QString, QString, QVariantList)));
    QSignalSpy resWpChSpy(resW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the property
    Nepomuk::ResourceWatcherConnection* propW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>());
    QSignalSpy propWpRemSpy(propW, SIGNAL(propertyRemoved(QString, QString, QVariantList)));
    QSignalSpy propWpChSpy(propW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the resource and the property
    Nepomuk::ResourceWatcherConnection* resPropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA, QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>());
    QSignalSpy resPropWpRemSpy(resPropW, SIGNAL(propertyRemoved(QString, QString, QVariantList)));
    QSignalSpy resPropWpChSpy(resPropW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the type
    Nepomuk::ResourceWatcherConnection* typeW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>(), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typeWpRemSpy(typeW, SIGNAL(propertyRemoved(QString, QString, QVariantList)));
    QSignalSpy typeWpChSpy(typeW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the type and the property
    Nepomuk::ResourceWatcherConnection* typePropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typePropWpRemSpy(typePropW, SIGNAL(propertyRemoved(QString, QString, QVariantList)));
    QSignalSpy typePropWpChSpy(typePropW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));


    // remove a property
    // ===============================================================
    m_dmModel->removeProperties(QList<QUrl>() << resA, QList<QUrl>() << QUrl("prop:/int"), QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE( resWpRemSpy.count(), 1 );
    QVariantList pRemArgs = resWpRemSpy.takeFirst();
    QCOMPARE(pRemArgs[0].toString(), resA.toString());
    QCOMPARE(pRemArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pRemArgs[2].value<QVariantList>().count(), 2);
    QVERIFY(pRemArgs[2].value<QVariantList>().first() == QVariant(12) || *(++pRemArgs[2].value<QVariantList>().constBegin()) == QVariant(12) );
    QVERIFY(pRemArgs[2].value<QVariantList>().first() == QVariant(42) || *(++pRemArgs[2].value<QVariantList>().constBegin()) == QVariant(42) );

    QCOMPARE( resWpChSpy.count(), 1 );
    QVariantList pChArgs = resWpChSpy.takeFirst();
    QCOMPARE(pChArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pChArgs[2].value<QVariantList>(), QVariantList());
    QCOMPARE(pChArgs[3].value<QVariantList>().count(), 2);
    QVERIFY(pChArgs[3].value<QVariantList>().first() == QVariant(12) || *(++pChArgs[3].value<QVariantList>().constBegin()) == QVariant(12) );
    QVERIFY(pChArgs[3].value<QVariantList>().first() == QVariant(42) || *(++pChArgs[3].value<QVariantList>().constBegin()) == QVariant(42) );

    QCOMPARE(propWpRemSpy.count(), 1);
    QVERIFY(propWpRemSpy.takeFirst() == pRemArgs);
    QCOMPARE(propWpChSpy.count(), 1);
    QCOMPARE(propWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(resPropWpRemSpy.count(), 1);
    QVERIFY(resPropWpRemSpy.takeFirst() == pRemArgs);
    QCOMPARE(resPropWpChSpy.count(), 1);
    QCOMPARE(resPropWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typeWpRemSpy.count(), 1);
    QVERIFY(typeWpRemSpy.takeFirst() == pRemArgs);
    QCOMPARE(typeWpChSpy.count(), 1);
    QCOMPARE(typeWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typePropWpRemSpy.count(), 1);
    QVERIFY(typePropWpRemSpy.takeFirst() == pRemArgs);
    QCOMPARE(typePropWpChSpy.count(), 1);
    QCOMPARE(typePropWpChSpy.takeFirst(), pChArgs);


    // remove non-existing property values
    // ===============================================================
    m_dmModel->removeProperties(QList<QUrl>() << resA, QList<QUrl>() << QUrl("prop:/int"), QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE(resWpChSpy.count(), 0);
    QCOMPARE(resWpRemSpy.count(), 0);

    QCOMPARE(propWpChSpy.count(), 0);
    QCOMPARE(propWpRemSpy.count(), 0);

    QCOMPARE(resPropWpChSpy.count(), 0);
    QCOMPARE(resPropWpRemSpy.count(), 0);

    QCOMPARE(typeWpChSpy.count(), 0);
    QCOMPARE(typeWpRemSpy.count(), 0);

    QCOMPARE(typePropWpChSpy.count(), 0);
    QCOMPARE(typePropWpRemSpy.count(), 0);


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
    Nepomuk::ResourceWatcherConnection* resW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA << resC, QList<QUrl>(), QList<QUrl>());
    QSignalSpy resWpRemSpy(resW, SIGNAL(resourceRemoved(QString, QString, QVariantList)));
}

void ResourceWatcherTest::testCreateResources()
{
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
    Nepomuk::ResourceWatcherConnection* typeW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>(), QList<QUrl>() << QUrl("class:/typeA"));
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
    Nepomuk::ResourceWatcherConnection* resW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resAUri, QList<QUrl>(), QList<QUrl>());
    QSignalSpy resWpAddSpy(resW, SIGNAL(propertyAdded(QString, QString, QVariantList)));
    QSignalSpy resWpChSpy(resW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the property
    Nepomuk::ResourceWatcherConnection* propW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>());
    QSignalSpy propWpAddSpy(propW, SIGNAL(propertyAdded(QString, QString, QVariantList)));
    QSignalSpy propWpChSpy(propW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the resource and the property
    Nepomuk::ResourceWatcherConnection* resPropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resAUri, QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>());
    QSignalSpy resPropWpAddSpy(resPropW, SIGNAL(propertyAdded(QString, QString, QVariantList)));
    QSignalSpy resPropWpChSpy(resPropW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the type
    Nepomuk::ResourceWatcherConnection* typeW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>(), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typeWpAddSpy(typeW, SIGNAL(propertyAdded(QString, QString, QVariantList)));
    QSignalSpy typeWpChSpy(typeW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the type and the property
    Nepomuk::ResourceWatcherConnection* typePropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typePropWpAddSpy(typePropW, SIGNAL(propertyAdded(QString, QString, QVariantList)));
    QSignalSpy typePropWpChSpy(typePropW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));


    // now add some new information to the existing resource
    SimpleResource resA(resAUri);
    resA.addProperty(QUrl("prop:/int"), 12);
    resA.addProperty(QUrl("prop:/int"), 42);
    resA.addProperty(QUrl("prop:/string"), QLatin1String("Hello World"));
    m_dmModel->storeResources(SimpleResourceGraph() << resA, QLatin1String("A"));


    // check the signals
    QCOMPARE( resWpAddSpy.count(), 2 );
    QVariantList pAddArgs1;
    QVariantList pAddArgs2;
    if(resWpAddSpy[0][1].toString() == QLatin1String("prop:/int")) {
        pAddArgs1 = resWpAddSpy[0];
        pAddArgs2 = resWpAddSpy[1];
    }
    else {
        pAddArgs1 = resWpAddSpy[1];
        pAddArgs2 = resWpAddSpy[2];
    }
    resWpAddSpy.clear();
    QCOMPARE(pAddArgs1[0].toString(), resAUri.toString());
    QCOMPARE(pAddArgs1[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pAddArgs1[2].value<QVariantList>().count(), 2);
    QVERIFY(pAddArgs1[2].value<QVariantList>().contains(QVariant(12)));
    QVERIFY(pAddArgs1[2].value<QVariantList>().contains(QVariant(42)));
    QCOMPARE(pAddArgs2[0].toString(), resAUri.toString());
    QCOMPARE(pAddArgs2[1].toString(), QLatin1String("prop:/string"));
    QCOMPARE(pAddArgs2[2].value<QVariantList>(), QVariantList() << QVariant(QLatin1String("Hello World")));

    QCOMPARE( resWpChSpy.count(), 2 );
    QVariantList pChArgs1, pChArgs2;
    if(resWpChSpy[0][1].toString() == QLatin1String("prop:/int")) {
        pChArgs1 = resWpChSpy[0];
        pChArgs2 = resWpChSpy[1];
    }
    else {
        pChArgs1 = resWpChSpy[1];
        pChArgs2 = resWpChSpy[2];
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

    QCOMPARE(propWpAddSpy.count(), 1);
    QCOMPARE(propWpAddSpy.takeFirst(), pAddArgs1);
    QCOMPARE(propWpChSpy.count(), 1);
    QCOMPARE(propWpChSpy.takeFirst(), pChArgs1);

    QCOMPARE(resPropWpAddSpy.count(), 1);
    QCOMPARE(resPropWpAddSpy.takeFirst(), pAddArgs1);
    QCOMPARE(resPropWpChSpy.count(), 1);
    QCOMPARE(resPropWpChSpy.takeFirst(), pChArgs1);

    QCOMPARE(typeWpAddSpy.count(), 2);
    QVERIFY(typeWpAddSpy.contains(pAddArgs1));
    QVERIFY(typeWpAddSpy.contains(pAddArgs2));
    QCOMPARE(typeWpChSpy.count(), 2);
    QVERIFY(typeWpChSpy.contains(pChArgs1));
    QVERIFY(typeWpChSpy.contains(pChArgs2));

    QCOMPARE(typePropWpAddSpy.count(), 2);
    QVERIFY(typePropWpAddSpy.contains(pAddArgs1));
    QVERIFY(typePropWpAddSpy.contains(pAddArgs2));
    QCOMPARE(typePropWpChSpy.count(), 2);
    QVERIFY(typePropWpChSpy.contains(pChArgs1));
    QVERIFY(typePropWpChSpy.contains(pChArgs2));
}

void ResourceWatcherTest::testMergeResources()
{
}


QTEST_KDEMAIN_CORE(ResourceWatcherTest)

#include "resourcewatchertest.moc"
