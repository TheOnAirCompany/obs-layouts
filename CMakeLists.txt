cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)
option(ENABLE_BUILD_LEGACY "Enable legacy build" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core Svg)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets Qt6::Svg)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON AUTOMOC_COMPILER_PREDEFINES OFF
  )
endif()

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.cpp
    src/core/geometry.cpp
    src/core/geometry.hpp
    src/core/item-model.cpp
    src/core/item-model.hpp
    src/core/arrange.cpp
    src/core/arrange.hpp
    src/core/grid.cpp
    src/core/grid.hpp
    src/core/snapping.cpp
    src/core/snapping.hpp
    src/core/expression.cpp
    src/core/expression.hpp
    src/core/guides.cpp
    src/core/guides.hpp
    src/core/layout.cpp
    src/core/layout.hpp
    src/obs/obs-convert.hpp
    src/obs/store.cpp
    src/obs/store.hpp
    src/obs/edit-context.cpp
    src/obs/edit-context.hpp
    src/obs/edit-session.cpp
    src/obs/edit-session.hpp
    src/obs/commands.cpp
    src/obs/constraints.cpp
    src/obs/constraints.hpp
    src/obs/hotkeys.cpp
    src/obs/hotkeys.hpp
    src/obs/commands.hpp
    src/ui/theme.cpp
    src/ui/theme.hpp
    src/ui/widgets.cpp
    src/ui/widgets.hpp
    src/ui/inspector-dock.cpp
    src/ui/inspector-dock.hpp
    src/ui/canvas-view.cpp
    src/ui/canvas-view.hpp
    src/ui/ruler.cpp
    src/ui/ruler.hpp
    src/ui/canvas-dock.cpp
    src/ui/canvas-dock.hpp
    src/ui/layouts-window.cpp
    src/ui/layouts-window.hpp
    resources/layouts.qrc
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

option(LAYOUTS_SELFTEST "Build the in-OBS self-test (development only)" OFF)
if(LAYOUTS_SELFTEST)
  target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/selftest.cpp)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE LAYOUTS_SELFTEST)
endif()
