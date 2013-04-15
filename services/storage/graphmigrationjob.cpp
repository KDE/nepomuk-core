/*
 * <one line to give the library's name and an idea of what it does.>
 * Copyright 2013  Vishesh Handa <me@vhanda.in>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "graphmigrationjob.h"

#include <Soprano/Model>
#include <Soprano/QueryResultIterator>
#include <Soprano/Vocabulary/NRL>

#include <QtCore/QTimer>
#include <Soprano/Vocabulary/NAO>
#include <KDebug>

using namespace Soprano::Vocabulary;
using namespace Nepomuk2;


GraphMigrationJob::GraphMigrationJob(Soprano::Model* model, QObject* parent)
    : KJob(parent)
    , m_model(model)
{
}

void GraphMigrationJob::start()
{
    QTimer::singleShot( 0, this, SLOT(migrateData()) );
}

namespace {
    int fetchAppCount(Soprano::Model* model, const QUrl& graph) {
        QString query = QString::fromLatin1("select count(?app) as ?c where { %1 nao:maintainedBy ?app . }")
                        .arg( Soprano::Node::resourceToN3(graph) );

        Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
        if( it.next() )
            return it[0].literal().toInt();

        return 0;
    }

    QUrl nepomukGraph(Soprano::Model* model) {
        QString query = QString::fromLatin1("select ?r where { ?r a nrl:Graph ; nao:maintainedBy ?app ."
                                            " ?app nao:identifier %1 . } LIMIT 1")
                        .arg( Soprano::Node::literalToN3(QLatin1String("nepomuk")) );

        Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
        if( it.next() )
            return it[0].uri();

        return QUrl();
    }
}

void GraphMigrationJob::mergeAgents()
{
    QString appQ = QString::fromLatin1("select distinct ?i where { ?r a %1 ; %2 ?i . }")
                   .arg( Soprano::Node::resourceToN3(NAO::Agent()),
                         Soprano::Node::resourceToN3(NAO::identifier()) );

    Soprano::QueryResultIterator it = m_model->executeQuery( appQ, Soprano::Query::QueryLanguageSparqlNoInference );
    while( it.next() ) {
        QString identifier = it[0].literal().toString();
        kDebug() << identifier;

        QString aQuery = QString::fromLatin1("select distinct ?r where { ?r a nao:Agent ; nao:identifier %1. }")
                         .arg( Soprano::Node::literalToN3(identifier) );

        Soprano::QueryResultIterator iter = m_model->executeQuery( aQuery, Soprano::Query::QueryLanguageSparqlNoInference );
        QList<QUrl> apps;
        while( iter.next() )
            apps << iter[0].uri();

        mergeAgents( apps );
    }
}

void GraphMigrationJob::mergeAgents(const QList< QUrl >& agents)
{
    kDebug() << "Merging agents" << agents;
    QUrl mainApp;
    foreach(const QUrl& app, agents) {
        if( mainApp.isEmpty() ) {
            mainApp = app;
            continue;
        }

        QString insertCom = QString::fromLatin1("insert into ?g { ?r nao:maintainedBy %1 . }"
                                                "where { graph ?g { ?r nao:maintainedBy %2. }")
                            .arg( Soprano::Node::resourceToN3(mainApp),
                                  Soprano::Node::resourceToN3(app) );

        m_model->removeAllStatements( app, QUrl(), QUrl() );
        m_model->removeAllStatements( QUrl(), QUrl(), app );
    }
}


void GraphMigrationJob::migrateData()
{
    mergeAgents();

    int graphCount = instanceBaseGraphCount();
    int discardableGraphCount = discardableInstanceBaseGraphCount();
    int totalCount = graphCount + discardableGraphCount;
    int count = 0;

    kDebug() << "Total Count:" << totalCount;

    m_nepomukGraph = nepomukGraph(m_model);
    if( m_nepomukGraph.isEmpty() ) {
        setErrorText("No nepomuk graph. Something is very wrong. Please report a bug");
        emitResult();
    }

    QString appQ = QString::fromLatin1("select distinct ?r where { ?r a %1 ; %2 ?i . }")
                   .arg( Soprano::Node::resourceToN3(NAO::Agent()),
                         Soprano::Node::resourceToN3(NAO::identifier()) );

    Soprano::QueryResultIterator it = m_model->executeQuery( appQ, Soprano::Query::QueryLanguageSparqlNoInference );
    if( it.next() ) {
        QUrl app = it[0].uri();
        if( !app.isEmpty() ) {
            m_apps << app;
            kDebug() << "Inserting" << app;
        }
    }

    if( m_apps.isEmpty() ) {
        setErrorText( "No application agents found. Data cannot be migrated" );
        emitResult();
    }

    // Fetch a graph for each application
    QHash<QUrl, QUrl> graphCache;
    QHash<QUrl, QUrl> discardableGraphCache;

    foreach(const QUrl& app, m_apps) {
        // Normal Graph
        QString query = QString::fromLatin1("select ?g where { ?g a nrl:Graph ; nao:maintainedBy %1 ."
                                            " FILTER NOT EXISTS { ?g a nrl:DiscardableInstanceBase . } }")
                        .arg( Soprano::Node::resourceToN3(app) );

        it = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
        while (it.next()) {
            const QUrl graph = it[0].uri();
            if( fetchAppCount(m_model, graph) == 1 ) {
                graphCache.insert( app, graph );
                count++;
                emitPercent( count, totalCount );
            }
        }

        // Discardable Graphs
        query = QString::fromLatin1("select ?g where { ?g a nrl:DiscardableInstanceBase ; nao:maintainedBy %1 .}")
                .arg( Soprano::Node::resourceToN3(app) );

        it = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
        while (it.next()) {
            const QUrl graph = it[0].uri();
            if( fetchAppCount(m_model, graph) == 1 ) {
                discardableGraphCache.insert( app, graph );
                count++;
                emitPercent( count, totalCount );
            }
        }
    }

    kDebug() << "Fetched graph cache:" << graphCache.size();
    kDebug() << "Fetched discardable graph cache:" << discardableGraphCache.size();

    // Start migrating the data
    QString query = QString::fromLatin1("select distinct ?g where { ?g a %1. "
                                        "FILTER NOT EXISTS { ?g a %2 . } }")
                    .arg( Soprano::Node::resourceToN3(NRL::InstanceBase()),
                          Soprano::Node::resourceToN3(NRL::DiscardableInstanceBase()) );

    it = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
    while( it.next() ) {
        const QUrl graph = it[0].uri();
        if( graphCache.contains(graph) )
            continue;

        migrateGraph( graph, graphCache );
        count++;
        emitPercent( count, totalCount );
    }

    // Discardbale Data
    QString gquery = QString::fromLatin1("select distinct ?g where { ?g a %1. }")
                     .arg( Soprano::Node::resourceToN3(NRL::DiscardableInstanceBase()) );

    it = m_model->executeQuery( gquery, Soprano::Query::QueryLanguageSparqlNoInference );
    while( it.next() ) {
        const QUrl graph = it[0].uri();
        if( discardableGraphCache.contains(graph) )
            continue;

        migrateGraph( graph, discardableGraphCache );
        count++;
        emitPercent( count, totalCount );
    }

    emitPercent( totalCount, totalCount );
    emitResult();
}



void GraphMigrationJob::migrateGraph(const QUrl& graph, const QHash<QUrl, QUrl>& graphCache)
{
    QString query = QString::fromLatin1("select distinct ?app where { %1 %2 ?app . }")
                    .arg( Soprano::Node::resourceToN3(graph),
                          Soprano::Node::resourceToN3(NAO::maintainedBy()) );

    QList<QUrl> apps;
    Soprano::QueryResultIterator it = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
    while( it.next() ) {
        apps << it[0].uri();
    }

    if( apps.isEmpty() ) {
        // If empty, then push into the Nepomuk Graph
        apps << m_nepomukGraph;
    }

    foreach(const QUrl& app, apps) {
        QUrl finalGraph = graphCache.value(app);
        Q_ASSERT( !finalGraph.isEmpty() );

        copyStatements( graph, finalGraph );
        deleteGraph( graph );
    }
}


void GraphMigrationJob::copyStatements(const QUrl& from, const QUrl& to)
{
    kDebug() << from << "-->" << to;
    QString insertQ = QString::fromLatin1("insert into %1 { ?r ?p ?o. } where { graph %2 { ?r ?p ?o. } }")
                        .arg( Soprano::Node::resourceToN3(to),
                              Soprano::Node::resourceToN3(from) );
    m_model->executeQuery( insertQ, Soprano::Query::QueryLanguageSparqlNoInference );
}

void GraphMigrationJob::deleteGraph(const QUrl& g)
{
    kDebug() << g;
    m_model->removeContext( g );

    // Fetch and remove the metadata graph
    QString metaQ = QString::fromLatin1("select ?mg where { ?mg %1 %2 . }")
                    .arg( Soprano::Node::resourceToN3(NRL::coreGraphMetadataFor()),
                          Soprano::Node::resourceToN3(g) );

    Soprano::QueryResultIterator it = m_model->executeQuery( metaQ, Soprano::Query::QueryLanguageSparqlNoInference );
    while( it.next() ) {
        m_model->removeContext( it[0] );
    }

}

int GraphMigrationJob::discardableInstanceBaseGraphCount()
{
    QString query = QString::fromLatin1("select count(distinct ?g) where { ?g a %1 . }")
                    .arg( Soprano::Node::resourceToN3(NRL::DiscardableInstanceBase()) );

    Soprano::QueryResultIterator it = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
    if( it.next() )
        return it[0].literal().toInt();
    return 0;
}

int GraphMigrationJob::instanceBaseGraphCount()
{
    QString query = QString::fromLatin1("select count(distinct ?g) where { ?g a %1. "
                                        "FILTER NOT EXISTS { ?g a %2 . } }")
                    .arg( Soprano::Node::resourceToN3(NRL::InstanceBase()),
                          Soprano::Node::resourceToN3(NRL::DiscardableInstanceBase()) );

    Soprano::QueryResultIterator it = m_model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );
    if( it.next() )
        return it[0].literal().toInt();
    return 0;
}

