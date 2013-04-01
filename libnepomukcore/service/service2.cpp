/* This file is part of the KDE Project
   Copyright (c) 2008 Sebastian Trueg <trueg@kde.org>
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

#include "service2.h"
#include "servicecontrol2.h"
#include "nepomukversion.h"
#include "../../servicestub/priority.h"

#include <QtCore/QTimer>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusConnectionInterface>

#include <KAboutData>
#include <KDebug>
#include <KService>

#include <signal.h>

namespace {
#ifndef Q_OS_WIN
    void signalHandler( int signal )
    {
        switch( signal ) {
        case SIGHUP:
        case SIGQUIT:
        case SIGINT:
            QCoreApplication::exit( 0 );
        }
    }
#endif

    void installSignalHandler() {
#ifndef Q_OS_WIN
        struct sigaction sa;
        ::memset( &sa, 0, sizeof( sa ) );
        sa.sa_handler = signalHandler;
        sigaction( SIGHUP, &sa, 0 );
        sigaction( SIGINT, &sa, 0 );
        sigaction( SIGQUIT, &sa, 0 );
#endif
    }
}

class Nepomuk2::Service2::Private
{
public:
    Private( Service2* s ) : q(s) {}

    Service2* q;
    ServiceControl2* m_serviceControl;

    bool delayedInitialization;

    QString serviceName;
    QString readableName;
    QString description;

    bool loadDetails();
    bool configurePriority();
    bool createDBusInterfaces();
};


Nepomuk2::Service2::Service2( QObject* parent, bool delayedInitialization )
    : QObject( parent ),
      d( new Private(this) )
{
    d->m_serviceControl = 0;
    d->delayedInitialization = delayedInitialization;
}


Nepomuk2::Service2::~Service2()
{
    delete d;
}


QString Nepomuk2::Service2::name()
{
    return d->readableName;
}

QString Nepomuk2::Service2::description()
{
    return d->description;
}


void Nepomuk2::Service2::setServiceInitialized( bool success )
{
    QMetaObject::invokeMethod( d->m_serviceControl,
                               "setServiceInitialized",
                               Qt::QueuedConnection,  // needs to be queued to give the service time to register with DBus
                               Q_ARG(bool, success) );
}

bool Nepomuk2::Service2::Private::loadDetails()
{
    KService::Ptr servicePtr = KService::serviceByDesktopName( serviceName );
    if( servicePtr.isNull() )
        return false;

    readableName = servicePtr->name();
    description = servicePtr->comment();

    return true;
}


bool Nepomuk2::Service2::Private::configurePriority()
{
    // Lower our priority by default which makes sense for most services since Nepomuk
    // does not want to get in the way of the user
    // TODO: make it configurable
    // ====================================
    if ( !lowerPriority() ) {
        kDebug() << "Failed to lower priority.";
        return false;
    }
    if ( !lowerSchedulingPriority() ) {
        kDebug() << "Failed to lower scheduling priority.";
        return false;
    }
    if ( !lowerIOPriority() ) {
        kDebug() << "Failed to lower io priority.";
        return false;
    }

    return true;
}

bool Nepomuk2::Service2::Private::createDBusInterfaces()
{
    QTextStream s( stderr );

    // register the service
    // ====================================
    QDBusConnection bus = QDBusConnection::sessionBus();
    if( !bus.registerService( q->dbusServiceName() ) ) {
        s << "Failed to register dbus service " << q->dbusServiceName() << "." << endl;
        return false;
    }

    bus.registerObject( '/' + serviceName, q,
                        QDBusConnection::ExportScriptableSlots |
                        QDBusConnection::ExportScriptableSignals |
                        QDBusConnection::ExportScriptableProperties |
                        QDBusConnection::ExportAdaptors);

    m_serviceControl = new ServiceControl2( q );
    if( m_serviceControl->failedToStart() )
        return false;

    return true;
}

QString Nepomuk2::Service2::dbusServiceName()
{
    return dbusServiceName( d->serviceName );
}

// static
QString Nepomuk2::Service2::dbusServiceName(const QString& serviceName)
{
    return QString("org.kde.nepomuk.services.%1").arg(serviceName.toLower());
}

bool Nepomuk2::Service2::initCommon(const QString& name)
{
    d->serviceName = name;

    if( !d->loadDetails() )
        return false;

    if( !d->configurePriority() )
        return false;

    if( !d->createDBusInterfaces() )
        return false;

    if ( !d->delayedInitialization )
        setServiceInitialized( true );

    installSignalHandler();

    return true;
}



#include "service2.moc"
