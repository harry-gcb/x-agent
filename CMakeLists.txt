cmake_minimum_required(VERSION 3.20)

project(s-agent)

option(BUILD_LIBBPF "Build libbpf" OFF)
option(BUILD_BPFTOOL "Build bpftool" OFF)

add_subdirectory(cmake)