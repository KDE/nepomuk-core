/*
    <one line to give the library's name and an idea of what it does.>
    Copyright (c) 2008-2010 Sebastian Trueg <trueg@kde.org>
    Copyright (C) 2012  Vishesh Handa <me@vhanda.in>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/


#include "queues.h"
#include "util.h"
#include "fileindexerconfig.h"
#include "indexer/simpleindexer.h"
#include "resourcemanager.h"
#include "nepomukindexer.h"

#include <Soprano/Model>
#include <Soprano/QueryResultIterator>

#include <QDateTime>
#include <KDebug>

namespace Nepomuk2 {

FastIndexingQueue::FastIndexingQueue(QObject* parent)
: IndexingQueue(parent)
{

}

void FastIndexingQueue::index(const QString& path)
{
    const QUrl fileUrl = QUrl::fromLocalFile( path );

    KJob* job = clearIndexedData( fileUrl );
    connect( job, SIGNAL(finished(KJob*)), this, SLOT(slotClearIndexedDataFinished(KJob*)) );
}

void FastIndexingQueue::slotClearIndexedDataFinished(KJob* job)
{
    if( job->error() ) {
        kDebug() << job->errorString();
    }

    SimpleIndexingJob* indexingJob = new SimpleIndexingJob( currentUrl() );
    connect( indexingJob, SIGNAL(finished(KJob*)), this, SLOT(slotIndexingFinished(KJob*)) );
}

void FastIndexingQueue::slotIndexingFinished(KJob* job)
{
    if( job->error() ) {
        kDebug() << job->errorString();
    }

    finishedIndexingFile();
}


bool FastIndexingQueue::shouldIndex(const QString& path)
{
    bool shouldIndexFile = FileIndexerConfig::self()->shouldFileBeIndexed( path );
    if( !shouldIndexFile )
        return false;

    QFileInfo fileInfo( path );

    // check if this file is new by looking it up in the store
    Soprano::Model* model = ResourceManager::instance()->mainModel();
    QString query = QString::fromLatin1("ask where { ?r nie:url %1 ; nie:lastModified ?dt . FILTER(?dt=%2) .}")
                    .arg( Soprano::Node::resourceToN3( QUrl::fromLocalFile(path) ),
                          Soprano::Node::literalToN3( Soprano::LiteralValue(fileInfo.lastModified()) ) );

    bool needToIndex = !model->executeQuery( query, Soprano::Query::QueryLanguageSparql ).boolValue();

    if( needToIndex ) {
        //kDebug() << path;
        return true;
    }

    return false;
}

bool FastIndexingQueue::shouldIndexContents(const QString& dir)
{
    return FileIndexerConfig::self()->shouldFolderBeIndexed( dir );
}


//
// Slow Indexer
//

SlowIndexingQueue::SlowIndexingQueue(QObject* parent)
: IndexingQueue(parent)
{
}

void SlowIndexingQueue::indexFile(const QString& file)
{
    KJob* job = new Indexer( QFileInfo(file) );
    job->start();
    connect( job, SIGNAL(finished(KJob*)), this, SLOT(finishedIndexingFile()) );
}

void SlowIndexingQueue::indexDir(const QString& )
{
    finishedIndexingFile();
}

bool SlowIndexingQueue::shouldIndex(const QString& file)
{
    QString query = QString::fromLatin1("ask where { ?r nie:url %1 ; kext:indexingLevel ?l . "
                                        " FILTER( ?l < 2 ) . }")
                    .arg( Soprano::Node::resourceToN3( QUrl::fromLocalFile(file) ) );

    Soprano::Model* model = ResourceManager::instance()->mainModel();
    return model->executeQuery( query, Soprano::Query::QueryLanguageSparql ).boolValue();
}

bool SlowIndexingQueue::shouldIndexContents(const QString& path)
{
    return FileIndexerConfig::self()->shouldFolderBeIndexed( path );
}



}