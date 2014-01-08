project(nepomukbaloomigrator)

set( BALOO_MIGRATOR_SRCS
  main.cpp
  systray.cpp
)

qt4_add_dbus_interface( BALOO_MIGRATOR_SRCS
                        ../interfaces/org.kde.NepomukServer.xml
                        serverinterface )

kde4_add_executable(nepomukbaloomigrator ${BALOO_MIGRATOR_SRCS})

target_link_libraries(nepomukbaloomigrator
  ${KDE4_KDEUI_LIBS}
  ${KDE4_KIO_LIBS}
  ${SOPRANO_LIBRARIES}
  ${BALOO_LIBRARIES}
  nepomukcore
)

install(TARGETS nepomukbaloomigrator ${INSTALL_TARGETS_DEFAULT_ARGS})
