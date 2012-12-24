/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2010 Sebastian Trueg <trueg@kde.org>
   Copyright (C) 2011 Vishesh Handa <handa.vish@gmail.com>

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

#ifndef _NEPOMUK_STRIG_INDEXER_H_
#define _NEPOMUK_STRIG_INDEXER_H_

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <KUrl>

namespace Nepomuk2 {

    class Resource;
    class ExtractorPluginManager;
    class SimpleResourceGraph;

    class Indexer : public QObject
    {
        Q_OBJECT

    public:
        /**
         * Create a new indexer.
         */
        Indexer( QObject* parent = 0 );

        /**
         * Destructor
         */
        ~Indexer();

        /**
         * Index a single local file or folder (files in a folder will
         * not be indexed recursively).
         *
         * This method just calls the appropriate file indexing plugins and then saves
         * the graph.
         */
        bool indexFile( const KUrl& url );

        /**
         * Extracts the SimpleResourceGraph of the local file or folder and returns it.
         * This is used in the rare cases when one does not wish to index the file, one
         * just wants to see the indexed information
         */
        Nepomuk2::SimpleResourceGraph indexFileGraph( const QUrl& url );

        QString lastError() const;

    private:
        QString m_lastError;
        ExtractorPluginManager* m_extractorManager;

        void updateIndexingLevel( const QUrl& uri, int level );

        /**
         * Sets the nie:plainTextContent as \p plainText. The parameter \p plainText
         * might be modified in the process, if it is too large.
         */
        void setNiePlainTextContent( const QUrl& uri, QString& plainText );

        bool clearIndexingData( const QUrl& url );
        bool simpleIndex( const QUrl& url, QUrl* uri, QString* mimetype );
        bool fileIndex( const QUrl& uri, const QUrl& url, const QString& mimeType );
    };
}

#endif
