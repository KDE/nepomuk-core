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
    SimpleResource res;

    res.addProperty(NIE::url(), fileUrl);
    res.addProperty(NFO::fileName(), KUrl(fileUrl).fileName());

    res.addType(NFO::FileDataObject());
    res.addType(NIE::InformationElement());

    QFileInfo fileInfo(fileUrl.toLocalFile());
    if( fileInfo.isDir() )
        res.addType(NFO::Folder());

    //
    // Types by mime type
    //
    QString mimeType = KMimeType::findByUrl( fileUrl )->name();
    QList<QUrl> types = typesForMimeType( mimeType );
    foreach(const QUrl& type, types)
        res.addType( type );

    res.addProperty(NIE::mimeType(), mimeType);

    // Do not set NIE::lastModified
    // We only set that for files which are properly indexed
    res.setProperty(NIE::created(), fileInfo.created());

    m_graph << res;
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

    // we do not have an event loop - thus, we need to delete the job ourselves
    QScopedPointer<KJob> job( Nepomuk2::storeResources( m_graph, IdentifyNone,
                                                        NoStoreResourcesFlags, additionalMetadata ) );
    job->setAutoDelete(false);
    job->exec();
    if( job->error() ) {
        kError() << "SimpleIndexerError: " << job->errorString();
        return false;
    }

    return true;
}
