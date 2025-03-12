cmake_minimum_required(VERSION 3.16)

project(wl-screenrec_GUI VERSION 0.1 LANGUAGES CXX C)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(CAIRO REQUIRED cairo)
pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
pkg_check_modules(WAYLAND_CURSOR REQUIRED wayland-cursor)
pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols>=1.32)
pkg_check_modules(XKBCOMMON REQUIRED xkbcommon)
set(RT_LIB rt)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)
set(PROJECT_C_SOURCES
    slurp/xdg-shell-protocol.c
    slurp/xdg-output-unstable-v1-protocol.c
    slurp/wlr-layer-shell-unstable-v1-protocol.c
    slurp/tablet-unstable-v2-protocol.c
    slurp/main.c
    slurp/render.c
    slurp/pool-buffer.c
    slurp/cursor-shape-v1-protocol.c
    slurp/include/xdg-shell-client-protocol.h
    slurp/include/xdg-output-unstable-v1-client-protocol.h
    slurp/include/wlr-layer-shell-unstable-v1-client-protocol.h
    slurp/include/tablet-unstable-v2-client-protocol.h
    slurp/include/slurp.h slurp/include/slurp_tool.h
    slurp/include/render.h
    slurp/include/pool-buffer.h
    slurp/include/cursor-shape-v1-client-protocol.h
)
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/slurp/include
)
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(wl-screenrec_GUI
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        ${PROJECT_C_SOURCES}
        resources.qrc
        reactcmd.h reactcmd.cpp
        slurp/main.c
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET wl-screenrec_GUI APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(wl-screenrec_GUI SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(wl-screenrec_GUI
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(wl-screenrec_GUI PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_link_libraries(wl-screenrec_GUI PRIVATE
  ${CAIRO_LIBRARIES}
  ${WAYLAND_CLIENT_LIBRARIES}
  ${WAYLAND_CURSOR_LIBRARIES}
  ${XKBCOMMON_LIBRARIES}
  ${RT_LIB}
)
# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.wl-screenrec_GUI)
endif()
set_target_properties(wl-screenrec_GUI PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS wl-screenrec_GUI
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(wl-screenrec_GUI)
endif()
