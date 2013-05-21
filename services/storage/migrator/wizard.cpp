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

    // No Back button
    QList<QWizard::WizardButton> layout;
    layout << QWizard::Stretch << QWizard::NextButton << QWizard::CancelButton << QWizard::FinishButton;
    setButtonLayout( layout );

    setPage( Id_MainPage, new MainPage(this) );
    setPage( Id_MigrationPage, new MigrationPage(this) );
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

    QLabel* label = new QLabel( this );
    label->setTextFormat( Qt::RichText );
    label->setWordWrap( true );
    label->setText( i18n("With this new release Nepomuk has migrated its internal data format. "
                         "You have launched this wizard because you wish to migrate your data and "
                         "not go with the recommended method of backup and restore. "
                         "This migration can take several hours") );

    QVBoxLayout* layout = new QVBoxLayout( this );
    layout->addWidget( label );
}

int MainPage::nextId() const
{
    return MigrationWizard::Id_MigrationPage;
}


//
// Migration Page
//

MigrationPage::MigrationPage(QWidget* parent)
    : QWizardPage(parent)
    , m_done(false)
{
}

void MigrationPage::initializePage()
{
    setTitle( i18n("The internal data is being migrated") );
    setSubTitle( i18n("This process could take a while") );

    QVBoxLayout* layout = new QVBoxLayout( this );
    m_progressBar = new QProgressBar( this );
    m_progressBar->setMaximum( 100 );

    layout->addWidget( m_progressBar );

    m_storageService = new StorageService( QLatin1String("org.kde.NepomukStorage"),
                                           QLatin1String("/nepomukstorage"),
                                           QDBusConnection::sessionBus(), this);
    connect( m_storageService, SIGNAL(migrateGraphsDone()), this, SLOT(slotMigrationDone()) );
    connect( m_storageService, SIGNAL(migrateGraphsPercent(int)), this, SLOT(slotMigrationPercent(int)) );

    m_storageService->migrateGraphs();
}


void MigrationPage::slotMigrationDone()
{
    m_done = true;
    emit completeChanged();

    wizard()->next();
}

void MigrationPage::slotMigrationPercent(int percent)
{
    kDebug() << percent;
    m_progressBar->setValue( percent );
}


bool MigrationPage::isComplete() const
{
    return m_done;
}

int MigrationPage::nextId() const
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
