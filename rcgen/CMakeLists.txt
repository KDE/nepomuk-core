project(nepomuk2_rcgen)

include_directories(
  ${nepomuk2_rcgen_SOURCE_DIR}
)

set(nepomuk2_rcgen_SRCS
  codegenerator.cpp
  property.cpp
  rcgen.cpp
  resourceclass.cpp
  safecode.cpp
  ontologyparser.cpp)

qt4_add_resources(nepomuk2_rcgen_SRCS templates.qrc)
kde4_add_executable(nepomuk2-rcgen NOGUI ${nepomuk2_rcgen_SRCS})
target_link_libraries(nepomuk2-rcgen
  ${QT_QTCORE_LIBRARY}
  ${QT_QTDBUS_LIBRARY}
  ${SOPRANO_LIBRARIES}
  ${KDE4_KDECORE_LIBS}
)

install(TARGETS nepomuk2-rcgen ${INSTALL_TARGETS_DEFAULT_ARGS} )
