# TrodesRippleDisruptionModule
A ripple disruption module for Trodes. 

**ALERT** - As of September 2024, we discovered that the legacy filter coefficients for the ripple band are bad. We're currently experimenting to decide on a new set of coefficients which optimze the frequency/latency tradeoff.

Interfaces with stimulation using Ethernet messages. A simple Ethernet-based stimulator is
the [pi-stimserver](https://github.com/kemerelab/pi-stimserver).

## Raspberry Pi Network-triggered GPIO
Some notes: https://codeandlife.com/2012/07/03/benchmarking-raspberry-pi-gpio-speed/
