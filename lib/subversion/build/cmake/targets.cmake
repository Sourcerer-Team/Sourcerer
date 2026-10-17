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
  add_library(libsvn_subr STATIC
    ${SVN_SUBR_SOURCES}
    ${SVN_BINARY_DIR}/libsvn_subr/internal_statements.h
  )
  target_compile_definitions(libsvn_subr PRIVATE
    SVN_INTERNAL_LZ4
    SVN_INTERNAL_UTF8PROC
    SVN_CHECKSUM_BACKEND_APR
  )
  svn_add_common_includes(libsvn_subr)
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
    utf8proc
  )
endif()

if(NOT TARGET libsvn_diff)
  file(GLOB SVN_DIFF_SOURCES CONFIGURE_DEPENDS
    ${SVN_SOURCE_DIR}/libsvn_diff/*.c
  )
  add_library(libsvn_diff STATIC ${SVN_DIFF_SOURCES})
  svn_add_common_includes(libsvn_diff)
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
  add_library(libsvn_delta STATIC ${SVN_DELTA_SOURCES})
  svn_add_common_includes(libsvn_delta)
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
  add_library(libsvn_fs_util STATIC ${SVN_FS_UTIL_SOURCES})
  svn_add_common_includes(libsvn_fs_util)
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
  add_library(libsvn_fs_fs STATIC
    ${SVN_FS_FS_SOURCES}
    ${SVN_BINARY_DIR}/libsvn_fs_fs/rep-cache-db.h
  )
  svn_add_common_includes(libsvn_fs_fs)
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
  add_library(libsvn_fs_x STATIC
    ${SVN_FS_X_SOURCES}
    ${SVN_BINARY_DIR}/libsvn_fs_x/rep-cache-db.h
  )
  svn_add_common_includes(libsvn_fs_x)
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
  add_library(libsvn_fs STATIC ${SVN_FS_SOURCES})
  svn_add_common_includes(libsvn_fs)
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
  add_library(libsvn_ra_local STATIC ${SVN_RA_LOCAL_SOURCES})
  svn_add_common_includes(libsvn_ra_local)
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
  add_library(libsvn_ra_svn STATIC ${SVN_RA_SVN_SOURCES})
  svn_add_common_includes(libsvn_ra_svn)
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
  add_library(libsvn_ra_serf STATIC ${SVN_RA_SERF_SOURCES})
  svn_add_common_includes(libsvn_ra_serf)
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
  target_compile_definitions(libsvn_ra_serf PRIVATE
    APR_DECLARE_STATIC
    APU_DECLARE_STATIC
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
  add_library(libsvn_ra STATIC ${SVN_RA_SOURCES})
  svn_add_common_includes(libsvn_ra)
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
  add_library(libsvn_repos STATIC ${SVN_REPOS_SOURCES})
  svn_add_common_includes(libsvn_repos)
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
  add_library(libsvn_wc STATIC
    ${SVN_WC_SOURCES}
    ${SVN_BINARY_DIR}/libsvn_wc/wc-queries.h
    ${SVN_BINARY_DIR}/libsvn_wc/wc-metadata.h
    ${SVN_BINARY_DIR}/libsvn_wc/wc-checks.h
  )
  svn_add_common_includes(libsvn_wc)
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
  add_library(libsvn_client STATIC ${SVN_CLIENT_SOURCES})
  svn_add_common_includes(libsvn_client)
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
  add_library(libsvnxx STATIC ${SVNXX_SOURCES})
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
