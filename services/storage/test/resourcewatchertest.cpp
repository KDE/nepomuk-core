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
    QSignalSpy spy(con, SIGNAL(propertyAdded(QString, QString, QDBusVariant)));

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
    QCOMPARE(args[2].value<QDBusVariant>().variant(), QVariant(QString(QLatin1String("foobar"))));

    // cleanup
    con->deleteLater();
}

void ResourceWatcherTest::testPropertyRemovedSignal()
{
    const QUrl resA = m_dmModel->createResource(QList<QUrl>() << QUrl("class:/typeA"), QLatin1String("foobar"), QString(), QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    Nepomuk::ResourceWatcherConnection* con = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA, QList<QUrl>(), QList<QUrl>());

    QSignalSpy spy(con, SIGNAL(propertyRemoved(QString, QString, QDBusVariant)));

    m_dmModel->removeProperty(QList<QUrl>() << resA, NAO::prefLabel(), QVariantList() << QLatin1String("foobar"), QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE( spy.count(), 1 );

    QList<QVariant> args = spy.takeFirst();

    QCOMPARE(args[0].toString(), resA.toString());
    QCOMPARE(args[1].toString(), NAO::prefLabel().toString());
    QCOMPARE(args[2].value<QDBusVariant>().variant(), QVariant(QString(QLatin1String("foobar"))));

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
    QSignalSpy resWpAddSpy(resW, SIGNAL(propertyAdded(QString, QString, QDBusVariant)));
    QSignalSpy resWpRemSpy(resW, SIGNAL(propertyRemoved(QString, QString, QDBusVariant)));
    QSignalSpy resWpChSpy(resW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the property
    Nepomuk::ResourceWatcherConnection* propW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>());
    QSignalSpy propWpAddSpy(propW, SIGNAL(propertyAdded(QString, QString, QDBusVariant)));
    QSignalSpy propWpRemSpy(propW, SIGNAL(propertyRemoved(QString, QString, QDBusVariant)));
    QSignalSpy propWpChSpy(propW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the resource and the property
    Nepomuk::ResourceWatcherConnection* resPropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA, QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>());
    QSignalSpy resPropWpAddSpy(resPropW, SIGNAL(propertyAdded(QString, QString, QDBusVariant)));
    QSignalSpy resPropWpRemSpy(resPropW, SIGNAL(propertyRemoved(QString, QString, QDBusVariant)));
    QSignalSpy resPropWpChSpy(resPropW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the type
    Nepomuk::ResourceWatcherConnection* typeW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>(), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typeWpAddSpy(typeW, SIGNAL(propertyAdded(QString, QString, QDBusVariant)));
    QSignalSpy typeWpRemSpy(typeW, SIGNAL(propertyRemoved(QString, QString, QDBusVariant)));
    QSignalSpy typeWpChSpy(typeW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the type and the property
    Nepomuk::ResourceWatcherConnection* typePropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typePropWpAddSpy(typePropW, SIGNAL(propertyAdded(QString, QString, QDBusVariant)));
    QSignalSpy typePropWpRemSpy(typePropW, SIGNAL(propertyRemoved(QString, QString, QDBusVariant)));
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
    QCOMPARE(pAddArgs[2].value<QDBusVariant>().variant(), QVariant(42));
    QCOMPARE( resWpRemSpy.count(), 0 );
    QCOMPARE( resWpChSpy.count(), 0 );

    QCOMPARE( propWpAddSpy.count(), 1 );
    QCOMPARE( propWpAddSpy.takeFirst(), pAddArgs );
    QCOMPARE( propWpRemSpy.count(), 0 );
    QCOMPARE( propWpChSpy.count(), 0 );

    QCOMPARE( resPropWpAddSpy.count(), 1 );
    QCOMPARE( resPropWpAddSpy.takeFirst(), pAddArgs );
    QCOMPARE( resPropWpRemSpy.count(), 0 );
    QCOMPARE( resPropWpChSpy.count(), 0 );

    QCOMPARE( typeWpAddSpy.count(), 1 );
    QCOMPARE( typeWpAddSpy.takeFirst(), pAddArgs);
    QCOMPARE( typeWpRemSpy.count(), 0 );
    QCOMPARE( typeWpChSpy.count(), 0 );

    QCOMPARE( typePropWpAddSpy.count(), 1 );
    QCOMPARE( typePropWpAddSpy.takeFirst(), pAddArgs );
    QCOMPARE( typePropWpRemSpy.count(), 0 );
    QCOMPARE( typePropWpChSpy.count(), 0 );


    // set an existing property
    // ===============================================================
    m_dmModel->setProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 12, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE( resWpAddSpy.count(), 1 );
    pAddArgs = resWpAddSpy.takeFirst();
    QCOMPARE(pAddArgs[0].toString(), resA.toString());
    QCOMPARE(pAddArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pAddArgs[2].value<QDBusVariant>().variant(), QVariant(12));

    QCOMPARE( resWpRemSpy.count(), 1 );
    QVariantList pRemArgs = resWpRemSpy.takeFirst();
    QCOMPARE(pRemArgs[0].toString(), resA.toString());
    QCOMPARE(pRemArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pRemArgs[2].value<QDBusVariant>().variant(), QVariant(42));

    QCOMPARE( resWpChSpy.count(), 1 );
    QVariantList pChArgs = resWpChSpy.takeFirst();
    QCOMPARE(pChArgs[0].toString(), resA.toString());
    QCOMPARE(pChArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pChArgs[2].value<QVariantList>(), QVariantList() << QVariant(42));
    QCOMPARE(pChArgs[3].value<QVariantList>(), QVariantList() << QVariant(12));

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
    QCOMPARE(pAddArgs[2].value<QDBusVariant>().variant(), QVariant(42));

    QCOMPARE( resWpRemSpy.count(), 0 );

    QCOMPARE( resWpChSpy.count(), 1 );
    pChArgs = resWpChSpy.takeFirst();
    QCOMPARE(pChArgs[0].toString(), resA.toString());
    QCOMPARE(pChArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pChArgs[2].value<QVariantList>(), QVariantList() << QVariant(12));
    QCOMPARE(pChArgs[3].value<QVariantList>(), QVariantList() << QVariant(12) << QVariant(42));

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
    QCOMPARE(pRemArgs[2].value<QDBusVariant>().variant(), QVariant(42));

    QCOMPARE( resWpChSpy.count(), 1 );
    pChArgs = resWpChSpy.takeFirst();
    QCOMPARE(pChArgs[0].toString(), resA.toString());
    QCOMPARE(pChArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pChArgs[2].value<QVariantList>(), QVariantList() << QVariant(12) << QVariant(42));
    QCOMPARE(pChArgs[3].value<QVariantList>(), QVariantList() << QVariant(12));

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
    QSignalSpy resWpAddSpy(resW, SIGNAL(propertyAdded(QString, QString, QDBusVariant)));
    QSignalSpy resWpChSpy(resW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the property
    Nepomuk::ResourceWatcherConnection* propW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>());
    QSignalSpy propWpAddSpy(propW, SIGNAL(propertyAdded(QString, QString, QDBusVariant)));
    QSignalSpy propWpChSpy(propW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the resource and the property
    Nepomuk::ResourceWatcherConnection* resPropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA, QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>());
    QSignalSpy resPropWpAddSpy(resPropW, SIGNAL(propertyAdded(QString, QString, QDBusVariant)));
    QSignalSpy resPropWpChSpy(resPropW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the type
    Nepomuk::ResourceWatcherConnection* typeW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>(), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typeWpAddSpy(typeW, SIGNAL(propertyAdded(QString, QString, QDBusVariant)));
    QSignalSpy typeWpChSpy(typeW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the type and the property
    Nepomuk::ResourceWatcherConnection* typePropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typePropWpAddSpy(typePropW, SIGNAL(propertyAdded(QString, QString, QDBusVariant)));
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
    QCOMPARE(pAddArgs[2].value<QDBusVariant>().variant(), QVariant(42));
    QCOMPARE( resWpChSpy.count(), 0 );

    QCOMPARE( propWpAddSpy.count(), 1 );
    QCOMPARE( propWpAddSpy.takeFirst(), pAddArgs );
    QCOMPARE( propWpChSpy.count(), 0 );

    QCOMPARE( resPropWpAddSpy.count(), 1 );
    QCOMPARE( resPropWpAddSpy.takeFirst(), pAddArgs );
    QCOMPARE( resPropWpChSpy.count(), 0 );

    QCOMPARE( typeWpAddSpy.count(), 1 );
    QCOMPARE( typeWpAddSpy.takeFirst(), pAddArgs);
    QCOMPARE( typeWpChSpy.count(), 0 );

    QCOMPARE( typePropWpAddSpy.count(), 1 );
    QCOMPARE( typePropWpAddSpy.takeFirst(), pAddArgs );
    QCOMPARE( typePropWpChSpy.count(), 0 );


    // add an existing property
    // ===============================================================
    m_dmModel->addProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 12, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE( resWpAddSpy.count(), 1 );
    pAddArgs = resWpAddSpy.takeFirst();
    QCOMPARE(pAddArgs[0].toString(), resA.toString());
    QCOMPARE(pAddArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pAddArgs[2].value<QDBusVariant>().variant(), QVariant(12));

    QCOMPARE( resWpChSpy.count(), 1 );
    QVariantList pChArgs = resWpChSpy.takeFirst();
    QCOMPARE(pChArgs[0].toString(), resA.toString());
    QCOMPARE(pChArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pChArgs[2].value<QVariantList>(), QVariantList() << QVariant(42));
    QCOMPARE(pChArgs[3].value<QVariantList>(), QVariantList() << QVariant(42) << QVariant(12));

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
    QSignalSpy resWpRemSpy(resW, SIGNAL(propertyRemoved(QString, QString, QDBusVariant)));
    QSignalSpy resWpChSpy(resW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the property
    Nepomuk::ResourceWatcherConnection* propW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>());
    QSignalSpy propWpRemSpy(propW, SIGNAL(propertyRemoved(QString, QString, QDBusVariant)));
    QSignalSpy propWpChSpy(propW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the resource and the property
    Nepomuk::ResourceWatcherConnection* resPropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA, QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>());
    QSignalSpy resPropWpRemSpy(resPropW, SIGNAL(propertyRemoved(QString, QString, QDBusVariant)));
    QSignalSpy resPropWpChSpy(resPropW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the type
    Nepomuk::ResourceWatcherConnection* typeW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>(), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typeWpRemSpy(typeW, SIGNAL(propertyRemoved(QString, QString, QDBusVariant)));
    QSignalSpy typeWpChSpy(typeW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the type and the property
    Nepomuk::ResourceWatcherConnection* typePropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typePropWpRemSpy(typePropW, SIGNAL(propertyRemoved(QString, QString, QDBusVariant)));
    QSignalSpy typePropWpChSpy(typePropW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));


    // remove a property
    // ===============================================================
    m_dmModel->removeProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 42, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE( resWpRemSpy.count(), 1 );
    QVariantList pRemArgs = resWpRemSpy.takeFirst();
    QCOMPARE(pRemArgs[0].toString(), resA.toString());
    QCOMPARE(pRemArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pRemArgs[2].value<QDBusVariant>().variant(), QVariant(42));

    QCOMPARE( resWpChSpy.count(), 1 );
    QVariantList pChArgs = resWpChSpy.takeFirst();
    QCOMPARE(pChArgs[0].toString(), resA.toString());
    QCOMPARE(pChArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pChArgs[2].value<QVariantList>(), QVariantList() << QVariant(12) << QVariant(42));
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


    // remove another property (to check that we get an empty list in the changed signal)
    // ===============================================================
    m_dmModel->removeProperty(QList<QUrl>() << resA, QUrl("prop:/int"), QVariantList() << 12, QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE( resWpRemSpy.count(), 1 );
    pRemArgs = resWpRemSpy.takeFirst();
    QCOMPARE(pRemArgs[0].toString(), resA.toString());
    QCOMPARE(pRemArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pRemArgs[2].value<QDBusVariant>().variant(), QVariant(12));

    QCOMPARE( resWpChSpy.count(), 1 );
    pChArgs = resWpChSpy.takeFirst();
    QCOMPARE(pChArgs[0].toString(), resA.toString());
    QCOMPARE(pChArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pChArgs[2].value<QVariantList>(), QVariantList() << QVariant(12));
    QCOMPARE(pChArgs[3].value<QVariantList>(), QVariantList());

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
    QSignalSpy resWpRemSpy(resW, SIGNAL(propertyRemoved(QString, QString, QDBusVariant)));
    QSignalSpy resWpChSpy(resW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the property
    Nepomuk::ResourceWatcherConnection* propW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>());
    QSignalSpy propWpRemSpy(propW, SIGNAL(propertyRemoved(QString, QString, QDBusVariant)));
    QSignalSpy propWpChSpy(propW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the resource and the property
    Nepomuk::ResourceWatcherConnection* resPropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>() << resA, QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>());
    QSignalSpy resPropWpRemSpy(resPropW, SIGNAL(propertyRemoved(QString, QString, QDBusVariant)));
    QSignalSpy resPropWpChSpy(resPropW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the type
    Nepomuk::ResourceWatcherConnection* typeW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>(), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typeWpRemSpy(typeW, SIGNAL(propertyRemoved(QString, QString, QDBusVariant)));
    QSignalSpy typeWpChSpy(typeW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));

    // a watcher for the type and the property
    Nepomuk::ResourceWatcherConnection* typePropW = m_dmModel->resourceWatcherManager()->createConnection(QList<QUrl>(), QList<QUrl>() << QUrl("prop:/int"), QList<QUrl>() << QUrl("class:/typeA"));
    QSignalSpy typePropWpRemSpy(typePropW, SIGNAL(propertyRemoved(QString, QString, QDBusVariant)));
    QSignalSpy typePropWpChSpy(typePropW, SIGNAL(propertyChanged(QString, QString, QVariantList, QVariantList)));


    // remove a property
    // ===============================================================
    m_dmModel->removeProperties(QList<QUrl>() << resA, QList<QUrl>() << QUrl("prop:/int"), QLatin1String("A"));
    QVERIFY(!m_dmModel->lastError());

    QCOMPARE( resWpRemSpy.count(), 2 );
    QVariantList pRemArgs1 = resWpRemSpy.takeFirst();
    QVariantList pRemArgs2 = resWpRemSpy.takeFirst();
    QCOMPARE(pRemArgs1[0].toString(), resA.toString());
    QCOMPARE(pRemArgs1[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pRemArgs2[0].toString(), resA.toString());
    QCOMPARE(pRemArgs2[1].toString(), QLatin1String("prop:/int"));
    QVERIFY(pRemArgs1[2].value<QDBusVariant>().variant() == QVariant(12) || pRemArgs2[2].value<QDBusVariant>().variant() == QVariant(12) );
    QVERIFY(pRemArgs1[2].value<QDBusVariant>().variant() == QVariant(42) || pRemArgs2[2].value<QDBusVariant>().variant() == QVariant(42) );

    QCOMPARE( resWpChSpy.count(), 1 );
    QVariantList pChArgs = resWpChSpy.takeFirst();
    QCOMPARE(pChArgs[0].toString(), resA.toString());
    QCOMPARE(pChArgs[1].toString(), QLatin1String("prop:/int"));
    QCOMPARE(pChArgs[2].value<QVariantList>(), QVariantList() << QVariant(12) << QVariant(42));
    QCOMPARE(pChArgs[3].value<QVariantList>(), QVariantList());

    QCOMPARE(propWpRemSpy.count(), 2);
    QVERIFY(propWpRemSpy[0] == pRemArgs1);
    QVERIFY(propWpRemSpy[1] == pRemArgs2);
    QCOMPARE(propWpChSpy.count(), 1);
    QCOMPARE(propWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(resPropWpRemSpy.count(), 2);
    QVERIFY(resPropWpRemSpy[0] == pRemArgs1);
    QVERIFY(resPropWpRemSpy[1] == pRemArgs2);
    QCOMPARE(resPropWpChSpy.count(), 1);
    QCOMPARE(resPropWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typeWpRemSpy.count(), 2);
    QVERIFY(typeWpRemSpy[0] == pRemArgs1);
    QVERIFY(typeWpRemSpy[1] == pRemArgs2);
    QCOMPARE(typeWpChSpy.count(), 1);
    QCOMPARE(typeWpChSpy.takeFirst(), pChArgs);

    QCOMPARE(typePropWpRemSpy.count(), 2);
    QVERIFY(typePropWpRemSpy[0] == pRemArgs1);
    QVERIFY(typePropWpRemSpy[1] == pRemArgs2);
    QCOMPARE(typePropWpChSpy.count(), 1);
    QCOMPARE(typePropWpChSpy.takeFirst(), pChArgs);


    // cleanup
    delete resW;
    delete propW;
    delete resPropW;
    delete typeW;
    delete typePropW;
}

void ResourceWatcherTest::testRemoveResources()
{
}

void ResourceWatcherTest::testCreateResources()
{
}

void ResourceWatcherTest::testRemoveDataByApplication()
{
}

void ResourceWatcherTest::testRemoveAllDataByApplication()
{
}

void ResourceWatcherTest::testStoreResources()
{
}

void ResourceWatcherTest::testMergeResources()
{
}


QTEST_KDEMAIN_CORE(ResourceWatcherTest)

#include "resourcewatchertest.moc"
