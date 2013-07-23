/* This file is part of the Nepomuk query parser
   Copyright (c) 2013 Denis Steckelmacher <steckdenis@yahoo.fr>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2.1 as published by the Free Software Foundation,
   or any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "pass_properties.h"
#include "utils.h"

#include "comparisonterm.h"
#include "property.h"
#include "resourcemanager.h"
#include "resourceterm.h"
#include "nco.h"

#include <soprano/nao.h>
#include <soprano/rdfs.h>
#include <soprano/model.h>
#include <soprano/queryresultiterator.h>

PassProperties::PassProperties()
: cached_tags_filled(false),
  cached_contacts_filled(false)
{
}

void PassProperties::setProperty(const QUrl &property, Types range)
{
    this->property = property;
    this->range = range;
}

const QHash<QString, QUrl> &PassProperties::tags() const
{
    if (!cached_tags_filled) {
        const_cast<PassProperties *>(this)->fillTagsCache();
    }

    return cached_tags;
}

const QHash<QString, QUrl> &PassProperties::contacts() const
{
    if (!cached_contacts_filled) {
        const_cast<PassProperties *>(this)->fillContactsCache();
    }

    return cached_contacts;
}

void PassProperties::fillTagsCache()
{
    cached_tags_filled = true;

    // Get the tags URIs and their label in one SPARQL query
    QString query = QString::fromLatin1("select ?tag ?label where { "
                                        "?tag a %1 . "
                                        "?tag %2 ?label . "
                                        "}")
                    .arg(Soprano::Node::resourceToN3(Soprano::Vocabulary::NAO::Tag()),
                         Soprano::Node::resourceToN3(Soprano::Vocabulary::RDFS::label()));

    Soprano::QueryResultIterator it =
        Nepomuk2::ResourceManager::instance()->mainModel()->executeQuery(query, Soprano::Query::QueryLanguageSparql);

    while(it.next()) {
        cached_tags.insert(
            it["label"].toString(),
            QUrl(it["tag"].toString())
        );
    }
}

void PassProperties::fillContactsCache()
{
    cached_contacts_filled = true;

    // Get the contact URIs, their fullnames and email addresses
    QString query = QString::fromLatin1("select distinct ?c, ?fullname, ?email where { "
                                        "?c a %1 . "
                                        "{ "
                                        " ?c %2 ?fullname . "
                                        "} OPTIONAL { "
                                        " ?c %3 ?emailaddr . "
                                        " ?emailaddr %4 ?email . "
                                        "} "
                                        "}")
                    .arg(Soprano::Node::resourceToN3(Nepomuk2::Vocabulary::NCO::Contact()),
                         Soprano::Node::resourceToN3(Nepomuk2::Vocabulary::NCO::fullname()),
                         Soprano::Node::resourceToN3(Nepomuk2::Vocabulary::NCO::hasEmailAddress()),
                         Soprano::Node::resourceToN3(Nepomuk2::Vocabulary::NCO::emailAddress()));

    Soprano::QueryResultIterator it =
        Nepomuk2::ResourceManager::instance()->mainModel()->executeQuery(query, Soprano::Query::QueryLanguageSparql);

    while(it.next()) {
        QUrl uri(it["c"].toString());
        QString fullname(it["fullname"].toString());
        QString email(it["email"].toString());

        if (!fullname.isEmpty()) {
            cached_contacts.insert(fullname, uri);
        }
        if (!email.isEmpty()) {
            cached_contacts.insert(email, uri);
        }
    }
}

Nepomuk2::Query::Term PassProperties::convertToRange(const Nepomuk2::Query::LiteralTerm &term) const
{
    Soprano::LiteralValue value = term.value();

    switch (range) {
    case Integer:
        if (value.isInt() || value.isInt64()) {
            return term;
        }
        break;

    case IntegerOrDouble:
        if (value.isInt() || value.isInt64() || value.isDouble()) {
            return term;
        }
        break;

    case String:
        if (value.isString()) {
            return term;
        }
        break;

    case DateTime:
        if (value.isDateTime()) {
            return term;
        }
        break;

    case Tag:
        if (value.isString() && tags().contains(value.toString())) {
            Nepomuk2::Query::ResourceTerm rs(cached_tags.value(value.toString()));
            rs.setPosition(term);

            return rs;
        }
        break;

    case Contact:
        if (value.isString() && contacts().contains(value.toString())) {
            Nepomuk2::Query::ResourceTerm rs(cached_contacts.value(value.toString()));
            rs.setPosition(term);

            return rs;
        }
        break;
    }

    return Nepomuk2::Query::Term();
}

QList<Nepomuk2::Query::Term> PassProperties::run(const QList<Nepomuk2::Query::Term> &match) const
{
    QList<Nepomuk2::Query::Term> rs;
    Nepomuk2::Query::Term term = match.at(0);
    Nepomuk2::Query::Term subterm;
    Nepomuk2::Query::ComparisonTerm::Comparator comparator = Nepomuk2::Query::ComparisonTerm::Equal;

    if (term.isComparisonTerm()) {
        Nepomuk2::Query::ComparisonTerm &comparison = term.toComparisonTerm();

        if (comparison.subTerm().isLiteralTerm()) {
            subterm = convertToRange(comparison.subTerm().toLiteralTerm());
            comparator = comparison.comparator();
        }
    } else if (term.isLiteralTerm()) {
        // Property followed by a value, the comparator is "contains" for strings
        // and the equality for everything else
        subterm = convertToRange(term.toLiteralTerm());
        comparator = (
            (subterm.isLiteralTerm() && subterm.toLiteralTerm().value().isString()) ?
            Nepomuk2::Query::ComparisonTerm::Contains :
            Nepomuk2::Query::ComparisonTerm::Equal
        );
    }

    if (subterm.isValid()) {
        rs.append(Nepomuk2::Query::ComparisonTerm(
            property,
            subterm,
            comparator
        ));
    }

    return rs;
}
