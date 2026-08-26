cmake_minimum_required(VERSION 3.5)

project(Sourcerer VERSION 1.0.13 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(QT_NO_PRIVATE_MODULE_WARNING ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

find_package(LibSSH2 REQUIRED)
find_package(pcre2 REQUIRED)
find_package(zlib REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets LinguistTools)

set(PROJECT_SOURCES
        main.cpp
)

if(WIN32)
  set(APP_ICON_RES "${CMAKE_CURRENT_SOURCE_DIR}/../component/GuiManager/resources/icons/app_icon.res")
endif()

qt_add_executable(Sourcerer
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
)

include("../cmake/helpers.cmake")

includeComponent(Sourcerer ComponentManager)
includeComponent(Sourcerer ConfigManager)
includeComponent(Sourcerer EventManager)
includeComponent(Sourcerer GitRepoManager)
includeComponent(Sourcerer SvnRepoManager)
includeComponent(Sourcerer NewsTracker)
includeComponent(Sourcerer UpdateManager)
includeComponent(Sourcerer GuiManager)
includeComponent(Sourcerer StylesheetManager)

includeLib(Sourcerer Common)
includeLib(Sourcerer GitRepo)
includeLib(Sourcerer QtWaitingSpinner)
includeLib(Sourcerer QVTerminal)
includeLib(Sourcerer Fsm)
includeLib(Sourcerer IssueTrackers)
includeLib(Sourcerer RecyclerView)
includeLib(Sourcerer SvnRepo)
includeLib(Sourcerer OpenSSLHelper)
includeLib(Sourcerer Log)
includeLib(Sourcerer Util)


if (WIN32)
  set(ADDITIONAL_LIBRARIES user32.lib Dwmapi.lib secur32.lib)
else ()
  set(ADDITIONAL_LIBRARIES)
endif ()

target_link_libraries(Sourcerer PRIVATE
        Qt6::Widgets
        libssh2
        libgit2
        llhttp
        pcre2
        reftable
        util
        xdiff
        libzip::zip
        ZLIB::ZLIB
        ${ADDITIONAL_LIBRARIES}
        "${APP_ICON_RES}")

set_target_properties(Sourcerer PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER www.sourcererapp.com
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE TRUE
)

qt_finalize_target(Sourcerer)

install(TARGETS Sourcerer
        BUNDLE DESTINATION .
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})


if (WIN32)
  add_custom_command(TARGET Sourcerer POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                    -D WINDEPLOYQT="${Qt6_DIR}/../../../bin/windeployqt.exe"
                    -D TARGET_FILE="$<TARGET_FILE:Sourcerer>"
                    -D OPENSSL_ROOT="$ENV{OPENSSL_ROOT_DIR}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/RunWinDeployQt.cmake"
            COMMENT "Deploying Qt runtime dependencies"
    )
endif ()
