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


#ifndef BACKUPRESTORATIONJOB_H
#define BACKUPRESTORATIONJOB_H

#include <KJob>
#include <Soprano/Model>
#include <QtCore/QUrl>

namespace Nepomuk2 {

    class Storage;

    class BackupRestorationJob : public KJob
    {
        Q_OBJECT
    public:
        explicit BackupRestorationJob(Nepomuk2::Storage* storageService, const QUrl& url, QObject* parent = 0);
        virtual void start();

        QString oldRepositoryPath() const { return m_oldRepoPath; }

    private slots:
        void slotRestRepo(const QString& old, const QString& newPath);
        void doWork();

    private:
        Soprano::Model* m_model;
        Storage* m_storageService;

        QUrl m_url;
        QString m_oldRepoPath;
    };

}

#endif // BACKUPRESTORATIONJOB_H
