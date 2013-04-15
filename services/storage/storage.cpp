/* This file is part of the KDE Project
   Copyright (c) 2007-10 Sebastian Trueg <trueg@kde.org>
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

#include "storage.h"
#include "repository.h"
#include "query/queryservice.h"
#include "backup/backupmanager.h"
#include "resourcemanager.h"

#include <QtDBus/QDBusConnection>
#include <QtCore/QFile>
#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>
#include <QtCore/QDir>

#include <KDebug>
#include <KGlobal>
#include <KStandardDirs>

namespace {
    static const char s_repositoryName[] = "main";

    /**
     * A light wrapper over ServerCore which just deals with one
     * model
     */
    class LocalSever : public Soprano::Server::ServerCore {
    public:
        LocalSever(Soprano::Model* model, QObject* parent = 0)
            : ServerCore(parent), m_model(model)
        {
            setMaximumConnectionCount( 80 );
        }

        // The LocalSocketClient calls the createModel function which calls this function
        virtual Soprano::Model* model(const QString& name) {
            if( name == s_repositoryName )
                return m_model;
            return 0;
        }
    private:
        Soprano::Model* m_model;
    };
}

Nepomuk2::Storage::Storage()
    : Service2( 0, true /* delayed initialization */ )
    , m_localServer( 0 )
    , m_queryService( 0 )
    , m_backupManager( 0 )
{
    // register the fancier name for this important service
    QDBusConnection::sessionBus().registerService( "org.kde.NepomukStorage" );

    // TODO: remove this one
    QDBusConnection::sessionBus().registerService(QLatin1String("org.kde.nepomuk.DataManagement"));

    m_repository = new Repository( QLatin1String( s_repositoryName ) );
    connect( m_repository, SIGNAL( loaded( Repository*, bool ) ),
                this, SLOT( slotRepositoryLoaded( Repository*, bool ) ) );
    connect( m_repository, SIGNAL( closed( Repository* ) ),
                this, SLOT( slotRepositoryClosed() ) );
    QTimer::singleShot( 0, m_repository, SLOT( open() ) );
}


Nepomuk2::Storage::~Storage()
{
    slotRepositoryClosed();

    delete m_repository;
    m_repository = 0;
}

void Nepomuk2::Storage::slotRepositoryLoaded(Nepomuk2::Repository* repo, bool success)
{
    // We overide the main model cause certain classes utilize the Resource class, and we
    // don't want them using the NepomukMainModel which communicates over a local socket.
    ResourceManager::instance()->setOverrideMainModel( repo );

    // Backup Service
    if( !m_backupManager )
        m_backupManager = new BackupManager( this );

    openPublicInterfaces();
    kDebug() << "Registered QueryService and DataManagement interface";

    setServiceInitialized( success );
    emit initialized();
}

void Nepomuk2::Storage::openPublicInterfaces()
{
    if( !m_queryService )
        m_queryService = new Query::QueryService( m_repository, this );

    // DataManagement interface
    m_repository->openPublicInterface();

    // the faster local socket interface
    if( !m_localServer ) {
        m_localServer = new LocalSever( m_repository, this );
    }
    QString socketPath = KGlobal::dirs()->locateLocal( "socket", "nepomuk-socket" );
    QFile::remove( socketPath ); // in case we crashed
    m_localServer->start( socketPath );
}

void Nepomuk2::Storage::closePublicInterfaces()
{
    setServiceInitialized( false );
    m_repository->closePublicInterface();
    if( m_localServer )
        m_localServer->stop();

    delete m_queryService;
    m_queryService = 0;
}

void Nepomuk2::Storage::slotRepositoryClosed()
{
    closePublicInterfaces();

    delete m_localServer;
    m_localServer = 0;

    delete m_backupManager;
    m_backupManager = 0;
}

namespace {

    bool removeDir(const QString & dirName)
    {
        bool result;
        QDir dir(dirName);

        if (dir.exists(dirName)) {
            QFileInfoList list = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden |
                                                   QDir::AllDirs | QDir::Files, QDir::DirsFirst);
            foreach(QFileInfo info, list) {
                if( info.isDir() )
                    result = removeDir(info.absoluteFilePath());
                else
                    result = QFile::remove(info.absoluteFilePath());

                if( !result ) {
                    return result;
                }
            }
            result = dir.rmdir(dirName);
        }
        return result;
    }
}

void Nepomuk2::Storage::resetRepository()
{
    closePublicInterfaces();
    m_repository->disconnect( this );

    connect( m_repository, SIGNAL(closed(Repository*)),
             this, SLOT(slotRepositoryClosedAfterReset()), Qt::QueuedConnection );
    m_repository->close();
}

void Nepomuk2::Storage::slotRepositoryClosedAfterReset()
{
    // Remove the damn repo
    QString path = m_repository->storagePath();
    kWarning() << "Deleting" << path;
    removeDir(path);

    m_repository->disconnect( this );
    connect( m_repository, SIGNAL( loaded( Repository*, bool ) ),
                this, SLOT( slotRepositoryLoaded( Repository*, bool ) ) );
    connect( m_repository, SIGNAL( closed( Repository* ) ),
                this, SLOT( slotRepositoryClosed() ) );

    m_repository->open();
}



QString Nepomuk2::Storage::usedSopranoBackend() const
{
    return m_repository->usedSopranoBackend();
}

Soprano::Model* Nepomuk2::Storage::model()
{
    return m_repository;
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
