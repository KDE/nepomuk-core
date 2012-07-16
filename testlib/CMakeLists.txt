# Set variables for some special install directories
set (NEPOMUK_TESTLIB_DATA_INSTALL_DIR
     "${DATA_INSTALL_DIR}/Nepomuk/TestLib"
)

set (NEPOMUK_TESTLIB_TOOLS_INSTALL_DIR
     "${NEPOMUK_TESTLIB_DATA_INSTALL_DIR}/tools"
)

set (NEPOMUK_TESTLIB_CMAKE_INSTALL_DIR
     "${DATA_INSTALL_DIR}/cmake/modules"
)

add_subdirectory( lib )
add_subdirectory( test )
