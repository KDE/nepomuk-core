/*
   Copyright (c) 2013 Gabriel Poesia <gabriel.poesia@gmail.com>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cstdlib>

#ifdef __unix__
    #include <unistd.h>
#elif defined _WIN32
    #include <windows.h>
    #define sleep(x) Sleep(1000 * x)
#else
    #define sleep(x)
#endif

#include <QTextStream>
#include <QProcess>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QThread>
#include <KAboutData>
#include <KCmdLineArgs>
#include <KService>
#include <KServiceTypeTrader>

QStringList allServiceNames;

void loadAllServiceNames();
void startNepomukServer();
void stopNepomukServer();
void startService( const QString& );
void stopService( const QString& );
void showStatus();
bool isServiceRunning( const QString& );
bool isServerRunning();

QTextStream out ( stdout );

int main( int argc, char* argv[] )
{
    QCoreApplication app(argc, argv);
    KAboutData aboutData( "nepomukctl",
                          "nepomukctl",
                          ki18n( "Nepomuk server manipulation tool" ),
                          "1.0",
                          ki18n( "Starts, stops or lists running Nepomuk services" ),
                          KAboutData::License_GPL,
                          ki18n( "(c) 2013, Gabriel Poesia" ),
                          KLocalizedString(),
                          "http://nepomuk.kde.org" );

    aboutData.addAuthor( ki18n( "Gabriel Poesia" ),
                         ki18n( "Developer" ),
                         "gabriel.poesia@gmail.com" );
    aboutData.setProgramIconName( "nepomuk" );

    KCmdLineOptions options;

    options.add( "+command", ki18n( "One of the available commands (start, stop, restart or status)" ) );
    options.add( "+[service]", ki18n( "Name of a Nepomuk service (filewatch, fileindexer or storage)\n"
                 "When a service name isn't given, the command will be applied to the Nepomuk Server" ) );

    KCmdLineArgs::init( argc, argv, &aboutData );
    KCmdLineArgs::addCmdLineOptions( options );
    KCmdLineArgs::addStdCmdLineOptions();

    KCmdLineArgs* args = KCmdLineArgs::parsedArgs();

    loadAllServiceNames();

    if( args->count() == 0 )
        KCmdLineArgs::usage();

    QString serviceName;
    bool specificService = false;

    if( args->count() == 2 ) {
        serviceName = args->arg( 1 );

        if( !allServiceNames.contains( serviceName ) ) {
            out << "Error: service '" << serviceName << "' does not exist.\n";
            out.flush();
            KCmdLineArgs::usage();
        }

        specificService = true;
    }

    QString command = args->arg( 0 );

    bool start = command == "start" || command == "restart";
    bool stop =  command == "stop"  || command == "restart";

   if( stop ) {
        if( specificService )
            stopService( serviceName );
        else
            stopNepomukServer();
    }

    if( start ) {
        if( stop ) {
            while( ( specificService &&  isServiceRunning( serviceName ) ) ||
                   ( !specificService && isServerRunning() ) ) {
                sleep(1);
            }
        }

        if( specificService )
            startService( serviceName );
        else
            startNepomukServer();
    }

    if( !( start || stop ) ) {
        if( command == "status" )
            showStatus();
        else {
            out << "Error: unrecognized command '" << command << "'.\n";
            out.flush();
            KCmdLineArgs::usage();
        }
    }

    return EXIT_SUCCESS;
}

void loadAllServiceNames()
{
    const QString serviceNamePrefix( "nepomuk" );
    allServiceNames = QStringList();

    const KService::List modules = KServiceTypeTrader::self()->query( "NepomukService" );
    for( KService::List::ConstIterator it = modules.constBegin(); it != modules.constEnd(); ++it )
        allServiceNames << (*it)->desktopEntryName().remove( 0, serviceNamePrefix.size() );
}

bool isServiceRunning( const QString& serviceName )
{
    return QDBusConnection::sessionBus().interface()->isServiceRegistered(
            QLatin1String( "org.kde.nepomuk.services.nepomuk" ) + serviceName );
}

bool isServerRunning()
{
    return QDBusConnection::sessionBus().interface()->isServiceRegistered(
            QLatin1String( "org.kde.NepomukServer" ) );
}

void startNepomukServer()
{
    if( isServerRunning() )
        out << "Nepomuk Server already running.\n";
    else if( QProcess::startDetached( "nepomukserver" ) )
        out << "Nepomuk Server started successfully.\n";
    else
        out << "Could not start the Nepomuk Server.\n";
}

void stopNepomukServer()
{
    QDBusInterface interface( "org.kde.NepomukServer", "/nepomukserver" );

    QDBusReply<void> reply = interface.call( "quit" );

    if( reply.isValid() )
        out << "Nepomuk Server stopped succesfully.\n";
    else
        out << "Couldn't stop the Nepomuk Server: " << interface.lastError().message() << "\n";
}

void startService( const QString& serviceName )
{
    if( !isServerRunning() ) {
        out << "Nepomuk Server is not running. Starting it...\n";
        startNepomukServer();
        return;
    }

    if( isServiceRunning( serviceName ) ) {
        out << serviceName << " already running.\n";
    }
    else {
        QString fullName = QLatin1String( "nepomuk" ) + serviceName;

        if( QProcess::startDetached( "nepomukservicestub", QStringList( fullName ) ) )
            out << serviceName << " started succesfully.\n";
        else
            out << serviceName << " could not be started.\n";
    }
}


void stopService( const QString& serviceName )
{
    QDBusInterface interface( "org.kde.nepomuk.services.nepomuk" + serviceName,
                              "/servicecontrol" );

    QDBusReply<void> reply = interface.call( "shutdown" );

    if( reply.isValid() )
        out << "Nepomuk " << serviceName << " stopped succesfully.\n";
    else
        out << "Couldn't stop Nepomuk " << serviceName << ": "
            << interface.lastError().message() << "\n";
}

void showStatus()
{
    if( isServerRunning() ) {
        out << "Nepomuk Server is running.\n";

        Q_FOREACH( const QString& serviceName, allServiceNames ) {
            if( isServiceRunning( serviceName ) )
                out << "Service " << serviceName << " is running.\n";
        }
    }
    else
        out << "Nepomuk Server is not running.\n";
}
