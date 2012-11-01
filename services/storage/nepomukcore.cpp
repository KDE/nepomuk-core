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
#include "ontologyloader.h"
#include "query/queryservice.h"
#include "backup/backupmanager.h"
#include "resourcemanager.h"

#include <KDebug>
#include <KSharedConfig>
#include <KConfigGroup>
#include <KStandardDirs>

#include <QtCore/QTimer>

#include <Soprano/BackendSetting>

static const char s_repositoryName[] = "main";

Nepomuk2::Core::Core( QObject* parent )
    : Soprano::Server::ServerCore( parent ),
      m_repository( 0 ),
      m_ontologyLoader( 0 ),
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


void Nepomuk2::Core::slotRepositoryOpened( Repository* repo, bool success )
{
    if( !success ) {
        emit initializationDone( success );
    }
    else if( !m_ontologyLoader ) {
        // We overide the main model cause certain classes utilize the Resource class, and we
        // don't want them using the NepomukMainModel which communicates over a local socket.
        ResourceManager::instance()->setOverrideMainModel( repo );

        // create the ontology loader, let it update all the ontologies,
        // and only then mark the service as initialized
        // TODO: fail the initialization in case loading the ontologies
        // failed.
        m_ontologyLoader = new OntologyLoader( repo, this );
        connect( m_ontologyLoader, SIGNAL(ontologyUpdateFinished(bool)),
                 this, SLOT(slotOntologiesLoaded(bool)) );
        m_ontologyLoader->updateLocalOntologies();

        // Query Service
        m_queryService = new Query::QueryService( repo, this );

        // Backup Service
        m_backupManager = new BackupManager( m_ontologyLoader, repo, this );
    }
}


void Nepomuk2::Core::slotRepositoryClosed(Nepomuk2::Repository*)
{
    delete m_ontologyLoader;
    m_ontologyLoader = 0;

    delete m_queryService;
    m_queryService = 0;

    delete m_backupManager;
    m_backupManager = 0;
}


void Nepomuk2::Core::slotOntologiesLoaded(bool somethingChanged)
{
    m_repository->updateInference(somethingChanged);

    if ( !m_initialized ) {
        // and finally we are done: the repository is online and the ontologies are loaded.
        m_initialized = true;
        emit initializationDone( true );
    }
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
        connect( m_repository, SIGNAL( opened( Repository*, bool ) ),
                 this, SLOT( slotRepositoryOpened( Repository*, bool ) ) );
        connect( m_repository, SIGNAL( closed( Repository* ) ),
                 this, SLOT( slotRepositoryClosed( Repository* ) ) );
        QTimer::singleShot( 0, m_repository, SLOT( open() ) );
    }
    return m_repository;
}

#include "nepomukcore.moc"
