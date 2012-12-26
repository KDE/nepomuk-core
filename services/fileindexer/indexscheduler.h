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

#include "basicindexingqueue.h" // Required for UpdateDirFlags

namespace Nepomuk2 {

    class IndexCleaner;
    class FileIndexingQueue;
    class WebMinerIndexingQueue;
    class EventMonitor;

    /**
     * The IndexScheduler is responsible for controlling the indexing
     * queues and reacting to events. It contains an EventMonitor
     * and listens for events such as power management, battery and
     * disk space.
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

        QUrl currentUrl() const;

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

        void basicIndexingDone();

        // Finer Index state
        void indexingFolder( const QString& );
        void indexingFile( const QString & );

        // Emitted on calling suspend/resume
        void indexingSuspended( bool suspended );

    private Q_SLOTS:
        void slotConfigChanged();
        void slotCleaningDone();

        void slotBeginIndexingFile(const QUrl& url);
        void slotEndIndexingFile(const QUrl& url);

        void slotStartedIndexing();
        void slotFinishedIndexing();

        // Event Monitor integration
        void slotScheduleIndexing();

    private:
        void queueAllFoldersForUpdate( bool forceUpdate = false );

        // emits indexingStarted or indexingStopped based on parameter. Makes sure
        // no signal is emitted twice
        void setIndexingStarted( bool started );

        bool m_indexing;

        IndexCleaner* m_cleaner;

        // Queues
        BasicIndexingQueue* m_basicIQ;
        FileIndexingQueue* m_fileIQ;
        WebMinerIndexingQueue *m_webIQ;

        EventMonitor* m_eventMonitor;

        enum State {
            State_Normal,
            State_OnBattery,
            State_UserIdle,
            State_LowDiskSpace,
            State_Suspended
        };
        State m_state;

        bool m_shouldSuspendFileIQOnNormal;
    };
}


#endif

