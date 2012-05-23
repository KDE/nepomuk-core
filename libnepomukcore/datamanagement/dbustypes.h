/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2011-2012 Sebastian Trueg <trueg@kde.org>

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

#ifndef DBUSTYPES_H
#define DBUSTYPES_H

#include <QtCore/QMetaType>
#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtDBus/QDBusVariant>
#include <QtDBus/QDBusArgument>

#include "simpleresource.h"

Q_DECLARE_METATYPE(Nepomuk2::PropertyHash)
Q_DECLARE_METATYPE(Nepomuk2::SimpleResource)
Q_DECLARE_METATYPE(QList<Nepomuk2::SimpleResource>)

//CAUTION: Q_DECLARE_METATYPE doesn't accept template arguments like QHash<T, T>
typedef QHash<QString, QString> __nepomuk_QHashQStringQString;
Q_DECLARE_METATYPE( __nepomuk_QHashQStringQString )

namespace Nepomuk2 {
    namespace DBus {
        QString convertUri(const QUrl& uri);
        QStringList convertUriList(const QList<QUrl>& uris);

        /// Convert QDBusArguments variants into QUrl, QDate, QTime, and QDateTime variants
        QVariant resolveDBusArguments(const QVariant& v);
        QVariantList resolveDBusArguments(const QVariantList& l);

        /// Replaces KUrl with QUrl for DBus marshalling.
        QVariantList normalizeVariantList(const QVariantList& l);

        void registerDBusTypes();
    }
}

QDBusArgument& operator<<( QDBusArgument& arg, const QUrl& url );
const QDBusArgument& operator>>( const QDBusArgument& arg, QUrl& url );
QDBusArgument& operator<<( QDBusArgument& arg, const Nepomuk2::PropertyHash& ph );
const QDBusArgument& operator>>( const QDBusArgument& arg, Nepomuk2::PropertyHash& ph );
QDBusArgument& operator<<( QDBusArgument& arg, const Nepomuk2::SimpleResource& res );
const QDBusArgument& operator>>( const QDBusArgument& arg, Nepomuk2::SimpleResource& res );

#endif // DBUSTYPES_H
