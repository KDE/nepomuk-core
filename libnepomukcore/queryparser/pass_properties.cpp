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
{
}

void PassProperties::setProperty(const QUrl &property, Types range)
{
    this->property = property;
    this->range = range;
}

QStringList PassProperties::tags() const
{
    return cacheContents(cached_tags);
}

QStringList PassProperties::contacts() const
{
    return cacheContents(cached_contacts);
}

QStringList PassProperties::emailAddresses() const
{
    return cacheContents(cached_emails);
}

QStringList PassProperties::cacheContents(const PassProperties::Cache &cache) const
{
    if (!cache.populated) {
        PassProperties *t = const_cast<PassProperties *>(this);
        Cache *c = const_cast<Cache *>(&cache);

        // Populate the cache
        if (c == &cached_tags) {
            t->fillCache(*c, QString::fromLatin1(
                "select distinct ?res ?label where { "
                "?res a %1 . "
                "?res %2 ?label . "
                "}"
                ).arg(Soprano::Node::resourceToN3(Soprano::Vocabulary::NAO::Tag()),
                      Soprano::Node::resourceToN3(Soprano::Vocabulary::RDFS::label()))
            );
        } else if (c == &cached_contacts) {
            t->fillCache(*c, QString::fromLatin1(
                "select distinct ?res ?label where { "
                "?res a %1 . "
                "?res %2 ?label . "
                "}"
                ).arg(Soprano::Node::resourceToN3(Nepomuk2::Vocabulary::NCO::Contact()),
                      Soprano::Node::resourceToN3(Nepomuk2::Vocabulary::NCO::fullname()))
            );
        } else if (c == &cached_emails) {
            t->fillCache(*c, QString::fromLatin1(
                "select distinct ?res ?label where { "
                "?res a %1 . "
                "?res %2 ?label . "
                "}"
                ).arg(Soprano::Node::resourceToN3(Nepomuk2::Vocabulary::NCO::EmailAddress()),
                      Soprano::Node::resourceToN3(Nepomuk2::Vocabulary::NCO::emailAddress()))
            );
        }
    }

    return cache.contents;
}

void PassProperties::fillCache(PassProperties::Cache &cache, const QString &query)
{
    cache.populated = true;

    Soprano::QueryResultIterator it =
        Nepomuk2::ResourceManager::instance()->mainModel()->executeQuery(query, Soprano::Query::QueryLanguageSparql);

    while(it.next()) {
        cache.contents.append(it["label"].toString());
    }

    qSort(cache.contents);
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
        return termFromCache(term, cached_tags);

    case Contact:
        return termFromCache(term, cached_contacts);

    case EmailAddress:
        return termFromCache(term, cached_emails);
    }

    return Nepomuk2::Query::Term();
}

Nepomuk2::Query::Term PassProperties::termFromCache(const Nepomuk2::Query::LiteralTerm &value,
                                                    const PassProperties::Cache &cache) const
{
    if (!cache.populated) {
        cacheContents(cache);
    }

    if (!value.value().isString()) {
        return Nepomuk2::Query::Term();
    }

    QStringList::const_iterator it = qBinaryFind(cache.contents, value.value().toString());

    if (it != cache.contents.end()) {
        Nepomuk2::Query::LiteralTerm rs(*it);
        rs.setPosition(value);

        return rs;
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
            range == PassProperties::String ?
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
