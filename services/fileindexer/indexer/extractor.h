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

#include "simpleresourcegraph.h"
#include "simpleresource.h"
#include "nepomuk_export.h"

#include <KService>

namespace Nepomuk2 {

    class NEPOMUK_EXPORT Extractor : public QObject
    {
        Q_OBJECT
    public:
        Extractor(QObject* parent);
        virtual ~Extractor();

        virtual QStringList mimetypes() = 0;
        virtual SimpleResourceGraph extract(const QUrl& resUri, const QUrl& fileUrl, const QString& mimeType) = 0;
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
