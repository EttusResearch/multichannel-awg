/*
 * Copyright 2023 Ettus Research, A National Instruments Brand
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include "multichannel_awg/multichannel_awg.hpp"
#include "multichannel_awg/sequence.hpp"
#include "CLI11/CLI11.hpp"
#include <iostream>
#include <memory>
#include <set>
#include <string>

int main(int argc, char* argv[])
{
    CLI::App app{"Sequencing Multichannel AWG"};
    app.get_formatter()->column_width(50);


    std::set<std::string> valid_modes{"host", "rfnoc"};
    std::set<std::string> valid_otw_formats{"sc16", "sc8", "sc12"};
    std::string mode{"host"};
    std::string device_address;
    std::string filename;

    app.add_option("-a,--address", device_address, "Device address to use");
    app.add_option("-m,--mode", mode, "Mode (host or rfnoc)")
        ->capture_default_str()
        ->transform(CLI::IsMember(valid_modes, CLI::ignore_case));
    app.add_option("-f,--file", filename, "Sequencer command file; defaults to stdin");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& err) {
        return app.exit(err);
    }
    // TODO use mode arg
    auto awg = awg_factory().make(mode, device_address);
    nlohmann::json data;
    if (filename.empty()) {
        std::cin >> data;
    } else {
        data = data.parse(std::ifstream(filename));
    }
    auto sequencer_d = std::make_unique<sequencer_data>(data);
    if (!awg->load_program(std::move(sequencer_d))) {
        return -1;
    }
    if (!awg->initialize()) {
        return -2;
    }
    if(!awg->start()) {
        return -3;
    }
    return 0;
}
