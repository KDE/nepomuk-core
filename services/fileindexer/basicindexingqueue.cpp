/*
    <one line to give the library's name and an idea of what it does.>
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


#include "basicindexingqueue.h"
#include "fileindexerconfig.h"
#include "util.h"
#include "indexer/simpleindexer.h"

#include "resourcemanager.h"

#include <Soprano/Model>
#include <Soprano/QueryResultIterator>

#include <KDebug>
#include <QtCore/QDateTime>

namespace Nepomuk2 {

BasicIndexingQueue::BasicIndexingQueue(QObject* parent): IndexingQueue(parent)
{

}

void BasicIndexingQueue::clear()
{
    m_currentUrl.clear();
    m_currentFlags = NoUpdateFlags;
    m_paths.clear();

    typedef QPair<QDirIterator*, UpdateDirFlags> DirPair;
    foreach( const DirPair& pair, m_iterators )
        delete pair.first;

    m_iterators.clear();
}

QUrl BasicIndexingQueue::currentUrl() const
{
    return m_currentUrl;
}

UpdateDirFlags BasicIndexingQueue::currentFlags() const
{
    return m_currentFlags;
}


bool BasicIndexingQueue::isEmpty()
{
    return m_iterators.isEmpty() && m_paths.isEmpty();
}

void BasicIndexingQueue::enqueue(const QString& path)
{
    UpdateDirFlags flags;
    flags |= UpdateRecursive;

    enqueue( path, flags );
}

void BasicIndexingQueue::enqueue(const QString& path, UpdateDirFlags flags)
{
    m_paths.enqueue( qMakePair( path, flags ) );
    callForNextIteration();
}

bool BasicIndexingQueue::processNextIteration()
{
    bool startedIndexing = false;

    // First process all the iterators and then the paths
    if( !m_iterators.isEmpty() ) {
        QPair< QDirIterator*, UpdateDirFlags > pair = m_iterators.first();
        QDirIterator* dirIt = pair.first;

        if( dirIt->hasNext() ) {
            startedIndexing = process( dirIt->next(), pair.second );
        }
        else {
            delete m_iterators.dequeue().first;
        }
    }

    else if( !m_paths.isEmpty() ) {
        QPair< QString, UpdateDirFlags > pair = m_paths.dequeue();
        startedIndexing = process( pair.first, pair.second );
    }

    return startedIndexing;
}


bool BasicIndexingQueue::process(const QString& path, UpdateDirFlags flags)
{
    bool startedIndexing = false;

    bool forced = flags & ForceUpdate;
    bool recursive = flags & UpdateRecursive;
    bool indexingRequired = shouldIndex( path );

    QFileInfo info( path );
    if( info.isDir() ) {
        if( forced || indexingRequired ) {
            m_currentUrl = QUrl::fromLocalFile( path );
            m_currentFlags = flags;

            startedIndexing = true;
            index( path );
        }

        if( recursive && shouldIndexContents(path) ) {
            QDir::Filters dirFilter = QDir::NoDotAndDotDot|QDir::Readable|QDir::Files|QDir::Dirs;

            QPair<QDirIterator*, UpdateDirFlags> pair = qMakePair( new QDirIterator( path, dirFilter ), flags );
            m_iterators.enqueue( pair );
        }
    }
    else if( info.isFile() && (forced || indexingRequired) ) {
        m_currentUrl = QUrl::fromLocalFile( path );
        m_currentFlags = flags;

        startedIndexing = true;
        index( path );
    }

    return startedIndexing;
}

bool BasicIndexingQueue::shouldIndex(const QString& path)
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
        kDebug() << path;
        return true;
    }

    return false;
}

bool BasicIndexingQueue::shouldIndexContents(const QString& dir)
{
    return FileIndexerConfig::self()->shouldFolderBeIndexed( dir );
}

void BasicIndexingQueue::index(const QString& path)
{
    kDebug() << path;
    const QUrl fileUrl = QUrl::fromLocalFile( path );
    emit beginIndexingFile( fileUrl );

    KJob* job = clearIndexedData( fileUrl );
    connect( job, SIGNAL(finished(KJob*)), this, SLOT(slotClearIndexedDataFinished(KJob*)) );
}

void BasicIndexingQueue::slotClearIndexedDataFinished(KJob* job)
{
    if( job->error() ) {
        kDebug() << job->errorString();
    }

    SimpleIndexingJob* indexingJob = new SimpleIndexingJob( currentUrl() );
    indexingJob->start();

    connect( indexingJob, SIGNAL(finished(KJob*)), this, SLOT(slotIndexingFinished(KJob*)) );
}

void BasicIndexingQueue::slotIndexingFinished(KJob* job)
{
    if( job->error() ) {
        kDebug() << job->errorString();
    }

    QUrl url = m_currentUrl;
    m_currentUrl.clear();
    m_currentFlags = NoUpdateFlags;

    emit endIndexingFile( url );

    // Continue the queue
    finishIndexingFile();
}


}