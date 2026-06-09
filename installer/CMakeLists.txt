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
set(INSTALLER_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/Sourcerer-0.2.1-Installer.msi)

# Get the Sourcerer binary directory for file references
get_target_property(SOURCERER_BINARY_DIR Sourcerer BINARY_DIR)
get_filename_component(SOURCERER_BIN_PATH "$<TARGET_FILE_DIR:Sourcerer>" ABSOLUTE)

# Detect MSYS2 installation
if(WIN32)
    if(EXISTS "C:/tools/msys64/usr/bin")
        set(MSYS2_BIN_PATH "C:/tools/msys64/usr/bin")
        message(STATUS "Found MSYS2 at C:/tools/msys64")
    elseif(EXISTS "C:/msys64/usr/bin")
        set(MSYS2_BIN_PATH "C:/msys64/usr/bin")
        message(STATUS "Found MSYS2 at C:/msys64")
    elseif(EXISTS "$ENV{ProgramFiles}/msys64/usr/bin")
        set(MSYS2_BIN_PATH "$ENV{ProgramFiles}/msys64/usr/bin")
        message(STATUS "Found MSYS2 at $ENV{ProgramFiles}/msys64")
    else()
        message(WARNING "MSYS2 installation not found. Please install from https://www.msys2.org/")
        set(MSYS2_BIN_PATH "C:/tools/msys64/usr/bin")  # Default path
    endif()
    
    # Detect Qt MinGW directory
    set(QT_VERSION "6.10.1")
    if(EXISTS "C:/Qt/${QT_VERSION}/mingw_64/bin")
        set(QT_MINGW_BIN_PATH "C:/Qt/${QT_VERSION}/mingw_64/bin")
        message(STATUS "Found Qt MinGW at C:/Qt/${QT_VERSION}/mingw_64")
    else()
        message(WARNING "Qt MinGW bin directory not found")
        set(QT_MINGW_BIN_PATH "C:/Qt/${QT_VERSION}/mingw_64/bin")  # Default path
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
            -d"MSYS2BinPath=${MSYS2_BIN_PATH}"
            -d"QtMinGWBinPath=${QT_MINGW_BIN_PATH}"
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
            "${WXO_FILE}"
        COMMENT "Linking WiX installer..."
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    
    message(STATUS "Installer output: ${INSTALLER_OUTPUT}")
else()
    message(WARNING "WiX-based installer is only available on Windows. Use build-installer.sh for other platforms.")
endif()

        -p ${PACKAGES_DIR}
        ${INSTALLER_DIR}/${INSTALLER_NAME}
    WORKING_DIRECTORY ${INSTALLER_DIR}
    COMMENT "Building installer with binarycreator..."
)

# Print success message
add_custom_command(TARGET installer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Installer created: ${INSTALLER_DIR}/${INSTALLER_NAME}"
)

