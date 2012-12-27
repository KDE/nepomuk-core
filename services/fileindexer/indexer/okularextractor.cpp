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

OkularExtractor::OkularExtractor(QObject* parent, const QVariantList&)
                                 : ExtractorPlugin(parent)
{
}

QStringList OkularExtractor::mimetypes()
{
/* Add support for all of the following
 *
 * application/postscriptimage/x-eps
 * application/x-gzpostscript
 * application/x-bzpostscript
 * image/x-gzeps
 * image/x-bzeps
 * image/vnd.djvu
 * application/x-fictionbook+xml
 * application/x-mobipocket-ebook
 * KParts/ReadWritePart
 * application/vnd.kde.okular-archive
 * application/x-dvi
 * application/x-gzdvi
 * application/x-bzdvi
 * image/fax-g3
 * image/g3fax
 * application/x-pdf
 * application/x-gzpdf
 * application/x-bzpdf
 * application/x-wwf
 * image/bmp
 * image/x-dds
 * image/x-eps
 * image/x-exr
 * image/gif
 * image/x-hdr
 * image/x-ico
 * video/x-mng
 * image/x-portable-bitmap
 * image/x-pcx
 * image/x-portable-graymap
 * image/x-portable-pixmap
 * image/x-psd
 * image/x-rgb
 * image/x-tga
 * image/x-xbitmap
 * image/x-xcf
 * image/x-xpixmap
 * image/x-gzeps
 * image/x-bzeps
 * application/x-chm
 * application/vnd.oasis.opendocument.text
 * application/oxps
 * application/vnd.ms-xpsdocument
 * application/prs.plucker
 * application/epub+zip
 * application/x-cbz
 * application/x-cbr
 * application/x-cbt
 */

    // An instance of Okular:SettingsCore is required for Okular to be able to work
    // Used to monitor configuration changes internally
    Okular::SettingsCore::instance("");
    const QScopedPointer <Okular::Document> document(new Okular::Document(0));

    QStringList supportedMimeTypes = document->supportedMimeTypes();

    // Supported by other plugins with better implementations
    supportedMimeTypes.removeAll(QLatin1String("application/pdf"));
    supportedMimeTypes.removeAll(QLatin1String("image/jpeg"));
    supportedMimeTypes.removeAll(QLatin1String("image/jp2"));
    supportedMimeTypes.removeAll(QLatin1String("image/png"));
    supportedMimeTypes.removeAll(QLatin1String("image/tiff"));

    return supportedMimeTypes;
}

SimpleResourceGraph OkularExtractor::extract(const QUrl& resUri, const QUrl& fileUrl, const QString& mimeType)
{
    Nepomuk2::SimpleResourceGraph graph;
    // An instance of Okular:SettingsCore is required for Okular to be able to work
    // Used to monitor configuration changes internally
    Okular::SettingsCore::instance("");
    QScopedPointer <Okular::Document> document(new Okular::Document(0));

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

        QString producer = docInfo->get(QLatin1String("producer"));
        if (!producer.isEmpty()) {
            fileRes.addProperty( NIE::generator(), producer );
        }

        QString copyright = docInfo->get(QLatin1String("copyright"));
        if (!producer.isEmpty()) {
            fileRes.addProperty( NIE::copyright(), copyright);
        }

        QString creationDate = docInfo->get(QLatin1String("creationDate"));
        if (!creationDate.isEmpty()) {
            fileRes.addProperty( NIE::contentCreated(), creationDate);
        }

        QString modificationDate = docInfo->get(QLatin1String("modificationDate"));
        if (!modificationDate.isEmpty()) {
            fileRes.addProperty( NIE::contentModified(), modificationDate);
        }

        QString title = docInfo->get(QLatin1String("title"));
        if (!title.isEmpty()) {
            fileRes.addProperty( NIE::title(), title);
        }

        if (document->canExportToText()) {
            KTemporaryFile tempFile;
            // KTemporaryFile only creates the file after open is called,
            // so we create the file and then close it.
            tempFile.open();
            tempFile.close();
            document->exportToText( tempFile.fileName() );
            // Open again, because exportToText closes the fd
            tempFile.open();
            fileRes.addProperty( NIE::plainTextContent(), tempFile.readAll() );
        }

        graph << fileRes;
    }

    return graph;
}
}
NEPOMUK_EXPORT_EXTRACTOR( Nepomuk2::OkularExtractor, "nepomukokularextractor" )
