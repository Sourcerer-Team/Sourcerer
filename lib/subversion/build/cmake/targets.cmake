find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(SVN_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/subversion)
set(SVN_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/subversion)

function(svn_generate_sql_header input_file output_file)
  add_custom_command(
    OUTPUT ${output_file}
    DEPENDS
      ${input_file}
      ${SVN_SOURCE_DIR}/../build/transform_sql.py
    COMMAND
      ${Python3_EXECUTABLE}
      ${SVN_SOURCE_DIR}/../build/transform_sql.py
      ${input_file}
      ${output_file}
    VERBATIM
  )
endfunction()

function(svn_add_common_includes target_name)
  target_include_directories(${target_name} PRIVATE
    ${SVN_SOURCE_DIR}/include
    ${SVN_SOURCE_DIR}/include/private
    ${CMAKE_CURRENT_BINARY_DIR}
    ${SVN_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/../apr
    ${APR_UTIL_BINARY_DIR}
    ${APR_UTIL_SOURCE_DIR}/include
  )
endfunction()

# Names a library like upstream does (libsvn_subr-1.dll, libsvn_subr-1.so).
# On Windows the functions declared in the given headers (the msvc-export
# lists of build.conf, relative to subversion/include) are exported.
function(svn_library_exports target_name)
  string(REGEX REPLACE "^lib" "" output_name "${target_name}")
  set_target_properties(${target_name} PROPERTIES OUTPUT_NAME "${output_name}-1")
  set(headers)
  foreach(header ${ARGN})
    list(APPEND headers "subversion/include/${header}")
  endforeach()
  target_exports(${target_name} ${headers})
  if(WIN32)
    # htonl() and friends, which the static libraries got from the executable.
    target_link_libraries(${target_name} PRIVATE ws2_32)
  endif()
endfunction()



file(MAKE_DIRECTORY
  ${SVN_BINARY_DIR}/libsvn_subr
  ${SVN_BINARY_DIR}/libsvn_wc
  ${SVN_BINARY_DIR}/libsvn_fs_fs
  ${SVN_BINARY_DIR}/libsvn_fs_x
)

svn_generate_sql_header(
  ${SVN_SOURCE_DIR}/libsvn_subr/internal_statements.sql
  ${SVN_BINARY_DIR}/libsvn_subr/internal_statements.h
)
svn_generate_sql_header(
  ${SVN_SOURCE_DIR}/libsvn_wc/wc-queries.sql
  ${SVN_BINARY_DIR}/libsvn_wc/wc-queries.h
)
svn_generate_sql_header(
  ${SVN_SOURCE_DIR}/libsvn_wc/wc-metadata.sql
  ${SVN_BINARY_DIR}/libsvn_wc/wc-metadata.h
)
svn_generate_sql_header(
  ${SVN_SOURCE_DIR}/libsvn_wc/wc-checks.sql
  ${SVN_BINARY_DIR}/libsvn_wc/wc-checks.h
)
svn_generate_sql_header(
  ${SVN_SOURCE_DIR}/libsvn_fs_fs/rep-cache-db.sql
  ${SVN_BINARY_DIR}/libsvn_fs_fs/rep-cache-db.h
)
svn_generate_sql_header(
  ${SVN_SOURCE_DIR}/libsvn_fs_x/rep-cache-db.sql
  ${SVN_BINARY_DIR}/libsvn_fs_x/rep-cache-db.h
)

if(NOT TARGET libsvn_subr)
  file(GLOB SVN_SUBR_SOURCES CONFIGURE_DEPENDS
    ${SVN_SOURCE_DIR}/libsvn_subr/*.c
    ${SVN_SOURCE_DIR}/libsvn_subr/lz4/*.c
    ${SVN_SOURCE_DIR}/libsvn_subr/utf8proc/*.c
  )
  list(FILTER SVN_SUBR_SOURCES EXCLUDE REGEX ".*/utf8proc_data\.c$")
  add_library(libsvn_subr
    ${SVN_SUBR_SOURCES}
    ${SVN_BINARY_DIR}/libsvn_subr/internal_statements.h
  )
  target_compile_definitions(libsvn_subr PRIVATE
    SVN_INTERNAL_LZ4
    SVN_INTERNAL_UTF8PROC
    # The internal utf8proc is compiled into libsvn_subr, not imported.
    UTF8PROC_STATIC
    SVN_CHECKSUM_BACKEND_APR
  )
  svn_add_common_includes(libsvn_subr)
  svn_library_exports(libsvn_subr
    svn_auth.h svn_base64.h svn_cache_config.h svn_checksum.h svn_cmdline.h
    svn_compat.h svn_config.h svn_ctype.h svn_dirent_uri.h svn_dso.h
    svn_error.h svn_hash.h svn_io.h svn_iter.h svn_md5.h svn_mergeinfo.h
    svn_nls.h svn_opt.h svn_path.h svn_pools.h svn_props.h svn_quoprint.h
    svn_sorts.h svn_string.h svn_subst.h svn_time.h svn_types.h svn_user.h
    svn_utf.h svn_version.h svn_xml.h svn_x509.h
    private/svn_atomic.h private/svn_cache.h private/svn_cmdline_private.h
    private/svn_debug.h private/svn_error_private.h private/svn_fspath.h
    private/svn_log.h private/svn_mergeinfo_private.h
    private/svn_opt_private.h private/svn_skel.h private/svn_sqlite.h
    private/svn_utf_private.h private/svn_eol_private.h
    private/svn_token.h private/svn_adler32.h
    private/svn_temp_serializer.h private/svn_io_private.h
    private/svn_sorts_private.h private/svn_auth_private.h
    private/svn_string_private.h private/svn_magic.h
    private/svn_subr_private.h private/svn_mutex.h private/svn_task.h
    private/svn_thread_cond.h private/svn_waitable_counter.h
    private/svn_packed_data.h private/svn_object_pool.h private/svn_cert.h
    private/svn_config_private.h private/svn_dirent_uri_private.h
    private/svn_xml_private.h ../libsvn_subr/crypto.h
  )
  if(WIN32)
    target_link_libraries(libsvn_subr PRIVATE
      advapi32 shfolder ole32 crypt32 version
    )
  endif()
  target_include_directories(libsvn_subr PRIVATE
    ${SVN_SOURCE_DIR}/libsvn_subr
    ${SVN_SOURCE_DIR}/libsvn_subr/lz4
    ${SVN_SOURCE_DIR}/libsvn_subr/utf8proc
    ${SVN_BINARY_DIR}/libsvn_subr
  )
  target_link_libraries(libsvn_subr PRIVATE
    apr-1
    aprutil-1
    ZLIB::ZLIB
    expat::expat
    sqlite3
  )
endif()

if(NOT TARGET libsvn_diff)
  file(GLOB SVN_DIFF_SOURCES CONFIGURE_DEPENDS
    ${SVN_SOURCE_DIR}/libsvn_diff/*.c
  )
  add_library(libsvn_diff ${SVN_DIFF_SOURCES})
  svn_add_common_includes(libsvn_diff)
  svn_library_exports(libsvn_diff
    svn_diff.h private/svn_diff_private.h private/svn_diff_tree.h
  )
  target_include_directories(libsvn_diff PRIVATE
    ${SVN_SOURCE_DIR}/libsvn_diff
  )
  target_link_libraries(libsvn_diff PRIVATE
    libsvn_subr
    apr-1
    aprutil-1
    ZLIB::ZLIB
  )
endif()

if(NOT TARGET libsvn_delta)
  file(GLOB SVN_DELTA_SOURCES CONFIGURE_DEPENDS
    ${SVN_SOURCE_DIR}/libsvn_delta/*.c
  )
  add_library(libsvn_delta ${SVN_DELTA_SOURCES})
  svn_add_common_includes(libsvn_delta)
  svn_library_exports(libsvn_delta
    svn_delta.h private/svn_editor.h private/svn_delta_private.h
  )
  target_include_directories(libsvn_delta PRIVATE
    ${SVN_SOURCE_DIR}/libsvn_delta
  )
  target_link_libraries(libsvn_delta PRIVATE
    libsvn_subr
    libsvn_diff
    apr-1
    aprutil-1
    ZLIB::ZLIB
  )
endif()

if(NOT TARGET libsvn_fs_util)
  file(GLOB SVN_FS_UTIL_SOURCES CONFIGURE_DEPENDS
    ${SVN_SOURCE_DIR}/libsvn_fs_util/*.c
  )
  add_library(libsvn_fs_util ${SVN_FS_UTIL_SOURCES})
  svn_add_common_includes(libsvn_fs_util)
  svn_library_exports(libsvn_fs_util private/svn_fs_util.h)
  target_include_directories(libsvn_fs_util PRIVATE
    ${SVN_SOURCE_DIR}/libsvn_fs_util
  )
  target_link_libraries(libsvn_fs_util PRIVATE
    libsvn_subr
    apr-1
    aprutil-1
  )
endif()

if(NOT TARGET libsvn_fs_fs)
  file(GLOB SVN_FS_FS_SOURCES CONFIGURE_DEPENDS
    ${SVN_SOURCE_DIR}/libsvn_fs_fs/*.c
  )
  add_library(libsvn_fs_fs ${SVN_FS_BUILD_TYPE}
    ${SVN_FS_FS_SOURCES}
    ${SVN_BINARY_DIR}/libsvn_fs_fs/rep-cache-db.h
  )
  svn_add_common_includes(libsvn_fs_fs)
  svn_library_exports(libsvn_fs_fs
    private/svn_fs_fs_private.h ../libsvn_fs_fs/fs_init.h
    ../libsvn_fs_fs/fs_fs.h ../libsvn_fs_fs/fs.h
    ../libsvn_fs_fs/id.h ../libsvn_fs_fs/index.h
    ../libsvn_fs_fs/low_level.h ../libsvn_fs_fs/pack.h
    ../libsvn_fs_fs/rep-cache.h ../libsvn_fs_fs/rev_file.h
    ../libsvn_fs_fs/util.h
  )
  target_include_directories(libsvn_fs_fs PRIVATE
    ${SVN_SOURCE_DIR}/libsvn_fs_fs
    ${SVN_BINARY_DIR}/libsvn_fs_fs
  )
  target_link_libraries(libsvn_fs_fs PRIVATE
    libsvn_subr
    libsvn_delta
    libsvn_fs_util
    apr-1
    aprutil-1
    sqlite3
    ZLIB::ZLIB
  )
endif()

if(NOT TARGET libsvn_fs_x)
  file(GLOB SVN_FS_X_SOURCES CONFIGURE_DEPENDS
    ${SVN_SOURCE_DIR}/libsvn_fs_x/*.c
  )
  add_library(libsvn_fs_x ${SVN_FS_BUILD_TYPE}
    ${SVN_FS_X_SOURCES}
    ${SVN_BINARY_DIR}/libsvn_fs_x/rep-cache-db.h
  )
  svn_add_common_includes(libsvn_fs_x)
  svn_library_exports(libsvn_fs_x
    ../libsvn_fs_x/fs_init.h ../libsvn_fs_x/batch_fsync.h
    ../libsvn_fs_x/fs_x.h ../libsvn_fs_x/fs.h
    ../libsvn_fs_x/hotcopy.h ../libsvn_fs_x/reps.h ../libsvn_fs_x/string_table.h
  )
  target_include_directories(libsvn_fs_x PRIVATE
    ${SVN_SOURCE_DIR}/libsvn_fs_x
    ${SVN_BINARY_DIR}/libsvn_fs_x
  )
  target_link_libraries(libsvn_fs_x PRIVATE
    libsvn_subr
    libsvn_delta
    libsvn_fs_util
    apr-1
    aprutil-1
    sqlite3
    ZLIB::ZLIB
  )
endif()

if(NOT TARGET libsvn_fs)
  file(GLOB SVN_FS_SOURCES CONFIGURE_DEPENDS
    ${SVN_SOURCE_DIR}/libsvn_fs/*.c
  )
  add_library(libsvn_fs ${SVN_FS_SOURCES})
  svn_add_common_includes(libsvn_fs)
  svn_library_exports(libsvn_fs svn_fs.h private/svn_fs_private.h)
  target_include_directories(libsvn_fs PRIVATE
    ${SVN_SOURCE_DIR}/libsvn_fs
  )
  target_link_libraries(libsvn_fs PRIVATE
    libsvn_subr
    libsvn_delta
    libsvn_fs_util
    libsvn_fs_fs
    libsvn_fs_x
    apr-1
    aprutil-1
  )
endif()

if(NOT TARGET libsvn_ra_local)
  file(GLOB SVN_RA_LOCAL_SOURCES CONFIGURE_DEPENDS
    ${SVN_SOURCE_DIR}/libsvn_ra_local/*.c
  )
  add_library(libsvn_ra_local ${SVN_RA_BUILD_TYPE} ${SVN_RA_LOCAL_SOURCES})
  svn_add_common_includes(libsvn_ra_local)
  svn_library_exports(libsvn_ra_local
    ../libsvn_ra_local/ra_init.h ../libsvn_ra_local/ra_local.h
  )
  target_include_directories(libsvn_ra_local PRIVATE
    ${SVN_SOURCE_DIR}/libsvn_ra_local
  )
  target_link_libraries(libsvn_ra_local PRIVATE
    libsvn_repos
    libsvn_fs
    libsvn_delta
    libsvn_subr
    apr-1
    aprutil-1
  )
endif()

if(NOT TARGET libsvn_ra_svn)
  file(GLOB SVN_RA_SVN_SOURCES CONFIGURE_DEPENDS
    ${SVN_SOURCE_DIR}/libsvn_ra_svn/*.c
  )
  add_library(libsvn_ra_svn ${SVN_RA_BUILD_TYPE} ${SVN_RA_SVN_SOURCES})
  svn_add_common_includes(libsvn_ra_svn)
  svn_library_exports(libsvn_ra_svn
    ../libsvn_ra_svn/ra_init.h svn_ra_svn.h private/svn_ra_svn_private.h
  )
  target_include_directories(libsvn_ra_svn PRIVATE
    ${SVN_SOURCE_DIR}/libsvn_ra_svn
    ${CYRUS_SASL_SOURCE_DIR}/include
  )
  target_link_libraries(libsvn_ra_svn PRIVATE
    libsvn_delta
    libsvn_subr
    apr-1
    aprutil-1
    cyrus-sasl
  )
endif()

if(NOT TARGET libsvn_ra_serf)
  file(GLOB SVN_RA_SERF_SOURCES CONFIGURE_DEPENDS
    ${SVN_SOURCE_DIR}/libsvn_ra_serf/*.c
  )
  add_library(libsvn_ra_serf ${SVN_RA_BUILD_TYPE} ${SVN_RA_SERF_SOURCES})
  svn_add_common_includes(libsvn_ra_serf)
  svn_library_exports(libsvn_ra_serf
    ../libsvn_ra_serf/ra_init.h ../libsvn_ra_serf/ra_serf.h
  )
  target_include_directories(libsvn_ra_serf PRIVATE
    ${SVN_SOURCE_DIR}/libsvn_ra_serf
    ${SERF_SOURCE_DIR}
    ${SERF_SOURCE_DIR}/auth
    ${SERF_SOURCE_DIR}/buckets
    ${CMAKE_CURRENT_BINARY_DIR}/../apr-util
  )
  target_include_directories(libsvn_ra_serf SYSTEM PRIVATE
    ${SERF_SOURCE_DIR}
  )
  target_link_libraries(libsvn_ra_serf PRIVATE
    libsvn_delta
    libsvn_subr
    apr-1
    aprutil-1
    serf
    ZLIB::ZLIB
  )
endif()

if(NOT TARGET libsvn_ra)
  file(GLOB SVN_RA_SOURCES CONFIGURE_DEPENDS
    ${SVN_SOURCE_DIR}/libsvn_ra/*.c
  )
  add_library(libsvn_ra ${SVN_RA_SOURCES})
  svn_add_common_includes(libsvn_ra)
  svn_library_exports(libsvn_ra svn_ra.h private/svn_ra_private.h)
  target_include_directories(libsvn_ra PRIVATE
    ${SVN_SOURCE_DIR}/libsvn_ra
  )
  target_link_libraries(libsvn_ra PRIVATE
    libsvn_delta
    libsvn_subr
    libsvn_ra_local
    libsvn_ra_serf
    libsvn_ra_svn
    apr-1
    aprutil-1
  )
endif()

if(NOT TARGET libsvn_repos)
  file(GLOB SVN_REPOS_SOURCES CONFIGURE_DEPENDS
    ${SVN_SOURCE_DIR}/libsvn_repos/*.c
  )
  add_library(libsvn_repos ${SVN_REPOS_SOURCES})
  svn_add_common_includes(libsvn_repos)
  svn_library_exports(libsvn_repos
    svn_repos.h private/svn_repos_private.h ../libsvn_repos/authz.h
  )
  target_include_directories(libsvn_repos PRIVATE
    ${SVN_SOURCE_DIR}/libsvn_repos
  )
  target_link_libraries(libsvn_repos PRIVATE
    libsvn_fs
    libsvn_delta
    libsvn_subr
    apr-1
    aprutil-1
  )
endif()

if(NOT TARGET libsvn_wc)
  file(GLOB SVN_WC_SOURCES CONFIGURE_DEPENDS
    ${SVN_SOURCE_DIR}/libsvn_wc/*.c
  )
  add_library(libsvn_wc
    ${SVN_WC_SOURCES}
    ${SVN_BINARY_DIR}/libsvn_wc/wc-queries.h
    ${SVN_BINARY_DIR}/libsvn_wc/wc-metadata.h
    ${SVN_BINARY_DIR}/libsvn_wc/wc-checks.h
  )
  svn_add_common_includes(libsvn_wc)
  # wc-queries.h of the msvc-export list is generated and has no functions.
  svn_library_exports(libsvn_wc
    svn_wc.h private/svn_wc_private.h ../libsvn_wc/conflicts.h
    ../libsvn_wc/lock.h ../libsvn_wc/token-map.h
    ../libsvn_wc/tree_conflicts.h ../libsvn_wc/wc_db_private.h
    ../libsvn_wc/wc_db.h ../libsvn_wc/wc.h ../libsvn_wc/workqueue.h
  )
  target_include_directories(libsvn_wc PRIVATE
    ${SVN_SOURCE_DIR}/libsvn_wc
    ${SVN_BINARY_DIR}/libsvn_wc
  )
  target_link_libraries(libsvn_wc PRIVATE
    libsvn_subr
    libsvn_delta
    libsvn_diff
    apr-1
    aprutil-1
    sqlite3
  )
endif()

if(NOT TARGET libsvn_client)
  file(GLOB SVN_CLIENT_SOURCES CONFIGURE_DEPENDS
    ${SVN_SOURCE_DIR}/libsvn_client/*.c
  )
  add_library(libsvn_client ${SVN_CLIENT_SOURCES})
  svn_add_common_includes(libsvn_client)
  svn_library_exports(libsvn_client
    svn_client.h private/svn_client_mtcc.h private/svn_client_private.h
    private/svn_client_shelf.h private/svn_client_shelf2.h
    ../libsvn_client/mergeinfo.h ../libsvn_client/client.h
  )
  target_include_directories(libsvn_client PRIVATE
    ${SVN_SOURCE_DIR}/libsvn_client
  )
  target_link_libraries(libsvn_client PRIVATE
    libsvn_wc
    libsvn_ra
    libsvn_delta
    libsvn_diff
    libsvn_subr
    apr-1
    aprutil-1
  )
endif()

if(NOT TARGET libsvnxx)
  file(GLOB SVNXX_SOURCES CONFIGURE_DEPENDS
    ${SVN_SOURCE_DIR}/bindings/cxx/src/*.cpp
    ${SVN_SOURCE_DIR}/bindings/cxx/src/aprwrap/*.cpp
  )
  add_library(libsvnxx ${SVNXX_SOURCES})
  # The C++ bindings have no export lists, they export everything.
  set_target_properties(libsvnxx PROPERTIES
    OUTPUT_NAME svnxx-1
    WINDOWS_EXPORT_ALL_SYMBOLS ON
  )
  target_include_directories(libsvnxx PUBLIC
    ${SVN_SOURCE_DIR}/bindings/cxx/include
  )
  target_include_directories(libsvnxx PRIVATE
    ${SVN_SOURCE_DIR}/bindings/cxx/src
    ${SVN_SOURCE_DIR}/bindings/cxx/src/private
    ${SVN_SOURCE_DIR}/bindings/cxx/src/aprwrap
    ${SVN_SOURCE_DIR}/include
    ${SVN_SOURCE_DIR}/include/private
    ${APR_UTIL_SOURCE_DIR}/include
    ${APR_UTIL_BINARY_DIR}
    ${SVN_SOURCE_DIR}/libsvn_subr
    ${SVN_SOURCE_DIR}/libsvn_client
    ${SVN_SOURCE_DIR}/libsvn_wc
    ${SVN_SOURCE_DIR}/libsvn_ra
    ${SVN_SOURCE_DIR}/libsvn_delta
    ${SVN_SOURCE_DIR}/libsvn_diff
    ${SVN_SOURCE_DIR}/libsvn_fs
    ${SVN_SOURCE_DIR}/libsvn_fs_util
    ${SVN_SOURCE_DIR}/libsvn_fs_fs
    ${SVN_SOURCE_DIR}/libsvn_fs_x
    ${SVN_SOURCE_DIR}/libsvn_repos
    ${SVN_BINARY_DIR}/libsvn_subr
  )
  target_link_libraries(libsvnxx PRIVATE
    libsvn_subr
    libsvn_client
    libsvn_wc
    libsvn_ra
    libsvn_delta
    libsvn_diff
    libsvn_fs
    apr-1
    aprutil-1
  )
endif()

find_package(BerkeleyDB QUIET)
if(BerkeleyDB_FOUND AND NOT TARGET libsvn_fs_base)
  file(GLOB SVN_FS_BASE_SOURCES CONFIGURE_DEPENDS
    ${SVN_SOURCE_DIR}/libsvn_fs_base/*.c
    ${SVN_SOURCE_DIR}/libsvn_fs_base/bdb/*.c
    ${SVN_SOURCE_DIR}/libsvn_fs_base/util/*.c
  )
  add_library(libsvn_fs_base STATIC ${SVN_FS_BASE_SOURCES})
  svn_add_common_includes(libsvn_fs_base)
  target_include_directories(libsvn_fs_base PRIVATE
    ${SVN_SOURCE_DIR}/libsvn_fs_base
    ${SVN_SOURCE_DIR}/libsvn_fs_base/bdb
    ${SVN_SOURCE_DIR}/libsvn_fs_base/util
    ${BerkeleyDB_INCLUDE_DIRS}
  )
  target_link_libraries(libsvn_fs_base PRIVATE
    libsvn_delta
    libsvn_subr
    libsvn_fs_util
    apr-1
    aprutil-1
    ${BerkeleyDB_LIBRARIES}
  )
endif()
