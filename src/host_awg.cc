/*
 * Copyright 2023 Ettus Research, A National Instruments Brand
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include "multichannel_awg/host_awg.hpp"
#include "fmt/core.h"
#include "multichannel_awg/multichannel_awg.hpp"
#include "multichannel_awg/sequence.hpp"
#include <uhd/exception.hpp>
#include <uhd/stream.hpp>
#include <uhd/types/metadata.hpp>
#include <uhd/types/time_spec.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <fmt/format.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <utility>


constexpr unsigned int time_offset = (1ULL << 20);

host_awg::host_awg(const std::string& address) : awg_base(address) {}

bool host_awg::load_program(std::unique_ptr<sequencer_data> dat)
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
    for (auto& [channel, sp_container] : seq_data->used_channels) {
        sequence_workers.emplace(std::make_pair(channel,
            sequencer_state{
                sp_container.begin(), sp_container.cend(), seq_data.get(), nullptr}));
    }
    return true;
}

bool host_awg::initialize()
{
    fmt::print("Initializing host with address '{}'\n", address);
    try {
        usrp = uhd::usrp::multi_usrp::make(address);
        setup_clocking();
        sync_dance();
    } catch (const uhd::lookup_error& err) {
        fmt::print(stderr, FMT_STRING("{}"), err.what());
        return false;
    }
    for (auto& [channel, seq_state] : sequence_workers) {
        (void)channel;
        seq_state.usrp = usrp;
    }
    return true;
}

bool host_awg::start()
{
    for (auto& [channel, s_state] : sequence_workers) {
        (void)channel;
        s_state();
    }
    return true;
}

void host_awg::setup_clocking()
{
    // We set the master clock source
    // TODO check whether samp rate is factor of master_clock_rate
    // usrp->set_master_clock_rate(200e6);
    usrp->set_tx_rate(sampling_rate);

    // We set the clock source of the 0. USRP to internal, all others
    // get the clock via clock distribution from that. Same for PPS.
    std::string clk_source(seq_data->settings.clock_source == clock_source_e::EXTERNAL
                               ? "external"
                               : "internal");
    usrp->set_clock_source(clk_source, 0);
    try {
        usrp->set_clock_source_out(true, 0);
    } catch (const uhd::runtime_error& e) {
        fmt::print(FMT_STRING("Setting clock out not supported on this device ({})\n"),
            e.what());
    }
    usrp->set_time_source("internal", 0);
    try {
        usrp->set_time_source_out(true, 0);
    } catch (const uhd::runtime_error& e) {
        fmt::print(
            FMT_STRING("Setting time out not supported on this device ({})\n"), e.what());
    }

    auto count = usrp->get_num_mboards();
    for (unsigned counter = 1; counter < count; ++counter) {
        // second and on must be fed from first one
        usrp->set_clock_source("external", counter);
        usrp->set_time_source("external", counter);
    }
}
void host_awg::sync_dance()
{
    // Do the sync dance.
    // Problem: we don't know how close we currently are to a PPS edge. If this
    // happens so that the "set time on the next PPS edge" command doesn't reach all
    // USRPs before the next PPS occurs, some will be of by nearly 1 s! Thus, we'll
    // have to first set a time to make sure we're close after an edge, then set the
    // correct time to all devices.
    usrp->set_time_next_pps({666666 /* value chosen by fair dice roll */, 0}, 0);
    auto last_time = usrp->get_time_last_pps(0);
    auto new_time  = usrp->get_time_last_pps(0);
    // Poll the device time on the first device until it changes,
    // at which point we're sure the new time has been set on a PPS edge
    while (last_time == new_time) {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(20ms);
        new_time = usrp->get_time_last_pps(0);
    }
    // we now know that we're within 20 ms of the beginning of a new second, and 980
    // ms is  plenty of time to set the time on all devices
    // we uniformly add 2²⁰ to all times, giving us a bit of a headstart :)
    usrp->set_time_next_pps({time_offset, 0});
}

host_awg::~host_awg()
{
    ;
}

void sequencer_state::operator()()
{
    // TODO make types flexible
    tx_streamer = usrp->get_tx_stream({"fc32", "sc16"});

    const size_t buffersize = tx_streamer->get_max_num_samps();
    const size_t itemsize   = static_cast<size_t>(data->settings.cpu_format);

    while (begin != end) {
        sequence_point& current_sp = *begin;
        auto segment_name          = current_sp.segment;
        fmt::print(
            FMT_STRING("Channel {} Segment {} Start Time {} segment name \"{}\"\n"),
            current_sp.channel,
            current_sp.segment,
            current_sp.start_time,
            segment_name);

        const segment_spec& sspec = data->filemap.at(segment_name);
        // using namespace std::chrono_literals;
        // std::this_thread::sleep_for(1000ms);
        uhd::tx_metadata_t metadata;
        metadata.has_time_spec = true;
        metadata.time_spec     = uhd::time_spec_t{current_sp.start_time}
                             + uhd::time_spec_t{static_cast<double>(time_offset)};
        metadata.start_of_burst = false;
        metadata.end_of_burst   = false;

        const double timeout   = 3600; // seconds
        size_t transmitted_yet = 0;
        while (transmitted_yet < sspec.length) {
            size_t samples_to_send = std::min(sspec.length - transmitted_yet, buffersize);
            size_t sent_this_iteration =
                tx_streamer->send(sspec.data + transmitted_yet * itemsize,
                    samples_to_send,
                    metadata,
                    timeout);
            if (sent_this_iteration < samples_to_send) {
                fmt::print(stderr,
                    FMT_STRING(
                        "Transmitted less samples than expected ({}  <  {}). Timeout?\n"),
                    sent_this_iteration,
                    samples_to_send);
            }
            transmitted_yet += sent_this_iteration;
            metadata.has_time_spec = false;
        }

        if (current_sp.repetitions > 1) /* more than one repitition left*/
        {
            --current_sp.repetitions;
        } else if (current_sp.repetitions < 1) /*loop endlessly*/
        {
            continue;
        } else {
            ++begin;
        }
    }
}
