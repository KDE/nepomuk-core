/* This file is part of the KDE Project
   Copyright (c) 2008-2010 Sebastian Trueg <trueg@kde.org>
   Copyright (c) 2010-2013 Vishesh Handa <handa.vish@gmail.com>

   Parts of this file are based on code from Strigi
   Copyright (C) 2006-2007 Jos van den Oever <jos@vandenoever.info>

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

#include "indexscheduler.h"
#include "fileindexerconfig.h"
#include "fileindexingqueue.h"
#include "basicindexingqueue.h"
#include "eventmonitor.h"
#include "indexcleaner.h"

#include <QtCore/QList>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QDirIterator>
#include <QtCore/QDateTime>
#include <QtCore/QByteArray>
#include <QtCore/QUrl>

#include <KDebug>
#include <KUrl>
#include <KStandardDirs>
#include <KConfigGroup>
#include <KLocale>


Nepomuk2::IndexScheduler::IndexScheduler( QObject* parent )
    : QObject( parent ),
      m_indexing( false )
{
    // remove old indexing error log
    if(FileIndexerConfig::self()->isDebugModeEnabled()) {
        QFile::remove(KStandardDirs::locateLocal("data", QLatin1String("nepomuk/file-indexer-error-log")));
    }

    FileIndexerConfig* indexConfig = FileIndexerConfig::self();
    connect( indexConfig, SIGNAL(includeFolderListChanged(QStringList,QStringList)),
             this, SLOT(slotIncludeFolderListChanged(QStringList,QStringList)) );
    connect( indexConfig, SIGNAL(excludeFolderListChanged(QStringList,QStringList)),
             this, SLOT(slotExcludeFolderListChanged(QStringList,QStringList)) );

    // FIXME: What if both the signals are emitted?
    connect( indexConfig, SIGNAL(fileExcludeFiltersChanged()),
             this, SLOT(slotConfigFiltersChanged()) );
    connect( indexConfig, SIGNAL(mimeTypeFiltersChanged()),
             this, SLOT(slotConfigFiltersChanged()) );

    // Stop indexing when a device is unmounted
    RemovableMediaCache* cache = new RemovableMediaCache( this );
    connect( cache, SIGNAL(deviceTeardownRequested(const Nepomuk2::RemovableMediaCache::Entry*)),
             this, SLOT(slotTeardownRequested(const Nepomuk2::RemovableMediaCache::Entry*)) );

    m_basicIQ = new BasicIndexingQueue( this );
    m_fileIQ = new FileIndexingQueue( this );

    connect( m_basicIQ, SIGNAL(finishedIndexing()), this, SIGNAL(basicIndexingDone()) );
    connect( m_fileIQ, SIGNAL(finishedIndexing()), this, SIGNAL(fileIndexingDone()) );

    connect( m_basicIQ, SIGNAL(beginIndexingFile(QUrl)), this, SLOT(slotBeginIndexingFile(QUrl)) );
    connect( m_basicIQ, SIGNAL(endIndexingFile(QUrl)), this, SLOT(slotEndIndexingFile(QUrl)) );
    connect( m_fileIQ, SIGNAL(beginIndexingFile(QUrl)), this, SLOT(slotBeginIndexingFile(QUrl)) );
    connect( m_fileIQ, SIGNAL(endIndexingFile(QUrl)), this, SLOT(slotEndIndexingFile(QUrl)) );

    connect( m_basicIQ, SIGNAL(startedIndexing()), this, SLOT(slotStartedIndexing()) );
    connect( m_basicIQ, SIGNAL(finishedIndexing()), this, SLOT(slotFinishedIndexing()) );
    connect( m_fileIQ, SIGNAL(startedIndexing()), this, SLOT(slotStartedIndexing()) );
    connect( m_fileIQ, SIGNAL(finishedIndexing()), this, SLOT(slotFinishedIndexing()) );

    // Connect both the queues together
    connect( m_basicIQ, SIGNAL(endIndexingFile(QUrl)), m_fileIQ, SLOT(enqueue(QUrl)) );

    // Status String
    connect( m_basicIQ, SIGNAL(beginIndexingFile(QUrl)), this, SIGNAL(statusStringChanged()) );
    connect( m_basicIQ, SIGNAL(endIndexingFile(QUrl)), this, SIGNAL(statusStringChanged()) );
    connect( m_basicIQ, SIGNAL(startedIndexing()), this, SIGNAL(statusStringChanged()) );
    connect( m_basicIQ, SIGNAL(finishedIndexing()), this, SIGNAL(statusStringChanged()) );
    connect( m_fileIQ, SIGNAL(beginIndexingFile(QUrl)), this, SIGNAL(statusStringChanged()) );
    connect( m_fileIQ, SIGNAL(endIndexingFile(QUrl)), this, SIGNAL(statusStringChanged()) );
    connect( m_fileIQ, SIGNAL(startedIndexing()), this, SIGNAL(statusStringChanged()) );
    connect( m_fileIQ, SIGNAL(finishedIndexing()), this, SIGNAL(statusStringChanged()) );
    connect( this, SIGNAL(indexingSuspended(bool)), this, SIGNAL(statusStringChanged()) );

    m_eventMonitor = new EventMonitor( this );
    connect( m_eventMonitor, SIGNAL(diskSpaceStatusChanged(bool)),
             this, SLOT(slotScheduleIndexing()) );
    connect( m_eventMonitor, SIGNAL(idleStatusChanged(bool)),
             this, SLOT(slotScheduleIndexing()) );
    connect( m_eventMonitor, SIGNAL(powerManagementStatusChanged(bool)),
             this, SLOT(slotScheduleIndexing()) );

    m_cleaner = new IndexCleaner(this);
    connect( m_cleaner, SIGNAL(finished(KJob*)), this, SLOT(slotCleaningDone()) );
    m_cleaner->start();
    m_state = State_Cleaning;

    // Special settings for the queues
    KConfig config( "nepomukstrigirc" );
    KConfigGroup cfg = config.group( "Indexing" );

    int basicIQDelay = cfg.readEntry<int>( "BasicIQDelay", 0 );
    int fileIQDelay = cfg.readEntry<int>( "FileIQDelay", 0 );
    m_basicIQ->setDelay( basicIQDelay );
    m_fileIQ->setDelay( fileIQDelay );

    QString value = cfg.readEntry<QString>( "NormalMode_FileIndexing", "suspend" );
    if( value == "suspend" )
        m_shouldSuspendFileIQOnNormal = true;
    else if( value == "resume" )
        m_shouldSuspendFileIQOnNormal = false;

    slotScheduleIndexing();
}


Nepomuk2::IndexScheduler::~IndexScheduler()
{
}


void Nepomuk2::IndexScheduler::suspend()
{
    if ( m_state != State_Suspended ) {
        m_state = State_Suspended;
        slotScheduleIndexing();

        if( m_cleaner ) {
            m_cleaner->suspend();
        }
        m_eventMonitor->disable();

        emit indexingSuspended( true );
    }
}


void Nepomuk2::IndexScheduler::resume()
{
    if( m_state == State_Suspended ) {
        m_state = State_Normal;
        slotScheduleIndexing();

        if( m_cleaner ) {
            m_cleaner->resume();
        }
        m_eventMonitor->enable();

        emit indexingSuspended( false );
    }
}


void Nepomuk2::IndexScheduler::setSuspended( bool suspended )
{
    if ( suspended )
        suspend();
    else
        resume();
}

bool Nepomuk2::IndexScheduler::isSuspended() const
{
    return m_state == State_Suspended;
}

bool Nepomuk2::IndexScheduler::isCleaning() const
{
    return m_state == State_Cleaning;
}

bool Nepomuk2::IndexScheduler::isIndexing() const
{
    return m_indexing;
}

QUrl Nepomuk2::IndexScheduler::currentUrl() const
{
    if( !m_fileIQ->currentUrl().isEmpty() )
        return m_fileIQ->currentUrl();
    else
        return m_basicIQ->currentUrl();
}

Nepomuk2::UpdateDirFlags Nepomuk2::IndexScheduler::currentFlags() const
{
    return m_basicIQ->currentFlags();
}


void Nepomuk2::IndexScheduler::setIndexingStarted( bool started )
{
    if ( started != m_indexing ) {
        m_indexing = started;
        emit indexingStateChanged( m_indexing );
        if ( m_indexing )
            emit indexingStarted();
        else
            emit indexingStopped();
    }
}

void Nepomuk2::IndexScheduler::slotStartedIndexing()
{
    m_eventMonitor->enable();
}

void Nepomuk2::IndexScheduler::slotFinishedIndexing()
{
    m_eventMonitor->suspendDiskSpaceMonitor();
}

void Nepomuk2::IndexScheduler::slotCleaningDone()
{
    m_cleaner = 0;

    for( int i=0; i<m_foldersToQueue.size(); i++ ) {
        QPair< QString, UpdateDirFlags > pair = m_foldersToQueue[i];
        m_basicIQ->enqueue( pair.first, pair.second );
    }

    m_state = State_Normal;
    slotScheduleIndexing();
}

void Nepomuk2::IndexScheduler::updateDir( const QString& path, UpdateDirFlags flags )
{
    m_basicIQ->enqueue( path, flags );
}


void Nepomuk2::IndexScheduler::updateAll( bool forceUpdate )
{
    queueAllFoldersForUpdate( forceUpdate );
}


void Nepomuk2::IndexScheduler::queueAllFoldersForUpdate( bool forceUpdate )
{
    m_basicIQ->clear();

    UpdateDirFlags flags = UpdateRecursive|AutoUpdateFolder;
    if ( forceUpdate )
        flags |= ForceUpdate;

    // update everything again in case the folders changed
    // If the cleaner is running, then we do not add the folders, instead they will be added
    // in slotCleaningDone
    foreach( const QString& f, FileIndexerConfig::self()->includeFolders() ) {
        if( m_cleaner )
            m_foldersToQueue.append( qMakePair( f, flags ) );
        else
            m_basicIQ->enqueue( f, flags );
    }
}


void Nepomuk2::IndexScheduler::slotIncludeFolderListChanged(const QStringList& added, const QStringList& removed)
{
    kDebug() << added << removed;
    foreach( const QString& path, removed ) {
        m_basicIQ->clear( path );
        m_fileIQ->clear( path );
    }

    restartCleaner();

    foreach( const QString& path, added ) {
        if( m_cleaner )
            m_foldersToQueue.append( qMakePair( path, UpdateDirFlags(UpdateRecursive) ) );
        else
            m_basicIQ->enqueue( path, UpdateRecursive );
    }
}

void Nepomuk2::IndexScheduler::slotExcludeFolderListChanged(const QStringList& added, const QStringList& removed)
{
    kDebug() << added << removed;
    foreach( const QString& path, added ) {
        m_basicIQ->clear( path );
        m_fileIQ->clear( path );
    }

    restartCleaner();

    foreach( const QString& path, removed ) {
        if( m_cleaner )
            m_foldersToQueue.append( qMakePair( path, UpdateDirFlags(UpdateRecursive) ) );
        else
            m_basicIQ->enqueue( path, UpdateRecursive );
    }
}

void Nepomuk2::IndexScheduler::restartCleaner()
{
    if( m_cleaner ) {
        m_cleaner->kill();
        delete m_cleaner;
    }

    // TODO: only clean the filters that were changed from the config
    m_cleaner = new IndexCleaner( this );
    connect( m_cleaner, SIGNAL(finished(KJob*)), this, SLOT(slotCleaningDone()) );
    m_cleaner->start();

    m_state = State_Cleaning;
    slotScheduleIndexing();
}


void Nepomuk2::IndexScheduler::slotConfigFiltersChanged()
{
    restartCleaner();

    // We need to this - there is no way to avoid it
    m_basicIQ->clear();
    m_fileIQ->clear();

    queueAllFoldersForUpdate();
}


void Nepomuk2::IndexScheduler::analyzeFile( const QString& path )
{
    m_basicIQ->enqueue( path );
}


void Nepomuk2::IndexScheduler::slotBeginIndexingFile(const QUrl& url)
{
    setIndexingStarted( true );

    QString path = url.toLocalFile();
    if( QFileInfo(path).isDir() )
        emit indexingFolder( path );
    else
        emit indexingFile( path );
}

void Nepomuk2::IndexScheduler::slotEndIndexingFile(const QUrl&)
{
    const QUrl basicUrl = m_basicIQ->currentUrl();
    const QUrl fileUrl = m_fileIQ->currentUrl();

    if( basicUrl.isEmpty() && fileUrl.isEmpty() ) {
        setIndexingStarted( false );
    }
}

void Nepomuk2::IndexScheduler::slotTeardownRequested(const Nepomuk2::RemovableMediaCache::Entry* entry)
{
    const QString path = entry->mountPath();
    QMutableListIterator< QPair<QString, UpdateDirFlags> > it( m_foldersToQueue );
    while( it.hasNext() ) {
        it.next();
        if( it.value().first.startsWith( path ) )
            it.remove();
    }

    m_basicIQ->clear( path );
    m_fileIQ->clear( path );
}

void Nepomuk2::IndexScheduler::slotScheduleIndexing()
{
    if( m_state == State_Suspended || m_state == State_Cleaning ) {
        kDebug() << "Cleaning | Suspended";
        m_basicIQ->suspend();
        m_fileIQ->suspend();
        return;
    }

    if( m_eventMonitor->isDiskSpaceLow() ) {
        kDebug() << "Disk Space";
        m_state = State_LowDiskSpace;

        m_basicIQ->suspend();
        m_fileIQ->suspend();
    }

    else if( m_eventMonitor->isOnBattery() ) {
        kDebug() << "Battery";
        m_state = State_OnBattery;

        m_basicIQ->resume();
        m_fileIQ->suspend();
    }

    else if( m_eventMonitor->isIdle() ) {
        kDebug() << "Idle";
        m_state = State_UserIdle;

        m_basicIQ->resume();
        m_fileIQ->resume();
    }

    else {
        kDebug() << "Normal";
        m_state = State_Normal;

        m_basicIQ->resume();
        if( m_shouldSuspendFileIQOnNormal )
            m_fileIQ->suspend();
        else
            m_fileIQ->resume();
    }
}

QString Nepomuk2::IndexScheduler::userStatusString() const
{
    bool indexing = isIndexing();
    bool suspended = isSuspended();
    bool cleaning = ( m_cleaner != 0 );
    bool processing = !m_basicIQ->isEmpty();

    if ( suspended ) {
        return i18nc( "@info:status", "File indexer is suspended." );
    }
    else if ( cleaning ) {
        // TODO: Change this string for 4.11
        return i18nc( "@info:status", "Scanning for recent changes in files for desktop search");
    }
    else if ( indexing ) {
        QUrl url = currentUrl();

        if( url.isEmpty() ) {
            return i18nc( "@info:status", "Indexing files for desktop search." );
        }
        else {
            return i18nc( "@info:status", "Indexing %1", url.toLocalFile() );
        }
    }
    else if ( processing ) {
        return i18nc( "@info:status", "Scanning for recent changes in files for desktop search");
    }
    else {
        return i18nc( "@info:status", "File indexer is idle." );
    }
}

Nepomuk2::IndexScheduler::State Nepomuk2::IndexScheduler::currentStatus() const
{
    return m_state;
}



#include "indexscheduler.moc"
