/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2011 Vishesh Handa <handa.vish@gmail.com>
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

#include "storeresourcesjob.h"
#include "datamanagementinterface.h"
#include "simpleresourcegraph.h"
#include "dbustypes.h"
#include "genericdatamanagementjob_p.h"

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusPendingCallWatcher>

#include <QtCore/QHash>
#include <QtCore/QUrl>

#include <KComponentData>
#include <KUrl>
#include <KDebug>

class Nepomuk2::StoreResourcesJob::Private {
public:
    Nepomuk2::StoreResourcesJob *q;
    QHash<QUrl, QUrl> m_mappings;

    void _k_slotDBusCallFinished(QDBusPendingCallWatcher* watcher);
};

Nepomuk2::StoreResourcesJob::StoreResourcesJob(const Nepomuk2::SimpleResourceGraph& resources,
                                              Nepomuk2::StoreIdentificationMode identificationMode,
                                              Nepomuk2::StoreResourcesFlags flags,
                                              const QHash< QUrl, QVariant >& additionalMetadata,
                                              const KComponentData& component)
    : KJob(),
      d( new Nepomuk2::StoreResourcesJob::Private )
{
    d->q = this;

    org::kde::nepomuk::DataManagement* dms = Nepomuk2::dataManagementDBusInterface();
    QDBusPendingCallWatcher* dbusCallWatcher
    = new QDBusPendingCallWatcher(dms->storeResources( resources.toList(), identificationMode,
                                                       flags, additionalMetadata,
                                                       component.componentName() ));

    connect(dbusCallWatcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            this, SLOT(_k_slotDBusCallFinished(QDBusPendingCallWatcher*)));
}

Nepomuk2::StoreResourcesJob::~StoreResourcesJob()
{
    delete d;
}

void Nepomuk2::StoreResourcesJob::start()
{
    // Nothing to do
}

QHash< QUrl, QUrl > Nepomuk2::StoreResourcesJob::mappings() const
{
    return d->m_mappings;
}

void Nepomuk2::StoreResourcesJob::Private::_k_slotDBusCallFinished(QDBusPendingCallWatcher* watcher)
{
    QDBusPendingReply< QHash<QString,QString> > reply = *watcher;
    if (reply.isError()) {
        QDBusError error = reply.error();
        q->setError(int(error.type()));
        q->setErrorText(error.message());
    }
    else {
        m_mappings.clear();
        QHash<QString, QString> mappings = reply.value();
        QHash<QString, QString>::const_iterator it = mappings.constBegin();
        for( ; it != mappings.constEnd(); it++ ) {
            m_mappings.insert( KUrl( it.key() ), KUrl( it.value() ) );
        }
    }

    watcher->deleteLater();
    q->emitResult();
}

#include "storeresourcesjob.moc"
