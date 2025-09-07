cmake_minimum_required(VERSION 3.24)

if(TARGET RecyclerView)
    return()
endif()

project(RecyclerView)

set(CMAKE_CXX_STANDARD 17)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(RecyclerView_SOURCE_FILES
    include/recyclerview.h
    include/recyclerviewadapter.h
    include/recyclerviewpool.h
    include/styledrecyclerlistitem.h
    include/viewholder.h
    src/recyclerview.cpp
    src/recyclerviewadapter.cpp
    src/recyclerviewpool.cpp
    src/styledrecyclerlistitem.cpp
    src/viewholder.cpp
)

add_library(RecyclerView STATIC ${RecyclerView_SOURCE_FILES})

target_link_libraries(RecyclerView PRIVATE Qt6::Widgets)

target_include_directories(RecyclerView PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
