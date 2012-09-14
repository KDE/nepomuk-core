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

    class IndexingQueue : public QObject
    {
        Q_OBJECT
    public:
        explicit IndexingQueue(QObject* parent = 0);

        QUrl currentUrl() const;

    public slots:
        void enqueue(const QString& path);
        void suspend();
        void resume();

    signals:
        void beginIndexing(const QString& path);
        void endIndexing(const QString& path);

    protected:
        virtual void indexDir(const QString& dir) = 0;

        /**
         * This method does not need to be synchronous. The indexing operation may be started
         * and on completion, the finishedIndexing method should be called
         */
        virtual void indexFile(const QString& file) = 0;

        virtual bool shouldIndex(const QString& file) = 0;
        virtual bool shouldIndexContents(const QString& dir) = 0;

    protected slots:
        /**
         * Call this function when you have finished indexing one file, and want the indexing
         * queue to continue
         */
        void finishedIndexingFile();

    private slots:
        bool process(const QString& path);
        void processNext();

    private:
        void callForNextIteration();

        QQueue<QString> m_paths;
        QQueue< QDirIterator* > m_iterators;

        QUrl m_currentUrl;

        bool m_suspended;
        bool m_sentEvent;
    };

}

#endif // FILEINDEXER_INDEXINGQUEUE_H
