/*
 * Copyright 2023 Ettus Research, A National Instruments Brand
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include "nlohmann/json_fwd.hpp"
#include "quadchannel_awg/sequence.hpp"
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>

using json = nlohmann::json;

sequencer_data::sequencer_data(const json& data) : def(data)
{
    cpu_format = data.at("config").value("data_fmt", "");
    cpu_format = data.at("config").value("wire_fmt", "");
    if (cpu_format == "sc16") {
        itemsize = 2 * 2;
    } else if (cpu_format == "fc32") {
        itemsize = 2 * 4;
    } else {
        fmt::print(stderr,
            FMT_STRING("Unrecognized config->data_fmt key \"{}\", assuming fc32.\n"),
            cpu_format);
        itemsize = 2 * 4;
    }
    data.at("config").at("sampling_rate").get_to(sampling_rate);

    for (const auto& filespec : data.at("segments")) {
        fmt::print(FMT_STRING("segment \"{}\" from \"{}\"\n"),
            filespec.at("id"),
            filespec.at("sample_file"));
        if (!std::filesystem::exists(filespec.at("sample_file"))) {
            fmt::print(stderr, "file '{:s}' not found\n", filespec.at("sample_file"));
            throw std::runtime_error("File Not Found");
        }
        filemap[filespec.at("id")] = {filespec.at("id"),
            filespec.at("sample_file"),
            std::filesystem::file_size(filespec.at("sample_file")) / itemsize};
    }

    for (const auto& entry : data.at("sequence")) {
        auto sp = entry.get<sequence_point>();
        fmt::print(
            FMT_STRING(
                "Sequence point: Channel {}, start time {}, segment {}, repetitions{}\n"),
            sp.channel,
            sp.start_time,
            sp.segment,
            sp.repetitions);
        used_channels[sp.channel].push_back(sp);
    }

    // verify sequence does not overlap
    for (auto& [channel, sp_vec] : used_channels) {
        double next_start_earliest = 0;
        fmt::print(FMT_STRING("Channel {}:\n"), channel);
        for (auto& sp : sp_vec) {
            fmt::print(
                FMT_STRING("Encountered segment: {}, start time: {}, repetitions: {}\n"),
                sp.segment,
                sp.start_time,
                sp.repetitions);
            if (next_start_earliest > sp.start_time) {
                fmt::print(stderr,
                    FMT_STRING("Channel {}: start time {} is before the end of the "
                               "previous segment ({}); adjusting.\n"),
                    channel,
                    sp.start_time,
                    next_start_earliest);
                sp.start_time = next_start_earliest;
            }
            if (sp.repetitions < 0) {
                fmt::print(FMT_STRING("Channel {}: Looping segment {} forever, ignoring "
                                      "further segments, as impossible to reach\n"),
                    channel,
                    sp.segment);
                break;
            }
            try {
                next_start_earliest =
                    sp.start_time
                    + sp.repetitions * filemap.at(sp.segment).length / sampling_rate;
            } catch (const std::out_of_range& err) {
                fmt::print(stderr,
                    FMT_STRING("Channel {}, Segment {}, {}\n"),
                    channel,
                    sp.segment,
                    err.what());
            }
        }
    }
}
void from_json(const nlohmann::json j, sequence_point& sp)
{
    j.at("channel").get_to(sp.channel);
    j.at("start_time").get_to(sp.start_time);
    sp.repetitions = j.value("repetitions", 0);
    j.at("segment").get_to(sp.segment);
}
