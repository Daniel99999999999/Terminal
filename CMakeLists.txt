cmake_minimum_required(VERSION 3.22)

# Explicit version also supports a newly initialized Git repository with no HEAD commit.
set(PROJECT_VER "0.1.11")

# Only application dependencies: HC-05 UART needs no native radio components.
# The v0.1.10 CMake/config remain preserved under baseline for rollback.
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(Terminal)
