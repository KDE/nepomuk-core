/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2011 Sebastian Trueg <trueg@kde.org>
   Copyright (C) 2011 Vishesh Handa <handa.vish@gmail.com>

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

#ifndef RESOURCEWATCHERSIGNALTEST_H
#define RESOURCEWATCHERSIGNALTEST_H

#include <QObject>

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

class ResourceWatcherTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();

    void testPropertyAddedSignal();
    void testPropertyRemovedSignal();
    void testResourceRemovedSignal();

    void testStoreResources_createResources();

    // test that make sure each method results in the correct signals
    void testSetProperty();
    void testAddProperty();
    void testRemoveProperty();
    void testRemoveProperties();
    void testRemoveResources();
    void testCreateResources();
    void testRemoveDataByApplication();
    void testRemoveAllDataByApplication();
    void testStoreResources_resourceCreated();
    void testStoreResources_propertyChanged();
    void testStoreResources_typeAdded();
    void testRemoveProperty_typeRemoved();
    void testMergeResources();

private:
    KTempDir* createNieUrlTestData();

    void resetModel();
    bool haveTrailingGraphs() const;

    KTempDir* m_storageDir;
    Soprano::Model* m_model;
    Soprano::NRLModel* m_nrlModel;
    Nepomuk2::ClassAndPropertyTree* m_classAndPropertyTree;
    Nepomuk2::DataManagementModel* m_dmModel;
    Nepomuk2::VirtuosoInferenceModel* m_inferenceModel;
};

#endif
