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


#include "backupwizardpages.h"
#include "backupwizard.h"
#include "resourcemanager.h"

#include <KDebug>
#include <KLineEdit>
#include <KStandardDirs>
#include <KFileDialog>
#include <KUrlRequester>

#include <Soprano/Model>
#include <Soprano/QueryResultIterator>

#include <QtGui/QVBoxLayout>
#include <QtGui/QGroupBox>
#include <QtGui/QLineEdit>

Nepomuk2::IntroPage::IntroPage(QWidget* parent)
    : QWizardPage( parent )
{
    setupUi( this );
    setTitle( i18n("Nepomuk Backup") );
    setSubTitle( i18n("Please choose one of the following options") );
}

int Nepomuk2::IntroPage::nextId() const
{
    if( m_backup->isChecked() )
        return BackupWizard::Id_BackupSettingsPage;
    else if( m_restore->isChecked() )
        return BackupWizard::Id_RestoreSelectionPage;

    return -1;
}


//
// Backup Page
//
Nepomuk2::BackupPage::BackupPage(QWidget* parent)
    : QWizardPage(parent),
      m_backupDone(false)
{
    QVBoxLayout* layout = new QVBoxLayout( this );
    m_progressBar = new QProgressBar( this );
    m_progressBar->setMinimum( 0 );
    m_progressBar->setMaximum( 100 );

    m_status = new QLabel( this );

    layout->addWidget( m_progressBar );
    layout->addWidget( m_status );

    setTitle( i18n("Nepomuk Backup") );
    setSubTitle( i18n("Performing backup") );
    setCommitPage(true);

    m_backupManager = new BackupManager( QLatin1String("org.kde.NepomukStorage"),
                                         QLatin1String("/backupmanager"),
                                         QDBusConnection::sessionBus(), this);
    connect( m_backupManager, SIGNAL(backupDone()), this, SLOT(slotBackupDone()) );
    connect( m_backupManager, SIGNAL(backupPercent(int)), m_progressBar, SLOT(setValue(int)) );
}

void Nepomuk2::BackupPage::initializePage()
{
    m_backupDone = false;
    KUrl backupUrl = field(QLatin1String("backupUrl")).value<KUrl>();
    kDebug() << backupUrl;
    m_status->setText( i18nc("@info", "Writing Nepomuk database backup to <filename>%1</filename>...",
                             field(QLatin1String("backupUrl")).value<KUrl>().pathOrUrl()));
    m_backupManager->backup( backupUrl.toLocalFile() );
}

bool Nepomuk2::BackupPage::isComplete() const
{
    return m_backupDone;
}

int Nepomuk2::BackupPage::nextId() const
{
    return -1;
}

void Nepomuk2::BackupPage::slotBackupDone()
{
    m_backupDone = true;
    m_status->setText( i18nc("@info","Backup of the Nepomuk database successfully written to <filename>%1</filename>.",
                             field(QLatin1String("backupUrl")).value<KUrl>().pathOrUrl()) );
    setSubTitle( i18n("Backup completed successfully") );

    emit completeChanged();
}

//
// Restore Selection Page
//

Nepomuk2::RestoreSelectionPage::RestoreSelectionPage(QWidget* parent): QWizardPage(parent)
{
    setupUi( this );
}

void Nepomuk2::RestoreSelectionPage::initializePage()
{
    QDir dir( KStandardDirs::locateLocal( "data", "nepomuk/backupsync/backups/" ) );
    QStringList backupFiles = dir.entryList( QDir::Files | QDir::NoDotAndDotDot, QDir::Name );

    foreach( const QString & backup, backupFiles ) {
        m_listWidget->addItem( backup );
    }

    if( backupFiles.isEmpty() ) {
        QLabel * errorLabel = new QLabel( i18nc("@info", "No system backups found. Please select a custom backup path.") , this);
        QGridLayout* layout = new QGridLayout(m_listWidget);
        layout->addWidget(errorLabel, 1, 1);
        layout->setRowStretch(0,1);
        layout->setRowStretch(2,1);
        layout->setColumnStretch(0,1);
        layout->setColumnStretch(2,1);
    }

    connect( m_customBackupButton, SIGNAL(clicked(bool)), this, SLOT(slotCustomBackupUrl()) );
    connect( m_listWidget, SIGNAL(itemSelectionChanged()),
             this, SLOT(slotSelectionChanged()) );

    registerField( "backupToRestorePath", this, "backupFilePath" );
}

bool Nepomuk2::RestoreSelectionPage::isComplete() const
{
    return QFile::exists(m_backupFilePath);
}

int Nepomuk2::RestoreSelectionPage::nextId() const
{
    return BackupWizard::Id_RestorePage;
}


void Nepomuk2::RestoreSelectionPage::slotSelectionChanged()
{
    if( QListWidgetItem* item = m_listWidget->currentItem() )
        m_backupFilePath = KStandardDirs::locateLocal("data", QLatin1String("nepomuk/backupsync/backups/") + item->data( Qt::DisplayRole ).toString() );
    else
        m_backupFilePath.truncate(0);
    kDebug() << m_backupFilePath;
    emit completeChanged();
}

void Nepomuk2::RestoreSelectionPage::slotCustomBackupUrl()
{
    m_backupFilePath = KFileDialog::getOpenFileName( KUrl(), QString(), this );
    kDebug() << "NEW BACKUP URL : " << m_backupFilePath;
    if( isComplete() ) {
        wizard()->next();
    }
}


//
// Restore Page
//
Nepomuk2::RestorePage::RestorePage(QWidget* parent)
    : QWizardPage(parent)
{
    // GUI
    QVBoxLayout* layout = new QVBoxLayout( this );

    m_progressBar = new QProgressBar( this );
    m_progressBar->setMinimum( 0 );
    m_progressBar->setMaximum( 100 );

    m_status = new QLabel( i18n("Restoring Backup"), this );
    m_restoreDone = false;

    layout->addWidget( m_progressBar );
    layout->addWidget( m_status );

    // Page Properties
    setTitle( i18n("Restoring Backup") );
    setSubTitle( i18n("The backup is being restored...") );

    m_backupManager = new BackupManager( QLatin1String("org.kde.NepomukStorage"),
                                         "/backupmanager",
                                         QDBusConnection::sessionBus(), this);
}


void Nepomuk2::RestorePage::initializePage()
{
    QString backupUrl = field("backupToRestorePath").toString();

    kDebug() << "Restoring " << backupUrl;
    m_backupManager->restore( backupUrl );

    connect( m_backupManager, SIGNAL(restoreDone()), this, SLOT(slotRestoreDone()) );
    connect( m_backupManager, SIGNAL(restorePercent(int)), m_progressBar, SLOT(setValue(int)) );
}

void Nepomuk2::RestorePage::slotRestoreDone()
{
    m_restoreDone = true;

    // vHanda: Maybe we shouldn't auto go to next
    wizard()->next();
}


int Nepomuk2::RestorePage::nextId() const
{
    if( !m_restoreDone )
        return -1;

    if( FileConflictPage::hasConflicts() )
        return BackupWizard::Id_FileConflictPage;
    else
        return BackupWizard::Id_RestoreEndPage;
}



//
// Backup Settings Page
//

Nepomuk2::BackupSettingsPage::BackupSettingsPage(QWidget *parent)
    : QWizardPage(parent)
{
    setupUi(this);
    setTitle( i18n("Nepomuk Backup") );
    setSubTitle( i18n("Please configure the Nepomuk backup") );
    connect(m_editBackupUrl, SIGNAL(textChanged(QString)),
            this, SIGNAL(completeChanged()));
    connect(m_editBackupUrl, SIGNAL(urlSelected(KUrl)),
            this, SIGNAL(completeChanged()));

    registerField(QLatin1String("backupUrl"), this, "backupUrl");
}

KUrl Nepomuk2::BackupSettingsPage::backupUrl() const
{
    return m_editBackupUrl->url();
}

bool Nepomuk2::BackupSettingsPage::isComplete() const
{
    const KUrl url = m_editBackupUrl->url();
    return  QDir( url.directory() ).exists() && url.isValid();
}

int Nepomuk2::BackupSettingsPage::nextId() const
{
    return BackupWizard::Id_BackupPage;
}


//
// Error Page
//

Nepomuk2::ErrorPage::ErrorPage( QWidget* parent )
    : QWizardPage(parent)
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

//
// File Conflict
//

Nepomuk2::FileConflictPage::FileConflictPage(QWidget* parent): QWizardPage(parent)
{
    setTitle( i18n("Nepomuk Backup") );
    setSubTitle( i18n("The following files were not found. Please either discard them or find them manually") );
}

void Nepomuk2::FileConflictPage::initializePage()
{
    m_conflictWidget = new FileConflictWidget( this );
    QVBoxLayout* layout = new QVBoxLayout( this );
    layout->addWidget( m_conflictWidget );

    setLayout( layout );
}

int Nepomuk2::FileConflictPage::nextId() const
{
    return BackupWizard::Id_RestoreEndPage;
}


// static
bool Nepomuk2::FileConflictPage::hasConflicts()
{
    QLatin1String query("select count(?url) where { ?r nie:url ?url . FILTER(REGEX(STR(?url), '^nepomuk-backup')) . }");

    Soprano::Model* model = ResourceManager::instance()->mainModel();
    Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );

    if( it.next() ) {
        return it[0].literal().toInt();
    }

    return false;
}

//
// Final Page
//

Nepomuk2::RestoreEndPage::RestoreEndPage(QWidget* parent): QWizardPage(parent)
{
}

void Nepomuk2::RestoreEndPage::initializePage()
{
    setTitle( i18n("Nepomuk Backup") );
    setSubTitle( i18n("The Backup has been successfully restored") );

    setFinalPage( true );
}

int Nepomuk2::RestoreEndPage::nextId() const
{
    // Without this a 'next' option in shown in the wizard. Weird.
    return -1;
}




#include "backupwizardpages.moc"
