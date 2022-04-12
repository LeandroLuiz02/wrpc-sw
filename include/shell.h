/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __SHELL_H
#define __SHELL_H

#include <stdint.h>

#define UI_SHELL_MODE 0
#define UI_GUI_MODE 1

#define SH_MAX_LINE_LEN 80
#define SH_MAX_ARGS 8

extern int wrc_ui_mode;
extern int wrc_stat_running;

/* refresh period for _gui_ and _stat_ commands */
extern int wrc_ui_refperiod;

/* internal "last", exported to shell command */
extern uint32_t wrc_stats_last;

void decode_ip(const char *str, unsigned char *ip);
char *format_ip(char *s, const unsigned char *ip);

struct wrc_shell_cmd {
	char *name;
	int (*exec) (const char *args[]);
};

/* Put the structures in their own section */
#define DEFINE_WRC_COMMAND(_name) \
	const struct wrc_shell_cmd __wrc_cmd_ ## _name

int shell_exec(const char *buf);
int shell_interactive(void);
extern int shell_is_interacting;

void shell_boot_script(void);
void shell_show_build_init(void);
void shell_register_command( const struct wrc_shell_cmd* cmd );
void shell_register_commands(void);
void shell_activate_ui_command( int (*callback)(void) );

#endif
