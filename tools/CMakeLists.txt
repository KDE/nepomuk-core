
#
# Scripts
#

# Configure the scripts
configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/with-nepomuk-sandbox.sh.in"
                "${CMAKE_CURRENT_BINARY_DIR}/with-nepomuk-sandbox.sh"
                IMMEDIATE @ONLY
)

configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/runNepomukTest.sh.in"
                "${CMAKE_CURRENT_BINARY_DIR}/runNepomukTest.sh"
                IMMEDIATE @ONLY
)

# Install the script
install (FILES "${CMAKE_CURRENT_SOURCE_DIR}/run_in_new_dbus_session.sh"
         DESTINATION "${NEPOMUK_TESTLIB_TOOLS_INSTALL_DIR}"
         COMPONENT Devel
)

install (FILES "${CMAKE_CURRENT_BINARY_DIR}/with-nepomuk-sandbox.sh"
         DESTINATION "${NEPOMUK_TESTLIB_TOOLS_INSTALL_DIR}"
         COMPONENT Devel
)

install (FILES "${CMAKE_CURRENT_BINARY_DIR}/runNepomukTest.sh"
         DESTINATION "${NEPOMUK_TESTLIB_TOOLS_INSTALL_DIR}"
         COMPONENT Devel
)
