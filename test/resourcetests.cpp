/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) <year>  <name of author>

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
    /*KTemporaryFile tempFile;
    QVERIFY(tempFile.open());

    Resource file(*/
}

void ResourceTests::existingFile()
{

}

void ResourceTests::existingContact()
{

}

void ResourceTests::existingTag()
{

}

void ResourceTests::initTagChange()
{

}

void ResourceTests::initUrlChange()
{

}

void ResourceTests::checkRating()
{

}

}

QTEST_KDEMAIN(Nepomuk2::ResourceTests, NoGUI)