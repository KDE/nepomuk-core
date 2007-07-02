/*
 *
 * $Id: sourceheader 511311 2006-02-19 14:51:05Z trueg $
 *
 * This file is part of the Nepomuk KDE project.
 * Copyright (C) 2006-2007 Sebastian Trueg <trueg@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING" for the exact licensing terms.
 */

#include <KApplication>
#include <KDebug>
#include <KLocale>
#include <KCmdLineArgs>
#include <KAboutData>

#include "coreservices.h"

static const char* version = "0.9";
static const char* description = I18N_NOOP( "Nepomuk Core Services including the RDF Repository" );

int main( int argc, char** argv )
{
    KAboutData aboutData( "nepomukcoreservices", 0,
                          ki18n("The Nepomuk Core Services"),
                          version,
                          ki18n(description),
                          KAboutData::License_GPL,
                          ki18n("(C) 2007, Sebastian Trüg"));
    aboutData.addAuthor(ki18n("Sebastian Trüg"), KLocalizedString(), "trueg@kde.org");
    aboutData.addAuthor(ki18n("Daniele Galdi"), KLocalizedString(), "daniele.galdi@gmail.com" );

    KCmdLineArgs::init( argc, argv, &aboutData );

    KCmdLineOptions options;
    KCmdLineArgs::addCmdLineOptions( options );

    KApplication app( false ); // no need for a GUI

    Nepomuk::CoreServices::DaemonImpl instance( &app );
    if( instance.registerServices() ) {
        return app.exec();
    }
    else {
        kDebug(300002) << "Failed to setup core services." << endl;
        return -1;
    }
}
