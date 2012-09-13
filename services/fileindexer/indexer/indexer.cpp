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
#include "extractor.h"
#include "simpleindexer.h"
#include "../util.h"

#include "storeresourcesjob.h"

#include <KDebug>
#include <KJob>

#include <KService>
#include <KServiceTypeTrader>

#include <QtCore/QDataStream>
#include <QtCore/QDateTime>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QTimer>

#include <Soprano/Vocabulary/NRL>
#include <Soprano/Vocabulary/RDF>

using namespace Soprano::Vocabulary;

Nepomuk2::Indexer::Indexer( QObject* parent )
    : QObject( parent )
{
    // Get all the plugins
    KService::List plugins = KServiceTypeTrader::self()->query( "NepomukFileExtractor" );

    KService::List::const_iterator it;
    for( it = plugins.constBegin(); it != plugins.constEnd(); it++ ) {
        KService::Ptr service = *it;

        QString error;
        Nepomuk2::Extractor* ex = service->createInstance<Nepomuk2::Extractor>( this, QVariantList(), &error );
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


bool Nepomuk2::Indexer::indexFile( const KUrl& url, const KUrl resUri, uint mtime )
{
    return indexFile( QFileInfo( url.toLocalFile() ), resUri, mtime );
}


bool Nepomuk2::Indexer::indexFile( const QFileInfo& info, const KUrl resUri, uint mtime )
{
    if( !info.exists() ) {
        m_lastError = QString::fromLatin1("'%1' does not exist.").arg(info.filePath());
        return false;
    }

    QUrl url = QUrl::fromLocalFile( info.filePath() );
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
    SimpleIndexer indexer( url );
    bool status = indexer.save();
    kDebug() << "Saving data";

    if( status ) {
        QString mimeType = indexer.mimeType();
        QUrl uri = indexer.uri();

        SimpleResourceGraph graph;

        QList<Extractor*> extractors = m_extractors.values( mimeType );
        foreach( Extractor* ex, extractors ) {
            graph += ex->extract( uri, url );
        }

        if( !graph.isEmpty() ) {
            QHash<QUrl, QVariant> additionalMetadata;
            additionalMetadata.insert( RDF::type(), NRL::DiscardableInstanceBase() );

            // we do not have an event loop - thus, we need to delete the job ourselves
            kDebug() << "Saving proper";
            QScopedPointer<StoreResourcesJob> job( Nepomuk2::storeResources( graph, IdentifyNone,
                                                        NoStoreResourcesFlags, additionalMetadata ) );
            job->setAutoDelete(false);
            job->exec();
            if( job->error() ) {
                kError() << "SimpleIndexerError: " << job->errorString();
                return false;
            }
            kDebug() << "Done";
        }
    }

    return status;
}

bool Nepomuk2::Indexer::indexStdin(const KUrl resUri, uint mtime)
{
    return false;
}

QString Nepomuk2::Indexer::lastError() const
{
    return m_lastError;
}



#include "indexer.moc"
