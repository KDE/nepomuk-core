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

#ifndef _PROCESSCONTROL_H
#define _PROCESSCONTROL_H

#include <QtCore/QObject>
#include <QtCore/QProcess>


/**
 * This class starts and observes a process. Depending on the
 * policy it also restarts the process when it crashes.
 */
class ProcessControl : public QObject
{
    Q_OBJECT

public:
    /**
     * Theses enums describe the behaviour when the observed
     * application crashed.
     *
     * @li StopOnCrash    - The application won't be restarted.
     * @li RestartOnCrash - The application is restarted with the same arguments.
     */
    enum CrashPolicy
    {
        StopOnCrash,
        RestartOnCrash
    };

    /**
     * Creates a new process control.
     *
     * @param parent The parent object.
     */
    ProcessControl( QObject *parent = 0 );

    /**
     * Destroys the process control.
     */
    ~ProcessControl();

    /**
     * Starts the @p application with the given list of @p arguments.
     */
    void start( const QString &application, const QStringList &arguments = QStringList(),
                CrashPolicy policy = RestartOnCrash, int maxCrashes = 5 );

    /**
     * Sends the term signal to the process.
     * \param waitAndKill If \p true does wait for the process to finish
     * and if it does not kills the process after a timeout. If \p false
     * a timer is used instead.
     */
    void terminate( bool waitAndKill = false );

    /**
     * Sets the crash policy.
     */
    void setCrashPolicy( CrashPolicy policy );

    bool isRunning() const;

    bool waitForStarted();

Q_SIGNALS:
    /**
     * This signal is emitted whenever the observed application
     * writes something to stderr.
     *
     * @param errorMsg The error output of the observed application.
     */
    void processErrorMessages( const QString &errorMsg );

    void finished( bool clean );

private Q_SLOTS:
    void slotError( QProcess::ProcessError );
    void slotFinished( int, QProcess::ExitStatus );

private:
    void start();

    QString commandLine() const;

    QProcess mProcess;
    QString mApplication;
    QStringList mArguments;
    CrashPolicy mPolicy;
    bool mFailedToStart;
    int mCrashCount;
};

#endif
