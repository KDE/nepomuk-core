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


#include "fileindexingqueue.h"
#include "resourcemanager.h"
#include "fileindexingjob.h"
#include "fileindexerconfig.h"

#include <Soprano/Model>
#include <Soprano/QueryResultIterator>

#include <KDebug>
#include <QTimer>

namespace Nepomuk2 {

FileIndexingQueue::FileIndexingQueue(QObject* parent): IndexingQueue(parent)
{
    m_fileQueue.reserve( 10 );

    FileIndexerConfig* config = FileIndexerConfig::self();
    connect( config, SIGNAL(configChanged()), this, SLOT(slotConfigChanged()) );
}

void FileIndexingQueue::start()
{
    fillQueue();
    emit startedIndexing();

    callForNextIteration();
}

void FileIndexingQueue::fillQueue()
{
    /* prevent abuse this API */
    if (m_fileQueue.size() > 0)
        return;

    QString query = QString::fromLatin1("select ?url where { ?r nie:url ?url ; kext:indexingLevel ?l "
                                        " FILTER(?l < 2 ). } LIMIT 10");

    Soprano::Model* model = ResourceManager::instance()->mainModel();
    Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
    while( it.next() )
        m_fileQueue.enqueue( it[0].uri() );
}

bool FileIndexingQueue::isEmpty()
{
    return m_fileQueue.isEmpty();
}

void FileIndexingQueue::processNextIteration()
{
    const QUrl fileUrl = m_fileQueue.dequeue();
    process( fileUrl );
}

void FileIndexingQueue::process(const QUrl& url)
{
    m_currentUrl = url;

    KJob* job = new FileIndexingJob( QFileInfo(url.toLocalFile()) );
    job->start();
    emit beginIndexingFile( url );
    connect( job, SIGNAL(finished(KJob*)), this, SLOT(slotFinishedIndexingFile(KJob*)) );
}

void FileIndexingQueue::slotFinishedIndexingFile(KJob* job)
{
    if( job->error() ) {
        kDebug() << job->errorString();
    }

    QUrl url = m_currentUrl;
    m_currentUrl.clear();
    emit endIndexingFile( url );
    if( m_fileQueue.isEmpty() ) {
        fillQueue();
    }
    finishIteration();
}

void FileIndexingQueue::clear()
{
    m_currentUrl.clear();
    m_fileQueue.clear();
}

QUrl FileIndexingQueue::currentUrl()
{
    return m_currentUrl;
}

void FileIndexingQueue::slotConfigChanged()
{
    m_fileQueue.clear();
    fillQueue();
}


}
