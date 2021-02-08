/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright 2020-2021 CERN
 * Author: Juan David Gonzalez Cobas
 *
 * The comm via USB serial is implemented here
 */

#include <limits.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include <stdlib.h>
#include <stdio.h>

#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "libertm.h"

int init_serial_device(const char *serial_device)
{
	struct termios	ts;
	int i;

	int fd = open(serial_device, 0, O_RDWR);
	printf("open fd = %d for %s\n", fd, serial_device);

	tcgetattr(fd, &ts);
	printf("tty state: \n"
		"c_iflag: %08x\n"
		"c_oflag: %08x\n"
		"c_cflag: %08x\n"
		"c_lflag: %08x\n"
		"c_cc: ", ts.c_iflag, ts.c_oflag, ts.c_cflag, ts.c_lflag);
	for (i = 0; i < NCCS; i++)
		printf("%02x ", ts.c_cc[i]);
	printf("\n");

	return fd;
}

const char *const_serial = "/dev/ttyUSB2";

int main(int argc, char *argv[])
{
	int fd, len;
	char msg[8192];
	char *cfg = "ertm show-config\n";

	fd = init_serial_device(const_serial);
	len = write(fd, cfg, strlen(cfg)+1);
	printf("wrote %d bytes\n", len);
	printf("error was: %s\n", strerror(errno));
	len = read(fd, msg, sizeof(msg));
	msg[len] = '\0';
	printf("%s\n", msg);
	printf("read %d bytes\n", len);
	printf(ttyname(fd));
	return 0;;
}


