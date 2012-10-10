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


#ifndef BACKUPFILE_H
#define BACKUPFILE_H

#include <QtCore/QUrl>
#include <QtCore/QDateTime>
#include <Soprano/QueryResultIterator>
#include <Soprano/StatementIterator>

namespace Nepomuk2 {

    class BackupStatementIterator;

    class BackupFile
    {
    public:
        BackupFile();

        Soprano::StatementIterator iterator();

        static BackupFile fromUrl(const QUrl& url);
        static bool createBackupFile(const QUrl& url, BackupStatementIterator& it);

        int numStatements();
        QDateTime created();

    private:
        Soprano::StatementIterator m_stIter;

        QDateTime m_created;
        int m_numStatements;
    };

}
#endif // BACKUPFILE_H
