

# Setting up the Raspberry Pi

These instructions are adapted from [here](https://ubuntu.com/tutorials/how-to-install-ubuntu-on-your-raspberry-pi).

## I. Creating a new boot disk.

Using the Raspberry Pi Imager software, here are the steps:

  1. (This has been tested using Ubuntu Server 22.04. It probably will work for others.) Click on `Choose OS`, 
     then `Other general-purpose OS`, then `Ubuntu`, then `Ubuntu Server 22.04.03 LTS (64 bit)`.
  2. Click on `Choose Storage` and select the micro-SD card you have inserted into your computer (e.g., using a
     USB adaptor).
  3. **Important!** Don't click on `Write` yet!!!
  4. Click on the gear icon in the bottom right corner. In MacOS, it may ask if you want to prefill WiFi settings. 
     You can click `No`.
  5. Click the checkbox next to `Enable SSH`.
  6. Make sure the checkbox next to `Set username and password` is checked. Fill in the username and password 
     you will use.
  7. For our institutional WiFi, we need to configure it after the disk is created, so uncheck the box next to 
     `Configure Wireless LAN`.
  8. Check the box next to `Set locale settings` and choose your timezone and keyboard layout. (For our group, 
     it is `America/Chicago` and `us`.)
  9. Click on `Save`, and then `Write`, then `Yes` to erase media (assuming you've double checked you don't want
     whatever is on it anymore!).

## II. Setting up networking

The next steps are a bit complicated if you're using a Mac or Windows machine that can't mount the ext4 partitions
that are used by Linux (and are now on the formatted MicroSD card). I used Ubuntu running in Parallels for the next
step. You could also use a Linux desktop or install the appropriate software for your OS to let your computer read/write
the partitions. Alternatively, you can plug a monitor and keyboard into your Pi and do these steps after booting for the
first time.

Ubuntu Server uses "netplan" to configure networking. If you install a desktop version of Ubuntu or Raspbian,
you'll have a GUI to configure networking, but for this system, we want minimal processes running, and so the Server
version is preferred. To configure the networking, edit the file `/etc/netplan/50-cloud-init.yaml` file. Our goal is
to set up the hardwired Ethernet to be on a local network, unconnected to the broader Internet, and use the WiFi
for internet connectivity. Note that the comment in the file says that the configuration will not persist across 
instance reboots. This doesn't apply to the raspberry pi.


The file should thus look something like this when you're done:

```
# This file configures the network for a WiFi internet connection and a static Ethernet IP address.
# The static IP address 192.168.0.2/24 assumes that the Trodes server connecting to the Pi will have
# an address in the range of 192.168.0.xxx.

network:
  version: 2
  ethernets:
    eth0:
      dhcp4: no
      addresses: [192.168.0.2/24]
  wifis:
    wlan0:
      dhcp4: true
      access-points:
        "Rice Visitor": {}

```

Note that we've specified a particular example open network for the WiFi configuration which is relevant
to our specific scenario.

If you have edited this file after the first boot, you'll need to run the two commands: `sudo netplan generate`
and then `sudo netplan apply`.

You should now be able to access your Raspberry Pi on the local network using ssh - i.e., `ssh your-user-name@192.168.0.2`.
Once you log in, you should run `sudo apt update` and then `sudo apt upgrade` to update your system libraries.


## III. Installing the software

libbcm2835-dev
