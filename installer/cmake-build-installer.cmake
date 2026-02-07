# CMake script to prepare application files for WiX installer
# This script copies the application and deploys Qt dependencies

find_program(WINDEPLOYQT windeployqt)

if(NOT WINDEPLOYQT)
    message(FATAL_ERROR "windeployqt not found. Unable to deploy Qt dependencies.")
endif()

message(STATUS "Using windeployqt: ${WINDEPLOYQT}")

# Source and destination paths
set(SOURCE_BIN "$<TARGET_FILE_DIR:Sourcerer>")
set(SOURCE_EXE "$<TARGET_FILE:Sourcerer>")

# Create deployment directory if it doesn't exist
if(NOT EXISTS "${SOURCE_BIN}")
    message(FATAL_ERROR "Binary directory does not exist: ${SOURCE_BIN}")
endif()

message(STATUS "Deploying Qt dependencies for: ${SOURCE_EXE}")
message(STATUS "Using source directory: ${SOURCE_BIN}")

# Run windeployqt
execute_process(
    COMMAND ${WINDEPLOYQT} --release --no-translations "${SOURCE_EXE}"
    WORKING_DIRECTORY "${SOURCE_BIN}"
    RESULT_VARIABLE DEPLOY_RESULT
)

if(NOT DEPLOY_RESULT EQUAL 0)
    message(WARNING "windeployqt returned non-zero exit code: ${DEPLOY_RESULT}")
endif()

message(STATUS "Qt deployment completed")
