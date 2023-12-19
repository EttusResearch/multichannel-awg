/*
 * Copyright 2023 Ettus Research, A National Instruments Brand
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#pragma once

#include "multichannel_awg.hpp"
#include "sequence.hpp"
#include <uhd/rfnoc_graph.hpp>
#include <uhd/rfnoc/block_id.hpp>
#include <uhd/rfnoc/duc_block_control.hpp>
#include <uhd/rfnoc/mb_controller.hpp>
#include <uhd/rfnoc/radio_control.hpp>
#include <uhd/rfnoc/replay_block_control.hpp>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include <unordered_map>

class rfnoc_awg : virtual public awg_base
{
public:
    static constexpr size_t MAX_NUM_SEQ_POINTS = 32;
    static constexpr double START_TIME_OFFSET = 1.0;

    rfnoc_awg(const std::string& address, const std::atomic<bool>& stop);
    bool load_program(std::unique_ptr<sequencer_data> seq) override;
    bool initialize() override;
    bool start() override;

    virtual ~rfnoc_awg();

private:
    struct replay_graph_config
    {
        size_t replay_port;
        size_t duc_port;
        size_t radio_port;
        std::shared_ptr<uhd::tx_streamer> tx_stream;
        std::shared_ptr<uhd::rfnoc::replay_block_control> replay_ctrl;
        std::shared_ptr<uhd::rfnoc::duc_block_control> duc_ctrl;
        std::shared_ptr<uhd::rfnoc::radio_control> radio_ctrl;
    };

    void create_graph();
    void validate();
    void connect_graph();
    void config_rfnoc_blocks();
    void setup_clocking();
    void sync_dance();
    void transmit_sequences();

    double sampling_rate;
    std::unique_ptr<sequencer_data> seq_data;
    std::shared_ptr<uhd::rfnoc::rfnoc_graph> graph;
    std::unordered_map<size_t, replay_graph_config> replay_graphs;

    std::vector<char> buffer;

};
