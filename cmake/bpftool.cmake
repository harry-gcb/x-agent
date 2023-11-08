


ExternalProject_Add(bpftool
    PREFIX bpftool
    GIT_REPOSITORY https://github.com/libbpf/bpftool.git
    GIT_TAG v7.2.0
    # SOURCE_DIR ${BPFTOOL_ROOT}/src
    CONFIGURE_COMMAND ""
    BUILD_COMMAND make bootstrap -C src
        OUTPUT=${PROJECT_SOURCE_DIR}/3rdparty/
    BUILD_IN_SOURCE TRUE
    INSTALL_COMMAND ""
    STEP_TARGETS build
)

set(BPFOBJECT_BPFTOOL_EXE ${PROJECT_SOURCE_DIR}/3rdparty/bootstrap/bpftool)