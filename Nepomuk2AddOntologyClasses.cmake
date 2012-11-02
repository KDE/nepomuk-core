#
# Use the Nepomuk resource class generator to generate convinient Resource subclasses
# from ontologies.
#
# Usage:
#   NEPOMUK2_ADD_ONTOLOGY_CLASSES(<sources-var>
#         [ONTOLOGIES] <onto-file1> [<onto-file2> ...]
#         [CLASSES <class1> [<class2> ...]]
#         [VISIBILITY <visibility-name>]
#       )
#
# The optional CLASSES parameter allows to specify the classes to be generated (RDF URIs) in
# case one does not want all classes in the ontologies to be generated.
#
# The optional VISIBILITY parameter allows to set the gcc visibility in order
# to make the generated classes usable in a publically exported API. The <visibility-name> is used to create
# the name of the export macro and the export include file. Thus, when using "VISIBILITY foobar" include
# file "foobar_export.h" needs to define FOOBAR_EXPORT.
#
# Copyright (c) 2009 Sebastian Trueg <trueg@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
macro(NEPOMUK2_ADD_ONTOLOGY_CLASSES _sources)
  # extract arguments
  set(_current_arg_type "onto")
  foreach(_arg ${ARGN})
    if(${_arg} STREQUAL "ONTOLOGIES")
      set(_current_arg_type "onto")
    elseif(${_arg} STREQUAL "VISIBILITY")
      set(_current_arg_type "visib")
    elseif(${_arg} STREQUAL "CLASSES")
      set(_current_arg_type "class")
    else(${_arg} STREQUAL "ONTOLOGIES")
      if(${_current_arg_type} STREQUAL "onto")
        list(APPEND _ontologies ${_arg})
        get_filename_component(_filename ${_arg} NAME)
        list(APPEND _ontofilenames ${_filename})
      elseif(${_current_arg_type} STREQUAL "class")
        list(APPEND _classes "--class" "${_arg}")
      else(${_current_arg_type} STREQUAL "onto")
        set(_visibility "--visibility" "${_arg}")
      endif(${_current_arg_type} STREQUAL "onto")
    endif(${_arg} STREQUAL "ONTOLOGIES")
  endforeach(_arg)

  # find our helper program (first in the install dir, then everywhere)
  if(NOT WINCE)
      find_program(RCGEN nepomuk2-rcgen PATHS ${KDE4_BIN_INSTALL_DIR} ${BIN_INSTALL_DIR})
  else(NOT WINCE)
      find_program(RCGEN nepomuk2-rcgen PATHS ${HOST_BINDIR} NO_DEFAULT_PATH)
  endif(NOT WINCE)


  if(NOT RCGEN)
    message(SEND_ERROR "Failed to find the Nepomuk source generator" )
  else(NOT RCGEN)
    file(TO_NATIVE_PATH ${RCGEN} RCGEN)

    # we generate the files in the current binary dir
    set(_targetdir ${CMAKE_CURRENT_BINARY_DIR})

    # generate the list of source and header files
    execute_process(
      COMMAND ${RCGEN} --listheaders --prefix ${_targetdir}/ ${_classes} ${_visibility} ${_ontologies}
      OUTPUT_VARIABLE _out_headers
      RESULT_VARIABLE rcgen_result
      )
    if(NOT ${rcgen_result} EQUAL 0)
      message(SEND_ERROR "Running ${RCGEN} to generate list of headers failed with error code ${rcgen_result}")
    endif(NOT ${rcgen_result} EQUAL 0)

    execute_process(
      COMMAND ${RCGEN} --listsources --prefix ${_targetdir}/ ${_classes} ${_visibility} ${_ontologies}
      OUTPUT_VARIABLE _out_sources
      RESULT_VARIABLE rcgen_result
      )
    if(NOT ${rcgen_result} EQUAL 0)
      message(SEND_ERROR "Running ${RCGEN} to generate list of sources failed with error code ${rcgen_result}")
    endif(NOT ${rcgen_result} EQUAL 0)

    add_custom_command(OUTPUT ${_out_headers} ${_out_sources}
      COMMAND ${RCGEN} --writeall --target ${_targetdir}/ ${_classes} ${_visibility} ${_ontologies}
      DEPENDS ${_ontologies}
      COMMENT "Generating ontology source files from ${_ontofilenames}"
      )

    # make sure the includes are found
    include_directories(${_targetdir})

    # finally append the source files to the source list
    list(APPEND ${_sources} ${_out_sources})
  endif(NOT RCGEN)

  # reset variable names used
  unset(_current_arg_type)
  unset(_arg)
  unset(_ontologies)
  unset(_ontofilenames)
  unset(_classes)
  unset(_visibility)
  unset(_targetdir)
  unset(_out_headers)
  unset(_out_sources)
  unset(rcgen_result)
endmacro(NEPOMUK2_ADD_ONTOLOGY_CLASSES)
