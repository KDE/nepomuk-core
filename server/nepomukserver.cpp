/* This file is part of the KDE Project
   Copyright (c) 2007 Sebastian Trueg <trueg@kde.org>

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

#include "nepomukserver.h"
#include "nepomukserveradaptor.h"
#include "nepomukserversettings.h"
#include "servicemanager.h"
#include "servicemanageradaptor.h"

#include <Soprano/Global>

#include <KConfig>
#include <KConfigGroup>
#include <KDebug>
#include <KGlobal>
#include <KStandardDirs>

#include <QtDBus/QDBusConnection>


Nepomuk2::Server* Nepomuk2::Server::s_self = 0;

Nepomuk2::Server::Server( QObject* parent )
    : QObject( parent ),
      m_fileIndexerServiceName( "nepomukfileindexer" ),
      m_currentState(StateDisabled)
{
    s_self = this;

    m_config = KSharedConfig::openConfig( "nepomukserverrc" );

    QDBusConnection::sessionBus().registerService( "org.kde.NepomukServer" );

    // register the nepomuk server adaptor
    (void)new NepomukServerAdaptor( this );
    QDBusConnection::sessionBus().registerObject( "/nepomukserver", this );

    // create the service manager.
    m_serviceManager = new ServiceManager( this );
    connect(m_serviceManager, SIGNAL(serviceInitialized(QString)),
            this, SLOT(slotServiceInitialized(QString)));
    connect(m_serviceManager, SIGNAL(serviceStopped(QString)),
            this, SLOT(slotServiceStopped(QString)));
    (void)new ServiceManagerAdaptor( m_serviceManager );

    // initialize according to config
    init();

    // Quit the server if Nepomuk is not running
    if( m_currentState == StateDisabled ) {
        quit();
    }
}


Nepomuk2::Server::~Server()
{
    NepomukServerSettings::self()->writeConfig();
    QDBusConnection::sessionBus().unregisterService( "org.kde.NepomukServer" );
}


void Nepomuk2::Server::init()
{
    // no need to start the file indexer explicitly. it is done in enableNepomuk
    enableNepomuk( NepomukServerSettings::self()->startNepomuk() );
}


void Nepomuk2::Server::enableNepomuk( bool enabled )
{
    kDebug() << "enableNepomuk" << enabled;
    if ( enabled != isNepomukEnabled() ) {
        if ( enabled ) {
            // update the server state
            m_currentState = StateEnabling;

            // start all autostart services
            m_serviceManager->startAllServices();

            // register the service manager interface
            QDBusConnection::sessionBus().registerObject( "/servicemanager", m_serviceManager );
        }
        else {
            // update the server state
            m_currentState = StateDisabling;

            // stop all running services
            m_serviceManager->stopAllServices();
            connect(this, SIGNAL(nepomukDisabled()),
                    QCoreApplication::instance(), SLOT(quit()));

            // unregister the service manager interface
            QDBusConnection::sessionBus().unregisterObject( "/servicemanager" );
        }
    }
}


void Nepomuk2::Server::enableFileIndexer( bool enabled )
{
    kDebug() << enabled;
    if ( isNepomukEnabled() ) {
        if ( enabled ) {
            m_serviceManager->startService( m_fileIndexerServiceName );
        }
        else {
            m_serviceManager->stopService( m_fileIndexerServiceName );
        }
    }
}


bool Nepomuk2::Server::isNepomukEnabled() const
{
    return m_currentState == StateEnabled || m_currentState == StateEnabling;
}


bool Nepomuk2::Server::isFileIndexerEnabled() const
{
    return m_serviceManager->runningServices().contains( m_fileIndexerServiceName );
}


QString Nepomuk2::Server::defaultRepository() const
{
    return QLatin1String("main");
}


void Nepomuk2::Server::reconfigure()
{
    NepomukServerSettings::self()->config()->sync();
    NepomukServerSettings::self()->readConfig();
    init();
}


void Nepomuk2::Server::quit()
{
    if( isNepomukEnabled() &&
        !m_serviceManager->runningServices().isEmpty() ) {
        enableNepomuk(false);
    }
    else {
        // We use a QTimer because the event loop might not be running when
        // this is called, in that case 'quit' will do nothing
        QTimer::singleShot( 0, QCoreApplication::instance(), SLOT(quit()) );
    }
}


KSharedConfig::Ptr Nepomuk2::Server::config() const
{
    return m_config;
}


Nepomuk2::Server* Nepomuk2::Server::self()
{
    return s_self;
}

void Nepomuk2::Server::slotServiceStopped(const QString &name)
{
    Q_UNUSED(name);

    kDebug() << name;

    if(m_currentState == StateDisabling &&
       m_serviceManager->runningServices().isEmpty()) {
        m_currentState = StateDisabled;
        emit nepomukDisabled();
    }
    else {
        kDebug() << "Services still running:" << m_serviceManager->runningServices();
    }
}

void Nepomuk2::Server::slotServiceInitialized(const QString &name)
{
    Q_UNUSED(name);

    if(m_currentState == StateEnabling &&
       m_serviceManager->pendingServices().isEmpty()) {
        m_currentState = StateEnabled;
        emit nepomukEnabled();
    }
}

#include "nepomukserver.moc"
