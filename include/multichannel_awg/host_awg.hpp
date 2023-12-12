/*
 * Copyright 2023 Ettus Research, A National Instruments Brand
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#pragma once

#include "multichannel_awg.hpp"
#include "sequence.hpp"
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include <unordered_map>


// fwd decl
namespace uhd {
namespace usrp {
class multi_usrp;
} // namespace usrp
class tx_streamer;
} // namespace uhd


struct sequencer_state
{
public:
    using sp_container = std::vector<sequence_point>;
    sequencer_state(sp_container::iterator&& begin,
        sp_container::const_iterator&& end,
        sequencer_data* data,
        const std::shared_ptr<uhd::usrp::multi_usrp>& usrp)
        : begin(begin), end(end), usrp(usrp), data(data)
    {
    }
    sp_container::iterator begin;
    sp_container::const_iterator end;
    std::shared_ptr<uhd::tx_streamer> tx_streamer;
    std::shared_ptr<uhd::usrp::multi_usrp> usrp;
    void operator()();
    const sequencer_data* const data;
};

class host_awg : virtual public awg_base
{
public:
    host_awg(const std::string& address);
    bool load_program(std::unique_ptr<sequencer_data> seq) override;
    bool initialize() override;
    bool start() override;

    virtual ~host_awg();

private:
    void setup_clocking();
    void sync_dance();

    double sampling_rate;
    std::shared_ptr<uhd::usrp::multi_usrp> usrp;
    std::unique_ptr<sequencer_data> seq_data;

    std::vector<char> buffer;
    std::unordered_map<size_t, sequencer_state> sequence_workers;
};
