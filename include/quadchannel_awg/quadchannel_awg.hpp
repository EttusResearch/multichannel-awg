/*
 * Copyright 2023 Ettus Research, A National Instruments Brand
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#pragma once

#include "sequence.hpp"
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

//!\brief base class for host, RFNoC, person-rapidly-cycling-floppy-disks-through-R&S-AWG,…
class awg_base
{
public:
    awg_base(const std::string& addr);
    
    //!\brief Overload this method; it's called before the hardware gets initialized
    virtual bool load_program(sequencer_data& seq) = 0;

    //!\brief Overload this method; initialize hardware here.
    virtual bool initialize()                      = 0;

    //!\brief Overload this method; this starts the transmitter
    //virtual bool start()=0;
    virtual ~awg_base();

protected:
    const std::string address;
};


struct awg_factory
{
    std::unique_ptr<awg_base> make(const std::string& name, const std::string& address);
};
