/* This file is part of the KDE Project
   Copyright (c) 2008-2011 Sebastian Trueg <trueg@kde.org>

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

#include "servicecontrol.h"
#include "servicecontroladaptor.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QTextStream>

#include "nepomukservice.h"


Nepomuk2::ServiceControl::ServiceControl( const QString& serviceName, const KService::Ptr& service, QObject* parent )
    : QObject( parent ),
      m_serviceName( serviceName ),
      m_service( service ),
      m_nepomukServiceModule( 0 ),
      m_initialized( false )
{
    m_description = service->comment();
    m_readableName = service->name();
}


Nepomuk2::ServiceControl::~ServiceControl()
{
}


void Nepomuk2::ServiceControl::setServiceInitialized( bool success )
{
    m_initialized = success;
    emit serviceInitialized( success );
}


bool Nepomuk2::ServiceControl::isInitialized() const
{
    return m_initialized;
}

QString Nepomuk2::ServiceControl::description() const
{
    return m_description;
}

QString Nepomuk2::ServiceControl::name() const
{
    return m_readableName;
}

void Nepomuk2::ServiceControl::start()
{
    QTextStream s( stderr );

    // register the service interface
    // We need to do this before creating the module to ensure that
    // the server can catch the serviceInitialized signal
    // ====================================
    (void)new ServiceControlAdaptor( this );
    if( !QDBusConnection::sessionBus().registerObject( "/servicecontrol", this ) ) {
        s << "Failed to register dbus service " << dbusServiceName( m_serviceName ) << "." << endl;
        qApp->exit( ErrorFailedToStart );
        return;
    }

    if( !QDBusConnection::sessionBus().registerService( dbusServiceName( m_serviceName ) ) ) {
        s << "Failed to register dbus service " << dbusServiceName( m_serviceName ) << "." << endl;
        qApp->exit( ErrorFailedToStart );
        return;
    }


    // start the service
    // ====================================
    QString startErrorDescription;
    m_nepomukServiceModule = m_service->createInstance<Nepomuk2::Service>( this, QVariantList(), &startErrorDescription);
    if( !m_nepomukServiceModule ) {
        s << "Failed to start service " << m_serviceName << " ("<< startErrorDescription << ")." << endl;
        qApp->exit( ErrorFailedToStart );
        return;
    }

    // register the service
    // ====================================
    QDBusConnection::sessionBus().registerObject( '/' + m_serviceName,
                                                  m_nepomukServiceModule,
                                                  QDBusConnection::ExportScriptableSlots |
                                                  QDBusConnection::ExportScriptableProperties |
                                                  QDBusConnection::ExportAdaptors);
    // Delete the QApplication after the m_nepomukServiceModule
    // Note: if services need QCoreApplication to hang around for
    // longer they can disconnect this signal.
    connect( m_nepomukServiceModule, SIGNAL( destroyed() ),
             QCoreApplication::instance(), SLOT( quit() ) );
}


void Nepomuk2::ServiceControl::shutdown()
{
    delete m_nepomukServiceModule;
    m_nepomukServiceModule = 0;
}

QString Nepomuk2::ServiceControl::dbusServiceName( const QString& serviceName )
{
    return QString("org.kde.nepomuk.services.%1").arg(serviceName);
}

#include "servicecontrol.moc"
