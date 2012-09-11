/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2012 Vishesh Handa <me@vhanda.in>

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

#include "simpleindexer.h"
#include "datamanagement.h"
#include "storeresourcesjob.h"

#include "nfo.h"
#include "nie.h"
#include "kext.h"

#include <Soprano/Vocabulary/NRL>
#include <Soprano/Vocabulary/RDF>

#include <QtCore/QFileInfo>
#include <QtCore/QDateTime>

#include <KMimeType>
#include <KDebug>
#include <KJob>
#include <kde_file.h>

using namespace Nepomuk2::Vocabulary;
using namespace Soprano::Vocabulary;

Nepomuk2::SimpleIndexer::SimpleIndexer(const QUrl& fileUrl)
{
    m_res.addProperty(NIE::url(), fileUrl);
    m_res.addProperty(NFO::fileName(), KUrl(fileUrl).fileName());

    m_res.addType(NFO::FileDataObject());
    m_res.addType(NIE::InformationElement());

    QFileInfo fileInfo(fileUrl.toLocalFile());
    if( fileInfo.isDir() )
        m_res.addType(NFO::Folder());

    //
    // Types by mime type
    //
    m_mimeType = KMimeType::findByUrl( fileUrl )->name();
    QList<QUrl> types = typesForMimeType( m_mimeType );
    foreach(const QUrl& type, types)
        m_res.addType( type );

    m_res.addProperty(NIE::mimeType(), m_mimeType);

    m_res.setProperty(NIE::created(), fileInfo.created());
    m_res.setProperty(NIE::lastModified(), fileInfo.lastModified());

#ifdef Q_OS_UNIX
    KDE_struct_stat statBuf;
    if( KDE_stat( QFile::encodeName(fileInfo.absoluteFilePath()).data(), &statBuf ) == 0 ) {
        m_res.setProperty( KExt::unixFileMode(), int(statBuf.st_mode) );
    }

    if( !fileInfo.owner().isEmpty() ) {
        m_res.setProperty( KExt::unixFileOwner(), fileInfo.owner() );
    }
    if( !fileInfo.group().isEmpty() ) {
        m_res.setProperty( KExt::unixFileGroup(), fileInfo.group() );
    }
#endif // Q_OS_UNIX
}

// static
QList<QUrl> Nepomuk2::SimpleIndexer::typesForMimeType(const QString& mimeType)
{
    QList<QUrl> types;

    // Basic types
    if( mimeType.contains(QLatin1String("audio")) )
        types << NFO::Audio();
    if( mimeType.contains(QLatin1String("video")) )
        types << NFO::Video();
    if( mimeType.contains(QLatin1String("image")) )
        types << NFO::Image();
    if( mimeType.contains(QLatin1String("text")) )
        types << NFO::PlainTextDocument();

    // Documents
    if( mimeType.contains(QLatin1String("application/msword")) )
        types << NFO::Document();
    if( mimeType.contains(QLatin1String("application/vnd.oasis.opendocument.text")) )
        types << NFO::Document();
    if( mimeType.contains(QLatin1String("application/epub")) )
        types << NFO::Document();
    if( mimeType.contains(QLatin1String("application/pdf")) )
        types << NFO::Document();

    // Presentation
    if( mimeType.contains(QLatin1String("application/vnd.oasis.opendocument.presentation")) )
        types << NFO::Presentation();
    if( mimeType.contains(QLatin1String("powerpoint") ) )
        types << NFO::Presentation();

    // Spreadsheet
    if( mimeType.contains(QLatin1String("excel")) )
        types << NFO::Spreadsheet();
    if( mimeType.contains(QLatin1String("application/vnd.oasis.opendocument.spreadsheet") ) )
        types << NFO::Spreadsheet();

    // Html
    if( mimeType.contains(QLatin1String("text/html") ) )
        types << NFO::HtmlDocument();

    // TODO: Add some basic NMM types?

    return types;
}

bool Nepomuk2::SimpleIndexer::save()
{
    QHash<QUrl, QVariant> additionalMetadata;
    additionalMetadata.insert( RDF::type(), NRL::DiscardableInstanceBase() );

    SimpleResourceGraph graph;
    graph << m_res;

    // we do not have an event loop - thus, we need to delete the job ourselves
    QScopedPointer<StoreResourcesJob> job( Nepomuk2::storeResources( graph, IdentifyNone,
                                           NoStoreResourcesFlags, additionalMetadata ) );
    job->setAutoDelete(false);
    job->exec();
    if( job->error() ) {
        kError() << "SimpleIndexerError: " << job->errorString();
        return false;
    }

    m_resUri = job->mappings().value( m_res.uri() );

    return true;
}

QString Nepomuk2::SimpleIndexer::mimeType()
{
    return m_mimeType;
}

QUrl Nepomuk2::SimpleIndexer::uri()
{
    return m_resUri;
}

