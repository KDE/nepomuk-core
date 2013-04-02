/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2011  Vishesh Handa <handa.vish@gmail.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) version 3, or any
   later version accepted by the membership of KDE e.V. (or its
   successor approved by the membership of KDE e.V.), which shall
   act as a proxy defined in Section 6 of version 3 of the license.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "backupgenerationjob.h"
#include "backupfile.h"
#include "simpleresource.h"

#include <Soprano/PluginManager>
#include <Soprano/Serializer>
#include <Soprano/Util/SimpleStatementIterator>

#include <QtCore/QTimer>
#include <QtCore/QSettings>

#include <KTar>
#include <KTemporaryFile>
#include <KDebug>
#include <KTempDir>

Nepomuk2::BackupGenerationJob::BackupGenerationJob(Soprano::Model *model, const QUrl& url, QObject* parent)
    : KJob(parent),
      m_model(model),
      m_url( url )
{
}

void Nepomuk2::BackupGenerationJob::start()
{
    QTimer::singleShot( 0, this, SLOT(doWork()) );
}

namespace {
    QList<QUrl> fetchGraphApps(Soprano::Model* model, const QUrl& graph) {
        QString query = QString::fromLatin1("select ?a where { %1 nao:maintainedBy ?a . }")
                        .arg( Soprano::Node::resourceToN3( graph) );

        Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
        QList<QUrl> apps;
        while( it.next() )
            apps << it[0].uri();

        return apps;
    }

    QList<Soprano::Statement> fetchStatements(Soprano::Model* model, const QUrl& uri) {
        QString query = QString::fromLatin1("select ?p ?o ?g where { graph ?g { %1 ?p ?o. } }")
                        .arg( Soprano::Node::resourceToN3(uri) );

        QList<Soprano::Statement> stList;
        QSet<QUrl> graphs;
        Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
        while( it.next() ) {
            Soprano::Statement st( uri, it[0], it[1], it[2] );
            stList << st;

            graphs << st.context().uri();
        }

        graphs.remove( uri );
        foreach(const QUrl& g, graphs) {
            stList << fetchStatements( model, g );
        }

        return stList;
    }
}

void Nepomuk2::BackupGenerationJob::doWork()
{
    //BackupStatementIterator it( m_model );
    //BackupFile::createBackupFile( m_url, it );

    // 1. Fetch all the URIs
    QString query = QString::fromLatin1("select distinct ?r where { graph ?g { ?r ?p ?o }"
                                        " ?g a nrl:InstanceBase ."
                                        " FILTER(!(?p in (nao:lastModified, nao:created))) ."
                                        " FILTER NOT EXISTS { ?g a nrl:DiscardableInstanceBase . } }");

    kDebug() << "Fetching URI list";
    Soprano::QueryResultIterator rit = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );

    KTemporaryFile dataFile;
    dataFile.open();

    QFile file( dataFile.fileName() );
    if( !file.open( QIODevice::ReadWrite | QIODevice::Append | QIODevice::Text ) ) {
        return;
    }

    QTextStream out( &file );

    const Soprano::Serializer * serializer = Soprano::PluginManager::instance()->discoverSerializerForSerialization( Soprano::SerializationNQuads );

    QHash<QUrl, QUrl> appGraphHash;
    int numStatements = 0;

    while( rit.next() ) {
        // 2. Fetch data for each URI
        const QUrl uri = rit[0].uri();
        kDebug() << "Processing" << uri;
        SimpleResource res( rit[0].uri() );

        QString query = QString::fromLatin1("select distinct ?p ?o ?g where { graph ?g { %1 ?p ?o. } "
                                            " FILTER(!(?p in (nao:lastModified, nao:created, rdf:type, nie:url))) ."
                                            " ?g a nrl:InstanceBase ."
                                            " FILTER NOT EXISTS { ?g a nrl:DiscardableInstanceBase . } }")
                        .arg( Soprano::Node::resourceToN3(uri) );

        Soprano::QueryResultIterator it = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
        QList<Soprano::Statement> stList;
        while( it.next() ) {
            Soprano::Statement st( uri, it[0], it[1] );
            QUrl graph = it[2].uri();

            QList<QUrl> apps = fetchGraphApps( m_model, graph );
            foreach(const QUrl& app, apps) {
                QHash< QUrl, QUrl >::iterator fit = appGraphHash.find( app );
                if( fit == appGraphHash.end() ) {
                    fit = appGraphHash.insert( app, graph );
                }

                st.setContext( fit.value() );
                stList << st;
            }
        }

        if( stList.count() ) {
            QString query = QString::fromLatin1("select distinct ?p ?o ?g where { graph ?g { %1 ?p ?o. } "
                                                " FILTER(?p in (nao:lastModified, nao:created, rdf:type, nie:url)) . }")
                            .arg( Soprano::Node::resourceToN3(uri) );

            Soprano::QueryResultIterator it = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
            while( it.next() ) {
                Soprano::Statement st( uri, it[0], it[1] );
                QUrl graph = it[2].uri();

                QList<QUrl> apps = fetchGraphApps( m_model, graph );
                foreach(const QUrl& app, apps) {
                    QHash< QUrl, QUrl >::iterator fit = appGraphHash.find( app );
                    if( fit == appGraphHash.end() ) {
                        fit = appGraphHash.insert( app, graph );
                    }

                    st.setContext( fit.value() );
                    stList << st;
                }
            }

            Soprano::Util::SimpleStatementIterator iter( stList );
            serializer->serialize( iter, out, Soprano::SerializationNQuads );
            numStatements += stList.size();
        }
    }

    // Push all the graphs
    QHashIterator<QUrl, QUrl> hit( appGraphHash );
    while( hit.hasNext() ) {
        hit.next();

        QList<Soprano::Statement> stList;
        stList << fetchStatements( m_model, hit.key() );
        stList << fetchStatements( m_model, hit.value() );

        Soprano::Util::SimpleStatementIterator iter( stList );
        serializer->serialize( iter, out, Soprano::SerializationNQuads );

        numStatements += stList.size();
    }

    file.close();

    // Metadata
    KTemporaryFile tmpFile;
    tmpFile.open();
    tmpFile.setAutoRemove( false );
    QString metdataFile = tmpFile.fileName();
    tmpFile.close();

    QSettings iniFile( metdataFile, QSettings::IniFormat );
    iniFile.setValue("NumStatements", numStatements);
    iniFile.setValue("Created", QDateTime::currentDateTime().toString() );
    iniFile.sync();

    // Push to tar file
    KTar tarFile( m_url.toLocalFile(), QString::fromLatin1("application/x-gzip") );
    if( !tarFile.open( QIODevice::WriteOnly ) ) {
        kWarning() << "File could not be opened : " << m_url.toLocalFile();
        return;
    }

    tarFile.addLocalFile( dataFile.fileName(), "data" );
    tarFile.addLocalFile( metdataFile, "metadata" );

    emitResult();
}



