/*
 * Copyright 2023 Ettus Research, A National Instruments Brand
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include "multichannel_awg/rfnoc_awg.hpp"
#include "fmt/core.h"
#include "multichannel_awg/multichannel_awg.hpp"
#include "multichannel_awg/sequence.hpp"
#include <uhd/exception.hpp>
#include <uhd/stream.hpp>
#include <uhd/types/metadata.hpp>
#include <uhd/types/time_spec.hpp>
#include <fmt/format.h>
#include <algorithm>
//#include <cmath>
#include <chrono>
//#include <cstddef>
//#include <cstdio>
#include <filesystem>
#include <fstream>
//#include <future>
//#include <memory>
#include <string>
#include <thread>
#include <utility>

rfnoc_awg::rfnoc_awg(const std::string& address, const std::atomic<bool>& stop) : awg_base(address, stop) {}

rfnoc_awg::~rfnoc_awg()
{
    for (auto& [channel, replay_graph] : replay_graphs) {
        replay_graph.replay_ctrl->stop(replay_graph.replay_port);
    }
}

bool rfnoc_awg::load_program(std::unique_ptr<sequencer_data> dat)
{
    seq_data      = std::move(dat);
    sampling_rate = seq_data->settings.sampling_rate;
    auto itemsize = static_cast<size_t>(seq_data->settings.cpu_format);

    size_t total_size = 0;
    for (auto& [id, seg] : seq_data->filemap) {
        auto length_bytes = std::filesystem::file_size(seg.filename.data());
        seg.length        = length_bytes / itemsize;
        total_size += length_bytes;
    }
    buffer.resize(total_size);

    size_t currsize = 0;
    for (auto& [id, seg] : seq_data->filemap) {
        seg.start_idx       = currsize;
        seg.data            = buffer.data() + currsize;
        size_t length_bytes = seg.length * 8;
        std::ifstream input_file(seg.filename.data(), std::ios::binary);
        input_file.read(buffer.data() + currsize, length_bytes);
        currsize += length_bytes;
        fmt::print(
            FMT_STRING(
                "Appended {:L} B of data from file '{}' for segment '{}' to buffer\n"),
            length_bytes,
            seg.filename,
            seg.name);
    }
    return true;
}

bool rfnoc_awg::initialize()
{
    fmt::print("Initializing host with address '{}'\n", address);
    try {
        create_graph();
        validate();
        connect_graph();
        setup_clocking();
        config_rfnoc_blocks();
        sync_dance();
        transmit_sequences();
    } catch (const std::exception& err) {
        fmt::print(stderr, FMT_STRING("{}\n"), err.what());
        return false;
    } catch (...) {
        fmt::print(stderr, FMT_STRING("Caught unknown exception\n"));
        return false;
    }
    return true;
}

bool rfnoc_awg::start()
{
    return true;
}

void rfnoc_awg::create_graph()
{
    fmt::print(FMT_STRING("Creating RFNoC graph with args: {}\n"), address);
    graph = uhd::rfnoc::rfnoc_graph::make(address);
}

void rfnoc_awg::validate()
{
    // The RFNoC implementation has several limitations, this checks each one.

    // Only one USRP device allowed
    if (graph->get_num_mboards() > 1) {
        throw uhd::runtime_error("RFNoC implementation only supports one USRP!");
    }

    // Must have a replay block...
    auto block_id = uhd::rfnoc::block_id_t("Replay#0");
    if (!graph->has_block(block_id)) {
        throw uhd::lookup_error(fmt::format(FMT_STRING("Could not find block {}"), block_id.to_string()));
    }
    auto replay_ctrl = graph->get_block<uhd::rfnoc::replay_block_control>(block_id);

    // Do not exceed the number of supported channels
    auto number_of_channels_used = seq_data->used_channels.size();
    auto number_of_channels_avail = replay_ctrl->get_num_output_ports();
    if (number_of_channels_used > number_of_channels_avail) {
        throw uhd::runtime_error(fmt::format(FMT_STRING("Sequences defined for too many channels. Defined: {}, Available: {}"),
            number_of_channels_used, number_of_channels_avail));
    }

    // Total segment memory usage cannot exceed the Replay block's available memory
    if (buffer.size() > replay_ctrl->get_mem_size()) {
        throw uhd::runtime_error(fmt::format(FMT_STRING("Total segments memory usage exceeds Replay Block's memory size. Used: {}, Available: {}"),
            buffer.size(), replay_ctrl->get_mem_size()));
    }

    // Only MAX_NUM_SEQ_POINTS number of sequence points
    for (const auto& [channel, seq_points] : seq_data->used_channels) {
        size_t num_seq_points = 0;
        for (const auto& seq_point : seq_points) {
            if (seq_point.repetitions > 0) {
                num_seq_points += seq_point.repetitions;
            }
            else {
                num_seq_points++;
            }
        }
        if (num_seq_points > MAX_NUM_SEQ_POINTS) {
            throw uhd::runtime_error(fmt::format(FMT_STRING("Total number of sequence points for channel {} exceed the maximum allowed. Number Defined: {}, Maximum Allowed: {}"),
                channel, num_seq_points, MAX_NUM_SEQ_POINTS));
        }
    }
}

void rfnoc_awg::connect_graph()
{
    auto check_block = [this](const std::string blockname) {
        auto block_id = uhd::rfnoc::block_id_t(blockname);
        if (!graph->has_block(block_id)) {
            throw uhd::lookup_error(fmt::format(FMT_STRING("Could not find block {}"), block_id.to_string()));
        }
        fmt::print(FMT_STRING("Found block {}\n"), block_id.to_string());
        return block_id;
    };

    auto connect_blocks = [this](const replay_graph_config replay_graph) {
        fmt::print(FMT_STRING("Connecting TX Streamer to {}:{}\n"),
            replay_graph.replay_ctrl->get_block_id().to_string(), replay_graph.replay_port);
        graph->connect(
            replay_graph.tx_stream, 0,
            replay_graph.replay_ctrl->get_block_id().to_string(), replay_graph.replay_port);
        fmt::print(FMT_STRING("Connecting {}:{} to {}:{}\n"),
            replay_graph.replay_ctrl->get_block_id().to_string(), replay_graph.replay_port,
            replay_graph.duc_ctrl->get_block_id().to_string(), replay_graph.duc_port);
        graph->connect(
            replay_graph.replay_ctrl->get_block_id().to_string(), replay_graph.replay_port,
            replay_graph.duc_ctrl->get_block_id().to_string(), replay_graph.duc_port);
        fmt::print(FMT_STRING("Connecting {}:{} to {}:{}\n"),
            replay_graph.duc_ctrl->get_block_id().to_string(), replay_graph.duc_port,
            replay_graph.radio_ctrl->get_block_id().to_string(), replay_graph.radio_port);
        graph->connect(
            replay_graph.duc_ctrl->get_block_id().to_string(), replay_graph.duc_port,
            replay_graph.radio_ctrl->get_block_id().to_string(), replay_graph.radio_port);
    };

    // WARNING: This is hardcoded for the X410 default image.
    for (const auto& [channel, sp] : seq_data->used_channels) {
        replay_graph_config replay_graph;
        switch (channel) {
            case 0:
                replay_graph = {
                    .replay_port = 0,
                    .duc_port    = 0,
                    .radio_port  = 0,
                    .tx_stream   = graph->create_tx_streamer(1, uhd::stream_args_t("fc32", "sc16")),
                    .replay_ctrl = graph->get_block<uhd::rfnoc::replay_block_control>(check_block("Replay#0")),
                    .duc_ctrl    = graph->get_block<uhd::rfnoc::duc_block_control>(check_block("DUC#0")),
                    .radio_ctrl  = graph->get_block<uhd::rfnoc::radio_control>(check_block("Radio#0"))
                };
                break;
            case 1:
                replay_graph = {
                    .replay_port = 1,
                    .duc_port    = 1,
                    .radio_port  = 1,
                    .tx_stream   = graph->create_tx_streamer(1, uhd::stream_args_t("fc32", "sc16")),
                    .replay_ctrl = graph->get_block<uhd::rfnoc::replay_block_control>(check_block("Replay#0")),
                    .duc_ctrl    = graph->get_block<uhd::rfnoc::duc_block_control>(check_block("DUC#0")),
                    .radio_ctrl  = graph->get_block<uhd::rfnoc::radio_control>(check_block("Radio#0"))
                };
                break;
            case 2:
                replay_graph = {
                    .replay_port = 2,
                    .duc_port    = 0,
                    .radio_port  = 0,
                    .tx_stream   = graph->create_tx_streamer(1, uhd::stream_args_t("fc32", "sc16")),
                    .replay_ctrl = graph->get_block<uhd::rfnoc::replay_block_control>(check_block("Replay#0")),
                    .duc_ctrl    = graph->get_block<uhd::rfnoc::duc_block_control>(check_block("DUC#1")),
                    .radio_ctrl  = graph->get_block<uhd::rfnoc::radio_control>(check_block("Radio#1"))
                };
                break;
            case 3:
                replay_graph = {
                    .replay_port = 3,
                    .duc_port    = 1,
                    .radio_port  = 1,
                    .tx_stream   = graph->create_tx_streamer(1, uhd::stream_args_t("fc32", "sc16")),
                    .replay_ctrl = graph->get_block<uhd::rfnoc::replay_block_control>(check_block("Replay#0")),
                    .duc_ctrl    = graph->get_block<uhd::rfnoc::duc_block_control>(check_block("DUC#1")),
                    .radio_ctrl  = graph->get_block<uhd::rfnoc::radio_control>(check_block("Radio#1"))
                };
                break;
            default:
                throw uhd::runtime_error(fmt::format("Invalid channel {} in sequence point", channel));
                break;
        }
        replay_graphs.emplace(channel, replay_graph);
    }

    for (const auto& [channel, replay_graph] : replay_graphs) {
        connect_blocks(replay_graph);
    }
    graph->commit();
}

void rfnoc_awg::config_rfnoc_blocks() {
    size_t settings_index = 0;
    for (const auto& [channel, seq_points] : seq_data->used_channels) {
        // RX Frequency
        auto [rx_freq, dsp_offset] = seq_data->settings.frequencies.at(settings_index);
        replay_graphs.at(channel).radio_ctrl->set_rx_frequency(rx_freq, replay_graphs.at(channel).radio_port);
        replay_graphs.at(channel).duc_ctrl->set_freq(dsp_offset, replay_graphs.at(channel).duc_port);

        // Gain
        replay_graphs.at(channel).radio_ctrl->set_rx_gain(seq_data->settings.gains.at(settings_index), replay_graphs.at(channel).radio_port);

        // Sampling rate
        replay_graphs.at(channel).duc_ctrl->set_output_rate(replay_graphs.at(channel).radio_ctrl->get_rate(), replay_graphs.at(channel).duc_port);
        replay_graphs.at(channel).duc_ctrl->set_input_rate(seq_data->settings.sampling_rate, replay_graphs.at(channel).duc_port);

        settings_index++;
    }

    // Load Replay block with segment data, doesn't matter what channel we use
    const auto replay_graph = replay_graphs[replay_graphs.begin()->first];
    const auto replay_ctrl = replay_graph.replay_ctrl;
    const auto tx_stream   = replay_graph.tx_stream;

    const uint64_t replay_buff_addr = 0;
    const uint64_t replay_buff_size_bytes = buffer.size()/(static_cast<int>(seq_data->settings.cpu_format)/static_cast<int>(seq_data->settings.wire_format));
    const size_t   send_buff_size_samples = buffer.size()/static_cast<int>(seq_data->settings.cpu_format);

    // Display replay configuration
    fmt::print(FMT_STRING("Segments combined buffer size (bytes): {}\n"), replay_buff_size_bytes);
    fmt::print(FMT_STRING("Replay block available memory (bytes): {}\n"), replay_ctrl->get_mem_size());
    fmt::print(FMT_STRING("Replay block memory usage: {:.3f}%\n"), static_cast<float>(replay_buff_size_bytes)/static_cast<float>(replay_ctrl->get_mem_size()));

    // Ensure Replay block input buffer is flushed
    uint64_t fullness = 0;
    do {
        replay_ctrl->record_restart();

        // Make sure the record buffer doesn't start to fill again
        auto start_time = std::chrono::steady_clock::now();
        do {
            fullness = replay_ctrl->get_record_fullness();
            if (fullness != 0)
                break;
        } while (start_time + std::chrono::milliseconds(250) > std::chrono::steady_clock::now());
    } while (fullness);

    // Stream data to replay block
    replay_ctrl->record(replay_buff_addr, replay_buff_size_bytes);

    uhd::tx_metadata_t tx_md;
    tx_md.start_of_burst = true;
    tx_md.end_of_burst   = true;

    fmt::print(FMT_STRING("Sending {} samples to Replay block...\n"), send_buff_size_samples);

    size_t num_tx_samps = tx_stream->send(buffer.data(), send_buff_size_samples, tx_md, 5.0);
    if (num_tx_samps != send_buff_size_samples) {
        throw uhd::runtime_error(fmt::format(FMT_STRING("Failed to send all samples to Replay block. Timed out after {} of {} samples."),
            num_tx_samps, send_buff_size_samples));
    }

    // Wait for samples to fill Replay Block's memory
    size_t loop_timeout = 20;
    do {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        fullness = replay_ctrl->get_record_fullness();
        if (loop_timeout == 0) {
            throw uhd::runtime_error(fmt::format(FMT_STRING("Replay block only recorded {} of {} samples."),
                fullness, replay_buff_size_bytes));
        }
        loop_timeout--;
    } while(fullness < replay_buff_size_bytes);
}

void rfnoc_awg::setup_clocking()
{
    std::string clk_source(seq_data->settings.clock_source == clock_source_e::EXTERNAL
                               ? "external"
                               : "internal");
    try {
        graph->get_mb_controller(0)->set_clock_source(clk_source);
    }
    catch (const uhd::runtime_error& e) {
        fmt::print(FMT_STRING("Clock source not supported on this device ({})\n"),
            e.what());
    }
    try {
        graph->get_mb_controller(0)->set_time_source("internal");
    }
    catch (const uhd::runtime_error& e) {
        fmt::print(
            FMT_STRING("Time source not supported on this device ({})\n"), e.what());
    }
}

void rfnoc_awg::sync_dance()
{
    // Right now the RFNoC implementation only supports one USRP, so
    // reset the timestamp to 0 on next PPS edge.
    graph->get_mb_controller(0)->get_timekeeper(0)->set_ticks_next_pps(0);
}

void rfnoc_awg::transmit_sequences()
{
    for (const auto& [channel, seq_points] : seq_data->used_channels) {
        const auto replay_graph = replay_graphs.at(channel);
        const auto replay_ctrl = replay_graph.replay_ctrl;

        for (size_t i = 0; i < seq_points.size(); ++i) {
            const auto seq_point = seq_points.at(i);
            const auto sspec = seq_data->filemap.at(seq_point.segment);

            const uint64_t replay_buff_addr = sspec.start_idx*static_cast<int>(seq_data->settings.wire_format);
            const uint64_t replay_buff_size_samples = sspec.length;
            const uint64_t replay_buff_size_bytes = replay_buff_size_samples*static_cast<int>(seq_data->settings.wire_format);
            uhd::time_spec_t time_spec = uhd::time_spec_t(seq_point.start_time + START_TIME_OFFSET);

            if (seq_point.repetitions == -1) {
                uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
                stream_cmd.time_spec = time_spec;
                stream_cmd.stream_now = false;

                fmt::print(FMT_STRING("Chan {} -- Time: {}, Num Samples {}, Replay Addr: {}\n"), channel, time_spec.get_real_secs(), replay_buff_size_samples, replay_buff_addr);
                replay_ctrl->config_play(replay_buff_addr, replay_buff_size_bytes, replay_graph.replay_port);
                replay_ctrl->issue_stream_cmd(stream_cmd, replay_graph.replay_port);
            }
            else {
                auto reps_left = seq_point.repetitions + 1;
                replay_ctrl->config_play(replay_buff_addr, replay_buff_size_bytes, replay_graph.replay_port);
                do {
                    uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_NUM_SAMPS_AND_MORE);
                    // Last sequence point
                    if ((reps_left == 1) && (i == seq_points.size()-1)) {
                        stream_cmd.stream_mode = uhd::stream_cmd_t::STREAM_MODE_NUM_SAMPS_AND_DONE;
                    }
                    stream_cmd.num_samps = replay_buff_size_samples;
                    stream_cmd.stream_now = false;
                    stream_cmd.time_spec = time_spec;

                    fmt::print(FMT_STRING("Chan {} -- Time: {}, Num Samples {}, Replay Addr: {}\n"), channel, time_spec.get_real_secs(), replay_buff_size_samples, replay_buff_addr);
                    replay_ctrl->issue_stream_cmd(stream_cmd, replay_graph.replay_port);

                    double time_increment = static_cast<double>(replay_buff_size_samples)/seq_data->settings.sampling_rate;
                    time_spec += uhd::time_spec_t(time_increment);

                    reps_left--;
                } while (reps_left > 0);
            }
        }
    }

    fmt::print("Transmitting sequences (Press Ctrl+C to stop)...\n");
    while (!stop.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    fmt::print("Stopping...\n");
    for (const auto& [channel, seq_points] : seq_data->used_channels) {
        const auto replay_graph = replay_graphs.at(channel);
        const auto replay_ctrl = replay_graph.replay_ctrl;

        replay_ctrl->stop(replay_graph.replay_port);
    }
    fmt::print("Letting device settle...\n");
    std::this_thread::sleep_for(std::chrono::seconds(1));
}
