# Tests for lib/GitRepo. Built as part of the main build (src/CMakeLists.txt)
# when SOURCERER_BUILD_GITREPO_TESTS is ON, so the GitRepo target and libgit2
# are reused. They need the git executable and are skipped without it.

find_package(Qt6 REQUIRED COMPONENTS Core Test)

add_executable(gitrepo_test tst_gitrepo.cpp)

set_target_properties(gitrepo_test PROPERTIES
  AUTOMOC ON
  # Next to the application, so the deployed Qt libraries are found.
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

includeLib(gitrepo_test GitRepo)
# GitRepo uses EventManager without linking it, like the application does.
includeComponent(gitrepo_test EventManager)
includeComponent(gitrepo_test ConfigManager)

target_include_directories(gitrepo_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/libgit2/include)

# The libgit2 libraries the application links, see src/CMakeLists.txt.
target_link_libraries(gitrepo_test PRIVATE
  Qt6::Core
  Qt6::Test
  libssh2
  libgit2
  llhttp
  pcre2
  reftable
  util
  xdiff
  ZLIB::ZLIB
  ${ADDITIONAL_LIBRARIES})

add_test(NAME gitrepo COMMAND gitrepo_test)
