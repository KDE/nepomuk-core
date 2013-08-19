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

#include "pass_decimalvalues.h"
#include "utils.h"

#include "literalterm.h"

#include <soprano/literalvalue.h>

static double scales[] = {
    1.0,
    0.1,
    0.01,
    0.001,
    0.0001,
    0.00001,
    0.000001,
    0.0000001,
    0.00000001,
    0.000000001,
    0.0000000001,
    0.00000000001,
    0.000000000001
};

QList<Nepomuk2::Query::Term> PassDecimalValues::run(const QList<Nepomuk2::Query::Term> &match) const
{
    QList<Nepomuk2::Query::Term> rs;
    int integer_part;
    int decimal_part;

    if (termIntValue(match.at(0), integer_part) &&
        termIntValue(match.at(1), decimal_part) &&
        match.at(1).length() <= 12) {
        double scale = scales[match.at(1).length()];

        rs.append(Nepomuk2::Query::LiteralTerm(
            double(integer_part) + (double(decimal_part) * scale)
        ));
    }

    return rs;
}
