cmake_minimum_required(VERSION 3.24)

if(TARGET OpenSSLHelper)
    return()
endif()

project(OpenSSLHelper)
set(CMAKE_CXX_STANDARD 17)

find_package(OpenSSL REQUIRED)

set(OPENSSLHELPER_SOURCE_FILES)

# applink.c is only needed on Windows
if(WIN32)
    list(APPEND OPENSSLHELPER_SOURCE_FILES src/applink.c)
endif()

add_library(OpenSSLHelper STATIC ${OPENSSLHELPER_SOURCE_FILES})

if(MSVC)
    # applink.c is shipped by OpenSSL, silence MSVC CRT deprecation.
    target_compile_definitions(OpenSSLHelper PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

include(../../cmake/helpers.cmake)

target_link_libraries(OpenSSLHelper PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_include_directories(OpenSSLHelper PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)