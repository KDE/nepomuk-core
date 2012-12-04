/*
 * This file is part of the Nepomuk KDE project.
 * Copyright (C) 2006-2009 Sebastian Trueg <trueg@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _NEPOMUK2_RESOURCE_MANAGER_H_
#define _NEPOMUK2_RESOURCE_MANAGER_H_

#include "nepomuk_export.h"

#include <QtCore/QObject>
#include <QtCore/QUrl>


namespace Soprano {
    class Model;
}

namespace Nepomuk2 {
    class Resource;
    class Variant;
    class ResourceManagerHelper;
    class ResourceManagerPrivate;
    namespace Types {
        class Class;
        class Property;
    }

    /**
     * \class ResourceManager resourcemanager.h Nepomuk2/ResourceManager
     *
     * \brief The ResourceManager is the central \a %Nepomuk configuration point.
     *
     * Use the initialized() method to check the availabity of the %Nepomuk system.
     * Signals nepomukSystemStarted() and nepomukSystemStopped() can be used to
     * enable or disable Nepomuk-specific GUI elements.
     *
     * \author Sebastian Trueg <trueg@kde.org>
     */
    class NEPOMUK_EXPORT ResourceManager : public QObject
    {
        Q_OBJECT

    public:
        static ResourceManager* instance();

        /**
         * Initialize the Nepomuk framework. This method will initialize the communication with
         * the local Nepomuk-KDE services, ie. the data repository. It will trigger a reconnect
         * to the %Nepomuk database.
         *
         * There is normally no reason to call this method manually except when using multiple
         * threads. In that case it is highly recommended to call this method in the main thread
         * before doing anything else.
         *
         * \return 0 if all necessary components could be found and -1 otherwise.
         */
        int init();

        /**
         * \return true if init() has been called successfully, ie. the KMetaData system is connected
         * to the local RDF repository service and ready to work.
         */
        bool initialized() const;

        /**
         * Retrieve the main data storage model.
         */
        Soprano::Model* mainModel();

        /**
         * Override the main model used for all storage. By default the main model
         * used is the Nepomuk server main model.
         *
         * \param model The model to use instead of the Nepomuk server or 0 to reset.
         *
         * \since 4.1
         */
        void setOverrideMainModel( Soprano::Model* model );

        /**
         * Remove the resource denoted by \a uri completely.
         *
         * This method is just a wrapper around Resource::remove. The result
         * is the same.
         */
        void removeResource( const QString& uri );

        /**
         * Generates a unique URI that is not used in the store yet. This method can be used to
         * generate URIs for virtual types such as Tag.
         *
         * \param label A label that the algorithm should use to try to create a more readable URI.
         *
         * \return A new unique URI which can be used to define a new resource.
         *
         * \since 4.2
         */
        QUrl generateUniqueUri( const QString& label );

        /**
         * \internal Non-public API. Used by Resource to signalize errors.
         */
        void notifyError( const QString& uri, int errorCode );

    Q_SIGNALS:
        /**
         * This signal gets emitted whenever a Resource changes due to a sync procedure.
         * Be aware that modifying resources locally via the Resource::setProperty method
         * does not result in a resourceModified signal being emitted.
         *
         * \param uri The URI of the modified resource.
         *
         * NOT IMPLEMENTED YET
         */
        void resourceModified( const QString& uri );

        /**
         * Whenever a problem occurs (like for example failed resource syncing) this
         * signal is emitted.
         *
         * \param uri The resource related to the error.
         * \param errorCode The type of the error (Resource::ErrorCode)
         */
        void error( const QString& uri, int errorCode );

        /**
         * Emitted once the Nepomuk system is up and can be used.
         *
         * \warning This signal will not be emitted if the Nepomuk
         * system is running when the ResourceManager is created.
         * Use initialized() to check the status.
         *
         * \since 4.4
         */
        void nepomukSystemStarted();

        /**
         * Emitted once the Nepomuk system goes down.
         *
         * \since 4.4
         */
        void nepomukSystemStopped();

    private Q_SLOTS:
        void slotPropertyAdded(const Nepomuk2::Resource &res, const Nepomuk2::Types::Property &prop, const QVariant &value);
        void slotPropertyRemoved(const Nepomuk2::Resource &res, const Nepomuk2::Types::Property &prop, const QVariant &value);

        /**
         * This slot is called when the application is about to quit in order to delete all the
         * ResourceData* and render the Resource classes useless. This help to avoid crashes
         * by an application trying to access a Resource when the application is quiting.
         */
        void cleanupResources();

    private:
        friend class Nepomuk2::Resource;
        friend class Nepomuk2::ResourceManagerPrivate;

        ResourceManager();
        ~ResourceManager();

        static ResourceManager* s_instance;

        ResourceManagerPrivate* const d;

        Q_PRIVATE_SLOT( d, void _k_storageServiceInitialized(bool) )
        Q_PRIVATE_SLOT( d, void _k_dbusServiceUnregistered(QString) )
    };
}

#endif
