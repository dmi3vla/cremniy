cmake_minimum_required(VERSION 3.16)
project(cremniy_tests LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8.3 REQUIRED COMPONENTS Test Widgets)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)

add_executable(test_dependency_parser
    test_dependency_parser.cpp
    ${SRC_DIR}/ToolTabs/Canvas/dependency_parser.cpp
    ${SRC_DIR}/ToolTabs/Canvas/dependency_parser.h
)

target_include_directories(test_dependency_parser PRIVATE
    ${SRC_DIR}
    ${SRC_DIR}/ToolTabs/Canvas
)

target_link_libraries(test_dependency_parser PRIVATE Qt6::Test Qt6::Widgets)

add_test(NAME DependencyParser COMMAND test_dependency_parser)
