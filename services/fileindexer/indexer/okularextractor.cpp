/*
 * Copyright (C) 2012  Rohan Garg <rohangarg@kubuntu.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#include "okularextractor.h"

#include <okular/core/document.h>
#include <okular/core/settings_core.h>

#include "nfo.h"
#include "nie.h"
#include "nco.h"

#include <KService>
#include <KServiceTypeTrader>
#include <KTemporaryFile>

using namespace Nepomuk2::Vocabulary;

namespace Nepomuk2 {

okularExtractor::okularExtractor(QObject* parent, const QVariantList&)
                                 : ExtractorPlugin(parent)
{
}

QStringList okularExtractor::mimetypes()
{
    /*
     * Copied from Okular::Document::supportedMimeTypes
     */
    QStringList supportedMimeTypes;
    QString constraint( "(Library == 'okularpart')" );
    QLatin1String basePartService( "KParts/ReadOnlyPart" );
    KService::List offers = KServiceTypeTrader::self()->query( basePartService, constraint );
    KService::List::ConstIterator it = offers.constBegin(), itEnd = offers.constEnd();
    for ( ; it != itEnd; ++it )
    {
        KService::Ptr service = *it;
        QStringList mimeTypes = service->serviceTypes();
        foreach ( const QString& mimeType, mimeTypes )
            if ( mimeType != basePartService )
                supportedMimeTypes.append( mimeType );
    }

    // Remove the pdf mimetype since the poppler plugin does this
    if (supportedMimeTypes.contains("application/pdf")) {
        supportedMimeTypes.removeAt(supportedMimeTypes.indexOf("application/pdf"));
    }

    return supportedMimeTypes;
}

SimpleResourceGraph okularExtractor::extract(const QUrl& resUri, const QUrl& fileUrl, const QString& mimeType)
{
    Nepomuk2::SimpleResourceGraph graph;
    Okular::SettingsCore::instance("");
    Okular::Document *document = new Okular::Document(0);

    if ( document->openDocument(fileUrl.path(), fileUrl, KMimeType::mimeType(mimeType)) ) {
        SimpleResource fileRes( resUri );

        // Basic Data
        const Okular::DocumentInfo* docInfo = document->documentInfo();
        fileRes.addType(NFO::PaginatedTextDocument());

        QString author = docInfo->get(QLatin1String("author"));
        if (!author.isEmpty()) {
            SimpleResource res;
            res.addType( NCO::Contact() );
            res.addProperty( NCO::fullname(), author );
            graph << res;

            fileRes.addProperty( NCO::creator(), res );
        }

        QString subject = docInfo->get(QLatin1String("subject"));
        if (!subject.isEmpty()) {
            fileRes.addProperty( NIE::subject(),  subject );
        }

        QString pages = docInfo->get(QLatin1String("pages"));
        if (!pages.isEmpty()) {
            fileRes.addProperty( NFO::pageCount(), pages );
        }

        if (document->canExportToText()) {
            KTemporaryFile tempFile;
            tempFile.open();
            document->exportToText( tempFile.fileName() );
            fileRes.addProperty( NIE::plainTextContent(), tempFile.readAll() );
        }

        graph << fileRes;
    }

    delete document;
    return graph;
}
}
NEPOMUK_EXPORT_EXTRACTOR( Nepomuk2::okularExtractor, "nepomukokularextractor" )
