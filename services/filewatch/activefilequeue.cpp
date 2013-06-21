/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2011 Sebastian Trueg <trueg@kde.org>
   Copyright (C) 2013 Vishesh Handa <me@vhanda.in>

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

#include "activefilequeue.h"

#include <QtCore/QQueue>
#include <QtCore/QTimer>


namespace {
    class Entry {
    public:
        Entry(const QString& url, int c);
        bool operator==(const Entry& other) const;

        /// The file url
        QString url;
        /// The seconds left in this entry
        int cnt;
    };

    Entry::Entry(const QString &u, int c)
        : url(u),
          cnt(c)
    {
    }

    bool Entry::operator==(const Entry &other) const
    {
        // we ignore the counter since we need this for the search in queueUrl only
        return url == other.url;
    }
}


Q_DECLARE_TYPEINFO(Entry, Q_MOVABLE_TYPE);


class ActiveFileQueue::Private
{
public:
    QQueue<Entry> m_queue;
    int m_queueTimeout;

    QTimer m_queueTimer;

    /// Contains a set of all the entries for which we emitted the urlTimeout sigal
    QList<Entry> m_emittedEntries;
    int m_emittedTimeout;

};


ActiveFileQueue::ActiveFileQueue(QObject *parent)
    : QObject(parent),
      d(new Private())
{
    // we default to 5 seconds
    d->m_queueTimeout = 5;
    d->m_emittedTimeout = 5;

    // setup the timer
    connect(&d->m_queueTimer, SIGNAL(timeout()),
            this, SLOT(slotTimer()));

    // we check in 1 sec intervals
    d->m_queueTimer.setInterval(1000);
}

ActiveFileQueue::~ActiveFileQueue()
{
    delete d;
}

void ActiveFileQueue::enqueueUrl(const QString& url)
{
    Entry defaultEntry(url, d->m_queueTimeout);

    // If the url is already in the queue update its timestamp
    QQueue<Entry>::iterator it = qFind(d->m_queue.begin(), d->m_queue.end(), defaultEntry);
    if(it != d->m_queue.end()) {
        it->cnt = d->m_queueTimeout;
    }
    else {
        // We check if we just emitted the url, if so we move it to the normal queue
        QList<Entry>::iterator iter = qFind(d->m_emittedEntries.begin(), d->m_emittedEntries.end(), defaultEntry);
        if( iter != d->m_emittedEntries.end() ) {
            d->m_queue.enqueue( defaultEntry );
            d->m_emittedEntries.erase( iter );
        }
        else {
            // It's not in any of the queues
            emit urlTimeout( url );
            defaultEntry.cnt = d->m_emittedTimeout;
            d->m_emittedEntries.append( defaultEntry );
        }
    }

    // make sure the timer is running
    if(!d->m_queueTimer.isActive()) {
        d->m_queueTimer.start();
    }
}

void ActiveFileQueue::setTimeout(int seconds)
{
    d->m_queueTimeout = seconds;
}

void ActiveFileQueue::setWaitTimeout(int seconds)
{
    d->m_emittedTimeout = seconds;
}

void ActiveFileQueue::slotTimer()
{
    // we run through the queue, decrease each counter and emit each entry which has a count of 0
    QMutableListIterator<Entry> it( d->m_queue );
    while( it.hasNext() ) {
        Entry& entry = it.next();
        entry.cnt--;
        if( entry.cnt <= 0 ) {
            // Insert into the emitted queue
            entry.cnt = d->m_emittedTimeout;
            d->m_emittedEntries.append( entry );

            emit urlTimeout( entry.url );
            it.remove();
        }
    }

    // Run through all the emitted entires and remove them
    it = QMutableListIterator<Entry>( d->m_emittedEntries );
    while( it.hasNext() ) {
        Entry& entry = it.next();
        entry.cnt--;
        if( entry.cnt <= 0 ) {
            it.remove();
        }
    }

    // stop the timer in case we have nothing left to do
    if(d->m_queue.isEmpty() && d->m_emittedEntries.isEmpty()) {
        d->m_queueTimer.stop();
    }
}

#include "activefilequeue.moc"
