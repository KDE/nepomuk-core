/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2011 Sebastian Trueg <trueg@kde.org>

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

#include "fileindexerconfigtest.h"
#include "../fileindexerconfig.h"

#include <KConfig>
#include <KConfigGroup>
#include <KTempDir>

#include <QtCore/QFile>
#include <QtCore/QDir>

#include <QtTest>

#include "qtest_kde.h"

namespace {
void writeIndexerConfig(const QStringList& includeFolders,
                        const QStringList& excludeFolders,
                        const QStringList& excludeFilters,
                        bool indexHidden) {
    KConfig fileIndexerConfig( "nepomukstrigirc" );
    fileIndexerConfig.group( "General" ).writePathEntry( "folders", includeFolders );
    fileIndexerConfig.group( "General" ).writePathEntry( "exclude folders", excludeFolders );
    fileIndexerConfig.group( "General" ).writeEntry( "exclude filters", excludeFilters );
    fileIndexerConfig.group( "General" ).writeEntry( "index hidden folders", indexHidden );
    fileIndexerConfig.sync();
    Nepomuk::FileIndexerConfig::self()->forceConfigUpdate();
}

KTempDir* createTmpFolders(const QStringList& folders) {
    KTempDir* tmpDir = new KTempDir();
    foreach(const QString& f, folders) {
        QDir dir(tmpDir->name());
        foreach(const QString& sf, f.split('/', QString::SkipEmptyParts)) {
            if(!dir.exists(sf)) {
                dir.mkdir(sf);
            }
            dir.cd(sf);
        }
    }
    return tmpDir;
}
}

//
// Trying to put all cases into one folder tree:
// |- indexedRootDir
//   |- indexedSubDir
//     |- indexedSubSubDir
//     |- excludedSubSubDir
//     |- .hiddenSubSubDir
//       |- ignoredSubFolderToIndexedHidden
//       |- indexedSubFolderToIndexedHidden
//   |- excludedSubDir
//     |- includedSubDirToExcluded
//   |- .hiddenSubDir
//   |- .indexedHiddenSubDir
// |- ignoredRootDir
// |- excludedRootDir
//
FileIndexerConfigTest::FileIndexerConfigTest()
    : indexedRootDir(QString::fromLatin1("d1")),
      indexedSubDir(QString::fromLatin1("d1/sd1")),
      indexedSubSubDir(QString::fromLatin1("d1/sd1/ssd1")),
      excludedSubSubDir(QString::fromLatin1("d1/sd1/ssd2")),
      hiddenSubSubDir(QString::fromLatin1("d1/sd1/.ssd3")),
      ignoredSubFolderToIndexedHidden(QString::fromLatin1("d1/sd1/.ssd3/isfh1")),
      indexedSubFolderToIndexedHidden(QString::fromLatin1("d1/sd1/.ssd3/isfh2")),
      excludedSubDir(QString::fromLatin1("d1/sd2")),
      indexedSubDirToExcluded(QString::fromLatin1("d1/sd2/isde1")),
      hiddenSubDir(QString::fromLatin1("d1/.sd3")),
      indexedHiddenSubDir(QString::fromLatin1("d1/.sd4")),
      ignoredRootDir(QString::fromLatin1("d2")),
      excludedRootDir(QString::fromLatin1("d3"))
{
}

// FIXME: how about in the config already excluding symlinks that point to indexed folders (compare the wine "My Documents" link)

void FileIndexerConfigTest::testShouldFolderBeIndexed()
{
    // create the full folder hierarchy
    KTempDir* mainDir = createTmpFolders(QStringList()
                                         << indexedRootDir
                                         << indexedSubDir
                                         << indexedSubSubDir
                                         << excludedSubSubDir
                                         << hiddenSubSubDir
                                         << ignoredSubFolderToIndexedHidden
                                         << indexedSubFolderToIndexedHidden
                                         << excludedRootDir
                                         << hiddenSubDir
                                         << indexedHiddenSubDir
                                         << ignoredRootDir
                                         << excludedRootDir);

    const QString dirPrefix = mainDir->name();

    // write the config
    writeIndexerConfig(QStringList()
                       << dirPrefix + indexedRootDir
                       << dirPrefix + indexedSubFolderToIndexedHidden
                       << dirPrefix + indexedHiddenSubDir
                       << dirPrefix + indexedSubDirToExcluded,
                       QStringList()
                       << dirPrefix + excludedRootDir
                       << dirPrefix + excludedSubDir
                       << dirPrefix + excludedSubSubDir,
                       QStringList(),
                       false);

    // create our test config object
    Nepomuk::FileIndexerConfig* cfg = Nepomuk::FileIndexerConfig::self();

    // run through all the folders
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + indexedRootDir));
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + indexedSubDir));
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + indexedSubSubDir));
    QVERIFY(!cfg->shouldFolderBeIndexed(dirPrefix + excludedSubSubDir));
    QVERIFY(!cfg->shouldFolderBeIndexed(dirPrefix + hiddenSubSubDir));
    QVERIFY(!cfg->shouldFolderBeIndexed(dirPrefix + ignoredSubFolderToIndexedHidden));
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + indexedSubFolderToIndexedHidden));
    QVERIFY(!cfg->shouldFolderBeIndexed(dirPrefix + excludedSubDir));
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + indexedSubDirToExcluded));
    QVERIFY(!cfg->shouldFolderBeIndexed(dirPrefix + hiddenSubDir));
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + indexedHiddenSubDir));
    QVERIFY(!cfg->shouldFolderBeIndexed(dirPrefix + ignoredRootDir));
    QVERIFY(!cfg->shouldFolderBeIndexed(dirPrefix + excludedRootDir));


    // re-write the config with hidden folder indexing enabled
    writeIndexerConfig(QStringList()
                       << dirPrefix + indexedRootDir
                       << dirPrefix + indexedSubFolderToIndexedHidden
                       << dirPrefix + indexedHiddenSubDir
                       << dirPrefix + indexedSubDirToExcluded,
                       QStringList()
                       << dirPrefix + excludedRootDir
                       << dirPrefix + excludedSubDir
                       << dirPrefix + excludedSubSubDir,
                       QStringList(),
                       true);

    // run through all the folders
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + indexedRootDir));
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + indexedSubDir));
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + indexedSubSubDir));
    QVERIFY(!cfg->shouldFolderBeIndexed(dirPrefix + excludedSubSubDir));
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + hiddenSubSubDir));
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + ignoredSubFolderToIndexedHidden));
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + indexedSubFolderToIndexedHidden));
    QVERIFY(!cfg->shouldFolderBeIndexed(dirPrefix + excludedSubDir));
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + indexedSubDirToExcluded));
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + hiddenSubDir));
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + indexedHiddenSubDir));
    QVERIFY(!cfg->shouldFolderBeIndexed(dirPrefix + ignoredRootDir));
    QVERIFY(!cfg->shouldFolderBeIndexed(dirPrefix + excludedRootDir));


    // cleanup
    delete mainDir;
}

void FileIndexerConfigTest::testShouldFolderBeIndexed_symlink()
{
    // create the folder hierarchy
    KTempDir* mainDir = createTmpFolders(QStringList()
                                         << indexedRootDir
                                         << indexedSubDir
                                         << indexedSubSubDir
                                         << ignoredRootDir);

    const QString dirPrefix = mainDir->name();

    // create links for us to test
    const QString symlinkToIndexedSubDir = indexedRootDir + QLatin1String("/") + QString::fromLatin1("sl1");
    const QString symlinkToIgnoredRootDir = indexedRootDir + QLatin1String("/") + QString::fromLatin1("sl2");
    QVERIFY(QFile::link(dirPrefix + indexedSubDir, dirPrefix + symlinkToIndexedSubDir));
    QVERIFY(QFile::link(dirPrefix + ignoredRootDir, dirPrefix + symlinkToIgnoredRootDir));

    // write a basic config
    writeIndexerConfig(QStringList()
                       << dirPrefix + indexedRootDir,
                       QStringList(),
                       QStringList(),
                       false);

    // create our test config object
    Nepomuk::FileIndexerConfig* cfg = Nepomuk::FileIndexerConfig::self();

    // check that the symlink folder should not be indexed since its target is already indexed
    QVERIFY(!cfg->shouldFolderBeIndexed(dirPrefix + symlinkToIndexedSubDir));

    // check that sub-folders to the symlink folder are not indexed
    QVERIFY(!cfg->shouldFolderBeIndexed(dirPrefix + symlinkToIndexedSubDir + QLatin1String("/") + indexedSubSubDir.mid(indexedSubDir.length())));

    // check that the contents of the symlink which points to the ignored root dir is indexed
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + symlinkToIgnoredRootDir));

    // TODO: what about this situation: a link to a folder which has an excluded sub-folder. Should we then also exclude the sub-folder in the symlink path?
}

void FileIndexerConfigTest::testShouldBeIndexed()
{
    // create the full folder hierarchy
    KTempDir* mainDir = createTmpFolders(QStringList()
                                         << indexedRootDir
                                         << indexedSubDir
                                         << indexedSubSubDir
                                         << excludedSubSubDir
                                         << hiddenSubSubDir
                                         << ignoredSubFolderToIndexedHidden
                                         << indexedSubFolderToIndexedHidden
                                         << excludedRootDir
                                         << hiddenSubDir
                                         << indexedHiddenSubDir
                                         << ignoredRootDir
                                         << excludedRootDir);

    const QString dirPrefix = mainDir->name();

    // write the config
    writeIndexerConfig(QStringList()
                       << dirPrefix + indexedRootDir
                       << dirPrefix + indexedSubFolderToIndexedHidden
                       << dirPrefix + indexedHiddenSubDir
                       << dirPrefix + indexedSubDirToExcluded,
                       QStringList()
                       << dirPrefix + excludedRootDir
                       << dirPrefix + excludedSubDir
                       << dirPrefix + excludedSubSubDir,
                       QStringList(),
                       false);

    // create our test config object
    Nepomuk::FileIndexerConfig* cfg = Nepomuk::FileIndexerConfig::self();

    // run through all the folders
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + indexedRootDir));
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + indexedSubDir));
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + indexedSubSubDir));
    QVERIFY(!cfg->shouldFolderBeIndexed(dirPrefix + excludedSubSubDir));
    QVERIFY(!cfg->shouldFolderBeIndexed(dirPrefix + hiddenSubSubDir));
    QVERIFY(!cfg->shouldFolderBeIndexed(dirPrefix + ignoredSubFolderToIndexedHidden));
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + indexedSubFolderToIndexedHidden));
    QVERIFY(!cfg->shouldFolderBeIndexed(dirPrefix + excludedSubDir));
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + indexedSubDirToExcluded));
    QVERIFY(!cfg->shouldFolderBeIndexed(dirPrefix + hiddenSubDir));
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + indexedHiddenSubDir));
    QVERIFY(!cfg->shouldFolderBeIndexed(dirPrefix + ignoredRootDir));
    QVERIFY(!cfg->shouldFolderBeIndexed(dirPrefix + excludedRootDir));

    // run through all the folders with a file name attached
    const QString fileName = QString::fromLatin1("/somefile.txt");
    QVERIFY(cfg->shouldBeIndexed(dirPrefix + indexedRootDir + fileName));
    QVERIFY(cfg->shouldBeIndexed(dirPrefix + indexedSubDir + fileName));
    QVERIFY(cfg->shouldBeIndexed(dirPrefix + indexedSubSubDir + fileName));
    QVERIFY(!cfg->shouldBeIndexed(dirPrefix + excludedSubSubDir + fileName));
    QVERIFY(!cfg->shouldBeIndexed(dirPrefix + hiddenSubSubDir + fileName));
    QVERIFY(!cfg->shouldBeIndexed(dirPrefix + ignoredSubFolderToIndexedHidden + fileName));
    QVERIFY(cfg->shouldBeIndexed(dirPrefix + indexedSubFolderToIndexedHidden + fileName));
    QVERIFY(!cfg->shouldBeIndexed(dirPrefix + excludedSubDir + fileName));
    QVERIFY(cfg->shouldBeIndexed(dirPrefix + indexedSubDirToExcluded + fileName));
    QVERIFY(!cfg->shouldBeIndexed(dirPrefix + hiddenSubDir + fileName));
    QVERIFY(cfg->shouldBeIndexed(dirPrefix + indexedHiddenSubDir + fileName));
    QVERIFY(!cfg->shouldBeIndexed(dirPrefix + ignoredRootDir + fileName));
    QVERIFY(!cfg->shouldBeIndexed(dirPrefix + excludedRootDir + fileName));


    // re-write the config with hidden folder indexing enabled
    writeIndexerConfig(QStringList()
                       << dirPrefix + indexedRootDir
                       << dirPrefix + indexedSubFolderToIndexedHidden
                       << dirPrefix + indexedHiddenSubDir
                       << dirPrefix + indexedSubDirToExcluded,
                       QStringList()
                       << dirPrefix + excludedRootDir
                       << dirPrefix + excludedSubDir
                       << dirPrefix + excludedSubSubDir,
                       QStringList(),
                       true);

    // run through all the folders
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + indexedRootDir));
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + indexedSubDir));
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + indexedSubSubDir));
    QVERIFY(!cfg->shouldFolderBeIndexed(dirPrefix + excludedSubSubDir));
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + hiddenSubSubDir));
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + ignoredSubFolderToIndexedHidden));
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + indexedSubFolderToIndexedHidden));
    QVERIFY(!cfg->shouldFolderBeIndexed(dirPrefix + excludedSubDir));
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + indexedSubDirToExcluded));
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + hiddenSubDir));
    QVERIFY(cfg->shouldFolderBeIndexed(dirPrefix + indexedHiddenSubDir));
    QVERIFY(!cfg->shouldFolderBeIndexed(dirPrefix + ignoredRootDir));
    QVERIFY(!cfg->shouldFolderBeIndexed(dirPrefix + excludedRootDir));

    // run through all the folders with a file name attached
    QVERIFY(cfg->shouldBeIndexed(dirPrefix + indexedRootDir + fileName));
    QVERIFY(cfg->shouldBeIndexed(dirPrefix + indexedSubDir + fileName));
    QVERIFY(cfg->shouldBeIndexed(dirPrefix + indexedSubSubDir + fileName));
    QVERIFY(!cfg->shouldBeIndexed(dirPrefix + excludedSubSubDir + fileName));
    QVERIFY(cfg->shouldBeIndexed(dirPrefix + hiddenSubSubDir + fileName));
    QVERIFY(cfg->shouldBeIndexed(dirPrefix + ignoredSubFolderToIndexedHidden + fileName));
    QVERIFY(cfg->shouldBeIndexed(dirPrefix + indexedSubFolderToIndexedHidden + fileName));
    QVERIFY(!cfg->shouldBeIndexed(dirPrefix + excludedSubDir + fileName));
    QVERIFY(cfg->shouldBeIndexed(dirPrefix + indexedSubDirToExcluded + fileName));
    QVERIFY(cfg->shouldBeIndexed(dirPrefix + hiddenSubDir + fileName));
    QVERIFY(cfg->shouldBeIndexed(dirPrefix + indexedHiddenSubDir + fileName));
    QVERIFY(!cfg->shouldBeIndexed(dirPrefix + ignoredRootDir + fileName));
    QVERIFY(!cfg->shouldBeIndexed(dirPrefix + excludedRootDir + fileName));

    // cleanup
    delete mainDir;
}

QTEST_KDEMAIN_CORE(FileIndexerConfigTest)

#include "fileindexerconfigtest.moc"
