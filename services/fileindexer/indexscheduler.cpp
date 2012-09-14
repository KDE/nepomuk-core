/* This file is part of the KDE Project
   Copyright (c) 2008-2010 Sebastian Trueg <trueg@kde.org>
   Copyright (c) 2010-11 Vishesh Handa <handa.vish@gmail.com>

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
#include "nepomukindexer.h"
#include "util.h"
#include "datamanagement.h"

#include <QtCore/QList>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QDirIterator>
#include <QtCore/QDateTime>
#include <QtCore/QByteArray>
#include <QtCore/QUrl>

#include <KDebug>
#include <KTemporaryFile>
#include <KUrl>
#include <KStandardDirs>
#include <KIdleTime>

#include "resource.h"
#include "resourcemanager.h"
#include "variant.h"

#include <Soprano/Model>
#include <Soprano/QueryResultIterator>
#include <Soprano/NodeIterator>
#include <Soprano/Node>

#include "indexcleaner.h"

Nepomuk2::IndexScheduler::IndexScheduler( QObject* parent )
    : QObject( parent ),
      m_suspended( false ),
      m_indexing( false )
{
    // remove old indexing error log
    if(FileIndexerConfig::self()->isDebugModeEnabled()) {
        QFile::remove(KStandardDirs::locateLocal("data", QLatin1String("nepomuk/file-indexer-error-log")));
    }

    m_cleaner = new IndexCleaner(this);
    connect( m_cleaner, SIGNAL(finished(KJob*)), this, SLOT(slotCleaningDone()) );
    m_cleaner->start();

    connect( FileIndexerConfig::self(), SIGNAL( configChanged() ),
             this, SLOT( slotConfigChanged() ) );

    m_fastQueue = new FastIndexingQueue( this );
    m_slowQueue = new SlowIndexingQueue( this );

    connect( m_fastQueue, SIGNAL(endIndexing(QString)),
             m_slowQueue, SLOT(enqueue(QString)) );

    m_slowQueue->suspend();

    // stop the slow queue on user activity
    KIdleTime* idleTime = KIdleTime::instance();
    idleTime->addIdleTimeout( 1000 * 60 * 2 ); // 2 min

    connect( idleTime, SIGNAL(timeoutReached(int)), this, SLOT(slotIdleTimeoutReached()) );
    connect( idleTime, SIGNAL(resumingFromIdle()), m_slowQueue, SLOT(suspend()) );
}


Nepomuk2::IndexScheduler::~IndexScheduler()
{
}

void Nepomuk2::IndexScheduler::slotIdleTimeoutReached()
{
    m_slowQueue->resume();
    KIdleTime::instance()->catchNextResumeEvent();
}


void Nepomuk2::IndexScheduler::suspend()
{
    if ( !m_suspended ) {
        m_suspended = true;
        if( m_cleaner ) {
            m_cleaner->suspend();
        }

        m_fastQueue->suspend();
        emit indexingSuspended( true );
    }
}


void Nepomuk2::IndexScheduler::resume()
{
    if ( m_suspended ) {
        m_suspended = false;

        if( m_cleaner ) {
            m_cleaner->resume();
        }

        m_fastQueue->resume();
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
    return m_suspended;
}


bool Nepomuk2::IndexScheduler::isIndexing() const
{
    return m_indexing;
}


QString Nepomuk2::IndexScheduler::currentFolder() const
{
    return m_currentUrl.directory();
}


QString Nepomuk2::IndexScheduler::currentFile() const
{
    return m_currentUrl.toLocalFile();
}


Nepomuk2::IndexScheduler::UpdateDirFlags Nepomuk2::IndexScheduler::currentFlags() const
{
    return m_currentFlags;
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


void Nepomuk2::IndexScheduler::slotCleaningDone()
{
    m_cleaner = 0;
}

/*
void Nepomuk2::IndexScheduler::analyzeDir( const QString& dir_, Nepomuk2::IndexScheduler::UpdateDirFlags flags )
{
    kDebug() << dir_;

    // normalize the dir name, otherwise things might break below
    QString dir( dir_ );
    if( dir.endsWith(QLatin1String("/")) ) {
        dir.truncate( dir.length()-1 );
    }

    // inform interested clients
    emit indexingFolder( dir );
    m_currentUrl = KUrl( dir );
    m_currentFlags = flags;

    const bool recursive = flags&UpdateRecursive;
    const bool forceUpdate = flags&ForceUpdate;

    // we start by updating the folder itself
    QFileInfo dirInfo( dir );
    KUrl dirUrl( dir );
    if ( !compareIndexedMTime(dirUrl, dirInfo.lastModified()) ) {
        KJob * indexer = new Indexer( dirInfo );
        connect( indexer, SIGNAL(finished(KJob*)), this, SLOT(slotIndexingDone(KJob*)) );
        indexer->start();
    }

    // get a map of all indexed files from the dir including their stored mtime
    QHash<QString, QDateTime> filesInStore = getChildren( dir );
    QHash<QString, QDateTime>::iterator filesInStoreEnd = filesInStore.end();

    QList<QFileInfo> filesToIndex;
    QStringList filesToDelete;

    // iterate over all files in the dir
    // and select the ones we need to add or delete from the store
    QDir::Filters dirFilter = QDir::NoDotAndDotDot|QDir::Readable|QDir::Files|QDir::Dirs;
    QDirIterator dirIt( dir, dirFilter );
    while ( dirIt.hasNext() ) {
        QString path = dirIt.next();

        // FIXME: we cannot use canonialFilePath here since that could lead into another folder. Thus, we probably
        // need to use another approach than the getChildren one.
        QFileInfo fileInfo = dirIt.fileInfo();//.canonialFilePath();

        bool indexFile = Nepomuk2::FileIndexerConfig::self()->shouldFileBeIndexed( fileInfo.fileName() );

        // check if this file is new by looking it up in the store
        QHash<QString, QDateTime>::iterator filesInStoreIt = filesInStore.find( path );
        bool newFile = ( filesInStoreIt == filesInStoreEnd );
        if ( newFile && indexFile )
            kDebug() << "NEW    :" << path;

        // do we need to update? Did the file change?
        bool fileChanged = !newFile && fileInfo.lastModified() != filesInStoreIt.value();
        //TODO: At some point make these "NEW", "CHANGED", and "FORCED" strings public
        //      so that they can be used to create a better status message.
        if ( fileChanged )
            kDebug() << "CHANGED:" << path << fileInfo.lastModified() << filesInStoreIt.value();
        else if( forceUpdate )
            kDebug() << "UPDATE FORCED:" << path;

        if ( indexFile && ( newFile || fileChanged || forceUpdate ) )
            filesToIndex << fileInfo;

        // we do not delete files to update here. We do that in the IndexWriter to make
        // sure we keep the resource URI
        else if ( !newFile && !indexFile )
            filesToDelete.append( filesInStoreIt.key() );

        // cleanup a bit for faster lookups
        if ( !newFile )
            filesInStore.erase( filesInStoreIt );

        // prepend sub folders to the dir queue
        // sub-dirs of auto-update folders are only addded if they are configured as such
        // all others (manually added ones) are always indexed
        if ( indexFile &&
                recursive &&
                fileInfo.isDir() &&
                !fileInfo.isSymLink() &&
                (!(flags & AutoUpdateFolder) || FileIndexerConfig::self()->shouldFolderBeIndexed( path )) ) {
            m_dirsToUpdate.prependDir( path, flags );
        }
    }

    // all the files left in filesInStore are not in the current
    // directory and should be deleted
    filesToDelete += filesInStore.keys();

    // remove all files that have been removed recursively
    deleteEntries( filesToDelete );

    // analyze all files that are new or need updating
    m_filesToUpdate.append( filesToIndex );

    // reset status
    m_currentUrl.clear();
    m_currentFlags = NoUpdateFlags;
}
*/


void Nepomuk2::IndexScheduler::updateDir( const QString& path, UpdateDirFlags flags )
{
    //FIXME: Use UpdateDirFlags

    m_fastQueue->enqueue( path );
}


void Nepomuk2::IndexScheduler::updateAll( bool forceUpdate )
{
    queueAllFoldersForUpdate( forceUpdate );
}


void Nepomuk2::IndexScheduler::queueAllFoldersForUpdate( bool forceUpdate )
{
    // TODO: Clear the queues

    /*
    UpdateDirFlags flags = UpdateRecursive|AutoUpdateFolder;
    if ( forceUpdate )
        flags |= ForceUpdate;
    */

    // update everything again in case the folders changed
    foreach( const QString& f, FileIndexerConfig::self()->includeFolders() ) {
        m_fastQueue->enqueue( f );
    }
}


void Nepomuk2::IndexScheduler::slotConfigChanged()
{
    // TODO: only update folders that were added in the config
    updateAll();

    if( m_cleaner ) {
        m_cleaner->kill();
        delete m_cleaner;
    }

    // TODO: only clean the folders that were removed from the config
    m_cleaner = new IndexCleaner( this );
    connect( m_cleaner, SIGNAL(finished(KJob*)), this, SLOT(slotCleaningDone()) );
    m_cleaner->start();
}


void Nepomuk2::IndexScheduler::analyzeFile( const QString& path )
{
    kDebug() << path;
    m_fastQueue->enqueue( path );

    /*

    // we prepend the file to give preference to newly created and changed files over
    // the initial indexing. Sadly operator== cannot be relied on for QFileInfo. Thus
    // we need to do a dumb search
    QMutableListIterator<QFileInfo> it(m_filesToUpdate);
    while(it.hasNext()) {
        if(it.next().filePath() == path) {
            kDebug() << "Already queued:" << path << "Moving to front of queue.";
            it.remove();
            break;
        }
    }
    kDebug() << "Queuing" << path;
    m_filesToUpdate.prepend(path);

    // continue indexing without any delay. We want changes reflected as soon as possible
    if( !m_indexing ) {
        callDoIndexing();
    }*/
}


void Nepomuk2::IndexScheduler::deleteEntries( const QStringList& entries )
{
    /*
    // recurse into subdirs
    // TODO: use a less mem intensive method
    for ( int i = 0; i < entries.count(); ++i ) {
        deleteEntries( getChildren( entries[i] ).keys() );
    }
    Nepomuk2::clearIndexedData(KUrl::List(entries));*/
}

#include "indexscheduler.moc"
