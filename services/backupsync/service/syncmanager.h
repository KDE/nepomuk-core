/*
    This file is part of the Nepomuk KDE project.
    Copyright (C) 2010  Vishesh Handa <handa.vish@gmail.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) version 3, or any
   later version accepted by the membership of KDE e.V. (or its
   successor approved by the membership of KDE e.V.), which shall
   act as a proxy defined in Section 6 of version 3 of the license.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SYNCMANAGER_H
#define SYNCMANAGER_H

#include <QObject>
#include <QUrl>
#include <QDateTime>

namespace Nepomuk {

    class SyncFile;

    class SyncManager : public QObject
    {
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.nepomuk.services.nepomukbackupsync.SyncManager")

    public :
        SyncManager(QObject * parent = 0);
        virtual ~SyncManager();

    public slots:
        void createSyncFile( const QString & url, const QString& startTime );
        void createSyncFile( const QUrl& outputUrl, const QList<QString> & nieUrls );
        void createSyncFile( const QUrl& outputUrl, QSet<QUrl>& nepomukUris, const QDateTime & min );
        void createFirstSyncFile( const QUrl& outputUrl ) const;

    private :
    };
}

#endif // SYNCMANAGER_H