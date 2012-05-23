/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2010-2011 Sebastian Trueg <trueg@kde.org>

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

#include "file.h"
#include "variant.h"
#include "nie.h"
#include "nfo.h"

Nepomuk2::File::File( const KUrl& url )
    : Resource( url, Nepomuk2::Vocabulary::NFO::FileDataObject() )
{
}


Nepomuk2::File::File( const Resource& other )
    : Resource( other )
{
}


Nepomuk2::File::~File()
{
}


Nepomuk2::File& Nepomuk2::File::operator=( const KUrl& url )
{
    this->Resource::operator=(url);
    return (*this);
}


KUrl Nepomuk2::File::url() const
{
    return property( Nepomuk2::Vocabulary::NIE::url() ).toUrl();
}


Nepomuk2::File Nepomuk2::File::dirResource() const
{
    if( isFile() ) {
        return File( url().upUrl() );
    }
    else {
        return File();
    }
}
