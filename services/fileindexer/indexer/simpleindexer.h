/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2012 Vishesh Handa <me@vhanda.in>

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

#ifndef SIMPLEINDEXER_H
#define SIMPLEINDEXER_H

#include <QtCore/QUrl>

#include "simpleresource.h"
#include "simpleresourcegraph.h"

#include <KJob>
#include <KUrl>

namespace Nepomuk2 {

    class SimpleResource;

    class SimpleIndexingJob : public KJob
    {
        Q_OBJECT
    public:
        SimpleIndexingJob(const QUrl& fileUrl, QObject* parent = 0);
        SimpleIndexingJob(const QUrl& fileUrl, const QString& mimeType, QObject* parent = 0);

        virtual void start();

        QUrl uri();
        QString mimeType();

        /**
         * Fills a new SimpleResource with the basic data of the fileUrl such
         * as type, mimetype, and other stat info.
         *
         * \param mimeType Indicates the mimetype of the fileUrl. If a non emtpy value has been
         *                 provided, then it will be used. Otherwise \p mimeType will be updated
         *                 to reflect the new value
         */
        static SimpleResource createSimpleResource( const KUrl& fileUrl, QString* mimeType );

    private slots:
        void slotJobFinished(KJob* job);

    private:
        KUrl m_nieUrl;
        QUrl m_resUri;
        QString m_mimeType;

        static QSet<QUrl> typesForMimeType(const QString& mimeType);
    };
}

#endif // SIMPLEINDEXER_H
