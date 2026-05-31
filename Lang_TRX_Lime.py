#!/usr/bin/env python3
# -*- coding: utf-8 -*-

#
# SPDX-License-Identifier: GPL-3.0
#
# GNU Radio Python Flow Graph
# Title: Lang Trx Lime
# GNU Radio version: 3.10.5.1

from gnuradio import analog
from gnuradio import audio
from gnuradio import blocks
from gnuradio import filter
from gnuradio.filter import firdes
from gnuradio import gr
from gnuradio.fft import window
import sys
import signal
from argparse import ArgumentParser
from gnuradio.eng_arg import eng_float, intx
from gnuradio import eng_notation
from gnuradio import network
from gnuradio.fft import logpwrfft
import osmosdr
import time
import os
import errno



class Lang_TRX_Lime(gr.top_block):

    def __init__(self):
        gr.top_block.__init__(self, "Lang Trx Lime", catch_exceptions=True)

        ##################################################
        # Variables
        ##################################################
        self.Tx_Mode = Tx_Mode = 0
        self.Tx_LO = Tx_LO = 435100000
        self.Tx_Gain = Tx_Gain = -40
        self.Tx_Filt_Low = Tx_Filt_Low = 300
        self.Tx_Filt_High = Tx_Filt_High = 3000
        self.ToneBurst = ToneBurst = False
        self.SSB_G_EQ_M2 = SSB_G_EQ_M2 = 40
        self.SSB_G_EQ_M1 = SSB_G_EQ_M1 = 20
        self.SSB_G_EQ_L = SSB_G_EQ_L = 1
        self.SSB_G_EQ_H = SSB_G_EQ_H = 52
        self.Rx_Mute = Rx_Mute = False
        self.Rx_Mode = Rx_Mode = 0
        self.Rx_LO = Rx_LO = 435100000
        self.Rx_Gain = Rx_Gain = -12
        self.Rx_Filt_Low = Rx_Filt_Low = 300
        self.Rx_Filt_High = Rx_Filt_High = 3000
        self.RxOffset = RxOffset = 0
        self.RATE = RATE = 0
        self.PTT = PTT = False
        self.MicGain = MicGain = 5.0
        self.KEY = KEY = False
        self.FMMIC = FMMIC = 50
        self.FFT_SEL = FFT_SEL = 1
        self.CTCSS = CTCSS = 885
        self.AMMIC = AMMIC = 5
        self.AGC_DECAY = AGC_DECAY = 30
        self.AFGain = AFGain = 100

        ##################################################
        # Blocks
        ##################################################

        self.rational_resampler_xxx_0 = filter.rational_resampler_ccc(
                interpolation=11,
                decimation=1,
                taps=[],
                fractional_bw=0)
        self.osmosdr_source_0 = osmosdr.source(
            args="numchan=" + str(1) + " " + 'soapy=0,driver=lime'
        )
        self.osmosdr_source_0.set_sample_rate(528000)
        self.osmosdr_source_0.set_center_freq(Rx_LO, 0)
        self.osmosdr_source_0.set_freq_corr(0, 0)
        self.osmosdr_source_0.set_dc_offset_mode(0, 0)
        self.osmosdr_source_0.set_iq_balance_mode(0, 0)
        self.osmosdr_source_0.set_gain_mode(False, 0)
        self.osmosdr_source_0.set_gain(Rx_Gain, 0)
        self.osmosdr_source_0.set_if_gain(0, 0)
        self.osmosdr_source_0.set_bb_gain(0, 0)
        self.osmosdr_source_0.set_antenna('', 0)
        self.osmosdr_source_0.set_bandwidth(2000000, 0)
        self.osmosdr_sink_0 = osmosdr.sink(
            args="numchan=" + str(1) + " " + 'soapy=0,driver=lime'
        )
        self.osmosdr_sink_0.set_sample_rate(528000)
        self.osmosdr_sink_0.set_center_freq(Tx_LO, 0)
        self.osmosdr_sink_0.set_freq_corr(0, 0)
        self.osmosdr_sink_0.set_gain(Tx_Gain, 0)
        self.osmosdr_sink_0.set_if_gain(10, 0)
        self.osmosdr_sink_0.set_bb_gain(10, 0)
        self.osmosdr_sink_0.set_antenna('', 0)
        self.osmosdr_sink_0.set_bandwidth(2000000, 0)
        self.network_udp_sink_1 = network.udp_sink(gr.sizeof_float, 1, '127.0.0.1', 7474, 0, 2048, False)
        self.network_udp_sink_0 = network.udp_sink(gr.sizeof_float, 1, '127.0.0.1', 7373, 0, 2048, False)
        self.low_pass_filter_0 = filter.fir_filter_fff(
            1,
            firdes.low_pass(
                1,
                48000,
                3000,
                1000,
                window.WIN_HAMMING,
                6.76))
        self.logpwrfft_x_0_0 = logpwrfft.logpwrfft_c(
            sample_rate=(48000/ (2** FFT_SEL)),
            fft_size=512,
            ref_scale=2,
            frame_rate=15,
            avg_alpha=0.9,
            average=True,
            shift=False)
        self.logpwrfft_x_0 = logpwrfft.logpwrfft_c(
            sample_rate=(48000/ (2 ** FFT_SEL)),
            fft_size=512,
            ref_scale=2,
            frame_rate=15,
            avg_alpha=0.9,
            average=True,
            shift=False)
        self.freq_xlating_fir_filter_xxx_0 = filter.freq_xlating_fir_filter_ccc(11, firdes.low_pass(1,529200,23000,2000), RxOffset, 528000)
        self.blocks_vector_to_stream_0_0 = blocks.vector_to_stream(gr.sizeof_float*1, 512)
        self.blocks_vector_to_stream_0 = blocks.vector_to_stream(gr.sizeof_float*1, 512)
        self.blocks_mute_xx_0_0_0 = blocks.mute_cc(bool((not PTT) or (Tx_Mode==2 and not KEY) or (Tx_Mode==3 and not KEY)))
        self.blocks_multiply_xx_0 = blocks.multiply_vcc(1)
        self.blocks_multiply_const_vxx_4 = blocks.multiply_const_cc((Tx_Mode < 4) or (Tx_Mode==5))
        self.blocks_multiply_const_vxx_3 = blocks.multiply_const_cc(Tx_Mode==4)
        self.blocks_multiply_const_vxx_2_2 = blocks.multiply_const_ff(Tx_Mode<4)
        self.blocks_multiply_const_vxx_2_1_0 = blocks.multiply_const_ff((1.0 + (Rx_Mode==5)))
        self.blocks_multiply_const_vxx_2_1 = blocks.multiply_const_ff(Rx_Mode==5)
        self.blocks_multiply_const_vxx_2_0_0 = blocks.multiply_const_ff((Tx_Mode==4) or (Tx_Mode==5))
        self.blocks_multiply_const_vxx_2_0 = blocks.multiply_const_ff(((Rx_Mode==4) * 0.2))
        self.blocks_multiply_const_vxx_2 = blocks.multiply_const_ff(Rx_Mode<4)
        self.blocks_multiply_const_vxx_1 = blocks.multiply_const_ff(((AFGain/100.0) *  (not Rx_Mute)))
        self.blocks_multiply_const_vxx_0_0 = blocks.multiply_const_ff((FMMIC/5.0))
        self.blocks_multiply_const_vxx_0 = blocks.multiply_const_ff(((MicGain)*(int(Tx_Mode==0)) + (MicGain)*(int(Tx_Mode==1)) + (AMMIC/5.0)*(int(Tx_Mode==5)) ))
        self.blocks_keep_one_in_n_0_0 = blocks.keep_one_in_n(gr.sizeof_gr_complex*1, (2 ** FFT_SEL))
        self.blocks_keep_one_in_n_0 = blocks.keep_one_in_n(gr.sizeof_gr_complex*1, (2 ** FFT_SEL))
        self.blocks_float_to_complex_0_0 = blocks.float_to_complex(1)
        self.blocks_complex_to_real_0 = blocks.complex_to_real(1)
        self.blocks_complex_to_mag_0 = blocks.complex_to_mag(1)
        self.blocks_add_xx_2 = blocks.add_vcc(1)
        self.blocks_add_xx_1_0_0 = blocks.add_vff(1)
        self.blocks_add_xx_1_0 = blocks.add_vff(1)
        self.blocks_add_xx_1 = blocks.add_vff(1)
        self.blocks_add_xx_0_0_0 = blocks.add_vff(1)
        self.blocks_add_xx_0_0 = blocks.add_vff(1)
        self.blocks_add_xx_0 = blocks.add_vff(1)
        self.blocks_add_const_vxx_0_0 = blocks.add_const_ff(((0.5 * int(Tx_Mode==5)) + int(Tx_Mode==2) +int(Tx_Mode==3)))
        self.band_pass_filter_mic4 = filter.interp_fir_filter_fff(
            1,
            firdes.band_pass(
                (SSB_G_EQ_H/10),
                48000,
                2350,
                3300,
                100,
                window.WIN_HAMMING,
                6.76))
        self.band_pass_filter_mic3 = filter.interp_fir_filter_fff(
            1,
            firdes.band_pass(
                (SSB_G_EQ_M2/10),
                48000,
                1450,
                2350,
                100,
                window.WIN_HAMMING,
                6.76))
        self.band_pass_filter_mic2 = filter.interp_fir_filter_fff(
            1,
            firdes.band_pass(
                (SSB_G_EQ_M1/10),
                48000,
                650,
                1450,
                100,
                window.WIN_HAMMING,
                6.76))
        self.band_pass_filter_mic1 = filter.interp_fir_filter_fff(
            1,
            firdes.band_pass(
                (SSB_G_EQ_L/10),
                48000,
                100,
                750,
                100,
                window.WIN_HAMMING,
                6.76))
        self.band_pass_filter_1 = filter.fir_filter_fff(
            1,
            firdes.band_pass(
                1,
                48000,
                300,
                3500,
                100,
                window.WIN_HAMMING,
                6.76))
        self.band_pass_filter_0_0 = filter.fir_filter_ccc(
            1,
            firdes.complex_band_pass(
                1,
                48000,
                Tx_Filt_Low,
                Tx_Filt_High,
                100,
                window.WIN_HAMMING,
                6.76))
        self.band_pass_filter_0 = filter.fir_filter_ccc(
            1,
            firdes.complex_band_pass(
                1,
                48000,
                Rx_Filt_Low,
                Rx_Filt_High,
                100,
                window.WIN_HAMMING,
                6.76))
        self.audio_source_0 = audio.source(48000, "hw:CARD=Device,DEV=0", False)
        self.audio_sink_0 = audio.sink(48000, "hw:CARD=Device,DEV=0", False)
        self.analog_sig_source_x_1_0 = analog.sig_source_f(48000, analog.GR_SIN_WAVE, (CTCSS/10.0), (0.15 * (CTCSS >0)), 0, 0)
        self.analog_sig_source_x_1 = analog.sig_source_f(48000, analog.GR_COS_WAVE, 1750, (1.0*ToneBurst), 0, 0)
        self.analog_sig_source_x_0 = analog.sig_source_c(48000, analog.GR_COS_WAVE, 0, 1, 0, 0)
        self.analog_rail_ff_0_0 = analog.rail_ff((-0.99), 0.99)
        self.analog_rail_ff_0 = analog.rail_ff((-1), 1)
        self.analog_nbfm_tx_0 = analog.nbfm_tx(
        	audio_rate=48000,
        	quad_rate=48000,
        	tau=(75e-6),
        	max_dev=12500,
        	fh=(-1),
                )
        self.analog_nbfm_rx_0 = analog.nbfm_rx(
        	audio_rate=48000,
        	quad_rate=48000,
        	tau=(75e-6),
        	max_dev=12500,
          )
        self.analog_const_source_x_0 = analog.sig_source_f(0, analog.GR_CONST_WAVE, 0, 0, 0)
        self.analog_agc2_xx_0 = analog.agc2_ff((0.1), (0.00002), (1.0), 1.0)
        self.analog_agc2_xx_0.set_max_gain(150)


        ##################################################
        # Connections
        ##################################################
        self.connect((self.analog_agc2_xx_0, 0), (self.blocks_multiply_const_vxx_2_1_0, 0))
        self.connect((self.analog_const_source_x_0, 0), (self.blocks_float_to_complex_0_0, 1))
        self.connect((self.analog_nbfm_rx_0, 0), (self.blocks_multiply_const_vxx_2_0, 0))
        self.connect((self.analog_nbfm_tx_0, 0), (self.blocks_multiply_const_vxx_3, 0))
        self.connect((self.analog_rail_ff_0, 0), (self.band_pass_filter_1, 0))
        self.connect((self.analog_rail_ff_0_0, 0), (self.blocks_float_to_complex_0_0, 0))
        self.connect((self.analog_sig_source_x_0, 0), (self.blocks_multiply_xx_0, 1))
        self.connect((self.analog_sig_source_x_1, 0), (self.blocks_add_xx_0, 0))
        self.connect((self.analog_sig_source_x_1_0, 0), (self.blocks_add_xx_0_0, 0))
        self.connect((self.audio_source_0, 0), (self.band_pass_filter_mic1, 0))
        self.connect((self.audio_source_0, 0), (self.band_pass_filter_mic2, 0))
        self.connect((self.audio_source_0, 0), (self.band_pass_filter_mic3, 0))
        self.connect((self.audio_source_0, 0), (self.band_pass_filter_mic4, 0))
        self.connect((self.audio_source_0, 0), (self.blocks_multiply_const_vxx_2_0_0, 0))
        self.connect((self.band_pass_filter_0, 0), (self.analog_nbfm_rx_0, 0))
        self.connect((self.band_pass_filter_0, 0), (self.blocks_complex_to_mag_0, 0))
        self.connect((self.band_pass_filter_0, 0), (self.blocks_complex_to_real_0, 0))
        self.connect((self.band_pass_filter_0_0, 0), (self.blocks_multiply_const_vxx_4, 0))
        self.connect((self.band_pass_filter_1, 0), (self.blocks_add_xx_0_0, 1))
        self.connect((self.band_pass_filter_mic1, 0), (self.blocks_add_xx_0_0_0, 3))
        self.connect((self.band_pass_filter_mic2, 0), (self.blocks_add_xx_0_0_0, 2))
        self.connect((self.band_pass_filter_mic3, 0), (self.blocks_add_xx_0_0_0, 1))
        self.connect((self.band_pass_filter_mic4, 0), (self.blocks_add_xx_0_0_0, 0))
        self.connect((self.blocks_add_const_vxx_0_0, 0), (self.analog_rail_ff_0_0, 0))
        self.connect((self.blocks_add_xx_0, 0), (self.analog_rail_ff_0, 0))
        self.connect((self.blocks_add_xx_0_0, 0), (self.analog_nbfm_tx_0, 0))
        self.connect((self.blocks_add_xx_0_0_0, 0), (self.blocks_multiply_const_vxx_2_2, 0))
        self.connect((self.blocks_add_xx_1, 0), (self.blocks_multiply_const_vxx_1, 0))
        self.connect((self.blocks_add_xx_1_0, 0), (self.analog_agc2_xx_0, 0))
        self.connect((self.blocks_add_xx_1_0_0, 0), (self.blocks_multiply_const_vxx_0, 0))
        self.connect((self.blocks_add_xx_1_0_0, 0), (self.blocks_multiply_const_vxx_0_0, 0))
        self.connect((self.blocks_add_xx_2, 0), (self.blocks_mute_xx_0_0_0, 0))
        self.connect((self.blocks_complex_to_mag_0, 0), (self.blocks_multiply_const_vxx_2_1, 0))
        self.connect((self.blocks_complex_to_real_0, 0), (self.blocks_multiply_const_vxx_2, 0))
        self.connect((self.blocks_float_to_complex_0_0, 0), (self.blocks_multiply_xx_0, 0))
        self.connect((self.blocks_keep_one_in_n_0, 0), (self.logpwrfft_x_0, 0))
        self.connect((self.blocks_keep_one_in_n_0_0, 0), (self.logpwrfft_x_0_0, 0))
        self.connect((self.blocks_multiply_const_vxx_0, 0), (self.blocks_add_const_vxx_0_0, 0))
        self.connect((self.blocks_multiply_const_vxx_0_0, 0), (self.blocks_add_xx_0, 1))
        self.connect((self.blocks_multiply_const_vxx_1, 0), (self.low_pass_filter_0, 0))
        self.connect((self.blocks_multiply_const_vxx_2, 0), (self.blocks_add_xx_1_0, 0))
        self.connect((self.blocks_multiply_const_vxx_2_0, 0), (self.blocks_add_xx_1, 1))
        self.connect((self.blocks_multiply_const_vxx_2_0_0, 0), (self.blocks_add_xx_1_0_0, 0))
        self.connect((self.blocks_multiply_const_vxx_2_1, 0), (self.blocks_add_xx_1_0, 1))
        self.connect((self.blocks_multiply_const_vxx_2_1_0, 0), (self.blocks_add_xx_1, 0))
        self.connect((self.blocks_multiply_const_vxx_2_2, 0), (self.blocks_add_xx_1_0_0, 1))
        self.connect((self.blocks_multiply_const_vxx_3, 0), (self.blocks_add_xx_2, 0))
        self.connect((self.blocks_multiply_const_vxx_4, 0), (self.blocks_add_xx_2, 1))
        self.connect((self.blocks_multiply_xx_0, 0), (self.band_pass_filter_0_0, 0))
        self.connect((self.blocks_mute_xx_0_0_0, 0), (self.blocks_keep_one_in_n_0_0, 0))
        self.connect((self.blocks_mute_xx_0_0_0, 0), (self.rational_resampler_xxx_0, 0))
        self.connect((self.blocks_vector_to_stream_0, 0), (self.network_udp_sink_0, 0))
        self.connect((self.blocks_vector_to_stream_0_0, 0), (self.network_udp_sink_1, 0))
        self.connect((self.freq_xlating_fir_filter_xxx_0, 0), (self.band_pass_filter_0, 0))
        self.connect((self.freq_xlating_fir_filter_xxx_0, 0), (self.blocks_keep_one_in_n_0, 0))
        self.connect((self.logpwrfft_x_0, 0), (self.blocks_vector_to_stream_0, 0))
        self.connect((self.logpwrfft_x_0_0, 0), (self.blocks_vector_to_stream_0_0, 0))
        self.connect((self.low_pass_filter_0, 0), (self.audio_sink_0, 0))
        self.connect((self.osmosdr_source_0, 0), (self.freq_xlating_fir_filter_xxx_0, 0))
        self.connect((self.rational_resampler_xxx_0, 0), (self.osmosdr_sink_0, 0))


    def get_Tx_Mode(self):
        return self.Tx_Mode

    def set_Tx_Mode(self, Tx_Mode):
        self.Tx_Mode = Tx_Mode
        self.blocks_add_const_vxx_0_0.set_k(((0.5 * int(self.Tx_Mode==5)) + int(self.Tx_Mode==2) +int(self.Tx_Mode==3)))
        self.blocks_multiply_const_vxx_0.set_k(((self.MicGain)*(int(self.Tx_Mode==0)) + (self.MicGain)*(int(self.Tx_Mode==1)) + (self.AMMIC/5.0)*(int(self.Tx_Mode==5)) ))
        self.blocks_multiply_const_vxx_2_0_0.set_k((self.Tx_Mode==4) or (self.Tx_Mode==5))
        self.blocks_multiply_const_vxx_2_2.set_k(self.Tx_Mode<4)
        self.blocks_multiply_const_vxx_3.set_k(self.Tx_Mode==4)
        self.blocks_multiply_const_vxx_4.set_k((self.Tx_Mode < 4) or (self.Tx_Mode==5))
        self.blocks_mute_xx_0_0_0.set_mute(bool((not self.PTT) or (self.Tx_Mode==2 and not self.KEY) or (self.Tx_Mode==3 and not self.KEY)))

    def get_Tx_LO(self):
        return self.Tx_LO

    def set_Tx_LO(self, Tx_LO):
        self.Tx_LO = Tx_LO
        self.osmosdr_sink_0.set_center_freq(self.Tx_LO, 0)

    def get_Tx_Gain(self):
        return self.Tx_Gain

    def set_Tx_Gain(self, Tx_Gain):
        self.Tx_Gain = Tx_Gain
        self.osmosdr_sink_0.set_gain(self.Tx_Gain, 0)

    def get_Tx_Filt_Low(self):
        return self.Tx_Filt_Low

    def set_Tx_Filt_Low(self, Tx_Filt_Low):
        self.Tx_Filt_Low = Tx_Filt_Low
        self.band_pass_filter_0_0.set_taps(firdes.complex_band_pass(1, 48000, self.Tx_Filt_Low, self.Tx_Filt_High, 100, window.WIN_HAMMING, 6.76))

    def get_Tx_Filt_High(self):
        return self.Tx_Filt_High

    def set_Tx_Filt_High(self, Tx_Filt_High):
        self.Tx_Filt_High = Tx_Filt_High
        self.band_pass_filter_0_0.set_taps(firdes.complex_band_pass(1, 48000, self.Tx_Filt_Low, self.Tx_Filt_High, 100, window.WIN_HAMMING, 6.76))

    def get_ToneBurst(self):
        return self.ToneBurst

    def set_ToneBurst(self, ToneBurst):
        self.ToneBurst = ToneBurst
        self.analog_sig_source_x_1.set_amplitude((1.0*self.ToneBurst))

    def get_SSB_G_EQ_M2(self):
        return self.SSB_G_EQ_M2

    def set_SSB_G_EQ_M2(self, SSB_G_EQ_M2):
        self.SSB_G_EQ_M2 = SSB_G_EQ_M2
        self.band_pass_filter_mic3.set_taps(firdes.band_pass((self.SSB_G_EQ_M2/10), 48000, 1450, 2350, 100, window.WIN_HAMMING, 6.76))

    def get_SSB_G_EQ_M1(self):
        return self.SSB_G_EQ_M1

    def set_SSB_G_EQ_M1(self, SSB_G_EQ_M1):
        self.SSB_G_EQ_M1 = SSB_G_EQ_M1
        self.band_pass_filter_mic2.set_taps(firdes.band_pass((self.SSB_G_EQ_M1/10), 48000, 650, 1450, 100, window.WIN_HAMMING, 6.76))

    def get_SSB_G_EQ_L(self):
        return self.SSB_G_EQ_L

    def set_SSB_G_EQ_L(self, SSB_G_EQ_L):
        self.SSB_G_EQ_L = SSB_G_EQ_L
        self.band_pass_filter_mic1.set_taps(firdes.band_pass((self.SSB_G_EQ_L/10), 48000, 100, 750, 100, window.WIN_HAMMING, 6.76))

    def get_SSB_G_EQ_H(self):
        return self.SSB_G_EQ_H

    def set_SSB_G_EQ_H(self, SSB_G_EQ_H):
        self.SSB_G_EQ_H = SSB_G_EQ_H
        self.band_pass_filter_mic4.set_taps(firdes.band_pass((self.SSB_G_EQ_H/10), 48000, 2350, 3300, 100, window.WIN_HAMMING, 6.76))

    def get_Rx_Mute(self):
        return self.Rx_Mute

    def set_Rx_Mute(self, Rx_Mute):
        self.Rx_Mute = Rx_Mute
        self.blocks_multiply_const_vxx_1.set_k(((self.AFGain/100.0) *  (not self.Rx_Mute)))

    def get_Rx_Mode(self):
        return self.Rx_Mode

    def set_Rx_Mode(self, Rx_Mode):
        self.Rx_Mode = Rx_Mode
        self.blocks_multiply_const_vxx_2.set_k(self.Rx_Mode<4)
        self.blocks_multiply_const_vxx_2_0.set_k(((self.Rx_Mode==4) * 0.2))
        self.blocks_multiply_const_vxx_2_1.set_k(self.Rx_Mode==5)
        self.blocks_multiply_const_vxx_2_1_0.set_k((1.0 + (self.Rx_Mode==5)))

    def get_Rx_LO(self):
        return self.Rx_LO

    def set_Rx_LO(self, Rx_LO):
        self.Rx_LO = Rx_LO
        self.osmosdr_source_0.set_center_freq(self.Rx_LO, 0)

    def get_Rx_Gain(self):
        return self.Rx_Gain

    def set_Rx_Gain(self, Rx_Gain):
        self.Rx_Gain = Rx_Gain
        self.osmosdr_source_0.set_gain(self.Rx_Gain, 0)

    def get_Rx_Filt_Low(self):
        return self.Rx_Filt_Low

    def set_Rx_Filt_Low(self, Rx_Filt_Low):
        self.Rx_Filt_Low = Rx_Filt_Low
        self.band_pass_filter_0.set_taps(firdes.complex_band_pass(1, 48000, self.Rx_Filt_Low, self.Rx_Filt_High, 100, window.WIN_HAMMING, 6.76))

    def get_Rx_Filt_High(self):
        return self.Rx_Filt_High

    def set_Rx_Filt_High(self, Rx_Filt_High):
        self.Rx_Filt_High = Rx_Filt_High
        self.band_pass_filter_0.set_taps(firdes.complex_band_pass(1, 48000, self.Rx_Filt_Low, self.Rx_Filt_High, 100, window.WIN_HAMMING, 6.76))

    def get_RxOffset(self):
        return self.RxOffset

    def set_RxOffset(self, RxOffset):
        self.RxOffset = RxOffset
        self.freq_xlating_fir_filter_xxx_0.set_center_freq(self.RxOffset)

    def get_RATE(self):
        return self.RATE

    def set_RATE(self, RATE):
        self.RATE = RATE

    def get_PTT(self):
        return self.PTT

    def set_PTT(self, PTT):
        self.PTT = PTT
        self.blocks_mute_xx_0_0_0.set_mute(bool((not self.PTT) or (self.Tx_Mode==2 and not self.KEY) or (self.Tx_Mode==3 and not self.KEY)))

    def get_MicGain(self):
        return self.MicGain

    def set_MicGain(self, MicGain):
        self.MicGain = MicGain
        self.blocks_multiply_const_vxx_0.set_k(((self.MicGain)*(int(self.Tx_Mode==0)) + (self.MicGain)*(int(self.Tx_Mode==1)) + (self.AMMIC/5.0)*(int(self.Tx_Mode==5)) ))

    def get_KEY(self):
        return self.KEY

    def set_KEY(self, KEY):
        self.KEY = KEY
        self.blocks_mute_xx_0_0_0.set_mute(bool((not self.PTT) or (self.Tx_Mode==2 and not self.KEY) or (self.Tx_Mode==3 and not self.KEY)))

    def get_FMMIC(self):
        return self.FMMIC

    def set_FMMIC(self, FMMIC):
        self.FMMIC = FMMIC
        self.blocks_multiply_const_vxx_0_0.set_k((self.FMMIC/5.0))

    def get_FFT_SEL(self):
        return self.FFT_SEL

    def set_FFT_SEL(self, FFT_SEL):
        self.FFT_SEL = FFT_SEL
        self.blocks_keep_one_in_n_0.set_n((2 ** self.FFT_SEL))
        self.blocks_keep_one_in_n_0_0.set_n((2 ** self.FFT_SEL))
        self.logpwrfft_x_0.set_sample_rate((48000/ (2 ** self.FFT_SEL)))
        self.logpwrfft_x_0_0.set_sample_rate((48000/ (2** self.FFT_SEL)))

    def get_CTCSS(self):
        return self.CTCSS

    def set_CTCSS(self, CTCSS):
        self.CTCSS = CTCSS
        self.analog_sig_source_x_1_0.set_frequency((self.CTCSS/10.0))
        self.analog_sig_source_x_1_0.set_amplitude((0.15 * (self.CTCSS >0)))

    def get_AMMIC(self):
        return self.AMMIC

    def set_AMMIC(self, AMMIC):
        self.AMMIC = AMMIC
        self.blocks_multiply_const_vxx_0.set_k(((self.MicGain)*(int(self.Tx_Mode==0)) + (self.MicGain)*(int(self.Tx_Mode==1)) + (self.AMMIC/5.0)*(int(self.Tx_Mode==5)) ))

    def get_AGC_DECAY(self):
        return self.AGC_DECAY

    def set_AGC_DECAY(self, AGC_DECAY):
        self.AGC_DECAY = AGC_DECAY
        self.analog_agc2_xx_0.set_decay_rate((self.AGC_DECAY/10000))

    def get_AFGain(self):
        return self.AFGain

    def set_AFGain(self, AFGain):
        self.AFGain = AFGain
        self.blocks_multiply_const_vxx_1.set_k(((self.AFGain/100.0) *  (not self.Rx_Mute)))




def docommands(tb):
  try:
    os.mkfifo("/tmp/langstoneTRx")
  except OSError as oe:
    if oe.errno != errno.EEXIST:
      raise    
  ex=False
  lastbase=0
  while not ex:
    fifoin=open("/tmp/langstoneTRx",'r')
    while True:
       try:
        with fifoin as filein:
         for line in filein:
           line=line.strip()
           if line[0]=='Q':
              ex=True                  
           if line[0]=='U':
              value=int(line[1:])
              tb.set_Rx_Mute(value)
           if line[0]=='H':
              value=int(line[1:])
              if value==1:   
                  tb.lock()
              if value==0:
                  tb.unlock() 
           if line[0]=='O':
              value=int(line[1:])
              tb.set_RxOffset(value)  
           if line[0]=='V':
              value=int(line[1:])
              tb.set_AFGain(value)
           if line[0]=='L':
              value=int(line[1:])
              tb.set_Rx_LO(value)
           if line[0]=='A':
              value=int(line[1:])
              tb.set_Rx_Gain(value) 
           if line[0]=='b':
              value=int(line[1:])
              tb.set_SSB_G_EQ_H(value)
           if line[0]=='p':
              value=int(line[1:])
              tb.set_SSB_G_EQ_M2(value)
           if line[0]=='P':
              value=int(line[1:])
              tb.set_SSB_G_EQ_M1(value)
           if line[0]=='e':
              value=int(line[1:])
              tb.set_SSB_G_EQ_L(value)
           if line[0]=='y':
              value=int(line[1:])
              tb.set_AGC_DECAY(value)              
           if line[0]=='F':
              value=int(line[1:])
              tb.set_Rx_Filt_High(value) 
           if line[0]=='I':
              value=int(line[1:])
              tb.set_Rx_Filt_Low(value) 
           if line[0]=='M':
              value=int(line[1:])
              tb.set_Rx_Mode(value) 
              tb.set_Tx_Mode(value)
           if line=='R':
              tb.set_PTT(False) 
           if line=='T':
              tb.set_PTT(True)
           if line[0]=='K':
              value=int(line[1:])
              tb.set_KEY(value) 
           if line[0]=='B':
              value=int(line[1:])
              tb.set_ToneBurst(value) 
           if line[0]=='G':
              value=int(line[1:])
              tb.set_MicGain(value) 
           if line[0]=='r':
              value=int(line[1:])
              tb.set_RATE(value)
           if line[0]=='g':
              value=int(line[1:])
              tb.set_FMMIC(value)
           if line[0]=='d':
              value=int(line[1:])
              tb.set_AMMIC(value)
           if line[0]=='f':
              value=int(line[1:])
              tb.set_Tx_Filt_High(value) 
           if line[0]=='i':
              value=int(line[1:])
              tb.set_Tx_Filt_Low(value)     
           if line[0]=='l':
              value=int(line[1:])
              tb.set_Tx_LO(value)  
           if line[0]=='a':
              value=int(line[1:])
              tb.set_Tx_Gain(value)     
           if line[0]=='C':
              value=int(line[1:])
              tb.set_CTCSS(value)   
           if line[0]=='W':
              value=int(line[1:])
              tb.set_FFT_SEL(value) 
             
       except:
         break


def main(top_block_cls=Lang_TRX_Lime, options=None):
    tb = top_block_cls()
    tb.start()
    docommands(tb)
    tb.stop()
    tb.wait()


if __name__ == '__main__':
    main()
