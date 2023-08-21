# Copyright 2023 Ettus Research, A National Instruments Brand
#
# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.16)
project(
    quadchannel_awg
    VERSION 0.0.1
    LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# Packaging support
set(CPACK_PACKAGE_VENDOR "Ettus Research")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Multichannel AWG based on UHD4")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

find_package(UHD 4.2.0 REQUIRED)
if(NOT TARGET UHD::UHD)
    add_library(UHD::UHD INTERFACE IMPORTED)
    set_target_properties(
        UHD::UHD PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${UHD_INCLUDE_DIRS}"
                            INTERFACE_LINK_LIBRARIES "${UHD_LIBRARIES}")
endif()
message(STATUS "Found UHD ${UHD_VERSION}")

include(FetchContent)
message(STATUS "Preparing {fmt}")
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt
    GIT_TAG 9.0.0)
FetchContent_MakeAvailable(fmt)
message(STATUS "Prepared {fmt}")

# We're not using the CLI11 submodule – its CMake build is too noisy for customer-facing software
# message(STATUS "Preparing CLI11")
# FetchContent_Declare(
#     clilib
#     QUIET
#     GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
#     GIT_TAG v1.9.0
# )
# FetchContent_MakeAvailable(clilib)
# message(STATUS "Prepared CLI11")
message(STATUS "Preparing nlohmann_json")
FetchContent_Declare(
    json URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz)
FetchContent_MakeAvailable(json)
message(STATUS "Prepared nlohmann_json")

#Here goes the actual work
add_subdirectory(src)

# Include cpack at the end
message(STATUS "Preparing CPack")
include(CPack)
message(STATUS "Prepared CPack")
