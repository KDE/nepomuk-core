/* This file is part of the KDE Project
   Copyright (c) 2008 Sebastian Trueg <trueg@kde.org>

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

#ifndef _NEPOMUK_STORAGE_H_
#define _NEPOMUK_STORAGE_H_

#include "service2.h"
#include <Soprano/Server/ServerCore>

class KJob;

namespace Nepomuk2 {

    namespace Query {
        class QueryService;
    }
    class BackupManager;
    class Repository;

    class Storage : public Service2
    {
        Q_OBJECT
        Q_CLASSINFO( "D-Bus Interface", "org.kde.nepomuk.Storage" )

    public:
        Storage();
        ~Storage();

        Soprano::Model* model();

    public Q_SLOTS:
        Q_SCRIPTABLE QString usedSopranoBackend() const;

        /**
         * Switches off the Repository and DELETES ALL ITS DATA.
         * Please use with caution.
         *
         * After deleting the repository it does not open the interfaces again, you
         * need to manually call openPublicInterfaces
         *
         * \warning This deletes all your Nepomuk data
         */
        Q_SCRIPTABLE void resetRepository();

        Q_SCRIPTABLE void closePublicInterfaces();
        Q_SCRIPTABLE void openPublicInterfaces();

        Q_SCRIPTABLE void migrateGraphs();
        Q_SCRIPTABLE void migrateGraphsByBackup();
    signals:
        // Used by the BackupManager to know we're back online after a reset
        Q_SCRIPTABLE void resetRepositoryDone();

        Q_SCRIPTABLE void migrateGraphsDone();
        Q_SCRIPTABLE void migrateGraphsPercent(int percent);

    private Q_SLOTS:
        void slotRepositoryLoaded( Repository* repo, bool success );
        void slotRepositoryClosed();
        void slotRepositoryClosedAfterReset();

        void slotMigrationPercent(KJob*, ulong percent);
        void slotMigrationDone();

        void slotMigrationBackupProgress(int percent);
        void slotMigrationRestoreProgress(int percent);
        void slotMigrationBackupDone();
        void slotMigrationRestoreDone();
        void slotMigrationDeletionDone();

    private:
        Soprano::Server::ServerCore* m_localServer;
        Repository* m_repository;

        Query::QueryService* m_queryService;
        BackupManager* m_backupManager;

        bool m_resetInProgress;

        bool dataMigrationRequired();
        void setDataMigrated();
        bool hasMigrationData();
    };
}

#endif
