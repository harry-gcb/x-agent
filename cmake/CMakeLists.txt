if (BUILD_LIBBPF)
    include(libbpf.cmake)
elseif(NOT EXISTS "${LIBBPF_ROOT}")
    message(FATAL_ERROR "This project depends on libbpf, please build 3rdparty/libbpf first:
    cmake -DBUILD_LIBBPF=on")
endif()

if (BUILD_BPFTOOL)
    include(bpftool.cmake)
elseif (NOT EXISTS "${BPFTOOL_ROOT}")
    message(FATAL_ERROR "This project depends on bpfbool, please build 3rdparty/bpfbool first:
    cmake -DBUILD_BPFTOOL=on ...")
endif()




# include(s-agent.cmake)