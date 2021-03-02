#!/bin/bash


/user/twlostow/apps/usb-bootloader.py -p /dev/ttyUSB2 -b default -s 921600 -f fpga /user/twlostow/bitstreams/ertm14/current/ertm14_top.bin

#just to force a reboot
/user/twlostow/apps/usb-bootloader.py -p /dev/ttyUSB1 -b ertm14m0 -s 115200 -t -f mcu /user/twlostow/bitstreams/ertm14/current/mmc14.bin

