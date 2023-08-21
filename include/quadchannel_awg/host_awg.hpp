/*
 * Copyright 2023 Ettus Research, A National Instruments Brand
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#pragma once

#include "quadchannel_awg.hpp"
#include "sequence.hpp"
#include <string>


// fwd decl
namespace uhd::usrp {
class multi_usrp;
}
class host_awg : virtual public awg_base
{
public:
    host_awg(const std::string& address);
    bool load_program(sequencer_data& seq) override;
    bool initialize() override;
    virtual ~host_awg();

private:
    void setup_clocking();
    void sync_dance();
    std::shared_ptr<uhd::usrp::multi_usrp> usrp;

    double samp_rate;
    
};
