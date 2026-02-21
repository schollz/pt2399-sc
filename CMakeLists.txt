cmake_minimum_required(VERSION 3.20)

project(onebitdelay_ugen VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

set(SC_VERSION_TAG "Version-3.14.1")
option(ONEBITDELAY_UGEN_FETCH_SUPERCOLLIDER "Fetch SuperCollider sources if local headers are unavailable" ON)

find_path(SC_PLUGIN_INTERFACE_INCLUDE_DIR
  NAMES SC_PlugIn.h
  PATHS
    /usr/local/include/SuperCollider/plugin_interface
    /usr/include/SuperCollider/plugin_interface
)

find_path(SC_COMMON_INCLUDE_DIR
  NAMES SC_Types.h
  PATHS
    /usr/local/include/SuperCollider/common
    /usr/include/SuperCollider/common
)

if((NOT SC_PLUGIN_INTERFACE_INCLUDE_DIR OR NOT SC_COMMON_INCLUDE_DIR) AND ONEBITDELAY_UGEN_FETCH_SUPERCOLLIDER)
  include(FetchContent)
  message(STATUS "SuperCollider headers not found locally; fetching ${SC_VERSION_TAG}")
  FetchContent_Declare(
    supercollider
    GIT_REPOSITORY https://github.com/supercollider/supercollider.git
    GIT_TAG ${SC_VERSION_TAG}
    GIT_SHALLOW TRUE
  )
  FetchContent_GetProperties(supercollider)
  if(NOT supercollider_POPULATED)
    FetchContent_Populate(supercollider)
  endif()

  set(SC_PLUGIN_INTERFACE_INCLUDE_DIR "${supercollider_SOURCE_DIR}/include/plugin_interface")
  set(SC_COMMON_INCLUDE_DIR "${supercollider_SOURCE_DIR}/include/common")
endif()

if(NOT SC_PLUGIN_INTERFACE_INCLUDE_DIR OR NOT SC_COMMON_INCLUDE_DIR)
  message(FATAL_ERROR "Could not find SuperCollider plugin headers. Install SuperCollider or enable ONEBITDELAY_UGEN_FETCH_SUPERCOLLIDER.")
endif()

add_library(PT2399UGens MODULE
  src/PT2399.cpp
)

target_include_directories(PT2399UGens PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${SC_PLUGIN_INTERFACE_INCLUDE_DIR}
  ${SC_COMMON_INCLUDE_DIR}
)

if(UNIX AND NOT APPLE)
  target_link_libraries(PT2399UGens PRIVATE m)
endif()

set_target_properties(PT2399UGens PROPERTIES
  PREFIX ""
  OUTPUT_NAME "PT2399UGens"
)

install(TARGETS PT2399UGens
  LIBRARY DESTINATION plugins
  RUNTIME DESTINATION plugins
)

install(FILES
  Classes/PT2399.sc
  DESTINATION Classes
)

install(FILES
  HelpSource/Classes/PT2399.schelp
  DESTINATION HelpSource/Classes
)
