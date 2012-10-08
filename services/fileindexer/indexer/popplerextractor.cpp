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


#include "popplerextractor.h"

#include "nco.h"
#include "nie.h"
#include "nfo.h"

#include <poppler/qt4/poppler-qt4.h>

using namespace Nepomuk2::Vocabulary;

namespace Nepomuk2 {

PopplerExtractor::PopplerExtractor(QObject* parent, const QVariantList&)
: Extractor(parent)
{

}

QStringList PopplerExtractor::mimetypes()
{
    QStringList list;
    list << QLatin1String("application/pdf");

    return list;
}


SimpleResourceGraph PopplerExtractor::extract(const QUrl& resUri, const QUrl& fileUrl, const QString& mimeType)
{
    Q_UNUSED( mimeType );

    SimpleResourceGraph graph;
    SimpleResource fileRes( resUri );

    Poppler::Document* pdfDoc = Poppler::Document::load( fileUrl.toLocalFile(), 0, 0 );
    QString title = pdfDoc->info(QLatin1String("Title"));
    if( !title.isEmpty() ) {
        fileRes.addProperty( NIE::title(), title );
    }

    QString subject = pdfDoc->info(QLatin1String("Subject"));
    if( !subject.isEmpty() ) {
        fileRes.addProperty( NIE::subject(), subject );
    }

    QString creator = pdfDoc->info(QLatin1String("Creator"));
    if( !creator.isEmpty() ) {
        SimpleResource res;
        res.addType( NCO::Contact() );
        res.addProperty( NCO::fullname(), creator );
        graph << res;

        fileRes.addProperty( NCO::creator(), res );
    }

    QString plainTextContent;
    for( int i=0; i<pdfDoc->numPages(); i++ ) {
        Poppler::Page* page = pdfDoc->page( i );
        plainTextContent.append( page->text( QRectF() ) );
    }

    if( !plainTextContent.isEmpty() ) {
        fileRes.addProperty( NIE::plainTextContent(), plainTextContent );
    }

    fileRes.addType( NFO::PaginatedTextDocument() );

    graph << fileRes;
    return graph;
}

}

NEPOMUK_EXPORT_EXTRACTOR( Nepomuk2::PopplerExtractor, "nepomukpopplerextractor" )