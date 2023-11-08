
set(BPFTOOL_ROOT "${PROJECT_SOURCE_DIR}/3rdparty/bpftool")

if (NOT EXISTS "${BPFTOOL_ROOT}")
    message(FATAL_ERROR "This project depends on bpfbool, please use the following command to download 3rdparty/bpfbool first:
    git submodule update --init --recusive")
endif()

ExternalProject_Add(bpftool
    PREFIX bpftool
    SOURCE_DIR ${BPFTOOL_ROOT}/src
    CONFIGURE_COMMAND ""
    BUILD_COMMAND make bootstrap
        OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bpftool/
    BUILD_IN_SOURCE TRUE
    INSTALL_COMMAND ""
    STEP_TARGETS build
)

set(BPFOBJECT_BPFTOOL_EXE ${CMAKE_CURRENT_BINARY_DIR}/bpftool/bootstrap/bpftool)