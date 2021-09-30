# cmake module for adding ALL

# Enable ExternalProject CMake module
include(FetchContent)
FetchContent_Declare(
  allfetch
  URL ${CMAKE_CURRENT_SOURCE_DIR}/ALL_20210930.zip
  URL_HASH MD5=841b87748f82da1666bdc26f2038a3a1
)

# Get ALL source and binary directories from CMake project
FetchContent_GetProperties(allfetch)

if (NOT allfetch)
  FetchContent_MakeAvailable(allfetch)
endif ()

set(ALL_LIB "ALL")
