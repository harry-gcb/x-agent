set(HELPERS_SRC_LIST
${PROJECT_SOURCE_DIR}/lib/trace_helpers.c
${PROJECT_SOURCE_DIR}/lib/uprobe_helpers.c
)
add_library(helpers)
target_sources(helpers PRIVATE ${HELPERS_SRC_LIST})
target_include_directories(helpers PUBLIC ${PROJECT_SOURCE_DIR}/lib)
target_include_directories(helpers PUBLIC ${LIBBPF_INCLUDE_DIRS})
target_link_libraries(helpers PUBLIC ${LIBBPF_LIBRARIES} elf z)