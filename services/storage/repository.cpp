/*
 *
 * $Id: sourceheader 511311 2006-02-19 14:51:05Z trueg $
 *
 * This file is part of the Nepomuk KDE project.
 * Copyright (C) 2006-2010 Sebastian Trueg <trueg@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING" for the exact licensing terms.
 */

#include "repository.h"
#include "modelcopyjob.h"
#include "removablemediamodel.h"
#include "datamanagementmodel.h"
#include "datamanagementadaptor.h"
#include "classandpropertytree.h"
#include "virtuosoinferencemodel.h"

#include <Soprano/Backend>
#include <Soprano/PluginManager>
#include <Soprano/Global>
#include <Soprano/Version>
#include <Soprano/StorageModel>
#include <Soprano/QueryResultIterator>
#include <Soprano/Node>
#include <Soprano/Error/Error>
#include <Soprano/Vocabulary/RDF>
#include <Soprano/Vocabulary/NAO>
#include <Soprano/Vocabulary/NRL>

#include <KStandardDirs>
#include <KDebug>
#include <KConfigGroup>
#include <KSharedConfig>
#include <KLocale>
#include <KNotification>
#include <KIcon>
#include <KIO/DeleteJob>

#include <QtCore/QTimer>
#include <QtCore/QFile>
#include <QtCore/QThread>
#include <QtCore/QCoreApplication>
#include <QtDBus/QDBusConnection>

using namespace Soprano::Vocabulary;

namespace {
    QString createStoragePath( const QString& repositoryId )
    {
        return KStandardDirs::locateLocal( "data", "nepomuk/repository/" + repositoryId + '/' );
    }
}


Nepomuk2::Repository::Repository( const QString& name )
    : m_name( name ),
      m_state( CLOSED ),
      m_model( 0 ),
      m_classAndPropertyTree( 0 ),
      m_removableStorageModel( 0 ),
      m_inferenceModel( 0 ),
      m_dataManagementModel( 0 ),
      m_dataManagementAdaptor( 0 ),
      m_backend( 0 ),
      m_modelCopyJob( 0 ),
      m_oldStorageBackend( 0 )
{
}


Nepomuk2::Repository::~Repository()
{
    kDebug() << m_name;
    close();
}


void Nepomuk2::Repository::close()
{
    kDebug() << m_name;

    if(m_dataManagementModel) {
        emit closed(this);
    }

    // delete DMS adaptor before anything else so we do not get requests while deleting the DMS
    delete m_dataManagementAdaptor;
    m_dataManagementAdaptor = 0;

    setParentModel(0);
    delete m_dataManagementModel;
    m_dataManagementModel = 0;

    delete m_inferenceModel;
    m_inferenceModel = 0;

    delete m_classAndPropertyTree;
    m_classAndPropertyTree = 0;

    delete m_removableStorageModel;
    m_removableStorageModel = 0;

    delete m_modelCopyJob;
    m_modelCopyJob = 0;

    delete m_model;
    m_model = 0;

    m_state = CLOSED;
}


void Nepomuk2::Repository::open()
{
    Q_ASSERT( m_state == CLOSED );

    m_state = OPENING;

    // get backend
    // =================================
    m_backend = Soprano::PluginManager::instance()->discoverBackendByName( QLatin1String( "virtuosobackend" ) );
    if ( !m_backend ) {
        KNotification::event( "failedToStart",
                              i18nc("@info - notification message",
                                    "Nepomuk Semantic Desktop needs the Virtuoso RDF server to store its data. "
                                    "Installing the Virtuoso Soprano plugin is mandatory for using Nepomuk." ),
                              KIcon( "application-exit" ).pixmap( 32, 32 ),
                              0,
                              KNotification::Persistent );
        m_state = CLOSED;
        emit opened( this, false );
        return;
    }
    else if ( !m_backend->isAvailable() ) {
        KNotification::event( "failedToStart",
                              i18nc("@info - notification message",
                                    "Nepomuk Semantic Desktop needs the Virtuoso RDF server to store its data. "
                                    "Installing the Virtuoso server and ODBC driver is mandatory for using Nepomuk." ),
                              KIcon( "application-exit" ).pixmap( 32, 32 ),
                              0,
                              KNotification::Persistent );
        m_state = CLOSED;
        emit opened( this, false );
        return;
    }

    // read config
    // =================================
    KConfigGroup repoConfig = KSharedConfig::openConfig( "nepomukserverrc" )->group( name() + " Settings" );
    QString oldBackendName = repoConfig.readEntry( "Used Soprano Backend", m_backend->pluginName() );
    QString oldBasePath = repoConfig.readPathEntry( "Storage Dir", QString() ); // backward comp: empty string means old storage path
    Soprano::BackendSettings settings = readVirtuosoSettings();

    // If possible we want to keep the old storage path. exception: oldStoragePath is empty. In that case we stay backwards
    // compatible and convert the data to the new default location createStoragePath( name ) + "data/" + m_backend->pluginName()
    //
    // If we have a proper oldStoragePath and a different backend we use the oldStoragePath as basePath
    // newDataPath = oldStoragePath + "data/" + m_backend->pluginName()
    // oldDataPath = oldStoragePath + "data/" + oldBackendName


    // create storage paths
    // =================================
    m_basePath = oldBasePath.isEmpty() ? createStoragePath( name() ) : oldBasePath;
    QString storagePath = m_basePath + "data/" + m_backend->pluginName();

    if ( !KStandardDirs::makeDir( storagePath ) ) {
        kDebug() << "Failed to create storage folder" << storagePath;
        m_state = CLOSED;
        emit opened( this, false );
        return;
    }

    kDebug() << "opening repository '" << name() << "' at '" << m_basePath << "'";

    // remove old pre 4.4 clucene index
    // =================================
    if ( QFile::exists( m_basePath + QLatin1String( "index" ) ) ) {
        KIO::del( QString( m_basePath + QLatin1String( "index" ) ) );
    }

    // open storage
    // =================================
    Soprano::settingInSettings( settings, Soprano::BackendOptionStorageDir ).setValue( storagePath );
    m_model = m_backend->createModel( settings );
    if ( !m_model ) {
        kDebug() << "Unable to create model for repository" << name();
        m_state = CLOSED;
        emit opened( this, false );
        return;
    }

#if SOPRANO_IS_VERSION(2, 7, 3)
    connect(m_model, SIGNAL(virtuosoStopped(bool)), this, SLOT(slotVirtuosoStopped(bool)));
#endif

    kDebug() << "Successfully created new model for repository" << name();

    // create the one class and property tree to be used in DMS
    // =================================
    m_classAndPropertyTree = new Nepomuk2::ClassAndPropertyTree(this);

    // create the RemovableMediaModel which does the transparent handling of removable mounts
    // =================================
    m_removableStorageModel = new Nepomuk2::RemovableMediaModel(m_model);

    // Create the Inference model which enables Virtuoso inference
    // =================================
    m_inferenceModel = new VirtuosoInferenceModel(m_removableStorageModel);

    // create the DataManagementModel on top of everything
    // =================================
    m_dataManagementModel = new DataManagementModel(m_classAndPropertyTree, m_inferenceModel, this);
    m_dataManagementAdaptor = new Nepomuk2::DataManagementAdaptor(m_dataManagementModel);
    QDBusConnection::sessionBus().registerObject(QLatin1String("/datamanagement"), m_dataManagementAdaptor, QDBusConnection::ExportScriptableContents);
    setParentModel(m_dataManagementModel);

    // check if we have to convert
    // =================================
    bool convertingData = false;

    // if the backend changed we convert
    // in case only the storage dir changes we normally would not have to convert but
    // it is just simpler this way
    if ( oldBackendName != m_backend->pluginName() ||
         oldBasePath.isEmpty() ) {

        kDebug() << "Previous backend:" << oldBackendName << "- new backend:" << m_backend->pluginName();
        kDebug() << "Old path:" << oldBasePath << "- new path:" << m_basePath;

        if ( oldBasePath.isEmpty() ) {
            // backward comp: empty string means old storage path
            // and before we stored the data directly in the default basePath
            m_oldStoragePath = createStoragePath( name() );
        }
        else {
            m_oldStoragePath = m_basePath + "data/" + oldBackendName;
        }

        // try creating a model for the old storage
        Soprano::Model* oldModel = 0;
        m_oldStorageBackend = Soprano::discoverBackendByName( oldBackendName );
        if ( m_oldStorageBackend ) {
            // FIXME: even if there is no old data we still create a model here which results in a new empty db!
            oldModel = m_oldStorageBackend->createModel( QList<Soprano::BackendSetting>() << Soprano::BackendSetting( Soprano::BackendOptionStorageDir, m_oldStoragePath ) );
        }

        if ( oldModel ) {
            if ( !oldModel->isEmpty() ) {
                kDebug() << "Starting model conversion";

                convertingData = true;
                m_modelCopyJob = new ModelCopyJob( oldModel, m_model, this );
                connect( m_modelCopyJob, SIGNAL( result( KJob* ) ), this, SLOT( copyFinished( KJob* ) ) );
                m_modelCopyJob->start();
            }
            else {
                delete oldModel;
                m_state = OPEN;
            }
        }
        else {
            kDebug( 300002 ) << "Unable to convert old model: cound not load old backend" << oldBackendName;
            KNotification::event( "convertingNepomukDataFailed",
                                  i18nc("@info - notification message",
                                        "Nepomuk was not able to find the configured database backend '%1'. "
                                        "Existing data can thus not be accessed. "
                                        "For data security reasons Nepomuk will be disabled until "
                                        "the situation has been resolved manually.",
                                        oldBackendName ),
                                  KIcon( "nepomuk" ).pixmap( 32, 32 ),
                                  0,
                                  KNotification::Persistent );
            m_state = CLOSED;
            emit opened( this, false );
            return;
        }
    }
    else {
        kDebug() << "no need to convert" << name();
        m_state = OPEN;
    }

    // save the settings
    // =================================
    // do not save when converting yet. If converting is cancelled we would loose data.
    // this way conversion is restarted the next time
    if ( !convertingData ) {
        repoConfig.writeEntry( "Used Soprano Backend", m_backend->pluginName() );
        repoConfig.writePathEntry( "Storage Dir", m_basePath );
        repoConfig.sync(); // even if we crash the model has been created

        if( m_state == OPEN ) {
            emit opened( this, true );
        }
    }
    else {
        KNotification::event( "convertingNepomukData",
                              i18nc("@info - notification message",
                                    "Converting Nepomuk data to a new backend. This might take a while."),
                                    KIcon( "nepomuk" ).pixmap( 32, 32 ) );
    }
}


void Nepomuk2::Repository::copyFinished( KJob* job )
{
    m_modelCopyJob = 0;

    if ( job->error() ) {
        KNotification::event( "convertingNepomukDataFailed",
                              i18nc("@info - notification message",
                                    "Converting Nepomuk data to the new backend failed. "
                                    "For data security reasons Nepomuk will be disabled until "
                                    "the situation has been resolved manually."),
                              KIcon( "nepomuk" ).pixmap( 32, 32 ),
                              0,
                              KNotification::Persistent );

        kDebug( 300002 ) << "Converting old model failed.";
        m_state = CLOSED;
        emit opened( this, false );
    }
    else {
        KNotification::event( "convertingNepomukDataDone",
                              i18nc("@info - notification message",
                                    "Successfully converted Nepomuk data to the new backend."),
                                    KIcon( "nepomuk" ).pixmap( 32, 32 ) );

        kDebug() << "Successfully converted model data for repo" << name();

        // delete the old model
        ModelCopyJob* copyJob = qobject_cast<ModelCopyJob*>( job );
        delete copyJob->source();

        // cleanup the actual data
        m_oldStorageBackend->deleteModelData( QList<Soprano::BackendSetting>() << Soprano::BackendSetting( Soprano::BackendOptionStorageDir, m_oldStoragePath ) );
        m_oldStorageBackend = 0;

        // save our new settings
        KConfigGroup repoConfig = KSharedConfig::openConfig( "nepomukserverrc" )->group( name() + " Settings" );
        repoConfig.writeEntry( "Used Soprano Backend", m_backend->pluginName() );
        repoConfig.writePathEntry( "Storage Dir", m_basePath );
        repoConfig.sync();

        m_state = OPEN;
        emit opened( this, true );
    }
}


QString Nepomuk2::Repository::usedSopranoBackend() const
{
    if ( m_backend )
        return m_backend->pluginName();
    else
        return QString();
}


Soprano::BackendSettings Nepomuk2::Repository::readVirtuosoSettings() const
{
    Soprano::BackendSettings settings;

    KConfigGroup repoConfig = KSharedConfig::openConfig( "nepomukserverrc" )->group( name() + " Settings" );
    const int maxMem = repoConfig.readEntry( "Maximum memory", 50 );

    // below NumberOfBuffers=400 virtuoso crashes (at least on amd64)
    settings << Soprano::BackendSetting( "buffers", qMax( 4, maxMem-30 )*100 );

    // make a checkpoint every 10 minutes to minimize the startup time
    settings << Soprano::BackendSetting( "CheckpointInterval", 10 );

    // lower the minimum transaction log size to make sure the checkpoints are actually executed
    settings << Soprano::BackendSetting( "MinAutoCheckpointSize", 200000 );

    // alwyays index literals
    settings << Soprano::BackendSetting( "fulltextindex", "sync" );

    // Always force the start, ie. kill previously started Virtuoso instances
    settings << Soprano::BackendSetting( "forcedstart", true );

    // 100 server threads is hopefully enough - at some point the problem of maximum server threads == max client
    // needs to be addressed as well
    settings << Soprano::BackendSetting( "ServerThreads", 100 );

    // we have our own notifications through the ResourceWatcher. Thus, we disable the statement signals
    settings << Soprano::BackendSetting( "noStatementSignals", true );

    // We don't care as they screw up performance
    settings << Soprano::BackendSetting( "fakeBooleans", false );
    settings << Soprano::BackendSetting( "emptyGraphs", false );

    // Never take more than 5 minutes to answer a query (this is to filter out broken queries and bugs in Virtuoso's query optimizer)
    // trueg: We cannot activate this yet. 1. Virtuoso < 6.3 crashes and 2. even open cursors are subject to the timeout which is really
    //        not what we want!
//    settings << Soprano::BackendSetting( "QueryTimeout", 5*60000 );

    return settings;
}

void Nepomuk2::Repository::updateInference(bool ontologiesChanged)
{
    // Update the query prefixes
    QHash<QString, QString> prefixes;

    QString query = QString::fromLatin1("select ?g ?abr where { ?r %1 ?g ; %2 ?abr . }")
                    .arg( Soprano::Node::resourceToN3( NAO::hasDefaultNamespace() ),
                          Soprano::Node::resourceToN3( NAO::hasDefaultNamespaceAbbreviation() ) );

    Soprano::QueryResultIterator it = executeQuery( query, Soprano::Query::QueryLanguageSparql );
    while( it.next() ) {
        QString ontology = it[0].toString();
        QString prefix = it[1].toString();

        prefixes.insert( prefix, ontology );

        // The '2' is for the prefixes to be global
        // http://docs.openlinksw.com/virtuoso/fn_xml_set_ns_decl.html
        QString command = QString::fromLatin1("DB.DBA.XML_SET_NS_DECL( '%1', '%2', 2 )")
                          .arg( prefix, ontology );

        executeQuery( command, Soprano::Query::QueryLanguageUser, QLatin1String("sql") );
    }

    // update the prefixes in the DMS adaptor for script convenience
    m_dataManagementAdaptor->setPrefixes(prefixes);

    // update the rest
    m_classAndPropertyTree->rebuildTree(this);
    m_inferenceModel->updateOntologyGraphs(ontologiesChanged);
}

void Nepomuk2::Repository::slotVirtuosoStopped(bool normalExit)
{
    if(!normalExit) {
        kDebug() << "Virtuoso was killed or crashed. Restarting the repository.";
        // restart the dumb way for now
        // Ideally we would inform the other services so they can be restarted or something.
        close();
        open();
    }
}


#include "repository.moc"
