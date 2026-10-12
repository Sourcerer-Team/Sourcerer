cmake_minimum_required(VERSION 3.24)

if(TARGET Util)
  return()
endif()

project(Util)

set(CMAKE_CXX_STANDARD 17)
set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Network Widgets LinguistTools)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

if(WIN32)
  set(PLATFORM_SOURCES
      include/win32/DirectoryWatcher.h
      src/win32/DirectoryWatcher.cpp)
endif()

add_library(Util STATIC
   include/BlockingQueue.h
   include/CloudCredentialStorage.h
   include/CloudRepo.h
   include/CommitGraphBuilder.h
   include/Dispatcher.h
   include/FileManager.h
   include/Timer.h
   include/Avatar.h
   include/FileLock.h
   include/General.h
   include/Graph.h
   include/Git.h
   include/HttpClient.h
   include/JWTDecoder.h
   include/LockGuard.h
   include/ProcessRunner.h
   include/ResizableVector.h
   include/ResizableMatrix.h
   include/SingleInstanceGuard.h

   include/TokenString.h
   include/Uuid.h
   src/Avatar.cpp
   src/CloudCredentialStorage.cpp
   src/CloudRepo.cpp
   src/Dispatcher.cpp
   src/FileManager.cpp
   src/FileLock.cpp
   src/General.cpp
   src/Git.cpp
   src/HttpClient.cpp
   src/JWTDecoder.cpp
   src/LockGuard.cpp
   src/ProcessRunner.cpp
   src/SingleInstanceGuard.cpp

   src/TokenString.cpp
   src/Uuid.cpp
   ${PLATFORM_SOURCES}
)

include(../../cmake/helpers.cmake)

includeComponent(Util ConfigManager)

includeLib(Util Log)


target_link_libraries(Util PRIVATE
    Threads::Threads
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Widgets
    OpenSSL::SSL
    OpenSSL::Crypto
    qt6keychain
)

if(WIN32)
  target_link_libraries(Util PRIVATE shell32)
endif()

target_include_directories(Util PRIVATE
   ${CMAKE_CURRENT_SOURCE_DIR}/include
)
