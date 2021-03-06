/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2011 Sebastian Trueg <trueg@kde.org>

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

#include "datamanagementadaptortest.h"
#include "../datamanagementmodel.h"
#include "../datamanagementadaptor.h"
#include "../classandpropertytree.h"
#include "simpleresource.h"

#include <QtTest>
#include "qtest_kde.h"
#include "qtest_dms.h"

#include <Soprano/Soprano>
#include <Soprano/Graph>
#define USING_SOPRANO_NRLMODEL_UNSTABLE_API
#include <Soprano/NRLModel>

#include <ktempdir.h>
#include <KDebug>

#include "nfo.h"
#include "nmm.h"
#include "nco.h"
#include "nie.h"

using namespace Soprano;
using namespace Soprano::Vocabulary;
using namespace Nepomuk2;
using namespace Nepomuk2::Vocabulary;


void DataManagementAdaptorTest::resetModel()
{
    // remove all the junk from previous tests
    m_model->removeAllStatements();

    // add some classes and properties
    QUrl graph1("graph:/onto1/");
    m_model->addStatement( graph1, RDF::type(), NRL::Ontology(), graph1 );
    m_model->addStatement( graph1, NAO::hasDefaultNamespace(), QUrl("graph:/onto1/"), graph1 );
    m_model->addStatement( graph1, NAO::hasDefaultNamespaceAbbreviation(), LiteralValue(QLatin1String("itda")), graph1 );

    QUrl graph2("graph:/onto2#");
    m_model->addStatement( graph2, RDF::type(), NRL::Ontology(), graph2 );
    m_model->addStatement( graph2, NAO::hasDefaultNamespace(), QUrl("graph:/onto2#"), graph1 );
    m_model->addStatement( graph2, NAO::hasDefaultNamespaceAbbreviation(), LiteralValue(QLatin1String("wbzo")), graph1 );


    m_model->addStatement( QUrl("graph:/onto1/P1"), RDF::type(), RDF::Property(), graph1 );
    m_model->addStatement( QUrl("graph:/onto1/P2"), RDF::type(), RDF::Property(), graph1 );
    m_model->addStatement( QUrl("graph:/onto1/T1"), RDFS::Class(), RDF::Property(), graph1 );

    m_model->addStatement( QUrl("graph:/onto2#P1"), RDF::type(), RDF::Property(), graph2 );
    m_model->addStatement( QUrl("graph:/onto2#P2"), RDF::type(), RDF::Property(), graph2 );
    m_model->addStatement( QUrl("graph:/onto2#T1"), RDFS::Class(), RDF::Property(), graph2 );


    m_classAndPropertyTree->rebuildTree(m_dmModel);
    m_nrlModel->setEnableQueryPrefixExpansion(false);
    m_nrlModel->setEnableQueryPrefixExpansion(true);
    m_dmModel->clearCache();
    QHash<QString, QString> prefixes;
    const QHash<QString, QUrl> namespaces = m_nrlModel->queryPrefixes();
    for(QHash<QString, QUrl>::const_iterator it = namespaces.constBegin();
        it != namespaces.constEnd(); ++it) {
        prefixes.insert(it.key(), QString::fromAscii(it.value().toEncoded()));
    }
    m_dmAdaptor->setPrefixes(prefixes);
}

void DataManagementAdaptorTest::initTestCase()
{
    const Soprano::Backend* backend = Soprano::PluginManager::instance()->discoverBackendByName( "virtuosobackend" );
    QVERIFY( backend );
    m_storageDir = new KTempDir();
    m_model = backend->createModel( Soprano::BackendSettings() << Soprano::BackendSetting(Soprano::BackendOptionStorageDir, m_storageDir->name()) );
    QVERIFY( m_model );

    // DataManagementModel relies on the ussage of a NRLModel in the storage service
    m_nrlModel = new Soprano::NRLModel(m_model);
    Nepomuk2::insertNamespaceAbbreviations(m_model);
    m_nrlModel->setEnableQueryPrefixExpansion( true );

    m_classAndPropertyTree = new Nepomuk2::ClassAndPropertyTree(this);

    m_dmModel = new Nepomuk2::DataManagementModel(m_classAndPropertyTree, m_nrlModel);

    m_dmAdaptor = new Nepomuk2::DataManagementAdaptor(m_dmModel);
}

void DataManagementAdaptorTest::cleanupTestCase()
{
    delete m_dmAdaptor;
    delete m_dmModel;
    delete m_nrlModel;
    delete m_model;
    delete m_storageDir;
}

void DataManagementAdaptorTest::init()
{
    resetModel();
}

void DataManagementAdaptorTest::testNamespaceExpansion()
{
    QCOMPARE(m_dmAdaptor->decodeUri(QLatin1String("itda:P1"), true), QUrl("graph:/onto1/P1"));
    QCOMPARE(m_dmAdaptor->decodeUri(QLatin1String("itda:T1"), true), QUrl("graph:/onto1/T1"));
    QCOMPARE(m_dmAdaptor->decodeUri(QLatin1String("wbzo:P1"), true), QUrl("graph:/onto2#P1"));
    QCOMPARE(m_dmAdaptor->decodeUri(QLatin1String("wbzo:T1"), true), QUrl("graph:/onto2#T1"));
}


QTEST_KDEMAIN_CORE(DataManagementAdaptorTest)

#include "datamanagementadaptortest.moc"
