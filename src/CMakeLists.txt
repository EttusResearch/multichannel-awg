# Copyright 2023 Ettus Research, A National Instruments Brand
#
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(multichannel_awg
    awg_base.cc
    host_awg.cc
    rfnoc_awg.cc
    json_helpers.cc
    main.cc 
    multichannel_awg.cc 
    sequencer.cc
    )

target_include_directories(multichannel_awg PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(multichannel_awg PUBLIC UHD::UHD)
target_link_libraries(multichannel_awg PRIVATE fmt::fmt)
target_link_libraries(multichannel_awg PRIVATE nlohmann_json::nlohmann_json)
# We're not using the CLI11 submodule – its CMake build is too noisy for customer-facing software
#target_link_libraries(multichannel_awg PRIVATE CLI11:CLI11)

# Enable build warnings – we're writing *good* software, not acceptable software
if(MSVC)
    target_compile_options(multichannel_awg PRIVATE /W4 /WX)
else()
    target_compile_options(multichannel_awg PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS multichannel_awg)
