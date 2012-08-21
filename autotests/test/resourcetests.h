/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) <year>  <name of author>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef RESOURCETESTS_H
#define RESOURCETESTS_H

#include <QtCore/QObject>
#include "../lib/testbase.h"

namespace Nepomuk2 {

class ResourceTests : public Nepomuk2::TestBase
{
    Q_OBJECT

private Q_SLOTS:
    // Required Tests -
    // 1. New Resources -
    //    a. Make sure you can create tags
    //    b. Make sure you can create stuff with custom types
    //    c. Make sure you can create files

    void newTag();
    void newContact();
    void newFile();
    void newFolder();
    void newResourceMetaProperties();

    // 2. Existing resources
    //    a. Can identify existing tags
    //    b. Can identify existing nepomuk uris
    //    c. Can identify files

    void existingTag();
    void existingContact();
    void existingFile();

    // 3. Properties
    //    a. Make sure you can set/add/remove properties
    //    Checked by other tests

    // 4. Initialization
    //    a. Init via tag - Change tag name, and see if the old tag exists
    //    b. Init via url - Change url, and same check

    void initTagChange();
    void initUrlChange();

    // 5. Type checks -
    //    a. Make sure the top most type is being given
    //    b. Make sure pimo type is returned when present
    //    c. Make sure tags have been given the correct type
    //    d. Make sure files and folders have the correct type

    void typeTopMost();
    void typePimo();
    // c and d are checking newTag, newFile and newFolder

    // 6. Resource Watcher
    //    a. Make sure tags are updated - Also cross check initialization
    //    b. Check if the meta properties are updated
    //    c. Make sure direct properties like ratings
    //    d. Make sure "new resources" are updated
    //    e. nao:identifier kickoff list
    //    f. nie:url kickoff list

    void tagsUpdate();
    void metaPropertiesUpdate();
    void ratingUpdate();
    void newResourcesUpdated();
    void identifierUpdate();
    void urlUpdate();

    // 7. Resource Deletion
    //
    void resourceDeletion();
};

}
#endif // RESOURCETESTS_H
