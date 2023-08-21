/*
 * Copyright 2023 Ettus Research, A National Instruments Brand
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include "quadchannel_awg/quadchannel_awg.hpp"
#include "quadchannel_awg/sequence.hpp"
#include "CLI11/CLI11.hpp"
#include <iostream>
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
    std::string wire_format{"sc16"};
    std::string filename;

    app.add_option("-a,--address", device_address, "Device address to use");
    app.add_option("-m,--mode", mode, "Mode (host or rfnoc)")
        ->capture_default_str()
        ->transform(CLI::IsMember(valid_modes, CLI::ignore_case));
    app.add_option("-f,--file", filename, "Sequencer command file; defaults to stdin");
    app.add_option("-w,--wire_format", wire_format, "Wire format")
        ->capture_default_str()
        ->transform(CLI::IsMember(valid_otw_formats, CLI::ignore_case));

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
    sequencer_data sequencer_d(data);
    if (!awg->initialize()) {
        return -1;
    }
    return 0;
}
