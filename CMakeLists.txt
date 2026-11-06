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
    src/core/broadcast-cams.cpp
    src/core/broadcast-cams.hpp
    src/core/broadcast.cpp
    src/core/broadcast-screens.cpp
    src/core/broadcast-screens.hpp
    src/core/broadcast.hpp
    src/core/broadcast-inventive.cpp
    src/core/broadcast-inventive.hpp
    src/core/mirror.cpp
    src/core/mirror.hpp
    src/core/failover.cpp
    src/core/failover.hpp
    src/obs/source-health.cpp
    src/obs/source-health.hpp
    src/obs/failover.cpp
    src/obs/failover.hpp
    src/core/roles.cpp
    src/core/user-layouts.hpp
    src/core/user-layouts.cpp
    src/core/roles.hpp
    src/core/style.cpp
    src/core/style.hpp
    src/core/slot-apply.cpp
    src/core/slot-apply.hpp
    src/core/shape.cpp
    src/core/shape.hpp
    src/core/swap.cpp
    src/core/swap.hpp
    src/core/morph.cpp
    src/core/morph.hpp
    src/obs/morph.cpp
    src/obs/morph.hpp
    src/core/portrait-map.cpp
    src/core/portrait-map.hpp
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
    src/obs/corners.cpp
    src/obs/corners.hpp
    src/obs/style-render.cpp
    src/obs/style-render.hpp
    src/obs/builder.cpp
    src/obs/dual.cpp
    src/obs/dual.hpp
    src/obs/builder.hpp
    src/obs/roles.cpp
    src/obs/roles.hpp
    src/obs/user-layouts.cpp
    src/obs/user-layouts.hpp
    src/obs/hotkeys.cpp
    src/obs/hotkeys.hpp
    src/obs/item-style.cpp
    src/obs/item-style.hpp
    src/obs/regie.cpp
    src/obs/regie.hpp
    src/third-party/obs-websocket-api.h
    src/obs/commands.hpp
    src/ui/theme.cpp
    src/ui/theme.hpp
    src/ui/widgets.cpp
    src/ui/widgets.hpp
    src/ui/inspector-dock.cpp
    src/ui/inspector-dock.hpp
    src/ui/layers-panel.cpp
    src/ui/broadcast-gallery.cpp
    src/ui/broadcast-gallery.hpp
    src/ui/regie-dock.cpp
    src/ui/regie-dock.hpp
    src/ui/builder-panel.cpp
    src/ui/builder-panel.hpp
    src/ui/role-ui.cpp
    src/ui/scenes-list.cpp
    src/ui/scenes-list.hpp
    src/ui/role-ui.hpp
    src/ui/style-editor.cpp
    src/ui/style-editor.hpp
    src/ui/layers-panel.hpp
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
