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
  datamanagementmodel.cpp
  datamanagementadaptor.cpp
  datamanagementcommand.cpp
  classandpropertytree.cpp
  resourcemerger.cpp
  resourceidentifier.cpp
  syncresourceidentifier.cpp
  syncresource.cpp
  resourcewatchermanager.cpp
  resourcewatcherconnection.cpp
  virtuosoinferencemodel.cpp
  typecache.cpp
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

set( Backup_SRCS
  # Backup Sources
  backup/backupmanager.cpp
  backup/dbusoperators.cpp
  backup/backupfile.cpp
  backup/backupgenerationjob.cpp
  backup/backuprestorationjob.cpp
  backup/backupstatementiterator.cpp
  )

qt4_add_dbus_adaptor( Backup_SRCS
                      ../../interfaces/org.kde.nepomuk.BackupManager.xml
                      backup/backupmanager.h Nepomuk2::BackupManager )

set(queryservice_SRCS
  query/queryservice.cpp
  query/folder.cpp
  query/folderconnection.cpp
  query/searchrunnable.cpp
  query/countqueryrunnable.cpp
)

qt4_add_dbus_adaptor(queryservice_SRCS
  ../../interfaces/org.kde.nepomuk.Query.xml
  query/folderconnection.h
  Nepomuk2::Query::FolderConnection)

kde4_add_plugin(nepomukstorage ${storage_SRCS} ${queryservice_SRCS} ${Backup_SRCS})

target_link_libraries(nepomukstorage
  ${SOPRANO_LIBRARIES}
  ${SOPRANO_SERVER_LIBRARIES}
  ${KDE4_KDECORE_LIBS}
  ${KDE4_KIO_LIBS}
  ${KDE4_SOLID_LIBS}
  nepomukcore
  nepomukcommon
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

add_subdirectory(backup)
add_subdirectory(rcgen)
add_subdirectory(test)
