/*
 * Copyright 2023 Ettus Research, A National Instruments Brand
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include "multichannel_awg/multichannel_awg.hpp"
#include <string>
#include <atomic>

awg_base::awg_base(const std::string& addr, const std::atomic<bool>& stop) : address(addr), stop(stop) {};
awg_base::~awg_base()
{
    ;
}
