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
};
struct sequence_point
{
    size_t channel;
    double start_time;
    int repetitions = 0;
    std::string segment;
};

void from_json(const nlohmann::json j, sequence_point& sp);

struct sequencer_data
{
    using filemap_t = std::unordered_map<std::string, segment_spec>;
    std::unordered_map<size_t, std::vector<sequence_point>> used_channels;
    sequencer_data(const nlohmann::json& data);
    nlohmann::json def;
    filemap_t filemap;
    size_t itemsize;
    double sampling_rate;
    std::string cpu_format;
    std::string wire_format;
};
