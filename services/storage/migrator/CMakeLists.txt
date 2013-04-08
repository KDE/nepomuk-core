project(nepomukmigrator)

set( MIGRATOR_SRCS
  wizard.cpp
  main.cpp
)

#----- DBus interfaces --------

set_source_files_properties(
  ../../../../interfaces/org.kde.nepomuk.BackupManager.xml
  PROPERTIES INCLUDE "QtCore/QList")
set_source_files_properties(
  ../../../../interfaces/org.kde.nepomuk.BackupManager.xml
  PROPERTIES INCLUDE "QtCore/QString")

qt4_add_dbus_interface( MIGRATOR_SRCS
                        ../../../interfaces/org.kde.nepomuk.BackupManager.xml
                        backupmanagerinterface )

kde4_add_executable(nepomukmigrator ${MIGRATOR_SRCS})

target_link_libraries(nepomukmigrator
  ${KDE4_KDEUI_LIBS}
  ${KDE4_KIO_LIBS}
  ${SOPRANO_LIBRARIES}
  nepomukcore
  )

install(TARGETS nepomukmigrator ${INSTALL_TARGETS_DEFAULT_ARGS} )
