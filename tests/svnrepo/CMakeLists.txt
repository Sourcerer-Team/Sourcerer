# Tests for lib/SvnRepo. Built as part of the main build (src/CMakeLists.txt)
# when SOURCERER_BUILD_SVNREPO_TESTS is ON, so the SvnRepo target and its
# Subversion dependencies are reused.

find_package(Qt6 REQUIRED COMPONENTS Core Test)

add_executable(svnrepo_test tst_svnrepo.cpp)

set_target_properties(svnrepo_test PROPERTIES
  AUTOMOC ON
  # Next to the application, so the deployed Qt libraries are found.
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

includeLib(svnrepo_test SvnRepo)
# SvnRepo and GitRepo use EventManager without linking it, like the
# application does.
includeComponent(svnrepo_test EventManager)

target_include_directories(svnrepo_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/libgit2/include)

target_link_libraries(svnrepo_test PRIVATE
  Qt6::Core
  Qt6::Test
  ${ADDITIONAL_LIBRARIES})

add_test(NAME svnrepo COMMAND svnrepo_test)
