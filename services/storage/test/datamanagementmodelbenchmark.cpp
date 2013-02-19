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

#include "datamanagementmodelbenchmark.h"
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


void DataManagementModelBenchmark::resetModel()
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

void DataManagementModelBenchmark::initTestCase()
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


void DataManagementModelBenchmark::cleanupTestCase()
{
    delete m_dmModel;
    delete m_inferenceModel;
    delete m_nrlModel;
    delete m_model;
    delete m_storageDir;
    delete m_classAndPropertyTree;
}

void DataManagementModelBenchmark::init()
{
    resetModel();
}

void DataManagementModelBenchmark::addProperty()
{
    const QString resTemp("nepomuk:/res/");
    int i = 0;

    QBENCHMARK {
        QUrl res( resTemp + QString::number(i) );
        i++;

        QList<QUrl> urlList;
        urlList << res;


        m_dmModel->addProperty( urlList, QUrl("prop:/string"), QVariantList() << "Boo1", QLatin1String("A") );
        m_dmModel->addProperty( urlList, QUrl("prop:/string"), QVariantList() << "Boo2", QLatin1String("B") );
        m_dmModel->addProperty( urlList, QUrl("prop:/string"), QVariantList() << "Boo3", QLatin1String("C") );
        m_dmModel->addProperty( urlList, QUrl("prop:/string"), QVariantList() << "Boo4", QLatin1String("D") );
    }
}

void DataManagementModelBenchmark::addProperty_sameData()
{
    const QString resTemp("nepomuk:/res/");
    int i = 0;

    QBENCHMARK {
        QUrl res( resTemp + QString::number(i) );
        i++;

        QList<QUrl> urlList;
        urlList << res;


        m_dmModel->addProperty( urlList, QUrl("prop:/string"), QVariantList() << "Boo", QLatin1String("A") );
        m_dmModel->addProperty( urlList, QUrl("prop:/string"), QVariantList() << "Boo", QLatin1String("B") );
        m_dmModel->addProperty( urlList, QUrl("prop:/string"), QVariantList() << "Boo", QLatin1String("C") );
        m_dmModel->addProperty( urlList, QUrl("prop:/string"), QVariantList() << "Boo", QLatin1String("D") );
    }
}

void DataManagementModelBenchmark::setProperty()
{
    const QString resTemp("nepomuk:/res/");
    int i = 0;

    QBENCHMARK {
        QUrl res( resTemp + QString::number(i) );
        i++;

        QList<QUrl> urlList;
        urlList << res;


        m_dmModel->setProperty( urlList, QUrl("prop:/string"), QVariantList() << "Boo1", QLatin1String("A") );
        m_dmModel->setProperty( urlList, QUrl("prop:/string"), QVariantList() << "Boo2", QLatin1String("B") );
        m_dmModel->setProperty( urlList, QUrl("prop:/string"), QVariantList() << "Boo3", QLatin1String("C") );
        m_dmModel->setProperty( urlList, QUrl("prop:/string"), QVariantList() << "Boo4", QLatin1String("D") );
    }
}

void DataManagementModelBenchmark::setProperty_sameData()
{
    const QString resTemp("nepomuk:/res/");
    int i = 0;

    QBENCHMARK {
        QUrl res( resTemp + QString::number(i) );
        i++;

        QList<QUrl> urlList;
        urlList << res;

        QVariantList vl;
        vl << "Boo1";

        m_dmModel->setProperty( urlList, QUrl("prop:/string"), vl, QLatin1String("A") );

        vl << "Boo2";
        m_dmModel->setProperty( urlList, QUrl("prop:/string"), vl, QLatin1String("B") );

        vl << "Boo3";
        m_dmModel->setProperty( urlList, QUrl("prop:/string"), vl, QLatin1String("C") );

        vl << "Boo4";
        m_dmModel->setProperty( urlList, QUrl("prop:/string"), vl, QLatin1String("D") );
    }
}

void DataManagementModelBenchmark::storeResources()
{
    SimpleResource res;
    res.addType( NCO::Contact() );
    res.addProperty( NCO::fullname(), QLatin1String("Boo") );
    res.addProperty( QUrl("prop:/string"), QLatin1String("Goo") );
    res.addProperty( QUrl("prop:/string"), QLatin1String("lho") );
    res.addProperty( QUrl("prop:/int"), 42 );
    res.addProperty( QUrl("prop:/int2"), 42 );
    res.addProperty( QUrl("prop:/int3"), 42 );
    res.addProperty( QUrl("prop:/date"), QDateTime::currentDateTime() );

    QBENCHMARK {
        QUrl uri = m_dmModel->createResource( QList<QUrl>(), QString(), QString(), "A" );
        res.setUri( uri );

        SimpleResourceGraph graph;
        graph << res;

        m_dmModel->storeResources( graph, "A" );
    }
}

namespace {
    SimpleResource createHeader(const QString& name, const QString &value, SimpleResourceGraph& graph) {
        SimpleResource res;
        res.addType( NMO::MessageHeader() );
        res.addProperty( NMO::headerName(), name );
        res.addProperty( NMO::headerValue(), value );

        graph << res;
        return res;
    }

    QUrl createContact(const QString& name, const QString& email, SimpleResourceGraph& graph) {
        SimpleResource emRes;
        emRes.addType( NCO::EmailAddress() );
        emRes.addProperty( NCO::emailAddress(), email );

        SimpleResource res;
        res.addType( NCO::Contact() );
        res.addProperty( NCO::fullname(), name );
        res.addProperty( NCO::hasEmailAddress(), emRes );

        graph << emRes << res;
        return res.uri();
    }

    QUrl createIcon(const QString& iconName, SimpleResourceGraph& graph) {
        SimpleResource iconRes;
        iconRes.addType( NAO::FreeDesktopIcon() );
        iconRes.addProperty( NAO::iconName(), iconName );

        graph << iconRes;
        return iconRes.uri();
    }

    QUrl createTag(const QString& identifier, const QString& label, SimpleResourceGraph& graph) {
        SimpleResource tagRes;
        tagRes.addType( NAO::Tag() );
        tagRes.addProperty( NAO::identifier(), identifier );
        tagRes.addProperty( NAO::prefLabel(), label );

        graph << tagRes;
        return tagRes.uri();
    }
}

void DataManagementModelBenchmark::storeResources_email()
{
    SimpleResourceGraph graph;

    SimpleResource res;
    res.addType( NMO::Email() );
    res.setProperty( NIE::byteSize(), 10 );
    res.setProperty( NMO::isRead(), QVariant(true) );
    res.setProperty( NMO::plainTextMessageContent(), QLatin1String("This is a test email") );
    res.setProperty( NAO::prefLabel(), QLatin1String("Email Subject") );
    res.setProperty( NMO::sentDate(), QDateTime::currentDateTime() );
    res.setProperty( NMO::messageId(), QLatin1String("message-id") );

    QStringList headers;
    headers << "List-Id" << "X-Loop" << "X-MailingList" << "X-Spam-Flag" << "Organization";

    foreach(const QString& head, headers) {
        SimpleResource headRes = createHeader( head, "Don't care about the value", graph );
        res.addProperty( NMO::messageHeader(), headRes );
    }

    // Contacts
    res.addProperty( NMO::from(), createContact("FromCon", "from@contact.org", graph ) );
    res.addProperty( NMO::to(), createContact("ToCon", "to@contact.org", graph ) );
    res.addProperty( NMO::bcc(), createContact("BccCon", "bcc@contact.org", graph ) );
    res.addProperty( NMO::cc(), createContact("ccCon", "cc@contact.org", graph ) );

    res.addProperty( NAO::prefSymbol(), createIcon("internet-mail", graph ) );

    res.addProperty( NAO::hasTag(), createTag( "mail-mark-important", "Important", graph ) );
    res.addProperty( NAO::hasTag(), createTag( "mail-mark-task", "TODO", graph ) );
    res.addProperty( NAO::hasTag(), createTag( "mail-mark-junk", "SPAM", graph ) );

    graph << res;

    QBENCHMARK {
        m_dmModel->storeResources( graph, "TestApp", Nepomuk2::IdentifyNone, Nepomuk2::NoStoreResourcesFlags );
    }
}

void DataManagementModelBenchmark::createResource()
{
    QList<QUrl> types;
    types << NCO::Contact();

    QBENCHMARK {
        m_dmModel->createResource( types, QString(), QString(), QString("app") );
    }
}

void DataManagementModelBenchmark::removeResources()
{
    QList<QUrl> types;
    types << NCO::Contact();

    QBENCHMARK {
        QUrl uri = m_dmModel->createResource( types, QString(), QString(), QString("app") );
        m_dmModel->removeResources( QList<QUrl>() << uri, NoRemovalFlags, QLatin1String("app") );
    }
}

void DataManagementModelBenchmark::removeDataByApplication()
{
    SimpleResource temp;
    temp.addType( NCO::PersonContact() );
    temp.addProperty( NCO::fullname(), QLatin1String("Peter Parker") );

    SimpleResourceGraph graph;
    for( int i=0; i<10; i++ ) {
        SimpleResource res;
        QUrl uri = res.uri();
        res = temp;
        res.setUri( uri );

        graph << res;
    }

    QBENCHMARK {
        QHash<QUrl, QUrl> mappings = m_dmModel->storeResources( graph, "app", Nepomuk2::IdentifyNone );
        m_dmModel->removeDataByApplication( mappings.values(), NoRemovalFlags, QLatin1String("app") );
    }
}

void DataManagementModelBenchmark::removeDataByApplication_subResources()
{
    SimpleResource temp;
    temp.addType( NCO::PersonContact() );
    temp.addProperty( NCO::fullname(), QLatin1String("Peter Parker") );

    SimpleResourceGraph graph;
    for( int i=0; i<10; i++ ) {
        SimpleResource res;
        QUrl uri = res.uri();
        res = temp;
        res.setUri( uri );

        graph << res;
    }

    QBENCHMARK {
        QHash<QUrl, QUrl> mappings = m_dmModel->storeResources( graph, "app", Nepomuk2::IdentifyNone );
        m_dmModel->removeDataByApplication( mappings.values(), RemoveSubResoures, QLatin1String("app") );
    }
}

void DataManagementModelBenchmark::removeAllDataByApplication()
{
    SimpleResource temp;
    temp.addType( NCO::PersonContact() );
    temp.addProperty( NCO::fullname(), QLatin1String("Peter Parker") );

    SimpleResourceGraph graph;
    for( int i=0; i<10; i++ ) {
        SimpleResource res;
        QUrl uri = res.uri();
        res = temp;
        res.setUri( uri );

        graph << res;
    }

    QBENCHMARK {
        m_dmModel->storeResources( graph, "app", Nepomuk2::IdentifyNone );
        m_dmModel->removeDataByApplication( NoRemovalFlags, QLatin1String("app") );
    }
}


QTEST_KDEMAIN_CORE(DataManagementModelBenchmark)
