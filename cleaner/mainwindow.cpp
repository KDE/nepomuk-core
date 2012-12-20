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
#include <QLabel>
#include <QCoreApplication>

#include <KLocalizedString>

MainWindow::MainWindow(QWidget* parent, Qt::WindowFlags flags): QMainWindow(parent, flags)
{
    QWidget* widget = new QWidget( this );
    QVBoxLayout* layout = new QVBoxLayout( widget );

    QLabel* label = new QLabel( i18n("The Nepomuk Cleaner can be used to cleanup invalid and buggy"
                                     " data in your Nepomuk database."), this );
    label->setWordWrap( true );

    m_model = new JobModel( this );
    connect( m_model, SIGNAL(finished()), this, SLOT(slotModelFinished()) );

    m_view = new QListView( widget );
    m_view->setModel( m_model );
    m_view->setWordWrap( true );
    m_view->setSelectionMode( QAbstractItemView::NoSelection );

    m_button = new KPushButton(i18n("Start"), widget);
    connect(m_button, SIGNAL(clicked(bool)), this, SLOT(slotButtonClicked()));

    layout->addWidget( label );
    layout->addWidget( m_view );
    layout->addWidget( m_button );

    setCentralWidget( widget );
    setWindowTitle( i18n("Nepomuk Cleaner") );
}

void MainWindow::slotButtonClicked()
{
    if( m_model->status() == JobModel::NotStarted ) {
        m_button->setText( i18n("Pause") );
        m_model->start();
    }
    else if( m_model->status() == JobModel::Running ) {
        m_model->pause();
        m_button->setText( i18n("Resume") );
    }
    else if( m_model->status() == JobModel::Paused ) {
        m_model->resume();
        m_button->setText( i18n("Pause") );
    }
    else if( m_model->status() == JobModel::Finished ) {
        QCoreApplication::instance()->quit();
    }
}

void MainWindow::slotModelFinished()
{
    m_button->setText( i18n("Quit") );
}
