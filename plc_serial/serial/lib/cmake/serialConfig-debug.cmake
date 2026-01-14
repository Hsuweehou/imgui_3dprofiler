#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "serial::serial" for configuration "Debug"
set_property(TARGET serial::serial APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(serial::serial PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LINK_INTERFACE_LIBRARIES_DEBUG "setupapi"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/seriald.lib"
  )

list(APPEND _cmake_import_check_targets serial::serial )
list(APPEND _cmake_import_check_files_for_serial::serial "${_IMPORT_PREFIX}/lib/seriald.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
