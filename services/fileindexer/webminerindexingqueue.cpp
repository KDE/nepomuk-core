/*
    <one line to give the library's name and an idea of what it does.>
    Copyright (C) 2012  Jörg Ehrichs <joerg.ehrichs@gmx.de>

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

#include "webminerindexingqueue.h"

#include "resourcemanager.h"
#include "webminerindexingjob.h"

#include <Soprano/Model>
#include <Soprano/QueryResultIterator>

#include <KDebug>
#include <QTimer>

namespace Nepomuk2 {

WebMinerIndexingQueue::WebMinerIndexingQueue(QObject *parent)
    : IndexingQueue(parent)
{
    m_fileQueue.reserve( 10 );

    QTimer::singleShot( 0, this, SLOT(init()) );
}

void WebMinerIndexingQueue::init()
{
    fillQueue();
    emit startedIndexing();

    callForNextIteration();
}

void WebMinerIndexingQueue::fillQueue()
{
    /* prevent abuse this API */
    if (m_fileQueue.size() > 0)
        return;

    QString query = QString::fromLatin1("select ?url where { ?r nie:url ?url ; kext:indexingLevel ?l ; nie:mimeType ?mime"
                                        " Filter regex(?mime , \"audio|video|pdf\", \"i\")"
                                        " FILTER(?l = 2  ). } LIMIT 10");

    Soprano::Model* model = ResourceManager::instance()->mainModel();
    Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
    while( it.next() )
        m_fileQueue.enqueue( it[0].uri() );
}

bool WebMinerIndexingQueue::isEmpty()
{
    return m_fileQueue.isEmpty();
}

void WebMinerIndexingQueue::processNextIteration()
{
    const QUrl fileUrl = m_fileQueue.dequeue();
    process( fileUrl );
}

void WebMinerIndexingQueue::process(const QUrl& url)
{
    m_currentUrl = url;

    KJob* job = new WebMinerIndexingJob( QFileInfo(url.toLocalFile()) );
    job->start();
    emit beginIndexingFile( url );
    connect( job, SIGNAL(finished(KJob*)), this, SLOT(slotFinishedIndexingFile(KJob*)) );
}

void WebMinerIndexingQueue::slotFinishedIndexingFile(KJob* job)
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

void WebMinerIndexingQueue::clear()
{
    m_currentUrl.clear();
    m_fileQueue.clear();
}

QUrl WebMinerIndexingQueue::currentUrl()
{
    return m_currentUrl;
}


}
