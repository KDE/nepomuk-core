/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2007-2010 Sebastian Trueg <trueg@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
 */

#ifndef _NEPOMUK2_SEARCH_QUERY_PARSER_H_
#define _NEPOMUK2_SEARCH_QUERY_PARSER_H_

#include "query.h"

#include <QtCore/QString>

#include "nepomuk_export.h"

class PatternMatcher;

namespace Nepomuk2 {
    namespace Query {
        class CompletionProposal;

        /**
         * \class QueryParser queryparser.h Nepomuk2/Query/QueryParser
         *
         * \brief Parser for desktop user queries.
         *
         * \warning This is NOT a SPARQL parser.
         *
         * The QueryParser can be used to parse user queries, ie. queries that the user
         * would enter in any search interface, and convert them into Query instances.
         *
         * The syntax is language-dependent and as natural as possible. Words are
         * by default matched against the content of documents, and special patterns
         * like "sent by X" are recognized and changed to property comparisons.
         *
         * Natural date-times can also be used and are recognized as date-time
         * literals. "sent on March 6" and "created last week" work as expected.
         *
         * \section queryparser_examples Examples
         *
         * %Query everything that contains the term "nepomuk":
         * \code
         * nepomuk
         * \endcode
         *
         * %Query everything that contains both the terms "Hello" and "World":
         * \code
         * Hello World
         * \endcode
         *
         * %Query everything that contains the term "Hello World":
         * \code
         * "Hello World"
         * \endcode
         *
         * %Query everything that has a tag whose label matches "nepomuk":
         * \code
         * tagged as nepomuk, has tag nepomuk
         * \endcode
         *
         * %Query everything that has a tag labeled "nepomuk" or a tag labeled "scribo":
         * \code
         * tagged as nepomuk OR tagged as scribo
         * \endcode
         *
         * \note "tagged as nepomuk or scribo" currently does not work as expected
         *
         * %Query everything that has a tag labeled "nepomuk" but not a tag labeled "scribo":
         * \code
         * tagged as nepomuk and not tagged as scribo
         * \endcode
         *
         * Some properties also accept nested queries. For instance, this %Query
         * returns the list of documents related to emails tagged as important.
         * \code
         * documents related to mails tagged as important
         * \endcode
         *
         * More complex nested queries can be built, and are ended with a comma
         * (in English):
         * \code
         * documents related to mails from Smith and tagged as important, size > 2M
         * \endcode
         *
         * \author Sebastian Trueg <trueg@kde.org>
         * \author Denis Steckelmacher <steckdenis@yahoo.fr> (4.12 version)
         *
         * \since 4.4
         */
        class NEPOMUK_EXPORT QueryParser
        {
            friend class ::PatternMatcher;

        public:
            /**
             * Create a new query parser.
             */
            QueryParser();

            /**
             * Destructor
             */
            ~QueryParser();

            /**
             * Flags to change the behaviour of the parser.
             *
             * \since 4.5
             */
            enum ParserFlag {
                /**
                 * No flags. Default for parse()
                 */
                NoParserFlags = 0x0,

                /**
                 * Make each full text term use a '*' wildcard
                 * to match longer strings ('foobar' is matched
                 * by 'foob*').
                 *
                 * Be aware that the query engine needs at least
                 * 4 chars to do globbing though.
                 *
                 * This is disabled by default.
                 */
                QueryTermGlobbing = 0x1,

                /**
                 * Try to detect filename pattern like *.mp3
                 * or hello*.txt and create appropriate ComparisonTerm
                 * instances using ComparisonTerm::Regexp instead of
                 * ComparisonTerm::Contains.
                 *
                 * \since 4.6
                 */
                DetectFilenamePattern = 0x2
            };
            Q_DECLARE_FLAGS( ParserFlags, ParserFlag )

            /**
             * Parse a user query.
             *
             * \return The parsed query or an invalid Query object
             * in case the parsing failed.
             */
            Query parse( const QString& query ) const;

            /**
             * Parse a user query.
             *
             * \param query The query string to parse
             * \param flags a set of flags influencing the parsing process.
             *
             * \return The parsed query or an invalid Query object
             * in case the parsing failed.
             *
             * \since 4.5
             */
            Query parse( const QString& query, ParserFlags flags ) const;

            /**
             * Parse a user query.
             *
             * \param query The query string to parse
             * \param flags a set of flags influencing the parsing process.
             * \param cursor_position position of the cursor in a line edit used
             *                        by the user to enter the query. It is used
             *                        to provide auto-completion proposals.
             *
             * \return The parsed query or an invalid Query object
             * in case the parsing failed.
             *
             * \since 4.12
             */
            Query parse( const QString& query, ParserFlags flags, int cursor_position ) const;

            /**
             * List of completion proposals related to the previously parsed query
             *
             * \note The query parser is responsible for deleting the CompletionProposal
             *       objects.
             */
            QList<CompletionProposal *> completionProposals() const;

            /**
             * List of all the valid tags on the user's system. This list is
             * cached and calling this function does not touch the Nepomuk server.
             */
            QStringList allTags() const;

            /**
             * List of all the contacts of the user. This list is
             * cached and calling this function does not touch the Nepomuk server.
             */
            QStringList allContacts() const;

            /**
             * Try to match a field name as used in a query string to actual
             * properties.
             *
             * \deprecated
             * \warning This method does not return anything, as the parser does
             *          not need this information since KDE 4.12
             */
            QList<Types::Property> matchProperty( const QString& fieldName ) const;

            /**
             * Convenience method to quickly parse a query without creating an object.
             *
             * \warning The parser caches many useful information that is slow to
             *          retrieve from the Nepomuk server. If you have to parse
             *          many queries, consider using a QueryParser object.
             *
             * \return The parsed query or an invalid Query object
             * in case the parsing failed.
             */
            static Query parseQuery( const QString& query );

            /**
             * \overload
             *
             * \param query The query string to parse
             * \param flags a set of flags influencing the parsing process.
             *
             * \since 4.6
             */
            static Query parseQuery( const QString& query, ParserFlags flags );

        private:
            void addCompletionProposal(CompletionProposal *proposal);

        private:
            class Private;
            Private* const d;
        };
    }
}

Q_DECLARE_OPERATORS_FOR_FLAGS( Nepomuk2::Query::QueryParser::ParserFlags )

#endif
