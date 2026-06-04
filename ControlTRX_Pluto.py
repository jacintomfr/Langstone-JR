
##################################################
# Piped Commands TRx Control Thread for Langstone V3H — PlutoSDR Version
# Author: G4EML / Adapted by CU2ED for Langstone-JR V3H
#
# NOTE: This file is for REFERENCE only.
# The docommands() function and main() replacement are already
# integrated into Lang_TRX_Pluto.py — do NOT insert manually.
#
# AGC: agc2_ff (post-demodulation, floating-point)
# PlutoSDR IP: hardcoded as ip:pluto.local in Lang_TRX_Pluto.py
##################################################

####################################################
# FIFO command reference (sent by LangstoneGUI_Pluto)
####################################################
#
# Q          — Quit flowgraph
# U<0|1>     — Rx Mute
# H<0|1>     — Lock/Unlock flowgraph
# O<hz>      — Rx frequency offset
# V<val>     — AF Gain
# L<hz>      — Rx LO frequency
# A<gain>    — Rx Gain (0-64 dB)
# F<hz>      — Rx filter high
# I<hz>      — Rx filter low
# M<mode>    — Rx+Tx mode (0=USB,1=LSB,2=CW,3=CWN,4=FM,5=AM)
# R          — PTT off
# T          — PTT on
# K<0|1>     — CW key
# B<val>     — Tone burst
# G<gain>    — Mic gain
# g<gain>    — FM mic gain
# d<gain>    — AM mic gain
# f<hz>      — Tx filter high
# i<hz>      — Tx filter low
# l<hz>      — Tx LO frequency
# a<att>     — Tx attenuation
# C<val>     — CTCSS
# W<val>     — FFT selector
# Y<dB>      — AGC level (-20 to +20 dB)
# J<0-4>     — AGC mode (0=OFF,1=FAST,2=MED,3=SLOW,4=LONG)
#              agc2_ff params: FAST(0.2,0.01) MED(0.1,0.002)
#                              SLOW(0.05,0.001) LONG(0.02,0.0002)
#
##################################################
