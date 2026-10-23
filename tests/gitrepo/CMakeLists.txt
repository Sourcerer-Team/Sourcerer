# Tests for lib/GitRepo. Built as part of the main build (src/CMakeLists.txt)
# when SOURCERER_BUILD_GITREPO_TESTS is ON, so the GitRepo target and libgit2
# are reused. They need the git executable and are skipped without it.
#
#   gitrepo_test   local repository operations (tst_gitrepo.cpp)
#   gitsync_test   clone, fetch, pull, push, submodules and LFS (tst_gitsync.cpp)
#   gitflows_test  commit signing and branching workflows (tst_gitflows.cpp)
#   gitcredentials_test  credential and SSH key stores (tst_gitcredentials.cpp)
#   gitrepomanager_test  the GitRepoManager component (tst_gitrepomanager.cpp)
#   gitcertificates_test server certificate policy (tst_gitcertificates.cpp)

find_package(Qt6 REQUIRED COMPONENTS Core Network Test)
find_package(OpenSSL REQUIRED)

# Test hooks in GitRepo, see GitRepo::Testing.
target_compile_definitions(GitRepo PRIVATE SOURCERER_GITREPO_TESTING)

function(addGitRepoTest name)
  add_executable(${name} ${ARGN} GitTestHelpers.h)

  set_target_properties(${name} PROPERTIES
    AUTOMOC ON
    # Next to the application, so the deployed Qt libraries are found.
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

  target_compile_definitions(${name} PRIVATE SOURCERER_GITREPO_TESTING)

  includeLib(${name} GitRepo)
  includeLib(${name} Util)
  # GitRepo uses EventManager without linking it, like the application does.
  includeComponent(${name} EventManager)
  includeComponent(${name} ConfigManager)

  target_include_directories(${name} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/libgit2/include)

  # The libgit2 libraries the application links, see src/CMakeLists.txt.
  target_link_libraries(${name} PRIVATE
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

  # zlib is a DLL in the build tree. Release finds a copy next to the
  # application; the Debug one (zd.dll) is only in its own folder.
  add_custom_command(TARGET ${name} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:ZLIB::ZLIB> $<TARGET_FILE_DIR:${name}>)
endfunction()

addGitRepoTest(gitrepo_test tst_gitrepo.cpp)
addGitRepoTest(gitsync_test tst_gitsync.cpp LfsStubServer.h)
addGitRepoTest(gitflows_test tst_gitflows.cpp)
addGitRepoTest(gitcredentials_test tst_gitcredentials.cpp FakeCredentialsOverlay.h)
addGitRepoTest(gitrepomanager_test tst_gitrepomanager.cpp FakeCredentialsOverlay.h
  GitHttpServer.h)
includeComponent(gitrepomanager_test GitRepoManager)
includeComponent(gitrepomanager_test ComponentManager)
addGitRepoTest(gitcertificates_test tst_gitcertificates.cpp GitHttpServer.h)
# Makes the certificates the tests serve.
target_link_libraries(gitcertificates_test PRIVATE OpenSSL::Crypto)

add_test(NAME gitrepo COMMAND gitrepo_test)
add_test(NAME gitsync COMMAND gitsync_test)
add_test(NAME gitflows COMMAND gitflows_test)
add_test(NAME gitcredentials COMMAND gitcredentials_test)
add_test(NAME gitrepomanager COMMAND gitrepomanager_test)
add_test(NAME gitcertificates COMMAND gitcertificates_test)
