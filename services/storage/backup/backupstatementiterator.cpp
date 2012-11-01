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


#include "backupstatementiterator.h"
#include <KDebug>

Nepomuk2::BackupStatementIterator::BackupStatementIterator(Soprano::Model* model)
    : m_model( model )
{
    QString resourceQuery = QString::fromLatin1("select distinct ?r ?p ?o ?g where { "
                                                "graph ?g { ?r ?p ?o. } "
                                                " ?g a nrl:InstanceBase ."
                                                 "FILTER( REGEX(STR(?r), '^nepomuk:/(res/|me)') ) ."
                                                " FILTER NOT EXISTS { ?g a nrl:DiscardableInstanceBase . }"
                                                " } ORDER BY ?r ?p");

    m_it = m_model->executeQuery( resourceQuery, Soprano::Query::QueryLanguageSparqlNoInference );
    m_state = State_ProcessingResources;
}

bool Nepomuk2::BackupStatementIterator::next()
{
    if( m_state == State_Done )
        return false;

    if( m_it.next() ) {
        return true;
    }

    if( m_state == State_ProcessingResources ) {
        // Graphs
        // Do not add "?r a nrl:InstanceBase" otherwise ?r won't match the nrl:GraphMetadata
        QString graphQuery = QString::fromLatin1("select distinct ?r ?p ?o ?g where { "
                                                 "graph ?g { ?r ?p ?o. } "
                                                 " ?g a nrl:GraphMetadata . "
                                                 "FILTER( REGEX(STR(?r), '^nepomuk:/ctx/') ) ."
                                                 " FILTER NOT EXISTS { graph ?g { ?r2 a nrl:DiscardableInstanceBase .} }"
                                                 "} ORDER BY ?r ?p ");

        m_it = m_model->executeQuery( graphQuery, Soprano::Query::QueryLanguageSparqlNoInference );
        m_state = State_ProcessingGraphs;

        return m_it.next();
    }

    if( m_state == State_ProcessingGraphs ) {
        m_state = State_Done;
        return false;
    }

    return false;
}

Soprano::Statement Nepomuk2::BackupStatementIterator::current()
{
    return Soprano::Statement( m_it["r"], m_it["p"], m_it["o"], m_it["g"] );
}

