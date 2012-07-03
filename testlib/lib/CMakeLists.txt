#
# nepomukserverrc config file
#
configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/nepomukserverrc.in"
                "${CMAKE_CURRENT_BINARY_DIR}/nepomukserverrc"
                IMMEDIATE @ONLY
)

install (FILES "${CMAKE_CURRENT_BINARY_DIR}/nepomukserverrc"
         DESTINATION ${NEPOMUK_TESTLIB_DATA_INSTALL_DIR}
         COMPONENT Devel
)

#
# CMake Macros
#

configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/NepomukTestLibMacros.cmake.in"
                "${CMAKE_CURRENT_BINARY_DIR}/NepomukTestLibMacros.cmake"
                IMMEDIATE @ONLY
)

install (FILES "${CMAKE_CURRENT_BINARY_DIR}/NepomukTestLibMacros.cmake"
         DESTINATION ${NEPOMUK_TESTLIB_CMAKE_INSTALL_DIR}
         COMPONENT Devel
)

#
# Source code
#

set (nepomuk_testlib_SRCS
    testbase.cpp
)

qt4_add_dbus_interface(nepomuk_testlib_SRCS ${DBUS_INTERFACES_INSTALL_DIR}/org.kde.nepomuk.ServiceManager.xml nepomukservicemanagerinterface)

kde4_add_library (nepomuktest
                  STATIC
                  ${nepomuk_testlib_SRCS}
)

target_link_libraries (nepomuktest
                       ${QT_QTCORE_LIBRARY}
                       ${QT_QTDBUS_LIBRARY}
                       ${NEPOMUK_CORE_LIBRARY}
)

set_target_properties (nepomuktest PROPERTIES
                       VERSION ${GENERIC_LIB_VERSION}
                       SOVERSION ${GENERIC_LIB_SOVERSION}
)

install (TARGETS nepomuktest
         ${INSTALL_TARGETS_DEFAULT_ARGS}
         COMPONENT Devel
)

set( nepomuk_testlib_HEADERS
     testbase.h
     nepomuktest_export.h
)

install (FILES ${nepomuk_testlib_HEADERS}
         DESTINATION ${INCLUDE_INSTALL_DIR}/Nepomuk2
         COMPONENT Devel
)

add_subdirectory(tools)
