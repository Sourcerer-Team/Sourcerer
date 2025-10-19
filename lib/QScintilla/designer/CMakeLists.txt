cmake_minimum_required(VERSION 3.16)
project(qscintillaplugin VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 REQUIRED COMPONENTS Designer Gui)

qt_standard_project_setup()

qt_add_plugin(qscintillaplugin
    qscintillaplugin.cpp qscintillaplugin.h
)
target_link_libraries(qscintillaplugin PRIVATE
    Qt::Core
    Qt::Designer
    Qt::Gui
)

install(TARGETS qscintillaplugin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FRAMEWORK DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
