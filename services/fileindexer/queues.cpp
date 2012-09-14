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

void FastIndexingQueue::indexDir(const QString& dir)
{
    KJob* job = clearIndexedData( QUrl::fromLocalFile(dir) );
    job->exec();

    SimpleIndexer indexer( QUrl::fromLocalFile(dir) );
    indexer.save();
}

void FastIndexingQueue::indexFile(const QString& file)
{
    KJob* job = clearIndexedData( QUrl::fromLocalFile(file) );
    job->exec();

    SimpleIndexer indexer( QUrl::fromLocalFile(file) );
    indexer.save();
}

bool FastIndexingQueue::shouldIndex(const QString& path)
{
    bool indexFile = FileIndexerConfig::self()->shouldFileBeIndexed( path );

    // check if this file is new by looking it up in the store
    Soprano::Model* model = ResourceManager::instance()->mainModel();
    QString query = QString::fromLatin1("select ?dt where { ?r nie:url %1 ; nie:lastModified ?dt . } LIMIT 1")
                    .arg( Soprano::Node::resourceToN3(QUrl::fromLocalFile(path)) );

    Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
    QDateTime lastModified;
    if( it.next() )
        lastModified = it["dt"].literal().toDateTime();

    bool newFile = lastModified.isNull();

    if ( newFile && indexFile )
        kDebug() << "NEW    :" << path;

    // do we need to update? Did the file change?
    QFileInfo fileInfo( path );
    bool fileChanged = !newFile && fileInfo.lastModified() != lastModified;

    //TODO: At some point make these "NEW", "CHANGED", and "FORCED" strings public
    //      so that they can be used to create a better status message.

    if ( fileChanged )
        kDebug() << "CHANGED:" << path << fileInfo.lastModified() << lastModified;
    /*
    else if( forceUpdate )
        kDebug() << "UPDATE FORCED:" << path;*/

    if ( indexFile && ( newFile || fileChanged ) )
        return true;

    return false;
}

bool FastIndexingQueue::shouldIndexContents(const QString& dir)
{
    // FIXME: Check the mtime as well
    return FileIndexerConfig::self()->shouldFolderBeIndexed( dir );
}


SlowIndexingQueue::SlowIndexingQueue(QObject* parent)
: IndexingQueue(parent)
{

}

void SlowIndexingQueue::indexFile(const QString& file)
{
    KJob* job = new Indexer( QFileInfo(file) );
    job->exec();

    if( job->error() ) {
        kError() << "Indexing Error: " << job->errorString();
    }
}



}