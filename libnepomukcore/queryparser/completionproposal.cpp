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

#include "completionproposal.h"

#include "klocalizedstring.h"

using namespace Nepomuk2::Query;

struct CompletionProposal::Private
{
    QStringList pattern;
    int last_matched_part;
    int start_position;
    int end_position;
    CompletionProposal::Type type;
    KLocalizedString description;
};

CompletionProposal::CompletionProposal(const QStringList &pattern,
                                       int last_matched_part,
                                       int start_position,
                                       int end_position,
                                       Type type,
                                       const KLocalizedString &description)
: d(new Private)
{
    d->pattern = pattern;
    d->last_matched_part = last_matched_part;
    d->start_position = start_position;
    d->end_position = end_position;
    d->type = type;
    d->description = description;
}

CompletionProposal::~CompletionProposal()
{
    delete d;
}

int CompletionProposal::startPosition() const
{
    return d->start_position;
}

int CompletionProposal::endPosition() const
{
    return d->end_position;
}

int CompletionProposal::lastMatchedPart() const
{
    return d->last_matched_part;
}

QStringList CompletionProposal::pattern() const
{
    return d->pattern;
}

CompletionProposal::Type CompletionProposal::type() const
{
    return d->type;
}

KLocalizedString CompletionProposal::description() const
{
    return d->description;
}
