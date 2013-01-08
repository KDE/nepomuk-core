/*
  This file is part of the Nepomuk KDE project.
  Copyright (C) 2007-2010 Sebastian Trueg <trueg@kde.org>

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

#include "searchrunnable.h"
#include "folder.h"

#include "resourcemanager.h"
#include "resource.h"
#include "resultiterator.h"

#include <Soprano/Version>
#include <Soprano/Model>
#include <Soprano/QueryResultIterator>
#include <Soprano/Node>
#include <Soprano/Statement>
#include <Soprano/LiteralValue>
#include <Soprano/StatementIterator>
#include <Soprano/Vocabulary/RDF>
#include <Soprano/Vocabulary/RDFS>
#include <Soprano/Vocabulary/NRL>
#include <Soprano/Vocabulary/NAO>
#include <Soprano/Vocabulary/XMLSchema>
#include <Soprano/Vocabulary/OWL>
#include <Soprano/Vocabulary/Xesam>
#include "nfo.h"

#include <KDebug>
#include <KDateTime>
#include <KRandom>

#include <QtCore/QTime>
#include <QtCore/QRegExp>
#include <QtCore/QLatin1String>
#include <QtCore/QStringList>


Nepomuk2::Query::SearchRunnable::SearchRunnable( Soprano::Model* model, const QString& sparqlQuery, const Nepomuk2::Query::RequestPropertyMap& map )
    : QRunnable(),
      m_model( model ),
      m_sparqlQuery( sparqlQuery ),
      m_requestPropertyMap( map ),
      m_cancelled( false )
{
}


Nepomuk2::Query::SearchRunnable::~SearchRunnable()
{
}


void Nepomuk2::Query::SearchRunnable::cancel()
{
    m_cancelled = true;
}


void Nepomuk2::Query::SearchRunnable::run()
{
    kDebug() << m_sparqlQuery;

#ifndef NDEBUG
    QTime time;
    time.start();
#endif

    ResultIterator hits( m_sparqlQuery, m_requestPropertyMap );
    while ( !m_cancelled && hits.next() ) {
        Result result = hits.result();

        kDebug() << "Found result:" << result.resource().uri() << result.score();
        emit newResult( result );
    }

#ifndef NDEBUG
    kDebug() << "Query Time:" << time.elapsed()/1000.0 << "seconds";
#endif

    emit listingFinished();
}
