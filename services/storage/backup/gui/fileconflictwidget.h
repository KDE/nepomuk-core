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


#ifndef _NEPOMUK2_FILECONFLICTWIDGET_H
#define _NEPOMUK2_FILECONFLICTWIDGET_H

#include <QWidget>

#include <QtGui/QTreeWidget>
#include <QtGui/QTreeWidgetItem>
#include <QtGui/QPushButton>

namespace Nepomuk2 {

    class FileConflictWidget : public QWidget
    {
        Q_OBJECT
    public:
        explicit FileConflictWidget(QWidget* parent = 0, Qt::WindowFlags f = 0);

    private slots:
        void slotDiscardButtonClicked();
        void slotIdentifyButtonClicked();

    private:
        enum CustomRoles {
            NepomukUriRole = 53123
        };

        void add( const QUrl& uri, const QUrl& url );
        void removeUrl( const QUrl& url );
        void removeItem(QTreeWidgetItem* item);

        QPushButton* m_discardButton;
        QPushButton* m_identifyButton;

        QTreeWidget* m_treeWidget;
    };
}

#endif // _NEPOMUK2_FILECONFLICTWIDGET_H
