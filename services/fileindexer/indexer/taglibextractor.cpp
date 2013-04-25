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


#include "taglibextractor.h"

#include "nie.h"
#include "nco.h"
#include "nmm.h"
#include "nfo.h"
#include <KDebug>

#include <taglib/taglib.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <QDateTime>

using namespace Nepomuk2::Vocabulary;

namespace Nepomuk2 {

TagLibExtractor::TagLibExtractor(QObject* parent, const QVariantList&)
: ExtractorPlugin(parent)
{
}

QStringList TagLibExtractor::mimetypes()
{
    QStringList types;
    // MP3 FLAC, MPC, Speex, WavPack TrueAudio, WAV, AIFF, MP4 and ASF files.
    // MP3
    types << QLatin1String("audio/mpeg");
    types << QLatin1String("audio/mpeg3"); types << QLatin1String("audio/x-mpeg");

    // FLAC
    types << QLatin1String("audio/flac");

    //OGG
    types << QLatin1String("audio/ogg"); types << QLatin1String("audio/x-vorbis+ogg");

    // WAV
    types << QLatin1String("audio/wav");

    // AIFF
    types << QLatin1String("audio/x-aiff");

    return types;
}

Nepomuk2::SimpleResourceGraph TagLibExtractor::extract(const QUrl& resUri, const QUrl& fileUrl, const QString& mimeType)
{
    Q_UNUSED( mimeType );

    TagLib::FileRef file( fileUrl.toLocalFile().toUtf8().data(), true );
    if( file.isNull() ) {
        return SimpleResourceGraph();
    }

    SimpleResourceGraph graph;
    SimpleResource fileRes( resUri );
    fileRes.addType( NMM::MusicPiece() );

    TagLib::Tag* tags = file.tag();
    if( tags ) {
        QString title = QString::fromUtf8( tags->title().toCString( true ) );
        if( !title.isEmpty() ) {
            fileRes.addProperty( NIE::title(), title );
        }

        QString comment = QString::fromUtf8( tags->comment().toCString( true ) );
        if( !comment.isEmpty() ) {
            fileRes.addProperty( NIE::comment(), comment );
        }

        // TODO: Split genres
        QString genre = QString::fromUtf8( tags->genre().toCString( true ) );
        if( !genre.isEmpty() ) {
            fileRes.addProperty( NMM::genre(), genre );
        }

        QString artistString = QString::fromUtf8( tags->artist().toCString( true ) );
        QList< SimpleResource > artists = contactsFromString( artistString );
        foreach( const SimpleResource& res, artists ) {
            fileRes.addProperty( NMM::performer(), res );
            graph << res;
        }

        QString album = QString::fromUtf8( tags->album().toCString( true ) );
        if( !album.isEmpty() ) {
            SimpleResource albumRes;
            albumRes.addType( NMM::MusicAlbum() );
            albumRes.setProperty( NIE::title(), album );

            fileRes.setProperty( NMM::musicAlbum(), albumRes );
            graph << albumRes;
        }

        if( tags->track() ) {
            fileRes.setProperty( NMM::trackNumber(), tags->track() );
        }

        if( tags->year() ) {
            QDateTime dt;
            dt.setUtcOffset(0); // Date must not be converted.
            QDate date = dt.date();
            // If there is only the year then January 1 it's used to complete the date.
            date.setDate( tags->year(), 1, 1 );
            if( date.year() < 0 ) {
                date.setDate( 1, 1, 1 );
            }
            dt.setDate( date );
            fileRes.setProperty( NMM::releaseDate(), dt );
        }
    }

    TagLib::AudioProperties* audioProp = file.audioProperties();
    if( audioProp ) {
        if( audioProp->length() ) {
            // What about the xml duration?
            fileRes.setProperty( NFO::duration(), audioProp->length() );
        }

        if( audioProp->bitrate() ) {
            fileRes.setProperty( NFO::averageBitrate(), audioProp->bitrate() * 1000 );
        }

        if( audioProp->channels() ) {
            fileRes.setProperty( NFO::channels(), audioProp->channels() );
        }

        if( audioProp->sampleRate() ) {
            fileRes.setProperty( NFO::sampleRate(), audioProp->sampleRate() );
        }
    }

    // TODO: Get more properties based on the file type
    // - Codec
    // - Album Artist
    // - Publisher

    graph << fileRes;
    return graph;
}

}

NEPOMUK_EXPORT_EXTRACTOR( Nepomuk2::TagLibExtractor, "nepomuktaglibextextractor" )
