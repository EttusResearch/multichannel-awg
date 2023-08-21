/*
 * Copyright 2023 Ettus Research, A National Instruments Brand
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include "quadchannel_awg/host_awg.hpp"
#include "quadchannel_awg/quadchannel_awg.hpp"
#include <uhd/exception.hpp>
#include <uhd/types/time_spec.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <fmt/format.h>
#include <string>
#include <thread>
host_awg::host_awg(const std::string& address) : awg_base(address) {}
host_awg::~host_awg()
{
    ;
}
bool host_awg::initialize()
{
    fmt::print("Initializing host with address '{}'\n", address);
    try {
        usrp = uhd::usrp::multi_usrp::make(address);
        setup_clocking();
        sync_dance();

    } catch (const uhd::lookup_error& err) {
        fmt::print(err.what());
        return false;
    }
    return true;
}

bool host_awg::load_program(sequencer_data& dat)
{
    for(const auto&[id, seg]: dat.filemap) 
    {
    }
    return true;
}
void host_awg::setup_clocking()
{
    // We set the master clock source
    // TODO check whether samp rate is factor of master_clock_rate
    // usrp->set_master_clock_rate(200e6);
    usrp->set_tx_rate(samp_rate);

    // We set the clock source of the 0. USRP to internal, all others
    // get the clock via clock distribution from that. Same for PPS.
    usrp->set_clock_source("internal", 0);
    usrp->set_clock_source_out(true, 0);
    usrp->set_time_source("internal", 0);
    usrp->set_time_source_out(true, 0);
    auto count = usrp->get_num_mboards();
    for (unsigned counter = 1; counter < count; ++counter) {
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
    usrp->set_time_next_pps({666666, 0}, 0);
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
    usrp->set_time_next_pps({(1ULL << 20), 0});
}
