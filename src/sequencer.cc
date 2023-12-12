/*
 * Copyright 2023 Ettus Research, A National Instruments Brand
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include "nlohmann/json_fwd.hpp"
#include "multichannel_awg/sequence.hpp"
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>

using json = nlohmann::json;

sequencer_data::sequencer_data(const json& data)
    : def(data), settings(data.at("config").get<device_settings>())
{
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
            std::filesystem::file_size(filespec.at("sample_file"))
                / static_cast<size_t>(settings.cpu_format),
            static_cast<size_t>(-1), /* Can't set start offset before loading */
            nullptr};
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
                next_start_earliest = sp.start_time
                                      + sp.repetitions * filemap.at(sp.segment).length
                                            / settings.sampling_rate;
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
