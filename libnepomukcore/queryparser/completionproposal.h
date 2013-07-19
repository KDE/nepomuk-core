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

#ifndef __NEPOMUK2_SEARCH_COMPLETION_PROPOSAL_H__
#define __NEPOMUK2_SEARCH_COMPLETION_PROPOSAL_H__

#include <QtCore/QtGlobal>
#include <QtCore/QStringList>

#include "nepomuk_export.h"
#include "klocalizedstring.h"

namespace Nepomuk2 {
    namespace Query {
        /**
         * \class CompletionProposal completionproposal.h Nepomuk2/Query/CompletionProposal
         * \brief Information about an auto-completion proposal
         *
         * When parsing an user query, QueryParser may find that a pattern nearly
         * matches and that the user may want to use it. In this case, one or more
         * completion proposals are used to describe what patterns can be used.
         */
        class NEPOMUK_EXPORT CompletionProposal
        {
            private:
                Q_DISABLE_COPY(CompletionProposal)

            public:
                /**
                 * \brief Data-type used by the first placeholder of the pattern
                 *
                 * If the pattern is "sent by %1", the type of "%1" is Contact. This
                 * way, a GUI can show to the user a list of his or her contacts.
                 */
                enum Type
                {
                    NoType,         /*!< No specific type (integer, string, something that does not need any auto-completion list) */
                    DateTime,       /*!< A date-time */
                    Tag,            /*!< A valid tag name */
                    Contact,        /*!< Something that can be parsed unambiguously to a contact (a contact name, email, pseudo, etc) */
                };

                CompletionProposal(const QStringList &pattern,
                                   int last_matched_part,
                                   int start_position,
                                   int end_position,
                                   Type type,
                                   const KLocalizedString &description);
                ~CompletionProposal();

                QStringList pattern() const;
                int lastMatchedPart() const;

                int startPosition() const;
                int endPosition() const;

                Type type() const;
                KLocalizedString description() const;

            private:
                struct Private;
                Private *const d;
        };
    }
}

#endif