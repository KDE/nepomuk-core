/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2010  Vishesh Handa <handa.vish@gmail.com>

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


#include "backupmanager.h"
#include "backupmanageradaptor.h"

#include "ontologyloader.h"
#include "backupgenerationjob.h"
#include "backuprestorationjob.h"

#include <QtDBus/QDBusConnection>
#include <QtCore/QListIterator>
#include <QtCore/QTimer>
#include <QtCore/QDir>
#include <QtCore/QThread>

#include <KDebug>
#include <KStandardDirs>
#include <KConfig>
#include <KConfigGroup>
#include <KDirWatch>
#include <KGlobal>
#include <KLocale>
#include <KCalendarSystem>


Nepomuk2::BackupManager::BackupManager(Nepomuk2::OntologyLoader* loader, Soprano::Model* model, QObject* parent)
    : QObject( parent ),
      m_config( "nepomukbackuprc" ),
      m_model( model ),
      m_ontologyLoader( loader )
{
    new BackupManagerAdaptor( this );
    // Register via DBUs
    QDBusConnection con = QDBusConnection::sessionBus();
    con.registerObject( QLatin1String("/backupmanager"), this );

    m_backupLocation = KStandardDirs::locateLocal( "data", "nepomuk/backupsync/backups/" );
    m_daysBetweenBackups = 0;

    KDirWatch* dirWatch = KDirWatch::self();
    connect( dirWatch, SIGNAL( dirty( const QString& ) ),
             this, SLOT( slotConfigDirty() ) );
    connect( dirWatch, SIGNAL( created( const QString& ) ),
             this, SLOT( slotConfigDirty() ) );

    dirWatch->addFile( KStandardDirs::locateLocal( "config", m_config.name() ) );

    connect( &m_timer, SIGNAL(timeout()), this, SLOT(automatedBackup()) );
    slotConfigDirty();
}


Nepomuk2::BackupManager::~BackupManager()
{
}


void Nepomuk2::BackupManager::backup(const QString& oldUrl)
{
    QString url = oldUrl;
    if( url.isEmpty() )
        url = KStandardDirs::locateLocal( "data", "nepomuk/backupsync/backup" ); // default location

    kDebug() << url;

    QFile::remove( url );

    KJob* job = new BackupGenerationJob( m_model, url );

    QThread* backupThread = new QThread( this );
    job->moveToThread( backupThread );
    backupThread->start();

    connect( job, SIGNAL(finished(KJob*)), backupThread, SLOT(quit()), Qt::QueuedConnection );
    connect( backupThread, SIGNAL(finished()), backupThread, SLOT(deleteLater()) );
    connect( job, SIGNAL(finished(KJob*)), this, SLOT(slotBackupDone(KJob*)), Qt::QueuedConnection );
    connect( job, SIGNAL(percent(KJob*,ulong)), this, SLOT(slotBackupPercent(KJob*,ulong)), Qt::QueuedConnection );
    job->start();

    emit backupStarted();
}



void Nepomuk2::BackupManager::automatedBackup()
{
    QDate today = QDate::currentDate();
    backup( m_backupLocation + today.toString(Qt::ISODate) );

    resetTimer();
    removeOldBackups();
}

void Nepomuk2::BackupManager::slotConfigDirty()
{
    //kDebug();
    m_config.reparseConfiguration();

    QString freq = m_config.group("Backup").readEntry( "backup frequency", QString("disabled") );
    //kDebug() << "Frequency : " << freq;

    if( freq == QLatin1String("disabled") ) {
        //kDebug() << "Auto Backups Disabled";
        m_timer.stop();
        return;
    }

    QString timeString = m_config.group("Backup").readEntry( "backup time", QTime().toString( Qt::ISODate ) );
    m_backupTime = QTime::fromString( timeString, Qt::ISODate );

    if( freq == QLatin1String("daily") ) {
        m_daysBetweenBackups = 0;
    }

    else if( freq == QLatin1String("weekly") ) {

        const KCalendarSystem* cal = KGlobal::locale()->calendar();

        int backupDay = m_config.group("Backup").readEntry( "backup day", 0 );
        int dayOfWeek = cal->dayOfWeek( QDate::currentDate() );

        //kDebug() << "DayOfWeek: " << dayOfWeek;
        //kDebug() << "BackupDay: " << backupDay;
        if( dayOfWeek < backupDay ) {
            m_daysBetweenBackups = backupDay - dayOfWeek;
        }
        else if( dayOfWeek > backupDay ) {
            m_daysBetweenBackups = cal->daysInWeek( QDate::currentDate() ) - dayOfWeek + backupDay;
        }
        else {
            if( QTime::currentTime() <= m_backupTime )
                m_daysBetweenBackups = 0;
            else
                m_daysBetweenBackups = cal->daysInWeek( QDate::currentDate() );
        }

        kDebug() << "Days between backups : " << m_daysBetweenBackups;
    }

    else if( freq == QLatin1String("monthly") ) {
        //TODO: Implement me!
    }

    m_maxBackups = m_config.group("Backup").readEntry<int>("max backups", 1);

    // Remove old timers and start new
    resetTimer();
    removeOldBackups();
}

void Nepomuk2::BackupManager::resetTimer()
{
    if( m_backupTime.isNull() && m_daysBetweenBackups == 0 ) {
        // Never perform automated backups
        return;
    }

    QDateTime current = QDateTime::currentDateTime();
    QDateTime dateTime = current.addDays( m_daysBetweenBackups );
    dateTime.setTime( m_backupTime );

    if( dateTime < current ) {
        dateTime = dateTime.addDays( 1 );
    }

    int msecs = current.msecsTo( dateTime );

    m_timer.stop();
    m_timer.start( msecs );
    kDebug() << "Setting timer for " << msecs/1000.0/60/60 << " hours";
}

void Nepomuk2::BackupManager::removeOldBackups()
{
    QDir dir( m_backupLocation );
    QStringList infoList = dir.entryList( QDir::Files | QDir::NoDotAndDotDot, QDir::Name );

    while( infoList.size() > m_maxBackups ) {
        const QString backupPath = m_backupLocation + infoList.last();
        kDebug() << "Removing : " << backupPath;
        QFile::remove( backupPath );
        infoList.pop_back();
    }
}

void Nepomuk2::BackupManager::slotBackupDone(KJob* job)
{
    if( !job->error() ) {
        emit backupDone();
    }
}

void Nepomuk2::BackupManager::slotBackupPercent(KJob* job, ulong percent)
{
    emit backupPercent( percent );
}

void Nepomuk2::BackupManager::restore(const QString& url)
{
    // FIXME: Output an error?
    if( url.isEmpty() )
        return;

    KJob* job = new BackupRestorationJob( m_model, m_ontologyLoader, QUrl::fromLocalFile(url) );
    job->start();

    connect( job, SIGNAL(finished(KJob*)), this, SLOT(slotRestorationDone(KJob*)) );
    connect( job, SIGNAL(percent(KJob*,ulong)), this, SLOT(slotRestorationPercent(KJob*,ulong)) );
}

void Nepomuk2::BackupManager::slotRestorationPercent(KJob*, ulong percent)
{
    emit restorePercent( percent );
}

void Nepomuk2::BackupManager::slotRestorationDone(KJob* job)
{
    if( !job->error() ) {
        emit restoreDone();
    }
}


#include "backupmanager.moc"
