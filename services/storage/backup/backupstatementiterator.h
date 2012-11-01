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


#ifndef _NEPOMUK2_BACKUPSTATEMENTITERATOR_H
#define _NEPOMUK2_BACKUPSTATEMENTITERATOR_H

#include <Soprano/Model>
#include <Soprano/Statement>
#include <Soprano/QueryResultIterator>

namespace Nepomuk2 {

class BackupStatementIterator
{
public:
    BackupStatementIterator(Soprano::Model* model);

    bool next();
    Soprano::Statement current();

private:
    Soprano::Model* m_model;
    Soprano::QueryResultIterator m_it;

    enum State {
        State_ProcessingResources,
        State_ProcessingGraphs,
        State_Done
    };
    State m_state;
};

}

#endif // _NEPOMUK2_BACKUPSTATEMENTITERATOR_H
