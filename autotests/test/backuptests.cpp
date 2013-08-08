/*
    <one line to give the library's name and an idea of what it does.>
    Copyright (C) 2012  Vishesh Handa <me@vhanda.in>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/


#include "backuptests.h"
#include "../lib/datagenerator.h"
#include "resourcemanager.h"
#include "storeresourcesjob.h"
#include "datamanagement.h"
#include "nfo.h"
#include "nie.h"

#include <KDebug>
#include <KJob>
#include <KTemporaryFile>
#include <Soprano/Graph>
#include <qtest_kde.h>
#include <Soprano/Model>

#include <QtDBus/QDBusConnection>

#include <Soprano/StatementIterator>
#include <Soprano/QueryResultIterator>
#include <Soprano/Vocabulary/RDF>
#include <Soprano/Vocabulary/NRL>
#include <Soprano/Vocabulary/NAO>

using namespace Nepomuk2::Vocabulary;
using namespace Soprano::Vocabulary;

namespace Nepomuk2 {

namespace {
    QList<Soprano::Statement> outputNepomukData() {
        QString query = QString::fromLatin1("select ?r ?p ?o ?g where { graph ?g { ?r ?p ?o. } "
                                            "FILTER(REGEX(STR(?r), '^nepomuk:')) . }");
        Soprano::Model* model = ResourceManager::instance()->mainModel();
        Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );

        QList<Soprano::Statement> stList;
        while( it.next() ) {
            stList << Soprano::Statement( it[0], it[1], it[2], it[3] );
        }
        return stList;
    }
}


BackupTests::BackupTests(QObject* parent): TestBase(parent)
{
    m_backupManager = new BackupManager( QLatin1String("org.kde.nepomuk.services.nepomukstorage"),
                                         QLatin1String("/backupmanager"),
                                         QDBusConnection::sessionBus(), this);
}

void BackupTests::backup(bool tags)
{
    KTempDir dir;
    dir.setAutoRemove( false );
    m_backupLocation = dir.name() + "backup";

    if( !tags )
        m_backupManager->backup( m_backupLocation );
    else
        m_backupManager->backupTagsAndRatings( m_backupLocation );

    QEventLoop loop;
    connect( m_backupManager, SIGNAL(backupDone()), &loop, SLOT(quit()) );
    loop.exec();
}

void BackupTests::restore()
{
    m_backupManager->restore( m_backupLocation );
    QEventLoop loop;
    connect( m_backupManager, SIGNAL(restoreDone()), &loop, SLOT(quit()) );
    kDebug() << "Waiting for restore to finish";
    loop.exec();

    m_backupLocation.clear();

    // Wait for Nepomuk to get initialized
    ResourceManager* rm = ResourceManager::instance();
    if( !rm->initialized() ) {
        QEventLoop loop;
        connect( rm, SIGNAL(nepomukSystemStarted()), &loop, SLOT(quit()) );
        kDebug() << "Waiting for Nepomuk to start";
        loop.exec();
    }
}

namespace {

    QList<QUrl> graphsForApp(const QString& app) {
        QString query = QString::fromLatin1("select distinct ?g where { ?g nao:maintainedBy ?a . "
                                            "?a nao:identifier %1 . }")
                        .arg( Soprano::Node::literalToN3(app) );

        Soprano::Model* model = ResourceManager::instance()->mainModel();
        Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
        QList<QUrl> graphs;
        while( it.next() )
            graphs << it[0].uri();

        return graphs;
    }

    int graphCountForApp(const QString& app) {
        // Check the number of Nepomuk graphs
        return graphsForApp(app).size();
    }

    QList<QUrl> nepomukMeGraph() {
        QString query = QString::fromLatin1("select distinct ?g where { graph ?g { <nepomuk:/me> ?p ?o. } }");

        Soprano::Model* model = ResourceManager::instance()->mainModel();
        Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
        QList<QUrl> nepomukGraphs;
        while( it.next() )
            nepomukGraphs << it[0].uri();

        return nepomukGraphs;
    }
}

void BackupTests::simpleData()
{
    QCOMPARE( nepomukMeGraph(), graphsForApp("nepomuk") );

    // Add just one file and check if it is restored
    Test::DataGenerator gen;
    gen.createMusicFile( "Fix you", "Coldplay", "AlbumName") ;

    backup();

    QCOMPARE( graphCountForApp("nepomuk"), 1 );
    QCOMPARE( graphCountForApp("qttest"), 1 );
    QCOMPARE( nepomukMeGraph(), graphsForApp("nepomuk") );

    // Save all statements in memory
    QSet< Soprano::Statement > origNepomukData = outputNepomukData().toSet();

    // Reset the repo
    resetRepository();

    QCOMPARE( nepomukMeGraph(), graphsForApp("nepomuk") );

    // Restore the backup
    restore();

    QSet< Soprano::Statement > finalNepomukData = outputNepomukData().toSet();

    foreach(const Soprano::Statement&st, origNepomukData) {
        if( !finalNepomukData.contains(st) ) {
            kDebug() << "Restore does not contains" << st;
            QVERIFY( 0 );
        }
    }
    foreach(const Soprano::Statement&st, finalNepomukData) {
        if( !origNepomukData.contains(st) ) {
            kDebug() << "Restore contains extra" << st;
            QVERIFY( 0 );
        }
    }

    QString query;
    Soprano::Model* model = ResourceManager::instance()->mainModel();

    // The Agent should still exist
    query = QString::fromLatin1("ask where { ?r a nao:Agent . }");
    QVERIFY( model->executeQuery( query, Soprano::Query::QueryLanguageSparql ).boolValue() );

    // Check the number of graphs
    QCOMPARE( graphCountForApp("nepomuk"), 1 );
    QCOMPARE( graphCountForApp("qttest"), 1 );
    QCOMPARE( nepomukMeGraph(), graphsForApp("nepomuk") );
}

void BackupTests::indexedData()
{
    KTempDir dir;

    QUrl fileUrl = QUrl::fromLocalFile( dir.name() + "1" ) ;

    SimpleResourceGraph graph = Test::DataGenerator::createMusicFile( fileUrl, "Fix you", "Coldplay", "Album" );

    QHash<QUrl, QVariant> additional;
    additional.insert( RDF::type(), NRL::DiscardableInstanceBase() );

    KJob* job = storeResources( graph, IdentifyNew, NoStoreResourcesFlags, additional );
    job->exec();
    QVERIFY(!job->error());

    backup();
    resetRepository();
    restore();

    QString query;
    Soprano::Model* model = ResourceManager::instance()->mainModel();

    // There should be no Nepomuk Resources with a nie url
    query = QString::fromLatin1("ask where { ?r nie:url ?o . FILTER( REGEX(STR(?r), '^nepomuk:/res') ). }");
    QVERIFY( !model->executeQuery( query, Soprano::Query::QueryLanguageSparql ).boolValue() );

    // Albums - Should not exist
    query = QString::fromLatin1("ask where { ?r a nmm:MusicAlbum . }");
    QVERIFY( !model->executeQuery( query, Soprano::Query::QueryLanguageSparql ).boolValue() );

    // Contacts - Should not exist
    query = QString::fromLatin1("ask where { ?r a nco:Contact . }");
    QVERIFY( !model->executeQuery( query, Soprano::Query::QueryLanguageSparql ).boolValue() );
}

void BackupTests::nonExistingData()
{
    // Create the file
    KTemporaryFile tempFile;
    QVERIFY( tempFile.open() );
    QUrl fileUrl = QUrl::fromLocalFile( tempFile.fileName() );

    SimpleResourceGraph graph = Test::DataGenerator::createMusicFile( fileUrl, "Fix you", "Coldplay", "Album" );
    KJob* job = graph.save();
    job->exec();
    QVERIFY( !job->error() );

    backup();

    resetRepository();
    QFile::remove( fileUrl.toLocalFile() );

    restore();

    QString query;
    Soprano::Model* model = ResourceManager::instance()->mainModel();

    query = QLatin1String("select ?url where { ?r nie:url ?url . }");
    Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );

    QList<QUrl> urls;
    while( it.next() )
        urls << it[0].uri();

    QCOMPARE( urls.size(), 1 );

    QUrl url = urls.first();
    QCOMPARE( url, fileUrl );
    QCOMPARE( url.path(), fileUrl.toLocalFile() );
}

void BackupTests::tagsAndRatings()
{
    KTempDir dir;
    KTemporaryFile tempFile;
    QVERIFY( tempFile.open() );
    QUrl fileUrl1 = QUrl::fromLocalFile( tempFile.fileName() );

    SimpleResource fileRes1;
    fileRes1.addType( NFO::FileDataObject() );
    fileRes1.setProperty( NIE::url(), fileUrl1 );
    fileRes1.setProperty( NAO::numericRating(), 5 );
    fileRes1.setProperty( NAO::description(), QLatin1String("Booga") );

    SimpleResource tagRes;
    tagRes.addType( NAO::Tag() );
    tagRes.setProperty( NAO::identifier(), "TagName" );
    fileRes1.setProperty( NAO::hasTag(), tagRes );

    SimpleResourceGraph graph;
    graph << fileRes1 << tagRes;

    KJob* job = graph.save();
    job->exec();
    QVERIFY( !job->error() );

    backup( true );
    resetRepository();
    restore();

    // Make sure a resource with nie:url fileUrl1 exists
    QString query = QString::fromLatin1("select ?r where { ?r nie:url %1. }")
                    .arg( Soprano::Node::resourceToN3(fileUrl1) );
    Soprano::Model* model = ResourceManager::instance()->mainModel();
    Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );

    QUrl resUri;
    QVERIFY( it.next() );
    resUri = it[0].uri();
    QCOMPARE( resUri.scheme(), QLatin1String("nepomuk") );

    // Make sure it has a rating
    query = QString::fromLatin1("select ?r where { %1 nao:numericRating ?r . }")
            .arg( Soprano::Node::resourceToN3( resUri ) );
    it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
    QVERIFY( it.next() );
    int rating = it[0].literal().toInt();
    QCOMPARE( rating, 5 );

    // Make sure it has the tag
    query = QString::fromLatin1("select ?r where { %1 nao:hasTag ?r . }")
            .arg( Soprano::Node::resourceToN3( resUri ) );
    it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
    QVERIFY( it.next() );
    QUrl tagUri = it[0].uri();
    QCOMPARE( tagUri.scheme(), QLatin1String("nepomuk") );

    // Make sure the tag has its type + identifier
    query = QString::fromLatin1("ask where { %1 a nao:Tag ; nao:identifier %2 . }")
            .arg( Soprano::Node::resourceToN3(tagUri),
                  Soprano::Node::literalToN3("TagName") );

    QVERIFY( model->executeQuery( query, Soprano::Query::QueryLanguageSparql ).boolValue() );

    // Make sure the fileRes has its type + meta properties
    query = QString::fromLatin1("ask where { %1 a nfo:FileDataObject ; nao:lastModified ?m ;"
                                " nao:created ?c . }")
            .arg( Soprano::Node::resourceToN3(resUri) );

    QVERIFY( model->executeQuery( query, Soprano::Query::QueryLanguageSparql ).boolValue() );

    // Make sure the fileRes has its description
    query = QString::fromLatin1("select ?d where { %1 nao:description ?d . }")
            .arg( Soprano::Node::resourceToN3( resUri ) );
    it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
    QVERIFY( it.next() );
    QCOMPARE( it[0].literal().toString(), QLatin1String("Booga") );
}


}

QTEST_KDEMAIN(Nepomuk2::BackupTests, NoGUI)
