/* This file is part of the KDE Project
   Copyright (c) 2008-2010 Sebastian Trueg <trueg@kde.org>
   Copyright (c) 2010 Vishesh Handa <handa.vish@gmail.com>

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

#ifndef _NEPOMUK_FILEINDEXER_SERVICE_H_
#define _NEPOMUK_FILEINDEXER_SERVICE_H_

#include "nepomukservice.h"
#include <QtCore/QTimer>

namespace Nepomuk2 {

    class IndexScheduler;

    /**
     * Service controlling the file indexer
     */
    class FileIndexer : public Nepomuk2::Service
    {
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.nepomuk.FileIndexer")

    public:
        FileIndexer( QObject* parent = 0, const QList<QVariant>& args = QList<QVariant>() );
        ~FileIndexer();

    Q_SIGNALS:
        void statusStringChanged();

        Q_SCRIPTABLE void statusChanged();
        Q_SCRIPTABLE void indexingFolder( const QString& path );
        Q_SCRIPTABLE void indexingStarted();
        Q_SCRIPTABLE void indexingStopped();
        Q_SCRIPTABLE void fileIndexingDone();

    public Q_SLOTS:
        /**
         * \return A user readable status string. Includes the currently indexed folder.
         */
        Q_SCRIPTABLE QString userStatusString() const;

        /**
         * Simplified status string without details.
         */
        Q_SCRIPTABLE QString simpleUserStatusString() const;

        Q_SCRIPTABLE bool isSuspended() const;
        Q_SCRIPTABLE bool isIndexing() const;
        Q_SCRIPTABLE bool isCleaning() const;

        Q_SCRIPTABLE void resume() const;
        Q_SCRIPTABLE void suspend() const;
        Q_SCRIPTABLE void setSuspended( bool );

        Q_SCRIPTABLE QString currentFolder() const;
        Q_SCRIPTABLE QString currentFile() const;

        /**
         * Update folder \a path if it is configured to be indexed.
         */
        Q_SCRIPTABLE void updateFolder( const QString& path, bool recursive, bool forced );

        /**
         * Update all folders configured to be indexed.
         */
        Q_SCRIPTABLE void updateAllFolders( bool forced );

        /**
         * Index a folder independent of its configuration status.
         */
        Q_SCRIPTABLE void indexFolder( const QString& path, bool recursive, bool forced );

        /**
         * Index a specific file
         */
        Q_SCRIPTABLE void indexFile( const QString& path );

    private Q_SLOTS:
        void updateWatches();
        void slotIndexingDone();

    private:
        QString userStatusString( bool simple ) const;

        IndexScheduler* m_indexScheduler;
    };
}

#endif
