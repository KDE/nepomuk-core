/*
   Copyright (C) 2007-2011 Sebastian Trueg <trueg@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this library; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef _NEPOMUK_FILEINDEXER_UTIL_H_
#define _NEPOMUK_FILEINDEXER_UTIL_H_

#include <KUrl>

class KJob;

namespace Nepomuk2 {
    /// remove all indexed data for \p url the datamanagement way
    KJob* clearIndexedData( const QUrl& url );
    KJob* clearIndexedData( const QList<QUrl>& urls );
    /// update kext::indexingLevel for \p url
    void updateIndexingLevel( const QUrl& uri, int level );

}
#endif
