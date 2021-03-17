#!/bin/sh

../tools/uart-bootloader/usb-bootloader.py \
	-p /dev/ttyUSB1 \
	-b ertm14m0 \
	-s 115200 \
	-f mcu \
		/user/twlostow/bitstreams/ertm14/current/mmc14_main.bin
