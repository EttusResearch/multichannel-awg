/*
 * Copyright 2023 Ettus Research, A National Instruments Brand
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include "nlohmann/detail/macro_scope.hpp"
#include "multichannel_awg/sequence.hpp"
#include <uhd/exception.hpp>
#include <nlohmann/json.hpp>
#include <stdexcept>

void from_json(const nlohmann::json& j, sequence_point& sp)
{
    j.at("channel").get_to(sp.channel);
    j.at("start_time").get_to(sp.start_time);
    sp.repetitions = j.value("repetitions", 0);
    j.at("segment").get_to(sp.segment);
}

NLOHMANN_JSON_SERIALIZE_ENUM(clock_source_e,
    {{clock_source_e::INTERNAL, "internal"}, {clock_source_e::EXTERNAL, "external"}});

NLOHMANN_JSON_SERIALIZE_ENUM(dataformat_e,
    {
        {dataformat_e::SC_16, "sc16"},
        {dataformat_e::FC_32, "fc32"},
        {dataformat_e::WIRE_DEFAULT, nullptr},
        {dataformat_e::CPU_DEFAULT, nullptr},
    });


void from_json(const nlohmann::json& j, device_settings& ds)
{
    j.at("sampling_rate").get_to(ds.sampling_rate);
    j.at("clock_source").get_to(ds.clock_source);
    j.at("gain").get_to(ds.gains);

    auto list = j.at("frequency");
    for (const auto& item : list) {
        double lo_offset = 0.0f;
        double rf_freq   = 0.0f;
        if (item.is_array()) {
            if (item.size() >= 0) {
                rf_freq = item.at(0).get<double>();
                if (item.size() > 1) {
                    lo_offset = item.at(1).get<double>();
                }
            } else {
                throw std::invalid_argument("encountered empty frequency array");
            }
        } else if (item.is_number()) {
            rf_freq = item.get<double>();
        }
        ds.frequencies.emplace_back(rf_freq, lo_offset);
    }

    ds.cpu_format  = j.value<dataformat_e>("data_fmt", dataformat_e::CPU_DEFAULT);
    ds.wire_format = j.value<dataformat_e>("wire_fmt", dataformat_e::WIRE_DEFAULT);
}
