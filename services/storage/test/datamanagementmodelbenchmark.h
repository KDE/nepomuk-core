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

#ifndef DATAMANAGEMENTMODELBENCHMARK_H
#define DATAMANAGEMENTMODELBENCHMARK_H

#include <QtCore/QObject>

namespace Soprano {
class Model;
class NRLModel;
}
namespace Nepomuk2 {
class DataManagementModel;
class ClassAndPropertyTree;
class VirtuosoInferenceModel;
}
class KTempDir;

class DataManagementModelBenchmark : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

    void addProperty();
    void addProperty_sameData();

    void setProperty();
    void setProperty_sameData();

    void storeResources();
    void storeResources_email();

    void createResource();
    void removeResources();

    void removeDataByApplication();
    void removeDataByApplication_subResources();

    void removeAllDataByApplication();
private:
    void resetModel();

    KTempDir* m_storageDir;
    Soprano::Model* m_model;
    Soprano::NRLModel* m_nrlModel;
    Nepomuk2::VirtuosoInferenceModel* m_inferenceModel;
    Nepomuk2::ClassAndPropertyTree* m_classAndPropertyTree;
    Nepomuk2::DataManagementModel* m_dmModel;
};

#endif // DATAMANAGEMENTMODELBENCHMARK_H
