/*
    <one line to give the library's name and an idea of what it does.>
    Copyright (C) 2012  Vishesh Handa <me@vhanda.in>
    Copyright (C) 2012  Jörg Ehrichs <joerg.ehrichs@gmx.de>

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

#include <KDE/KDebug>

#include <poppler-qt4.h>

using namespace Nepomuk2::Vocabulary;

namespace Nepomuk2 {

PopplerExtractor::PopplerExtractor(QObject* parent, const QVariantList&)
: ExtractorPlugin(parent)
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

    if ( !pdfDoc || pdfDoc->isLocked() ){
        delete pdfDoc;
        return graph;
    }

    QString title = pdfDoc->info(QLatin1String("Title")).trimmed();

    // The title extracted from the pdf metadata is in many cases not the real title
    // of the document. Especially for research papers that are exported to pdf.
    // As mostly the title of a pdf document is written on the first page in the biggest font
    // we use this if the pdfDoc title is considered junk
    if(title.isEmpty() ||
       !title.contains(' ') ||                        // very unlikely the title of a document does only contain one word.
       title.contains(QLatin1String("Microsoft"), Qt::CaseInsensitive)) {  // most research papers i found written with microsoft word
                                                                           // have a garbage title of the pdf creator rather than the real document title
        title = parseFirstPage(pdfDoc, fileUrl);
    }

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
        if(!page) {  // broken pdf files do not return a valid page
            kWarning() << "Could not read page content from" << fileUrl;
            break;
        }
        plainTextContent.append( page->text( QRectF() ) );
        delete page;

        // This number has been experimentally chosen. Virtuoso cannot handle more than this
        static const int maxSize = 3 * 1024 * 1024;
        if( plainTextContent.size() >= maxSize ) {
            kWarning() << "Trimming plain text content from " << plainTextContent.size() << " to " << maxSize;
            plainTextContent.resize( maxSize );
            break;
        }
    }

    if( !plainTextContent.isEmpty() ) {
        fileRes.addProperty( NIE::plainTextContent(), plainTextContent );
    }

    fileRes.addType( NFO::PaginatedTextDocument() );

    delete pdfDoc;

    graph << fileRes;
    return graph;
}

QString PopplerExtractor::parseFirstPage(Poppler::Document* pdfDoc, const QUrl& fileUrl)
{
    Poppler::Page *p = pdfDoc->page(0);

    if(!p) {
        kWarning() << "Could not read page content from" << fileUrl;
      return QString();
    }

    QList<Poppler::TextBox*> tbList = p->textList();
    QMap<int, QString> possibleTitleMap;

    int currentLargestChar = 0;
    int skipTextboxes = 0;

    // Iterate over all textboxes. Each textbox can be a single character/word or textblock
    // Here we combine the etxtboxes back together based on the textsize
    // Important are the words with the biggest font size
    foreach (Poppler::TextBox * tb, tbList) {

        // if we added followup words, skip the textboxes here now
        if (skipTextboxes > 0) {
            skipTextboxes--;
            continue;
        }

        int height = tb->charBoundingBox(0).height();

        // if the following text is smaller than the biggest we found up to now, ignore it
        if (height >= currentLargestChar) {
            QString possibleTitle;
            possibleTitle.append(tb->text());
            currentLargestChar = height;

            // if the text has follow up words add them to to create the full title
            Poppler::TextBox * next = tb->nextWord();
            while (next) {
                possibleTitle.append(QLatin1String(" "));
                possibleTitle.append(next->text());
                next = next->nextWord();
                skipTextboxes++;
            }

            // now combine text for each font size together, very likeley it must be connected
            QString existingTitlePart = possibleTitleMap.value(currentLargestChar, QString());
            existingTitlePart.append(QLatin1String(" "));
            existingTitlePart.append(possibleTitle);
            possibleTitleMap.insert(currentLargestChar, existingTitlePart);
        }
    }

    qDeleteAll(tbList);
    delete p;

    QList<int> titleSizes = possibleTitleMap.keys();
    qSort(titleSizes.begin(), titleSizes.end(), qGreater<int>());

    QString newPossibleTitle;

    // find the text with the largest font that is not just 1 character
    foreach (int i, titleSizes) {
        QString title = possibleTitleMap.value(i);

        // sometime the biggest part is a single letter
        // as a starting paragraph letter
        if (title.size() < 5) {
            continue;
        } else {
            newPossibleTitle = title.trimmed();
            break;
        }
    }

    return newPossibleTitle;
}

}

NEPOMUK_EXPORT_EXTRACTOR( Nepomuk2::PopplerExtractor, "nepomukpopplerextractor" )
