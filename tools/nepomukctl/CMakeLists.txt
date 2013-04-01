project(nepomukctl)

find_package(KDE4 REQUIRED)

include(KDE4Defaults)

include_directories(
  ${QT_INCLUDES}
  ${KDE4_INCLUDES}
  ${CMAKE_SOURCE_DIR}
  )

set(SRCS main.cpp)

kde4_add_executable(nepomukctl ${SRCS})

target_link_libraries( nepomukctl
    ${QT_QTCORE_LIBRARY}
    ${QT_QTDBUS_LIBRARY}
    ${KDE4_KDECORE_LIBS}
  )

install(TARGETS nepomukctl
        DESTINATION ${BIN_INSTALL_DIR})
