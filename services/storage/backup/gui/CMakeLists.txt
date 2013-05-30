project(nepomukbackup_gui)

include_directories(
  ${QT_INCLUDES}
  ${KDE4_INCLUDES}
  ${SOPRANO_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_CURRENT_BUILD_DIR}
  )

set( NepomukBackup_GUI_SRCS
  fileconflictwidget.cpp
  backupwizardpages.cpp
  backupwizard.cpp
  main.cpp
)

#----- DBus interfaces --------

set_source_files_properties(
  ../../../../interfaces/org.kde.nepomuk.BackupManager.xml
  PROPERTIES INCLUDE "QtCore/QList")
set_source_files_properties(
  ../../../../interfaces/org.kde.nepomuk.BackupManager.xml
  PROPERTIES INCLUDE "QtCore/QString")

qt4_add_dbus_interface( NepomukBackup_GUI_SRCS
                        ../../../../interfaces/org.kde.nepomuk.BackupManager.xml
                        backupmanagerinterface )

#------UI Interface files ------

kde4_add_ui_files( NepomukBackup_GUI_SRCS
  intropage.ui
  backupsettingspage.ui
  restoreselection.ui
  errorpage.ui
)

kde4_add_app_icon(NepomukBackup_GUI_SRCS "${KDE4_INSTALL_DIR}/share/icons/oxygen/*/apps/nepomuk.png")
kde4_add_executable(nepomukbackup ${NepomukBackup_GUI_SRCS})

target_link_libraries(nepomukbackup
  ${KDE4_KDEUI_LIBS}
  ${KDE4_KIO_LIBS}
  ${SOPRANO_LIBRARIES}
  nepomukcore
  )

install(TARGETS nepomukbackup ${INSTALL_TARGETS_DEFAULT_ARGS} )
install(PROGRAMS nepomukbackup.desktop
  DESTINATION ${XDG_APPS_INSTALL_DIR})
