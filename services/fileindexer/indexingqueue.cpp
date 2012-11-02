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
    if( m_suspended || isEmpty() ) {
        m_sentEvent = false;
        return;
    }

    bool startedIndexing = processNextIteration();

    if( !startedIndexing ) {
        m_sentEvent = false;
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
    // If already called callForNextIteration
    if( m_sentEvent )
        return;

    if( isEmpty() ) {
        emit finishedIndexing();
        return;
    }

    if( !m_suspended ) {
        QTimer::singleShot( 0, this, SLOT(processNext()) );
        m_sentEvent = true;
    }
}

void IndexingQueue::finishIndexingFile()
{
    m_sentEvent = false;
    callForNextIteration();
}



}
