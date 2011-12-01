project(libnepomukcore)

add_definitions(-DKDE_DEFAULT_DEBUG_AREA=300000)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/stuff-to-remove-for-kdelibs-5.0
  ${CMAKE_CURRENT_BINARY_DIR}
)

configure_file(misc/nepomukversion.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/nepomukversion.h)

set(nepomuk_resource_SRCS
  ${libnepomukcore_SOURCE_DIR}/resource/variant.cpp
  ${libnepomukcore_SOURCE_DIR}/resource/resourcedata.cpp
  ${libnepomukcore_SOURCE_DIR}/resource/resourcemanager.cpp
  ${libnepomukcore_SOURCE_DIR}/resource/resourcefiltermodel.cpp
  ${libnepomukcore_SOURCE_DIR}/resource/nepomukmainmodel.cpp
  ${libnepomukcore_SOURCE_DIR}/resource/resource.cpp
  ${libnepomukcore_SOURCE_DIR}/resource/thing.cpp
  ${libnepomukcore_SOURCE_DIR}/resource/file.cpp
  ${libnepomukcore_SOURCE_DIR}/resource/tag.cpp
  ${libnepomukcore_SOURCE_DIR}/resource/nepomukservice.cpp
  ${libnepomukcore_SOURCE_DIR}/resource/graphwrapper.cpp
)

set(nepomuk_types_SRCS
  ${libnepomukcore_SOURCE_DIR}/types/entity.cpp
  ${libnepomukcore_SOURCE_DIR}/types/ontology.cpp
  ${libnepomukcore_SOURCE_DIR}/types/class.cpp
  ${libnepomukcore_SOURCE_DIR}/types/property.cpp
  ${libnepomukcore_SOURCE_DIR}/types/literal.cpp
  ${libnepomukcore_SOURCE_DIR}/types/entitymanager.cpp
)

set(nepomuk_query_SRCS
  ${libnepomukcore_SOURCE_DIR}/query/term.cpp
  ${libnepomukcore_SOURCE_DIR}/query/negationterm.cpp
  ${libnepomukcore_SOURCE_DIR}/query/orterm.cpp
  ${libnepomukcore_SOURCE_DIR}/query/andterm.cpp
  ${libnepomukcore_SOURCE_DIR}/query/comparisonterm.cpp
  ${libnepomukcore_SOURCE_DIR}/query/simpleterm.cpp
  ${libnepomukcore_SOURCE_DIR}/query/groupterm.cpp
  ${libnepomukcore_SOURCE_DIR}/query/resourceterm.cpp
  ${libnepomukcore_SOURCE_DIR}/query/literalterm.cpp
  ${libnepomukcore_SOURCE_DIR}/query/resourcetypeterm.cpp
  ${libnepomukcore_SOURCE_DIR}/query/optionalterm.cpp
  ${libnepomukcore_SOURCE_DIR}/query/query.cpp
  ${libnepomukcore_SOURCE_DIR}/query/filequery.cpp
  ${libnepomukcore_SOURCE_DIR}/query/queryparser.cpp
  ${libnepomukcore_SOURCE_DIR}/query/dateparser.cpp
  ${libnepomukcore_SOURCE_DIR}/query/result.cpp
  ${libnepomukcore_SOURCE_DIR}/query/queryserviceclient.cpp
  ${libnepomukcore_SOURCE_DIR}/query/dbusoperators.cpp
  ${libnepomukcore_SOURCE_DIR}/query/queryserializer.cpp
  ${libnepomukcore_SOURCE_DIR}/query/standardqueries.cpp
)

set_source_files_properties(
  ../interfaces/org.kde.nepomuk.QueryService.xml
  PROPERTIES INCLUDE "dbusoperators_p.h")
set_source_files_properties(
  ../interfaces/org.kde.nepomuk.Query.xml
  PROPERTIES INCLUDE "result.h")

qt4_add_dbus_interface(nepomuk_query_SRCS
  ../interfaces/org.kde.nepomuk.QueryService.xml
  queryserviceinterface)
qt4_add_dbus_interface(nepomuk_query_SRCS
  ../interfaces/org.kde.nepomuk.Query.xml
  queryinterface)

set(nepomuk_misc_SRCS
  ${libnepomukcore_SOURCE_DIR}/misc/utils.cpp
  ${libnepomukcore_SOURCE_DIR}/misc/nepomukversion.cpp
)

set(nepomuk_datamanagement_SRCS
  ${libnepomukcore_SOURCE_DIR}/datamanagement/abstracttimeoutdbusinterface.cpp
  ${libnepomukcore_SOURCE_DIR}/datamanagement/datamanagement.cpp
  ${libnepomukcore_SOURCE_DIR}/datamanagement/dbustypes.cpp
  ${libnepomukcore_SOURCE_DIR}/datamanagement/genericdatamanagementjob.cpp
  ${libnepomukcore_SOURCE_DIR}/datamanagement/simpleresource.cpp
  ${libnepomukcore_SOURCE_DIR}/datamanagement/createresourcejob.cpp
  ${libnepomukcore_SOURCE_DIR}/datamanagement/datamanagementinterface.cpp
  ${libnepomukcore_SOURCE_DIR}/datamanagement/describeresourcesjob.cpp
  ${libnepomukcore_SOURCE_DIR}/datamanagement/resourcewatcher.cpp
  ${libnepomukcore_SOURCE_DIR}/datamanagement/simpleresourcegraph.cpp
  ${libnepomukcore_SOURCE_DIR}/datamanagement/storeresourcesjob.cpp
)

qt4_add_dbus_interface(nepomuk_datamanagement_SRCS
  ../interfaces/org.kde.nepomuk.ResourceWatcherConnection.xml
  resourcewatcherconnectioninterface)

qt4_add_dbus_interface(nepomuk_datamanagement_SRCS
  ../interfaces/org.kde.nepomuk.ResourceWatcher.xml
  resourcewatchermanagerinterface)

set(nepomuk_LIB_SRCS
  ${nepomuk_resource_SRCS}
  ${nepomuk_misc_SRCS}
  ${nepomuk_types_SRCS}
  ${nepomuk_query_SRCS}
  ${nepomuk_datamanagement_SRCS}
  ${libnepomukcore_SOURCE_DIR}/stuff-to-remove-for-kdelibs-5.0/kdbusconnectionpool.cpp
)

soprano_add_ontology(nepomuk_LIB_SRCS "${SHAREDDESKTOPONTOLOGIES_ROOT_DIR}/nie/nie.trig" "NIE" "Nepomuk::Vocabulary" "trig" VISIBILITY "nepomuk")
soprano_add_ontology(nepomuk_LIB_SRCS "${SHAREDDESKTOPONTOLOGIES_ROOT_DIR}/nie/nfo.trig" "NFO" "Nepomuk::Vocabulary" "trig" VISIBILITY "nepomuk")
soprano_add_ontology(nepomuk_LIB_SRCS "${SHAREDDESKTOPONTOLOGIES_ROOT_DIR}/nie/ncal.trig" "NCAL" "Nepomuk::Vocabulary" "trig" VISIBILITY "nepomuk")
soprano_add_ontology(nepomuk_LIB_SRCS "${SHAREDDESKTOPONTOLOGIES_ROOT_DIR}/nie/nco.trig" "NCO" "Nepomuk::Vocabulary" "trig" VISIBILITY "nepomuk")
soprano_add_ontology(nepomuk_LIB_SRCS "${SHAREDDESKTOPONTOLOGIES_ROOT_DIR}/nie/nmo.trig" "NMO" "Nepomuk::Vocabulary" "trig" VISIBILITY "nepomuk")
soprano_add_ontology(nepomuk_LIB_SRCS "${SHAREDDESKTOPONTOLOGIES_ROOT_DIR}/nie/nmm.trig" "NMM" "Nepomuk::Vocabulary" "trig" VISIBILITY "nepomuk")
soprano_add_ontology(nepomuk_LIB_SRCS "${SHAREDDESKTOPONTOLOGIES_ROOT_DIR}/nie/nexif.trig" "NEXIF" "Nepomuk::Vocabulary" "trig" VISIBILITY "nepomuk")
soprano_add_ontology(nepomuk_LIB_SRCS "${SHAREDDESKTOPONTOLOGIES_ROOT_DIR}/pimo/pimo.trig" "PIMO" "Nepomuk::Vocabulary" "trig" VISIBILITY "nepomuk")
soprano_add_ontology(nepomuk_LIB_SRCS "${SHAREDDESKTOPONTOLOGIES_ROOT_DIR}/pimo/tmo.trig" "TMO" "Nepomuk::Vocabulary" "trig" VISIBILITY "nepomuk")
soprano_add_ontology(nepomuk_LIB_SRCS "${SHAREDDESKTOPONTOLOGIES_ROOT_DIR}/nepomuk/nuao.trig" "NUAO" "Nepomuk::Vocabulary" "trig" VISIBILITY "nepomuk")
soprano_add_ontology(nepomuk_LIB_SRCS "${SHAREDDESKTOPONTOLOGIES_ROOT_DIR}/nepomuk/ndo.trig" "NDO" "Nepomuk::Vocabulary" "trig" VISIBILITY "nepomuk")

kde4_add_library(nepomukcore SHARED ${nepomuk_LIB_SRCS})

target_link_libraries(nepomukcore
  ${SOPRANO_LIBRARIES}
  ${SOPRANO_CLIENT_LIBRARIES}
  ${QT_QTCORE_LIBRARY}
  ${QT_QTGUI_LIBRARY}
  ${QT_QTDBUS_LIBRARY}
  kdecore
  kdeui
)

target_link_libraries(nepomukcore
  LINK_INTERFACE_LIBRARIES
  kdecore
  ${SOPRANO_LIBRARIES})


set_target_properties(nepomukcore PROPERTIES
   VERSION ${GENERIC_LIB_VERSION}
   SOVERSION ${GENERIC_LIB_SOVERSION}
)

install(TARGETS nepomukcore EXPORT kdelibsLibraryTargets ${INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES
  nepomuk_export.h
  resource/variant.h
  resource/resourcemanager.h
  resource/nepomukservice.h
  resource/resource.h
  resource/thing.h
  resource/tag.h
  resource/file.h
  types/entity.h
  types/class.h
  types/property.h
  types/ontology.h
  types/literal.h
  misc/utils.h
  query/term.h
  query/negationterm.h
  query/orterm.h
  query/andterm.h
  query/comparisonterm.h
  query/simpleterm.h
  query/groupterm.h
  query/resourceterm.h
  query/literalterm.h
  query/resourcetypeterm.h
  query/optionalterm.h
  query/query.h
  query/filequery.h
  query/result.h
  query/queryserviceclient.h
  query/queryparser.h
  query/standardqueries.h
  query/standardqueries.h
  datamanagement/simpleresource.h
  datamanagement/simpleresourcegraph.h
  datamanagement/datamanagement.h
  datamanagement/createresourcejob.h
  datamanagement/describeresourcesjob.h
  datamanagement/resourcewatcher.h
  datamanagement/storeresourcesjob.h
  ${CMAKE_CURRENT_BINARY_DIR}/nepomukversion.h

  # Vocabularies
  ${CMAKE_CURRENT_BINARY_DIR}/nie.h
  ${CMAKE_CURRENT_BINARY_DIR}/nfo.h
  ${CMAKE_CURRENT_BINARY_DIR}/ncal.h
  ${CMAKE_CURRENT_BINARY_DIR}/nco.h
  ${CMAKE_CURRENT_BINARY_DIR}/nmo.h
  ${CMAKE_CURRENT_BINARY_DIR}/nmm.h
  ${CMAKE_CURRENT_BINARY_DIR}/nexif.h
  ${CMAKE_CURRENT_BINARY_DIR}/pimo.h
  ${CMAKE_CURRENT_BINARY_DIR}/tmo.h
  ${CMAKE_CURRENT_BINARY_DIR}/nuao.h
  ${CMAKE_CURRENT_BINARY_DIR}/ndo.h

  DESTINATION ${INCLUDE_INSTALL_DIR}/nepomuk COMPONENT Devel
)

add_subdirectory(test)

# API docs
find_package(Doxygen)

if(DOXYGEN_EXECUTABLE)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.cmake ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

  add_custom_target(
    apidox
    COMMAND ${DOXYGEN_EXECUTABLE} Doxyfile)
endif(DOXYGEN_EXECUTABLE)
