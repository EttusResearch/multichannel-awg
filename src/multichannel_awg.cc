/*
 * Copyright 2023 Ettus Research, A National Instruments Brand
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include "multichannel_awg/multichannel_awg.hpp"
#include "multichannel_awg/host_awg.hpp"
#include "multichannel_awg/rfnoc_awg.hpp"
// TODO include rfnoc awg
#include <memory>
#include <stdexcept>
#include <string>

std::unique_ptr<awg_base> awg_factory::make(
    const std::string& name, const std::string& address, const std::atomic<bool>& stop)
{
    if (name == "host") {
        return std::unique_ptr<awg_base>(new host_awg(address, stop));
    } else if (name == "rfnoc") {
        return std::unique_ptr<awg_base>(new rfnoc_awg(address, stop));
    } else {
        throw std::runtime_error("factory for mode \"" + name + "\" not implemented");
    }
}
