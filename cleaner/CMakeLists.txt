project(nepomukcleaner)

include_directories(${nepomuk-core_SOURCE_DIR}/libnepomukcore)

set( cleaner_SRCS
  jobmodel.cpp
  cleaningjobs.cpp
  main.cpp
  mainwindow.cpp
)

kde4_add_app_icon( cleaner_SRCS "${KDE4_INSTALL_DIR}/share/icons/oxygen/*/apps/nepomuk.png")

kde4_add_executable( nepomukcleaner ${cleaner_SRCS} )

target_link_libraries( nepomukcleaner
  ${KDE4_KDEUI_LIBS}
  ${KDE4_KIO_LIBS}
  ${SOPRANO_LIBRARIES}
  nepomukcore
  )

install(TARGETS nepomukcleaner
        DESTINATION ${BIN_INSTALL_DIR})
install(PROGRAMS nepomukcleaner.desktop
  DESTINATION ${XDG_APPS_INSTALL_DIR})
