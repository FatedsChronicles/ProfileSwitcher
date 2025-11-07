cmake_minimum_required(VERSION 3.28...3.30)

# Uses the template bootstrap to define ${_name}, ${_version}, toolchains, etc.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

# Keep project name/version driven by the template variables (no hard-coding)
project(${_name} VERSION ${_version} LANGUAGES C CXX)

# This plugin is a frontend dock -> needs obs-frontend-api and Qt
option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

# Require C++20
target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_20)

# Core OBS dependency
find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

# Frontend API for docks
if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
else()
  message(FATAL_ERROR "This plugin is a dock and requires ENABLE_FRONTEND_API=ON")
endif()

# Qt UI (Widgets)
if(ENABLE_QT)
  find_package(Qt6 REQUIRED COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE
      $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON)
else()
  message(FATAL_ERROR "This plugin provides a Qt dock and requires ENABLE_QT=ON")
endif()

# ---- Sources (C++ instead of the old C files) ----
target_sources(${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.cpp
    src/profile_dock.cpp
)

target_include_directories(${CMAKE_PROJECT_NAME}
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Keep output name driven by the template (_name)
set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

# Helpful defines (no project/version hard-coding here)
set(PLUGIN_MIN_OBS_MAJOR 32 CACHE STRING "Minimum supported OBS major version")
target_compile_definitions(${CMAKE_PROJECT_NAME}
  PRIVATE
    PLUGIN_NAME_STR="${_name}"
    PLUGIN_MIN_OBS_MAJOR=${PLUGIN_MIN_OBS_MAJOR}
)

# (Optional) export compile_commands.json for IDEs if you ever work locally
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
