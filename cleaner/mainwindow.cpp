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


#include "mainwindow.h"
#include "jobmodel.h"

#include <QtGui/QVBoxLayout>

#include <KLocalizedString>

MainWindow::MainWindow(QWidget* parent, Qt::WindowFlags flags): QMainWindow(parent, flags)
{
    QWidget* widget = new QWidget( this );
    QVBoxLayout* layout = new QVBoxLayout( widget );

    m_model = new JobModel( this );
    m_view = new QListView( widget );
    m_view->setModel( m_model );

    m_button = new KPushButton(i18n("Start"), widget);
    connect(m_button, SIGNAL(clicked(bool)), this, SLOT(slotStarted()));

    layout->addWidget( m_view );
    layout->addWidget( m_button );

    setCentralWidget( widget );
}

void MainWindow::slotStarted()
{
    m_model->start();
}


