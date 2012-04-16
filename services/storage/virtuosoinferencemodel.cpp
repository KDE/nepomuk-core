/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2012 Sebastian Trueg <trueg@kde.org>
   
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

#include "virtuosoinferencemodel.h"

#include <Soprano/QueryResultIterator>
#include <Soprano/Node>
#include <Soprano/Vocabulary/NRL>
#include <Soprano/Vocabulary/NAO>
#include <Soprano/Vocabulary/XMLSchema>
#include <Soprano/Vocabulary/RDFS>

#include <KDebug>

using namespace Soprano::Vocabulary;

namespace {
    const char* s_typeVisibilityGraph = "nepomuk:/ctx/typevisibility";
    const char* s_nepomukInferenceRuleSetName = "nepomukinference";
}


Nepomuk::VirtuosoInferenceModel::VirtuosoInferenceModel(Model *model)
    : Soprano::FilterModel(model),
      m_haveInferenceRule(false)
{
}

Nepomuk::VirtuosoInferenceModel::~VirtuosoInferenceModel()
{
}

Soprano::QueryResultIterator Nepomuk::VirtuosoInferenceModel::executeQuery(const QString &query, Soprano::Query::QueryLanguage language, const QString &userQueryLanguage) const
{
    if(language == Soprano::Query::QueryLanguageSparqlNoInference) {
        return FilterModel::executeQuery(query, Soprano::Query::QueryLanguageSparql);
    }
    else if(language == Soprano::Query::QueryLanguageSparql && m_haveInferenceRule) {
        return FilterModel::executeQuery(QString::fromLatin1("DEFINE input:inference <%1> ")
                                         .arg(QLatin1String(s_nepomukInferenceRuleSetName)) + query, language);
    }
    else {
        return FilterModel::executeQuery(query, language, userQueryLanguage);
    }
}

void Nepomuk::VirtuosoInferenceModel::updateOntologyGraphs(bool forced)
{
    int ontologyCount= 0;

    // update the ontology graph group only if something changed (forced) or if it is empty
    if(forced || ontologyCount <= 0) {
        kDebug() << "Need to update ontology graph group";
        // fetch all nrl:Ontology graphs and add them to the graph group
        Soprano::QueryResultIterator it
                = executeQuery(QString::fromLatin1("select distinct ?r where { ?r a %1 . }")
                               .arg(Soprano::Node::resourceToN3(Soprano::Vocabulary::NRL::Ontology())),
                               Soprano::Query::QueryLanguageSparql);
        while(it.next()) {
            ++ontologyCount;
            executeQuery(QString::fromLatin1("rdfs_rule_set('%1','%2')")
                         .arg(QLatin1String(s_nepomukInferenceRuleSetName))
                         .arg(it[0].uri().toString()),
                         Soprano::Query::QueryLanguageUser,
                         QLatin1String("sql"));
        }
    }

    m_haveInferenceRule = (ontologyCount > 0);

    // update graph visibility if something has changed or if we never did it
    const QUrl visibilityGraph = QUrl::fromEncoded(s_typeVisibilityGraph);
    if(forced ||
       !executeQuery(QString::fromLatin1("ask where { graph %1 { ?s ?p ?o . } }")
                     .arg(Soprano::Node::resourceToN3(visibilityGraph)),
                     Soprano::Query::QueryLanguageSparql).boolValue()) {
        kDebug() << "Need to update type visibility.";
        updateTypeVisibility();
    }
}

void Nepomuk::VirtuosoInferenceModel::updateTypeVisibility()
{
    const QUrl visibilityGraph = QUrl::fromEncoded(s_typeVisibilityGraph);

    // 1. remove all visibility values we added ourselves
    removeContext(visibilityGraph);

    // 2. Set each type non-visible which has a parent type that is non-visible
    executeQuery(QString::fromLatin1("insert into %1 { "
                                     "?t %2 'false'^^%3 . "
                                     "} where { "
                                     "?t a rdfs:Class . "
                                     "filter not exists { ?t %2 ?v . } . "
                                     "filter exists { ?tt %2 'false'^^%3 .  ?t rdfs:subClassOf ?tt . } }")
                 .arg(Soprano::Node::resourceToN3(visibilityGraph),
                      Soprano::Node::resourceToN3(NAO::userVisible()),
                      Soprano::Node::resourceToN3(XMLSchema::boolean())),
                 Soprano::Query::QueryLanguageSparql);

    // 3. Set each type visible which is not rdfs:Resource and does not have a non-visible parent
    executeQuery(QString::fromLatin1("insert into %1 { "
                                     "?t %2 'true'^^%3 . "
                                     "} where { "
                                     "?t a rdfs:Class . "
                                     "filter not exists { ?t %2 ?v . } . "
                                     "filter not exists { ?tt %2 'false'^^%3 .  ?t rdfs:subClassOf ?tt . } }")
                 .arg(Soprano::Node::resourceToN3(visibilityGraph),
                      Soprano::Node::resourceToN3(NAO::userVisible()),
                      Soprano::Node::resourceToN3(XMLSchema::boolean())),
                 Soprano::Query::QueryLanguageSparql);

    // 4. make rdfs:Resource non-visible (this is required since with KDE 4.9 we introduced a new
    //    way of visibility handling which relies on types alone rather than visibility values on
    //    resources. Any visible type will make all sub-types visible, too. If rdfs:Resource were
    //    visible everything would be.
    removeAllStatements(RDFS::Resource(), NAO::userVisible(), Soprano::Node());
}

#include "virtuosoinferencemodel.moc"
