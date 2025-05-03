
include(ExternalProject)
ExternalProject_Add(libbpf
    PREFIX libbpf
    # SOURCE_DIR ${LIBBPF_ROOT}/src
    GIT_REPOSITORY https://github.com/libbpf/libbpf.git
    GIT_TAG v1.2.2
    CONFIGURE_COMMAND ""
    BUILD_COMMAND make -C src
        BUILD_STATIC_ONLY=1
        OBJDIR=${CMAKE_CURRENT_BINARY_DIR}/libbpf/libbpf
        DESTDIR=${CMAKE_CURRENT_BINARY_DIR}/libbpf
        INCLUDEDIR=
        LIBDIR=
        UAPIDIR=
        install install_uapi_headers
    BUILD_IN_SOURCE TRUE
    INSTALL_COMMAND ""
    STEP_TARGETS build
)
set(LIBBPF_INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR}/libbpf)
set(LIBBPF_LIBRARIES ${CMAKE_CURRENT_BINARY_DIR}/libbpf/libbpf.a)
