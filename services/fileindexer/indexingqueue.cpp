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


#include "indexingqueue.h"

#include <QtCore/QTimer>
#include <KDebug>

namespace Nepomuk2 {


IndexingQueue::IndexingQueue(QObject* parent): QObject(parent)
{
    m_sentEvent = false;
    m_suspended = false;
}

void IndexingQueue::processNext()
{
    if( m_suspended )
        return;

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


    if( !startedIndexing ) {
        m_sentEvent = false;
        callForNextIteration();
    }
}

bool IndexingQueue::process(const QString& path, Nepomuk2::UpdateDirFlags flags)
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

            emit beginIndexing( path );

            startedIndexing = true;
            indexDir( path );
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

        emit beginIndexing( path );

        startedIndexing = true;
        indexFile( path );
    }

    return startedIndexing;
}

void IndexingQueue::enqueue(const QString& path)
{
    UpdateDirFlags flags;
    flags |= UpdateRecursive;

    enqueue( path, flags );
}

void IndexingQueue::enqueue(const QString& path, Nepomuk2::UpdateDirFlags flags)
{
    m_paths.enqueue( qMakePair( path, flags ) );

    if( !m_suspended ) {
        callForNextIteration();
    }
}


void IndexingQueue::resume()
{
    m_suspended = false;
    callForNextIteration();
}

void IndexingQueue::suspend()
{
    m_suspended = true;
}

void IndexingQueue::callForNextIteration()
{
    bool queuesEmpty = m_iterators.isEmpty() && m_paths.isEmpty();

    if( !m_sentEvent && !queuesEmpty ) {
        QTimer::singleShot( 0, this, SLOT(processNext()) );
        m_sentEvent = true;
    }
}

void IndexingQueue::finishedIndexingFile()
{
    m_sentEvent = false;
    callForNextIteration();

    QUrl url = m_currentUrl;
    m_currentUrl.clear();
    m_currentFlags = NoUpdateFlags;

    emit endIndexing( url.toLocalFile() );
}

QUrl IndexingQueue::currentUrl() const
{
    return m_currentUrl;
}

void IndexingQueue::clear()
{
    m_currentUrl.clear();
    m_currentFlags = NoUpdateFlags;
    m_paths.clear();

    typedef QPair<QDirIterator*, UpdateDirFlags> DirPair;
    foreach( const DirPair& pair, m_iterators )
        delete pair.first;

    m_iterators.clear();
}


}
