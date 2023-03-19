#The MIT License (MIT)

#Copyright © 2022-2023 Antonio Dias (https://github.com/antonypro)

#Permission is hereby granted, free of charge, to any person obtaining a copy
#of this software and associated documentation files (the "Software"), to deal
#in the Software without restriction, including without limitation the rights
#to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#copies of the Software, and to permit persons to whom the Software is
#furnished to do so, subject to the following conditions:

#The above copyright notice and this permission notice shall be included in all
#copies or substantial portions of the Software.

#THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#SOFTWARE.

#project(QGoodCentralWidget)

#if(TARGET ${PROJECT_NAME})
#    return()
#endif()

#add_library(${PROJECT_NAME} SHARED "")

#include("../../cmake/helpers.cmake")

#includeLib(${PROJECT_NAME} QGoodWindow)

target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/qgoodcentralwidget.cpp ${CMAKE_CURRENT_LIST_DIR}/src/qgoodcentralwidget.h
)

set(qgoodwindow TRUE)

if(qgoodwindow)
    target_sources(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src/captionbutton.cpp  ${CMAKE_CURRENT_LIST_DIR}/src/captionbutton.h
        ${CMAKE_CURRENT_LIST_DIR}/src/iconwidget.cpp ${CMAKE_CURRENT_LIST_DIR}/src/iconwidget.h
        ${CMAKE_CURRENT_LIST_DIR}/src/titlebar.cpp ${CMAKE_CURRENT_LIST_DIR}/src/titlebar.h
        ${CMAKE_CURRENT_LIST_DIR}/src/titlewidget.cpp ${CMAKE_CURRENT_LIST_DIR}/src/titlewidget.h

        ${CMAKE_CURRENT_LIST_DIR}/src/qgoodcentralwidget_icons.qrc
    )

    find_package(Qt${QT_VERSION_MAJOR} REQUIRED
        Core
        Svg
        Widgets
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Svg
        Qt${QT_VERSION_MAJOR}::Widgets
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR})

target_compile_definitions(${PROJECT_NAME} PRIVATE QGOODCENTRALWIDGET)
