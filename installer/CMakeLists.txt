cmake_minimum_required(VERSION 3.16)
project(SourcererInstaller)

# Find Qt Installer Framework
find_program(QT_IFW_BINARYCREATOR
    NAMES binarycreator binarycreator.exe
    PATHS
        "C:/Qt/QtIFW-4.0.0/bin"
        "/usr/local/Qt/QtIFW-4.0.0/bin"
        "$ENV{HOME}/Qt/QtIFW-4.0.0/bin"
        "$ENV{QT_IFW_DIR}/bin"
    DOC "Qt Installer Framework binarycreator tool"
)

if(NOT QT_IFW_BINARYCREATOR)
    message(WARNING "Qt Installer Framework binarycreator not found. Installer target will not be available.")
    message(WARNING "Please install Qt IFW 4.0 from https://download.qt.io/official_releases/qt-installer-framework/")
    return()
endif()

message(STATUS "Found Qt IFW binarycreator: ${QT_IFW_BINARYCREATOR}")

# Paths
set(INSTALLER_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(CONFIG_DIR ${INSTALLER_DIR}/config)
set(PACKAGES_DIR ${INSTALLER_DIR}/packages)
set(DATA_DIR ${PACKAGES_DIR}/com.sourcerer.app/data)

# Platform-specific settings
if(WIN32)
    set(INSTALLER_NAME "Sourcerer-Windows-Installer.exe")
    set(SOURCERER_EXECUTABLE "$<TARGET_FILE:Sourcerer>")
elseif(APPLE)
    set(INSTALLER_NAME "Sourcerer-macOS-Installer")
    set(SOURCERER_BUNDLE "$<TARGET_BUNDLE_DIR:Sourcerer>")
else()
    set(INSTALLER_NAME "Sourcerer-Linux-Installer")
    set(SOURCERER_EXECUTABLE "$<TARGET_FILE:Sourcerer>")
endif()

# Create custom target for installer
add_custom_target(installer
    DEPENDS Sourcerer
    COMMENT "Creating installer package..."
)

# Clean data directory
add_custom_command(TARGET installer PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${DATA_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DATA_DIR}
    COMMENT "Cleaning installer data directory..."
)

# Copy application files
if(WIN32)
    # Windows: Copy executable and deploy Qt dependencies
    add_custom_command(TARGET installer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:Sourcerer> ${DATA_DIR}/
        COMMENT "Copying Sourcerer.exe to installer..."
    )
    
    # Deploy Qt dependencies with windeployqt
    find_program(WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT)
        add_custom_command(TARGET installer POST_BUILD
            COMMAND ${WINDEPLOYQT} --release --no-translations ${DATA_DIR}/Sourcerer.exe
            COMMENT "Deploying Qt dependencies with windeployqt..."
        )
    else()
        message(WARNING "windeployqt not found. Qt dependencies will not be automatically deployed.")
    endif()
    
elseif(APPLE)
    # macOS: Copy .app bundle and deploy Qt dependencies
    add_custom_command(TARGET installer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${SOURCERER_BUNDLE} ${DATA_DIR}/Sourcerer.app
        COMMENT "Copying Sourcerer.app to installer..."
    )
    
    # Deploy Qt dependencies with macdeployqt
    find_program(MACDEPLOYQT macdeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(MACDEPLOYQT)
        add_custom_command(TARGET installer POST_BUILD
            COMMAND ${MACDEPLOYQT} ${DATA_DIR}/Sourcerer.app
            COMMENT "Deploying Qt dependencies with macdeployqt..."
        )
    else()
        message(WARNING "macdeployqt not found. Qt dependencies will not be automatically deployed.")
    endif()
    
else()
    # Linux: Copy executable
    add_custom_command(TARGET installer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:Sourcerer> ${DATA_DIR}/
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DATA_DIR}/lib
        COMMENT "Copying Sourcerer to installer..."
    )
    
    # Note: Linux dependency deployment is complex and should be done with linuxdeployqt or similar
    message(STATUS "Linux: Please use linuxdeployqt manually or the build-installer.sh script for proper dependency deployment")
endif()

# Build installer package
add_custom_command(TARGET installer POST_BUILD
    COMMAND ${QT_IFW_BINARYCREATOR}
        -c ${CONFIG_DIR}/config.xml
        -p ${PACKAGES_DIR}
        ${INSTALLER_DIR}/${INSTALLER_NAME}
    WORKING_DIRECTORY ${INSTALLER_DIR}
    COMMENT "Building installer with binarycreator..."
)

# Print success message
add_custom_command(TARGET installer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Installer created: ${INSTALLER_DIR}/${INSTALLER_NAME}"
)
