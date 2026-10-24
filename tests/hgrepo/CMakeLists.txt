# Tests for lib/HgRepo. Built as part of the main build (src/CMakeLists.txt)
# when SOURCERER_BUILD_HGREPO_TESTS is ON. They need the hg executable and are
# skipped when it isn't found.

find_package(Qt6 REQUIRED COMPONENTS Core Network Test)

add_executable(hgrepo_test tst_hgrepo.cpp HgAuthServer.h)

set_target_properties(hgrepo_test PROPERTIES
  AUTOMOC ON
  # Next to the application, so the deployed Qt libraries are found.
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

includeLib(hgrepo_test HgRepo)
includeComponent(hgrepo_test ConfigManager)

target_link_libraries(hgrepo_test PRIVATE
  Qt6::Core
  Qt6::Network
  Qt6::Test
  ${ADDITIONAL_LIBRARIES})

add_test(NAME hgrepo COMMAND hgrepo_test)

# The HgRepoManager component, driven through events. Git and Subversion
# subrepo tests need the git and svn command line tools, which Mercurial runs.
add_executable(hgrepomanager_test tst_hgrepomanager.cpp HgAuthServer.h)

set_target_properties(hgrepomanager_test PROPERTIES
  AUTOMOC ON
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

includeComponent(hgrepomanager_test HgRepoManager)
includeComponent(hgrepomanager_test EventManager)
includeComponent(hgrepomanager_test ComponentManager)

target_include_directories(hgrepomanager_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/libgit2/include)

# The libgit2 and Subversion libraries the application links, see
# src/CMakeLists.txt.
target_link_libraries(hgrepomanager_test PRIVATE
  Qt6::Core
  Qt6::Network
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

# The passphrase tests run the askpass program next to the test.
add_dependencies(hgrepomanager_test SourcererAskPass)

add_test(NAME hgrepomanager COMMAND hgrepomanager_test)
