project(nepomukcleaner)

set( cleaner_SRCS
  jobmodel.cpp
  cleaningjobs.cpp
  main.cpp
  mainwindow.cpp
)

kde4_add_executable( nepomukcleaner ${cleaner_SRCS} )

target_link_libraries( nepomukcleaner
  ${KDE4_KDEUI_LIBS}
  ${KDE4_KIO_LIBS}
  ${SOPRANO_LIBRARIES}
  nepomukcore
  )

install(TARGETS nepomukcleaner
        DESTINATION ${BIN_INSTALL_DIR})
