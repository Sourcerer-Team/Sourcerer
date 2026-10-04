cmake_minimum_required(VERSION 3.5)

# Must be set before project() so CMake picks it up for the whole build tree.
# The deployment target is intentionally not forced here: it has to match (or
# be higher than) the one Qt was built with, so CI passes it explicitly with
# -DCMAKE_OSX_DEPLOYMENT_TARGET=<version>.
option(SOURCERER_MACOS_UNIVERSAL
       "Build a universal (arm64 + x86_64) macOS binary" OFF)
if(APPLE AND SOURCERER_MACOS_UNIVERSAL AND NOT CMAKE_OSX_ARCHITECTURES)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING
      "Build architectures for macOS" FORCE)
endif()

project(Sourcerer VERSION 1.0.13 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(QT_NO_PRIVATE_MODULE_WARNING ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

find_package(LibSSH2 REQUIRED)
if(APPLE)
  # Only a probe: the vendored libgit2 pcre2 target is what gets linked, and a
  # clean macOS machine has no system pcre2.
  find_package(pcre2 QUIET)
else()
  find_package(pcre2 REQUIRED)
endif()
find_package(zlib REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets LinguistTools)

set(PROJECT_SOURCES
        main.cpp
)

set(LAUNCHER_SOURCES
        launcher.cpp
)

if(APPLE)
  list(APPEND LAUNCHER_SOURCES macos/MacUpdater.cpp macos/MacUpdater.h)
endif()

if(WIN32)
  set(APP_ICON_RES "${CMAKE_CURRENT_SOURCE_DIR}/../component/GuiManager/resources/icons/app_icon.res")
endif()

qt_add_executable(Sourcerer
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
)

qt_add_executable(SourcererLauncher
        MANUAL_FINALIZATION
        ${LAUNCHER_SOURCES}
)

include("../cmake/helpers.cmake")

includeComponent(Sourcerer ComponentManager)
includeComponent(Sourcerer ConfigManager)
includeComponent(Sourcerer EventManager)
includeComponent(Sourcerer GitRepoManager)
includeComponent(Sourcerer NewsTracker)
includeComponent(Sourcerer UpdateManager)
includeComponent(Sourcerer GuiManager)
includeComponent(Sourcerer StylesheetManager)

includeLib(Sourcerer Fsm)
includeLib(Sourcerer IssueTrackers)
includeLib(Sourcerer Log)
includeLib(Sourcerer RecyclerView)
includeLib(Sourcerer Util)

includeComponent(SourcererLauncher ConfigManager)
# Header-only helpers (UpdateStaging.h); the launcher must not link Util.
includeLibFiles(SourcererLauncher Util)

if (WIN32)
  set(ADDITIONAL_LIBRARIES user32.lib Dwmapi.lib secur32.lib)
elseif (APPLE)
  # libgit2 auto-detects iconv as "provided by libc", but on macOS it lives in
  # the separate libiconv library.
  set(ADDITIONAL_LIBRARIES iconv)
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

target_link_libraries(SourcererLauncher PRIVATE
        Qt6::Core
        ${ADDITIONAL_LIBRARIES}
        "${APP_ICON_RES}")

set_target_properties(Sourcerer PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER www.sourcererapp.com
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE TRUE
)

set_target_properties(SourcererLauncher PROPERTIES
        WIN32_EXECUTABLE TRUE
)

if (APPLE)
  if (CMAKE_OSX_DEPLOYMENT_TARGET)
    set(SOURCERER_MACOS_MIN_VERSION "${CMAKE_OSX_DEPLOYMENT_TARGET}")
  else ()
    # Without an explicit deployment target the compiler targets the macOS
    # version of the build machine, so the result may not run on older systems.
    message(WARNING "CMAKE_OSX_DEPLOYMENT_TARGET is not set; the app will "
                    "require the macOS version of this machine. Pass e.g. "
                    "-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 for release builds.")
    set(SOURCERER_MACOS_MIN_VERSION "13.0")
  endif ()

  set(SOURCERER_MACOS_ICON
      "${CMAKE_CURRENT_SOURCE_DIR}/../component/GuiManager/resources/icons/app_icon.icns")
  target_sources(Sourcerer PRIVATE "${SOURCERER_MACOS_ICON}")
  set_source_files_properties("${SOURCERER_MACOS_ICON}" PROPERTIES
          MACOSX_PACKAGE_LOCATION Resources)

  # The application version is the single source of truth in GitVersion.h.in
  # (the release workflows read it from there as well).
  file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../component/ConfigManager/GitVersion.h.in"
       _sourcerer_version_line REGEX "APPLICATION_VERSION *= *\"[^\"]+\"")
  string(REGEX REPLACE ".*APPLICATION_VERSION *= *\"([^\"]+)\".*" "\\1"
         SOURCERER_APP_VERSION "${_sourcerer_version_line}")
  if (NOT SOURCERER_APP_VERSION)
    set(SOURCERER_APP_VERSION "${PROJECT_VERSION}")
  endif ()

  # Custom Info.plist templates are filled from CMake variables (not from the
  # target properties used by the default template).
  set(MACOSX_BUNDLE_EXECUTABLE_NAME Sourcerer)
  set(MACOSX_BUNDLE_GUI_IDENTIFIER www.sourcererapp.com)
  set(MACOSX_BUNDLE_ICON_FILE app_icon.icns)
  set(MACOSX_BUNDLE_BUNDLE_VERSION "${SOURCERER_APP_VERSION}")
  set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${SOURCERER_APP_VERSION}")

  set_target_properties(Sourcerer PROPERTIES
          MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/macos/Info.plist.in"
  )

  # The updater launches SourcererLauncher from next to the main executable
  # (see launcherPath() in main.cpp), so it must live in Contents/MacOS.
  add_dependencies(Sourcerer SourcererLauncher)
  add_custom_command(TARGET Sourcerer POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different
                  "$<TARGET_FILE:SourcererLauncher>"
                  "$<TARGET_BUNDLE_CONTENT_DIR:Sourcerer>/MacOS/SourcererLauncher"
          COMMENT "Embedding SourcererLauncher into the application bundle"
  )

  set(SOURCERER_JWT_PUBLIC_KEY
      "${CMAKE_CURRENT_SOURCE_DIR}/../keys/cloud_jwt_public_key.pem")
  if (EXISTS "${SOURCERER_JWT_PUBLIC_KEY}")
    add_custom_command(TARGET Sourcerer POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_BUNDLE_CONTENT_DIR:Sourcerer>/Resources/keys"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${SOURCERER_JWT_PUBLIC_KEY}"
                    "$<TARGET_BUNDLE_CONTENT_DIR:Sourcerer>/Resources/keys/cloud_jwt_public_key.pem"
            COMMENT "Embedding JWT public key into the application bundle"
    )
  endif ()
endif ()

qt_finalize_target(Sourcerer)
qt_finalize_target(SourcererLauncher)

install(TARGETS Sourcerer SourcererLauncher
        BUNDLE DESTINATION .
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})


if (WIN32)
  set(_windeployqt_args
      --release
      --no-translations)
  if(DEFINED ENV{OPENSSL_ROOT_DIR} AND NOT "$ENV{OPENSSL_ROOT_DIR}" STREQUAL "")
    list(APPEND _windeployqt_args --openssl-root "$ENV{OPENSSL_ROOT_DIR}")
  endif()

  add_custom_command(TARGET Sourcerer POST_BUILD
            COMMAND "${Qt6_DIR}/../../../bin/windeployqt.exe"
            ${_windeployqt_args}
            "$<TARGET_FILE:Sourcerer>"
            COMMENT "Deploying Qt runtime dependencies"
    )
endif ()
