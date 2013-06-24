/* This file is part of the KDE Project
   Copyright (c) 2007-10 Sebastian Trueg <trueg@kde.org>
   Copyright (c) 2013 Vishesh Handa <me@vhanda.in>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "storage.h"
#include "repository.h"
#include "query/queryservice.h"
#include "backup/backupmanager.h"
#include "resourcemanager.h"
#include "graphmigrationjob.h"

#include <QtDBus/QDBusConnection>
#include <QtCore/QFile>
#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>
#include <QtCore/QDir>
#include <QtCore/QThread>

#include <KDebug>
#include <KGlobal>
#include <KStandardDirs>
#include <KConfigGroup>
#include <KProcess>
#include <Soprano/QueryResultIterator>

namespace {
    static const char s_repositoryName[] = "main";
}


Nepomuk2::Storage::Storage()
    : Service2( 0, true /* delayed initialization */ )
    , m_queryService( 0 )
    , m_backupManager( 0 )
    , m_resetInProgress( false )
{
    // register the fancier name for this important service
    QDBusConnection::sessionBus().registerService( "org.kde.NepomukStorage" );

    m_repository = new Repository( QLatin1String( s_repositoryName ) );
    connect( m_repository, SIGNAL( loaded( Repository*, bool ) ),
                this, SLOT( slotRepositoryLoaded( Repository*, bool ) ) );
    connect( m_repository, SIGNAL( closed( Repository* ) ),
                this, SLOT( slotRepositoryClosed() ) );
    QTimer::singleShot( 0, m_repository, SLOT( open() ) );
}


Nepomuk2::Storage::~Storage()
{
    slotRepositoryClosed();

    delete m_repository;
    m_repository = 0;
}

void Nepomuk2::Storage::slotRepositoryLoaded(Nepomuk2::Repository* repo, bool success)
{
    // FIXME: ResourceManager::instance() will result in a another VirtuosoModel being made
    //        and then removed. Is that good?
    //
    // We overide the main model cause certain classes utilize the Resource class, and we
    // don't want them using a different model
    ResourceManager::instance()->setOverrideMainModel( repo );

    // Backup Service
    if( !m_backupManager )
        m_backupManager = new BackupManager( this );

    if( m_resetInProgress ) {
        m_resetInProgress = false;
        emit resetRepositoryDone( m_oldPath, m_newPath );
        return;
    }

    if( !dataMigrationRequired() ) {
        openPublicInterfaces();
    }
    else {
        KProcess::execute( "nepomukmigrator" );
        setServiceInitialized( false );
    }
}

void Nepomuk2::Storage::openPublicInterfaces()
{
    if( !m_queryService )
        m_queryService = new Query::QueryService( m_repository, this );

    // DataManagement interface
    m_repository->openPublicInterface();

    setServiceInitialized( true );
    kDebug() << "Registered QueryService and DataManagement interface";
}

void Nepomuk2::Storage::closePublicInterfaces()
{
    setServiceInitialized( false );

    m_repository->closePublicInterface();

    delete m_queryService;
    m_queryService = 0;
}

void Nepomuk2::Storage::slotRepositoryClosed()
{
    closePublicInterfaces();

    delete m_backupManager;
    m_backupManager = 0;
}

namespace {

    bool removeDir(const QString & dirName)
    {
        bool result;
        QDir dir(dirName);

        if (dir.exists(dirName)) {
            QFileInfoList list = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden |
                                                   QDir::AllDirs | QDir::Files, QDir::DirsFirst);
            foreach(QFileInfo info, list) {
                if( info.isDir() )
                    result = removeDir(info.absoluteFilePath());
                else
                    result = QFile::remove(info.absoluteFilePath());

                if( !result ) {
                    return result;
                }
            }
            result = dir.rmdir(dirName);
        }
        return result;
    }
}

void Nepomuk2::Storage::resetRepository()
{
    closePublicInterfaces();
    m_resetInProgress = true;
    m_repository->disconnect( this );

    connect( m_repository, SIGNAL(closed(Repository*)),
             this, SLOT(slotRepositoryClosedAfterReset()), Qt::QueuedConnection );
    m_repository->close();
}

void Nepomuk2::Storage::slotRepositoryClosedAfterReset()
{
    // Remove the damn repo
    m_oldPath = m_repository->storagePath();
    m_newPath = m_oldPath + QDateTime::currentDateTime().toString(Qt::ISODate);

    QFile::rename( m_oldPath, m_newPath );

    m_repository->disconnect( this );
    connect( m_repository, SIGNAL( loaded( Repository*, bool ) ),
                this, SLOT( slotRepositoryLoaded( Repository*, bool ) ) );
    connect( m_repository, SIGNAL( closed( Repository* ) ),
                this, SLOT( slotRepositoryClosed() ) );

    m_repository->open();
}

bool Nepomuk2::Storage::hasMigrationData()
{
    QString query = QString::fromLatin1("ask where { graph ?g { ?r rdf:type ?t . FILTER(?t in (nao:Tag, nfo:FileDataObject)) . }"
                                        "FILTER NOT EXISTS { ?g a nrl:DiscardableInstanceBase . } }");

    bool mg = m_repository->executeQuery( query, Soprano::Query::QueryLanguageSparql ).boolValue();
    kDebug() << "HAS MIGRATION DATA:" << mg;
    return mg;
}

void Nepomuk2::Storage::migrateGraphsByBackup()
{
    closePublicInterfaces();

    if( !dataMigrationRequired() ) {
        emit migrateGraphsDone();
        return;
    }

    if( !hasMigrationData() ) {
        connect( this, SIGNAL(resetRepositoryDone(QString, QString)), this, SLOT(slotMigrationResetDone(QString, QString)) );
        emit migrateGraphsPercent( -1 );
        resetRepository();
        return;
    }

    QString backupLocation = KStandardDirs::locateLocal( "data", "nepomuk/migrationBackup" );
    if( !QFile::exists(backupLocation) ) {
        connect( m_backupManager, SIGNAL(backupPercent(int)), this, SLOT(slotMigrationBackupProgress(int)) );
        connect( m_backupManager, SIGNAL(backupDone()), this, SLOT(slotMigrationBackupDone()) );

        m_backupManager->backupTagsAndRatings( backupLocation );
        emit migrateGraphsPercent( 3 );
    }
    else {
        slotMigrationBackupDone();
        emit migrateGraphsPercent( 53 );
    }
}

void Nepomuk2::Storage::slotMigrationResetDone(const QString&, const QString& newPath)
{
    disconnect( this, SIGNAL(resetRepositoryDone(QString, QString)), this, SLOT(slotMigrationResetDone(QString, QString)) );

    removeDir( newPath );
    slotMigrationDone();
}

void Nepomuk2::Storage::slotMigrationBackupDone()
{
    disconnect( m_backupManager, SIGNAL(backupPercent(int)), this, SLOT(slotMigrationBackupProgress(int)) );
    disconnect( m_backupManager, SIGNAL(backupDone()), this, SLOT(slotMigrationBackupDone()) );

    connect( m_backupManager, SIGNAL(restorePercent(int)), this, SLOT(slotMigrationRestoreProgress(int)) );
    connect( m_backupManager, SIGNAL(restoreDone()), this, SLOT(slotMigrationRestoreDone()) );

    QString backupLocation = KStandardDirs::locateLocal( "data", "nepomuk/migrationBackup" );
    m_backupManager->restore( backupLocation );
}

void Nepomuk2::Storage::slotMigrationRestoreDone()
{
    disconnect( m_backupManager, SIGNAL(restorePercent(int)), this, SLOT(slotMigrationRestoreProgress(int)) );
    disconnect( m_backupManager, SIGNAL(restoreDone()), this, SLOT(slotMigrationRestoreDone()) );

    setDataMigrated();
    emit migrateGraphsDone();
}

void Nepomuk2::Storage::slotMigrationBackupProgress(int percent)
{
    emit migrateGraphsPercent( 3 + percent/2 );
}

void Nepomuk2::Storage::slotMigrationRestoreProgress(int percent)
{
    emit migrateGraphsPercent( qMin( 53, 53 + percent/2 ) );
}

void Nepomuk2::Storage::migrateGraphs()
{
    closePublicInterfaces();

    KJob* job = new GraphMigrationJob( model() );

    QThread* migrationThread = new QThread( this );
    job->moveToThread( migrationThread );
    migrationThread->start();

    connect( job, SIGNAL(finished(KJob*)), migrationThread, SLOT(quit()), Qt::QueuedConnection );
    connect( migrationThread, SIGNAL(finished()), migrationThread, SLOT(deleteLater()) );
    connect( job, SIGNAL(finished(KJob*)), this, SLOT(slotMigrationDone()), Qt::QueuedConnection );
    connect( job, SIGNAL(percent(KJob*,ulong)), this, SLOT(slotMigrationPercent(KJob*,ulong)), Qt::QueuedConnection );
    job->start();
}

void Nepomuk2::Storage::slotMigrationDone()
{
    setDataMigrated();
    openPublicInterfaces();
    emit migrateGraphsDone();
}

void Nepomuk2::Storage::slotMigrationPercent(KJob* , ulong percent)
{
    kDebug() << percent;
    emit migrateGraphsPercent( percent );
}

QString Nepomuk2::Storage::usedSopranoBackend() const
{
    return m_repository->usedSopranoBackend();
}

Soprano::Model* Nepomuk2::Storage::model()
{
    return m_repository;
}

bool Nepomuk2::Storage::dataMigrationRequired()
{
    KConfigGroup group = KSharedConfig::openConfig("nepomukserverrc")->group( m_repository->name() + " Settings" );
    return group.readEntry( "GraphMigrationRequired", true );
}

void Nepomuk2::Storage::setDataMigrated()
{
    KConfigGroup group = KSharedConfig::openConfig("nepomukserverrc")->group( m_repository->name() + " Settings" );
    group.writeEntry( "GraphMigrationRequired", false );

    group = KSharedConfig::openConfig("nepomukstrigirc")->group( "General" );
    group.writeEntry( "first run", true );
}


int main( int argc, char **argv ) {
    KAboutData aboutData( "nepomukstorage",
                          "nepomukstorage",
                          ki18n("Nepomuk Storage"),
                          NEPOMUK_VERSION_STRING,
                          ki18n("Nepomuk Storage"),
                          KAboutData::License_GPL,
                          ki18n("(c) 2008-2013, Sebastian Trüg"),
                          KLocalizedString(),
                          "http://nepomuk.kde.org" );
    aboutData.addAuthor(ki18n("Sebastian Trüg"),ki18n("Developer"), "trueg@kde.org");
    aboutData.addAuthor(ki18n("Vishesh Handa"),ki18n("Maintainer"), "me@vhanda.in");

    Nepomuk2::Service2::init<Nepomuk2::Storage>( argc, argv, aboutData );
}

#include "storage.moc"
