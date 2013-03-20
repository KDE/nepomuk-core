/* This file is part of the KDE Project
   Copyright (c) 2008 Sebastian Trueg <trueg@kde.org>
   Copyright (c) 2013 Vishesh Handa <me@vhanda.in>

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

#ifndef _NEPOMUK2_SERVICE2_H_
#define _NEPOMUK2_SERVICE2_H_

#include "nepomuk_export.h"
#include "nepomukversion.h"

#include <QtCore/QObject>
#include <QtCore/QCoreApplication>
#include <QtCore/QTextStream>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusConnectionInterface>

#include <KAboutData>
#include <KComponentData>
#include <KApplication>
#include <KCmdLineArgs>

namespace Nepomuk2 {
    /**
     * \class Service2 service.h Nepomuk2/Service2
     *
     * \brief New Base class for all Nepomuk services.
     *
     * A %Nepomuk service is intended to perform some kind of
     * operation on the %Nepomuk data storage. This can include
     * data gathering, data enrichment, or enhanced data query.
     *
     * %Nepomuk services are started and managed by the %Nepomuk
     * server. Very much like KDED modules a %Nepomuk service
     * has autostart and start-on-demand properties. In addition
     * a %Nepomuk service can define an arbitrary number of
     * dependencies which are necessary to run the service. These
     * dependencies name other services.
     *
     * The following is needed for creating a new service -
     * - A class deriving from the Service2 class
     * - call init() in your main function
     * - a .desktop file similar to the following example
     *   \code
     * [Desktop Entry]
     * Type=Service
     * X-KDE-ServiceTypes=NepomukService
     * X-KDE-Nepomuk-autostart=true
     * X-KDE-Nepomuk-start-on-demand=false
     * # Dependencies default to 'nepomukstorage'
     * X-KDE-Nepomuk-dependencies=nepomukfoobar
     * Name=My fancy Nepomuk Service
     * Comment=A Nepomuk service that does fancy things
     * Exec=nepomuk_service_exec
     *
     *  \endcode
     *
     * \code
     * class MyService : public Nepomuk2::Service2
     * {
     *    // do fancy stuff
     * };
     * \endcode
     *
     * The %Nepomuk server will automatically export all D-Bus
     * interfaces defined on the service instance. Thus, the 
     * simplest way to export methods via D-Bus is by marking
     * them with Q_SCRIPTABLE.
     *
     * \author Vishesh Handa <me@vhanda.in>
     *
     * \since 4.11
     */
    class NEPOMUK_EXPORT Service2 : public QObject
    {
        Q_OBJECT

    public:
        /**
         * Use this method in the main function of your service
         * application to initialize your service.
         * This method takes care of creation a KApplication,
         * registering it on DBus and parsing the command line args
         *
         * @code
         *   int main( int argc, char **argv ) {
         *      KAboutData aboutData( ... );
         *      return Nepomuk2::Service2<MyService>( argc, argv, aboutData );
         *   }
         * @endcode
         */
        template <typename T>
        static int init( int argc, char **argv, const KAboutData& aboutData )
        {
            QCoreApplication app( argc, argv );
            KComponentData data( aboutData, KComponentData::RegisterAsMainComponent );

            const QString name = aboutData.appName();
            const QString dbusName = dbusServiceName( name );

            QTextStream s( stderr );
            QDBusConnectionInterface* busInt = QDBusConnection::sessionBus().interface();
            if( busInt->isServiceRegistered( dbusName ) ) {
                s << "Service " << name << " already running." << endl;
                return 1;
            }

            T* service = new T();
            if( !service->initCommon(name) )
                return 1;

            int rv = app.exec();
            delete service;
            return rv;
        }

        /**
         * Similar to the init method above except that it creates
         * an application with a UI so that you can use UI components
         * in the service
         */
        template <typename T>
        static int initUI( int argc, char **argv, const KAboutData& aboutData ) {
            KCmdLineArgs::init( argc, argv, &aboutData );

            KApplication app;
            app.disableSessionManagement();

            // We explicitly remove the MainApplication object cause it inteferes with the ability to
            // properly shut down the nepomuk services
            QDBusConnection con = QDBusConnection::sessionBus();
            con.unregisterObject( QLatin1String("/MainApplication"), QDBusConnection::UnregisterNode );

            const QString name = aboutData.appName();
            const QString dbusName = dbusServiceName( name );

            QTextStream s( stderr );
            QDBusConnectionInterface* busInt = QDBusConnection::sessionBus().interface();
            if( busInt->isServiceRegistered( dbusName ) ) {
                s << "Service " << name << " already running." << endl;
                return 1;
            }

            T* service = new T();
            if( !service->initCommon(name) )
                return 1;

            int rv = app.exec();
            delete service;
            return rv;
        }

        /**
         * Gives the dbus service name for the service with name \p serviceName
         */
        static QString dbusServiceName( const QString& serviceName );

        /**
         * Create a new Service.
         *
         * \param parent The parent object
         * \param delayedInitialization If \p true the service will not
         * be usable until setServiceInitialized has been called.
         * This allows to design services that need to perform 
         * long initialization tasks.
         */
        Service2( QObject* parent = 0, bool delayedInitialization = false );

        /**
         * Destructor
         */
        virtual ~Service2();

        /**
         * Return the generic service name.
         */
        QString name();

        /**
         * Returns the description of the service
         */
        QString description();

        /**
         * Returns the DBus Service Name
         */
        QString dbusServiceName();

    protected:
        /**
         * A %Nepomuk service can make use of a warmup phase in which
         * it is not usable yet. Call this method once your service
         * is fully initialized.
         *
         * Most services do not need to call this method.
         *
         * \param success Set to \c true if initialization was
         * successful, \c false otherwise.
         *
         * \sa Service::Service
         */
        void setServiceInitialized( bool success );

        bool initCommon( const QString& name );
    private:
        class Private;
        Private* const d;
    };
}


#endif
