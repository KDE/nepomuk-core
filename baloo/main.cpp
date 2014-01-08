/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2014  Vishesh Handa <me@vhanda.in>

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

#include "systray.h"

#include <KUniqueApplication>
#include <KAboutData>
#include <KLocale>
#include <KCmdLineArgs>
#include <KDebug>

int main( int argc, char** argv )
{
    KAboutData aboutData("nepomukbaloomigrator",
                         "nepomukbaloomigrator",
                         ki18n("NepomukBalooMigrator"),
                         "1.0",
                         ki18n("Nepomuk-Baloo Data Migration Tool"),
                         KAboutData::License_GPL,
                         ki18n("(c) 2014, Nepomuk-KDE Team"),
                         KLocalizedString(),
                         "http://nepomuk.kde.org");
    aboutData.addAuthor(ki18n("Vishesh Handa"), ki18n("Maintainer"), "me@vhanda.in");

    KCmdLineArgs::init( argc, argv, &aboutData );

    KUniqueApplication app;
    Nepomuk2::SysTray* systray = new Nepomuk2::SysTray();

    return app.exec();
}
