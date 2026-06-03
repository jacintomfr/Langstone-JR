#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# ══════════════════════════════════════════════════════════════════════════════
#  Langstone V3H — HackRF flowgraph  (Lang_TRX_Hack.py)
#  Hardware: HackRF One, GNU Radio 3.10, RPi5, 48kHz audio
#  AGC block: analog.agc2_ff  (floating-point, post-demodulation)
# ══════════════════════════════════════════════════════════════════════════════
#
#  AGC MODE — Button 5 (FREQ mode), cycles OFF→F→M→S→L→OFF
#  FIFO command: J<n>  (J0=OFF, J1=FAST, J2=MED, J3=SLOW, J4=LONG)
#  Handler: set_AGC_Params(attack, decay) → agc2_ff.set_attack_rate/set_decay_rate
#
#  agc2_ff uses LOOP GAIN semantics (NOT time-constant like agc3_cc)
#  Larger value = faster response. Formula: τ ≈ 1 / (rate × sample_rate)
#
#  To edit AGC mode parameters: search for "params = [" around line 699
#
#  Mode   attack   decay    τ_attack   τ_decay   Best for
#  ─────────────────────────────────────────────────────────
#  OFF    bypass   bypass   —          —          No AGC
#  FAST   0.2      0.01     0.1 ms     2 ms       CW, strong signals
#  MED    0.1      0.002    0.2 ms     10 ms      SSB fast QSO
#  SLOW   0.05     0.001    0.4 ms     21 ms      SSB normal
#  LONG   0.02     0.0002   1.0 ms     104 ms     SSB DX, slow fading
#
#  NOTE: agc2_ff max_gain=20 (26dB). Prevents saturation on noise/silence.
#        For more gain range increase max_gain (e.g. 50 = 34dB).
#
# ──────────────────────────────────────────────────────────────────────────────
#  AGC LEVEL — SET menu "AGC Adj=" scrolls -20dB to +20dB
#  FIFO command: Y<dB>  (e.g. Y0, Y+10, Y-6)
#  Handler: set_AGC_Level(level_db)
#  Method: ramps agc2_ff reference from current to target in 20 steps × 5ms
#          (100ms smooth transition — avoids audio dropout on level change)
#  Reference baseline = 0.3 (0dB). Range: 0.003 (-20dB) to 3.0 (+20dB).
#
# ──────────────────────────────────────────────────────────────────────────────
#  OTHER FIFO COMMANDS (for reference)
#  F<freq_hz>   Set RX frequency
#  G<gain>      Set RX gain (0-40 dB LNA)
#  V<vol>       Set AF volume (0-100)
#  W<sel>       Set FFT source select
#  Y<dB>        AGC level adjust (-20 to +20 dB)
#  J<n>         AGC mode (0=OFF 1=F 2=M 3=S 4=L)
# ══════════════════════════════════════════════════════════════════════════════

#
# SPDX-License-Identifier: GPL-3.0
#
# GNU Radio Python Flow Graph
# Title: Lang Trx Hack
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
from gnuradio import soapy
from gnuradio.fft import logpwrfft
import os
import errno



class Lang_TRX_Hack(gr.top_block):

    def __init__(self):
        gr.top_block.__init__(self, "Lang Trx Hack", catch_exceptions=True)

        ##################################################
        # Variables
        ##################################################
        self.Tx_Mode = Tx_Mode = 0
        self.Tx_LO = Tx_LO = 435100000
        self.Tx_Gain = Tx_Gain = 0
        self.Tx_Filt_Low = Tx_Filt_Low = 300
        self.Tx_Filt_High = Tx_Filt_High = 3000
        self.Tx_AMP = Tx_AMP = True
        self.ToneBurst = ToneBurst = False
        self.SSB_G_EQ_M2 = SSB_G_EQ_M2 = 40
        self.SSB_G_EQ_M1 = SSB_G_EQ_M1 = 20
        self.SSB_G_EQ_L = SSB_G_EQ_L = 1
        self.SSB_G_EQ_H = SSB_G_EQ_H = 52
        self.Rx_Mute = Rx_Mute = False
        self.Rx_Mode = Rx_Mode = 0
        self.Rx_LO = Rx_LO = 435100000
        self.Rx_Gain = Rx_Gain = 40
        self.Rx_Filt_Low = Rx_Filt_Low = 300
        self.Rx_Filt_High = Rx_Filt_High = 3000
        self.Rx_Base = Rx_Base = 62
        self.Rx_AMP = Rx_AMP = True
        self.RxOffset = RxOffset = 0
        self.RATE = RATE = 0
        self.PTT = PTT = False
        self.MicGain = MicGain = 5.0
        self.KEY = KEY = False
        self.FMMIC = FMMIC = 50
        self.FFT_SEL = FFT_SEL = 1
        self.CTCSS = CTCSS = 885
        self.AMMIC = AMMIC = 5
        self.AFGain = AFGain = 100

        ##################################################
        # Blocks
        ##################################################

        self.soapy_hackrf_source_0 = None
        dev = 'driver=hackrf'
        stream_args = ''
        tune_args = ['']
        settings = ['']

        self.soapy_hackrf_source_0 = soapy.source(dev, "fc32", 1, '',
                                  stream_args, tune_args, settings)
        self.soapy_hackrf_source_0.set_sample_rate(0, 8000000)
        self.soapy_hackrf_source_0.set_bandwidth(0, 0)
        self.soapy_hackrf_source_0.set_frequency(0, Rx_LO)
        self.soapy_hackrf_source_0.set_gain(0, 'AMP', Rx_AMP)
        self.soapy_hackrf_source_0.set_gain(0, 'LNA', min(max(Rx_Gain, 0.0), 40.0))
        self.soapy_hackrf_source_0.set_gain(0, 'VGA', min(max(Rx_Base, 0.0), 62.0))
        self.soapy_hackrf_sink_0 = None
        dev = 'driver=hackrf'
        stream_args = ''
        tune_args = ['']
        settings = ['']

        self.soapy_hackrf_sink_0 = soapy.sink(dev, "fc32", 1, '',
                                  stream_args, tune_args, settings)
        self.soapy_hackrf_sink_0.set_sample_rate(0, 8000000)
        self.soapy_hackrf_sink_0.set_bandwidth(0, 0)
        self.soapy_hackrf_sink_0.set_frequency(0, Tx_LO)
        self.soapy_hackrf_sink_0.set_gain(0, 'AMP', Tx_AMP)
        self.soapy_hackrf_sink_0.set_gain(0, 'VGA', min(max(Tx_Gain, 0.0), 47.0))
        self.rational_resampler_xxx_1 = filter.rational_resampler_ccc(
                interpolation=33,
                decimation=500,
                taps=[],
                fractional_bw=0)
        self.network_udp_sink_1 = network.udp_sink(gr.sizeof_float, 1, '127.0.0.1', 7474, 0, 2048, False)
        self.network_udp_sink_0 = network.udp_sink(gr.sizeof_float, 1, '127.0.0.1', 7373, 0, 2048, False)
        self.mmse_resampler_xx_1 = filter.mmse_resampler_ff(0, (480/481))
        self.mmse_resampler_xx_1.set_block_alias("48/49")
        self.mmse_resampler_xx_1.set_min_output_buffer(4096)
        self.mmse_resampler_xx_1.set_max_output_buffer(8192)
        self.mmse_resampler_xx_0 = filter.mmse_resampler_cc(0, (48/(8000 + 100*RATE)))
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
        self.blocks_selector_0 = blocks.selector(gr.sizeof_gr_complex*1,0,PTT)
        self.blocks_selector_0.set_enabled(True)
        self.blocks_null_sink_0 = blocks.null_sink(gr.sizeof_gr_complex*1)
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
        self.blocks_multiply_const_vxx_0 = blocks.multiply_const_ff(((MicGain)*(int(Tx_Mode==0)) + (MicGain)*(int(Tx_Mode==1)) + (AMMIC/10.0)*(int(Tx_Mode==5)) ))
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
                500,  # transition width 100→500Hz: ~5× fewer taps, ~3ms latency vs 16ms
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
        	max_dev=5000,
        	fh=(-1),
                )
        self.analog_nbfm_rx_0 = analog.nbfm_rx(
        	audio_rate=48000,
        	quad_rate=48000,
        	tau=(75e-6),
        	max_dev=5e3,
          )
        self.analog_const_source_x_0 = analog.sig_source_f(0, analog.GR_CONST_WAVE, 0, 0, 0)
        self.analog_agc2_xx_0 = analog.agc2_ff(0.1, 0.00002, (3e-1), 1.0)
        self.analog_agc2_xx_0.set_max_gain(20)
        # ── AVL — Audio Volume Limiter ────────────────────────────────────────
        # Second agc2_ff used as a hard limiter — last stage before audio_sink
        # attack very fast (~0.04ms) to catch peaks before they clip
        # decay very slow (~2s) so it doesn't breathe audibly
        # max_gain=1.0 — can ONLY reduce, never amplify
        # reference=0.85 — 85% fullscale, safe headroom
        self.analog_avl_0 = analog.agc2_ff(0.5, 0.00001, 0.85, 1.0)
        self.analog_avl_0.set_max_gain(1.0)
        # Power probe for noise gate — measures AGC output level
        self.blocks_probe_signal_f_0 = blocks.probe_signal_f()


        ##################################################
        # Connections
        ##################################################
        self.connect((self.analog_agc2_xx_0, 0), (self.blocks_multiply_const_vxx_2_1_0, 0))
        self.connect((self.analog_agc2_xx_0, 0), (self.blocks_probe_signal_f_0, 0))
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
        self.connect((self.blocks_add_xx_1_0_0, 0), (self.mmse_resampler_xx_1, 0))
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
        self.connect((self.blocks_mute_xx_0_0_0, 0), (self.mmse_resampler_xx_0, 0))
        self.connect((self.blocks_selector_0, 0), (self.blocks_null_sink_0, 0))
        self.connect((self.blocks_selector_0, 1), (self.soapy_hackrf_sink_0, 0))
        self.connect((self.blocks_vector_to_stream_0, 0), (self.network_udp_sink_0, 0))
        self.connect((self.blocks_vector_to_stream_0_0, 0), (self.network_udp_sink_1, 0))
        self.connect((self.freq_xlating_fir_filter_xxx_0, 0), (self.band_pass_filter_0, 0))
        self.connect((self.freq_xlating_fir_filter_xxx_0, 0), (self.blocks_keep_one_in_n_0, 0))
        self.connect((self.logpwrfft_x_0, 0), (self.blocks_vector_to_stream_0, 0))
        self.connect((self.logpwrfft_x_0_0, 0), (self.blocks_vector_to_stream_0_0, 0))
        self.connect((self.low_pass_filter_0, 0), (self.analog_avl_0, 0))
        self.connect((self.analog_avl_0, 0), (self.audio_sink_0, 0))
        self.connect((self.mmse_resampler_xx_0, 0), (self.blocks_selector_0, 0))
        self.connect((self.mmse_resampler_xx_1, 0), (self.blocks_multiply_const_vxx_0, 0))
        self.connect((self.mmse_resampler_xx_1, 0), (self.blocks_multiply_const_vxx_0_0, 0))
        self.connect((self.rational_resampler_xxx_1, 0), (self.freq_xlating_fir_filter_xxx_0, 0))
        self.connect((self.soapy_hackrf_source_0, 0), (self.rational_resampler_xxx_1, 0))


    def get_Tx_Mode(self):
        return self.Tx_Mode

    def set_Tx_Mode(self, Tx_Mode):
        self.Tx_Mode = Tx_Mode
        self.blocks_add_const_vxx_0_0.set_k(((0.5 * int(self.Tx_Mode==5)) + int(self.Tx_Mode==2) +int(self.Tx_Mode==3)))
        self.blocks_multiply_const_vxx_0.set_k(((self.MicGain)*(int(self.Tx_Mode==0)) + (self.MicGain)*(int(self.Tx_Mode==1)) + (self.AMMIC/10.0)*(int(self.Tx_Mode==5)) ))
        self.blocks_multiply_const_vxx_2_0_0.set_k((self.Tx_Mode==4) or (self.Tx_Mode==5))
        self.blocks_multiply_const_vxx_2_2.set_k(self.Tx_Mode<4)
        self.blocks_multiply_const_vxx_3.set_k(self.Tx_Mode==4)
        self.blocks_multiply_const_vxx_4.set_k((self.Tx_Mode < 4) or (self.Tx_Mode==5))
        self.blocks_mute_xx_0_0_0.set_mute(bool((not self.PTT) or (self.Tx_Mode==2 and not self.KEY) or (self.Tx_Mode==3 and not self.KEY)))

    def get_Tx_LO(self):
        return self.Tx_LO

    def set_Tx_LO(self, Tx_LO):
        self.Tx_LO = Tx_LO
        self.soapy_hackrf_sink_0.set_frequency(0, self.Tx_LO)

    def get_Tx_Gain(self):
        return self.Tx_Gain

    def set_Tx_Gain(self, Tx_Gain):
        self.Tx_Gain = Tx_Gain
        self.soapy_hackrf_sink_0.set_gain(0, 'VGA', min(max(self.Tx_Gain, 0.0), 47.0))

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

    def get_Tx_AMP(self):
        return self.Tx_AMP

    def set_Tx_AMP(self, Tx_AMP):
        self.Tx_AMP = Tx_AMP
        self.soapy_hackrf_sink_0.set_gain(0, 'AMP', self.Tx_AMP)

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
        self.soapy_hackrf_source_0.set_frequency(0, self.Rx_LO)

    def get_Rx_Gain(self):
        return self.Rx_Gain

    def set_Rx_Gain(self, Rx_Gain):
        self.Rx_Gain = Rx_Gain
        self.soapy_hackrf_source_0.set_gain(0, 'LNA', min(max(Rx_Gain, 0.0), 40.0))

    def set_AGC_Freeze(self, freeze):
        # Disabled — noise gate caused audio dropout on filter/AGC adjustments
        pass

    def agc_hold(self, duration_ms=200):
        # Freeze AGC gain during filter/setting changes to prevent dropout
        import threading, time
        current_gain  = self.analog_agc2_xx_0.get_gain()
        normal_decay  = getattr(self, '_agc_normal_decay', 0.00002)
        normal_attack = getattr(self, '_agc_normal_attack', 0.1)
        def _hold():
            self.analog_agc2_xx_0.set_decay_rate(0.000001)
            self.analog_agc2_xx_0.set_attack_rate(0.000001)
            self.analog_agc2_xx_0.set_gain(current_gain)
            time.sleep(duration_ms / 1000.0)
            self.analog_agc2_xx_0.set_attack_rate(normal_attack)
            self.analog_agc2_xx_0.set_decay_rate(normal_decay)
        threading.Thread(target=_hold, daemon=True).start()

    def set_AGC_Level(self, level_db):
        # AGC level: ramp reference gradually to avoid gain jump transient
        # agc2_ff set_reference() works but causes glitch if stepped directly
        # Solution: ramp in 20 steps of 5ms each = 100ms total transition
        if level_db < -20: level_db = -20
        if level_db >  20: level_db =  20
        self._agc_level_db = level_db
        import math, threading, time
        target_ref = 0.3 * (10.0 ** (level_db / 20.0))
        current_ref = getattr(self, '_current_agc_ref', 0.3)
        steps = 20
        def _ramp():
            for i in range(1, steps+1):
                r = current_ref + (target_ref - current_ref) * i / steps
                self.analog_agc2_xx_0.set_reference(r)
                self.analog_agc2_xx_0.set_gain(r)
                time.sleep(0.005)  # 5ms per step = 100ms total
            self._current_agc_ref = target_ref
        threading.Thread(target=_ramp, daemon=True).start()

    def set_AGC_Params(self, attack, decay):
        # AGC2 mode control (J command)
        # agc2_ff uses LOOP GAIN semantics (not 1/τ like agc3_cc)
        # Correct scale: 0.02-0.2 for attack, 0.0002-0.01 for decay
        if attack == 0 and decay == 0:
            # OFF: unity gain — AGC loop frozen at gain=1.0, no amplitude change
            self.analog_agc2_xx_0.set_max_gain(1.0)
            self.analog_agc2_xx_0.set_gain(1.0)
            self.analog_agc2_xx_0.set_reference(1.0)
        else:
            self._agc_normal_attack = attack
            self._agc_normal_decay  = decay
            self.analog_agc2_xx_0.set_max_gain(20)
            self.analog_agc2_xx_0.set_reference(0.3)
            self.analog_agc2_xx_0.set_attack_rate(attack)
            self.analog_agc2_xx_0.set_decay_rate(decay)

    def get_Rx_Filt_Low(self):
        return self.Rx_Filt_Low

    def set_Rx_Filt_Low(self, Rx_Filt_Low):
        self.agc_hold(250)  # freeze AGC during filter retap
        self.Rx_Filt_Low = Rx_Filt_Low
        self.band_pass_filter_0.set_taps(firdes.complex_band_pass(1, 48000, self.Rx_Filt_Low, self.Rx_Filt_High, 500, window.WIN_HAMMING, 6.76))

    def get_Rx_Filt_High(self):
        return self.Rx_Filt_High

    def set_Rx_Filt_High(self, Rx_Filt_High):
        self.agc_hold(250)  # freeze AGC during filter retap — prevents dropout
        self.Rx_Filt_High = Rx_Filt_High
        self.band_pass_filter_0.set_taps(firdes.complex_band_pass(1, 48000, self.Rx_Filt_Low, self.Rx_Filt_High, 500, window.WIN_HAMMING, 6.76))
        # Also update post-demod low_pass_filter_0 — hardcoded 3kHz was cutting AM highs
        lp_cutoff = abs(self.Rx_Filt_High)
        if lp_cutoff < 3000:  lp_cutoff = 3000
        if lp_cutoff > 10000: lp_cutoff = 10000
        self.low_pass_filter_0.set_taps(firdes.low_pass(1, 48000, lp_cutoff, 500, window.WIN_HAMMING, 6.76))

    def get_Rx_Base(self):
        return self.Rx_Base

    def set_Rx_Base(self, Rx_Base):
        self.Rx_Base = Rx_Base
        self.soapy_hackrf_source_0.set_gain(0, 'VGA', min(max(self.Rx_Base, 0.0), 62.0))

    def get_Rx_AMP(self):
        return self.Rx_AMP

    def set_Rx_AMP(self, Rx_AMP):
        self.Rx_AMP = Rx_AMP
        self.soapy_hackrf_source_0.set_gain(0, 'AMP', self.Rx_AMP)

    def get_RxOffset(self):
        return self.RxOffset

    def set_RxOffset(self, RxOffset):
        self.RxOffset = RxOffset
        self.freq_xlating_fir_filter_xxx_0.set_center_freq(self.RxOffset)

    def get_RATE(self):
        return self.RATE

    def set_RATE(self, RATE):
        self.RATE = RATE
        self.mmse_resampler_xx_0.set_resamp_ratio((48/(8000 + 100*self.RATE)))

    def get_PTT(self):
        return self.PTT

    def set_PTT(self, PTT):
        self.PTT = PTT
        self.blocks_mute_xx_0_0_0.set_mute(bool((not self.PTT) or (self.Tx_Mode==2 and not self.KEY) or (self.Tx_Mode==3 and not self.KEY)))
        self.blocks_selector_0.set_output_index(self.PTT)

    def get_MicGain(self):
        return self.MicGain

    def set_MicGain(self, MicGain):
        self.MicGain = MicGain
        self.blocks_multiply_const_vxx_0.set_k(((self.MicGain)*(int(self.Tx_Mode==0)) + (self.MicGain)*(int(self.Tx_Mode==1)) + (self.AMMIC/10.0)*(int(self.Tx_Mode==5)) ))

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
        self.blocks_multiply_const_vxx_0.set_k(((self.MicGain)*(int(self.Tx_Mode==0)) + (self.MicGain)*(int(self.Tx_Mode==1)) + (self.AMMIC/10.0)*(int(self.Tx_Mode==5)) ))

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
              tb.set_Rx_Base(value)
           if line[0]=='n':
              value=int(line[1:])
              tb.set_SSB_G_EQ_H(value)
           if line[0]=='N':
              value=int(line[1:])
              tb.set_SSB_G_EQ_M2(value)
           if line[0]=='z':
              value=int(line[1:])
              tb.set_SSB_G_EQ_M1(value)
           if line[0]=='Z':
              value=int(line[1:])
              tb.set_SSB_G_EQ_L(value)       
           if line[0]=='p':
              value=int(line[1:])
              tb.set_Rx_AMP(value)   
           if line[0]=='P':
              value=int(line[1:])
              tb.set_Tx_AMP(value)           
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
           if line[0]=='~':
              # Noise gate: ~1=enable ~0=disable
              # Freezes AGC decay during silence — prevents gain pumping
              import threading, time
              value = int(line[1:])
              tb._ng_run = (value == 1)
              if value == 1:
                def _noise_gate():
                    THRESH = 0.008   # power threshold (below = silence)
                    HOLD   = 4       # ×50ms = 200ms hold before freeze
                    hold   = 0
                    frozen = False
                    while tb._ng_run:
                        time.sleep(0.05)
                        try:
                            pwr = abs(tb.blocks_probe_signal_f_0.level())
                            if pwr < THRESH:
                                hold = min(hold+1, HOLD+1)
                                if hold > HOLD and not frozen:
                                    tb.set_AGC_Freeze(True)
                                    frozen = True
                            else:
                                hold = 0
                                if frozen:
                                    tb.set_AGC_Freeze(False)
                                    frozen = False
                        except: pass
                    tb.set_AGC_Freeze(False)  # restore on disable
                threading.Thread(target=_noise_gate, daemon=True).start()
              else:
                tb.set_AGC_Freeze(False)
           if line[0]=='Y':
              # AGC level adjust: Y<dB> e.g. Y0, Y-10, Y+10
              value=int(line[1:])
              tb.set_AGC_Level(value)
              value=int(line[1:])
              # agc2_ff loop gain params (NOT agc3 rate params)
              params = [
                (0,     0      ),  # J0 OFF  — bypass
                (0.2,   0.01   ),  # J1 FAST — τ_attack=0.1ms, τ_decay=2ms
                (0.1,   0.002  ),  # J2 MED  — τ_attack=0.2ms, τ_decay=10ms
                (0.05,  0.001  ),  # J3 SLOW — τ_attack=0.4ms, τ_decay=21ms
                (0.02,  0.0002 ),  # J4 LONG — τ_attack=1.0ms, τ_decay=104ms
              ]
              if 0 <= value < len(params):
                tb.set_AGC_Params(*params[value])

                                                                                
       except:
         break

def main(top_block_cls=Lang_TRX_Hack, options=None):
    tb = top_block_cls()
    tb.start()
    docommands(tb)
    tb.stop()
    tb.wait()


if __name__ == '__main__':
    main()
