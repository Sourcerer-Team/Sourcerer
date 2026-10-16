if(NOT DEFINED WINDEPLOYQT OR WINDEPLOYQT STREQUAL "")
  message(WARNING "windeployqt path was not provided; skipping Qt deployment")
  return()
endif()

if(NOT DEFINED TARGET_FILE OR TARGET_FILE STREQUAL "")
  message(WARNING "Target executable was not provided; skipping Qt deployment")
  return()
endif()

if(NOT EXISTS "${TARGET_FILE}")
  message(WARNING "Target executable does not exist: ${TARGET_FILE}")
  return()
endif()

file(READ "${TARGET_FILE}" _target_header LIMIT 2 HEX)
if(NOT _target_header STREQUAL "4d5a")
  message(WARNING "Target executable is not a valid PE file: ${TARGET_FILE}")
  return()
endif()

set(_deploy_args --release --no-translations)
if(DEFINED OPENSSL_ROOT AND NOT OPENSSL_ROOT STREQUAL "")
  list(APPEND _deploy_args --openssl-root "${OPENSSL_ROOT}")
endif()
list(APPEND _deploy_args "${TARGET_FILE}")

get_filename_component(_target_dir "${TARGET_FILE}" DIRECTORY)
message(STATUS "Deploying Qt runtime dependencies for ${TARGET_FILE}")
execute_process(
  COMMAND "${WINDEPLOYQT}" ${_deploy_args}
  WORKING_DIRECTORY "${_target_dir}"
  RESULT_VARIABLE _deploy_result
)

if(NOT _deploy_result EQUAL 0)
  message(WARNING "windeployqt failed with exit code ${_deploy_result}; continuing build")
endif()
