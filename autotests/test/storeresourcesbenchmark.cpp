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


#include "storeresourcesbenchmark.h"
#include "../lib/datagenerator.h"

#include <KDebug>
#include <KTemporaryFile>
#include <KJob>
#include <KTempDir>
#include <qtest_kde.h>

#include "datamanagement.h"
#include "storeresourcesjob.h"

namespace Nepomuk2 {
namespace Test {

void StoreResourcesBenchmark::musicPiece()
{
    DataGenerator gen;
    QBENCHMARK {
        QString title = QUuid::createUuid().toString();
        QString artist = QUuid::createUuid().toString();
        QString album = QUuid::createUuid().toString();

        gen.createMusicFile( title, artist, album );
    }
}

}
}

QTEST_KDEMAIN(Nepomuk2::Test::StoreResourcesBenchmark, NoGUI)
