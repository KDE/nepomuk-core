/* 
 *
 * $Id: sourceheader 511311 2006-02-19 14:51:05Z trueg $
 *
 * This file is part of the Nepomuk KDE project.
 * Copyright (C) 2006 Sebastian Trueg <trueg@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING" for the exact licensing terms.
 */

#ifndef _NEPOMUK_RESOURCE_MANAGER_H_
#define _NEPOMUK_RESOURCE_MANAGER_H_

#include <kmetadata/kmetadata_export.h>

#include <QtCore>

#include <krandom.h>


namespace Nepomuk {
    namespace Backbone {
	class Registry;
    }

    namespace KMetaData {

	class Resource;
	class Ontology;
	class Variant;

	// FIXME: introduce a simple overwrite mode which is way faster and doe not merge with the store

	/**
	 * \brief The ResourceManager is the central \a %KMetaData configuration point.
	 *
	 * For now it only provides the possibility to disbable auto syncing and get informed 
	 * of Resource changes via the resourceModified signal.
	 *
	 * At the moment auto syncing is not complete and will only write back changes once 
	 * all instances of a Resource have been deleted. In the future auto syncing will try
	 * to keep in sync with external changes also.
	 */
	class KMETADATA_EXPORT ResourceManager : public QObject
	    {
		Q_OBJECT

	    public:
		~ResourceManager();

		static ResourceManager* instance();

		/**
		 * Initialize the KMetaData framework. This method will initialize the communication with
		 * the local Nepomuk-KDE services, ie. the RDF repository.
		 *
		 * KMetaData cannot be used before a successful call to init.
		 *
		 * \return 0 if all necessary components could be found and -1 otherwise.
		 *
		 * FIXME: introduce error codes and human readable translated error messages.
		 */
		int init();

		/**
		 * \return true if init() has been called successfully, ie. the KMetaData system is connected
		 * to the local RDF repository service and ready to work.
		 */
		bool initialized() const;

		/**
		 * The Ontology instance representing the underlying Nepomuk desktop ontology.
		 */
		Ontology* ontology() const;

		/**
		 * The NEPOMUK Service Registry used.
		 */
		Backbone::Registry* serviceRegistry() const;

		/**
		 * \return true if autosync is enabled
		 */
		bool autoSync() const;

		/**
		 * Creates a Resource object representing the data referenced by \a uri.
		 * The result is the same as from using the Resource::Resource( const QString&, const QString& )
		 * constructor with an empty type.
		 *
		 * \return The Resource representing the data at \a uri or an invalid Resource object if the local
		 * NEPOMUK RDF store does not contain an object with URI \a uri.
		 */
		Resource createResourceFromUri( const QString& uri );

		/**
		 * Remove the resource denoted by \a uri completely.
		 *
		 * This method is just a wrapper around Resource::remove. The result
		 * is the same.
		 */
		void removeResource( const QString& uri );

		/**
		 * Retrieve a list of all resources of the specified \a type.
		 *
		 * This includes Resources that are not synced yet so it might
		 * not represent exactly the state as in the RDF store.
		 */
		QList<Resource> allResourcesOfType( const QString& type ) const;

		/**
		 * Retrieve a list of all resources that have property \a uri defined with a value of \a v.
		 *
		 * This includes Resources that are not synced yet so it might
		 * not represent exactly the state as in the RDF store.
		 *
		 * \param uri The URI identifying the property. If this URI does
		 *            not include a namespace the default namespace is
		 *            prepended.
		 * \param v The value all returned resources should have set as properts \a uri.
		 *
		 * \sa Ontology::defaultNamespace
		 */
		QList<Resource> allResourcesWithProperty( const QString& uri, const Variant& v ) const;

		/**
		 * Generates a unique URI that is not used in the store yet. This method ca be used to 
		 * generate URIs for virtual types such as Tag.
		 */
		QString generateUniqueUri() const;

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

		// FIXME: add a loggin mechanism that reports successfully and failed sync operations and so on

	    public Q_SLOTS:
		/**
		 * Enable or disable autosync. If autosync is enabled (which is the default)
		 * all Resource objects will be synced with the local storage automatically.
		 *
		 * FIXME: Hint that the resources will not be actually deleted from memory before
		 * they have been synced. Thus, if auto sync is disabled the memory might get crowded
		 * if sync is never called. Exception: unmodified resources.
		 */
		void setAutoSync( bool enabled );

		/**
		 * Sync all Resource objects. There is no need to call this
		 * unless autosync has been disabled.
		 *
		 * This is a blocking call which syncs all modifed data to the store.
		 */
		void syncAll();

		/**
		 * This method can be used in combination with auto syncing and forces an immediate
		 * sync in the background. If an asyncroneous sync is currently in progress this method
		 * does nothing.
		 */
		void triggerSync();

	    private Q_SLOTS:
		void slotStartAutoSync();

	    private:
		ResourceManager();

		class Private;
		Private* d;
	    };
    }
}

#endif
