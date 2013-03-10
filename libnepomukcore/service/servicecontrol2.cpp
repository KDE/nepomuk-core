/* This file is part of the KDE Project
   Copyright (c) 2008-2011 Sebastian Trueg <trueg@kde.org>
   Copyright (c) 2013 Vishesh Handa <me@vhanda.in>

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

#include "servicecontrol2.h"
#include "servicecontroladaptor.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QTextStream>

#include "nepomukservice.h"
#include "service2.h"

using namespace Nepomuk2;

ServiceControl2::ServiceControl2(Service2* service)
    : QObject(service)
    , m_service( service )
    , m_initialized( false )
    , m_failed( false )
{
    QTextStream s( stderr );

    (void)new ServiceControlAdaptor( this );
    QDBusConnection bus = QDBusConnection::sessionBus();
    if( !bus.registerObject( "/servicecontrol", this ) ) {
        s << "Failed to register dbus service " << service->dbusServiceName() << "." << endl;
        m_failed = true;
        return;
    }
}


ServiceControl2::~ServiceControl2()
{
}


void ServiceControl2::setServiceInitialized( bool success )
{
    m_initialized = success;
    emit serviceInitialized( success );
}


bool ServiceControl2::isInitialized() const
{
    return m_initialized;
}

QString ServiceControl2::name() const
{
    return m_service->name();
}

QString ServiceControl2::description() const
{
    return m_service->description();
}

bool ServiceControl2::failedToStart()
{
    return m_failed;
}

void ServiceControl2::shutdown()
{
    m_service->deleteLater();
}

#include "servicecontrol2.moc"
