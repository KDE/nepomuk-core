/*
   This file is part of the Nepomuk KDE project.
   Copyright (C) 2011-2012 Sebastian Trueg <trueg@kde.org>
   Copyright (C) 2011 Vishesh Handa <handa.vish@gmail.com>

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

#include "qtest_dms.h"

#include <Soprano/Vocabulary/RDF>
#include <Soprano/Vocabulary/RDFS>
#include <Soprano/Vocabulary/NAO>
#include <Soprano/Vocabulary/NRL>
#include <Soprano/Vocabulary/XMLSchema>
#include "nfo.h"
#include "nmm.h"
#include "nco.h"
#include "nie.h"
#include "pimo.h"
#include "nmo.h"

#include <Soprano/LiteralValue>
#include <Soprano/QueryResultIterator>

using namespace Soprano::Vocabulary;
using namespace Soprano;
using namespace Nepomuk2::Vocabulary;
using namespace Nepomuk2;

namespace {
    void addProperty(Soprano::Model* model, const QUrl& graph, const QUrl& prop, const Node& dom, const Node& ran) {
        model->addStatement( prop, RDF::type(), RDF::Property(), graph );
        model->addStatement( prop, RDFS::domain(), dom, graph );
        model->addStatement( prop, RDFS::range(), ran, graph );
    }

    void addType(Soprano::Model* model, const QUrl& graph, const QUrl& type, const QList<QUrl>& superTypes) {
        model->addStatement( type, RDF::type(), RDFS::Class(), graph );

        foreach(const QUrl& super, superTypes) {
            model->addStatement( type, RDFS::subClassOf(), super, graph );
            model->addStatement( super, RDF::type(), RDFS::Class(), graph );
        }
    }
}
void Nepomuk2::insertOntologies(Soprano::Model* model, const QUrl& graph)
{
    model->addStatement( graph, RDF::type(), NRL::Ontology(), graph );
    // removeResources depends on type inference
    model->addStatement( graph, RDF::type(), NRL::Graph(), graph );

    model->addStatement( QUrl("prop:/int"), RDF::type(), RDF::Property(), graph );
    model->addStatement( QUrl("prop:/int"), RDFS::range(), XMLSchema::xsdInt(), graph );

    model->addStatement( QUrl("prop:/int2"), RDF::type(), RDF::Property(), graph );
    model->addStatement( QUrl("prop:/int2"), RDFS::range(), XMLSchema::xsdInt(), graph );

    model->addStatement( QUrl("prop:/int3"), RDF::type(), RDF::Property(), graph );
    model->addStatement( QUrl("prop:/int3"), RDFS::range(), XMLSchema::xsdInt(), graph );

    model->addStatement( QUrl("prop:/int_c1"), RDF::type(), RDF::Property(), graph );
    model->addStatement( QUrl("prop:/int_c1"), RDFS::range(), XMLSchema::xsdInt(), graph );
    model->addStatement( QUrl("prop:/int_c1"), NRL::maxCardinality(), LiteralValue(1), graph );

    model->addStatement( QUrl("prop:/string"), RDF::type(), RDF::Property(), graph );
    model->addStatement( QUrl("prop:/string"), RDFS::range(), XMLSchema::string(), graph );

    model->addStatement( QUrl("prop:/date"), RDF::type(), RDF::Property(), graph );
    model->addStatement( QUrl("prop:/date"), RDFS::range(), XMLSchema::date(), graph );

    model->addStatement( QUrl("prop:/time"), RDF::type(), RDF::Property(), graph );
    model->addStatement( QUrl("prop:/time"), RDFS::range(), XMLSchema::time(), graph );

    model->addStatement( QUrl("prop:/dateTime"), RDF::type(), RDF::Property(), graph );
    model->addStatement( QUrl("prop:/dateTime"), RDFS::range(), XMLSchema::dateTime(), graph );

    model->addStatement( QUrl("prop:/res"), RDF::type(), RDF::Property(), graph );
    model->addStatement( QUrl("prop:/res"), RDFS::range(), RDFS::Resource(), graph );

    model->addStatement( QUrl("prop:/res2"), RDF::type(), RDF::Property(), graph );
    model->addStatement( QUrl("prop:/res2"), RDFS::range(), RDFS::Resource(), graph );

    model->addStatement( QUrl("prop:/res3"), RDF::type(), RDF::Property(), graph );
    model->addStatement( QUrl("prop:/res3"), RDFS::range(), RDFS::Resource(), graph );

    model->addStatement( QUrl("prop:/res_ident"), RDF::type(), RDF::Property(), graph );
    model->addStatement( QUrl("prop:/res_ident"), RDF::type(), NRL::DefiningProperty(), graph );
    model->addStatement( QUrl("prop:/res_ident"), RDFS::range(), RDFS::Resource(), graph );

    model->addStatement( QUrl("prop:/res_c1"), RDF::type(), RDF::Property(), graph );
    model->addStatement( QUrl("prop:/res_c1"), RDFS::range(), RDFS::Resource(), graph );
    model->addStatement( QUrl("prop:/res_c1"), NRL::maxCardinality(), LiteralValue(1), graph );

    model->addStatement( QUrl("class:/typeA"), RDF::type(), RDFS::Class(), graph );
    model->addStatement( QUrl("class:/typeB"), RDF::type(), RDFS::Class(), graph );
    model->addStatement( QUrl("class:/typeB"), RDFS::subClassOf(), QUrl("class:/typeA"), graph );

    model->addStatement( QUrl("prop:/graph"), RDF::type(), RDF::Property(), graph );
    model->addStatement( QUrl("prop:/graph"), RDFS::range(), RDFS::Resource(), graph );
    model->addStatement( QUrl("prop:/graph"), RDFS::domain(), NRL::Graph(), graph );

    // properties used all the time
    model->addStatement( NAO::identifier(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NAO::hasSubResource(), RDF::type(), RDF::Property(), graph );
    model->addStatement( RDF::type(), RDF::type(), RDF::Property(), graph );
    model->addStatement( RDF::type(), RDFS::range(), RDFS::Class(), graph );
    model->addStatement( NIE::url(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NIE::url(), RDFS::range(), RDFS::Resource(), graph );


    // some ontology things the ResourceMerger depends on
    model->addStatement( RDFS::Class(), RDF::type(), RDFS::Class(), graph );
    model->addStatement( RDFS::Class(), RDFS::subClassOf(), RDFS::Resource(), graph );
    model->addStatement( NRL::Graph(), RDF::type(), RDFS::Class(), graph );
    model->addStatement( NRL::InstanceBase(), RDF::type(), RDFS::Class(), graph );
    model->addStatement( NRL::InstanceBase(), RDFS::subClassOf(), NRL::Graph(), graph );
    model->addStatement( NAO::prefLabel(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NAO::prefLabel(), RDFS::range(), RDFS::Literal(), graph );
    model->addStatement( NFO::fileName(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NFO::fileName(), RDFS::range(), XMLSchema::string(), graph );
    model->addStatement( NCO::fullname(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NCO::fullname(), RDFS::range(), XMLSchema::string(), graph );
    model->addStatement( NCO::fullname(), RDFS::domain(), NCO::Contact(), graph );
    model->addStatement( NCO::fullname(), NRL::maxCardinality(), LiteralValue(1), graph );
    model->addStatement( NIE::title(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NIE::title(), RDFS::range(), XMLSchema::string(), graph );
    model->addStatement( NAO::created(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NAO::created(), RDFS::range(), XMLSchema::dateTime(), graph );
    model->addStatement( NAO::created(), NRL::maxCardinality(), LiteralValue(1), graph );
    model->addStatement( NAO::lastModified(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NAO::lastModified(), RDFS::range(), XMLSchema::dateTime(), graph );
    model->addStatement( NAO::lastModified(), NRL::maxCardinality(), LiteralValue(1), graph );

    // used in testStoreResources_sameNieUrl
    model->addStatement( NAO::numericRating(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NAO::numericRating(), RDFS::range(), XMLSchema::xsdInt(), graph );
    model->addStatement( NAO::numericRating(), NRL::maxCardinality(), LiteralValue(1), graph );

    // some ontology things we need in testStoreResources_realLife
    model->addStatement( NMM::season(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NMM::season(), RDFS::range(), XMLSchema::xsdInt(), graph );
    model->addStatement( NMM::episodeNumber(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NMM::episodeNumber(), RDFS::range(), XMLSchema::xsdInt(), graph );
    model->addStatement( NMM::hasEpisode(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NMM::hasEpisode(), RDFS::range(), NMM::TVShow(), graph );
    model->addStatement( NIE::description(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NIE::description(), RDFS::range(), XMLSchema::string(), graph );
    model->addStatement( NMM::synopsis(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NMM::synopsis(), RDFS::range(), XMLSchema::string(), graph );
    model->addStatement( NMM::series(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NMM::series(), RDFS::range(), NMM::TVSeries(), graph );

    // some ontology things we need in testStoreResources_strigiCase
    model->addStatement( NMM::performer(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NMM::performer(), RDFS::domain(), NMM::MusicPiece(), graph );
    model->addStatement( NMM::performer(), RDFS::range(), NCO::Contact(), graph );
    model->addStatement( NMM::musicAlbum(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NMM::musicAlbum(), RDFS::range(), NMM::MusicAlbum(), graph );
    model->addStatement( NMM::MusicAlbum(), RDF::type(), RDFS::Class(), graph );
    model->addStatement( NMM::TVShow(), RDF::type(), RDFS::Class(), graph );
    model->addStatement( NMM::TVSeries(), RDF::type(), RDFS::Class(), graph );
    model->addStatement( NMM::MusicPiece(), RDF::type(), RDFS::Class(), graph );
    model->addStatement( NMM::MusicPiece(), RDFS::subClassOf(), NFO::FileDataObject(), graph );

    // used by testStoreResources_duplicates
    model->addStatement( NFO::hashAlgorithm(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NFO::hashAlgorithm(), RDFS::range(), XMLSchema::string(), graph );
    model->addStatement( NFO::hashValue(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NFO::hashValue(), RDFS::range(), XMLSchema::string(), graph );
    model->addStatement( NFO::hashValue(), NRL::maxCardinality(), LiteralValue(1), graph );
    model->addStatement( NFO::hasHash(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NFO::hasHash(), RDFS::range(), NFO::FileHash(), graph );
    model->addStatement( NFO::hasHash(), RDFS::domain(), NFO::FileDataObject(), graph );
    model->addStatement( NFO::FileHash(), RDF::type(), RDFS::Resource(), graph );
    model->addStatement( NFO::FileHash(), RDF::type(), RDFS::Class(), graph );

    // used by testStoreResources_duplicatesHierarchy
    model->addStatement( NCO::emailAddress(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NCO::emailAddress(), RDFS::range(), XMLSchema::string(), graph );
    model->addStatement( NCO::emailAddress(), RDFS::domain(), NCO::EmailAddress(), graph );
    model->addStatement( NCO::hasEmailAddress(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NCO::hasEmailAddress(), RDFS::range(), NCO::EmailAddress(), graph );
    model->addStatement( NCO::hasEmailAddress(), RDFS::domain(), NCO::Contact(), graph );
    model->addStatement( NCO::EmailAddress(), RDF::type(), RDFS::Resource(), graph );
    model->addStatement( NCO::EmailAddress(), RDF::type(), RDFS::Class(), graph );
    model->addStatement( NCO::EmailAddress(), RDFS::subClassOf(), NCO::ContactMedium(), graph );

    model->addStatement( NIE::isPartOf(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NIE::isPartOf(), RDFS::range(), NFO::FileDataObject(), graph );
    model->addStatement( NIE::lastModified(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NIE::lastModified(), RDFS::range(), XMLSchema::dateTime(), graph );

    model->addStatement( NCO::fullname(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NCO::fullname(), RDFS::range(), XMLSchema::string(), graph );
    model->addStatement( NCO::fullname(), RDFS::domain(), NCO::Contact(), graph );
    model->addStatement( NCO::fullname(), NRL::maxCardinality(), LiteralValue(1), graph );
    model->addStatement( NCO::Contact(), RDF::type(), RDFS::Resource(), graph );
    model->addStatement( NCO::Contact(), RDF::type(), RDFS::Class(), graph );
    model->addStatement( NCO::Role(), RDF::type(), RDFS::Class(), graph );
    model->addStatement( NCO::Contact(), RDFS::subClassOf(), NCO::Role(), graph );
    model->addStatement( NCO::Contact(), RDFS::subClassOf(), NAO::Party(), graph );
    model->addStatement( NCO::Contact(), RDFS::subClassOf(), NIE::InformationElement(), graph );

    model->addStatement( NCO::gender(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NCO::gender(), RDFS::range(), NCO::Gender(), graph );
    model->addStatement( NCO::gender(), RDFS::domain(), NCO::Contact(), graph );

    model->addStatement( NCO::Gender(), RDF::type(), RDFS::Resource(), graph );
    model->addStatement( NCO::Gender(), RDF::type(), RDFS::Class(), graph );
    model->addStatement( NCO::Gender(), RDFS::subClassOf(), RDFS::Resource(), graph );
    model->addStatement( NCO::male(), RDF::type(), NCO::Gender(), graph );
    model->addStatement( NCO::male(), RDF::type(), RDFS::Resource(), graph );
    model->addStatement( NCO::female(), RDF::type(), NCO::Gender(), graph );
    model->addStatement( NCO::female(), RDF::type(), RDFS::Resource(), graph );

    model->addStatement( NCO::PersonContact(), RDF::type(), RDFS::Resource(), graph );
    model->addStatement( NCO::PersonContact(), RDF::type(), RDFS::Class(), graph );
    model->addStatement( NCO::PersonContact(), RDFS::subClassOf(), NCO::Contact(), graph );

    model->addStatement( NAO::Tag(), RDF::type(), RDFS::Class(), graph );
    model->addStatement( NFO::FileDataObject(), RDF::type(), RDFS::Class(), graph );
    model->addStatement( NFO::FileDataObject(), RDFS::subClassOf(), NIE::DataObject(), graph );
    model->addStatement( NIE::DataObject(), RDF::type(), RDFS::Class(), graph );
    model->addStatement( NIE::DataObject(), RDFS::subClassOf(), RDFS::Resource(), graph );
    model->addStatement( NFO::Folder(), RDF::type(), RDFS::Class(), graph );
    model->addStatement( NFO::Folder(), RDFS::subClassOf(), NFO::DataContainer(), graph );
    model->addStatement( NFO::DataContainer(), RDF::type(), RDFS::Class(), graph );
    model->addStatement( NFO::DataContainer(), RDFS::subClassOf(), NIE::InformationElement(), graph );
    model->addStatement( NIE::InformationElement(), RDF::type(), RDFS::Class(), graph );
    model->addStatement( NIE::InformationElement(), RDFS::subClassOf(), RDFS::Resource(), graph );
    model->addStatement( NFO::Video(), RDF::type(), RDFS::Class(), graph );
    model->addStatement( QUrl("class:/typeA"), RDF::type(), RDFS::Class(), graph );
    model->addStatement( QUrl("class:/typeB"), RDF::type(), RDFS::Class(), graph );
    model->addStatement( QUrl("class:/typeC"), RDF::type(), RDFS::Class(), graph );

    model->addStatement( NCO::EmailAddress(), RDF::type(), RDFS::Class(), graph );
    model->addStatement( NCO::hasEmailAddress(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NCO::hasEmailAddress(), RDFS::domain(), NCO::Role(), graph );
    model->addStatement( NCO::hasEmailAddress(), RDFS::range(), NCO::EmailAddress(), graph );
    model->addStatement( NCO::emailAddress(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NCO::emailAddress(), RDFS::domain(), NCO::EmailAddress(), graph );
    model->addStatement( NCO::emailAddress(), RDFS::range(), XMLSchema::string(), graph );

    model->addStatement( NAO::hasTag(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NAO::hasTag(), RDFS::domain(), RDFS::Resource(), graph );
    model->addStatement( NAO::hasTag(), RDFS::range(), NAO::Tag(), graph );

    // PIMO
    model->addStatement( PIMO::groundingOccurrence(), RDF::type(), RDF::Property(), graph );
    model->addStatement( PIMO::groundingOccurrence(), RDFS::range(), NIE::InformationElement(), graph );
    model->addStatement( PIMO::groundingOccurrence(), RDFS::domain(), PIMO::Thing(), graph );

    model->addStatement( PIMO::Person(), RDF::type(), RDFS::Class(), graph );
    model->addStatement( PIMO::Person(), RDFS::subClassOf(), PIMO::Agent(), graph );

    model->addStatement( PIMO::Agent(), RDF::type(), RDFS::Class(), graph );
    model->addStatement( PIMO::Agent(), RDFS::subClassOf(), PIMO::Thing(), graph );
    model->addStatement( PIMO::Agent(), RDFS::subClassOf(), NIE::InformationElement(), graph );

    model->addStatement( PIMO::Thing(), RDF::type(), RDFS::Class(), graph );

    // Agent
    model->addStatement( NAO::Agent(), RDF::type(), RDFS::Class(), graph );
    model->addStatement( NAO::Agent(), RDFS::subClassOf(), RDFS::Resource(), graph );
    model->addStatement( NAO::maintainedBy(), RDF::type(), RDF::Property(), graph );
    model->addStatement( NAO::maintainedBy(), RDFS::range(), RDFS::Resource(), graph );
    model->addStatement( NAO::maintainedBy(), RDFS::domain(), NAO::Agent(), graph );

    // Email stuff
    addType( model, graph, NMO::MessageHeader(), QList<QUrl>() << RDFS::Resource() );
    addProperty( model, graph, NMO::messageHeader(), NMO::Message(), NMO::MessageHeader() );
    addProperty( model, graph, NMO::headerName(), NMO::MessageHeader(), XMLSchema::string() );
    addProperty( model, graph, NMO::headerValue(), NMO::MessageHeader(), XMLSchema::string() );

    addType( model, graph, NMO::Message(), QList<QUrl>() << NIE::InformationElement() );
    addType( model, graph, NMO::Email(), QList<QUrl>() << NMO::Message() );
    addProperty( model, graph, NMO::to(), NMO::Email(), NCO::Contact() );
    addProperty( model, graph, NMO::from(), NMO::Email(), NCO::Contact() );
    addProperty( model, graph, NMO::cc(), NMO::Email(), NCO::Contact() );
    addProperty( model, graph, NMO::bcc(), NMO::Email(), NCO::Contact() );
    addProperty( model, graph, NMO::isRead(), NMO::Email(), XMLSchema::boolean() );
    addProperty( model, graph, NMO::plainTextMessageContent(), NMO::Email(), XMLSchema::string() );
    addProperty( model, graph, NMO::messageId(), NMO::Email(), XMLSchema::string() );
    addProperty( model, graph, NMO::sentDate(), NMO::Message(), XMLSchema::dateTime() );
    addProperty( model, graph, NIE::byteSize(), NIE::InformationElement(), XMLSchema::integer() );

    addType( model, graph, NCO::EmailAddress(), QList<QUrl>() << RDFS::Resource() );
    addProperty( model, graph, NCO::hasEmailAddress(), NCO::Contact(), NCO::EmailAddress() );
    addProperty( model, graph, NCO::emailAddress(), NCO::EmailAddress(), XMLSchema::string() );

    // Icons
    addType( model, graph, NAO::Symbol(), QList<QUrl>() << RDFS::Resource() );
    addType( model, graph, NAO::FreeDesktopIcon(), QList<QUrl>() << NAO::Symbol() );
    addProperty( model, graph, NAO::iconName(), NAO::FreeDesktopIcon(), XMLSchema::string() );
    addProperty( model, graph, NAO::prefSymbol(), RDFS::Resource(), NAO::FreeDesktopIcon() );
}

void Nepomuk2::insertNamespaceAbbreviations(Model* model)
{
    typedef QPair<QString, QString> StringPair;

    QList<StringPair> graphs;
    graphs << qMakePair<QString, QString>("http://www.semanticdesktop.org/ontologies/2010/01/25/nuao#", "nuao");
    graphs << qMakePair<QString, QString>("http://www.semanticdesktop.org/ontologies/2009/11/08/nso#", "nso");
    graphs << qMakePair<QString, QString>("http://www.semanticdesktop.org/ontologies/2007/08/15/nrl#", "nrl");
    graphs << qMakePair<QString, QString>("http://www.semanticdesktop.org/ontologies/2010/04/30/ndo#", "ndo");
    graphs << qMakePair<QString, QString>("http://www.semanticdesktop.org/ontologies/2007/08/15/nao#", "nao");
    graphs << qMakePair<QString, QString>("http://www.w3.org/1999/02/22-rdf-syntax-ns#", "rdf");
    graphs << qMakePair<QString, QString>("http://www.w3.org/2000/01/rdf-schema#", "rdfs");
    graphs << qMakePair<QString, QString>("http://purl.org/dc/elements/1.1/", "dces");
    graphs << qMakePair<QString, QString>("http://purl.org/dc/dcmitype/", "dctype");
    graphs << qMakePair<QString, QString>("http://purl.org/dc/terms/", "dcterms");
    graphs << qMakePair<QString, QString>("http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#", "ncal");
    graphs << qMakePair<QString, QString>("http://www.semanticdesktop.org/ontologies/2007/05/10/nexif#", "nexif");
    graphs << qMakePair<QString, QString>("http://www.semanticdesktop.org/ontologies/2007/05/10/nid3#", "nid3");
    graphs << qMakePair<QString, QString>("http://www.semanticdesktop.org/ontologies/2007/01/19/nie#", "nie");
    graphs << qMakePair<QString, QString>("http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#", "nfo");
    graphs << qMakePair<QString, QString>("http://www.semanticdesktop.org/ontologies/2009/02/19/nmm#", "nmm");
    graphs << qMakePair<QString, QString>("http://www.semanticdesktop.org/ontologies/2007/03/22/nco#", "nco");
    graphs << qMakePair<QString, QString>("http://www.semanticdesktop.org/ontologies/2007/03/22/nmo#", "nmo");
    graphs << qMakePair<QString, QString>("http://www.semanticdesktop.org/ontologies/2008/05/20/tmo#", "tmo");
    graphs << qMakePair<QString, QString>("http://www.semanticdesktop.org/ontologies/2007/11/01/pimo#", "pimo");
    graphs << qMakePair<QString, QString>("http://nepomuk.kde.org/ontologies/2010/08/18/kuvo#", "kuvo");
    graphs << qMakePair<QString, QString>("http://nepomuk.kde.org/ontologies/2010/11/11/nrio#", "nrio");
    graphs << qMakePair<QString, QString>("http://nepomuk.kde.org/ontologies/2010/11/29/kext#", "kext");
    graphs << qMakePair<QString, QString>("http://www.example.org/ontologies/2010/05/29/ndco#", "ndco");
    graphs << qMakePair<QString, QString>("http://akonadi-project.org/ontologies/aneo#", "aneo");
    graphs << qMakePair<QString, QString>("http://nepomuk.kde.org/ontologies/2012/02/29/kao#", "kao");

    foreach(const StringPair& pair, graphs) {
        QString command = QString::fromLatin1("DB.DBA.XML_SET_NS_DECL( '%1', '%2', 2 )")
                          .arg( pair.second, pair.first );

        model->executeQuery( command, Soprano::Query::QueryLanguageUser, QLatin1String("sql") );
    }
}
