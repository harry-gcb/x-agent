set(LIBBPF_INCLUDE_DIRS ${LIBBPF_ROOT}/include)
set(LIBBPF_LIBRARIES ${LIBBPF_ROOT}/lib/libbpf.a)
set(BPFOBJECT_BPFTOOL_EXE ${BPFTOOL_ROOT}/bpftool)
set(S_AGENT_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/include)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/tools)

if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
  set(ARCH "x86")
elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
  set(ARCH "arm")
elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
  set(ARCH "arm64")
elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "ppc64le")
  set(ARCH "powerpc")
elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "mips")
  set(ARCH "mips")
elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "riscv64")
  set(ARCH "riscv")
elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "loongarch64")
  set(ARCH "loongarch")
endif()

find_package(BpfObject REQUIRED)

# Create an executable for each application
file(GLOB apps ${PROJECT_SOURCE_DIR}/src/*.bpf.c)
if(NOT CARGO_EXISTS)
  list(REMOVE_ITEM apps ${CMAKE_CURRENT_SOURCE_DIR}/profile.bpf.c)
endif()
foreach(app ${apps})
  get_filename_component(app_stem ${app} NAME_WE)

  # Build object skeleton and depend skeleton on libbpf build
  bpf_object(${app_stem} ${app_stem}.bpf.c)
  # add_dependencies(${app_stem}_skel libbpf-build bpftool-build)

  add_executable(${app_stem} ${PROJECT_SOURCE_DIR}/src/${app_stem}.c)
  target_include_directories(${app_stem} PRIVATE ${S_AGENT_INCLUDE_DIRS})
  target_link_libraries(${app_stem} ${app_stem}_skel)
#   if(${app_stem} STREQUAL profile)
#     target_include_directories(${app_stem} PRIVATE
#       ${CMAKE_CURRENT_SOURCE_DIR}/../../blazesym/include)
#     target_link_libraries(${app_stem}
#       ${CMAKE_CURRENT_SOURCE_DIR}/../../blazesym/target/release/libblazesym.a -lpthread -lrt -ldl)
#   endif()
endforeach()