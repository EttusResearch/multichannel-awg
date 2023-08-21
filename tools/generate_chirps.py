#!/usr/bin/env python3
# -*- coding: utf-8 -*-

#
# SPDX-License-Identifier: GPL-3.0
#
# GNU Radio Python Flow Graph
# Title: Generate Chirps
# Author: Marcus Müller
# Copyright: 2023
# GNU Radio version: v3.11.0.0git-499-g5eee613e

from gnuradio import analog
from gnuradio import blocks
from gnuradio import gr
from gnuradio.filter import firdes
from gnuradio.fft import window
import sys
import signal
from argparse import ArgumentParser
from gnuradio.eng_arg import eng_float, intx
from gnuradio import eng_notation




class generate_chirps(gr.top_block):

    def __init__(self, chirp_rate=1, length=(10**6), output='chirp.fc32', samp_rate=10e6):
        gr.top_block.__init__(self, "Generate Chirps", catch_exceptions=True)

        ##################################################
        # Parameters
        ##################################################
        self.chirp_rate = chirp_rate
        self.length = length
        self.output = output
        self.samp_rate = samp_rate

        ##################################################
        # Blocks
        ##################################################

        self.blocks_head_0 = blocks.head(gr.sizeof_gr_complex*1, length)
        self.blocks_file_sink_0 = blocks.file_sink(gr.sizeof_gr_complex*1, output, False)
        self.blocks_file_sink_0.set_unbuffered(False)
        self.analog_sig_source_x_0 = analog.sig_source_f(samp_rate, analog.GR_SAW_WAVE, chirp_rate, 3.141, (-3.141/2), 0)
        self.analog_frequency_modulator_fc_0 = analog.frequency_modulator_fc(chirp_rate)


        ##################################################
        # Connections
        ##################################################
        self.connect((self.analog_frequency_modulator_fc_0, 0), (self.blocks_head_0, 0))
        self.connect((self.analog_sig_source_x_0, 0), (self.analog_frequency_modulator_fc_0, 0))
        self.connect((self.blocks_head_0, 0), (self.blocks_file_sink_0, 0))


    def get_chirp_rate(self):
        return self.chirp_rate

    def set_chirp_rate(self, chirp_rate):
        self.chirp_rate = chirp_rate
        self.analog_frequency_modulator_fc_0.set_sensitivity(self.chirp_rate)
        self.analog_sig_source_x_0.set_frequency(self.chirp_rate)

    def get_length(self):
        return self.length

    def set_length(self, length):
        self.length = length
        self.blocks_head_0.set_length(self.length)

    def get_output(self):
        return self.output

    def set_output(self, output):
        self.output = output
        self.blocks_file_sink_0.open(self.output)

    def get_samp_rate(self):
        return self.samp_rate

    def set_samp_rate(self, samp_rate):
        self.samp_rate = samp_rate
        self.analog_sig_source_x_0.set_sampling_freq(self.samp_rate)



def argument_parser():
    parser = ArgumentParser()
    parser.add_argument(
        "--chirp-rate", dest="chirp_rate", type=eng_float, default=eng_notation.num_to_str(float(1)),
        help="Set Chirp Rate [default=%(default)r]")
    parser.add_argument(
        "--length", dest="length", type=intx, default=(10**6),
        help="Set Length in Samples [default=%(default)r]")
    parser.add_argument(
        "-o", "--output", dest="output", type=str, default='chirp.fc32',
        help="Set Output file [default=%(default)r]")
    parser.add_argument(
        "--samp-rate", dest="samp_rate", type=eng_float, default=eng_notation.num_to_str(float(10e6)),
        help="Set Sampling Rate [default=%(default)r]")
    return parser


def main(top_block_cls=generate_chirps, options=None):
    if options is None:
        options = argument_parser().parse_args()
    tb = top_block_cls(chirp_rate=options.chirp_rate, length=options.length, output=options.output, samp_rate=options.samp_rate)

    def sig_handler(sig=None, frame=None):
        tb.stop()
        tb.wait()

        sys.exit(0)

    signal.signal(signal.SIGINT, sig_handler)
    signal.signal(signal.SIGTERM, sig_handler)

    tb.start()

    tb.wait()


if __name__ == '__main__':
    main()
