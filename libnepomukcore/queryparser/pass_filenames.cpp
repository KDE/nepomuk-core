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

#include "pass_filenames.h"
#include "utils.h"

#include "literalterm.h"
#include "comparisonterm.h"
#include "property.h"
#include "nfo.h"

#include <soprano/literalvalue.h>

#include <QtCore/QRegExp>

QList<Nepomuk2::Query::Term> PassFileNames::run(const QList<Nepomuk2::Query::Term> &match) const
{
    QList<Nepomuk2::Query::Term> rs;
    QString value = termStringValue(match.at(0));

    if (value.contains(QLatin1Char('.'))) {
        if (value.contains(QLatin1Char('*')) || value.contains(QLatin1Char('?'))) {
            // Filename globbing (original code here from Vishesh Handa)
            QString regex = QRegExp::escape(value);

            regex.replace(QLatin1String("\\*"), QLatin1String(".*"));
            regex.replace(QLatin1String("\\?"), QLatin1String("."));
            regex.replace(QLatin1String("\\"), QLatin1String("\\\\"));
            regex.prepend(QLatin1Char('^'));
            regex.append(QLatin1Char('$'));

            Nepomuk2::Query::ComparisonTerm comparison(
                Nepomuk2::Vocabulary::NFO::fileName(),
                Nepomuk2::Query::LiteralTerm(regex),
                Nepomuk2::Query::ComparisonTerm::Regexp
            );

            comparison.setPosition(match.at(0));
            comparison.subTerm().setPosition(match.at(0));
            rs.append(comparison);
        } else {
            // Exact file-name matching
            Nepomuk2::Query::ComparisonTerm comparison(
                Nepomuk2::Vocabulary::NFO::fileName(),
                match.at(0),
                Nepomuk2::Query::ComparisonTerm::Contains
            );

            comparison.setPosition(match.at(0));
            rs.append(comparison);
        }
    }

    return rs;
}