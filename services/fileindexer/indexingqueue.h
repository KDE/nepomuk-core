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


#ifndef FILEINDEXER_INDEXINGQUEUE_H
#define FILEINDEXER_INDEXINGQUEUE_H

#include <QtCore/QObject>
#include <QtCore/QQueue>
#include <QtCore/QDirIterator>
#include <QtCore/QUrl>

namespace Nepomuk2 {

    /**
     * An abstract class representing the queue. All indexing queues
     * should be derived from this queue.
     *
     */
    class IndexingQueue : public QObject
    {
        Q_OBJECT
    public:
        explicit IndexingQueue(QObject* parent = 0);

        virtual bool isEmpty() = 0;

    public slots:
        void suspend();
        void resume();

    signals:
        void startedIndexing();
        void finishedIndexing();

    protected:
        /**
         * Process the next iteration in your queue. Once you are done
         * processing the file call finishIndexingFile so that the queue
         * can continue processing and call this function when it is
         * ready for the next iteration.
         *
         * \sa finishIndexingFile
         */
        virtual void processNextIteration() = 0;

    protected slots:
        /**
         * Call this function when you have finished processing the
         * iteration from processNextIteration.
         *
         * \sa processNextIteration
         */
        void finishIteration();

        void callForNextIteration();

    private slots:
        void processNext();

    private:
        bool m_suspended;
        bool m_sentEvent;
    };

}


#endif // FILEINDEXER_INDEXINGQUEUE_H
