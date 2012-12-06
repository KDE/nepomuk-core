/* This file is part of the KDE Project
   Copyright (c) 2008 Sebastian Trueg <trueg@kde.org>
   Copyright (c) 2010-12 Vishesh Handa <me@vhanda.in>

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

#include "eventmonitor.h"
#include "fileindexerconfig.h"
#include "indexscheduler.h"

#include <KDebug>
#include <KPassivePopup>
#include <KLocale>
#include <KDiskFreeSpaceInfo>
#include <KStandardDirs>
#include <KNotification>
#include <KIcon>
#include <KConfigGroup>
#include <KIdleTime>

#include <Solid/PowerManagement>

#include <QtDBus/QDBusInterface>

// TODO: Make idle timeout configurable?
static int s_idleTimeout = 1000;// * 60 * 2; // 2 min
static int s_availSpaceTimeout = 1000 * 30; // 30 seconds

namespace {
    void sendEvent( const QString& event, const QString& text, const QString& iconName ) {
        KNotification::event( event, text, KIcon( iconName ).pixmap( 32, 32 ) );
    }
}


Nepomuk2::EventMonitor::EventMonitor( QObject* parent )
    : QObject( parent )
{
    // monitor the powermanagement to not drain the battery
    connect( Solid::PowerManagement::notifier(), SIGNAL( appShouldConserveResourcesChanged( bool ) ),
             this, SLOT(slotPowerManagementStatusChanged(bool)) );

    // setup the avail disk usage monitor
    connect( &m_availSpaceTimer, SIGNAL( timeout() ),
             this, SLOT( slotCheckAvailableSpace() ) );

    // setup idle time
    KIdleTime* idleTime = KIdleTime::instance();
    idleTime->addIdleTimeout( s_idleTimeout );

    connect( idleTime, SIGNAL(timeoutReached(int)), this, SLOT(slotIdleTimeoutReached()) );
    connect( idleTime, SIGNAL(resumingFromIdle()), this, SLOT(slotResumeFromIdle()) );

    /*
    FIXME: Inital run? Do we really need to notify the users?
    if ( FileIndexerConfig::self()->isInitialRun() ) {
        // TODO: add actions to this notification

        m_indexingStartTime = QDateTime::currentDateTime();

        // inform the user about the initial indexing
        sendEvent( "initialIndexingStarted",
                   i18n( "Indexing files for fast searching. This process may take a while." ),
                   "nepomuk" );

        // connect to get the end of initial indexing
        // (make it a queued connection since KNotification does not like multithreading
        // (mostly because it uses dialogs)
        connect( m_indexScheduler, SIGNAL( indexingStopped() ),
                 this, SLOT( slotIndexingStopped() ),
                 Qt::QueuedConnection );

        connect( m_indexScheduler, SIGNAL( indexingSuspended(bool) ),
                 this, SLOT( slotIndexingSuspended(bool) ) );
    }*/

    m_isOnBattery = Solid::PowerManagement::appShouldConserveResources();
    m_isIdle = false;
    m_isDiskSpaceLow = false; /* We hope */
    m_enabled = false;
}


Nepomuk2::EventMonitor::~EventMonitor()
{
}


void Nepomuk2::EventMonitor::slotPowerManagementStatusChanged( bool conserveResources )
{
    m_isOnBattery = conserveResources;
    if( m_enabled ) {
        emit powerManagementStatusChanged( conserveResources );
    }

    // This all should be done in the index scheduler or ideally some other class
    /*
    KConfig config("nepomukstrigirc");
    KConfigGroup group = config.group("General");
    bool showEvents = group.readEntry<bool>("ShowSuspendResumeEvents", false);

    if ( !conserveResources && m_pauseState == PausedDueToPowerManagement ) {
        kDebug() << "Resuming indexer due to power management";
        resumeIndexing();
        if( showEvents && m_wasIndexingWhenPaused )
            sendEvent( "indexingResumed", i18n("Resuming indexing of files for fast searching."), "battery-charging" );
    }
    else if ( conserveResources &&
              !FileIndexerConfig::self()->suspendOnPowerSaveDisabled() &&
              !m_indexScheduler->isSuspended() ) {
        kDebug() << "Pausing indexer due to power management";
        m_wasIndexingWhenPaused = m_indexScheduler->isIndexing();
        if( showEvents && m_wasIndexingWhenPaused )
            sendEvent( "indexingSuspended", i18n("Suspending the indexing of files to preserve resources."), "battery-100" );
        pauseIndexing( PausedDueToPowerManagement );
    } */
}


void Nepomuk2::EventMonitor::slotCheckAvailableSpace()
{
    if( !m_enabled )
        return;


    QString path = KStandardDirs::locateLocal( "data", "nepomuk/repository/", false );
    KDiskFreeSpaceInfo info = KDiskFreeSpaceInfo::freeSpaceInfo( path );
    if ( info.isValid() ) {
        if ( info.available() <= FileIndexerConfig::self()->minDiskSpace() ) {
            m_isDiskSpaceLow = true;
            emit diskSpaceStatusChanged( true );
        }
        else if( m_isDiskSpaceLow ) {
            // We only emit this signal, if previously emitted with the value true
            m_isDiskSpaceLow = false;
            emit diskSpaceStatusChanged( false );
        }
        /*
            // Do we emit false?
            if ( !m_indexScheduler->isSuspended() ) {
                pauseIndexing( PausedDueToAvailSpace );
                sendEvent( "indexingSuspended",
                           i18n("Disk space is running low (%1 left). Suspending indexing of files.",
                                KIO::convertSize( info.available() ) ),
                           "drive-harddisk" );
            }
        }
        else if ( m_pauseState == PausedDueToAvailSpace ) {
            kDebug() << "Resuming indexer due to disk space";
            resumeIndexing();
            sendEvent( "indexingResumed", i18n("Resuming indexing of files for fast searching."), "drive-harddisk" );
        }*/
    }
    else {
        // if it does not work once, it will probably never work
        m_availSpaceTimer.stop();
    }
}


/*
void Nepomuk2::EventMonitor::slotIndexingStopped()
{
    // inform the user about the end of initial indexing. This will only be called once
    if ( !m_indexScheduler->isSuspended() ) {
        m_totalIndexingSeconds += m_indexingStartTime.secsTo( QDateTime::currentDateTime() );
        const int elapsed = m_totalIndexingSeconds * 1000;

        kDebug() << "initial indexing took" << elapsed;
        sendEvent( "initialIndexingFinished",
                   i18nc( "@info %1 is a duration formatted using KLocale::prettyFormatDuration",
                          "Initial indexing of files for fast searching finished in %1",
                          KGlobal::locale()->prettyFormatDuration( elapsed ) ),
                   "nepomuk" );
        m_indexScheduler->disconnect( this );
    }
}


void Nepomuk2::EventMonitor::pauseIndexing(int pauseState)
{
    m_pauseState = pauseState;
    m_indexScheduler->suspend();

    m_totalIndexingSeconds += m_indexingStartTime.secsTo( QDateTime::currentDateTime() );
}


void Nepomuk2::EventMonitor::resumeIndexing()
{
    m_pauseState = NotPaused;
    m_indexScheduler->resume();

    m_indexingStartTime = QDateTime::currentDateTime();
}


void Nepomuk2::EventMonitor::slotIndexingSuspended( bool suspended )
{
    if( suspended ) {
        //The indexing is already paused, this meerly sets the correct state, and adjusts the timing.
        pauseIndexing( PausedCustom );
    }
    else {
        //Again, used to set the correct state, and adjust the timing.
        resumeIndexing();
    }
}*/

void Nepomuk2::EventMonitor::enable()
{
    m_enabled = true;

    KIdleTime::instance()->addIdleTimeout( s_idleTimeout );
    m_availSpaceTimer.start( s_availSpaceTimeout );
}

void Nepomuk2::EventMonitor::disable()
{
    m_enabled = false;

    KIdleTime::instance()->removeAllIdleTimeouts();
    m_availSpaceTimer.stop();
}


/*
void Nepomuk2::EventMonitor::slotIndexingStateChanged(bool indexing)
{
    // there is no need to check the available space (and wasting IO) when we are not indexing
    // the only exception is when we suspended due to disk space shortage since we can resume once
    // disk space has been freed up
    if( indexing ) {
        kDebug() << "Starting available disk space timer.";
        m_availSpaceTimer.start( 20*1000 ); // every 20 seconds should be enough
    }
    else if( m_pauseState != PausedDueToAvailSpace ) {
        kDebug() << "Stopping available disk space timer.";
        m_availSpaceTimer.stop();
    }
}*/

void Nepomuk2::EventMonitor::slotIdleTimeoutReached()
{
    if( m_enabled ) {
        m_isIdle = true;
        emit idleStatusChanged( true );
    }
    KIdleTime::instance()->catchNextResumeEvent();
}

void Nepomuk2::EventMonitor::slotResumeFromIdle()
{
    m_isIdle = false;
    if( m_enabled ) {
        emit idleStatusChanged( false );
    }
}


#include "eventmonitor.moc"
