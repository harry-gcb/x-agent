set(LIB_3RDPARTY_PATH ${PROJECT_SOURCE_DIR}/3rdparty/)
set(LIBBPF_ROOT ${LIB_3RDPARTY_PATH}/libbpf/)
set(BPFTOOL_ROOT ${LIB_3RDPARTY_PATH}/bootstrap/)

if (NOT EXISTS "${LIBBPF_ROOT}")
    message(STATUS "Build libbpf ${LIBBPF_ROOT}")
    include(libbpf.cmake)
elseif (NOT EXISTS "${BPFTOOL_ROOT}")
    message(STATUS "Build bpftool ${BPFTOOL_ROOT}")
    include(bpftool.cmake)
else()
    message(STATUS "Build s-agent")
    include(s-agent.cmake)
endif()



# if (BUILD_LIBBPF)
#     include(libbpf.cmake)
# elseif(NOT EXISTS "${LIBBPF_ROOT}")
#     message(FATAL_ERROR "This project depends on libbpf, please build 3rdparty/libbpf first:
#     cmake -DBUILD_LIBBPF=ON ...")
# endif()

# if (BUILD_BPFTOOL)
#     include(bpftool.cmake)
# elseif (NOT EXISTS "${BPFTOOL_ROOT}")
#     message(FATAL_ERROR "This project depends on bpfbool, please build 3rdparty/bpfbool first:
#     cmake -DBUILD_BPFTOOL=ON ...")
# endif()

# if (EXISTS "${LIBBPF_ROOT}")
#     if (EXISTS "${BPFTOOL_ROOT}")
#         message(STATUS "Build s-agent")
#         include(s-agent.cmake)
#     endif()
# endif()