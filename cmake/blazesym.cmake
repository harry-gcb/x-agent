include(ExternalProject)
ExternalProject_Add(blazesym
    PREFIX blazesym
    GIT_REPOSITORY https://github.com/libbpf/blazesym.git
    GIT_TAG v0.2.0-rc.3
    CONFIGURE_COMMAND ""
    BUILD_COMMAND cargo build --package=blazesym-c --release
    BUILD_IN_SOURCE TRUE
    INSTALL_COMMAND ""
    STEP_TARGETS build
)
# set(LIBBPF_INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR}/libbpf)
# set(LIBBPF_LIBRARIES ${CMAKE_CURRENT_BINARY_DIR}/libbpf/libbpf.a)
