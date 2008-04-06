/***************************************************************************
 *   Copyright (C) 2006 by Tobias Koenig <tokoe@kde.org>                   *
 *   Copyright (C) 2008 by Sebastian Trueg <trueg@kde.org>                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/

#include "processcontrol.h"

#include <QtCore/QDebug>
#include <QtCore/QTimer>


ProcessControl::ProcessControl( QObject *parent )
    : QObject( parent ), mFailedToStart( false ), mCrashCount( 0 )
{
    connect( &mProcess, SIGNAL( error( QProcess::ProcessError ) ),
             this, SLOT( slotError( QProcess::ProcessError ) ) );
    connect( &mProcess, SIGNAL( finished( int, QProcess::ExitStatus ) ),
             this, SLOT( slotFinished( int, QProcess::ExitStatus ) ) );
    connect( &mProcess, SIGNAL( readyReadStandardError() ),
             this, SLOT( slotErrorMessages() ) );
    connect( &mProcess, SIGNAL( readyReadStandardOutput() ),
             this, SLOT( slotStdoutMessages() ) );
}

ProcessControl::~ProcessControl()
{
    stop();
}

bool ProcessControl::start( const QString &application, const QStringList &arguments, CrashPolicy policy )
{
    mFailedToStart = false;

    mApplication = application;
    mArguments = arguments;
    mPolicy = policy;

    return start();
}

void ProcessControl::setCrashPolicy( CrashPolicy policy )
{
    mPolicy = policy;
}

void ProcessControl::stop()
{
    if ( mProcess.state() != QProcess::NotRunning ) {
        if ( !mProcess.waitForFinished( 30000 ) ) {
            mProcess.terminate();
        }
    }
}

void ProcessControl::slotError( QProcess::ProcessError error )
{
    switch ( error ) {
    case QProcess::Crashed:
        // do nothing, we'll respawn in slotFinished
        break;
    case QProcess::FailedToStart:
    default:
        mFailedToStart = true;
        break;
    }

    qDebug( "ProcessControl: Application '%s' stopped unexpected (%s)",
            qPrintable( mApplication ), qPrintable( mProcess.errorString() ) );
}

void ProcessControl::slotFinished( int exitCode, QProcess::ExitStatus exitStatus )
{
    if ( exitStatus == QProcess::CrashExit ) {
        if ( mPolicy == RestartOnCrash ) {
            if ( !mFailedToStart ) // don't try to start an unstartable application
                start();
            else
                emit finished(false);
        }
    } else {
        if ( exitCode != 0 ) {
            qDebug( "ProcessControl: Application '%s' returned with exit code %d (%s)",
                    qPrintable( mApplication ), exitCode, qPrintable( mProcess.errorString() ) );
            if ( mPolicy == RestartOnCrash ) {
                if ( mCrashCount > 3 ) {
                    qWarning() << mApplication << "crashed too often and will not be restarted!";
                    mPolicy = StopOnCrash;
                    return;
                }
                ++mCrashCount;
                QTimer::singleShot( 5000, this, SLOT(resetCrashCount()) );
                if ( !mFailedToStart ) // don't try to start an unstartable application
                    start();
                else
                    emit finished(false);
            }
        } else {
            qDebug( "Application '%s' exited normally...", qPrintable( mApplication ) );
            emit finished(true);
        }
    }
}

void ProcessControl::slotErrorMessages()
{
    QString message = QString::fromUtf8( mProcess.readAllStandardError() );
    emit processErrorMessages( message );
    qDebug( "[%s] %s", qPrintable( mApplication ), qPrintable( message.trimmed() ) );
}

bool ProcessControl::start()
{
    mProcess.start( mApplication, mArguments );
    if ( !mProcess.waitForStarted( ) ) {
        qDebug( "ProcessControl: Unable to start application '%s' (%s)",
                qPrintable( mApplication ), qPrintable( mProcess.errorString() ) );
        return false;
    }
    return true;
}

void ProcessControl::slotStdoutMessages()
{
    QString message = QString::fromUtf8( mProcess.readAllStandardOutput() );
    qDebug() << mApplication << "[out]" << message;
}

void ProcessControl::resetCrashCount()
{
    mCrashCount = 0;
}

bool ProcessControl::isRunning() const
{
    return mProcess.state() == QProcess::Running;
}

#include "processcontrol.moc"
