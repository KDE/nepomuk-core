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
#include "simpleindexer.h"
#include "../util.h"
#include "kext.h"

#include "storeresourcesjob.h"
#include "resourcemanager.h"

#include <Soprano/Model>
#include <Soprano/QueryResultIterator>

#include <KDebug>
#include <KJob>

#include <KService>
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
    // Get all the plugins
    KService::List plugins = KServiceTypeTrader::self()->query( "NepomukFileExtractor" );

    KService::List::const_iterator it;
    for( it = plugins.constBegin(); it != plugins.constEnd(); it++ ) {
        KService::Ptr service = *it;

        QString error;
        Nepomuk2::ExtractorPlugin* ex = service->createInstance<Nepomuk2::ExtractorPlugin>( this, QVariantList(), &error );
        if( !ex ) {
            kError() << "Could not create Extractor: " << service->library();
            kError() << error;
            continue;
        }

        foreach(const QString& mime, ex->mimetypes())
            m_extractors.insertMulti( mime, ex );
    }
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

    QString query = QString::fromLatin1("select ?r ?mtype where { ?r nie:url %1; nie:mimeType ?mtype . }")
                    .arg( Soprano::Node::resourceToN3( url ) );
    Soprano::Model* model = ResourceManager::instance()->mainModel();

    Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );

    QUrl uri;
    QString mimeType;
    if( it.next() ) {
        uri = it[0].uri();
        mimeType = it[1].literal().toString();
    }
    else {
        SimpleIndexingJob* indexingJob = new SimpleIndexingJob( url );
        indexingJob->exec();
        uri = indexingJob->uri();
        mimeType = indexingJob->mimeType();
    }

    kDebug() << uri << mimeType;
    SimpleResourceGraph graph;

    QList<ExtractorPlugin*> extractors = m_extractors.values( mimeType );
    foreach( ExtractorPlugin* ex, extractors ) {
        graph += ex->extract( uri, url, mimeType );
    }

    if( !graph.isEmpty() ) {
        QHash<QUrl, QVariant> additionalMetadata;
        additionalMetadata.insert( RDF::type(), NRL::DiscardableInstanceBase() );

        // we do not have an event loop - thus, we need to delete the job ourselves
        QScopedPointer<StoreResourcesJob> job( Nepomuk2::storeResources( graph, IdentifyNew,
                                                                         NoStoreResourcesFlags, additionalMetadata ) );
        job->setAutoDelete(false);
        job->exec();
        if( job->error() ) {
            m_lastError = job->errorString();
            kError() << "SimpleIndexerError: " << job->errorString();
            return false;
        }
    }

    // Update the indexing level even if no data has changed
    kDebug() << "Updating indexing level";
    updateIndexingLevel( uri, 2 );

    return true;
}

bool Nepomuk2::Indexer::indexFileDebug(const KUrl& url)
{
    QFileInfo info( url.toLocalFile() );
    if( !info.exists() ) {
        m_lastError = QString::fromLatin1("'%1' does not exist.").arg(info.filePath());
        return false;
    }

    kDebug() << "Starting to clear";
    KJob* job = Nepomuk2::clearIndexedData( url );
    kDebug() << "Done";

    job->exec();
    if( job->error() ) {
        kError() << job->errorString();
        m_lastError = job->errorString();

        return false;
    }

    kDebug() << "Starting SimpleIndexer";
    SimpleIndexingJob* indexingJob = new SimpleIndexingJob( url );
    indexingJob->exec();

    bool status = indexingJob->error();
    kDebug() << "Saving data " << indexingJob->errorString();

    if( !status ) {
        QString mimeType = indexingJob->mimeType();
        QUrl uri = indexingJob->uri();

        SimpleResourceGraph graph;

        QList<ExtractorPlugin*> extractors = m_extractors.values( mimeType );
        foreach( ExtractorPlugin* ex, extractors ) {
            graph += ex->extract( uri, url, mimeType );
        }

        if( !graph.isEmpty() ) {
            QHash<QUrl, QVariant> additionalMetadata;
            additionalMetadata.insert( RDF::type(), NRL::DiscardableInstanceBase() );

            // we do not have an event loop - thus, we need to delete the job ourselves
            kDebug() << "Saving proper";
            // HACK: Use OverwriteProperties for the setting the indexingLevel
            QScopedPointer<StoreResourcesJob> job( Nepomuk2::storeResources( graph, IdentifyNew,
                                                        NoStoreResourcesFlags, additionalMetadata ) );
            job->setAutoDelete(false);
            job->exec();
            if( job->error() ) {
                m_lastError = job->errorString();
                kError() << "SimpleIndexerError: " << job->errorString();
                return false;
            }
        }

        kDebug() << "Updating the indexing level";
        updateIndexingLevel( uri, 2 );
        kDebug() << "Done";
    }

    return status;
}

QString Nepomuk2::Indexer::lastError() const
{
    return m_lastError;
}

void Nepomuk2::Indexer::updateIndexingLevel(const QUrl& uri, int level)
{
    //FIXME: Maybe this could be done via the manual Soprano Model?
    QScopedPointer<KJob> job( Nepomuk2::setProperty( QList<QUrl>() << uri, KExt::indexingLevel(),
                                                            QVariantList() << QVariant(level) ) );
    job->setAutoDelete(false);
    job->exec();
}



#include "indexer.moc"
