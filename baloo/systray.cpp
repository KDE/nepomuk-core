/*
 * <one line to give the library's name and an idea of what it does.>
 * Copyright 2014  Vishesh Handa <me@vhanda.in>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "systray.h"
#include "resourcemanager.h"

#include <KLocalizedString>
#include <KTemporaryFile>
#include <KDebug>
#include <KConfig>
#include <KConfigGroup>

#include <iostream>
#include <QTimer>

#include <Soprano/Node>
#include <Soprano/Model>
#include <Soprano/QueryResultIterator>
#include <Soprano/LiteralValue>

#include <baloo/filemodifyjob.h>
#include <baloo/file.h>

using namespace Nepomuk2;

SysTray::SysTray(QObject* parent): KStatusNotifierItem(parent)
{
    setStatus( KStatusNotifierItem::Active );
    setIconByName( "nepomuk" );
    setCategory( KStatusNotifierItem::SystemServices );
    setTitle( i18n("Optimizing search database") );
    setToolTip( "nepomuk", i18n("Optimizing search database"), QString() );

    m_nepomukServer = new org::kde::NepomukServer(QLatin1String("org.kde.NepomukServer"),
                                                  QLatin1String("/nepomukserver"),
                                                  QDBusConnection::sessionBus(), this);

    QTimer::singleShot(0, this, SLOT(startMigration()));
}

SysTray::~SysTray()
{
}

void SysTray::startMigration()
{
    // Check if migration is required?
    if (KConfig("nepomukserverrc").group("Baloo").readEntry("migrated", false)) {
        std::cout << "Data already migrated. Quitting" << std::endl;
        QCoreApplication::instance()->quit();
        return;
    }

    //
    // Port tags, rating and comments
    //
    QString query = QString::fromLatin1("select ?r where { "
                                        "{ ?r nao:numericRating ?rating .} UNION "
                                        "{ ?r nao:description ?d . } UNION "
                                        "{ ?r nao:hasTag ?tag . } }");

    Soprano::Model* model = ResourceManager::instance()->mainModel();
    Soprano::QueryResultIterator it = model->executeQuery(query, Soprano::Query::QueryLanguageSparql);

    bool foundData = false;

    while (it.next()) {
        const QUrl uri = it[0].uri();

        int rating = 0;
        QString comment;
        QString url;
        QStringList tags;

        // Ask for basic details
        // URL
        QString query = QString::fromLatin1("select ?url where { %1 nie:url ?url . } LIMIT 1")
                        .arg(Soprano::Node::resourceToN3(uri));

        Soprano::QueryResultIterator iter = model->executeQuery(query, Soprano::Query::QueryLanguageSparql);
        if (iter.next()) {
            QUrl u = iter[0].uri();
            if (!u.isLocalFile())
                continue;

            url = u.toLocalFile();
        }
        else {
            continue;
        }

        // Rating
        query = QString::fromLatin1("select ?r where { %1 nao:numericRating ?r . } LIMIT 1")
                        .arg(Soprano::Node::resourceToN3(uri));

        iter = model->executeQuery(query, Soprano::Query::QueryLanguageSparql);
        if (iter.next()) {
            Soprano::LiteralValue lv = iter[0].literal();
            if (lv.isInt())
                rating = lv.toInt();
        }

        // Description
        query = QString::fromLatin1("select ?d where { %1 nao:description ?d . } LIMIT 1")
                        .arg(Soprano::Node::resourceToN3(uri));

        iter = model->executeQuery(query, Soprano::Query::QueryLanguageSparql);
        if (iter.next()) {
            comment = iter[0].literal().toString();
        }

        // Tags
        query = QString::fromLatin1("select distinct ?tname where { %1 nao:hasTag ?t . ?t nao:identifier ?tname . }")
                        .arg(Soprano::Node::resourceToN3(uri));

        iter = model->executeQuery(query, Soprano::Query::QueryLanguageSparql);
        while (iter.next()) {
            QString tag = iter[0].literal().toString();
            tags << tag;
        }

        if (rating == 0 && comment.isEmpty() && tags.isEmpty())
            continue;

        // Push to Baloo
        Baloo::File file(url);
        file.setRating(rating);
        file.setUserComment(comment);
        file.setTags(tags);

        kDebug() << url << rating << comment << tags;
        KJob* job = new Baloo::FileModifyJob(file);
        job->exec();

        foundData = true;
    }

    if (!foundData) {
        std::cout << "No Nepomuk data found" << std::endl;
    }

    // Switch off Nepomuk
    m_nepomukServer->quit();

    // Port all the config files
    KConfig config("nepomukstrigirc");
    KConfigGroup group = config.group("General");

    bool indexHidden = group.readEntry("index hidden folders", false );

    QStringList includeFolders = group.readPathEntry("folders", QStringList());
    QStringList excludeFolders = group.readPathEntry("exclude folders", QStringList());
    QStringList excludeFilters = group.readEntry("exclude filters", QStringList());
    QStringList mimetypes = group.readEntry("exclude mimetypes", QStringList());

    KConfig balooFileConfig("baloofilerc");
    KConfigGroup cg = balooFileConfig.group("General");

    if (indexHidden)
        cg.writeEntry("index hidden folders", indexHidden);

    if (!includeFolders.isEmpty())
        cg.writeEntry("folders", includeFolders);

    if (!excludeFolders.isEmpty())
        cg.writeEntry("exclude folders", excludeFolders);

    if (!excludeFilters.isEmpty())
        cg.writeEntry("exclude filters", excludeFilters);

    if (!mimetypes.isEmpty())
        cg.writeEntry("exclude mimetypes", mimetypes);

    // Disable Nepomuk
    KConfig nepomukConfig("nepomukserverrc");
    nepomukConfig.group("Basic Settings").writeEntry("Start Nepomuk", false);
    nepomukConfig.group("Service-nepomukfileindexer").writeEntry("autostart", false);
    nepomukConfig.group("Service-nepomuktelepathyservice").writeEntry("autostart", false);
    nepomukConfig.group("Baloo").writeEntry("migrated", true);

    KConfig akonadiConfig("akonadi_nepomuk_feederrc");
    akonadiConfig.group("akonadi_nepomuk_email_feeder").writeEntry("Enabled", false);

    QCoreApplication::instance()->quit();
}

