/* 
 *
 * $Id: sourceheader 511311 2006-02-19 14:51:05Z trueg $
 *
 * This file is part of the Nepomuk KDE project.
 * Copyright (C) 2006 Sebastian Trueg <trueg@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING" for the exact licensing terms.
 */

static const QString gplTemplate = 
"/*\n"
" *\n"
" * $Id: $\n"
" *\n"
" * This file is part of the Nepomuk KDE project.\n"
" * Copyright (C) 2006 Sebastian Trueg <trueg@kde.org>\n"
" *\n"
" * This program is free software; you can redistribute it and/or modify\n"
" * it under the terms of the GNU General Public License as published by\n"
" * the Free Software Foundation; either version 2 of the License, or\n"
" * (at your option) any later version.\n"
" * See the file \"COPYING\" for the exact licensing terms.\n"
" */\n"
"\n"
"/*\n"
" * This file has been generated by the libKMetaData Resource class generator.\n"
" * DO NOT EDIT THIS FILE.\n"
" * ANY CHANGES WILL BE LOST.\n"
" */\n";

static const QString headerTemplate = gplTemplate +
"\n"
"#ifndef _NEPOMUK_KMETADATA_RESOURCENAMEUPPER_H_\n"
"#define _NEPOMUK_KMETADATA_RESOURCENAMEUPPER_H_\n"
"\n"
"namespace Nepomuk {\n"
"namespace KMetaData {\n"
"KMETADATA_OTHERCLASSES"
"}\n"
"}\n"
"\n"
"#include <kmetadata/KMETADATA_PARENTRESOURCELOWER.h>\n"
"#include <kmetadata/kmetadata_export.h>\n"
"\n"
"namespace Nepomuk {\n"
"namespace KMetaData {\n"
"\n"
"KMETADATA_RESOURCECOMMENT\n"
"class KMETADATA_EXPORT KMETADATA_RESOURCENAME : public KMETADATA_PARENTRESOURCE\n"
"{\n"
" public:\n"
"   /**\n"
"    * Create a new empty and invalid KMETADATA_RESOURCENAME instance\n"
"    */\n"
"   KMETADATA_RESOURCENAME();\n"
"   /**\n"
"    * Default copy constructor\n"
"    */\n"
"   KMETADATA_RESOURCENAME( const KMETADATA_RESOURCENAME& );\n"
"   KMETADATA_RESOURCENAME( const Resource& );\n"
"   /**\n"
"    * Create a new RESOURCENAME instance representing the resource\n"
"    * referenced by \\a uri.\n"
"    */\n"
"   KMETADATA_RESOURCENAME( const QString& uri );\n"
"   ~KMETADATA_RESOURCENAME();\n"
"\n"
"   KMETADATA_RESOURCENAME& operator=( const KMETADATA_RESOURCENAME& );\n"
"\n"
"KMETADATA_METHODS\n"
" protected:\n"
"   KMETADATA_RESOURCENAME( const QString& uri, const QString& type );\n"
"};\n"
"}\n"
"}\n"
"\n"
"#endif\n";


static const QString sourceTemplate = gplTemplate +
"\n"
"#include <kmetadata/kmetadata.h>\n"
"#include \"tools.h\"\n"
"#include \"KMETADATA_RESOURCENAMELOWER.h\"\n"
"\n"
"Nepomuk::KMetaData::KMETADATA_RESOURCENAME::KMETADATA_RESOURCENAME()\n"
"  : KMETADATA_PARENTRESOURCE()\n"
"{\n"
"}\n"
"\n"
"\n"
"Nepomuk::KMetaData::KMETADATA_RESOURCENAME::KMETADATA_RESOURCENAME( const KMETADATA_RESOURCENAME& res )\n"
"  : KMETADATA_PARENTRESOURCE( res )\n"
"{\n"
"}\n"
"\n"
"\n"
"Nepomuk::KMetaData::KMETADATA_RESOURCENAME::KMETADATA_RESOURCENAME( const Nepomuk::KMetaData::Resource& res )\n"
"  : KMETADATA_PARENTRESOURCE( res )\n"
"{\n"
"}\n"
"\n"
"\n"
"Nepomuk::KMetaData::KMETADATA_RESOURCENAME::KMETADATA_RESOURCENAME( const QString& uri )\n"
"  : KMETADATA_PARENTRESOURCE( uri, \"KMETADATA_RESOURCETYPEURI\" )\n"
"{\n"
"}\n"
"\n"
"Nepomuk::KMetaData::KMETADATA_RESOURCENAME::KMETADATA_RESOURCENAME( const QString& uri, const QString& type )\n"
"  : KMETADATA_PARENTRESOURCE( uri, type )\n"
"{\n"
"}\n"
"\n"
"Nepomuk::KMetaData::KMETADATA_RESOURCENAME::~KMETADATA_RESOURCENAME()\n"
"{\n"
"}\n"
"\n"
"\n"
"Nepomuk::KMetaData::KMETADATA_RESOURCENAME& Nepomuk::KMetaData::KMETADATA_RESOURCENAME::operator=( const KMETADATA_RESOURCENAME& res )\n"
"{\n"
"   Resource::operator=( res );\n"
"   return *this;\n"
"}\n"
"\n"
"KMETADATA_METHODS\n";


static const QString ontologySrcTemplate = 
"static const QString NKDE_NAMESPACE = \"http://nepomuk-kde.semanticdesktop.org/ontology/nkde-0.1\";\n"
"\n"
"Nepomuk::KMetaData::Ontology::Ontology()\n"
"{\n"
"   d = new Private;\n"
"KMETADATA_CONSTRUCTOR"
"}\n"
"\n"
"\n"
"Nepomuk::KMetaData::Ontology::~Ontology()\n"
"{\n"
"   delete d;\n"
"}\n";
