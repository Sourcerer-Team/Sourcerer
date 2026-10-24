# Tests for lib/SvnRepo. Built as part of the main build (src/CMakeLists.txt)
# when SOURCERER_BUILD_SVNREPO_TESTS is ON, so the SvnRepo target and its
# Subversion dependencies are reused. They need the svn, svnadmin and svnlook
# executables (PATH, or TortoiseSVN's command line tools) and are skipped
# without them. See docs/SVN_TEST_PLAN.md.
#
#   svnrepo_test   synchronous SvnRepo API on file:// repositories
#                  (tst_svnrepo.cpp)
#   svnsync_test   two working copies of one repository (tst_svnsync.cpp)
#   svnrepomanager_test  the SvnRepoManager component (tst_svnrepomanager.cpp)
#   svnauth_test   credentials over svn:// and the server certificate policy
#                  (tst_svnauth.cpp; needs svnserve)

find_package(Qt6 REQUIRED COMPONENTS Core Network Test)
find_package(OpenSSL REQUIRED)

# Test hooks in SvnRepo (SvnRepo::Testing) and GitRepo (GitRepo::Testing):
# SvnRepo keeps credentials in GitRepo's credential storage, whose master key
# the tests replace so the OS keychain is left alone.
target_compile_definitions(SvnRepo PRIVATE SOURCERER_SVNREPO_TESTING)
target_compile_definitions(GitRepo PRIVATE SOURCERER_GITREPO_TESTING)

function(addSvnRepoTest name)
  add_executable(${name} ${ARGN} SvnTestHelpers.h)

  set_target_properties(${name} PROPERTIES
    AUTOMOC ON
    # Next to the application, so the deployed Qt libraries are found.
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

  target_compile_definitions(${name} PRIVATE
    SOURCERER_SVNREPO_TESTING
    SOURCERER_GITREPO_TESTING)

  includeLib(${name} SvnRepo)
  # SvnRepo and GitRepo use EventManager without linking it, like the
  # application does.
  includeComponent(${name} EventManager)
  includeComponent(${name} ConfigManager)

  target_include_directories(${name} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/libgit2/include)

  # The libgit2 libraries the application links (for GitRepo), see
  # src/CMakeLists.txt.
  target_link_libraries(${name} PRIVATE
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

  # zlib is a DLL in the build tree. Release finds a copy next to the
  # application; the Debug one (zd.dll) is only in its own folder.
  add_custom_command(TARGET ${name} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:ZLIB::ZLIB> $<TARGET_FILE_DIR:${name}>)
endfunction()

addSvnRepoTest(svnrepo_test tst_svnrepo.cpp)
addSvnRepoTest(svnsync_test tst_svnsync.cpp)
addSvnRepoTest(svnrepomanager_test tst_svnrepomanager.cpp)
includeComponent(svnrepomanager_test SvnRepoManager)
includeComponent(svnrepomanager_test ComponentManager)
# GitRepoResult, which carries the clone progress.
includeComponent(svnrepomanager_test GitRepoManager)
addSvnRepoTest(svnauth_test tst_svnauth.cpp SvnServe.h TlsHandshakeServer.h
  ../gitrepo/FakeCredentialsOverlay.h)
# A TLS server, and the certificates it presents.
target_link_libraries(svnauth_test PRIVATE Qt6::Network OpenSSL::Crypto)

# QtTest's report doesn't reach ctest when its output is redirected, so each
# test also writes it to <name>.txt in this build folder.
foreach(test svnrepo svnsync svnrepomanager svnauth)
  add_test(NAME ${test} COMMAND ${test}_test
    -o ${CMAKE_CURRENT_BINARY_DIR}/${test}.txt,txt)
endforeach()
