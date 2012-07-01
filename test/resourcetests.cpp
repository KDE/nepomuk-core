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

#include <KDebug>
#include <KTemporaryFile>
#include <qtest_kde.h>

#include <Soprano/Vocabulary/RDF>
#include <Soprano/Vocabulary/NAO>
#include <Soprano/Statement>
#include <Soprano/Model>
#include <Soprano/StatementIterator>
#include <Nepomuk2/Vocabulary/NCO>
#include <Nepomuk2/Vocabulary/NIE>
#include <Nepomuk2/Vocabulary/NFO>

#include <Nepomuk2/Resource>
#include <Nepomuk2/Variant>
#include <Nepomuk2/Tag>
#include <Nepomuk2/ResourceManager>
#include <Nepomuk2/DataManagement>

using namespace Soprano::Vocabulary;
using namespace Nepomuk2::Vocabulary;

namespace Nepomuk2 {

void ResourceTests::newTag()
{
    Resource tag("Test", NAO::Tag());
    QVERIFY(!tag.exists());

    //QCOMPARE(tag.label(), QString("Test"));
    QVERIFY(!tag.exists());
    QVERIFY(tag.resourceUri().isEmpty());

    // Resources are only saved when some changes are made
    // FIXME: Maybe add some save function?
    {
        Resource res;
        res.addTag( tag );
        res.remove();
    }

    QVERIFY(tag.exists());
    QVERIFY(!tag.resourceUri().isEmpty());
    QCOMPARE(tag.resourceType(), NAO::Tag());

    // Check the statements
    Soprano::Model* model = ResourceManager::instance()->mainModel();

    // One for <resUri> nao:lastModified ..
    //         <resUri> nao:created ..
    //         <resUri> rdf:type nao:Tag
    //         <resUri> nao:identifier "Test"
    const QUrl uri = tag.resourceUri();
    QVERIFY(model->containsAnyStatement(uri, NAO::lastModified(), Soprano::Node()));
    QVERIFY(model->containsAnyStatement(uri, NAO::created(), Soprano::Node()));
    QVERIFY(model->containsAnyStatement(uri, RDF::type(), NAO::Tag()));
    QVERIFY(model->containsAnyStatement(uri, NAO::identifier(), Soprano::LiteralValue("Test")));

    QList<Soprano::Statement> stList = model->listStatements( tag.resourceUri(), Soprano::Node(),
                                                              Soprano::Node() ).allStatements();
    QCOMPARE(stList.size(), 4);
}

void ResourceTests::newContact()
{
    Resource con( QUrl(), NCO::Contact() );
    QVERIFY(!con.exists());
    QVERIFY(con.properties().empty());
    QVERIFY(con.resourceUri().isEmpty());

    QString martin("Martin Klapetek");
    con.setProperty(NCO::fullname(), martin);
    QVERIFY(!con.resourceType().isEmpty());
    QCOMPARE(con.property(NCO::fullname()).toString(), martin);

    // Cross check the statements
    Soprano::Model* model = ResourceManager::instance()->mainModel();

    // One for <resUri> nao:lastModified ..
    //         <resUri> nao:created ..
    //         <resUri> rdf:type nco:Contact
    //         <resUri> nco:fullname "Martin Klapetek"
    const QUrl uri = con.resourceUri();
    QVERIFY(model->containsAnyStatement(uri, NAO::lastModified(), Soprano::Node()));
    QVERIFY(model->containsAnyStatement(uri, NAO::created(), Soprano::Node()));
    QVERIFY(model->containsAnyStatement(uri, RDF::type(), NCO::Contact()));
    QVERIFY(model->containsAnyStatement(uri, NCO::fullname(), Soprano::LiteralValue(martin)));

    QList<Soprano::Statement> stList = model->listStatements( con.resourceUri(), Soprano::Node(),
                                                              Soprano::Node() ).allStatements();
    kDebug() << stList;
    QCOMPARE(stList.size(), 4);
}

void ResourceTests::newFile()
{
    KTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    const QUrl fileUrl(tempFile.fileName());

    Resource fileRes( fileUrl );
    QVERIFY(!fileRes.exists());
    QVERIFY(fileRes.resourceUri().isEmpty());
    QCOMPARE(fileRes.property(NIE::url()).toUrl(), fileUrl);

    // Save the resource
    fileRes.setRating( 5 );
    QVERIFY(fileRes.rating() == 5);
    fileRes.removeProperty(NAO::numericRating());
    QVERIFY(!fileRes.hasProperty(NAO::numericRating()));

    QVERIFY(!fileRes.resourceUri().isEmpty());
    QVERIFY(fileRes.exists());

    // Cross check the statements
    Soprano::Model* model = ResourceManager::instance()->mainModel();

    // One for <resUri> nao:lastModified ..
    //         <resUri> nao:created ..
    //         <resUri> rdf:type nco:Contact
    //         <resUri> nie:url fileUrl
    const QUrl uri = fileRes.resourceUri();
    QVERIFY(model->containsAnyStatement(uri, NAO::lastModified(), Soprano::Node()));
    QVERIFY(model->containsAnyStatement(uri, NAO::created(), Soprano::Node()));
    QVERIFY(model->containsAnyStatement(uri, RDF::type(), NFO::FileDataObject()));
    QVERIFY(model->containsAnyStatement(uri, NIE::url(), fileUrl));

    QList<Soprano::Statement> stList = model->listStatements( uri, Soprano::Node(),
                                                              Soprano::Node() ).allStatements();
    QCOMPARE(stList.size(), 4);
}

void ResourceTests::existingTag()
{
    // Create the tag
    QUrl tagUri;
    {
        Tag t("Tag");
        QVERIFY(!t.exists());
        QVERIFY(t.resourceUri().isEmpty());

        // Save the tag
        Resource res;
        res.addTag(t);
        res.remove();

        QVERIFY(t.exists());
        QVERIFY(!t.resourceUri().isEmpty());
        tagUri = t.resourceUri();
    }
    ResourceManager::instance()->clearCache();

    Tag t("Tag");
    QVERIFY(t.exists());
    QCOMPARE(t.resourceUri(), tagUri);
    QCOMPARE(t.genericLabel(), QString("Tag"));
}

void ResourceTests::existingFile()
{
    KTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    const QUrl fileUrl(tempFile.fileName());
    QUrl fileUri;
    {
        Resource fileRes( fileUrl );
        QVERIFY(!fileRes.exists());
        QVERIFY(fileRes.resourceUri().isEmpty());
        QCOMPARE(fileRes.property(NIE::url()).toUrl(), fileUrl);

        // Save the resource
        fileRes.setRating( 5 );
        QVERIFY(fileRes.rating() == 5);
        fileRes.removeProperty(NAO::numericRating());
        QVERIFY(!fileRes.hasProperty(NAO::numericRating()));
        fileUri = fileRes.resourceUri();
    }

    ResourceManager::instance()->clearCache();

    {
        Resource fileRes( fileUrl );
        QVERIFY(fileRes.exists());
        QCOMPARE(fileRes.resourceUri(), fileUri);
        QCOMPARE(fileRes.property(NIE::url()).toUrl(), fileUrl);
    }

    ResourceManager::instance()->clearCache();

    {
        Resource fileRes( fileUri );
        QVERIFY(fileRes.exists());
        QCOMPARE(fileRes.resourceUri(), fileUri);
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
        QVERIFY(con.resourceUri().isEmpty());

        con.setProperty(NCO::fullname(), martin);

        QVERIFY(!con.resourceType().isEmpty());
        QVERIFY(!con.resourceUri().isEmpty());
        QCOMPARE(con.property(NCO::fullname()).toString(), martin);

        contactUri = con.resourceUri();
    }

    ResourceManager::instance()->clearCache();

    Resource con(contactUri);
    QVERIFY(con.exists());
    QCOMPARE(con.resourceUri(), contactUri);
    QCOMPARE(con.property(NCO::fullname()).toString(), martin);

    contactUri = con.resourceUri();
}

void ResourceTests::checkRating()
{
}

void ResourceTests::initTagChange()
{
    Tag tag("Tag1");
    QVERIFY(!tag.exists());
    QVERIFY(tag.resourceUri().isEmpty());

    // Save the tag
    {
        Resource res;
        res.addTag(tag);
        res.remove();
    }

    const QUrl tagUri = tag.resourceUri();
    tag.setProperty(NAO::identifier(), QString("Tag2"));

    Tag tag2("Tag2");
    QVERIFY(tag.exists());
    QCOMPARE(tag.resourceUri(), tagUri);

    Tag tag3(tagUri);
    QVERIFY(tag.exists());
    QCOMPARE(tag.resourceUri(), tagUri);
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
        QVERIFY(!fileRes.resourceUri().isEmpty());
        fileUri = fileRes.resourceUri();

        fileRes.setProperty(NIE::url(), fileUrl2);
    }

    {
        Resource fileRes(fileUrl2);
        QVERIFY(fileRes.exists());
        QCOMPARE(fileRes.resourceUri(), fileUri);
    }

    {
        Resource fileRes(fileUrl);
        QVERIFY(!fileRes.exists());
        QVERIFY(fileRes.resourceUri().isEmpty());
    }
}


}

QTEST_KDEMAIN(Nepomuk2::ResourceTests, NoGUI)