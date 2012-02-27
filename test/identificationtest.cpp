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


#include "identificationtest.h"

#include <KDebug>
#include <QtCore/QProcess>

#include <kdebug.h>
#include <qtest_kde.h>

#include <KTempDir>
#include <KTemporaryFile>
#include <KJob>
#include <KStandardDirs>

#include <Nepomuk/Resource>
#include <Nepomuk/ResourceManager>
#include <Nepomuk/Tag>

#include <Soprano/Model>
#include <Soprano/StatementIterator>
#include <Soprano/Vocabulary/NAO>
#include <Nepomuk/Vocabulary/NIE>

#include "nepomuk/datamanagement.h"

using namespace Soprano::Vocabulary;
using namespace Nepomuk::Vocabulary;

void NepomukSyncTests::basicIdentification()
{
    Soprano::Model *model = Nepomuk::ResourceManager::instance()->mainModel();
    kDebug() << "num: " << model->statementCount();

    KTemporaryFile file;
    QVERIFY(file.open());

    QUrl fileUrl = KUrl(file.fileName());
    kDebug() << "File Url: " << fileUrl;

    Nepomuk::Resource res( fileUrl );
    res.setRating( 5 );

    QVERIFY( res.exists() );
    QVERIFY( res.rating() == 5 );

    KJob* job = Nepomuk::setProperty( QList<QUrl>() << fileUrl, NAO::numericRating(), QVariantList() << 2 );
    job->exec();

    if( job->error() ) {
        kWarning() << job->errorString();
    }

    QVERIFY( res.rating() == 2 );

    resetRepository();

    QVERIFY( !res.exists() );
}

QTEST_KDEMAIN(NepomukSyncTests, NoGUI)
