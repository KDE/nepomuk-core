/*
  Copyright (c) 2010 Oszkar Ambrus <aoszkar@gmail.com>
  Copyright (C) 2010 Sebastian Trueg <trueg@kde.org>

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

#ifndef RESOURCESEARCHWIDGET_H
#define RESOURCESEARCHWIDGET_H

#include <QWidget>
#include <QAbstractItemView>

#include "query.h"
#include "nepomukutils_export.h"

namespace Nepomuk {
    class Resource;

    namespace Utils {
        /**
         * \class SearchWidget searchwidget.h Nepomuk/Utils/SearchWidget
         *
         * \brief Provides a GUI for searching files or resources of any type.
         *
         * The SearchWidget combines the different search GUI elements provided in
         * Nepomuk in one widget: a line edit which allows to type in a query and
         * the facet widget.
         *
         * The SearchWidget allows to specify a base query which is fixed and cannot
         * be changed by the user. This allows to restrict the set of resources searched
         * by the user.
         *
         * \author Oszkar Ambrus <aoszkar@gmail.com>, Sebastian Trueg <trueg@kde.org>
         *
         * \since 4.6
         */
        class NEPOMUKUTILS_EXPORT SearchWidget : public QWidget
        {
            Q_OBJECT

        public:
            /**
             * Constructor.
             */
            SearchWidget(QWidget *parent = 0);

            /**
             * Destructor
             */
            ~SearchWidget();

            /**
             * The config flags can be used to configure
             * the search widget to fit the needs of the application.
             */
            enum ConfigFlag {
                /// no flags, a plain search widget
                NoConfigFlags = 0x0,

                /// show the facets allowing the user to modify the query
                ShowFacets = 0x1,

                /// enable auto searching while the user is typing in the query (live query)
                SearchWhileYouType = 0x2,

                /// the default: show facets and enable live query
                DefaultConfigFlags = ShowFacets|SearchWhileYouType
            };
            Q_DECLARE_FLAGS( ConfigFlags, ConfigFlag )

            /**
             * Set the config flags to be used. Defaults to DefaultConfigFlags.
             *
             * \sa configFlags()
             */
            void setConfigFlags( ConfigFlags flags );

            /**
             * The config flags set via setConfigFlags().
             */
            ConfigFlags configFlags() const;

            /**
             * Sets the selection mode of the view to @p mode
             */
            void setSelectionMode(QAbstractItemView::SelectionMode mode);

            /**
             * @returns the selection mode of the view
             */
            QAbstractItemView::SelectionMode selectionMode () const;

            /**
             * @returns the base query set via setBaseQuery().
             */
            Query::Query baseQuery() const;

            /**
             * Construct the query currently used by this widget including
             * the baseQuery(), all facets, and the user desktop query.
             */
            Query::Query query() const;

            /**
             * @returns the current resource if any, an invalid resource if none is selected
             */
            Resource currentResource() const;

            /**
             * @returns a list of all selected resources.
             */
            QList<Resource> selectedResources() const;

            /**
             * Creates a dialog embedding a SearchWidget that allows the user to select one resource from the
             * result set.
             *
             * \return The Resource the user selected or an invalid one in case there was no result to choose
             * or the user canceled the search.
             */
            static Nepomuk::Resource searchResource( QWidget* parent = 0,
                                                     const Nepomuk::Query::Query& baseQuery = Nepomuk::Query::Query(),
                                                     SearchWidget::ConfigFlags flags = SearchWidget::DefaultConfigFlags );

            /**
             * Creates a dialog embedding a SearchWidget that allows the user to select resources from the
             * result set.
             *
             * \return The resources the user selected or an empty list in case there was no result to choose
             * or the user canceled the search.
             */
            static QList<Nepomuk::Resource> searchResources( QWidget* parent = 0,
                                                             const Nepomuk::Query::Query& baseQuery = Nepomuk::Query::Query(),
                                                             SearchWidget::ConfigFlags flags = SearchWidget::DefaultConfigFlags );

        public Q_SLOTS:
            /**
             * Set the query currently configured in the widget. Parts that cannot be converted into
             * facets or a user desktop query string are set as the baseQuery().
             *
             * \sa setQueryTerm()
             */
            void setQuery( const Nepomuk::Query::Query& query );

            /**
             * Set the user changable part of the currently configured query to \p term.
             * Contrary to setQuery() this will not change the baseQuery() but return the
             * part of \p term that cannot be converted into facets or a user desktop query string.
             */
            Nepomuk::Query::Term setQueryTerm( const Nepomuk::Query::Term& term );

            /**
             * Set the base query. The base query is the fixed part of the query
             * which cannot be changed by the user. It allows to restrict the
             * searched set of resources.
             *
             * Be default the base query is empty, ie. an invalid Query.
             */
            void setBaseQuery( const Nepomuk::Query::Query& query );

        Q_SIGNALS:
            /**
             * Emitted when the selection is changed, ie. the values returned by currentResource() and
             * selectedResources() have changed.
             */
            void selectionChanged();

        private:
            class SearchWidgetPrivate;
            SearchWidgetPrivate * const d_ptr;

            Q_DECLARE_PRIVATE(SearchWidget)

            Q_PRIVATE_SLOT( d_ptr, void _k_queryComponentChanged() )
            Q_PRIVATE_SLOT( d_ptr, void _k_listingFinished() )
        };
    }
}

Q_DECLARE_OPERATORS_FOR_FLAGS( Nepomuk::Utils::SearchWidget::ConfigFlags )

#endif // RESOURCESEARCHWIDGET_H
