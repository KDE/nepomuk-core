/*
 * <one line to give the library's name and an idea of what it does.>
 * Copyright 2013  Vishesh Handa <me@vhanda.in>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "systray.h"

#include <KLocalizedString>
#include <KTemporaryFile>
#include <KDebug>

using namespace Nepomuk2;

SysTray::SysTray(QObject* parent): KStatusNotifierItem(parent)
{
    setStatus( KStatusNotifierItem::Active );
    setIconByName( "nepomuk" );
    setCategory( KStatusNotifierItem::SystemServices );
    setTitle( i18n("Optimizing search database") );
    setToolTip( "nepomuk", i18n("Optimizing search database"), QString() );

    m_storageService = new StorageService( QLatin1String("org.kde.NepomukStorage"),
                                           QLatin1String("/nepomukstorage"),
                                           QDBusConnection::sessionBus(), this);
    connect( m_storageService, SIGNAL(migrateGraphsPercent(int)), this, SLOT(setPercent(int)) );
    connect( m_storageService, SIGNAL(migrateGraphsDone()), this, SLOT(slotMigrationDone()) );

    m_storageService->migrateGraphsByBackup();
}

SysTray::~SysTray()
{
}

void SysTray::setPercent(int percent)
{
    if( percent == -1 )
        setToolTipSubTitle( i18n("In Progress") );
    else
        setToolTipSubTitle( i18n("Progress: %1%", percent ) );
}

void SysTray::slotMigrationDone()
{
    QCoreApplication::instance()->quit();
}


