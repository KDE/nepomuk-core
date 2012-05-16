/*
   Copyright (C) 2010 Sebastian Trueg <trueg@kde.org>

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

#include "querytester.h"

#include <QtCore/QStringList>
#include <QtCore/QHash>
#include <QtCore/QDate>

#include <QtGui/QApplication>
#include <QtGui/QInputDialog>
#include <QtGui/QFontMetrics>
#include <QtGui/QVBoxLayout>
#include <QtGui/QTextBrowser>
#include <QtGui/QCheckBox>

#include <KApplication>
#include <KDebug>
#include <KDialog>
#include <KCmdLineArgs>
#include <KAboutData>

#include "query.h"
#include "queryparser.h"

int main( int argc, char **argv )
{
    KAboutData aboutData( "querytester",
                          0,
                          ki18n("Simple Query Tester"),
                          "1.0",
                          ki18n("A Simple Test app for Nepomuk2::Query::QueryParser"),
                          KAboutData::License_GPL,
                          ki18n("(c) 2011, Sebastian Trüg"),
                          KLocalizedString(),
                          "http://nepomuk.kde.org" );
    aboutData.addAuthor(ki18n("Sebastian Trüg"), ki18n("Maintainer"), "trueg@kde.org");

    KCmdLineArgs::init( argc, argv, &aboutData );
    KCmdLineOptions options;
    KCmdLineArgs::addCmdLineOptions( options );

    KApplication app;
    QueryTester tester;
    tester.show();
    return app.exec();
}

QueryTester::QueryTester(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);

    connect(m_queryEdit, SIGNAL(textEdited(QString)), this, SLOT(slotConvert()));
    connect(m_checkTermGlobbing, SIGNAL(toggled(bool)), this, SLOT(slotConvert()));
    connect(m_checkFilenamePattern, SIGNAL(toggled(bool)), this, SLOT(slotConvert()));
}

QueryTester::~QueryTester()
{
}

void QueryTester::slotConvert()
{
    Nepomuk2::Query::QueryParser::ParserFlags flags;
    if(m_checkTermGlobbing->isChecked()) {
        flags |= Nepomuk2::Query::QueryParser::QueryTermGlobbing;
    }
    if(m_checkFilenamePattern->isChecked()) {
        flags |= Nepomuk2::Query::QueryParser::DetectFilenamePattern;
    }

    QString query = Nepomuk2::Query::QueryParser::parseQuery(m_queryEdit->text(), flags).toSparqlQuery();
    query= query.simplified();
    QString newQuery;
    int i = 0;
    int indent = 0;
    int space = 4;

    while(i < query.size()) {
        newQuery.append(query[i]);
        if(query[i] != QLatin1Char('"') && query[i] != QLatin1Char('<') && query[i] != QLatin1Char('\'')) {
            if(query[i] == QLatin1Char('{')) {
                indent++;
                newQuery.append('\n');
                newQuery.append(QString().fill(QLatin1Char(' '), indent*space));
            }
            else if (query[i] == QLatin1Char('.')) {
                if(i+2<query.size()) {
                    if(query[i+1] == QLatin1Char('}')||query[i+2] == QLatin1Char('}')) {
                        newQuery.append('\n');
                        newQuery.append(QString().fill(QLatin1Char(' '), (indent-1)*space));
                    }
                    else {
                        newQuery.append('\n');
                        newQuery.append(QString().fill(QLatin1Char(' '), indent*space));
                    }
                }
                else {
                    newQuery.append('\n');
                    newQuery.append(QString().fill(QLatin1Char(' '), indent*space));
                }
            }
            else if (query[i] == QLatin1Char('}')) {
                indent--;
                if(i+2<query.size()) {
                    if(query[i+2] == QLatin1Char('.')||query[i+1] == QLatin1Char('.'))
                        newQuery.append(QString().fill(QLatin1Char(' '), 1));
                    else {
                        newQuery.append('\n');
                        newQuery.append(QString().fill(QLatin1Char(' '), indent*space));
                    }
                }
                else {
                    newQuery.append('\n');
                    newQuery.append(QString().fill(QLatin1Char(' '), indent*space));
                }
            }
        }
        else {
            i++;
            while(i < query.size()) {
                if(query[i] == QLatin1Char('"') || query[i] == QLatin1Char('>') || query[i] == QLatin1Char('\'')) {
                    newQuery.append(query[i]);
                    break;
                }
                newQuery.append(query[i]);
                i++;
            }
        }
        i++;
    }
    m_sparqlView->setPlainText( newQuery );
}

#include "querytester.moc"
