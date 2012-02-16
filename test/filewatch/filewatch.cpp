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


#include "filewatch.h"

#include <KDebug>

#include <kdebug.h>
#include <ktemporaryfile.h>
#include <qtest_kde.h>

#include <KTempDir>
#include <KTemporaryFile>

#include <Nepomuk/Resource>
#include <Nepomuk/ResourceManager>
#include <Nepomuk/Tag>
#include <Soprano/Model>
#include <Soprano/StatementIterator>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <Nepomuk/Vocabulary/NIE>
#include <Nepomuk/Variant>

FileWatchTest::FileWatchTest(QObject* parent)
: TestBase(parent)
{
    startServiceAndWait("nepomukfilewatch");
}


namespace {
    // It's hard to believe that Qt doesn't provide its own copy folder function
    void copyFolder(QString sourceFolder, QString destFolder)
    {
        
        QDir sourceDir(sourceFolder);
        if(!sourceDir.exists()) {
            kDebug() << sourceFolder << " doesn't exist!";
            return;
        }
        
        QDir destDir(destFolder);
        if(!destDir.exists()) {
            kDebug() << destFolder << " doesn't exist! Creating it!";
            destDir.mkdir(destFolder);
        }
        
        QStringList files = sourceDir.entryList(QDir::Files);
        for(int i = 0; i< files.count(); i++) {
            QString srcName = sourceFolder + QDir::separator() + files[i];
            QString destName = destFolder + QDir::separator() + files[i];
            kDebug() << srcName << " --> " << destName;
            QFile::copy(srcName, destName);
        }
        files.clear();
        
        QStringList directories = sourceDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
        for(int i = 0; i< directories.count(); i++) {
            QString srcName = sourceFolder + QDir::separator() + directories[i];
            QString destName = destFolder + QDir::separator() + directories[i];
            copyFolder(srcName, destName);
        }
    }
    
    bool removeDir(const QString &dirName)
    {
        bool result = true;
        QDir dir(dirName);
        
        if (dir.exists(dirName)) {
            QFileInfoList list =  dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden  | QDir::AllDirs | QDir::Files, QDir::DirsFirst);
            Q_FOREACH(QFileInfo info, list) {
                if (info.isDir()) {
                    result = removeDir(info.absoluteFilePath());
                }
                else {
                    result = QFile::remove(info.absoluteFilePath());
                }
                
                if (!result) {
                    return result;
                }
            }
            result = dir.rmdir(dirName);
        }
        return result;
    }

    void touch( const QString & filePath ) {
        QFile f1( filePath );
        f1.open( QIODevice::ReadWrite );
        f1.write("blah!");
        f1.close();
    }
}

void FileWatchTest::moveTest()
{
    QString path = createTempDir();
    
    QString f1Path = path + "/1";
    touch( f1Path );
    
    Nepomuk::Resource res1( f1Path );
    res1.setRating( 6 );

    QString f1NewPath = path + "/new-1";

    QFile::remove( f1NewPath ); // Just in case
    QVERIFY( QFile::rename( f1Path, f1NewPath ) );
    QVERIFY( !QFile::exists( f1Path ) );
    QVERIFY( QFile::exists( f1NewPath ) );

    // Sleep for 4 seconds as the filewatcher is not synchronous
    QTest::qSleep( 4000 );
    
    Nepomuk::Resource res2( f1NewPath );
    kDebug() << res1.resourceUri();
    kDebug() << res2.resourceUri();

    kDebug() << res1.property( Nepomuk::Vocabulary::NIE::url() ).toUrl();
    QVERIFY( res1.resourceUri() == res2.resourceUri() );
    QVERIFY( res2.rating() == 6 );

    QFile::remove( f1Path );
    QFile::remove( f1NewPath );

    removeTempDir();
}

void FileWatchTest::deletionTest()
{
    QString path = createTempDir();
    
    QString filePath = path + "/1";
    touch( filePath );

    QUrl resUri;
    {
        Nepomuk::Resource res1( filePath );
        res1.setRating( 5 );
        resUri = res1.resourceUri();
        QVERIFY( res1.exists() );
    }

    QVERIFY( QFile::remove( filePath ) );
    QVERIFY( !QFile::exists( filePath ) );
    
    QTest::qSleep( 4000 );
    
    Soprano::Model * model = Nepomuk::ResourceManager::instance()->mainModel();
    QVERIFY( model->containsAnyStatement( resUri, Soprano::Node(), Soprano::Node() ) );
    
    removeTempDir();
}

QString FileWatchTest::createTempDir()
{
    // We need to use the home directory because the filewatcher doesn't track
    // anyhting in /tmp/, which is where KTempDir would create a directory
    QDir home = QDir::home();
    home.mkdir("nepomuk-filewatch-test");
    
    QDir dir( QDir::homePath() + QDir::separator() + "nepomuk-filewatch-test" );
    return dir.absolutePath();
}

void FileWatchTest::removeTempDir()
{
    removeDir( QDir::homePath() + QDir::separator() + "nepomuk-filewatch-test" );
}


QTEST_KDEMAIN(FileWatchTest, NoGUI)