#!/bin/sh

../tools/uart-bootloader/usb-bootloader.py \
	-p /dev/ttyUSB0 \
	-b ertm14m1 \
	-s 115200 \
	-f mcu \
		/user/twlostow/bitstreams/ertm14/current/mmc15_main.bin
	
