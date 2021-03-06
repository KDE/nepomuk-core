/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2011 Sebastian Trueg <trueg@kde.org>

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

#include "genericdatamanagementjob_p.h"
#include "datamanagementinterface.h"
#include "dbustypes.h"
#include "kdbusconnectionpool.h"

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusPendingCallWatcher>

#include <QtCore/QVariant>
#include <QtCore/QHash>
#include <QThreadStorage>

#include <KDebug>

Nepomuk2::GenericDataManagementJob::GenericDataManagementJob(const char *methodName,
                                                            QGenericArgument val0,
                                                            QGenericArgument val1,
                                                            QGenericArgument val2,
                                                            QGenericArgument val3,
                                                            QGenericArgument val4,
                                                            QGenericArgument val5)
    : KJob(0)
{
    QDBusPendingReply<> reply;
    QMetaObject::invokeMethod(Nepomuk2::dataManagementDBusInterface(),
                              methodName,
                              Qt::DirectConnection,
                              Q_RETURN_ARG(QDBusPendingReply<> , reply),
                              val0,
                              val1,
                              val2,
                              val3,
                              val4,
                              val5);
    QDBusPendingCallWatcher* dbusCallWatcher = new QDBusPendingCallWatcher(reply);
    connect(dbusCallWatcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            this, SLOT(slotDBusCallFinished(QDBusPendingCallWatcher*)));
}

Nepomuk2::GenericDataManagementJob::~GenericDataManagementJob()
{
}

void Nepomuk2::GenericDataManagementJob::start()
{
    // do nothing
}

void Nepomuk2::GenericDataManagementJob::slotDBusCallFinished(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<> reply = *watcher;
    if (reply.isError()) {
        QDBusError error = reply.error();
        kDebug() << error;
        setError(int(error.type()));
        setErrorText(error.message());
    }
    delete watcher;
    emitResult();
}

QThreadStorage<OrgKdeNepomukDataManagementInterface *> s_perThreadDms;

OrgKdeNepomukDataManagementInterface* Nepomuk2::dataManagementDBusInterface()
{
  if (!s_perThreadDms.hasLocalData()) {
    // DBus types necessary for storeResources
    DBus::registerDBusTypes();

    s_perThreadDms.setLocalData(
      new org::kde::nepomuk::DataManagement(QLatin1String("org.kde.NepomukStorage"),
                                            QLatin1String("/datamanagement"),
                                            KDBusConnectionPool::threadConnection()));
  }
  return s_perThreadDms.localData();
}

#include "genericdatamanagementjob_p.moc"
