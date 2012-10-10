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

#include "backupwizard.h"
#include "backupwizardpages.h"

#include <kglobal.h>
#include <kcomponentdata.h>
#include <kaboutdata.h>

// TODO: 1. "Cancel" just closes the dialog without any feedback on the backup
//       2. If the service crashes the dialog does not notice

Nepomuk2::BackupWizard::BackupWizard(QWidget* parent, Qt::WindowFlags flags)
    : QWizard(parent, flags)
{
    setPixmap(LogoPixmap, KIcon(QLatin1String("nepomuk")).pixmap(32, 32));
    setWindowIcon( KIcon(QLatin1String("nepomuk")) );
    setWindowTitle( KGlobal::activeComponent().aboutData()->shortDescription() );

    setPage( Id_IntroPage, new IntroPage(this) );
    setPage( Id_BackupPage, new BackupPage(this) );
    setPage( Id_BackupSettingsPage, new BackupSettingsPage(this) );
    setPage( Id_RestoreSelectionPage, new RestoreSelectionPage(this) );
    setPage( Id_RestorePage, new RestorePage(this) );
    setPage( Id_FileConflictPage, new FileConflictPage(this) );
    setPage( Id_RestoreEndPage, new RestoreEndPage(this) );
    setPage( Id_ErrorPage, new ErrorPage( this ) );
    setStartId( Id_IntroPage );

    setOption( HaveFinishButtonOnEarlyPages, false );
    setOption( HaveNextButtonOnLastPage, false );
    setOption( DisabledBackButtonOnLastPage, true );
}

void Nepomuk2::BackupWizard::startBackup()
{
    setStartId(Id_BackupSettingsPage);
}

void Nepomuk2::BackupWizard::startRestore()
{
    setStartId(Id_RestoreSelectionPage);
}

void Nepomuk2::BackupWizard::startConflictResolution()
{
    setStartId(Id_FileConflictPage);
}


void Nepomuk2::BackupWizard::showError(const QString &error)
{
    setField(QLatin1String("errorMessage"), error);
    setStartId(Id_ErrorPage);
}

#include "backupwizard.moc"
