/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2011 Sebastian Trueg <trueg@kde.org>
   Copyright (C) 2013 Vishesh Handa <me@vhanda.in>

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

#include "asyncclientapitest.h"
#include "simpleresource.h"
#include "simpleresourcegraph.h"
#include "datamanagement.h"
#include "createresourcejob.h"
#include "describeresourcesjob.h"
#include "storeresourcesjob.h"
#include "resourcemanager.h"

#include <QtTest>
#include "qtest_kde.h"

#include <QtDBus>
#include <QProcess>
#include <Soprano/Soprano>

#include <Soprano/Graph>
#define USING_SOPRANO_NRLMODEL_UNSTABLE_API
#include <Soprano/NRLModel>

#include <ktempdir.h>
#include <KDebug>
#include <KJob>

#include "nfo.h"
#include "nmm.h"
#include "nco.h"
#include "nie.h"

using namespace Soprano;
using namespace Soprano::Vocabulary;
using namespace Nepomuk2;
using namespace Nepomuk2::Vocabulary;


void AsyncClientApiTest::testAddProperty()
{
    Soprano::Model *m_model = Nepomuk2::ResourceManager::instance()->mainModel();
    Soprano::NRLModel nrlModel(m_model);
    const QUrl g1 = nrlModel.createGraph(NRL::InstanceBase());

    const QUrl res("nepomuk:/res/A");
    m_model->addStatement(res, RDF::type(), NCO::Contact(), g1);
    
    KJob* job = Nepomuk2::addProperty(QList<QUrl>() << res, NCO::fullname(), QVariantList() << QLatin1String("Name"));
    QVERIFY(QTest::kWaitForSignal(job, SIGNAL(result(KJob*)), 5000));
    QVERIFY(!job->error());

    QVERIFY(m_model->containsAnyStatement(res, NCO::fullname(), LiteralValue("Name")));
}

void AsyncClientApiTest::testSetProperty()
{
    Soprano::Model *m_model = Nepomuk2::ResourceManager::instance()->mainModel();
    Soprano::NRLModel nrlModel(m_model);
    const QUrl g1 = nrlModel.createGraph(NRL::InstanceBase());

    const QUrl res("nepomuk:/res/A");
    m_model->addStatement(res, RDF::type(), NCO::Contact(), g1);

    KJob* job = Nepomuk2::setProperty(QList<QUrl>() << res, NCO::fullname(), QVariantList() << QLatin1String("Name"));
    QVERIFY(QTest::kWaitForSignal(job, SIGNAL(result(KJob*)), 5000));
    QVERIFY(!job->error());

    QVERIFY(m_model->containsAnyStatement(res, NCO::fullname(), LiteralValue("Name")));
}

void AsyncClientApiTest::testCreateResource()
{
    Soprano::Model *m_model = Nepomuk2::ResourceManager::instance()->mainModel();
    CreateResourceJob* job = Nepomuk2::createResource(QList<QUrl>() << NCO::Contact(), QLatin1String("label"), QLatin1String("desc"));
    QVERIFY(QTest::kWaitForSignal(job, SIGNAL(result(KJob*)), 5000));
    QVERIFY(!job->error());

    const QUrl uri = job->resourceUri();
    QVERIFY(!uri.isEmpty());

    QVERIFY(m_model->containsAnyStatement(uri, RDF::type(), NCO::Contact()));
    QVERIFY(m_model->containsAnyStatement(uri, NAO::prefLabel(), LiteralValue::createPlainLiteral(QLatin1String("label"))));
    QVERIFY(m_model->containsAnyStatement(uri, NAO::description(), LiteralValue::createPlainLiteral(QLatin1String("desc"))));
}

void AsyncClientApiTest::testRemoveProperty()
{
    Soprano::Model *m_model = Nepomuk2::ResourceManager::instance()->mainModel();
    Soprano::NRLModel nrlModel(m_model);
    const QUrl g1 = nrlModel.createGraph(NRL::InstanceBase());

    const QUrl res("nepomuk:/res/A");
    m_model->addStatement(res, NCO::fullname(), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(res, NCO::fullname(), LiteralValue(QLatin1String("hello world")), g1);
    m_model->addStatement(res, NAO::lastModified(), LiteralValue(QDateTime::currentDateTime()), g1);

    KJob* job = Nepomuk2::removeProperty(QList<QUrl>() << res, NCO::fullname(), QVariantList() << QLatin1String("hello world"));
    QVERIFY(QTest::kWaitForSignal(job, SIGNAL(result(KJob*)), 5000));
    QVERIFY(!job->error());

    // test that the data has been removed
    QVERIFY(!m_model->containsAnyStatement(res, NCO::fullname(), LiteralValue(QLatin1String("hello world"))));
}

void AsyncClientApiTest::testRemoveResources()
{
    Soprano::Model *m_model = Nepomuk2::ResourceManager::instance()->mainModel();
    Soprano::NRLModel nrlModel(m_model);
    const QUrl g1 = nrlModel.createGraph(NRL::InstanceBase());
    m_model->addStatement(QUrl("res:/A"), RDF::type(), NAO::Tag(), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("foobar")), g1);
    m_model->addStatement(QUrl("res:/A"), QUrl("prop:/string"), LiteralValue(QLatin1String("hello world")), g1);

    KJob* job = Nepomuk2::removeResources(QList<QUrl>() << QUrl("res:/A"), NoRemovalFlags);
    QVERIFY(QTest::kWaitForSignal(job, SIGNAL(result(KJob*)), 5000));
    QVERIFY(!job->error());

    // verify that the resource is gone
    QVERIFY(!m_model->containsAnyStatement(QUrl("res:/A"), Node(), Node()));
}



QTEST_KDEMAIN_CORE(AsyncClientApiTest)

#include "asyncclientapitest.moc"
