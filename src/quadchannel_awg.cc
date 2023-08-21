/*
 * Copyright 2023 Ettus Research, A National Instruments Brand
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include "quadchannel_awg/quadchannel_awg.hpp"
#include "quadchannel_awg/host_awg.hpp"
// TODO include rfnoc awg
#include <memory>
#include <stdexcept>
#include <string>

std::unique_ptr<awg_base> awg_factory::make(
    const std::string& name, const std::string& address)
{
    // TODO add "rfnoc" clause
    if (name == "host") {
        return std::unique_ptr<awg_base>(new host_awg(address));
    } else {
        throw std::runtime_error("factory for mode \"" + name + "\" not implemented");
    }
}
