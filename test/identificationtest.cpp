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
#include <Soprano/QueryResultIterator>
#include <Nepomuk/Vocabulary/NIE>
#include <Nepomuk/Vocabulary/NMO>
#include <Nepomuk/Vocabulary/NCO>
#include <Nepomuk/Vocabulary/NCAL>

#include "nepomuk/datamanagement.h"
#include "nepomuk/simpleresource.h"
#include "nepomuk/simpleresourcegraph.h"
#include "nepomuk/storeresourcesjob.h"

using namespace Soprano::Vocabulary;
using namespace Nepomuk::Vocabulary;

void listResources() {
    Soprano::Model *model = Nepomuk::ResourceManager::instance()->mainModel();
    const QString query = QString::fromLatin1("select distinct ?r where { ?r ?p ?o . "
                                              "FILTER(regex(str(?r), '^nepomuk:/res/')) . }");
    Soprano::QueryResultIterator it = model->executeQuery( query, Soprano::Query::QueryLanguageSparql );
    while( it.next() ) {
        kDebug() << it[0];
    }
}

void NepomukSyncTests::basicIdentification()
{
    listResources();

    KTemporaryFile file;
    QVERIFY(file.open());

    QUrl fileUrl = KUrl(file.fileName());
    Nepomuk::Resource res( fileUrl );
    res.setRating( 5 );

    QVERIFY( res.exists() );
    QVERIFY( res.rating() == 5 );

    KJob* job = Nepomuk::setProperty( QList<QUrl>() << fileUrl, NAO::numericRating(), QVariantList() << 2 );
    job->exec();

    if( job->error() ) {
        kWarning() << job->errorString();
    }
    QVERIFY(!job->error());
    QVERIFY( res.rating() == 2 );

    listResources();
    resetRepository();
    listResources();

    // This is required as resetRepository works on a the models
    Nepomuk::ResourceManager::instance()->clearCache();
    QVERIFY( !res.exists() );
}

void NepomukSyncTests::pim()
{
    using namespace Nepomuk;

    SimpleResource volkerEmail;
    volkerEmail.addType( NCO::EmailAddress() );
    volkerEmail.addProperty( NCO::emailAddress(), QLatin1String("vkrause@kde.org") );

    SimpleResource volker;
    volker.addType( NCO::Contact() );
    volker.addProperty( NCO::fullname(), QLatin1String("Volker Krause") );
    volker.addProperty( NAO::prefLabel(), QLatin1String("Volker Krause") );
    volker.addProperty( NCO::hasEmailAddress(), volkerEmail );

    SimpleResourceGraph g;
    g << volker << volkerEmail;

    KJob *j = storeResources( g );
    j->exec();
    if( j->error() ) {
        kWarning() << j->errorString() << endl;
    }
    QVERIFY( !j->error() );

    SimpleResource rd;
    rd.addType( NAO::FreeDesktopIcon() );
    rd.addProperty( NAO::iconName(), QLatin1String("mail-attachment") );

    SimpleResource qd;
    qd.addType( NIE::InformationElement() );
    qd.addType( NCAL::Attachment() );
    qd.addProperty( NAO::prefSymbol(), rd );
    
    SimpleResource pd;
    pd.addType( NCO::EmailAddress() );
    pd.addProperty( NCO::emailAddress(), QLatin1String("wstephenson@kde.org") );
    
    SimpleResource od;
    od.addType( NCO::Contact() );
    od.addProperty( NCO::fullname(), QLatin1String("Will Stephenson") );
    od.addProperty( NAO::prefLabel(), QLatin1String("Will Stephenson") );
    od.addProperty( NCO::hasEmailAddress(), pd );

    SimpleResource nd;
    nd.addType( NCO::EmailAddress() );
    nd.addProperty( NCO::emailAddress(), QLatin1String("kde-pim@kde.org") );

    SimpleResource md;
    md.addType( NCO::Contact() );
    md.addProperty( NCO::fullname(), QLatin1String("KDEPIM-Libraries") );
    md.addProperty( NAO::prefLabel(), QLatin1String("KDEPIM-Libraries") );
    md.addProperty( NCO::hasEmailAddress(), nd );

    SimpleResource ld;
    ld.addType( NCO::EmailAddress() );
    ld.addProperty( NCO::emailAddress(), QLatin1String("vkrause@kde.org") );

    SimpleResource kd;
    kd.addType( NCO::Contact() );
    kd.addProperty( NCO::fullname(), QLatin1String("Volker Krause") );
    kd.addProperty( NAO::prefLabel(), QLatin1String("Volker Krause") );
    kd.addProperty( NCO::hasEmailAddress(), ld );

    SimpleResource jd;
    jd.addType( NCO::EmailAddress() );
    //WARNING: Isn't this a little weird?
    jd.addProperty( NCO::emailAddress(), QLatin1String("noreply@git.reviewboard.kde.org") );

    SimpleResource id;
    id.addType( NCO::Contact() );
    id.addProperty( NCO::fullname(), QLatin1String("Volker Krause") );
    id.addProperty( NAO::prefLabel(), QLatin1String("Volker Krause") );
    id.addProperty( NCO::hasEmailAddress(), jd );

    SimpleResource hd;
    hd.addType( NCO::EmailAddress() );
    hd.addProperty( NCO::emailAddress(), QLatin1String("vkrause@kde.org") );

    SimpleResource gd;
    gd.addType( NCO::Contact() );
    gd.addProperty( NCO::fullname(), QLatin1String("Volker Krause") );
    gd.addProperty( NAO::prefLabel(), QLatin1String("Volker Krause") );
    gd.addProperty( NCO::hasEmailAddress(), ld );

    SimpleResource fd;
    fd.addType( NAO::FreeDesktopIcon() );
    fd.addProperty( NAO::iconName(), QLatin1String("internet-mail") );

    SimpleResource ed;
    ed.addType( NMO::Message() );
    ed.addType( NMO::Email() );
    ed.addType( NIE::InformationElement() );
    ed.addType( QUrl("http://akonadi-project.org/ontologies/aneo#AkonadiDataObject") );
    ed.addProperty( NMO::sentDate(), QDateTime::currentDateTime() );
    ed.addProperty( NIE::lastModified(), QDateTime::currentDateTime() );
    ed.addProperty( NMO::sender(), id );
    ed.addProperty( NAO::prefLabel(), QLatin1String("Re: Review Request: Support multiple default selections in CollectionDialog"));
    ed.addProperty( NMO::from(), gd );
    ed.addProperty( NMO::plainTextMessageContent(), QLatin1String("-----------------------------------------------------------") );
    ed.addProperty( NIE::url(), QString("akonadi:?item=12289") );
    ed.addProperty( NAO::prefSymbol(), fd );
    ed.addProperty( NMO::messageId(), QLatin1String("<20120209130938.11234.86308@vidsolbach.de>") );
    ed.addProperty( NMO::isRead(), true );
    ed.addProperty( QUrl("http://akonadi-project.org/ontologies/aneo#akonadiItemId"), QString("12289") );
    ed.addProperty( NMO::to(), od );
    ed.addProperty( NMO::to(), md );
    ed.addProperty( NMO::to(), kd );
    ed.addProperty( NMO::messageSubject(), QLatin1String("Re: Review Request: Support multiple default selections in CollectionDialog") );
    ed.addProperty( NIE::byteSize(), 9648 );
    ed.addProperty( NMO::hasAttachment(), qd );
    
    qd.addProperty( NIE::isPartOf(), ed );

    SimpleResourceGraph graph;
    graph << ed << fd << gd << hd << id << jd << kd << ld << md << nd << od << pd << qd << rd;

    KJob *job = Nepomuk::storeResources( graph );
    job->exec();

    if( job->error() ) {
        kWarning() << job->errorString();
    }

    QVERIFY(!job->error());
}

QTEST_KDEMAIN(NepomukSyncTests, NoGUI)
