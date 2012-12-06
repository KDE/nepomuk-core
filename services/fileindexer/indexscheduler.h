/* This file is part of the KDE Project
   Copyright (c) 2008-2010 Sebastian Trueg <trueg@kde.org>
   Copyright (c) 2012 Vishesh Handa <me@vhanda.in>

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

#ifndef _NEPOMUK_FILEINDEXER_INDEX_SCHEDULER_H_
#define _NEPOMUK_FILEINDEXER_INDEX_SCHEDULER_H_

#include "basicindexingqueue.h"

#include <QtCore/QQueue>
#include <QtCore/QDateTime>
#include <QtCore/QTimer>

#include <KUrl>

class KJob;
class QFileInfo;
class QByteArray;

namespace Nepomuk2 {

    class IndexCleaner;
    class FileIndexingQueue;
    class EventMonitor;

    /**
     * The IndexScheduler performs the normal indexing,
     * ie. the initial indexing and the timed updates
     * of all files.
     *
     * Events are not handled.
     */
    class IndexScheduler : public QObject
    {
        Q_OBJECT

    public:
        IndexScheduler( QObject* parent=0 );
        ~IndexScheduler();

        bool isSuspended() const;
        bool isIndexing() const;

        /**
         * The folder currently being indexed. Empty if not indexing.
         * If suspended the folder might still be set!
         */
        QString currentFolder() const;

        /**
         * The file currently being indexed. Empty if not indexing.
         * If suspended the file might still be set!
         *
         * This file should always be a child of currentFolder().
         */
        QString currentFile() const;

        /**
         * The UpdateDirFlags of the the current url that is being
         * indexed.
         */
        UpdateDirFlags currentFlags() const;

    public Q_SLOTS:
        void suspend();
        void resume();

        void setSuspended( bool );

        /**
         * Slot to connect to certain event systems like KDirNotify
         * or KDirWatch
         *
         * Updates a complete folder. Makes sense for
         * signals like KDirWatch::dirty.
         *
         * \param path The folder to update
         * \param flags Additional flags, all except AutoUpdateFolder are supported. This
         * also means that by default \p path is updated non-recursively.
         */
        void updateDir( const QString& path, UpdateDirFlags flags = NoUpdateFlags );

        /**
         * Updates all configured folders.
         */
        void updateAll( bool forceUpdate = false );

        /**
         * Analyze the one file without conditions.
         */
        void analyzeFile( const QString& path );

    Q_SIGNALS:
        // Indexing State
        void indexingStarted();
        void indexingStopped();
        void indexingStateChanged( bool indexing );

        // Finer Index state
        void indexingFolder( const QString& );
        void indexingFile( const QString & );

        // Emitted on calling suspend/resume
        void indexingSuspended( bool suspended );

    private Q_SLOTS:
        void slotConfigChanged();
        void slotCleaningDone();

        void slotBeginIndexingFile(const QUrl& url);

        void slotStartedIndexing();
        void slotFinishedIndexing();

        // Event Monitor integration
        void slotScheduleIndexing();

    private:
        /**
         * It first indexes \p dir. Then it checks all the files in \p dir
         * against the configuration and the data in Nepomuk to fill
         * m_filesToUpdate. No actual indexing is done besides \p dir
         * itself.
         */
        void analyzeDir( const QString& dir, UpdateDirFlags flags );

        void queueAllFoldersForUpdate( bool forceUpdate = false );

        // emits indexingStarted or indexingStopped based on parameter. Makes sure
        // no signal is emitted twice
        void setIndexingStarted( bool started );

        bool m_suspended;
        bool m_indexing;

        IndexCleaner* m_cleaner;

        // Queues
        BasicIndexingQueue* m_basicIQ;
        FileIndexingQueue* m_fileIQ;

        EventMonitor* m_eventMonitor;

        enum State {
            State_Normal,
            State_OnBattery,
            State_UserIdle,
            State_LowDiskSpace,
            State_Suspended
        };
        State m_state;
    };
}


#endif

