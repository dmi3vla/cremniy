cmake_minimum_required(VERSION 3.16)
project(cremniy_tests LANGUAGES CXX)
enable_testing()

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

# --- test_codemap ---
add_executable(test_codemap
    test_codemap.cpp
    ${SRC_DIR}/ToolTabs/Canvas/codemap.cpp
    ${SRC_DIR}/ToolTabs/Canvas/codemap.h
    ${SRC_DIR}/ToolTabs/Canvas/codemap_utils.cpp
    ${SRC_DIR}/ToolTabs/Canvas/codemap_utils.h
    ${SRC_DIR}/ToolTabs/Canvas/codemap_store.cpp
    ${SRC_DIR}/ToolTabs/Canvas/codemap_store.h
)

target_include_directories(test_codemap PRIVATE
    ${SRC_DIR}
    ${SRC_DIR}/ToolTabs/Canvas
)

target_link_libraries(test_codemap PRIVATE Qt6::Test Qt6::Widgets)

add_test(NAME Codemap COMMAND test_codemap)

# --- test_generate_codemap_tool ---
add_executable(test_generate_codemap_tool
    test_generate_codemap_tool.cpp
    ${SRC_DIR}/Agent/tools/generate_codemap_tool.cpp
    ${SRC_DIR}/Agent/tools/generate_codemap_tool.h
    ${SRC_DIR}/Agent/tools/agent_tool.h
    ${SRC_DIR}/ToolTabs/Canvas/codemap.cpp
    ${SRC_DIR}/ToolTabs/Canvas/codemap.h
    ${SRC_DIR}/ToolTabs/Canvas/codemap_utils.cpp
    ${SRC_DIR}/ToolTabs/Canvas/codemap_utils.h
    ${SRC_DIR}/ToolTabs/Canvas/codemap_store.cpp
    ${SRC_DIR}/ToolTabs/Canvas/codemap_store.h
    ${SRC_DIR}/Agent/llm_client.cpp
    ${SRC_DIR}/Agent/llm_client.h
    ${SRC_DIR}/Agent/endpoint_manager.cpp
    ${SRC_DIR}/Agent/endpoint_manager.h
    ${SRC_DIR}/utils/appsettings.cpp
    ${SRC_DIR}/utils/appsettings.h
)

target_include_directories(test_generate_codemap_tool PRIVATE
    ${SRC_DIR}
    ${SRC_DIR}/ToolTabs/Canvas
    ${SRC_DIR}/Agent
    ${SRC_DIR}/Agent/tools
)

target_compile_definitions(test_generate_codemap_tool PRIVATE CREMNIY_TESTING)

target_link_libraries(test_generate_codemap_tool PRIVATE
    Qt6::Test Qt6::Widgets Qt6::Network)

add_test(NAME GenerateCodemapTool COMMAND test_generate_codemap_tool)

# Source/binary index regression cases deliberately avoid compiler behaviour.
add_executable(test_source_binary
    test_source_binary.cpp
    ${SRC_DIR}/SourceBinary/source_binary_store.cpp
)
target_include_directories(test_source_binary PRIVATE ${SRC_DIR})
target_link_libraries(test_source_binary PRIVATE Qt6::Test)
add_test(NAME SourceBinary COMMAND test_source_binary)

add_executable(test_trace_explorer
    test_trace_explorer.cpp
    ${SRC_DIR}/ToolTabs/Canvas/trace_explorer.cpp
    ${SRC_DIR}/ToolTabs/Canvas/trace_model.cpp
    ${SRC_DIR}/ToolTabs/Canvas/codemap.cpp
    ${SRC_DIR}/ToolTabs/Canvas/codemap_utils.cpp
    ${SRC_DIR}/ToolTabs/Canvas/codemap_store.cpp
    ${SRC_DIR}/SourceBinary/source_binary_store.cpp
    ${SRC_DIR}/ToolTabs/Disassembler/disassemblerworker.cpp
    ${SRC_DIR}/ToolTabs/Disassembler/disasm/backends/radare2backend.cpp
    ${SRC_DIR}/utils/appsettings.cpp
)
target_include_directories(test_trace_explorer PRIVATE ${SRC_DIR} ${SRC_DIR}/ToolTabs/Canvas)
target_compile_definitions(test_trace_explorer PRIVATE
    TRACE_FIXTURE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/trace")
target_link_libraries(test_trace_explorer PRIVATE Qt6::Test Qt6::Widgets Qt6::Network)
add_test(NAME TraceExplorer COMMAND test_trace_explorer)
set_tests_properties(TraceExplorer PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME TraceFixture COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/trace/test_fixture.py)
