cmake_minimum_required(VERSION 3.16)
project(cremniy_tests LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8.3 REQUIRED COMPONENTS Test Widgets Network)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)

# --- test_dependency_parser ---
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

# --- test_semantic_map ---
add_executable(test_semantic_map
    test_semantic_map.cpp
    ${SRC_DIR}/ToolTabs/Canvas/semantic_map.cpp
    ${SRC_DIR}/ToolTabs/Canvas/semantic_map.h
    ${SRC_DIR}/ToolTabs/Canvas/semantic_map_store.cpp
    ${SRC_DIR}/ToolTabs/Canvas/semantic_map_store.h
    ${SRC_DIR}/ToolTabs/Canvas/semantic_map_utils.cpp
    ${SRC_DIR}/ToolTabs/Canvas/semantic_map_utils.h
)

target_include_directories(test_semantic_map PRIVATE
    ${SRC_DIR}
    ${SRC_DIR}/ToolTabs/Canvas
)

target_link_libraries(test_semantic_map PRIVATE Qt6::Test Qt6::Widgets)

add_test(NAME SemanticMap COMMAND test_semantic_map)

# --- test_generate_semantic_map_tool ---
add_executable(test_generate_semantic_map_tool
    test_generate_semantic_map_tool.cpp
    ${SRC_DIR}/Agent/tools/generate_semantic_map_tool.cpp
    ${SRC_DIR}/Agent/tools/generate_semantic_map_tool.h
    ${SRC_DIR}/Agent/tools/agent_tool.h
    ${SRC_DIR}/ToolTabs/Canvas/semantic_map.cpp
    ${SRC_DIR}/ToolTabs/Canvas/semantic_map.h
    ${SRC_DIR}/ToolTabs/Canvas/semantic_map_store.cpp
    ${SRC_DIR}/ToolTabs/Canvas/semantic_map_store.h
    ${SRC_DIR}/ToolTabs/Canvas/semantic_map_utils.cpp
    ${SRC_DIR}/ToolTabs/Canvas/semantic_map_utils.h
    ${SRC_DIR}/Agent/llm_client.cpp
    ${SRC_DIR}/Agent/llm_client.h
    ${SRC_DIR}/Agent/endpoint_manager.cpp
    ${SRC_DIR}/Agent/endpoint_manager.h
    ${SRC_DIR}/utils/appsettings.cpp
    ${SRC_DIR}/utils/appsettings.h
)

target_include_directories(test_generate_semantic_map_tool PRIVATE
    ${SRC_DIR}
    ${SRC_DIR}/ToolTabs/Canvas
    ${SRC_DIR}/Agent
    ${SRC_DIR}/Agent/tools
)

target_compile_definitions(test_generate_semantic_map_tool PRIVATE CREMNIY_TESTING)

target_link_libraries(test_generate_semantic_map_tool PRIVATE
    Qt6::Test Qt6::Widgets Qt6::Network)

add_test(NAME GenerateSemanticMapTool COMMAND test_generate_semantic_map_tool)
