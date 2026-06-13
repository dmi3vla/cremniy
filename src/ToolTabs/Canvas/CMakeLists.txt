target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/canvastab.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/canvastab.h
    ${CMAKE_CURRENT_SOURCE_DIR}/canvas_view.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/canvas_view.h
    ${CMAKE_CURRENT_SOURCE_DIR}/canvas_layout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/canvas_layout.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dependency_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dependency_parser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gource_animator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gource_animator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/layer_panel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/layer_panel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/minimap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/minimap.h
    ${CMAKE_CURRENT_SOURCE_DIR}/nodes/file_node.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/nodes/file_node.h
    ${CMAKE_CURRENT_SOURCE_DIR}/edges/dependency_edge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/edges/dependency_edge.h
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
