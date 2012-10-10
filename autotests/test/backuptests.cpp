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

#include <KDebug>
#include <KJob>
#include <qtest_kde.h>
#include <Soprano/Model>

#include <QtDBus/QDBusConnection>

#include <Soprano/StatementIterator>
#include <Soprano/QueryResultIterator>
#include <Soprano/Vocabulary/RDF>
#include <Soprano/Vocabulary/NRL>

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

void BackupTests::backup()
{
    KTempDir dir;
    dir.setAutoRemove( false );
    m_backupLocation = dir.name() + "backup";

    m_backupManager->backup( m_backupLocation );
    QEventLoop loop;
    connect( m_backupManager, SIGNAL(backupDone()), &loop, SLOT(quit()) );
    loop.exec();
}

void BackupTests::restore()
{
    m_backupManager->restore( m_backupLocation );
    QEventLoop loop;
    connect( m_backupManager, SIGNAL(restoreDone()), &loop, SLOT(quit()) );
    loop.exec();

    m_backupLocation.clear();
}

void BackupTests::simpleData()
{
    // Add just one file and check if it is restored
    Test::DataGenerator gen;
    gen.createMusicFile( "Fix you", "Coldplay", "AlbumName") ;

    backup();

    // Save all statements in memory
    QList< Soprano::Statement > origNepomukData = outputNepomukData();

    // Reset the repo
    resetRepository();

    // Restore the backup
    restore();

    QList< Soprano::Statement > finalNepomukData = outputNepomukData();

    // We can't check all the data cause some of the ontology data would have changed
    // eg - nao:lastModified
    QCOMPARE( origNepomukData, finalNepomukData );

    QString query;
    Soprano::Model* model = ResourceManager::instance()->mainModel();

    // The Agent should still exist
    query = QString::fromLatin1("ask where { ?r a nao:Agent . }");
    QVERIFY( model->executeQuery( query, Soprano::Query::QueryLanguageSparql ).boolValue() );

    // The pimo:Person - nepomuk:/me should still exist
    query = QString::fromLatin1("ask where { <nepomuk:/me> ?p ?o . }");
    QVERIFY( model->executeQuery( query, Soprano::Query::QueryLanguageSparql ).boolValue() );
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

    // The Agent should still exist
    query = QString::fromLatin1("ask where { ?r a nao:Agent . }");
    QVERIFY( model->executeQuery( query, Soprano::Query::QueryLanguageSparql ).boolValue() );

    // The pimo:Person - nepomuk:/me should still exist
    query = QString::fromLatin1("ask where { <nepomuk:/me> ?p ?o . }");
    QVERIFY( model->executeQuery( query, Soprano::Query::QueryLanguageSparql ).boolValue() );
}

void BackupTests::nonExistingData()
{
    // Create the file
    KTempDir dir;
    QUrl fileUrl = QUrl::fromLocalFile( dir.name() + "1" ) ;

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
    QCOMPARE( url.scheme(), QLatin1String("nepomuk-backup") );
    QCOMPARE( url.path(), fileUrl.toLocalFile() );
}


}

QTEST_KDEMAIN(Nepomuk2::BackupTests, NoGUI)
