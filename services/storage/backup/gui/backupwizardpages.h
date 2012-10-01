/*
    This file is part of the Nepomuk KDE project.
    Copyright (C) 2010  Vishesh Handa <handa.vish@gmail.com>
    Copyright (C) 2010 Sebastian Trueg <trueg@kde.org>

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


#ifndef BACKUPWIZARDPAGES_H
#define BACKUPWIZARDPAGES_H

#include <QtGui/QWizardPage>
#include <QtGui/QRadioButton>
#include <QtGui/QProgressBar>
#include <QtGui/QListWidget>

#include "backupmanagerinterface.h"
#include "fileconflictwidget.h"

#include "ui_intropage.h"
#include "ui_backupsettingspage.h"
#include "ui_restoreselection.h"
#include "ui_errorpage.h"

namespace Nepomuk2 {

    class BackupWizard;

    typedef org::kde::nepomuk::BackupManager BackupManager;

    class IntroPage : public QWizardPage, public Ui::IntroPage {
        Q_OBJECT

    public:
        IntroPage(QWidget* parent = 0);

        int nextId() const;
    };

    class BackupSettingsPage : public QWizardPage, public Ui::BackupSettingsPage {
        Q_OBJECT
        Q_PROPERTY(KUrl backupUrl READ backupUrl)

    public:
        BackupSettingsPage(QWidget* parent = 0);

        KUrl backupUrl() const;

        bool isComplete() const;
        int nextId() const;
    };

    class BackupPage : public QWizardPage {
        Q_OBJECT

    public:
        BackupPage(QWidget* parent = 0);

        void initializePage();
        bool isComplete() const;
        int nextId() const;

    private slots:
        void slotBackupDone();

    private:
        BackupManager* m_backupManager;
        bool m_backupDone;

        QLabel* m_status;
        QProgressBar* m_progressBar;
    };

    class RestoreSelectionPage : public QWizardPage, public Ui::RestoreSelection {
        Q_OBJECT
        Q_PROPERTY(QString backupFilePath READ backupFilePath)

    public:
        RestoreSelectionPage(QWidget* parent = 0);

        QString backupFilePath() const { return m_backupFilePath; }

        void initializePage();
        bool isComplete() const;
        int nextId() const;

    private slots:
        void slotSelectionChanged();
        void slotCustomBackupUrl();

    private:
        QString m_backupFilePath;
    };


    /**
     * This is the Page that is shown when a restore is being performed.
     *
     * It mainly shows a progress bar and a label
     */
    class RestorePage : public QWizardPage {
        Q_OBJECT

    public:
        RestorePage( QWidget * parent );

        void initializePage();
        int nextId() const;

    private slots:
        void slotRestoreDone();

    private:
        BackupManager* m_backupManager;

        QLabel* m_status;
        QProgressBar* m_progressBar;
        bool m_restoreDone;
    };

    class ErrorPage : public QWizardPage, public Ui::ErrorPage {
        Q_OBJECT
        Q_PROPERTY(QString errorMessage READ message WRITE setMessage)

    public:
        ErrorPage( QWidget* parent = 0 );

        QString message() const;

    public Q_SLOTS:
        void setMessage(const QString& s);
    };

    /**
     * File Conflict Page
     *
     * Used to resolve all the files that were not found during backup restoration
     */
    class FileConflictPage : public QWizardPage {
        Q_OBJECT

    public:
        FileConflictPage(QWidget* parent = 0);

        void initializePage();
        int nextId() const;

        /**
         * Returns true if there are conflicts that need to be solved
         */
        static bool hasConflicts();
    private:
        FileConflictWidget* m_conflictWidget;
    };

    /**
     * Final ending page
     */
    class RestoreEndPage : public QWizardPage {
    public:
        RestoreEndPage(QWidget* parent = 0);

        void initializePage();
        int nextId() const;
    };


}

#endif // BACKUPWIZARDPAGES_H
