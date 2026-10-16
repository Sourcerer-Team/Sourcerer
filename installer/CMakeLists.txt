cmake_minimum_required(VERSION 3.16)
project(SourcererInstaller)

# Find WiX Toolset
find_program(WIX_CANDLE
    NAMES candle candle.exe
    PATHS
        "C:/Program Files (x86)/WiX Toolset v3.14/bin"
        "C:/Program Files (x86)/WiX Toolset v3.13/bin"
        "C:/Program Files (x86)/WiX Toolset v3.12/bin"
        "$ENV{WIX}/bin"
    DOC "WiX Candle compiler"
)

find_program(WIX_LIGHT
    NAMES light light.exe
    PATHS
        "C:/Program Files (x86)/WiX Toolset v3.14/bin"
        "C:/Program Files (x86)/WiX Toolset v3.13/bin"
        "C:/Program Files (x86)/WiX Toolset v3.12/bin"
        "$ENV{WIX}/bin"
    DOC "WiX Light linker"
)

if(NOT WIX_CANDLE OR NOT WIX_LIGHT)
    message(WARNING "WiX Toolset not found. Installer target will not be available.")
    message(WARNING "Please install WiX Toolset v3.11+ from https://wixtoolset.org/")
    return()
endif()

message(STATUS "Found WiX Candle: ${WIX_CANDLE}")
message(STATUS "Found WiX Light: ${WIX_LIGHT}")

# Paths
set(INSTALLER_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(WXS_FILE ${INSTALLER_DIR}/Sourcerer.wxs)
set(WXO_FILE ${CMAKE_CURRENT_BINARY_DIR}/Sourcerer.wixobj)
set(INSTALLER_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/Sourcerer-Installer.msi)
set(SOURCERER_VERSION_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../component/ConfigManager/GitVersion.h.in)

file(READ "${SOURCERER_VERSION_FILE}" SOURCERER_VERSION_CONTENTS)
string(REGEX MATCH [=[APPLICATION_VERSION[ \t]*=[ \t]*"([^"]+)"]=] _sourcerer_version_match "${SOURCERER_VERSION_CONTENTS}")
set(SOURCERER_VERSION "${CMAKE_MATCH_1}")

if(NOT SOURCERER_VERSION)
    message(FATAL_ERROR "Unable to determine Sourcerer version from ${SOURCERER_VERSION_FILE}")
endif()

message(STATUS "Sourcerer version: ${SOURCERER_VERSION}")

# Get the Sourcerer binary directory for file references
get_target_property(SOURCERER_BINARY_DIR Sourcerer BINARY_DIR)
get_filename_component(SOURCERER_BIN_PATH "$<TARGET_FILE_DIR:Sourcerer>" ABSOLUTE)

# Detect Qt MSVC installation
if(WIN32)
    set(QT_VERSION "6.10.3")
    if(EXISTS "C:/Qt/${QT_VERSION}/msvc2022_64/bin")
        set(QT_MSVC_BIN_PATH "C:/Qt/${QT_VERSION}/msvc2022_64/bin")
        message(STATUS "Found Qt MSVC at C:/Qt/${QT_VERSION}/msvc2022_64")
    else()
        message(WARNING "Qt MSVC bin directory not found")
        set(QT_MSVC_BIN_PATH "C:/Qt/${QT_VERSION}/msvc2022_64/bin")
    endif()
endif()

# Create custom target for installer (Windows only with WiX)
if(WIN32)
    add_custom_target(installer
        DEPENDS Sourcerer
        COMMENT "Creating WiX installer package..."
    )
    
    # Deploy Qt dependencies with windeployqt before building installer
    find_program(WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")
    
    add_custom_command(TARGET installer PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake-build-installer.cmake
        COMMENT "Deploying application and dependencies..."
    )
    
    # Compile WiX source to object file
    add_custom_command(TARGET installer POST_BUILD
        COMMAND ${WIX_CANDLE} 
            -out "${WXO_FILE}" 
            -d"SourcererBinPath=${SOURCERER_BIN_PATH}"
            -d"SourcererVersion=${SOURCERER_VERSION}"
            "${WXS_FILE}"
        COMMENT "Compiling WiX source files..."
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    
    # Link object file to MSI
    add_custom_command(TARGET installer POST_BUILD
        COMMAND ${WIX_LIGHT}
            -out "${INSTALLER_OUTPUT}"
            -ext WixUIExtension
            -ext WixUtilExtension
            -sice:ICE91
            "${WXO_FILE}"
        COMMENT "Linking WiX installer..."
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    
    message(STATUS "Installer output: ${INSTALLER_OUTPUT}")
else()
    message(WARNING "WiX-based installer is only available on Windows. Use build-installer.sh for other platforms.")
endif()


