cmake_minimum_required(VERSION 3.24)
project(gitlab_flow_test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Test)

enable_testing()

add_executable(gitlab_flow_test tst_gitlab_flow.cpp)
target_include_directories(gitlab_flow_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/GitRepo/include)
target_link_libraries(gitlab_flow_test PRIVATE Qt6::Core Qt6::Test)

add_test(NAME gitlab_flow COMMAND gitlab_flow_test)
