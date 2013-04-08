/*
 * <one line to give the library's name and an idea of what it does.>
 * Copyright 2013  Vishesh Handa <me@vhanda.in>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef NEPOMUK2_BACKUP_RESOURCELISTGENERATOR_H
#define NEPOMUK2_BACKUP_RESOURCELISTGENERATOR_H

#include <QtCore/QUrl>
#include <KJob>

#include <Soprano/Model>

namespace Nepomuk2 {
namespace Backup {

/**
 * This class generates a list of resource URIs that should be
 * backed up. This is generally used in the first stage of the
 * backup process.
 */
class ResourceListGenerator : public KJob
{
    Q_OBJECT
public:
    ResourceListGenerator(Soprano::Model* model, const QString& outputFile, QObject* parent = 0);

    virtual void start();

    enum Filter {
        Filter_None,
        Filter_FilesAndTags
    };

    void setFilter(Filter filter);
private slots:
    void doJob();

private:
    Soprano::Model *m_model;
    QString m_outputFile;

    Filter m_filter;
};

}
}

#endif // NEPOMUK2_BACKUP_RESOURCELISTGENERATOR_H
