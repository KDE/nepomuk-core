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

#if defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__))
    #include <unistd.h>
#elif defined _WIN32
    #include <windows.h>
    static inline void sleep(unsigned int x) { Sleep(1000 * x); };
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

KService::List allServices;

void loadAllServiceNames();
void startNepomukServer();
void stopNepomukServer();
void showStatus();
bool isServerRunning();

bool isServiceRunning( const KService::Ptr& ptr );
void startService( const KService::Ptr& ptr );
void stopService( const KService::Ptr& ptr );

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

    KService::Ptr service;
    if( args->count() == 2 ) {
        serviceName = args->arg( 1 ).toLower();

        foreach( const KService::Ptr servicePtr, allServices ) {
            QString name = servicePtr->desktopEntryName().toLower();
            if( name == serviceName || name == QLatin1String("nepomuk") + serviceName ) {
                service = servicePtr;
                specificService = true;
            }
        }

        if( service.isNull() ) {
            out << "Error: service '" << serviceName << "' does not exist.\n";
            out.flush();
            KCmdLineArgs::usage();
        }
    }

    QString command = args->arg( 0 );

    bool start = command == "start" || command == "restart";
    bool stop =  command == "stop"  || command == "restart";

   if( stop ) {
        if( specificService )
            stopService( service );
        else
            stopNepomukServer();
    }

    if( start ) {
        if( stop ) {
            while( ( specificService &&  isServiceRunning( service ) ) ||
                   ( !specificService && isServerRunning() ) ) {
                sleep(1);
            }
        }

        if( specificService )
            startService( service );
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
    allServices.clear();
    allServices << KServiceTypeTrader::self()->query( "NepomukService2" );
    allServices << KServiceTypeTrader::self()->query( "NepomukService" );
}

bool isServiceRunning( const KService::Ptr& ptr )
{
    QString serviceName = ptr->desktopEntryName();
    return QDBusConnection::sessionBus().interface()->isServiceRegistered(
            QLatin1String( "org.kde.nepomuk.services." ) + serviceName );
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

void startService(const KService::Ptr& ptr)
{
    if( !isServerRunning() ) {
        out << "Nepomuk Server is not running. Starting it...\n";
        startNepomukServer();
        return;
    }

    if( isServiceRunning( ptr ) ) {
        out << ptr->desktopEntryName() << " already running.\n";
    }
    else {
        QString program = ptr->exec();
        QStringList args;

        if( program.isEmpty() ) {
            program = QLatin1String("nepomukservicestub");
            args << ptr->desktopEntryName();
        }

        if( QProcess::startDetached( program, args ) )
            out << ptr->desktopEntryName() << " started succesfully.\n";
        else
            out << ptr->desktopEntryName() << " could not be started.\n";
    }
}


void stopService( const KService::Ptr& ptr )
{
    QString serviceName = ptr->desktopEntryName();
    QDBusInterface interface( "org.kde.nepomuk.services." + serviceName,
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

        foreach( const KService::Ptr& ptr, allServices ) {
            if( isServiceRunning( ptr ) ) {
                QString name = ptr->desktopEntryName();
                if( name.startsWith(QLatin1String("nepomuk")) ) {
                    name = name.mid( 7 );
                }
                out << "Service " << name << " is running.\n";
            }
        }
    }
    else
        out << "Nepomuk Server is not running.\n";
}
