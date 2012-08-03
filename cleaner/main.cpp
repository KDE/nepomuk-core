/* This file is part of the KDE Project
   Copyright (c) 2012 Vishesh Handa <me@vhanda.in>

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


#include <KAboutData>

#include "nepomukversion.h"
#include "mainwindow.h"

#include <KUniqueApplication>
#include <KCmdLineArgs>

int main( int argc, char** argv )
{
    KAboutData aboutData( "nepomukcleaner",
                          "nepomukcleaner",
                          ki18n("Nepomuk Cleaner"),
                          NEPOMUK_VERSION_STRING,
                          ki18n("An Application to clean old and invalid Nepomuk data"),
                          KAboutData::License_GPL,
                          ki18n("(c) 2012, Vishesh Handa"),
                          KLocalizedString(),
                          "http://nepomuk.kde.org" );
    aboutData.setProgramIconName( "nepomuk" );
    aboutData.addAuthor(ki18n("Vishesh Handa"),ki18n("Maintainer"), "me@vhanda.in");

    KCmdLineArgs::init( argc, argv, &aboutData );

    KUniqueApplication app;

    MainWindow window;
    window.show();

    return app.exec();
}