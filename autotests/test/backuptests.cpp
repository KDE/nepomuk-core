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
#include <resourcemanager.h>
#include "backupmanagerinterface.h"

#include <KDebug>
#include <KJob>
#include <qtest_kde.h>
#include <Soprano/Model>

#include <QtDBus/QDBusConnection>
#include <Soprano/StatementIterator>
#include <Soprano/QueryResultIterator>

typedef org::kde::nepomuk::services::nepomukbackupsync::BackupManager BackupManager;

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

void BackupTests::simpleData()
{
    // Add just one file and check if it is restored
    Test::DataGenerator gen;
    gen.createMusicFile( "Fix you", "Coldplay", "AlbumName") ;

    BackupManager* backupManager = new BackupManager( QLatin1String("org.kde.nepomuk.services.nepomukstorage"),
                                                      QLatin1String("/backupmanager"),
                                                      QDBusConnection::sessionBus(), this);

    KTempDir dir;
    dir.setAutoRemove( false );
    const QString dirUrl = dir.name() + "backup";

    // Create a backup
    {
        backupManager->backup( dirUrl );
        QEventLoop loop;
        connect( backupManager, SIGNAL(backupDone()), &loop, SLOT(quit()) );
        loop.exec();
    }

    // Save all statements in memory
    QList< Soprano::Statement > origNepomukData = outputNepomukData();

    // Reset the repo
    resetRepository();

    // Restore the backup
    {
        backupManager->restore( dirUrl );
        QEventLoop loop;
        connect( backupManager, SIGNAL(restoreDone()), &loop, SLOT(quit()) );
        loop.exec();
    }

    QList< Soprano::Statement > finalNepomukData = outputNepomukData();

    // We can't check all the data cause some of the ontology data would have changed
    // eg - nao:lastModified
    QCOMPARE( origNepomukData, finalNepomukData );
}

}

QTEST_KDEMAIN(Nepomuk2::BackupTests, NoGUI)
