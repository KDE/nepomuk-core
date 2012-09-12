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


#include "exiv2extractor.h"

#include "nfo.h"
#include "nexif.h"
#include "nie.h"

#include <KDebug>

#include <exiv2/exiv2.hpp>

using namespace Nepomuk2::Vocabulary;

namespace Nepomuk2 {

QStringList Exiv2Extractor::mimetypes()
{
    QStringList types;

    types << QLatin1String("image/jpeg")
          << QLatin1String("image/x-exv")
          << QLatin1String("image/x-canon-cr2")
          << QLatin1String("image/x-canon-crw")
          << QLatin1String("image/x-minolta-mrw")
          << QLatin1String("image/tiff")
          << QLatin1String("image/x-nikon-nef")
          << QLatin1String("image/x-pentax-pef")
          << QLatin1String("image/x-panasonic-rw2")
          << QLatin1String("image/x-samsung-srw")
          << QLatin1String("image/x-olympus-orf")
          << QLatin1String("image/png")
          << QLatin1String("image/pgf")
          << QLatin1String("image/x-fuji-raf")
          << QLatin1String("image/x-photoshop")
          << QLatin1String("image/jp2");

    return types;
}

namespace {
    QVariant toVariantLong(const Exiv2::Value& value) {
        qlonglong l = value.toLong();
        return QVariant(l);
    }

    QVariant toVariantFloat(const Exiv2::Value& value) {
        double f = value.toFloat();
        return QVariant(f);
    }

    QVariant toVariantString(const Exiv2::Value& value) {
        std::string str = value.toString();
        return QVariant( QString::fromUtf8( str.c_str(), str.length() ) );
    }
}

SimpleResourceGraph Exiv2Extractor::extract(const QUrl& resUri, const QUrl& fileUrl)
{
    QByteArray arr = fileUrl.toLocalFile().toUtf8();
    std::string fileString( arr.data(), arr.length() );

    Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open( fileString );
    if( !image.get() ) {
        return SimpleResourceGraph();
    }

    image->readMetadata();
    const Exiv2::ExifData &data = image->exifData();

    SimpleResourceGraph graph;
    SimpleResource fileRes( resUri );

    if( image->pixelHeight() ) {
        fileRes.setProperty( NFO::height(), image->pixelHeight() );
    }

    if( image->pixelWidth() ) {
        fileRes.setProperty( NFO::width(), image->pixelWidth() );
    }

    std::string comment = image->comment();
    if( !comment.empty() ) {
        fileRes.setProperty( NIE::comment(), QString::fromUtf8( comment.c_str(), comment.length() ) );
    }

    /*
    Exiv2::ExifData::const_iterator end = data.end();
    Exiv2::ExifData::const_iterator i = data.begin();
    for( ; i != end; i++ ) {
        kDebug() << i->key().c_str();
        kDebug() << i->value().toString().c_str();
    }*/

    Exiv2::ExifData::const_iterator it;

    it = data.findKey( Exiv2::ExifKey("Exif.Photo.Flash") );
    if( it != data.end() ) {
        fileRes.setProperty( NEXIF::flash(), toVariantLong( it->value() ) );
    }

    it = data.findKey( Exiv2::ExifKey("Exif.Photo.PixelXDimension") );
    if( it != data.end() ) {
        fileRes.setProperty( NFO::width(), toVariantLong( it->value() ) );
    }

    it = data.findKey( Exiv2::ExifKey("Exif.Photo.PixelYDimension") );
    if( it != data.end() ) {
        fileRes.setProperty( NFO::height(), toVariantLong( it->value() ) );
    }

    it = data.findKey( Exiv2::ExifKey("Exif.Image.Make") );
    if( it != data.end() ) {
        fileRes.setProperty( NEXIF::make(), toVariantString( it->value() ) );
    }

    it = data.findKey( Exiv2::ExifKey("Exif.Image.Model") );
    if( it != data.end() ) {
        fileRes.setProperty( NEXIF::model(), toVariantString( it->value() ) );
    }

    it = data.findKey( Exiv2::ExifKey("Exif.Image.DateTime") );
    if( it != data.end() ) {
        //FIXME: Convert to date time
        //fileRes.setProperty( NIE::contentCreated(), toVariantString( it->value() ) );
    }

    it = data.findKey( Exiv2::ExifKey("Exif.Image.Orientation") );
    if( it != data.end() ) {
        fileRes.setProperty( NEXIF::orientation(), toVariantLong( it->value() ) );
    }

    it = data.findKey( Exiv2::ExifKey("Exif.Photo.FocalLength") );
    if( it != data.end() ) {
        fileRes.setProperty( NEXIF::focalLength(), toVariantFloat( it->value() ) );
    }

    it = data.findKey( Exiv2::ExifKey("Exif.Photo.FocalLengthIn35mmFilm") );
    if( it != data.end() ) {
        fileRes.setProperty( NEXIF::focalLengthIn35mmFilm(), toVariantFloat( it->value() ) );
    }

    it = data.findKey( Exiv2::ExifKey("Exif.Photo.ExposureTime") );
    if( it != data.end() ) {
        fileRes.setProperty( NEXIF::exposureTime(), toVariantFloat( it->value() ) );
    }

    it = data.findKey( Exiv2::ExifKey("Exif.Photo.ApertureValue") );
    if( it != data.end() ) {
        fileRes.setProperty( NEXIF::apertureValue(), toVariantFloat( it->value() ) );
    }

    it = data.findKey( Exiv2::ExifKey("Exif.Photo.ExposureBiasValue") );
    if( it != data.end() ) {
        fileRes.setProperty( NEXIF::exposureBiasValue(), toVariantFloat( it->value() ) );
    }

    it = data.findKey( Exiv2::ExifKey("Exif.Photo.WhiteBalance") );
    if( it != data.end() ) {
        fileRes.setProperty( NEXIF::whiteBalance(), toVariantLong( it->value() ) );
    }

    it = data.findKey( Exiv2::ExifKey("Exif.Photo.MeteringMode") );
    if( it != data.end() ) {
        fileRes.setProperty( NEXIF::meteringMode(), toVariantLong( it->value() ) );
    }

    it = data.findKey( Exiv2::ExifKey("Exif.Photo.ISOSpeedRatings") );
    if( it != data.end() ) {
        fileRes.setProperty( NEXIF::isoSpeedRatings(), toVariantLong( it->value() ) );
    }

    it = data.findKey( Exiv2::ExifKey("Exif.Photo.Saturation") );
    if( it != data.end() ) {
        fileRes.setProperty( NEXIF::saturation(), toVariantLong( it->value() ) );
    }

    it = data.findKey( Exiv2::ExifKey("Exif.Photo.Sharpness") );
    if( it != data.end() ) {
        fileRes.setProperty( NEXIF::sharpness(), toVariantLong( it->value() ) );
    }

    fileRes.addType( NEXIF::Photo() );

    graph << fileRes;
    return graph;
}

}