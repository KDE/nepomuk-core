/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2012 Vishesh Handa <me@vhanda.in>
   Copyright (C) 2007-2010 Sebastian Trueg <trueg@kde.org>

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

#include "resultiterator.h"

#include <resource.h>
#include <resourcemanager.h>

#include <Soprano/Model>
#include <Soprano/QueryResultIterator>

namespace Nepomuk2 {
    namespace Query {
        class ResultIterator::Private {
        public:
            RequestPropertyMap m_requestMap;
            Soprano::QueryResultIterator m_it;
        };
    }
}

Nepomuk2::Query::ResultIterator::ResultIterator(const Nepomuk2::Query::Query& query)
    : d( new Nepomuk2::Query::ResultIterator::Private() )
{
    Soprano::Model* model = ResourceManager::instance()->mainModel();

    d->m_requestMap = query.requestPropertyMap();
    d->m_it = model->executeQuery( query.toSparqlQuery(), Soprano::Query::QueryLanguageSparql );
}

Nepomuk2::Query::ResultIterator::ResultIterator(const QString& sparql, const Nepomuk2::Query::RequestPropertyMap& map)
    : d( new Nepomuk2::Query::ResultIterator::Private() )
{
    d->m_requestMap = map;

    if( !sparql.isEmpty() ) {
        Soprano::Model* model = ResourceManager::instance()->mainModel();
        d->m_it = model->executeQuery( sparql, Soprano::Query::QueryLanguageSparql );
    }
}

Nepomuk2::Query::ResultIterator::~ResultIterator()
{
    delete d;
}


Nepomuk2::Query::Result Nepomuk2::Query::ResultIterator::current() const
{
    Result result( Resource::fromResourceUri( d->m_it[0].uri() ) );

    // make sure we do not store values twice
    QStringList names = d->m_it.bindingNames();
    names.removeAll( QLatin1String( "r" ) );

    RequestPropertyMap::const_iterator rpIt = d->m_requestMap.constBegin();
    for (  ; rpIt != d->m_requestMap.constEnd(); ++rpIt ) {
        result.addRequestProperty( rpIt.value(), d->m_it.binding( rpIt.key() ) );
    }

    static const char* s_scoreVarName = "_n_f_t_m_s_";
    static const char* s_excerptVarName = "_n_f_t_m_ex_";

    Soprano::BindingSet set;
    int score = 0;
    Q_FOREACH( const QString& var, names ) {
        if ( var == QLatin1String( s_scoreVarName ) )
            score = d->m_it[var].literal().toInt();
        else if ( var == QLatin1String( s_excerptVarName ) )
            result.setExcerpt( d->m_it[var].toString() );
        else
            set.insert( var, d->m_it[var] );
    }

    result.setAdditionalBindings( set );
    result.setScore( ( double )score );

    return result;
}

bool Nepomuk2::Query::ResultIterator::next()
{
    return d->m_it.next();
}

bool Nepomuk2::Query::ResultIterator::isValid() const
{
    return d->m_it.isValid();
}

Nepomuk2::Query::Result Nepomuk2::Query::ResultIterator::operator*() const
{
    return current();
}

Nepomuk2::Query::Result Nepomuk2::Query::ResultIterator::result() const
{
    return current();
}

