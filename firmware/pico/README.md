# Firmware for the RP2040 on the shield for Network Triggered IO


## Setup for compiling (e.g. on my MacOS)
To compile the RP2040 firmware, we needed to:
 - Download the gcc-arm cross compiler here - https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads - and tar -xf the file somewhere
  - Download the SDK: `git clone https://github.com/raspberrypi/pico-sdk.git; cd pico-sdk; git submodule update --init;`
 - Install cmake (`brew install cmake`)
 - (Optional) Download the examples: `git clone https://github.com/raspberrypi/pico-examples.git`
 - Set some environmental variables:
```
 1024  export PICO_SDK_PATH="$HOME/Code/Pico/pico-sdk"
 1106  export PICO_TOOLCHAIN_PATH=/Users/ckemere/Code/arm-gnu-toolchain-12.3.rel1-darwin-arm64-arm-none-eabi
 1107  export PATH=$PATH:/Users/ckemere/Code/arm-gnu-toolchain-12.3.rel1-darwin-arm64-arm-none-eabi/bin
```

 - Compile the sdk (and examples). `cd pico-sdk; mkdir build; cd build; cmake ..; make`

## Compiling the firmware

## Setup for programing a Pico using the debug interface on from a Raspberry Pi.
_from the [Getting Started Guide](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf)_. In order to install OpenOCD, we needed the following packages:
`automake autoconf build-essential texinfo libtool libftdi-dev libusb-1.0-0-dev pkg-config`. 

Then we ran:
```
git clone https://github.com/raspberrypi/openocd.git --branch rp2040 --recursive --depth=1
cd openocd
./bootstrap
./configure --enable-ftdi --enable-sysfsgpio --enable-bcm2835gpio
make -j4
sudo make install
```

We connected SWDIO (the left debug pin when the usb connector is facing north) to the Pi GPIO 24 (pin 18) and SWCLK to Pi GPIO 25 (pin 22), with the center ground pin connected via a different Pico ground pin. (Note that there's other suggested pins, GPIO 14 and 15, which should be connected to enable UART interface during debugging.)

After copying the `.elf` file over to the Pi, we finally, we have the command `openocd -f interface/raspberrypi-swd.cfg -f target/rp2040.cfg -c "program program.elf verify reset exit"`.

