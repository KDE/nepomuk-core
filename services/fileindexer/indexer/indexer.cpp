/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2010-2011 Sebastian Trueg <trueg@kde.org>
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

#include "indexer.h"
#include "simpleindexer.h"
#include "../util.h"

#include <KDebug>
#include <KJob>

#include <QtCore/QDataStream>
#include <QtCore/QDateTime>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>


//The final option defaults to empty, for backward compatibility
Nepomuk2::Indexer::Indexer( QObject* parent )
    : QObject( parent )
{
}

Nepomuk2::Indexer::~Indexer()
{
}


bool Nepomuk2::Indexer::indexFile( const KUrl& url, const KUrl resUri, uint mtime )
{
    return indexFile( QFileInfo( url.toLocalFile() ), resUri, mtime );
}


bool Nepomuk2::Indexer::indexFile( const QFileInfo& info, const KUrl resUri, uint mtime )
{
    if( !info.exists() ) {
        m_lastError = QString::fromLatin1("'%1' does not exist.").arg(info.filePath());
        return false;
    }

    KJob* job = Nepomuk2::clearIndexedData( info.filePath() );
    job->exec();
    if( job->error() ) {
        kError() << job->errorString();
        m_lastError = job->errorString();

        return false;
    }

    SimpleIndexer indexer( QUrl::fromLocalFile(info.filePath()) );
    return indexer.save();
}

bool Nepomuk2::Indexer::indexStdin(const KUrl resUri, uint mtime)
{
    return false;
}

QString Nepomuk2::Indexer::lastError() const
{
    return m_lastError;
}



#include "indexer.moc"
