/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2012  Vishesh Handa <me@vhanda.in>
   Copyright (C) 2013 Sebastian Trueg <trueg@kde.org>

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

#ifndef NEPOMUK2_CLEANINGJOB_H
#define NEPOMUK2_CLEANINGJOB_H

#include <KJob>
#include <KLocalizedString>
#include <QtCore/QUrl>
#include <KService>

#include "nepomuk_export.h"

namespace Nepomuk2 {
    class NEPOMUK_EXPORT CleaningJob : public KJob
    {
        Q_OBJECT
    public:
        explicit CleaningJob(QObject* parent = 0);
        virtual ~CleaningJob();

        virtual void start();
        virtual QString jobName() = 0;

        void quit();

    protected:
        bool shouldQuit();

    private slots:
        void slotStartExecution();

    private:
        virtual void execute() = 0;

        bool m_shouldQuit;
    };
} // namespace Nepomuk2

/**
 * Export a Nepomuk cleaner job.
 *
 * \param classname The name of the subclass to export
 * \param libname The name of the library which should export the extractor
 */
#define NEPOMUK_EXPORT_CLEANINGJOB( classname, libname )    \
K_PLUGIN_FACTORY(factory, registerPlugin<classname>();) \
K_EXPORT_PLUGIN(factory(#libname))

#endif // NEPOMUK2_CLEANINGJOB_H
