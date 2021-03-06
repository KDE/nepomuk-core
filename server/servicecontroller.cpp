/* This file is part of the KDE Project
   Copyright (c) 2008 Sebastian Trueg <trueg@kde.org>

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

#include "servicecontroller.h"
#include "processcontrol.h"
#include "servicecontrolinterface.h"
#include "nepomukserver.h"

#include <QtCore/QTimer>

#include <QtDBus/QDBusServiceWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusPendingCallWatcher>

#include <KStandardDirs>
#include <KConfigGroup>
#include <KDebug>


namespace {
    inline QString dbusServiceName( const QString& serviceName ) {
        return QString("org.kde.nepomuk.services.%1").arg(serviceName);
    }
}


class Nepomuk2::ServiceController::Private
{
public:
    Private()
        : processControl( 0 ),
          serviceControlInterface( 0 ),
          dbusServiceWatcher( 0 ),
          attached( false ),
          started( false ),
          initialized( false ),
          failedToInitialize( false ),
          currentState(ServiceController::StateStopped) {
    }

    KService::Ptr service;
    bool autostart;
    bool startOnDemand;
    bool runOnce;

    ProcessControl* processControl;
    OrgKdeNepomukServiceControlInterface* serviceControlInterface;
    QDBusServiceWatcher* dbusServiceWatcher;

    // true if we attached to an already running instance instead of
    // starting our own (in that case processControl will be 0)
    bool attached;

    // true if we were asked to start the service
    bool started;

    bool initialized;
    bool failedToInitialize;
    ServiceController::State currentState;

    void init( KService::Ptr service );
    void reset();
};


void Nepomuk2::ServiceController::Private::init( KService::Ptr s )
{
    service = s;
    autostart = service->property( "X-KDE-Nepomuk-autostart", QVariant::Bool ).toBool();
    KConfigGroup cg( Server::self()->config(), QString("Service-%1").arg(service->desktopEntryName()) );
    autostart = cg.readEntry( "autostart", autostart );

    QVariant p = service->property( "X-KDE-Nepomuk-start-on-demand", QVariant::Bool );
    startOnDemand = ( p.isValid() ? p.toBool() : false );

    p = service->property( "X-KDE-Nepomuk-run-once", QVariant::Bool );
    runOnce = ( p.isValid() ? p.toBool() : false );

    initialized = false;
}


void Nepomuk2::ServiceController::Private::reset()
{
    initialized = false;
    attached = false;
    started = false;
    currentState = ServiceController::StateStopped;
    failedToInitialize = false;
    delete serviceControlInterface;
    serviceControlInterface = 0;
}


Nepomuk2::ServiceController::ServiceController( KService::Ptr service, QObject* parent )
    : QObject( parent ),
      d(new Private())
{
    d->init( service );

    d->dbusServiceWatcher = new QDBusServiceWatcher( dbusServiceName(name()),
                                                     QDBusConnection::sessionBus(),
                                                     QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration,
                                                     this );
    connect( d->dbusServiceWatcher, SIGNAL( serviceRegistered( QString ) ),
             this, SLOT( slotServiceRegistered( QString ) ) );
    connect( d->dbusServiceWatcher, SIGNAL( serviceUnregistered( QString ) ),
             this, SLOT( slotServiceUnregistered( QString ) ) );
}


Nepomuk2::ServiceController::~ServiceController()
{
    delete d;
}


KService::Ptr Nepomuk2::ServiceController::service() const
{
    return d->service;
}


QString Nepomuk2::ServiceController::name() const
{
    return d->service->desktopEntryName();
}


QStringList Nepomuk2::ServiceController::dependencies() const
{
    QStringList deps = d->service->property( "X-KDE-Nepomuk-dependencies", QVariant::StringList ).toStringList();
    if ( deps.isEmpty() ) {
        deps.append( "nepomukstorage" );
    }
    deps.removeAll( name() );
    return deps;
}


void Nepomuk2::ServiceController::setAutostart( bool enable )
{
    KConfigGroup cg( Server::self()->config(), QString("Service-%1").arg(name()) );
    cg.writeEntry( "autostart", enable );
}


bool Nepomuk2::ServiceController::autostart() const
{
    return d->autostart;
}


bool Nepomuk2::ServiceController::startOnDemand() const
{
    return d->startOnDemand;
}


bool Nepomuk2::ServiceController::runOnce() const
{
    return d->runOnce;
}


void Nepomuk2::ServiceController::start()
{
    if( d->currentState == StateStopped ) {
        d->reset();
        d->started = true;

        // check if the service is already running, ie. has been started by someone else or by a crashed instance of the server
        // we cannot rely on the auto-restart feature of ProcessControl here. So we handle that completely via DBus signals
        if( QDBusConnection::sessionBus().interface()->isServiceRegistered( dbusServiceName( name() ) ) ) {
            kDebug() << "Attaching to already running service" << name();
            d->attached = true;
            d->currentState = StateRunning;
            createServiceControlInterface();
        }
        else {
            kDebug() << "Starting" << name();

            d->currentState = StateStarting;

            if( !d->processControl ) {
                d->processControl = new ProcessControl( this );
                connect( d->processControl, SIGNAL( finished( bool ) ),
                         this, SLOT( slotProcessFinished( bool ) ) );
            }

            // we wait for the service to be registered with DBus before creating the service interface
            QString exec = d->service->exec();
            if( exec.isEmpty() ) {
                d->processControl->start( KStandardDirs::findExe( "nepomukservicestub" ),
                                          QStringList() << name(),
                                          ProcessControl::RestartOnCrash );
            }
            else {
                d->processControl->start( KStandardDirs::findExe( exec ),
                                          QStringList(),
                                          ProcessControl::RestartOnCrash );
            }
        }
    }
}


void Nepomuk2::ServiceController::stop()
{
    if( d->currentState == StateRunning ||
        d->currentState == StateStarting ) {
        kDebug() << "Stopping" << name();

        d->attached = false;
        d->started = false;
        d->currentState = StateStopping;

        if ( d->serviceControlInterface ) {
            d->serviceControlInterface->shutdown();
        }
        else if( d->processControl ) {
            // make sure we do not stop a process which has not been started properly yet.
            // that would result in crashes
            d->processControl->waitForStarted();
            d->processControl->setCrashPolicy( ProcessControl::StopOnCrash );
            d->processControl->terminate();
        }
        else {
            kDebug() << "Cannot shut down service process.";
        }
    }
}


bool Nepomuk2::ServiceController::isRunning() const
{
    return( d->attached || ( d->processControl ? d->processControl->isRunning() : false ) );
}


bool Nepomuk2::ServiceController::isInitialized() const
{
    return d->initialized;
}


void Nepomuk2::ServiceController::slotProcessFinished( bool /*clean*/ )
{
    kDebug() << "Service" << name() << "went down";
    d->reset();
    emit serviceStopped( this );
}


void Nepomuk2::ServiceController::slotServiceRegistered( const QString& serviceName )
{
    if( serviceName == dbusServiceName( name() ) ) {
        d->currentState = StateRunning;
        kDebug() << serviceName;
        if( !d->processControl || !d->processControl->isRunning() ) {
            d->attached = true;
        }
        createServiceControlInterface();
    }
}

void Nepomuk2::ServiceController::slotServiceUnregistered( const QString& serviceName )
{
    // an attached service was not started through ProcessControl. Thus, we cannot rely
    // on its restart-on-crash feature and have to do it manually. Afterwards it is back
    // to normal
    if( serviceName == dbusServiceName( name() ) ) {
        if( d->attached ) {
            emit serviceStopped( this );
            if( d->started ) {
                kDebug() << "Attached service" << name() << "went down. Restarting ourselves.";
                start();
            }
            else {
                d->reset();
            }
        }
    }
}


void Nepomuk2::ServiceController::createServiceControlInterface()
{
    if(!d->attached && !d->started)
        return;

    delete d->serviceControlInterface;
    d->serviceControlInterface = new OrgKdeNepomukServiceControlInterface( dbusServiceName( name() ),
                                                                           QLatin1String("/servicecontrol"),
                                                                           QDBusConnection::sessionBus(),
                                                                          this );
    QDBusPendingCallWatcher* isInitializedWatcher = new QDBusPendingCallWatcher(d->serviceControlInterface->isInitialized(),
                                                                                this);
    connect(isInitializedWatcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            this, SLOT(slotIsInitializedDBusCallFinished(QDBusPendingCallWatcher*)));
}


void Nepomuk2::ServiceController::slotServiceInitialized( bool success )
{
    if ( !d->initialized ) {
        if ( success ) {
            kDebug() << "Service" << name() << "initialized";
            d->initialized = true;
            emit serviceInitialized( this );

            if ( runOnce() ) {
                // we have been run once. Do not autostart next time
                KConfigGroup cg( Server::self()->config(), QString("Service-%1").arg(name()) );
                cg.writeEntry( "autostart", false );
            }
        }
        else {
            d->failedToInitialize = true;
            kDebug() << "Failed to initialize service" << name();
            // Do not stop because the storageservice now gets deinitialized when restoring a backup
            // or migrating the data
            //stop();
        }
    }
}

Nepomuk2::ServiceController::State Nepomuk2::ServiceController::state() const
{
    return d->currentState;
}

void Nepomuk2::ServiceController::slotIsInitializedDBusCallFinished(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<bool> isInitializedReply = *watcher;
    if( isInitializedReply.isError() ) {
        delete d->serviceControlInterface;
        d->serviceControlInterface = 0;
        kDebug() << "Failed to check service init state for" << name() << "Retrying.";
        QMetaObject::invokeMethod( this, "createServiceControlInterface", Qt::QueuedConnection );
    }
    else {
        if( isInitializedReply.value() ) {
            slotServiceInitialized( true );
        }
        else {
            kDebug() << "Service" << name() << "not initialized yet. Listening for signal.";
            connect( d->serviceControlInterface, SIGNAL( serviceInitialized( bool ) ),
                    this, SLOT( slotServiceInitialized( bool ) ),
                    Qt::QueuedConnection );
        }
    }

    watcher->deleteLater();
}

#include "servicecontroller.moc"
