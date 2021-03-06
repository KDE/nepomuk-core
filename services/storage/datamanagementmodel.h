/*
   This file is part of the Nepomuk KDE project.
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

#ifndef DATAMANAGEMENTMODEL_H
#define DATAMANAGEMENTMODEL_H

#include "datamanagement.h"

#include <Soprano/FilterModel>

#include <QtCore/QDateTime>

namespace Nepomuk2 {

class ClassAndPropertyTree;
class ResourceMerger;
class SimpleResourceGraph;
class ResourceWatcherManager;
class TypeCache;

class DataManagementModel : public Soprano::FilterModel
{
    Q_OBJECT

public:
    DataManagementModel(ClassAndPropertyTree* tree, Soprano::Model* model, QObject *parent = 0);
    ~DataManagementModel();

    /// used by the unit tests
    ResourceWatcherManager* resourceWatcherManager() const;

public Q_SLOTS:
    /**
     * \name Basic API
     */
    //@{
    /**
     * Add \p property with \p values to each resource
     * from \p resources. Existing values will not be touched.
     * If a cardinality is breached an error will be thrown.
     */
    void addProperty(const QList<QUrl>& resources,
                     const QUrl& property,
                     const QVariantList& values,
                     const QString& app);

    /**
     * Set, ie. overwrite properties. Set \p property with
     * \p values for each resource from \p resources. Existing
     * values will be replaced.
     */
    void setProperty(const QList<QUrl>& resources,
                     const QUrl& property,
                     const QVariantList& values,
                     const QString& app);

    /**
     * Remove the property \p property with \p values from each
     * resource in \p resources.
     */
    void removeProperty(const QList<QUrl>& resources,
                        const QUrl& property,
                        const QVariantList& values,
                        const QString& app);

    /**
     * Remove all statements involving any proerty from \p properties from
     * all resources in \p resources.
     */
    void removeProperties(const QList<QUrl>& resources,
                          const QList<QUrl>& properties,
                          const QString& app);

    /**
     * Create a new resource with several \p types.
     */
    QUrl createResource(const QList<QUrl>& types,
                        const QString& label,
                        const QString& description,
                        const QString& app);

    /**
     * Remove resources from the database.
     * \param resources The URIs of the resources to be removed.
     * \param app The calling application.
     * \param force Force deletion of the resource and all sub-resources.
     * If false sub-resources will be kept if they are still referenced by
     * other resources.
     */
    void removeResources(const QList<QUrl>& resources,
                         Nepomuk2::RemovalFlags flags,
                         const QString& app);
    //@}

    /**
     * \name Advanced API
     */
    //@{
    /**
     * Remove all information about resources from the database which
     * have been created by a specific application.
     * \param resources The URIs of the resources to be removed.
     * \param app The application for which data should be removed.
     * \param force Force deletion of the resource and all sub-resources.
     * If false sub-resources will be kept if they are still referenced by
     * other resources.
     */
    void removeDataByApplication(const QList<QUrl>& resources,
                                 RemovalFlags flags,
                                 const QString& app);

    /**
     * Remove all information from the database which
     * has been created by a specific application.
     * \param app The application for which data should be removed.
     * \param force Force deletion of the resource and all sub-resources.
     * If false sub-resources will be kept if they are still referenced by
     * resources that have been created by other applications.
     */
    void removeDataByApplication(RemovalFlags flags,
                                 const QString& app);

    /**
     * \param resources The resources to be merged. Blank nodes will be converted into new
     * URIs (unless the corresponding resource already exists).
     * \param identificationMode This method can try hard to avoid duplicate resources by looking
     * for already existing duplicates based on nrl:DefiningProperty. By default it only looks
     * for duplicates of resources that do not have a resource URI (SimpleResource::uri()) defined.
     * This behaviour can be changed with this parameter.
     * \param flags Additional flags to change the behaviour of the method.
     * \param additionalMetadata Additional metadata for the added resources. This can include
     * such details as the creator of the data or details on the method of data recovery.
     * One typical usecase is that the file indexer uses (rdf:type, nrl:DiscardableInstanceBase)
     * to state that the provided information can be recreated at any time. Only built-in types
     * such as int, string, or url are supported.
     * \note Due to performance concerns, currently only (rdf:type, nrl:DiscardableInstanceBase)
     * is considered as additionalMetadata. The rest is ignored.
     *
     * \param app The calling application
     */
    QHash<QUrl,QUrl> storeResources(const SimpleResourceGraph& resources,
                        const QString& app,
                        Nepomuk2::StoreIdentificationMode identificationMode = Nepomuk2::IdentifyNew,
                        Nepomuk2::StoreResourcesFlags flags = Nepomuk2::NoStoreResourcesFlags,
                        const QHash<QUrl, QVariant>& additionalMetadata = (QHash<QUrl, QVariant>()) );

    /**
     * \param resources The resources to be merged. Blank nodes will be converted into new
     * URIs (unless the corresponding resource already exists).
     * \param identificationMode This method can try hard to avoid duplicate resources by looking
     * for already existing duplicates based on nrl:DefiningProperty. By default it only looks
     * for duplicates of resources that do not have a resource URI (SimpleResource::uri()) defined.
     * This behaviour can be changed with this parameter.
     * \param flags Additional flags to change the behaviour of the method.
     * \param discardable Indicates if the data being stored is discardable. This is typically used
     * in the file indexer so that the rdf:type nrl:DiscardableInstanceBase is applied.
     * \param app The calling application
     */
    QHash<QUrl,QUrl> storeResources(const SimpleResourceGraph& resources,
                        const QString& app,
                        bool discardable,
                        Nepomuk2::StoreIdentificationMode identificationMode = Nepomuk2::IdentifyNew,
                        Nepomuk2::StoreResourcesFlags flags = Nepomuk2::NoStoreResourcesFlags);
    /**
     * Merges all the resources into one.
     * Properties from the first resource in \p resources take precedence over all other resources
     * present in \p resources
     */
    void mergeResources(const QList<QUrl>& resources, const QString& app);

    /**
     * Import an RDF graph from a URL.
     * \param url The url from which the graph should be loaded. This does not have to be local.
     * \param serialization The RDF serialization used for the file. If Soprano::SerializationUnknown a crude automatic
     * detection based on file extension is used.
     * \param userSerialization If \p serialization is Soprano::SerializationUser this value is used. See Soprano::Parser
     * for details.
     * \param identificationMode This method can try hard to avoid duplicate resources by looking
     * for already existing duplicates based on nrl:DefiningProperty. By default it only looks
     * for duplicates of resources that do not have a resource URI (SimpleResource::uri()) defined.
     * This behaviour can be changed with this parameter.
     * \param flags Additional flags to change the behaviour of the method.
     * \param additionalMetadata Additional metadata for the added resources. This can include
     * such details as the creator of the data or details on the method of data recovery.
     * One typical usecase is that the file indexer uses (rdf:type, nrl:DiscardableInstanceBase)
     * to state that the provided information can be recreated at any time. Only built-in types
     * such as int, string, or url are supported.
     * \param app The calling application
     */
    void importResources(const QUrl& url, const QString& app,
                         Soprano::RdfSerialization serialization,
                         const QString& userSerialization = QString(),
                         Nepomuk2::StoreIdentificationMode identificationMode = Nepomuk2::IdentifyNew,
                         Nepomuk2::StoreResourcesFlags flags = Nepomuk2::NoStoreResourcesFlags,
                         const QHash<QUrl, QVariant>& additionalMetadata = (QHash<QUrl, QVariant>()) );

    /**
     * Describe a set of resources, i.e. retrieve all their properties.
     * \param resources The resource URIs of the resources to describe. Non-existing resources are ignored.
     * \param flags Optional flags to modify the data which is returned.
     * \param targetParties This optional list can be used to specify the parties (nao:Party) which should
     * receive the returned data. This will result in a filtering of the result according to configured
     * permissions. Only data which is set as being public or readable by the specified parties is returned.
     */
    SimpleResourceGraph describeResources(const QList<QUrl>& resources,
                                          DescribeResourcesFlags flags = NoDescribeResourcesFlags,
                                          const QList<QUrl>& targetParties = QList<QUrl>() );

    /**
     * Export a set of resources, i.e. retrieve their properties.
     * \param resources The resource URIs of the resources to describe. Non-existing resources are ignored.
     * \param serialization The RDF serialization used for the result.
     * \param userSerialization If \p serialization is Soprano::SerializationUser this value is used. See Soprano::Parser
     * for details.
     * \param flags Optional flags to modify the data which is returned.
     * \param targetParties This optional list can be used to specify the parties (nao:Party) which should
     * receive the returned data. This will result in a filtering of the result according to configured
     * permissions. Only data which is set as being public or readable by the specified parties is returned.
     *
     * \return A serialized representation of the requested resources.
     *
     * \sa describeResources
     */
    QString exportResources(const QList<QUrl>& resources,
                            Soprano::RdfSerialization serialization,
                            const QString& userSerialization = QString(),
                            DescribeResourcesFlags flags = NoDescribeResourcesFlags,
                            const QList<QUrl>& targetParties = QList<QUrl>() );
    //@}

    /**
     * Clear the internal cache present in the model
     */
    void clearCache();

    TypeCache* typeCache();

    QUrl nepomukGraph();
private:
    QUrl createNepomukGraph();
    QUrl createGraph(const QString& app, const QMultiHash<QUrl, Soprano::Node>& additionalMetadata);
    QUrl fetchGraph(const QString& app, bool discardable = false);

    QUrl findApplicationResource(const QString& app, bool create = true);

    /**
     * Updates the modification date of \p resource to \p date.
     * Adds the new statement in the nepomuk graph
     */
    Soprano::Error::ErrorCode updateModificationDate( const QUrl& resource,
                                                      const QDateTime& date = QDateTime::currentDateTime() );

    /**
     * Updates the modification date of \p resources to \p date.
     * Adds the new statement in the nepomuk graph
     */
    Soprano::Error::ErrorCode updateModificationDate( const QSet<QUrl>& resources,
                                                      const QDateTime& date = QDateTime::currentDateTime() );

    /**
     * Adds for each resource in \p resources a property for each node in nodes. \p nodes cannot be empty.
     * This method is used in the public setProperty and addProperty slots to avoid a lot of code duplication.
     *
     * \param resources A list of resource uris
     * \param property The property to use. This cannot be empty.
     * \param nodes A list of objects
     * \param app The calling application.
     *
     * \return A mapping from changed resources to actually newly added values.
     */
    QHash<QUrl, QList<Soprano::Node> > addProperty(const QList<QUrl>& resources, const QUrl& property,
                                                   const QList<Soprano::Node>& nodes, const QString& app,
                                                   bool signalPropertyChanged = false);

    /**
     * Removes the given resources without any additional checks. The provided list needs to contain already resolved valid resource URIs.
     *
     * Used by removeResources() and removeDataByApplication()
     */
    void removeAllResources(const QSet<QUrl>& resourceUris, RemovalFlags flags);

    /**
     * Checks if resource \p res actually exists. A resource exists if any information other than the standard metadata
     * (nao:created, nao:creator, nao:lastModified, nao:userVisible) or the nie:url is defined.
     */
    bool doesResourceExist(const QUrl& res, const QUrl& graph = QUrl()) const;

    /**
     * Resolves a local file url to its resource URI. Returns \p url if it is not a file URL and
     * an empty QUrl in case \p url is a file URL but has no resource URI in the model.
     *
     * \param statLocalFiles If \p true the method will check if local files exist and set an error
     * if not.
     */
    QUrl resolveUrl(const QUrl& url, bool statLocalFiles = false);

    /**
     * Resolves local file URLs through nie:url.
     * \return a list of urls which contain the actual resource uris. If the resource uri does not exist
     *         it is created
     *
     * \param statLocalFiles If \p true this method does check if the local file exists and may set an error if not.
     */
    QList<QUrl> resolveUrls(const QList< QUrl >& urls, const QString& app, bool statLocalFiles = true);

    /**
     * Resolves local file URLs through nie:url.
     *
     * This method does check if the local file exists and may set an error.
     */
    QList<Soprano::Node> resolveNodes(const QSet<Soprano::Node>& nodes, const QString& app);

    QList<QUrl> createFileResources(const QList<QUrl>& nieUrls, const QUrl& graph);

    /**
     * Updates the nie:url of a local file resource.
     * \return \p true if the url has been updated and nothing else needs to be done, \p false
     * if the update was not handled. An error also results in a return value of \p true.
     *
     * \param resource The resource to update. Both file URLs and resource URIs are supported. Thus, there is no need to resolve the URL
     * before calling this method.
     * \param nieUrl The new nie:url to assign to the resource.
     */
    bool updateNieUrlOnLocalFile(const QUrl& resource, const QUrl& nieUrl);

    ClassAndPropertyTree * classAndPropertyTree();

    enum UriType {
        GraphUri,
        ResourceUri
    };
    QUrl createUri(UriType type);

    bool isProtectedProperty(const QUrl& prop) const;

    class Private;
    Private* const d;

    friend class ResourceMerger;
};
}

#endif
