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


#include "fileconflictwidget.h"
#include "resourcemanager.h"
#include "datamanagement.h"
#include "nie.h"

#include <QtGui/QBoxLayout>

#include <Soprano/Model>
#include <Soprano/QueryResultIterator>

#include <KUrl>
#include <KLocalizedString>
#include <KJob>
#include <KFileDialog>
#include <KDebug>

using namespace Nepomuk2::Vocabulary;

Nepomuk2::FileConflictWidget::FileConflictWidget(QWidget* parent, Qt::WindowFlags f): QWidget(parent, f)
{
    // Create the UI
    m_discardButton = new QPushButton( i18n("Discard"), this );
    m_identifyButton = new QPushButton( i18n("Resolve"), this );

    connect( m_discardButton, SIGNAL(clicked(bool)), this, SLOT(slotDiscardButtonClicked()) );
    connect( m_identifyButton, SIGNAL(clicked(bool)), this, SLOT(slotIdentifyButtonClicked()) );

    QHBoxLayout* hLayout = new QHBoxLayout();
    hLayout->addWidget( m_discardButton );
    hLayout->addWidget( m_identifyButton );

    m_treeWidget = new QTreeWidget( this );

    QVBoxLayout* layout = new QVBoxLayout( this );
    layout->addWidget( m_treeWidget );
    layout->addItem( hLayout );

    // Fill with data
    QLatin1String query("select ?r ?url where { ?r nie:url ?url. FILTER(REGEX(STR(?url), '^nepomuk-backup')) . }");

    Soprano::Model* model = ResourceManager::instance()->mainModel();
    Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
    while( it.next() ) {
        add( it[0].uri(), it[1].uri() );
    }
}

void Nepomuk2::FileConflictWidget::add(const QUrl& uri, const QUrl& url)
{
    QTreeWidgetItem* item = new QTreeWidgetItem;
    item->setText( 0, url.path() );
    item->setData( 0, NepomukUriRole, uri );

    // Find its parents
    QString parentString = KUrl(url).directory();

    QList<QTreeWidgetItem*> parents = m_treeWidget->findItems( parentString, Qt::MatchCaseSensitive );
    if( parents.isEmpty() ) {
        // FIXME: Should it really be 0?
        m_treeWidget->insertTopLevelItem( 0, item );
        return;
    }

    QTreeWidgetItem* parentItem = parents.first();
    parentItem->addChild( item );
}

void Nepomuk2::FileConflictWidget::removeUrl(const QUrl& url)
{
    QList<QTreeWidgetItem*> items = m_treeWidget->findItems( url.path(), Qt::MatchCaseSensitive );
    foreach( QTreeWidgetItem* item, items ) {
        m_treeWidget->removeItemWidget( item, 0 );
    }
}

void Nepomuk2::FileConflictWidget::removeItem(QTreeWidgetItem* item)
{
    if( !item )
        return;

    if( item->parent() ) {
        item->parent()->removeChild( item );
        delete item;
    }
    else {
        delete m_treeWidget->takeTopLevelItem( m_treeWidget->indexOfTopLevelItem(item) );
    }
}


void Nepomuk2::FileConflictWidget::slotDiscardButtonClicked()
{
    QTreeWidgetItem* item = m_treeWidget->currentItem();
    if( !item )
        return;

    QUrl uri = item->data( 0, NepomukUriRole ).toUrl();

    QList<QUrl> uris;
    uris << uri;

    // Error Handling?
    KJob* job = Nepomuk2::removeResources( uris );
    job->exec();

    removeItem( item );
}

void Nepomuk2::FileConflictWidget::slotIdentifyButtonClicked()
{
    QTreeWidgetItem* item = m_treeWidget->currentItem();
    if( !item )
        return;

    QUrl url = item->text( 0 );
    QUrl uri = item->data( 0, NepomukUriRole ).toUrl();

    // Launch a KDialog to select the appropriate file
    // TODO: Change the dialog based on the type of file? Maybe even apply different filters
    QUrl newUrl = KFileDialog::getOpenUrl( url, QString(), this, i18n("Find the url") );

    // Error Handling?
    KJob* job = Nepomuk2::setProperty( QList<QUrl>() << uri, NIE::url(), QVariantList() << newUrl );

    job->exec();
    if( job->error() ) {
        kDebug() << "AHHHHHHHHHHH!!!";
        kError() << job->errorString();
    }

    removeItem( item );
}

