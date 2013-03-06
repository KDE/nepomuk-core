/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2010-2011 Sebastian Trueg <trueg@kde.org>
   Copyright (C) 2011-2012 Vishesh Handa <handa.vish@gmail.com>

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

#include "indexer.h"
#include "extractorplugin.h"
#include "extractorpluginmanager.h"
#include "simpleindexer.h"
#include "../util.h"
#include "kext.h"
#include "nie.h"

#include "storeresourcesjob.h"
#include "resourcemanager.h"

#include <Soprano/Model>
#include <Soprano/QueryResultIterator>

#include <KDebug>
#include <KJob>

#include <KService>
#include <KMimeType>
#include <KServiceTypeTrader>

#include <QtCore/QDataStream>
#include <QtCore/QDateTime>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTimer>

#include <Soprano/Vocabulary/NRL>
#include <Soprano/Vocabulary/RDF>

using namespace Soprano::Vocabulary;
using namespace Nepomuk2::Vocabulary;

Nepomuk2::Indexer::Indexer( QObject* parent )
    : QObject( parent )
{
    m_extractorManager = new ExtractorPluginManager( this );
}

Nepomuk2::Indexer::~Indexer()
{
}


bool Nepomuk2::Indexer::indexFile(const KUrl& url)
{
    QFileInfo info( url.toLocalFile() );
    if( !info.exists() ) {
        m_lastError = QString::fromLatin1("'%1' does not exist.").arg(info.filePath());
        return false;
    }

    QString query = QString::fromLatin1("select ?r ?mtype ?l where { ?r nie:url %1; nie:mimeType ?mtype ;"
                                        " kext:indexingLevel ?l . }")
                    .arg( Soprano::Node::resourceToN3( url ) );
    Soprano::Model* model = ResourceManager::instance()->mainModel();

    Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );

    QUrl uri;
    QString mimeType;
    if( it.next() ) {
        uri = it[0].uri();
        mimeType = it[1].literal().toString();
        int level = it[2].literal().toInt();

        if( level > 1 ) {
            clearIndexingData( url );
            if( !simpleIndex( url, &uri, &mimeType ) )
                return false;
        }
    }
    else {
        if( !simpleIndex( url, &uri, &mimeType ) )
            return false;
    }

    kDebug() << uri << mimeType;
    return fileIndex( uri, url, mimeType );
}


bool Nepomuk2::Indexer::clearIndexingData(const QUrl& url)
{
    kDebug() << "Starting to clear";
    KJob* job = Nepomuk2::clearIndexedData( url );
    kDebug() << "Done";

    job->exec();
    if( job->error() ) {
        m_lastError = job->errorString();
        kError() << m_lastError;

        return false;
    }

    return true;
}

bool Nepomuk2::Indexer::simpleIndex(const QUrl& url, QUrl* uri, QString* mimetype)
{
    QScopedPointer<SimpleIndexingJob> job( new SimpleIndexingJob( url ) );
    job->setAutoDelete(false);
    job->exec();

    if( job->error() ) {
        m_lastError = job->errorString();
        kError() << m_lastError;

        return false;
    }

    *uri = job->uri();
    *mimetype = job->mimeType();
    return true;
}

bool Nepomuk2::Indexer::fileIndex(const QUrl& uri, const QUrl& url, const QString& mimeType)
{
    SimpleResourceGraph graph;

    QList<ExtractorPlugin*> extractors = m_extractorManager->fetchExtractors( url, mimeType );
    foreach( ExtractorPlugin* ex, extractors ) {
        graph += ex->extract( uri, url, mimeType );
    }

    if( !graph.isEmpty() ) {
        // Do not send the full plain text content with all the other properties.
        // It is too large
        QString plainText;
        QVariantList vl = graph[uri].property( NIE::plainTextContent() );
        if( vl.size() == 1 ) {
            plainText = vl.first().toString();
            graph[uri].remove( NIE::plainTextContent() );
        }

        QHash<QUrl, QVariant> additionalMetadata;
        additionalMetadata.insert( RDF::type(), NRL::DiscardableInstanceBase() );

        // we do not have an event loop - thus, we need to delete the job ourselves
        QScopedPointer<StoreResourcesJob> job( Nepomuk2::storeResources( graph, IdentifyNew,
                                                                         NoStoreResourcesFlags, additionalMetadata ) );
        job->setAutoDelete(false);
        job->exec();
        if( job->error() ) {
            m_lastError = job->errorString();
            kError() << "SimpleIndexerError: " << m_lastError;
            return false;
        }

        if( plainText.length() ) {
            kDebug() << "Saving plain text content";
            setNiePlainTextContent( uri, plainText );
        }
    }

    // Update the indexing level even if no data has changed
    kDebug() << "Updating indexing level";
    updateIndexingLevel( uri, 2 );

    return true;
}



Nepomuk2::SimpleResourceGraph Nepomuk2::Indexer::indexFileGraph(const QUrl& url)
{
    SimpleResource res;

    QString mimeType = KMimeType::findByUrl( url )->name();
    res.addProperty(NIE::mimeType(), mimeType);
    res.addProperty(NIE::url(), url);

    SimpleResourceGraph graph;
    graph << res;

    QList<ExtractorPlugin*> extractors = m_extractorManager->fetchExtractors( url, mimeType );
    foreach( ExtractorPlugin* ex, extractors ) {
        graph += ex->extract( res.uri(), url, mimeType );
    }

    kDebug() << graph;
    return graph;
}


QString Nepomuk2::Indexer::lastError() const
{
    return m_lastError;
}

//
// We don't really care if the indexing level is in the incorrect graph
//
void Nepomuk2::Indexer::updateIndexingLevel(const QUrl& uri, int level)
{
    QString uriN3 = Soprano::Node::resourceToN3( uri );

    QString query = QString::fromLatin1("select ?g ?l where { graph ?g { %1 kext:indexingLevel ?l . } }")
                    .arg ( uriN3 );
    Soprano::Model* model = ResourceManager::instance()->mainModel();
    Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );

    QUrl graph;
    Soprano::Node prevLevel;
    if( it.next() ) {
        graph = it[0].uri();
        prevLevel = it[1];
        it.close();
    }

    if( !graph.isEmpty() ) {
        QString graphN3 = Soprano::Node::resourceToN3( graph );
        QString removeCommand = QString::fromLatin1("sparql delete { graph %1 { %2 kext:indexingLevel %3 . } }")
                                .arg( graphN3, uriN3, prevLevel.toN3() );
        model->executeQuery( removeCommand, Soprano::Query::QueryLanguageUser, QLatin1String("sql") );

        QString insertCommand = QString::fromLatin1("sparql insert { graph %1 { %2 kext:indexingLevel %3 . } }")
                                .arg( graphN3, uriN3, Soprano::Node::literalToN3(level) );
        model->executeQuery( insertCommand, Soprano::Query::QueryLanguageUser, QLatin1String("sql") );
    }
    // Practically, this should never happen, but still
    else {
        QScopedPointer<KJob> job( Nepomuk2::setProperty( QList<QUrl>() << uri, KExt::indexingLevel(),
                                                                QVariantList() << QVariant(level) ) );
        job->setAutoDelete(false);
        job->exec();
    }
}

void Nepomuk2::Indexer::setNiePlainTextContent(const QUrl& uri, QString& plainText)
{
    // This number has been experimentally chosen. Virtuoso cannot handle more than this
    static const int maxSize = 3 * 1024 * 1024;
    if( plainText.size() > maxSize )  {
        kWarning() << "Trimming plain text content from " << plainText.size() << " to " << maxSize;
        plainText.resize( maxSize );
    }

    QString uriN3 = Soprano::Node::resourceToN3( uri );

    // FIXME: Do not use the kext:indexingLevel graph.
    QString query = QString::fromLatin1("select ?g where { graph ?g { %1 kext:indexingLevel ?l . } }")
                    .arg ( uriN3 );
    Soprano::Model* model = ResourceManager::instance()->mainModel();
    Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparqlNoInference );

    QUrl graph;
    if( it.next() ) {
        graph = it[0].uri();
        it.close();
    }

    if( !graph.isEmpty() ) {
        QString graphN3 = Soprano::Node::resourceToN3( graph );
        QString insertCommand = QString::fromLatin1("sparql insert { graph %1 { %2 nie:plainTextContent %3 . } }")
                                .arg( graphN3, uriN3, Soprano::Node::literalToN3(plainText) );

        model->executeQuery( insertCommand, Soprano::Query::QueryLanguageUser, QLatin1String("sql") );
    }
}


#include "indexer.moc"
