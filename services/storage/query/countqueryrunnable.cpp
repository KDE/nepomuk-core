/*
   Copyright (c) 2010 Sebastian Trueg <trueg@kde.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "countqueryrunnable.h"
#include "folder.h"

#include "query/query.h"

#include <Soprano/QueryResultIterator>
#include <Soprano/Node>
#include <Soprano/LiteralValue>
#include <Soprano/Model>

#include <KDebug>

#include <QtCore/QMutexLocker>
#include <QtCore/QTime>

Nepomuk2::Query::CountQueryRunnable::CountQueryRunnable( Soprano::Model* model, const Nepomuk2::Query::Query& query )
    : QRunnable(),
      m_model( model ),
      m_cancelled( false )
{
    m_countQuery = query.toSparqlQuery( Query::CreateCountQuery );
    kDebug();
}


Nepomuk2::Query::CountQueryRunnable::~CountQueryRunnable()
{
}


void Nepomuk2::Query::CountQueryRunnable::cancel()
{
    m_cancelled = true;
}


void Nepomuk2::Query::CountQueryRunnable::run()
{
    int count = -1;

#ifndef NDEBUG
    QTime time;
    time.start();
#endif

    Soprano::QueryResultIterator it = m_model->executeQuery( m_countQuery, Soprano::Query::QueryLanguageSparql );
    if( it.next() && !m_cancelled ) {
        count = it.binding( 0 ).literal().toInt();
    }
    kDebug() << "Count:" << count;

#ifndef NDEBUG
    kDebug() << "Count Query Time:" << time.elapsed()/1000.0 << "seconds";
#endif

    if( !m_cancelled )
        emit countQueryFinished( count );
}
