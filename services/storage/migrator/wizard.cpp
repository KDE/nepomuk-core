/*
 * <one line to give the library's name and an idea of what it does.>
 * Copyright 2013  Vishesh Handa <me@vhanda.in>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "wizard.h"

#include <KGlobal>
#include <KIcon>
#include <KLocalizedString>
#include <KComponentData>
#include <KAboutData>
#include <KTemporaryFile>
#include <KDebug>

#include <QBoxLayout>
#include <QRadioButton>
#include <QGroupBox>

using namespace Nepomuk2;

MigrationWizard::MigrationWizard(QWidget* parent, Qt::WindowFlags flags)
    : QWizard(parent, flags)
{
    setPixmap(LogoPixmap, KIcon(QLatin1String("nepomuk")).pixmap(32, 32));
    setWindowIcon( KIcon(QLatin1String("nepomuk")) );
    setWindowTitle( KGlobal::activeComponent().aboutData()->shortDescription() );

    setOption( HaveFinishButtonOnEarlyPages, false );
    setOption( HaveNextButtonOnLastPage, false );
    setOption( DisabledBackButtonOnLastPage, true );

    setPage( Id_MainPage, new MainPage(this) );
    setPage( Id_BackupRestorePage, new BackupRestorePage(this) );
    setPage( Id_MigrationPage, new MigrationPage(this) );
    setPage( Id_DeletionPage, new DeletionPage(this) );
    setPage( Id_FinishPage, new FinishPage(this) );
    setPage( Id_ErrorPage, new ErrorPage(this) );


    setStartId( Id_MainPage );
}

void MigrationWizard::showError(const QString& error)
{
    setField(QLatin1String("errorMessage"), error);
    setStartId(Id_ErrorPage);
}

//
// Main Page
//

MainPage::MainPage(QWidget* parent): QWizardPage(parent)
{
    setTitle( i18n("Nepomuk Data Migration") );
    setSubTitle( i18n("With this new release Nepomuk has migrated its internal data format. "
                      "Therefore the old data needs to be migrated. There are many different ways "
                      "in which this can be performed -") );

    QVBoxLayout* vLayout = new QVBoxLayout( this );

    QString backupText = i18n("Backup existing tags and rating, and then restore the data");
    m_backupOption = new QRadioButton( backupText, this );
    m_backupOption->setChecked( true );

    QString migrationText = i18n("Migrate all the existing data.");
    m_migrateOption = new QRadioButton( migrationText, this );

    QString deleteText = i18n("Delete existing metadata, and start with a fresh install");
    m_deleteOption = new QRadioButton( deleteText, this );

    vLayout->addWidget( m_backupOption );
    vLayout->addWidget( m_migrateOption );
    vLayout->addWidget( m_deleteOption );
}

int MainPage::nextId() const
{
    if( m_backupOption->isChecked() )
        return MigrationWizard::Id_BackupRestorePage;
    if( m_migrateOption->isChecked() )
        return MigrationWizard::Id_MigrationPage;
    if( m_deleteOption->isChecked() )
        return MigrationWizard::Id_DeletionPage;

    return -1;
}


//
// Backup Restore Page
//

BackupRestorePage::BackupRestorePage(QWidget* parent)
    : QWizardPage(parent)
    , m_restoreDone( false )
    , m_error( false )
{
    setTitle( i18n("Backup Tags and Ratings") );
    setSubTitle( i18n("This will backup only the tags, and ratings and then restore that data") );

    KTemporaryFile tempFile;
    tempFile.setAutoRemove( false );
    tempFile.open();
    m_url = tempFile.fileName();
    tempFile.close();

    m_backupManager = new BackupManager( QLatin1String("org.kde.NepomukStorage"),
                                         QLatin1String("/backupmanager"),
                                         QDBusConnection::sessionBus(), this);
    connect( m_backupManager, SIGNAL(backupDone()), this, SLOT(slotBackupDone()) );
    connect( m_backupManager, SIGNAL(backupPercent(int)), this, SLOT(slotBackupProgress(int)) );
    connect( m_backupManager, SIGNAL(backupError(QString)), this, SLOT(slotBackupError(QString)));
    connect( m_backupManager, SIGNAL(restoreDone()), this, SLOT(slotRestoneDone()) );
    connect( m_backupManager, SIGNAL(restorePercent(int)), this, SLOT(slotRestoreProgress(int)) );
    connect( m_backupManager, SIGNAL(backupError(QString)), this, SLOT(slotRestoreError(QString)));

    m_backupGroup = new QGroupBox( i18n("Backup"), this );
    m_restoreGroup = new QGroupBox( i18n("Restore"), this );
    m_restoreGroup->setEnabled( false );

    QVBoxLayout* vLayout = new QVBoxLayout( this );
    vLayout->addWidget( m_backupGroup );
    vLayout->addWidget( m_restoreGroup );

    m_backupProgress = new QProgressBar( this );
    m_restoreProgress = new QProgressBar( this );

    QLabel* backupLabel = new QLabel( i18n("Performing backup of tags and ratings") );
    QVBoxLayout* backupLayout = new QVBoxLayout();
    backupLayout->addWidget( backupLabel );
    backupLayout->addWidget( m_backupProgress );
    m_backupGroup->setLayout( backupLayout );

    QLabel* restoreLabel = new QLabel( i18n("Restoring data") );
    QVBoxLayout* restoreLayout = new QVBoxLayout();
    restoreLayout->addWidget( restoreLabel );
    restoreLayout->addWidget( m_restoreProgress );
    m_restoreGroup->setLayout( restoreLayout );
}

void BackupRestorePage::initializePage()
{
    m_backupManager->backupTagsAndRatings( m_url );
}

void BackupRestorePage::slotBackupDone()
{
    m_backupGroup->setEnabled( false );
    m_backupGroup->setEnabled( true );

    m_backupManager->restore( m_url );
}

void BackupRestorePage::slotBackupProgress(int percent)
{
    m_backupProgress->setValue( percent );
}

void BackupRestorePage::slotBackupError(const QString& error)
{
    m_errorMessage = error;
    m_error = true;
    wizard()->next();
}

void BackupRestorePage::slotRestoneDone()
{
    m_restoreDone = true;
}

void BackupRestorePage::slotRestoreProgress(int percent)
{
    m_restoreProgress->setValue( percent );
}

void BackupRestorePage::slotRestoreError(const QString& error)
{
    m_errorMessage = error;
    m_error = true;
    wizard()->next();
}


bool BackupRestorePage::isComplete() const
{
    return m_restoreDone;
}

int BackupRestorePage::nextId() const
{
    if( m_error ) {
        wizard()->setField(QLatin1String("errorMessage"), m_errorMessage);
        return MigrationWizard::Id_ErrorPage;
    }
    else
        return MigrationWizard::Id_FinishPage;
}

//
// Migration Page
//

MigrationPage::MigrationPage(QWidget* parent): QWizardPage(parent)
{

}

int MigrationPage::nextId() const
{
    return QWizardPage::nextId();
}

//
// Deletion Page
//

DeletionPage::DeletionPage(QWidget* parent)
    : QWizardPage(parent)
    , m_done( false )
{
}

void DeletionPage::initializePage()
{
    setTitle( i18n("Removing all existing Metadata") );
    setSubTitle( i18n("In the process of creating a fresh database") );

    QVBoxLayout* layout = new QVBoxLayout( this );
    QProgressBar* progress = new QProgressBar( this );
    progress->setMinimum( 0 );
    progress->setMaximum( 0 );

    layout->addWidget( progress );

    m_storageService = new StorageService( QLatin1String("org.kde.NepomukStorage"),
                                           QLatin1String("/nepomukstorage"),
                                           QDBusConnection::sessionBus(), this);
    connect( m_storageService, SIGNAL(initialized()), this, SLOT(slotDeletionDone()) );

    m_storageService->resetRepository();
}

void DeletionPage::slotDeletionDone()
{
    m_done = true;
    emit completeChanged();

    wizard()->next();
}

bool DeletionPage::isComplete() const
{
    return m_done;
}

int DeletionPage::nextId() const
{
    return MigrationWizard::Id_FinishPage;
}

//
// Finish Page
//

FinishPage::FinishPage(QWidget* parent): QWizardPage(parent)
{
    setTitle( i18n("Data has been successfully migrated") );
}

int FinishPage::nextId() const
{
    return -1;
}

//
// Error Page
//

ErrorPage::ErrorPage(QWidget* parent): QWizardPage(parent)
{
    setupUi(this);
    setFinalPage(true);
    m_labelPixmap->setPixmap(KIcon(QLatin1String("dialog-error")).pixmap(48,48));
    registerField( QLatin1String("errorMessage"), this, "errorMessage" );
}

QString Nepomuk2::ErrorPage::message() const
{
    return m_labelMessage->text();
}

void Nepomuk2::ErrorPage::setMessage(const QString& s)
{
    m_labelMessage->setText(s);
}


#include "wizard.moc"
