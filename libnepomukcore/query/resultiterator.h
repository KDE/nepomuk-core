/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2012 Vishesh Handa <me@vhanda.in>

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

#ifndef NEPOMUK_QUERY_RESULTITERATOR_H
#define NEPOMUK_QUERY_RESULTITERATOR_H

#include "query.h"
#include "result.h"

namespace Nepomuk2 {

    namespace Query {

        /**
         * \class ResultIterator resultiterator.h Nepomuk2/Query/ResultIterator
         *
         * \brief A class to iterate over Nepomuk query results
         *
         * Can be used to execute a Nepomuk Query or a traditional sparql query. It returns
         * a Query::Result object which contains an additionalBinding parameter in the case
         * of a sparql query.
         *
         * \author Vishesh Handa <me@vhanda.in>
         *
         * \since 4.10
         */
        class NEPOMUK_EXPORT ResultIterator
        {
        public:
            /**
             * Constructor used to run sparql queries.
             *
             * \param map The RequestPropertyMap is purely optional and is used if you want to
             * automatically set the requestProperties of the result. It maps the
             * bindings name to the actual property.
             */
            ResultIterator(const QString& sparql, const RequestPropertyMap& map = RequestPropertyMap() );

            /**
             * Default constructor to run the Query and get the iterator
             */
            ResultIterator(const Query& query);
            ~ResultIterator();

            /**
             * Get the next result
             *
             * \return \p true if there are any more results and \p false otherwise
             */
            bool next();

            /**
             * Check if the iterator is in a valid state.
             *
             * \return \p true if the iterator is valid
             */
            bool isValid() const;

            // Result fetching
            Result result() const;
            Result current() const;
            Result operator*() const;

        private:
            class Private;
            Private* d;
        };

    }

}

#endif // NEPOMUK_QUERY_RESULTITERATOR_H
