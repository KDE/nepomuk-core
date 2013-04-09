/* This file is part of the KDE Project
   Copyright (c) 2007-2010 Sebastian Trueg <trueg@kde.org>

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

#include "nepomukcore.h"
#include "repository.h"
#include "query/queryservice.h"
#include "backup/backupmanager.h"
#include "resourcemanager.h"

#include <KDebug>
#include <KSharedConfig>
#include <KConfigGroup>
#include <KStandardDirs>

#include <QtCore/QTimer>

static const char s_repositoryName[] = "main";

Nepomuk2::Core::Core( QObject* parent )
    : Soprano::Server::ServerCore( parent ),
      m_repository( 0 ),
      m_queryService( 0 ),
      m_backupManager( 0 ),
      m_initialized( false )
{
    // we give the Virtuoso server a server thread max of 100 which is already an insane number
    // just make sure we never reach that limit
    setMaximumConnectionCount( 80 );
}


Nepomuk2::Core::~Core()
{
    kDebug() << "Shutting down Nepomuk storage core.";
}


void Nepomuk2::Core::init()
{
    // TODO: export the main model on org.kde.NepomukRepository via Soprano::Server::DBusExportModel

    // we have only the one repository
    model( QLatin1String( s_repositoryName ) );
}


bool Nepomuk2::Core::initialized() const
{
    return m_initialized;
}


void Nepomuk2::Core::slotRepositoryLoaded( Repository* repo, bool success )
{
    if( !success ) {
        emit initializationDone( false );
    }
    else if( !m_initialized ) {
        // We overide the main model cause certain classes utilize the Resource class, and we
        // don't want them using the NepomukMainModel which communicates over a local socket.
        ResourceManager::instance()->setOverrideMainModel( repo );

        // Query Service
        m_queryService = new Query::QueryService( m_repository, this );

        // Backup Service
        m_backupManager = new BackupManager( m_repository->ontologyLoader(), m_repository, this );

        // DataManagement
        m_repository->openPublicInterface();
        kDebug() << "Registered QueryService and DataManagement interface";

        // and finally we are done: the repository is online and the ontologies are loaded.
        m_initialized = true;
        emit initializationDone( true );
    }
}


void Nepomuk2::Core::slotRepositoryClosed(Nepomuk2::Repository*)
{
    delete m_queryService;
    m_queryService = 0;

    delete m_backupManager;
    m_backupManager = 0;
}


Soprano::Model* Nepomuk2::Core::model( const QString& name )
{
    // we only allow the one model
    if ( name == QLatin1String( s_repositoryName ) ) {
        // we need to use createModel via ServerCore::model to ensure proper memory
        // management. Otherwise m_repository could be deleted before all connections
        // are down
        return ServerCore::model( name );
    }
    else {
        return 0;
    }
}


Soprano::Model* Nepomuk2::Core::createModel( const Soprano::BackendSettings& )
{
    if ( !m_repository ) {
        m_repository = new Repository( QLatin1String( s_repositoryName ) );
        connect( m_repository, SIGNAL( loaded( Repository*, bool ) ),
                 this, SLOT( slotRepositoryLoaded( Repository*, bool ) ) );
        connect( m_repository, SIGNAL( closed( Repository* ) ),
                 this, SLOT( slotRepositoryClosed( Repository* ) ) );
        QTimer::singleShot( 0, m_repository, SLOT( open() ) );
    }
    return m_repository;
}

#include "nepomukcore.moc"
