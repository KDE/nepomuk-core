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
#include "fileindexingqueue.h"
#include "basicindexingqueue.h"

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

    m_basicIQ = new BasicIndexingQueue( this );
    m_fileIQ = new FileIndexingQueue( this );

    connect( m_basicIQ, SIGNAL(finishedIndexing()), this, SLOT(slotIndexingFinished()) );
    connect( m_fileIQ, SIGNAL(finishedIndexing()), this, SLOT(slotIndexingFinished()) );

    m_fileIQ->suspend();

    // stop the slow queue on user activity
    KIdleTime* idleTime = KIdleTime::instance();
    idleTime->addIdleTimeout( 1000 * 60 * 2 ); // 2 min

    connect( idleTime, SIGNAL(timeoutReached(int)), this, SLOT(slotIdleTimeoutReached()) );
    connect( idleTime, SIGNAL(resumingFromIdle()), m_fileIQ, SLOT(suspend()) );
}


Nepomuk2::IndexScheduler::~IndexScheduler()
{
}

void Nepomuk2::IndexScheduler::slotIdleTimeoutReached()
{
    kDebug() << "Resuming File indexing queue";
    m_fileIQ->resume();
    KIdleTime::instance()->catchNextResumeEvent();
}


void Nepomuk2::IndexScheduler::suspend()
{
    if ( !m_suspended ) {
        m_suspended = true;
        if( m_cleaner ) {
            m_cleaner->suspend();
        }

        m_basicIQ->suspend();
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

        m_basicIQ->resume();
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

// WARNING: We are only returning files from the basic indexing queue over here
//          Maybe we should also take the file indexing queue into consideration? Then again
//          it only works when idle, so the user should never typically see it.
QString Nepomuk2::IndexScheduler::currentFolder() const
{
    return KUrl(m_basicIQ->currentUrl()).directory();
}

QString Nepomuk2::IndexScheduler::currentFile() const
{
    return m_basicIQ->currentUrl().toLocalFile();
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


void Nepomuk2::IndexScheduler::slotCleaningDone()
{
    m_cleaner = 0;
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
    foreach( const QString& f, FileIndexerConfig::self()->includeFolders() ) {
        m_basicIQ->enqueue( f, flags );
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
    m_basicIQ->enqueue( path );
}

void Nepomuk2::IndexScheduler::slotIndexingFinished()
{
    if( m_basicIQ->isEmpty() && m_fileIQ->isEmpty() )
        emit indexingDone();
}


#include "indexscheduler.moc"
