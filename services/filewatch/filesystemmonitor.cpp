/* This file is part of the Nepomuk KDE Project
   Copyright (c) 2013 Gabriel Poesia <gabriel.poesia@gmail.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "filesystemmonitor.h"
#include "regexpcache.h"
#include "fileindexerconfig.h"

Nepomuk2::FileSystemMonitor::FileSystemMonitor() : m_pathExcludeRegExpCache(new RegExpCache)
{ }

Nepomuk2::FileSystemMonitor::~FileSystemMonitor()
{ }

void Nepomuk2::FileSystemMonitor::setFilterList( const RegExpCache* filtersList )
{
    m_pathExcludeRegExpCache = filtersList;
}

bool Nepomuk2::FileSystemMonitor::filterWatch( const QString& path )
{
    return Nepomuk2::FileIndexerConfig::self()->shouldFolderBeWatched( path );
}

#include "filesystemmonitor.moc"
