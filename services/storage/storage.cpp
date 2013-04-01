/* This file is part of the KDE Project
   Copyright (c) 2008 Sebastian Trueg <trueg@kde.org>

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

#include "storage.h"
#include "nepomukcore.h"
#include "repository.h"

#include <QtDBus/QDBusConnection>
#include <QtCore/QFile>
#include <QCoreApplication>

#include <KDebug>
#include <KGlobal>
#include <KStandardDirs>

#include <Soprano/Backend>


Nepomuk2::Storage::Storage()
    : Service2( 0, true /* delayed initialization */ )
{
    // register the fancier name for this important service
    QDBusConnection::sessionBus().registerService( "org.kde.NepomukStorage" );

    // TODO: remove this one
    QDBusConnection::sessionBus().registerService(QLatin1String("org.kde.nepomuk.DataManagement"));

    m_core = new Core( this );
    connect( m_core, SIGNAL( initializationDone(bool) ),
             this, SLOT( slotNepomukCoreInitialized(bool) ) );
    m_core->init();
}


Nepomuk2::Storage::~Storage()
{
}


void Nepomuk2::Storage::slotNepomukCoreInitialized( bool success )
{
    if ( success ) {
        kDebug() << "Successfully initialized nepomuk core";

        // the faster local socket interface
        QString socketPath = KGlobal::dirs()->locateLocal( "socket", "nepomuk-socket" );
        QFile::remove( socketPath ); // in case we crashed
        m_core->start( socketPath );
    }
    else {
        kDebug() << "Failed to initialize nepomuk core";
    }

    setServiceInitialized( success );
}


QString Nepomuk2::Storage::usedSopranoBackend() const
{
    if ( Repository* rep = static_cast<Repository*>( m_core->model( QLatin1String( "main" ) ) ) )
        return rep->usedSopranoBackend();
    else
        return QString();
}

int main( int argc, char **argv ) {
    KAboutData aboutData( "nepomukstorage",
                          "nepomukstorage",
                          ki18n("Nepomuk Storage"),
                          NEPOMUK_VERSION_STRING,
                          ki18n("Nepomuk Storage"),
                          KAboutData::License_GPL,
                          ki18n("(c) 2008-2013, Sebastian Trüg"),
                          KLocalizedString(),
                          "http://nepomuk.kde.org" );
    aboutData.addAuthor(ki18n("Sebastian Trüg"),ki18n("Developer"), "trueg@kde.org");
    aboutData.addAuthor(ki18n("Vishesh Handa"),ki18n("Maintainer"), "me@vhanda.in");

    Nepomuk2::Service2::init<Nepomuk2::Storage>( argc, argv, aboutData );
}

#include "storage.moc"
