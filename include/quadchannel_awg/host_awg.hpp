/*
 * Copyright 2023 Ettus Research, A National Instruments Brand
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#pragma once

#include "quadchannel_awg.hpp"
#include "sequence.hpp"
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include <unordered_map>


// fwd decl
namespace uhd::usrp {
class multi_usrp;
}
class host_awg : virtual public awg_base
{
public:
    host_awg(const std::string& address);
    bool load_program(std::unique_ptr<sequencer_data> seq) override;
    bool initialize() override;
    virtual ~host_awg();

private:
    void setup_clocking();
    void sync_dance();

    double sampling_rate;
    std::shared_ptr<uhd::usrp::multi_usrp> usrp;
    std::unique_ptr<sequencer_data> seq_data;

    std::vector<char> buffer;
    std::unordered_map<std::string, std::tuple<size_t, size_t>> buffer_offsets;
    struct sequencer_state
    {
        using sp_container = std::vector<sequence_point>;
        sp_container::iterator begin;
        sp_container::const_iterator end;
    };
    std::unordered_map<size_t, sequencer_state> program_counters;
};
