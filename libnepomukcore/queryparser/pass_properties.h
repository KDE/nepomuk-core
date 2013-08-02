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

#ifndef __PASS_PROPERTIES_H__
#define __PASS_PROPERTIES_H__

#include <QtCore/QStringList>
#include <QtCore/QUrl>

#include "literalterm.h"

class PassProperties
{
    public:
        enum Types {
            Integer,
            IntegerOrDouble,
            String,
            DateTime,
            Tag,
            Contact,
            EmailAddress
        };

        PassProperties();

        void setProperty(const QUrl &property, Types range);

        QStringList tags() const;
        QStringList contacts() const;
        QStringList emailAddresses() const;

        QList<Nepomuk2::Query::Term> run(const QList<Nepomuk2::Query::Term> &match) const;

    private:
        struct Cache {
            Cache() : populated(false) {}

            QStringList contents;
            bool populated;
        };

        QStringList cacheContents(const Cache &cache) const;
        void fillCache(Cache &cache, const QString &query);

        Nepomuk2::Query::Term convertToRange(const Nepomuk2::Query::LiteralTerm &term) const;
        Nepomuk2::Query::Term termFromCache(const Nepomuk2::Query::LiteralTerm &value, const Cache &cache) const;

    private:
        QUrl property;
        Types range;

        // Caches
        Cache cached_tags;
        Cache cached_contacts;
        Cache cached_emails;
};

#endif