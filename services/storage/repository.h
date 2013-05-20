/*
 *
 * $Id: sourceheader 511311 2006-02-19 14:51:05Z trueg $
 *
 * This file is part of the Nepomuk KDE project.
 * Copyright (C) 2006-2009 Sebastian Trueg <trueg@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING" for the exact licensing terms.
 */

#ifndef _REPOSITORY_H_
#define _REPOSITORY_H_

#include <QtCore/QString>
#include <QtCore/QMap>
#include <QtCore/QPointer>

#include <Soprano/BackendSettings>
#include <Soprano/FilterModel>
#include <Soprano/Util/DummyModel>


namespace Soprano {
    class Model;
    class Backend;
}

class KJob;

namespace Nepomuk2 {
    class DataManagementModel;
    class DataManagementAdaptor;
    class ClassAndPropertyTree;
    class VirtuosoInferenceModel;
    class OntologyLoader;

    /**
     * Represents the main Nepomuk model. While it looks as if there could be more than
     * one instance of Repository there is only the one.
     *
     * Repository uses a whole stack of Soprano models to provide its functionality. The
     * following list shows the layering from top to bottom:
     *
     * \li The DataManagementModel provides the actual data modification interface. For this
     *     purpose it is exported via DBus.
     *
     * \author Sebastian Trueg <trueg@kde.org>
     */
    class Repository : public Soprano::FilterModel
    {
        Q_OBJECT

    public:
        Repository( const QString& name );
        ~Repository();

        QString name() const { return m_name; }

        QString usedSopranoBackend() const;
        QString storagePath() const { return m_storagePath; }

    public Q_SLOTS:
        /**
         * Will emit the opened signal. This will NOT open the public interface.
         */
        void open();
        void close();

        /**
         * Switches off the datamanagement interface that is used to communicate
         * with the rest of the world
         */
        void closePublicInterface();

        /**
         * Registers the datamangement interface this is used to communicate with
         * the rest of world
         */
        void openPublicInterface();

    Q_SIGNALS:
        /// Emitted when the Repository successfully opened.
        void opened( Repository*, bool success );

        /// Emitted when the ontologies have been loaded, and the repo may be used
        void loaded( Repository*, bool success );
        void closed( Repository* );

    private Q_SLOTS:
        void slotVirtuosoStopped( bool normalExit );
        void slotOpened( Repository*, bool success );
        void slotOntologiesLoaded( bool somethingChanged );

    private:
        Soprano::BackendSettings readVirtuosoSettings() const;

        enum State {
            CLOSED,
            OPENING,
            OPEN,
            LOADED,
        };

        QString m_name;
        State m_state;

        Soprano::Model* m_model;
        Nepomuk2::ClassAndPropertyTree* m_classAndPropertyTree;
        VirtuosoInferenceModel* m_inferenceModel;
        DataManagementModel* m_dataManagementModel;
        Nepomuk2::DataManagementAdaptor* m_dataManagementAdaptor;
        const Soprano::Backend* m_backend;

        Soprano::Util::DummyModel* m_dummyModel;

        // the base path for the data. Will contain subfolder:
        // "data" for the data
        QString m_basePath;
        QString m_storagePath;
        // ------------------------------------------

        OntologyLoader* m_ontologyLoader;
        void updateInference(bool ontologiesChanged);
    };

    typedef QMap<QString, Repository*> RepositoryMap;
}

#endif
