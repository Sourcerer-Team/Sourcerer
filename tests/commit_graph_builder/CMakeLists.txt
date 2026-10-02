cmake_minimum_required(VERSION 3.24)
project(commit_graph_builder_test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Test)

enable_testing()

add_executable(commit_graph_builder_test tst_commit_graph_builder.cpp)
target_include_directories(commit_graph_builder_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/Util/include)
target_link_libraries(commit_graph_builder_test PRIVATE Qt6::Core Qt6::Test)

add_test(NAME commit_graph_builder COMMAND commit_graph_builder_test)
