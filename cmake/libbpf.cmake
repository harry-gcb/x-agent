set(LIBBPF_ROOT "${PROJECT_SOURCE_DIR}/3rdparty/libbpf")

if (NOT EXISTS "${LIBBPF_ROOT}")
    message(FATAL_ERROR "This project depends on libbpf, please use the following command to download 3rdparty/libbpf first:
    git submodule update --init --recusive")
endif()

include(ExternalProject)
ExternalProject_Add(libbpf
    PREFIX libbpf
    SOURCE_DIR ${LIBBPF_ROOT}/src
    CONFIGURE_COMMAND ""
    BUILD_COMMAND make
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