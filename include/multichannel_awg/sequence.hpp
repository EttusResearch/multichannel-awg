/*
 * Copyright 2023 Ettus Research, A National Instruments Brand
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#pragma once

#include <uhd/types/stream_cmd.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <tuple>
#include <unordered_map>

struct timed_stream_cmd
{
    size_t channel;
    size_t start_time_samples;
    uhd::stream_cmd_t command;
};

struct segment_spec
{
    std::string name;
    std::string filename;
    size_t length;
    size_t start_idx;
    char* data;
};

struct sequence_point
{
    size_t channel;
    double start_time;
    int repetitions = 0;
    std::string segment;
};

enum class clock_source_e { INTERNAL, EXTERNAL };
enum class dataformat_e {
    SC_16        = 2 * 2,
    FC_32        = 2 * 4,
    WIRE_DEFAULT = SC_16,
    CPU_DEFAULT  = FC_32
};

struct device_settings
{
    // Types for clarity purposes
    using channel   = size_t;
    using gain      = float;
    using rf_freq   = double;
    using lo_offset = double;
    using freq_spec = std::tuple<rf_freq, lo_offset>;

    // data fields
    double sampling_rate;
    clock_source_e clock_source;
    std::vector<gain> gains;
    std::vector<freq_spec> frequencies;
    dataformat_e cpu_format;
    dataformat_e wire_format;
    size_t itemsize;
};

void from_json(const nlohmann::json& j, sequence_point& sp);
void from_json(const nlohmann::json& j, device_settings& ds);

struct sequencer_data
{
    using filemap_t = std::unordered_map<std::string, segment_spec>;
    std::unordered_map<size_t, std::vector<sequence_point>> used_channels;
    sequencer_data(const nlohmann::json& data);
    nlohmann::json def;
    device_settings settings;
    filemap_t filemap;
};
