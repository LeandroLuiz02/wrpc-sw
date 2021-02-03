/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __NETCONSOLE_H__
#define __NETCONSOLE_H__

void netconsole_init(void);
int netconsole_poll(void);

int netconsole_read_byte(void);
int netconsole_write_string(const char *s);

#endif /* __NETCONSOLE_H__ */
