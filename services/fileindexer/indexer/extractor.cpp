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


#include "extractor.h"
#include "popplerextractor.h"

#include <QtCore/QMultiHash>

QList<Nepomuk2::Extractor*> Nepomuk2::Extractor::extractorsForMimeType(const QString& mimeType)
{
    static QMultiHash<QString, Extractor*> hash;
    if( hash.isEmpty() ) {
        QList<Extractor*> extractors;
        extractors << new PopplerExtractor;

        foreach(Extractor* ex, extractors) {
            foreach(const QString& mime, ex->mimetypes())
                hash.insertMulti( mime, ex );
        }
    }

    return hash.values( mimeType );
}

Nepomuk2::Extractor::~Extractor()
{

}
