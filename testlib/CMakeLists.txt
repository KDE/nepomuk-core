project (nepomuktestlib)

# Set minimum versions of packages we need
set (KDE_MIN_VERSION "4.8.0")

# Find all the required packages
find_package (KDE4 ${KDE_MIN_VERSION} REQUIRED)
find_package (NepomukCore REQUIRED)
find_package (Soprano REQUIRED)
find_package (SharedDesktopOntologies  REQUIRED)

# Include some stuff
include (KDE4Defaults)
include (MacroLibrary)

# Add some misc definitions
add_definitions (${KDE4_DEFINITIONS}
                 -DDISABLE_NEPOMUK_LEGACY
)

# Set the basic include directories
include_directories (${KDE4_INCLUDES}
                     ${NEPOMUK_CORE_INCLUDE_DIR}
                     ${CMAKE_SOURCE_DIR}
                     ${CMAKE_BINARY_DIR}
                     ${CMAKE_CURRENT_SOURCE_DIR}
                     ${CMAKE_CURRENT_BINARY_DIR}
)

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
