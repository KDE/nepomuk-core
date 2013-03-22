/* This file is part of the KDE Project
   Copyright (c) 2007-2011 Sebastian Trueg <trueg@kde.org>
   Copyright (c) 2012-2013 Vishesh Handa <me@vhanda.in>

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

#include "nepomukfilewatch.h"
#include "metadatamover.h"
#include "fileindexerinterface.h"
#include "fileexcludefilters.h"
#include "removabledeviceindexnotification.h"
#include "removablemediacache.h"
#include "fileindexerconfig.h"
#include "activefilequeue.h"
#include "removablemediadatamigrator.h"

#ifdef BUILD_KINOTIFY
#include "kinotify.h"
#endif

#include <QtCore/QDir>
#include <QtCore/QThread>
#include <QtDBus/QDBusConnection>

#include <KDebug>
#include <KUrl>
#include <KPluginFactory>
#include <KConfigGroup>

#include "resourcemanager.h"
#include "nie.h"

#include <Soprano/Model>
#include <Soprano/QueryResultIterator>
#include <Soprano/Node>

using namespace Nepomuk2::Vocabulary;

NEPOMUK_EXPORT_SERVICE( Nepomuk2::FileWatch, "nepomukfilewatch")


#ifdef BUILD_KINOTIFY
namespace {
    /**
     * A variant of KInotify which ignores all paths in the Nepomuk
     * ignore list.
     */
    class IgnoringKInotify : public KInotify
    {
    public:
        IgnoringKInotify( RegExpCache* rec, QObject* parent );
        ~IgnoringKInotify();

    protected:
        bool filterWatch( const QString & path, WatchEvents & modes, WatchFlags & flags );

    private:
        RegExpCache* m_pathExcludeRegExpCache;
    };

    IgnoringKInotify::IgnoringKInotify( RegExpCache* rec, QObject* parent )
        : KInotify( parent ),
          m_pathExcludeRegExpCache( rec )
    {
    }

    IgnoringKInotify::~IgnoringKInotify()
    {
    }

    bool IgnoringKInotify::filterWatch( const QString & path, WatchEvents & modes, WatchFlags & flags )
    {
        Q_UNUSED( flags );

        if( !Nepomuk2::FileIndexerConfig::self()->shouldFolderBeWatched( path ) )
            return false;
        // Only watch the index folders for file creation and change.
        if( Nepomuk2::FileIndexerConfig::self()->shouldFolderBeIndexed( path ) ) {
            modes |= KInotify::EventCloseWrite;
            modes |= KInotify::EventCreate;
        }
        else {
            modes &= (~KInotify::EventCloseWrite);
            modes &= (~KInotify::EventCreate);
            return Nepomuk2::FileIndexerConfig::self()->shouldFolderBeWatched( path );
        }
        return true;
    }
}
#endif // BUILD_KINOTIFY


Nepomuk2::FileWatch::FileWatch( QObject* parent, const QList<QVariant>& )
    : Service( parent )
#ifdef BUILD_KINOTIFY
    , m_dirWatch( 0 )
#endif
{
    // Create the configuration instance singleton (for thread-safety)
    // ==============================================================
    (void)new FileIndexerConfig(this);

    // the list of default exclude filters we use here differs from those
    // that can be configured for the file indexer service
    // the default list should only contain files and folders that users are
    // very unlikely to ever annotate but that change very often. This way
    // we avoid a lot of work while hopefully not breaking the workflow of
    // too many users.
    m_pathExcludeRegExpCache = new RegExpCache();
    m_pathExcludeRegExpCache->rebuildCacheFromFilterList( defaultExcludeFilterList() );

    // start the mover thread
    m_metadataMoverThread = new QThread(this);
    m_metadataMoverThread->start();
    m_metadataMover = new MetadataMover( ResourceManager::instance()->mainModel() );
    connect( m_metadataMover, SIGNAL(movedWithoutData(QString)),
             this, SLOT(slotMovedWithoutData(QString)),
             Qt::QueuedConnection );
    m_metadataMover->moveToThread(m_metadataMoverThread);

    m_fileModificationQueue = new ActiveFileQueue(this);
    connect(m_fileModificationQueue, SIGNAL(urlTimeout(KUrl)),
            this, SLOT(slotActiveFileQueueTimeout(KUrl)));

#ifdef BUILD_KINOTIFY
    // monitor the file system for changes (restricted by the inotify limit)
    m_dirWatch = new IgnoringKInotify( m_pathExcludeRegExpCache, this );

    connect( m_dirWatch, SIGNAL( moved( QString, QString ) ),
             this, SLOT( slotFileMoved( QString, QString ) ) );
    connect( m_dirWatch, SIGNAL( deleted( QString, bool ) ),
             this, SLOT( slotFileDeleted( QString, bool ) ) );
    connect( m_dirWatch, SIGNAL( created( QString, bool ) ),
             this, SLOT( slotFileCreated( QString, bool ) ) );
    connect( m_dirWatch, SIGNAL( closedWrite( QString ) ),
             this, SLOT( slotFileClosedAfterWrite( QString ) ) );
    connect( m_dirWatch, SIGNAL( watchUserLimitReached() ),
             this, SLOT( slotInotifyWatchUserLimitReached() ) );

    // recursively watch the whole home dir

    // FIXME: we cannot simply watch the folders that contain annotated files since moving
    // one of these files out of the watched "area" would mean we "lose" it, i.e. we have no
    // information about where it is moved.
    // On the other hand only using the homedir means a lot of restrictions.
    // One dummy solution would be a hybrid: watch the whole home dir plus all folders that
    // contain annotated files outside of the home dir and hope for the best

    watchFolder( QDir::homePath() );
#else
    connectToKDirWatch();
#endif

    // we automatically watch newly mounted media - it is very unlikely that anything non-interesting is mounted
    m_removableMediaCache = new RemovableMediaCache(this);
    connect(m_removableMediaCache, SIGNAL(deviceMounted(const Nepomuk2::RemovableMediaCache::Entry*)),
            this, SLOT(slotDeviceMounted(const Nepomuk2::RemovableMediaCache::Entry*)));
    connect(m_removableMediaCache, SIGNAL(deviceTeardownRequested(const Nepomuk2::RemovableMediaCache::Entry*)),
            this, SLOT(slotDeviceTeardownRequested(const Nepomuk2::RemovableMediaCache::Entry*)));
    addWatchesForMountedRemovableMedia();

    connect( FileIndexerConfig::self(), SIGNAL( configChanged() ),
             this, SLOT( updateIndexedFoldersWatches() ) );
}


Nepomuk2::FileWatch::~FileWatch()
{
    kDebug();
    m_metadataMoverThread->quit();
    m_metadataMoverThread->wait();
    delete m_metadataMover;
}


// FIXME: listen to Create for folders!
void Nepomuk2::FileWatch::watchFolder( const QString& path )
{
    kDebug() << path;
#ifdef BUILD_KINOTIFY
    if ( m_dirWatch && !m_dirWatch->watchingPath( path ) )
        m_dirWatch->addWatch( path,
                              KInotify::WatchEvents( KInotify::EventMove|KInotify::EventDelete|KInotify::EventDeleteSelf|KInotify::EventCloseWrite|KInotify::EventCreate ),
                              KInotify::WatchFlags() );
#endif
}


void Nepomuk2::FileWatch::slotFileMoved( const QString& urlFrom, const QString& urlTo )
{
    if( !ignorePath( urlFrom ) || !ignorePath( urlTo ) ) {
        const KUrl from( urlFrom );
        const KUrl to( urlTo );
        m_metadataMover->moveFileMetadata( from, to );
    }
}


void Nepomuk2::FileWatch::slotFilesDeleted( const QStringList& paths )
{
    KUrl::List urls;
    foreach( const QString& path, paths ) {
        if( !ignorePath( path ) ) {
            urls << KUrl(path);
        }
    }

    if( !urls.isEmpty() ) {
        m_metadataMover->removeFileMetadata( urls );
    }
}


void Nepomuk2::FileWatch::slotFileDeleted( const QString& urlString, bool isDir )
{
    // Directories must always end with a trailing slash '/'
    QString url = urlString;
    if( isDir && url[ url.length() - 1 ] != '/') {
        url.append('/');
    }
    slotFilesDeleted( QStringList( url ) );
}


void Nepomuk2::FileWatch::slotFileCreated( const QString& path, bool isDir )
{
    // we only need the file creation event for folders
    // file creation is always followed by a CloseAfterWrite event
    if(isDir) {
        updateFileViaFileIndexer(path);
    }
}

void Nepomuk2::FileWatch::slotFileClosedAfterWrite( const QString& path )
{
    QDateTime current = QDateTime::currentDateTime();
    QDateTime modification = QFileInfo(path).lastModified();

    // If we have recieved a FileClosedAfterWrite event, then the file must have been
    // closed within the last minute.
    // This is being done cause many applications open the file under write mode, do not
    // make any modifications and then close the file. This results in us getting
    // the FileClosedAfterWrite event
    if( modification.secsTo(current) <= 1000 * 60 ) {
        m_fileModificationQueue->enqueueUrl( path );
    }
}

void Nepomuk2::FileWatch::slotMovedWithoutData( const QString& path )
{
    updateFileViaFileIndexer( path );
}


// static
void Nepomuk2::FileWatch::updateFileViaFileIndexer(const QString &path)
{
    if( FileIndexerConfig::self()->shouldBeIndexed(path) ) {
        org::kde::nepomuk::FileIndexer fileIndexer( "org.kde.nepomuk.services.nepomukfileindexer", "/nepomukfileindexer", QDBusConnection::sessionBus() );
        if ( fileIndexer.isValid() ) {
            fileIndexer.indexFile( path );
        }
    }
}


// static
void Nepomuk2::FileWatch::updateFolderViaFileIndexer( const QString& path )
{
    if( FileIndexerConfig::self()->shouldBeIndexed(path) ) {
        //
        // Tell the file indexer service (if running) to update the newly created
        // folder or the folder containing the newly created file
        //
        org::kde::nepomuk::FileIndexer fileIndexer( "org.kde.nepomuk.services.nepomukfileindexer", "/nepomukfileindexer", QDBusConnection::sessionBus() );
        if ( fileIndexer.isValid() ) {
            fileIndexer.updateFolder( path, false /* non-recursive */, false /* no forced update */ );
        }
    }
}


void Nepomuk2::FileWatch::connectToKDirWatch()
{
    // monitor KIO for changes
    QDBusConnection::sessionBus().connect( QString(), QString(), "org.kde.KDirNotify", "FileMoved",
                                           this, SIGNAL( slotFileMoved( const QString&, const QString& ) ) );
    QDBusConnection::sessionBus().connect( QString(), QString(), "org.kde.KDirNotify", "FilesRemoved",
                                           this, SIGNAL( slotFilesDeleted( const QStringList& ) ) );
}


#ifdef BUILD_KINOTIFY
void Nepomuk2::FileWatch::slotInotifyWatchUserLimitReached()
{
    // we do it the brutal way for now hoping with new kernels and defaults this will never happen
    // Delete the KInotify and switch to KDirNotify dbus signals
    if( m_dirWatch ) {
        m_dirWatch->deleteLater();
        m_dirWatch = 0;
    }
    connectToKDirWatch();
}
#endif


bool Nepomuk2::FileWatch::ignorePath( const QString& path )
{
    // when using KInotify there is no need to check the folder since
    // we only watch interesting folders to begin with.
    return m_pathExcludeRegExpCache->filenameMatch( path );
}


void Nepomuk2::FileWatch::updateIndexedFoldersWatches()
{
#ifdef BUILD_KINOTIFY
    if( m_dirWatch ) {
        QStringList folders = FileIndexerConfig::self()->includeFolders();
        foreach( const QString & folder, folders ) {
            m_dirWatch->removeWatch( folder );
            watchFolder( folder );
        }
    }
#endif
}


void Nepomuk2::FileWatch::addWatchesForMountedRemovableMedia()
{
    Q_FOREACH(const RemovableMediaCache::Entry* entry, m_removableMediaCache->allMedia()) {
        if(entry->isMounted())
            slotDeviceMounted(entry);
    }
}

void Nepomuk2::FileWatch::slotDeviceMounted(const Nepomuk2::RemovableMediaCache::Entry* entry)
{
    //
    // tell the file indexer to update the newly mounted device
    //
    KConfig fileIndexerConfig( "nepomukstrigirc" );
    const QByteArray devGroupName( "Device-" + entry->url().toUtf8() );
    int index = 0;
    // Old-format -> port to new format
    if(fileIndexerConfig.group("Devices").hasKey(entry->url())) {
        KConfigGroup devicesGroup = fileIndexerConfig.group( "Devices" );
        index = devicesGroup.readEntry(entry->url(), false) ? 1 : -1;
        devicesGroup.deleteEntry( entry->url() );

        KConfigGroup devGroup = fileIndexerConfig.group( devGroupName );
        devGroup.writeEntry( "mount path", entry->mountPath() );
        if( index == 1 )
            devGroup.writeEntry( "folders", QStringList() << QLatin1String("/") );
        else if( index == -1 )
            devGroup.writeEntry( "exclude folders", QStringList() << QLatin1String("/") );

        fileIndexerConfig.sync();

        // Migrate the existing filex data
        RemovableMediaDataMigrator* migrator = new RemovableMediaDataMigrator( entry );
        QThreadPool::globalInstance()->start( migrator );
    }

    // Already exists
    else if( fileIndexerConfig.hasGroup( devGroupName ) ) {
        // Inform the indexer to start the indexing process

        org::kde::nepomuk::FileIndexer fileIndexer( "org.kde.nepomuk.services.nepomukfileindexer", "/nepomukfileindexer", QDBusConnection::sessionBus() );
        if ( fileIndexer.isValid() ) {
            KConfigGroup group = fileIndexerConfig.group( devGroupName );
            const QString mountPath = group.readEntry( "mount path", QString() );
            if( !mountPath.isEmpty() ) {
                const QStringList folders = group.readPathEntry("folders", QStringList());
                foreach( const QString& folder, folders ) {
                    QString path = mountPath + folder;
                    fileIndexer.indexFolder( path, true, false );
                }
            }
        }

        index = 1;
    }

    const bool indexNewlyMounted = fileIndexerConfig.group( "RemovableMedia" ).readEntry( "index newly mounted", false );
    const bool askIndividually = fileIndexerConfig.group( "RemovableMedia" ).readEntry( "ask user", false );

    if( index == 0 && indexNewlyMounted && !askIndividually ) {
        KConfigGroup deviceGroup = fileIndexerConfig.group( "Device-" + entry->url() );
        deviceGroup.writeEntry( "folders", QStringList() << entry->mountPath() );

        index = 1;
    }

    // ask the user if we should index
    else if( index == 0 && indexNewlyMounted && askIndividually ) {
        kDebug() << "Device unknown. Asking user for action.";
        (new RemovableDeviceIndexNotification(entry, this))->sendEvent();
    }

    //
    // Install the watches
    //
    KConfig config("nepomukstrigirc");
    KConfigGroup cfg = config.group( "RemovableMedia" );

    if( cfg.readEntry<bool>( "add watches", true ) ) {
        QString path = entry->mountPath();
        // If the device is not a storage device, mountPath returns QString().
        // In this case do not try to install watches.
        if( path.isEmpty() )
            return;
        if( entry->device().isDeviceInterface( Solid::DeviceInterface::NetworkShare ) ) {
            if( cfg.readEntry<bool>( "add watches network share", false ) ) {
                kDebug() << "Installing watch for network share at mount point" << path;
                watchFolder(path);
            }
        }
        else {
            kDebug() << "Installing watch for removable storage at mount point" << path;
            // vHanda: Perhaps this should only be done if we have some metadata on the removable media
            // and if we do not then we add the watches when we get some metadata?
            watchFolder(path);
        }
    }
}

void Nepomuk2::FileWatch::slotDeviceTeardownRequested(const Nepomuk2::RemovableMediaCache::Entry* entry )
{
#ifdef BUILD_KINOTIFY
    if( m_dirWatch ) {
        kDebug() << entry->mountPath();
        m_dirWatch->removeWatch( entry->mountPath() );
    }
#endif
}


void Nepomuk2::FileWatch::slotActiveFileQueueTimeout(const KUrl &url)
{
    kDebug() << url;
    updateFileViaFileIndexer(url.toLocalFile());
}

#include "nepomukfilewatch.moc"
