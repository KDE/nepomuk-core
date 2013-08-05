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
#include "datamanagementmodel.h"
#include "datamanagementadaptor.h"
#include "classandpropertytree.h"
#include "virtuosoinferencemodel.h"
#include "ontologyloader.h"

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
      m_inferenceModel( 0 ),
      m_dataManagementModel( 0 ),
      m_dataManagementAdaptor( 0 ),
      m_backend( 0 ),
      m_ontologyLoader( 0 ),
      m_port( 0 )
{
    m_dummyModel = new Soprano::Util::DummyModel();

    connect( this, SIGNAL(opened(Repository*,bool)), this, SLOT(slotOpened(Repository*,bool)) );
}


Nepomuk2::Repository::~Repository()
{
    kDebug() << m_name;
    close();
    delete m_dummyModel;
}


void Nepomuk2::Repository::close()
{
    kDebug() << m_name;

    if( m_model ) {
        emit closed(this);
    }

    // delete DMS adaptor before anything else so we do not get requests while deleting the DMS
    delete m_dataManagementAdaptor;
    m_dataManagementAdaptor = 0;

    setParentModel(m_dummyModel);
    delete m_dataManagementModel;
    m_dataManagementModel = 0;

    delete m_inferenceModel;
    m_inferenceModel = 0;

    delete m_classAndPropertyTree;
    m_classAndPropertyTree = 0;

    delete m_model;
    m_model = 0;

    delete m_ontologyLoader;
    m_ontologyLoader = 0;

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
    QString basePath = repoConfig.readPathEntry( "Storage Dir", QString() );

    if( basePath.isEmpty() ) {
        m_basePath = createStoragePath(name());
        // First time run
        repoConfig.writeEntry("GraphMigrationRequired", false);
    }
    else {
        m_basePath = basePath;
    }
    m_storagePath = m_basePath + "data/" + m_backend->pluginName();
    Soprano::BackendSettings settings = readVirtuosoSettings();

    if ( !KStandardDirs::makeDir( m_storagePath ) ) {
        kDebug() << "Failed to create storage folder" << m_storagePath;
        m_state = CLOSED;
        emit opened( this, false );
        return;
    }

    kDebug() << "opening repository '" << name() << "' at '" << m_basePath << "'";

    // WARNING:
    // This is used as a hack to get the port number and virtuoso version number. This slot
    // should not by async, as the values it sets are subsequently used.
    // The QObject cast is because m_backend is const
    QObject* qobj = dynamic_cast<QObject*>(const_cast<Soprano::Backend*>(m_backend));
    connect( qobj, SIGNAL(virtuosoInitParameters(int, QString)),
             this, SLOT(slotVirtuosoInitParameters(int,QString)) );

    // open storage
    // =================================
    Soprano::settingInSettings( settings, Soprano::BackendOptionStorageDir ).setValue( m_storagePath );
    m_model = m_backend->createModel( settings );
    if ( !m_model ) {
        kDebug() << "Unable to create model for repository" << name();
        kError() << m_backend->lastError();
        m_state = CLOSED;
        emit opened( this, false );
        return;
    }

    // We accept virtuoso version 6.1.6+ Nepomuk hasn't really been tested with 7
    QRegExp regex("(6\.1\.[6789])");
    if( !m_virtuosoVersion.contains(regex) ) {
        kError() << "NepomukStorage only works with virtuoso 6.1.6 and beyond";
        return;
    }

    connect(m_model, SIGNAL(virtuosoStopped(bool)), this, SLOT(slotVirtuosoStopped(bool)));

    kDebug() << "Successfully created new model for repository" << name();

    // create the one class and property tree to be used in DMS
    // =================================
    m_classAndPropertyTree = new Nepomuk2::ClassAndPropertyTree(this);

    // Create the Inference model which enables Virtuoso inference
    // =================================
    m_inferenceModel = new VirtuosoInferenceModel(m_model);

    // Set the parent as the main model for now. Once the ontologies have been loaded it will
    // be set to the data management model
    setParentModel( m_model );

    // save the settings
    repoConfig.writeEntry( "Used Soprano Backend", m_backend->pluginName() );
    repoConfig.writePathEntry( "Storage Dir", m_basePath );
    repoConfig.writeEntry( "Port", m_port );
    repoConfig.sync(); // even if we crash the model has been created

    m_state = OPEN;
    emit opened( this, true );
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

void Nepomuk2::Repository::slotOpened(Nepomuk2::Repository* , bool success)
{
    if( !success ) {
        emit loaded(this, false);
        return;
    }

    m_ontologyLoader = new OntologyLoader( this, this );
    connect( m_ontologyLoader, SIGNAL(ontologyUpdateFinished(bool)),
             this, SLOT(slotOntologiesLoaded(bool)) );
    m_ontologyLoader->updateLocalOntologies();
}

void Nepomuk2::Repository::slotOntologiesLoaded(bool somethingChanged)
{
    updateInference(somethingChanged);
    if( m_state == OPEN ) {
        m_state = LOADED;
        emit loaded(this, true);
    }
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
    if( m_dataManagementAdaptor )
        m_dataManagementAdaptor->setPrefixes(prefixes);

    // update the rest
    m_classAndPropertyTree->rebuildTree(this);
    m_inferenceModel->updateOntologyGraphs(ontologiesChanged);
}

void Nepomuk2::Repository::slotVirtuosoInitParameters(int port, const QString& version)
{
    m_port = port;
    m_virtuosoVersion = version;
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

void Nepomuk2::Repository::closePublicInterface()
{
    delete m_dataManagementAdaptor;
    m_dataManagementAdaptor = 0;

    setParentModel( m_inferenceModel );
    connect(m_model, SIGNAL(virtuosoStopped(bool)), this, SLOT(slotVirtuosoStopped(bool)));

    delete m_dataManagementModel;
    m_dataManagementModel = 0;
}

void Nepomuk2::Repository::openPublicInterface()
{
    if( m_dataManagementModel && m_dataManagementAdaptor )
        return;

    closePublicInterface();

    // =================================
    // create the DataManagementModel on top of everything
    m_dataManagementModel = new DataManagementModel(m_classAndPropertyTree, m_inferenceModel, this);
    setParentModel(m_dataManagementModel);

    // setParentModel disconnects all signals from the previous parent
    connect(m_model, SIGNAL(virtuosoStopped(bool)), this, SLOT(slotVirtuosoStopped(bool)));

    m_dataManagementAdaptor = new Nepomuk2::DataManagementAdaptor(m_dataManagementModel);

    QDBusConnection con = QDBusConnection::sessionBus();
    con.registerObject(QLatin1String("/datamanagement"), m_dataManagementAdaptor,
                       QDBusConnection::ExportScriptableContents);
}

#include "repository.moc"
