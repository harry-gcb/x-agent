if (NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/bpftool-v7.5.0-amd64.tar.gz)
    file(DOWNLOAD  "https://github.com/libbpf/bpftool/releases/download/v7.5.0/bpftool-v7.5.0-amd64.tar.gz" ${CMAKE_CURRENT_BINARY_DIR}/bpftool-v7.5.0-amd64.tar.gz SHOW_PROGRESS)
endif()

if (NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/bpftool)
    file(ARCHIVE_EXTRACT INPUT ${CMAKE_CURRENT_BINARY_DIR}/bpftool-v7.5.0-amd64.tar.gz DESTINATION  ${CMAKE_CURRENT_BINARY_DIR})
    file(CHMOD ${CMAKE_CURRENT_BINARY_DIR}/bpftool FILE_PERMISSIONS OWNER_EXECUTE)
endif()

set(BPFOBJECT_BPFTOOL_EXE ${CMAKE_CURRENT_BINARY_DIR}/bpftool)