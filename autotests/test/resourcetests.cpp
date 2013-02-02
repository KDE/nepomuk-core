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


#include "resourcetests.h"

#include "nco.h"
#include "nie.h"
#include "nfo.h"
#include "pimo.h"

#include "resource.h"
#include "variant.h"
#include "tag.h"
#include "datamanagement.h"
#include "resourcemanager.h"

#include <KDebug>
#include <KTemporaryFile>
#include <KJob>
#include <KTempDir>
#include <qtest_kde.h>

#include <Soprano/Vocabulary/RDF>
#include <Soprano/Vocabulary/NAO>
#include <Soprano/Statement>
#include <Soprano/Model>
#include <Soprano/StatementIterator>


using namespace Soprano::Vocabulary;
using namespace Nepomuk2::Vocabulary;

namespace Nepomuk2 {

void ResourceTests::newTag()
{
    Resource tag("Test", NAO::Tag());
    QVERIFY(!tag.exists());

    //QCOMPARE(tag.label(), QString("Test"));
    QVERIFY(!tag.exists());
    QVERIFY(tag.uri().isEmpty());

    // Resources are only saved when some changes are made
    // FIXME: Maybe add some save function?
    {
        Resource res;
        res.addTag( tag );
        res.remove();
    }

    QVERIFY(tag.exists());
    QVERIFY(!tag.uri().isEmpty());
    QCOMPARE(tag.type(), NAO::Tag());

    // Check the statements
    Soprano::Model* model = ResourceManager::instance()->mainModel();

    // One for <resUri> nao:lastModified ..
    //         <resUri> nao:created ..
    //         <resUri> rdf:type nao:Tag
    //         <resUri> nao:identifier "Test"
    const QUrl uri = tag.uri();
    QVERIFY(model->containsAnyStatement(uri, NAO::lastModified(), Soprano::Node()));
    QVERIFY(model->containsAnyStatement(uri, NAO::created(), Soprano::Node()));
    QVERIFY(model->containsAnyStatement(uri, RDF::type(), NAO::Tag()));
    QVERIFY(model->containsAnyStatement(uri, NAO::identifier(), Soprano::LiteralValue("Test")));
    QVERIFY(model->containsAnyStatement(uri, NAO::prefLabel(), Soprano::LiteralValue::createPlainLiteral("Test")));

    QList<Soprano::Statement> stList = model->listStatements( tag.uri(), Soprano::Node(),
                                                              Soprano::Node() ).allStatements();
    QCOMPARE(stList.size(), 5);
}

void ResourceTests::newContact()
{
    Resource con( QUrl(), NCO::Contact() );
    QVERIFY(!con.exists());
    QVERIFY(con.properties().empty());
    QVERIFY(con.uri().isEmpty());

    QString martin("Martin Klapetek");
    con.setProperty(NCO::fullname(), martin);
    QVERIFY(!con.type().isEmpty());
    QCOMPARE(con.property(NCO::fullname()).toString(), martin);

    // Cross check the statements
    Soprano::Model* model = ResourceManager::instance()->mainModel();

    // One for <resUri> nao:lastModified ..
    //         <resUri> nao:created ..
    //         <resUri> rdf:type nco:Contact
    //         <resUri> nco:fullname "Martin Klapetek"
    const QUrl uri = con.uri();
    QVERIFY(model->containsAnyStatement(uri, NAO::lastModified(), Soprano::Node()));
    QVERIFY(model->containsAnyStatement(uri, NAO::created(), Soprano::Node()));
    QVERIFY(model->containsAnyStatement(uri, RDF::type(), NCO::Contact()));
    QVERIFY(model->containsAnyStatement(uri, NCO::fullname(), Soprano::LiteralValue(martin)));

    QList<Soprano::Statement> stList = model->listStatements( con.uri(), Soprano::Node(),
                                                              Soprano::Node() ).allStatements();
    QCOMPARE(stList.size(), 4);
}

void ResourceTests::newFile()
{
    KTemporaryFile tempFile;
    QVERIFY(tempFile.open());

    QUrl fileUrl = QUrl::fromLocalFile(tempFile.fileName());

    Resource fileRes( tempFile.fileName() );
    QVERIFY(!fileRes.exists());
    QVERIFY(fileRes.uri().isEmpty());
    QCOMPARE(fileRes.property(NIE::url()).toUrl(), fileUrl);

    // Save the resource
    fileRes.setRating( 5 );
    QVERIFY(fileRes.rating() == 5);
    fileRes.removeProperty(NAO::numericRating());
    QVERIFY(!fileRes.hasProperty(NAO::numericRating()));

    QVERIFY(!fileRes.uri().isEmpty());
    QVERIFY(fileRes.exists());

    // Cross check the statements
    Soprano::Model* model = ResourceManager::instance()->mainModel();

    // One for <resUri> nao:lastModified ..
    //         <resUri> nao:created ..
    //         <resUri> rdf:type nfo:FileDataObject
    //         <resUri> nie:url fileUrl
    const QUrl uri = fileRes.uri();
    QVERIFY(model->containsAnyStatement(uri, NAO::lastModified(), Soprano::Node()));
    QVERIFY(model->containsAnyStatement(uri, NAO::created(), Soprano::Node()));
    QVERIFY(model->containsAnyStatement(uri, RDF::type(), NFO::FileDataObject()));
    QVERIFY(model->containsAnyStatement(uri, NIE::url(), fileUrl));

    QList<Soprano::Statement> stList = model->listStatements( uri, Soprano::Node(),
                                                              Soprano::Node() ).allStatements();
    QCOMPARE(stList.size(), 4);
}

void ResourceTests::newFolder()
{
    KTempDir tempDir;
    QVERIFY(tempDir.exists());
    QUrl dirUrl = QUrl::fromLocalFile(tempDir.name());

    Resource dirRes( dirUrl );
    QVERIFY(!dirRes.exists());
    QVERIFY(dirRes.uri().isEmpty());
    QCOMPARE(dirRes.property(NIE::url()).toUrl(), dirUrl);

    // Save the resource
    dirRes.setRating( 5 );
    QVERIFY(dirRes.rating() == 5);
    dirRes.removeProperty(NAO::numericRating());
    QVERIFY(!dirRes.hasProperty(NAO::numericRating()));

    QVERIFY(!dirRes.uri().isEmpty());
    QVERIFY(dirRes.exists());

    // Cross check the statements
    Soprano::Model* model = ResourceManager::instance()->mainModel();

    // One for <resUri> nao:lastModified ..
    //         <resUri> nao:created ..
    //         <resUri> rdf:type nfo:FileDataObject
    //         <resUri> rdf:type nfo:Folder
    //         <resUri> nie:url url
    const QUrl uri = dirRes.uri();
    QVERIFY(model->containsAnyStatement(uri, NAO::lastModified(), Soprano::Node()));
    QVERIFY(model->containsAnyStatement(uri, NAO::created(), Soprano::Node()));
    QVERIFY(model->containsAnyStatement(uri, RDF::type(), NFO::Folder()));
    QVERIFY(model->containsAnyStatement(uri, RDF::type(), NFO::FileDataObject()));
    QVERIFY(model->containsAnyStatement(uri, NIE::url(), dirUrl));

    QList<Soprano::Statement> stList = model->listStatements( uri, Soprano::Node(),
                                                              Soprano::Node() ).allStatements();
    QCOMPARE(stList.size(), 5);
}

void ResourceTests::newResourceMetaProperties()
{
    Resource res(QUrl(), NCO::Contact());
    QVERIFY(!res.exists());
    QVERIFY(res.uri().isEmpty());

    QString name("Contact Name");
    res.setProperty(NCO::fullname(), name);

    QVERIFY(res.exists());
    QEXPECT_FAIL("", "Meta properties are currently not loaded", Abort);
    QVERIFY(res.hasProperty(NAO::lastModified()));
    QVERIFY(res.hasProperty(NAO::created()));
}


void ResourceTests::existingTag()
{
    // Create the tag
    QUrl tagUri;
    {
        Tag t("Tag");
        QVERIFY(!t.exists());
        QVERIFY(t.uri().isEmpty());

        // Save the tag
        Resource res;
        res.addTag(t);
        res.remove();

        QVERIFY(t.exists());
        QVERIFY(!t.uri().isEmpty());
        tagUri = t.uri();
    }

    Tag t("Tag");
    QVERIFY(t.exists());
    QCOMPARE(t.uri(), tagUri);
    QCOMPARE(t.property(NAO::identifier()).toString(), QLatin1String("Tag"));
}

void ResourceTests::existingFile()
{
    KTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    QUrl fileUrl = QUrl::fromLocalFile(tempFile.fileName());

    QUrl fileUri;
    {
        Resource fileRes( fileUrl );
        QVERIFY(!fileRes.exists());
        QVERIFY(fileRes.uri().isEmpty());
        QCOMPARE(fileRes.property(NIE::url()).toUrl(), fileUrl);

        // Save the resource
        fileRes.setRating( 5 );
        QVERIFY(fileRes.rating() == 5);
        fileRes.removeProperty(NAO::numericRating());
        QVERIFY(!fileRes.hasProperty(NAO::numericRating()));
        fileUri = fileRes.uri();
    }

    {
        Resource fileRes( fileUrl );
        QVERIFY(fileRes.exists());
        QCOMPARE(fileRes.uri(), fileUri);
        QCOMPARE(fileRes.property(NIE::url()).toUrl(), fileUrl);
    }

    {
        Resource fileRes( fileUri );
        QVERIFY(fileRes.exists());
        QCOMPARE(fileRes.uri(), fileUri);
        QCOMPARE(fileRes.property(NIE::url()).toUrl(), fileUrl);
    }
}

void ResourceTests::existingContact()
{
    QUrl contactUri;
    QString martin("Martin Klapetek");

    {
        Resource con( QUrl(), NCO::Contact() );
        QVERIFY(!con.exists());
        QVERIFY(con.properties().empty());
        QVERIFY(con.uri().isEmpty());

        con.setProperty(NCO::fullname(), martin);

        QVERIFY(!con.type().isEmpty());
        QVERIFY(!con.uri().isEmpty());
        QCOMPARE(con.property(NCO::fullname()).toString(), martin);

        contactUri = con.uri();
    }

    Resource con(contactUri);
    QVERIFY(con.exists());
    QCOMPARE(con.uri(), contactUri);
    QCOMPARE(con.property(NCO::fullname()).toString(), martin);

    contactUri = con.uri();
}

void ResourceTests::initTagChange()
{
    Tag tag("Tag1");
    QVERIFY(!tag.exists());
    QVERIFY(tag.uri().isEmpty());

    // Save the tag
    {
        Resource res;
        res.addTag(tag);
        res.remove();
    }

    const QUrl tagUri = tag.uri();
    tag.setProperty(NAO::identifier(), QString("Tag2"));

    Tag tag2("Tag2");
    QVERIFY(tag.exists());
    QCOMPARE(tag.uri(), tagUri);

    Tag tag3(tagUri);
    QVERIFY(tag.exists());
    QCOMPARE(tag.uri(), tagUri);
}

void ResourceTests::initUrlChange()
{
    KTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    const QUrl fileUrl(tempFile.fileName());
    QUrl fileUri;

    KTemporaryFile tempFile2;
    QVERIFY(tempFile2.open());
    const QUrl fileUrl2(tempFile2.fileName());

    {
        Resource fileRes(fileUrl);
        fileRes.setRating(0);

        QVERIFY(fileRes.exists());
        QVERIFY(!fileRes.uri().isEmpty());
        fileUri = fileRes.uri();

        fileRes.setProperty(NIE::url(), fileUrl2);
    }

    {
        Resource fileRes(fileUrl2);
        QVERIFY(fileRes.exists());
        QCOMPARE(fileRes.uri(), fileUri);
    }

    {
        Resource fileRes(fileUrl);
        QVERIFY(!fileRes.exists());
        QVERIFY(fileRes.uri().isEmpty());
    }
}

void ResourceTests::typeTopMost()
{
    Resource res;
    res.addType(NFO::PlainTextDocument());
    res.addType(NFO::Document());
    res.addType(NIE::InformationElement());

    QCOMPARE(res.type(), NFO::PlainTextDocument());
}

void ResourceTests::typePimo()
{
    Resource res;
    res.addType(NFO::PlainTextDocument());
    res.addType(NFO::Document());
    res.addType(NFO::FileDataObject());
    res.addType(NIE::InformationElement());
    res.addType(PIMO::Note());

    QEXPECT_FAIL("", "PIMO Type support not yet implemented", Abort);
    QCOMPARE(res.type(), PIMO::Note());
}

void ResourceTests::tagsUpdate()
{
    Tag tag1("Tag_1");
    Tag tag2("Tag_2");
    Tag tag3("Tag_3");

    QUrl resUri;
    {
        Resource res;
        res.addTag( tag1 );
        res.addTag( tag2 );
        res.addTag( tag3 );
        resUri = res.uri();
    }

    QVERIFY(tag1.exists());
    QVERIFY(tag2.exists());
    QVERIFY(tag3.exists());

    Resource res(resUri);
    res.setWatchEnabled( true );

    QList<Tag> tags;
    tags << tag1 << tag2 << tag3;

    QVERIFY(res.exists());
    QVERIFY(!res.uri().isEmpty());
    QCOMPARE(res.tags(), tags);

    KJob* job = removeProperty( QList<QUrl>() <<res.uri(), NAO::hasTag(),
                                QVariantList() << tag3.uri() );
    job->exec();
    QVERIFY(!job->error());
    QTest::qWait(100);

    QCOMPARE(QList<Tag>() << tag1 << tag2, res.tags());
}

void ResourceTests::metaPropertiesUpdate()
{
    {
        Tag tag("Fire");
        QVERIFY(!tag.exists());

        // Save the tag
        Resource res;
        res.addTag(tag);
    }

    Tag tag("Fire");

    QDateTime modifiedDt = tag.property(NAO::lastModified()).toDateTime();
    kDebug() << modifiedDt;
    QVERIFY(!modifiedDt.isNull());

    KJob* job = Nepomuk2::setProperty(QList<QUrl>() << tag.uri(),
                                      NAO::identifier(), QList<QVariant>() << QString("Water"));
    job->exec();
    QVERIFY(!job->error());
    QTest::qWait(100);

    QEXPECT_FAIL("", "Meta properties are currently not loaded", Abort);
    QVERIFY(tag.property(NAO::lastModified()).toDateTime() != modifiedDt);
}

void ResourceTests::ratingUpdate()
{
    KTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    const QUrl fileUrl(tempFile.fileName());
    QUrl fileUri;

    {
        Resource fileRes( fileUrl );
        QVERIFY(!fileRes.exists());
        QVERIFY(fileRes.uri().isEmpty());

        fileRes.setRating(0);
        QVERIFY(fileRes.exists());
        QVERIFY(!fileRes.type().isEmpty());

        fileUri = fileRes.uri();
    }

    Resource fileRes(fileUri);

    KJob* job = Nepomuk2::setProperty(QList<QUrl>() << fileRes.uri(),
                                      NAO::numericRating(), QVariantList() << 2);
    job->exec();
    QVERIFY(!job->error());

    QTest::qWait(100);
    QVERIFY(fileRes.rating() == 2);
}

void ResourceTests::newResourcesUpdated()
{
    KTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    const QUrl fileUrl(tempFile.fileName());
    QUrl fileUri;

    Resource fileRes( fileUrl );
    fileRes.setWatchEnabled( true );
    QVERIFY(!fileRes.exists());
    QVERIFY(fileRes.uri().isEmpty());

    fileRes.setRating(0);
    QVERIFY(fileRes.exists());
    QVERIFY(!fileRes.type().isEmpty());

    KJob* job = Nepomuk2::setProperty(QList<QUrl>() << fileRes.uri(),
                                      NAO::numericRating(), QVariantList() << 2);
    job->exec();
    QVERIFY(!job->error());

    QTest::qWait(100);
    QVERIFY(fileRes.rating() == 2);
}

void ResourceTests::identifierUpdate()
{
    Tag tag("Fire");
    tag.setWatchEnabled( true );
    QVERIFY(!tag.exists());

    // Save the tag
    {
        Resource res;
        res.addTag(tag);
    }

    QVERIFY(tag.exists());

    KJob* job = Nepomuk2::setProperty( QList<QUrl>() << tag.uri(), NAO::identifier(),
                                       QVariantList() << QLatin1String("Water") );
    job->exec();
    QVERIFY(!job->error());

    QTest::qWait( 200 );

    QCOMPARE(tag.property(NAO::identifier()).toString(), QLatin1String("Water"));

    Tag tag2("Water");
    QCOMPARE(tag2.uri(), tag.uri());
    QCOMPARE(tag2, tag);

    Tag tag3("Fire");
    QVERIFY(!tag3.exists());
}

void ResourceTests::urlUpdate()
{
    QUrl resUrl("akonadi:?item=2342");

    Resource res;
    res.setProperty(NIE::url(), resUrl);

    QVERIFY(res.exists());
    QVERIFY(!res.uri().isEmpty());

    QUrl resUrl2("akonadi:?item=2342");
    KJob* job = Nepomuk2::setProperty( QList<QUrl>() << res.uri(), NIE::url(),
                                       QVariantList() << resUrl2 );
    job->exec();
    QEXPECT_FAIL("", "You currently aren't allowed to change the nie:url", Abort);
    QVERIFY(!job->error());

    QTest::qWait( 200 );

    QUrl nieUrl = res.property(NIE::url()).toUrl();
    QCOMPARE(nieUrl, resUrl2);

    Resource res2(resUrl);
    QVERIFY(!res.exists());

    Resource res3(resUrl2);
    QVERIFY(res3.exists());
}


void ResourceTests::resourceDeletion()
{
    KTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    const QUrl fileUrl(tempFile.fileName());

    Tag tag("Poop");
    Resource fileRes( fileUrl );
    fileRes.setWatchEnabled(true);
    fileRes.addTag(tag);

    const QUrl tagUri = tag.uri();
    QCOMPARE(fileRes.tags(), QList<Tag>() << tag);

    QVERIFY(tag.exists());
    tag.remove();
    QVERIFY(!tag.exists());
    QVERIFY(tag.uri().isEmpty());

    QTest::qWait(200);
    QVERIFY(fileRes.tags().isEmpty());

    // Verify the statements
    Soprano::Model* model = ResourceManager::instance()->mainModel();
    QVERIFY(!model->containsAnyStatement(tagUri, QUrl(), QUrl()));
    QVERIFY(!model->containsAnyStatement(QUrl(), QUrl(), tagUri));
}


}

QTEST_KDEMAIN(Nepomuk2::ResourceTests, NoGUI)
