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

#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/mpegfile.h>
#include <taglib/oggfile.h>
#include <taglib/taglib.h>
#include <taglib/tag.h>
#include <taglib/vorbisfile.h>
#include <taglib/xiphcomment.h>
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

    TagLib::Tag* tags = file.tag();
    if( !tags->isEmpty() ) {
        fileRes.addType( NMM::MusicPiece() );
    } else {
        fileRes.addType( NFO::Audio() );
    }

    TagLib::String artists = "";
    TagLib::StringList genres;

    // Handling multiple tags in mpeg files.
    if( (mimeType == "audio/mpeg") || (mimeType == "audio/mpeg3") || (mimeType == "audio/x-mpeg") ) {
        TagLib::MPEG::File mpegFile( fileUrl.toLocalFile().toUtf8().data(), true );
        if( !mpegFile.ID3v2Tag()->isEmpty() ) {
            TagLib::ID3v2::FrameList lstID3v2;

            // Artist.
            lstID3v2 = mpegFile.ID3v2Tag()->frameListMap()["TPE1"];
            if ( !lstID3v2.isEmpty() ) {
                for (TagLib::ID3v2::FrameList::ConstIterator it = lstID3v2.begin(); it != lstID3v2.end(); ++it) {
                    if( !artists.isEmpty() ) {
                        artists += ", ";
                    }
                    artists += (*it)->toString();
                }
            }

            // Genre.
            lstID3v2 = mpegFile.ID3v2Tag()->frameListMap()["TCON"];
            if ( !lstID3v2.isEmpty() ) {
                for (TagLib::ID3v2::FrameList::ConstIterator it = lstID3v2.begin(); it != lstID3v2.end(); ++it) {
                    genres.append((*it)->toString());
                }
            }
        }
    }

    // Handling multiple tags in FLAC files.
    if( mimeType == "audio/flac" ) {
        TagLib::FLAC::File flacFile( fileUrl.toLocalFile().toUtf8().data(), true );
        if( !flacFile.xiphComment()->isEmpty() ) {
            TagLib::Ogg::FieldListMap lstFLAC = flacFile.xiphComment()->fieldListMap();
            TagLib::Ogg::FieldListMap::ConstIterator itFLAC;

            // Artist.
            itFLAC = lstFLAC.find("ARTIST");
            if( itFLAC != lstFLAC.end() ) {
                if( !artists.isEmpty() ) {
                    artists += ", ";
                }
                artists += (*itFLAC).second.toString(", ");
            }

            // Genre.
            itFLAC = lstFLAC.find("GENRE");
            if( itFLAC != lstFLAC.end() ) {
                genres.append((*itFLAC).second);
            }
        }
    }

    // Handling multiple tags in OGG files.
    if( mimeType == "audio/ogg" || mimeType == "audio/x-vorbis+ogg" ) {
        TagLib::Ogg::Vorbis::File oggFile( fileUrl.toLocalFile().toUtf8().data(), true );
        if( !oggFile.tag()->isEmpty() ) {
            TagLib::Ogg::FieldListMap lstOGG = oggFile.tag()->fieldListMap();
            TagLib::Ogg::FieldListMap::ConstIterator itOGG;

            // Artist.
            itOGG = lstOGG.find("ARTIST");
            if( itOGG != lstOGG.end() ) {
                if( !artists.isEmpty() ) {
                    artists += ", ";
                }
                artists += (*itOGG).second.toString(", ");
            }

            // Genre.
            itOGG = lstOGG.find("GENRE");
            if( itOGG != lstOGG.end() ) {
                genres.append((*itOGG).second);
            }
        }
    }

    if( !tags->isEmpty() ) {
        QString title = QString::fromUtf8( tags->title().toCString( true ) );
        if( !title.isEmpty() ) {
            fileRes.addProperty( NIE::title(), title );
        }

        QString comment = QString::fromUtf8( tags->comment().toCString( true ) );
        if( !comment.isEmpty() ) {
            fileRes.addProperty( NIE::comment(), comment );
        }

        if( genres.isEmpty() ) {
            // TODO: Split genres.
            QString genre = QString::fromUtf8( tags->genre().toCString( true ) );
            if( !genre.isEmpty() ) {
                fileRes.addProperty( NMM::genre(), genre );
            }
        } else {
            for( uint i = 0; i < genres.size(); i++ ) {
                QString genre = QString::fromUtf8( genres[i].toCString( true ) ).trimmed();
                fileRes.addProperty( NMM::genre(), genre );
            }
        }

        QString artistString;
        if( artists.isEmpty() ) {
            artistString = QString::fromUtf8( tags->artist().toCString( true ) );
        } else {
            artistString = QString::fromUtf8( artists.toCString( true ) ).trimmed();
        }

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
            QDateTime dt = QDateTime::currentDateTime();
            QDate date = dt.date();
            date.setDate( tags->year(), date.month(), date.day() );
            if( date.year() < 0 )
                date.setDate( 1, date.month(), date.day() );
            dt.setDate( date );
            fileRes.setProperty( NIE::contentCreated(), dt );
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

    // TAG information (incomplete).
    // A good reference: http://qoobar.sourceforge.net/en/documentation.htm
    // -- FLAC/OGG --
    // Artist:          ARTIST, PERFORMER
    // Album artist:    ALBUMARTIST
    // Composer:        COMPOSER
    // Lyricist:        LYRICIST
    // Conductor:       CONDUCTOR
    // Disc number:     DISCNUMBER
    // Total discs:     TOTALDISCS, DISCTOTAL
    // Track number:    TRACKNUMBER
    // Total tracks:    TOTALTRACKS, TRACKTOTAL
    // Genre: GENRE
    // -- ID3v2 --
    // Artist:                          TPE1
    // Album artist:                    TPE2
    // Composer:                        TCOM
    // Lyricist:                        TEXT
    // Conductor:                       TPE3
    // Disc number[/total dics]:        TPOS
    // Track number[/total tracks]:     TRCK
    // Genre:                           TCON

    graph << fileRes;
    return graph;
}

}

NEPOMUK_EXPORT_EXTRACTOR( Nepomuk2::TagLibExtractor, "nepomuktaglibextextractor" )
