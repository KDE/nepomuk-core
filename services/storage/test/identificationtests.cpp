/*
 * <one line to give the library's name and an idea of what it does.>
 * Copyright 2013  Vishesh Handa <me@vhanda.in>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "identificationtests.h"
#include "../datamanagementmodel.h"
#include "../classandpropertytree.h"
#include "../virtuosoinferencemodel.h"
#include "simpleresource.h"
#include "simpleresourcegraph.h"

#include <QtTest>
#include "qtest_kde.h"
#include "qtest_dms.h"

#include <Soprano/Soprano>
#include <Soprano/Graph>
#define USING_SOPRANO_NRLMODEL_UNSTABLE_API
#include <Soprano/NRLModel>

#include <KTemporaryFile>
#include <KTempDir>
#include <KProtocolInfo>
#include <KDebug>

#include "nfo.h"
#include "nmm.h"
#include "nco.h"
#include "nie.h"
#include "nmo.h"
#include "resourcemanager.h"

using namespace Soprano;
using namespace Soprano::Vocabulary;
using namespace Nepomuk2;
using namespace Nepomuk2::Vocabulary;


void IdentificationTests::resetModel()
{
    // remove all the junk from previous tests
    m_model->removeAllStatements();

    // add some classes and properties
    QUrl graph("graph:/onto");
    Nepomuk2::insertOntologies( m_model, graph );

    // rebuild the internals of the data management model
    m_classAndPropertyTree->rebuildTree(m_dmModel);
    m_inferenceModel->updateOntologyGraphs(true);
    m_dmModel->clearCache();
}

void IdentificationTests::initTestCase()
{
    const Soprano::Backend* backend = Soprano::PluginManager::instance()->discoverBackendByName( "virtuosobackend" );
    QVERIFY( backend );
    m_storageDir = new KTempDir();
    m_model = backend->createModel( Soprano::BackendSettings() << Soprano::BackendSetting(Soprano::BackendOptionStorageDir, m_storageDir->name()) );
    QVERIFY( m_model );

    // DataManagementModel relies on the usage of a NRLModel in the storage service
    m_nrlModel = new Soprano::NRLModel(m_model);
    Nepomuk2::insertNamespaceAbbreviations( m_model );

    m_classAndPropertyTree = new Nepomuk2::ClassAndPropertyTree(this);
    m_inferenceModel = new Nepomuk2::VirtuosoInferenceModel(m_nrlModel);
    m_dmModel = new Nepomuk2::DataManagementModel(m_classAndPropertyTree, m_inferenceModel);
}


void IdentificationTests::cleanupTestCase()
{
    delete m_dmModel;
    delete m_inferenceModel;
    delete m_nrlModel;
    delete m_model;
    delete m_storageDir;
    delete m_classAndPropertyTree;
}

void IdentificationTests::init()
{
    resetModel();
}

void IdentificationTests::testContact()
{
    SimpleResource res;
    res.addType( NCO::Contact() );
    res.addProperty( NCO::fullname(), QLatin1String("Peter Parker") );

    SimpleResourceGraph graph;
    graph << res;

    QHash< QUrl, QUrl > map = m_dmModel->storeResources( graph, QLatin1String("app") );
    const QUrl resUri = map.value( res.uri() );

    // Pushing it again should get it identified to the exact same resource
    map = m_dmModel->storeResources( graph, QLatin1String("app") );
    const QUrl resUri2 = map.value( res.uri() );

    QCOMPARE( resUri, resUri2 );
}

void IdentificationTests::testContact_extraProperty()
{
    SimpleResource res;
    res.addType( NCO::Contact() );
    res.addProperty( NCO::fullname(), QLatin1String("Peter Parker") );

    SimpleResourceGraph graph;
    graph << res;

    QHash< QUrl, QUrl > map = m_dmModel->storeResources( graph, QLatin1String("app") );
    const QUrl resUri = map.value( res.uri() );

    // Try with an extra property with a resource range
    SimpleResource res2;
    res2.addType( NCO::Contact() );
    res2.addProperty( NCO::fullname(), QLatin1String("Peter Parker") );
    res2.addProperty( NCO::gender(), NCO::male() );

    map = m_dmModel->storeResources( SimpleResourceGraph() << res2, QLatin1String("app") );
    const QUrl resUri2 = map.value( res2.uri() );

    QCOMPARE( resUri, resUri2 );

    // Try with an extra property with a literal range
    SimpleResource res3;
    res3.addType( NCO::Contact() );
    res3.addProperty( NCO::fullname(), QLatin1String("Peter Parker") );
    res3.addProperty( NCO::nickname(), QLatin1String("Spidey") );

    map = m_dmModel->storeResources( SimpleResourceGraph() << res3, QLatin1String("app") );
    const QUrl resUri3 = map.value( res3.uri() );

    QCOMPARE( resUri, resUri3 );

    // Try with 2 extra properties with a literal range
    SimpleResource res4;
    res4.addType( NCO::Contact() );
    res4.addProperty( NCO::fullname(), QLatin1String("Peter Parker") );
    res4.addProperty( NCO::nickname(), QLatin1String("Spidey") );
    res4.addProperty( NCO::birthDate(), QDate::currentDate() );

    map = m_dmModel->storeResources( SimpleResourceGraph() << res4, QLatin1String("app") );
    const QUrl resUri4 = map.value( res4.uri() );

    QCOMPARE( resUri, resUri4 );

    // Try with an extra literal and resource property
    SimpleResource res5;
    res5.addType( NCO::Contact() );
    res5.addProperty( NCO::fullname(), QLatin1String("Peter Parker") );
    res5.addProperty( NCO::nickname(), QLatin1String("Spidey") );
    res5.addProperty( NCO::gender(), NCO::male() );

    map = m_dmModel->storeResources( SimpleResourceGraph() << res5, QLatin1String("app") );
    const QUrl resUri5 = map.value( res5.uri() );

    QCOMPARE( resUri, resUri5 );

    // Make sure it is not identified if a literal term does not match
    SimpleResource res6;
    res6.addType( NCO::Contact() );
    res6.addProperty( NCO::fullname(), QLatin1String("Peter Parker") );
    res6.addProperty( NCO::birthDate(), QDate(2001, 02, 02) );

    map = m_dmModel->storeResources( SimpleResourceGraph() << res6, QLatin1String("app") );
    const QUrl resUri6 = map.value( res6.uri() );

    QVERIFY( resUri != resUri6 );
}

void IdentificationTests::testContact_differentTypes()
{
    SimpleResource res;
    res.addType( NCO::PersonContact() );
    res.addProperty( NCO::fullname(), QLatin1String("Peter Parker") );

    QHash< QUrl, QUrl > map = m_dmModel->storeResources( SimpleResourceGraph() << res, QLatin1String("app") );
    const QUrl resUri = map.value( res.uri() );

    // Exact same, but this time with a base type
    SimpleResource res2;
    res2.addType( NCO::Contact() );
    res2.addProperty( NCO::fullname(), QLatin1String("Peter Parker") );

    map = m_dmModel->storeResources( SimpleResourceGraph() << res2, QLatin1String("app") );
    const QUrl resUri2 = map.value( res2.uri() );

    QCOMPARE( resUri, resUri2 );
}

void IdentificationTests::testContact_sameUID()
{
    SimpleResource res;
    res.addType( NCO::PersonContact() );
    res.addProperty( NCO::fullname(), QLatin1String("Peter Parker") );
    res.addProperty( NCO::nickname(), QLatin1String("WebHead") );
    res.addProperty( NCO::contactUID(), QLatin1String("contact-UID") );

    QHash< QUrl, QUrl > mappings = m_dmModel->storeResources(SimpleResourceGraph() << res, "app");
    QUrl resUri = mappings.value(res.uri());
    QVERIFY(!m_model->lastError());

    SimpleResource res2;
    res2.addType( NCO::PersonContact() );
    res2.addProperty( NCO::nickname(), QLatin1String("Spidey") );
    res2.addProperty( NCO::contactUID(), QLatin1String("contact-UID") );

    QHash< QUrl, QUrl > mappings2 = m_dmModel->storeResources(SimpleResourceGraph() << res, "app");
    QUrl resUri2 = mappings2.value(res2.uri());
    QVERIFY(!m_model->lastError());

    // They should be the same even though the nicknames are different
    QCOMPARE(resUri, resUri2);
}

QTEST_KDEMAIN_CORE(IdentificationTests)
