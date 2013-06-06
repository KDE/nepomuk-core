project(nepomuksearch)

include_directories(
  ${QT_INCLUDES}
  ${KDE4_INCLUDES}
  ${SOPRANO_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}
  ${NEPOMUK_CORE_INCLUDE_DIR}
  )

set( SRCS
  main.cpp
)

kde4_add_executable(nepomuksearch ${SRCS})

target_link_libraries(nepomuksearch
  ${KDE4_KDECORE_LIBS}
  ${SOPRANO_LIBRARIES}
  nepomukcore
  )

install(TARGETS nepomuksearch
        DESTINATION ${BIN_INSTALL_DIR})
