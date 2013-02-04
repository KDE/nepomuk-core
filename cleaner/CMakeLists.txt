project(nepomukcleaner)

include_directories(${nepomuk-core_SOURCE_DIR}/libnepomukcore)

kde4_add_library( nepomukcleaner SHARED cleaningjob.cpp )

set_target_properties(nepomukcleaner PROPERTIES
   VERSION ${GENERIC_LIB_VERSION}
   SOVERSION ${GENERIC_LIB_SOVERSION}
)

target_link_libraries(nepomukcleaner
  ${KDE4_KDECORE_LIBS}
  ${SOPRANO_LIBRARIES}
  nepomukcore
  )

set( cleaner_SRCS
  jobmodel.cpp
  cleaningjobs.cpp
  main.cpp
  mainwindow.cpp
)

kde4_add_executable( nepomukcleaner-bin ${cleaner_SRCS} )

SET_TARGET_PROPERTIES(nepomukcleaner-bin
  PROPERTIES OUTPUT_NAME nepomukcleaner)

target_link_libraries( nepomukcleaner-bin
  nepomukcleaner
  ${KDE4_KDEUI_LIBS}
  ${KDE4_KIO_LIBS}
  ${SOPRANO_LIBRARIES}
  nepomukcore
  )

install(TARGETS nepomukcleaner
        DESTINATION ${LIB_INSTALL_DIR})
install(TARGETS nepomukcleaner-bin
        DESTINATION ${BIN_INSTALL_DIR})
install(PROGRAMS nepomukcleaner.desktop
  DESTINATION ${XDG_APPS_INSTALL_DIR})
install(
  FILES nepomukcleaningjob.desktop
  DESTINATION ${SERVICETYPES_INSTALL_DIR})
install(FILES
  cleaningjob.h
  DESTINATION ${INCLUDE_INSTALL_DIR}/nepomuk2 COMPONENT Devel)
