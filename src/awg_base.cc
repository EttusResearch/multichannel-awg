/*
 * Copyright 2023 Ettus Research, A National Instruments Brand
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include "quadchannel_awg/quadchannel_awg.hpp"
#include <string>

awg_base::awg_base(const std::string& addr) : address(addr){};
awg_base::~awg_base()
{
    ;
}
