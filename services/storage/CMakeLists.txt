project(storage)

add_definitions(-DKDE_DEFAULT_DEBUG_AREA=300105)

include_directories(
  ${SOPRANO_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}
  )

set(storage_SRCS
  storage.cpp
  nepomukcore.cpp
  modelcopyjob.cpp
  repository.cpp
  ontologyloader.cpp
  ontologymanagermodel.cpp
  graphretriever.cpp
  removablemediamodel.cpp
  datamanagementmodel.cpp
  datamanagementadaptor.cpp
  datamanagementcommand.cpp
  classandpropertytree.cpp
  resourcemerger.cpp
  resourceidentifier.cpp
  resourcewatchermanager.cpp
  resourcewatcherconnection.cpp
  graphmaintainer.cpp
  virtuosoinferencemodel.cpp
  ${libnepomukcore_SOURCE_DIR}/datamanagement/dbustypes.cpp
  )

qt4_add_dbus_adaptor(storage_SRCS
  ../../interfaces/org.kde.nepomuk.OntologyManager.xml
  ontologyloader.h
  Nepomuk2::OntologyLoader)

qt4_add_dbus_adaptor(storage_SRCS
  ../../interfaces/org.kde.nepomuk.ResourceWatcherConnection.xml
  resourcewatcherconnection.h
  Nepomuk2::ResourceWatcherConnection)

set( BackupSyncService_SRCS
  # BackupSync Service
  backupsync/service/logstorage.cpp
  backupsync/service/backupsyncservice.cpp
  backupsync/service/diffgenerator.cpp
  backupsync/service/syncmanager.cpp
  backupsync/service/backupmanager.cpp
  backupsync/service/dbusoperators.cpp
  backupsync/service/tools.cpp
  backupsync/service/changelog.cpp
  backupsync/service/changelogrecord.cpp
  backupsync/service/identificationset.cpp
  backupsync/service/syncfile.cpp
  backupsync/service/backupgenerationjob.cpp
  )

qt4_add_dbus_adaptor( BackupSyncService_SRCS
                      ../../interfaces/org.kde.nepomuk.services.nepomukbackupsync.backupmanager.xml
                      backupsync/service/backupmanager.h Nepomuk2::BackupManager )
qt4_add_dbus_adaptor( BackupSyncService_SRCS
                      ../../interfaces/org.kde.nepomuk.services.nepomukbackupsync.syncmanager.xml
                      backupsync/service/syncmanager.h Nepomuk2::SyncManager )

#--------- Ontologies -------#
soprano_add_ontology(BackupSyncService_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/../../ontologies/nrio.trig
  "NRIO"
  "Nepomuk::Vocabulary"
  "trig")

kde4_add_plugin(nepomukstorage ${storage_SRCS} ${BackupSyncService_SRCS})

target_link_libraries(nepomukstorage
  ${SOPRANO_LIBRARIES}
  ${SOPRANO_SERVER_LIBRARIES}
  ${KDE4_KDECORE_LIBS}
  ${KDE4_KIO_LIBS}
  ${KDE4_SOLID_LIBS}
  nepomukcore
  nepomukcommon
  nepomuksync
  )

install(
  FILES nepomukstorage.desktop
  DESTINATION ${SERVICES_INSTALL_DIR})

install(
  FILES nepomukstorage.notifyrc
  DESTINATION ${DATA_INSTALL_DIR}/nepomukstorage)

install(
  TARGETS nepomukstorage
  DESTINATION ${PLUGIN_INSTALL_DIR})
# -----------------------------

add_subdirectory(backupsync)
add_subdirectory(rcgen)
add_subdirectory(test)
