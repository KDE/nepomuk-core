/*
    <one line to give the library's name and an idea of what it does.>
    Copyright (C) 2012  Vishesh Handa <me@vhanda.in>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/


#ifndef EXTRACTOR_H
#define EXTRACTOR_H

#include <QtCore/QStringList>
#include <QtCore/QDateTime>

#include "simpleresourcegraph.h"
#include "simpleresource.h"
#include "nepomuk_export.h"

#include <KService>

#ifndef NEPOMUK_EXTRACTOR_EXPORT
# if defined(KDELIBS_STATIC_LIBS)
   /* No export/import for static libraries */
#  define NEPOMUK_EXTRACTOR_EXPORT
# elif defined(MAKE_NEPOMUKEXTRACTOR_LIB)
   /* We are building this library */
#  define NEPOMUK_EXTRACTOR_EXPORT KDE_EXPORT
# else
   /* We are using this library */
#  define NEPOMUK_EXTRACTOR_EXPORT KDE_IMPORT
# endif
#endif

namespace Nepomuk2 {

    /**
     * \class ExtractorPlugin extractorplugin.h
     *
     * \brief The ExtractorPlugin is the base class for all file metadata
     * extractors. It is responsible for extracting the metadata and providing
     * it to Nepomuk.
     *
     * Make sure to implement either mimetypes or the shouldExtract function
     * and update the indexingCriteria accordingly
     *
     * \author Vishesh Handa <me@vhanda.in>
     */
    class NEPOMUK_EXTRACTOR_EXPORT ExtractorPlugin : public QObject
    {
        Q_OBJECT
    public:
        ExtractorPlugin(QObject* parent);
        virtual ~ExtractorPlugin();

        /**
        * Each Plugin provides an extracting critera which determines when the
        * plugin should be called.
        */
        enum ExtractingCritera {
            /**
            * This is a simple plugin that just has a list of mimetypes
            * that it supports.
            */
            BasicMimeType = 1,

            /**
            * The plugin implements the determineMimeType function and uses
            * that to determine if the url and mimetype are supported
            */
            Custom = 2
        };

        /**
         * Returns the critera that is being used for determining if this plugin
         * can index the files provided to it.
         *
         * By default this returns BasicMimeType
         *
         * \sa mimetypes
         * \sa shouldExtract
         */
        virtual ExtractingCritera criteria();

        /**
         * By default this returns true if \p mimetype is in the list of
         * mimetypes provided by the plugin.
         *
         * If this function has been reimplemented then the ExtractingCritera should
         * be changed.
         */
        virtual bool shouldExtract(const QUrl& url, const QString& mimeType);

        /**
         * Provide a list of mimetypes which are supported by this plugin.
         * Only files with those mimetypes will be provided to the plugin via
         * the extract function.
         *
         * \return A StringList containing the mimetypes.
         * \sa extract
         */
        virtual QStringList mimetypes();

        /**
         * The main function of the plugin that is responsible for extracting the data
         * from the file url and returning a SimpleResourceGraph.
         *
         * It does so on the basis of the mimetype provided.
         *
         * \param resUri The resource uri of the fileUrl which should be used in the SimpleResource
         * \param fileUrl The url of the file being indexed
         * \param mimeType the mimetype of the file url
         */
        virtual SimpleResourceGraph extract(const QUrl& resUri, const QUrl& fileUrl, const QString& mimeType) = 0;

        //
        // Helper functions
        //

        /**
         * Tries to extract a valid date time from the string provided.
         */
        static QDateTime dateTimeFromString(const QString& dateString);

        /**
         * Creates a list of nco:Contacts from a list of strings which are separated
         * by a number of different separators. It sets the contact's nco:fullname.
         */
        static QList<SimpleResource> contactsFromString(const QString& string);

        /**
         * Virtuoso does not support streaming operators, and does not accept queries
         * above a certain size. Since the plain text has to go in a query, there is
         * no point extracting more than this plain text
         */
        static int maxPlainTextSize();

        static void setMaxPlainTextSize(int size);
        static void resetMaxPlainTextSize();

    private:
        static int s_plainTextSize;
    };
}

/**
 * Export a Nepomuk file extractor.
 *
 * \param classname The name of the subclass to export
 * \param libname The name of the library which should export the extractor
 */
#define NEPOMUK_EXPORT_EXTRACTOR( classname, libname )    \
K_PLUGIN_FACTORY(factory, registerPlugin<classname>();) \
K_EXPORT_PLUGIN(factory(#libname))

#endif // EXTRACTOR_H
