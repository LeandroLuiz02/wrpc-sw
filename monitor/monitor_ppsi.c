/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2021 CERN
 * Author: Adam Wujek
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#include <inttypes.h>
#include <wrc.h>
#include <dev/w1.h>
#include <ppsi/ppsi.h>
#include <wrpc.h>
#include <wr-api.h>
#include <dev/minic.h>
#include <softpll_ng.h>
#include <dev/syscon.h>
#include <dev/pps_gen.h>
#include <dev/onewire.h>
#include <dev/endpoint.h>
#include <dev/netif.h>
#include <temperature.h>
#include "wrc_ptp.h"
#include "hal_exports.h"
#include "lib/ipv4.h"
#include "shell.h"
#include "revision.h"

#ifndef CONFIG_PRINTF_FULL
#error ("WRPC monitor requires full version of pp_printf implementation")
#endif


#define WRC_MONITOR_REFRESH_PERIOD (1 * TICS_PER_SECOND)
#define WRC_DIAG_REFRESH_PERIOD (1 * TICS_PER_SECOND)

#define DESCRIPTION_MAIN 1
#define DESCRIPTION_SERVO 2
#define DESCRIPTION_WR_SERVO 4

/* protocol extension data */
#define IS_PROTO_EXT_INFO_AVAILABLE(proto_id) \
	((proto_id < sizeof(proto_ext_info) / sizeof(struct proto_ext_info_t)) \
			&& (proto_ext_info[proto_id].valid == 1))


/* internal "last", exported to shell command */
uint32_t wrc_stats_last;
extern struct pp_instance ppi_static;
extern struct pp_globals ppg_static;
extern struct pp_globals *ppg;
extern char wrc_hw_name[HW_NAME_LENGTH];
static int prev_gui_description = 0;
static int gui_description = 1;
static uint32_t next_update_ticks;

void print_main_description(void);
void print_main_data(void);
void print_servo_description(void);
void print_servo_data(struct pp_instance *ppi);
void print_aux_data(void);
int wrc_wr_diags(void);


struct proto_ext_info_t {
	int valid;
	char *ext_name; /* Extension name */
	char short_ext_name; /* Very short extension name - just one character */
	int servo_ext_size; /* Size of the extension */
	int ipc_cmd_tacking; /* Command to enable/disable servo tacking*/
	int track_onoff;     /* Tracking on/off */
	time_t lastt;
	int last_count;
};

static struct proto_ext_info_t proto_ext_info [] = {
		[PPSI_EXT_NONE] = {
				.valid = 1,
				.ext_name = "PTP",
				.short_ext_name = 'P',
				.ipc_cmd_tacking = -1, /* Invalid */
		},

#if CONFIG_HAS_EXT_WR
		[PPSI_EXT_WR] = {
				.valid = 1,
				.ext_name ="White-Rabbit",
				.short_ext_name ='W',
				.servo_ext_size = sizeof(struct wr_data),
// 				.ipc_cmd_tacking = PTPDEXP_COMMAND_WR_TRACKING,
				.track_onoff = 1,
		},
#endif
#if CONFIG_HAS_EXT_L1SYNC
		[PPSI_EXT_L1S] = {
				.valid = 1,
				.ext_name ="L1Sync",
				.short_ext_name ='L',
				.servo_ext_size = sizeof(struct l1e_data),
// 				.ipc_cmd_tacking = PTPDEXP_COMMAND_L1SYNC_TRACKING,
				.track_onoff = 1,
		},
#endif

};


/* define size of pp_instance_state_to_name as a last element + 1 */
#define PP_INSTANCE_STATE_MAX (sizeof(pp_instance_state_to_name) / sizeof(char *))

/* define conversion array for the field state in the struct pp_instance */
static char *pp_instance_state_to_name[] = {
	/* from ppsi/include/ppsi/ieee1588_types.h, enum pp_std_states */
	/* PPS_END_OF_TABLE = 0 */
	[PPS_END_OF_TABLE] =      "EOT       ",
	[PPS_INITIALIZING] =      "INITING   ",
	[PPS_FAULTY] =            "FAULTY    ",
	[PPS_DISABLED] =          "DISABLED  ",
	[PPS_LISTENING] =         "LISTENING ",
	[PPS_PRE_MASTER] =        "PRE_MASTER",
	[PPS_MASTER] =            "MASTER    ",
	[PPS_PASSIVE] =           "PASSIVE   ",
	[PPS_UNCALIBRATED] =      "UNCALIBRAT",
	[PPS_SLAVE] =             "SLAVE     ",
	NULL
	};

#define EMPTY_EXTENSION_STATE_NAME "          "

#if CONFIG_HAS_EXT_L1SYNC
static char * l1e_instance_extension_state[]={
		[__L1SYNC_MISSING   ] = "INVALID   ",
		[L1SYNC_DISABLED    ] = "DISABLED  ",
		[L1SYNC_IDLE        ] = "IDLE      ",
		[L1SYNC_LINK_ALIVE  ] = "LINK ALIVE",
		[L1SYNC_CONFIG_MATCH] = "CFG MATCH ",
		[L1SYNC_UP          ] = "UP        ",
		NULL
};
#define L1S_INSTANCE_EXTENSION_STATE_MAX (sizeof (l1e_instance_extension_state)/sizeof(char *))

#endif

static char * timing_mode_state[] = {
		[WRH_TM_GRAND_MASTER]=     "GM",
		[WRH_TM_FREE_MASTER]=      "FR",
		[WRH_TM_BOUNDARY_CLOCK]=   "BC",
		[WRH_TM_DISABLED]=         "--",
		NULL
	};

#if CONFIG_HAS_EXT_WR
static char * wr_instance_extension_state[]={
		[WRS_IDLE ]=              "IDLE      ",
		[WRS_PRESENT] =           "WR_PRESENT",
		[WRS_S_LOCK] =            "WR_S_LOCK ",
		[WRS_M_LOCK] =            "WR_M_LOCK ",
		[WRS_LOCKED] =            "WR_LOCKED ",
		[WRS_CALIBRATION] =       "WR_CAL-ION",
		[WRS_CALIBRATED] =        "WR_CAL-ED ",
		[WRS_RESP_CALIB_REQ] =    "WR_RSP_CAL",
		[WRS_WR_LINK_ON] =        "WR_LINK_ON",
		NULL
};
#define WR_INSTANCE_EXTENSION_STATE_MAX (sizeof (wr_instance_extension_state) / sizeof(char *))

#endif

static char *prot_detection_state_name[]={
		"NONE   ", /* No meaning. No extension present */
		"WA_MSG ", /* Waiting first message */
		"PD_IPRG", /* Protocol detection  */
		"EXT_ON ", /* Protocol detected */
		"EXT_OFF" /* Protocol not detected */
};

static struct desired_state_t{
	char *str_state;
	int state;
} 

desired_states[] = {
	{ "initializing", PPS_INITIALIZING},
	{ "faulty",       PPS_FAULTY},
	{ "disabled",     PPS_DISABLED},
	{ "listening",    PPS_LISTENING},
	{ "pre-master",   PPS_PRE_MASTER},
	{ "master",       PPS_MASTER},
	{ "passive",      PPS_PASSIVE},
	{ "uncalibrated", PPS_UNCALIBRATED},
	{ "slave",        PPS_SLAVE},
	{}
};

char * timeIntervalToString_ns_dot_ps(TimeInterval time, char *buf)
{

	int64_t nanos;
	uint32_t picos;
	char sign = ' ';

	if (time < 0 && time != INT64_MIN) {
		sign = '-';
		time = -time;
	}
	nanos = time >> TIME_INTERVAL_FRACBITS;
	picos = (((time & TIME_INTERVAL_FRACMASK) * 1000) + TIME_INTERVAL_ROUNDING_VALUE) >> TIME_INTERVAL_FRACBITS;
	sprintf(buf, "%c%Ld.%03d", sign, nanos, picos);

	return buf;
}

char * timeToString_ps_as_ns(struct pp_time *time, char *buf)
{
	char sign = '+';
	int64_t scaled_nsecs = time->scaled_nsecs;
	int64_t secs = time->secs, nanos, picos;

	if (!is_incorrect(time)) {
		if (scaled_nsecs < 0 || secs < 0) {
			sign = '-';
			scaled_nsecs = -scaled_nsecs;
			secs = -secs;
		}
		nanos = scaled_nsecs >> TIME_FRACBITS;
		picos = ((scaled_nsecs & TIME_FRACMASK) * 1000 + TIME_ROUNDING_VALUE)
				>> TIME_FRACBITS;
		if ((scaled_nsecs > 0 && secs < 0) 
		    || (scaled_nsecs < 0 && secs > 0)) {
			sprintf(buf, "!Wsign:s%cns%c", secs > 0 ? '+':'-', scaled_nsecs > 0 ? '+':'-');
			return buf;
		}
		sprintf(buf, "%c%Ld",
			sign, secs);
		sprintf(buf, "%s.%Ld",
			buf, nanos);
		sprintf(buf, "%s.%Ld",
			buf, picos);

	} else {
		sprintf(buf, "--Incorrect--");
	}
	return buf;
}

char * relativeDifferenceToString(RelativeDifference time, char *buf)
{
	char sign;
	int32_t nsecs;
	uint64_t sub_yocto = 0;
	int64_t fraction;
	uint64_t bitWeight = 500000000000000000;
	uint64_t mask;

	if (time < 0) {
		time =- time;
		sign = '-';
	} else {
		sign = '+';
	}

	nsecs = time >> REL_DIFF_FRACBITS;
	fraction = time & REL_DIFF_FRACMASK;
	for (mask = (uint64_t) 1 << (REL_DIFF_FRACBITS - 1); mask != 0; mask >>= 1) {
		if (mask & fraction)
			sub_yocto += bitWeight;
		bitWeight /= 2;
	}
	sprintf(buf,"%c%d.%018Ld", sign, nsecs, sub_yocto);
	return buf;
}

static inline int extensionStateColor(struct pp_instance *ppi)
{
	if (ppi->protocol_extension == PPSI_EXT_NONE) {
		return C_GREEN; /* No extension */
	}
	switch (ppi->extState) {
	case PP_EXSTATE_ACTIVE :
		return C_GREEN;
	case PP_EXSTATE_PTP :
		return C_WHITE;
	case PP_EXSTATE_DISABLE :
	default:
		return C_RED;
	}
}

static char *getStateAsString(char *p[], int index)
{
	int i, len;
	char *errMsg = "?????????????????????";

	len = strlen(p[0]);
	for (i = 0; ; i++) {
		if (p[i] == NULL)
			return errMsg + strlen(errMsg) - len;
		if (i == index)
			return p[index];
	}
}

static char *optimized_pp_time_toString_ps_as_ns(struct pp_time *pptime, char *buf)
{
	char lbuf[128];

	if (pptime->secs)
		sprintf(buf,"%s sec ", timeToString_ps_as_ns(pptime, lbuf));
	else
		sprintf(buf,"%s nsec", timeIntervalToString_ns_dot_ps(pp_time_to_interval(pptime), lbuf));
	return buf;
}

char * convert_ps_to_str_ns(char *buff, int64_t num)
{
	int i;
	int len;
	char *p;
	sprintf(buff, "% 05Ld", num);
	len = strlen(buff);
	p = buff + len;
	for (i = 0; i < 4; i++, p--) {/* move EOL and 3 chars */
		*(p + 1) = *(p);
	}
	*(p + 1) = '.';
	return buff;
}

int time(void *a)
{
    return 1;
}

void redraw_gui(void)
{
	prev_gui_description = 0;
	next_update_ticks = 0;
}

int wrc_mon_gui(void)
{
	static uint32_t last_servo_count;
	struct pp_servo *s = SRV(ppg->pp_instances);
	/* print new values only if time elapsed or servo's update_count
	 * increased */
	if (prev_gui_description != gui_description) {
		term_clear();
		if (gui_description & DESCRIPTION_MAIN) {
			print_main_description();
		}
		if (gui_description & DESCRIPTION_SERVO) {
			print_servo_description();
		}
		next_update_ticks = 0;
		term_clear_to_end();
		prev_gui_description = gui_description;
		return 1;
	}

	/* update on timeout or servo update */
	if (time_before(timer_get_tics(), next_update_ticks)
	    && last_servo_count == s->update_count)
		return 0;

	next_update_ticks = timer_get_tics() + WRC_MONITOR_REFRESH_PERIOD;
	last_servo_count = s->update_count;
	prev_gui_description = gui_description;
	gui_description = DESCRIPTION_MAIN;
	print_main_data();

	/* Print servo */
	/* FIXME: add support of multiple instances */
	print_servo_data(ppg->pp_instances);
	print_aux_data();
	/* clear colors */
	pp_printf("\e[m\n");
	/* clear till the end of a screen */
	term_clear_to_end();

	return 1;
}

void print_main_description(void)
{
	int i;
	int ndevs;

	pcprintf(1, 1, C_BLUE, "%s WR PTP Core Sync Monitor %s",
		wrc_hw_name, build_revision);
	cprintf(C_MAGENTA, "\nEsc or q = exit; r = redraw GUI");

	cprintf(C_BLUE, "\n\nTAI Time:%22sUTC offset:", "");

	pp_printf("\nPLL mode:%22sPLL state:\n", "");

	ndevs = netif_get_device_count();

	/*show_ports */
	cprintf(C_CYAN, "------+-------------------+-------------------------+---------+---------+-----\n");
	pp_printf(      "Iface |        MAC        |       IP (source)       |    RX   |    TX   | VLAN\n");
	pp_printf(      "------+-------------------+-------------------------+---------+---------+-----\n");

	for (i = 0 ; i < ndevs; i++) {
		/* reuse the string above, strings between "|" will be overwritten anyway */
		pp_printf("Iface |        MAC        |       IP (source)       |    RX   |    TX   | VLAN\n");
	}

	pp_printf("\n----- HAL ---|---------------- PPSI -------------------------------------------------\n");
	pp_printf(  " Iface| Freq |    Config    | MAC of peer port  |    PTP/EXT/PDETECT States    | Pro \n");
	pp_printf(  "------+------+--------------+-------------------+------------------------------+-----\n");
	for (i = 0 ; i < ndevs; i++) {
		/* reuse the string above, strings between "|" will be overwritten anyway */
		pp_printf(" Iface| Freq |    Config    | MAC of peer port  |    PTP/EXT/PDETECT States    | Pro \n");
	}

	cprintf(C_BLUE, "Pro - Protocol mapping: V-Ethernet over "
			"VLAN; U-UDP; R-Ethernet\n");
	
	cprintf(C_CYAN, "\n--------------------------- Synchronization status ----------------------------");
}

void print_main_data(void)
{
	struct hal_port_state state;
	int tx, rx;
	int leap_sec, tmp;
	uint64_t sec;
	uint32_t nsec;
	char buf[20];
	uint8_t mac[ETH_ALEN];
	int ndevs;
	int i;

	const char *pll_locking_state_name;
	shw_pps_gen_get_time(&sec, &nsec);

	/* TAI Time */
	pcprintf(4, 11, C_WHITE, "%s", format_time(sec, TIME_FORMAT_SORTED));

	
	/* UTC offset */
	wrc_ptp_get_leapsec(&leap_sec , &tmp /* dummy */);
	pprintf(4, 44, "%d", leap_sec);

	/* Timing mode  */
	pprintf(5, 11, getStateAsString(timing_mode_state, WRPC_ARCH_G(ppg)->timingMode));

	/* PLL locking state */
	if (spll_check_lock(0))
		pll_locking_state_name = "Locked ";
	else {
		pll_locking_state_name = "Locking";
	}
	pprintf(5, 44, "%s", pll_locking_state_name);

	ndevs = netif_get_device_count();

	/* show_ports
	------+-------------------+-------------------------+---------+---------+-----
	Iface |        MAC        |       IP (source)       |    RX   |    TX   | VLAN
	------+-------------------+-------------------------+---------+---------+-----
	*/

	for (i = 0 ; i < ndevs; i++) {
		struct wrc_netif_device *ndev = netif_get_device(i);
		int port_up = ndev->link_state == NETIF_LINK_UP;

		if (port_up) {
			pcprintf(9, 1, C_GREEN, " %s: ", ndev->name);
		} else {
			pcprintf(9, 1, C_RED, "*%s: ", ndev->name);
		}

		if (i == 0) /* FIXME: should be independent for each interface */
		{
			ep_get_mac_addr(&wrc_endpoint_dev, mac);
			format_mac(buf, mac);
			pcprintf(9, 9, C_MAGENTA, "%s", buf);
			if (HAS_IP && port_up) {
				uint8_t ip[INET_ALEN];

				getIP(ip);
				format_ip(buf, ip);
				switch (ip_status) {
				case IP_TRAINING:
					pcprintf(9, 29, C_RED,   "BOOTP running          ");
					break;
				case IP_OK_BOOTP:
					pcprintf(9, 29, C_GREEN, "%16s(BOOTP)", buf);
					break;
				case IP_OK_STATIC:
					pcprintf(9, 29, C_GREEN, "%15s(static)", buf);
					break;
				}
			} else
				pcprintf(9, 29, C_GREEN, "                       ");

			minic_get_stats(&tx, &rx);
			pcprintf(9, 55, C_MAGENTA, "%7d", rx);
			pprintf(9, 65, "%7d", tx);
			pprintf(9, 75, "%4d", wrc_vlan_number);
		}

	}
	/* 
	----- HAL ---|---------------- PPSI -------------------------------------------------
	 Iface| Freq |    Config    | MAC of peer port  |    PTP/EXT/PDETECT States    | Pro 
	------+------+--------------+-------------------+------------------------------+----- */

	for (i = 0 ; i < ndevs; i++) {
		struct wrc_netif_device *ndev = netif_get_device(i);
		int port_up = ndev->link_state == NETIF_LINK_UP;
		int color;

		if (port_up) {
			pcprintf(14, 1, C_GREEN, " %s: ", ndev->name);
		} else {
			pcprintf(14, 1, C_RED,   "*%s: ", ndev->name);
		}

		/* FIXME: should be independent for each interface */
		wrpc_get_port_state(&state, NULL);

		if (state.locked)
			pcprintf(14, 9, C_GREEN, "Lock");
		else
			pcprintf(14, 9, C_RED,   "    ");

		
/* ----------------------------------------------------------------------------------------------------------------------- */
		/*
		 * Actually, what is interesting is the PTP state.
		 * For this lookup, the port in ppsi shmem
		 */
		/* Assume one instance per port */
		/* FIXME: add support of more ports */
//		for (j = 0; j < ppg->nlinks; j++) {
			{
			char str_config[15];
			/* so far support only for one instance */
			struct pp_instance *ppi_pt = ppg->pp_instances;
			int proto_extension = ppi_pt->protocol_extension;
			struct proto_ext_info_t *pe_info = IS_PROTO_EXT_INFO_AVAILABLE(proto_extension) ? &proto_ext_info[proto_extension] :  &proto_ext_info[0] ;
			unsigned char *p = ppi_pt->activePeer;
			char * extension_state_name = EMPTY_EXTENSION_STATE_NAME;
			char proto;

#if 0 /* FIXME: only one instance so far */
			if (strcmp(if_name,
					ppi->cfg.iface_name)) {
				/* Instance not for this interface
				    * skip */
				continue;
			}
#endif
			// Evaluate the instance configuration
			strcpy(str_config,"unknown");
			if (is_slaveOnly(ppg->defaultDS)) {
				strncpy(str_config, "slaveOnly", sizeof(str_config) - 1);
			} else {
				if (is_externalPortConfigurationEnabled(ppg->defaultDS)) {
					int s = 0;
					for (s = 0; s < sizeof(desired_states) / sizeof(struct desired_state_t); s++) {
						if (desired_states[s].state == ppi_pt->externalPortConfigurationPortDS.desiredState) {
							strncpy(str_config, desired_states[s].str_state, sizeof(str_config) - 1);
							break;
						}
					}

				} else {
					if (is_masterOnly(ppi_pt->portDS)) {
						strncpy(str_config, "masterOnly", sizeof(str_config) - 1);
					} else {
						strncpy(str_config, "auto", sizeof(str_config) - 1);
					}
				}
			}
			str_config[sizeof(str_config) - 1] = 0; // Force the string to be well terminated
			pcprintf(14, 16, C_WHITE, "%-12s", str_config);

			/* peer not implemented */
			pprintf(14, 31, "%02x:%02x"
					":%02x:%02x:%02x:%02x ",
					p[0], p[1], p[2], p[3],
					p[4], p[5]);

			pcprintf(14, 51, C_GREEN, "%s/", getStateAsString(pp_instance_state_to_name, ppi_pt->state));
			/* print extension state */
			switch (ppi_pt->protocol_extension) {
#if CONFIG_HAS_EXT_WR
			case PPSI_EXT_WR :
			{
				portDS_t *portDS = ppi_pt->portDS;
				struct wr_dsport *extPortDS;

				extension_state_name = getStateAsString(wr_instance_extension_state, - 1); // Default value

				if (portDS) {
					if ((extPortDS = portDS->ext_dsport))
						extension_state_name = getStateAsString(wr_instance_extension_state, extPortDS->state);
				}
				break;
			}
#endif
#if CONFIG_HAS_EXT_L1SYNC
			case PPSI_EXT_L1S :
			{
				portDS_t *portDS;

				extension_state_name = getStateAsString(l1e_instance_extension_state, - 1); // Default value
				if ((portDS = wrs_shm_follow(ppsi_head, ppi->portDS))) {
					l1e_ext_portDS_t *extPortDS;

					if ((extPortDS = wrs_shm_follow(ppsi_head, portDS->ext_dsport))) {
							extension_state_name = getStateAsString(l1e_instance_extension_state, extPortDS->basic.L1SyncState);
					}
				}
				break;
			}
#endif
			}
			pp_printf("%s/%s", extension_state_name, getStateAsString(prot_detection_state_name, ppi_pt->pdstate));

			/* proto */
			switch (ppi_pt->proto) {
			case PPSI_PROTO_RAW:
				proto = 'R';
				break;
			case PPSI_PROTO_UDP:
				proto = 'U';
				break;
			case PPSI_PROTO_VLAN:
				proto = 'V';
				break;
			default:
				proto = '?';
			}

			pcprintf(14, 82, C_WHITE, "%c", proto);
			color = extensionStateColor(ppi_pt);
			cprintf(color, "-%c", pe_info->short_ext_name);
		}
/* ----------------------------------------------------------------------------------------------------------------------- */
	}

	return;
}

void print_aux_data(void)
{
	int n_out, i;
	struct spll_aux_clock_status aux_stat;

	spll_get_num_channels(NULL, &n_out);

	for (i = 0; i < n_out - 1; i++) {
		cprintf(C_MAGENTA, "\n\nAux clock %d status:        ", i);

		aux_stat = spll_get_aux_status(i);

		if (aux_stat.flags & SPLL_AUX_SLAVE_ENABLED)
			cprintf(C_GREEN, "enabled");

		if (aux_stat.flags & SPLL_AUX_TRACKING_ENABLED)
			cprintf(C_GREEN, "tracking source");

		if (aux_stat.flags & SPLL_AUX_SLAVE_LOCKED)
			cprintf(C_GREEN, ", locked");

		if (aux_stat.flags & SPLL_AUX_TRACKING_READY)
		{
			cprintf(C_GREEN, ", ready");
			cprintf(C_WHITE, " (AUX-to-WR offset: %d ps)", aux_stat.phase);
		}
	}

	return;
}

void print_servo_description()
{
	pcprintf(18, 1, C_BLUE, "Servo state:\n");

	cprintf(C_CYAN, "\n--- Timing parameters ---------------------------------------------------------\n");

	cprintf(C_BLUE, "meanDelay        :\n");

	pp_printf("delayMS          :\n");
	pp_printf("delayMM          :\n");

	//pp_printf("Estimated link length:     ");

	pp_printf("delayAsymmetry   :\n");
	pp_printf("delayCoefficient :");
	pprintf(25, 45, "fpa\n");

	pp_printf("ingressLatency   :\n");
	pp_printf("egressLatency    :\n");
	pp_printf("semistaticLatency:\n");
	pp_printf("offsetFromMaster :\n");
	if (gui_description & DESCRIPTION_WR_SERVO) {
		pp_printf("Phase setpoint   :\n");
		pp_printf("Skew             :\n");
	}
	pp_printf("Update counter   :\n");
	if (gui_description & DESCRIPTION_WR_SERVO) {
		pp_printf("Master PHY delays TX:\n"); /* RX: */
		pp_printf("Slave  PHY delays TX:\n"); /* RX: */
	}
}

void print_servo_data(struct pp_instance *ppi)
{
	wrh_servo_t * wr_servo;
	wr_servo_ext_t * wr_servo_ext = NULL;
	char buf[128];
	int row_offset;
	int proto_extension = ppi->extState!= PP_EXSTATE_DISABLE ? ppi->protocol_extension : PPSI_EXT_NONE;
	struct proto_ext_info_t *pe_info = IS_PROTO_EXT_INFO_AVAILABLE(proto_extension) ? &proto_ext_info[proto_extension] :  &proto_ext_info[0];

	/* --------------------------- Synchronization status ---------------------------- */
	pprintf(18, 1, "");
	if (ppi->state != PPS_SLAVE || !(ppi->servo->flags & PP_SERVO_FLAG_VALID)) {
		cprintf(C_RED, "Link down, master mode or sync info not valid");
		return;
	}

	wr_servo = (ppi->protocol_extension == PPSI_EXT_WR && ppi->extState == PP_EXSTATE_ACTIVE) ?
			(wrh_servo_t*) ppi->ext_data : NULL;

	/* should print servio description */
	gui_description |= DESCRIPTION_SERVO;


	if (wr_servo) {
		wr_servo_ext = &((struct wr_data *)wr_servo)->servo_ext;
	}

	/* should print WR servio description */
	gui_description |= wr_servo ? DESCRIPTION_WR_SERVO : 0;
	
	/* Avoid printing new data if change in description is expected.
	 * This avoids extra redraw of data values */
	if(prev_gui_description
		!= (gui_description 
		    & (DESCRIPTION_MAIN
		       | DESCRIPTION_SERVO
		       | DESCRIPTION_WR_SERVO)
		   )
	  )
		return;

	if (pe_info->lastt && time(NULL) - pe_info->lastt > 5) {
		pcprintf(18, 23, C_RED, "--- not updating ---\n");
	} else {
		pcprintf(18, 23, C_WHITE, "%s:%s: %s%-15s\n",
				ppi->cfg.iface_name,
				    pe_info->ext_name,
				    ppi->servo->servo_state_name,
				    ppi->servo->flags & PP_SERVO_FLAG_WAIT_HW ?
				" (wait for hw)" : "");
	}

	/* "tracking disabled" is just a testing tool */
	if (wr_servo  && !wr_servo->tracking_enabled)
		cprintf(C_RED, "Tracking forcibly disabled\n");
	else
		pp_printf("\e[K"); /* clear till the end of a line */
		

	/* +- Timing parameters --------------------------------------------------------- */

	pcprintf(21, 20, C_WHITE, "%19s nsec", timeIntervalToString_ns_dot_ps(ppg->currentDS->meanDelay, buf));

	/*delayMS */
	pcprintf(22, 20, C_WHITE,"%24s", optimized_pp_time_toString_ps_as_ns(&ppi->servo->delayMS, buf));	
	{
		struct pp_time *delayMM = wr_servo_ext ?
				&wr_servo_ext->rawDelayMM :
				&ppi->servo->delayMM;
		/* delayMM */
		pcprintf(23, 20, C_WHITE,"%24s", optimized_pp_time_toString_ps_as_ns(delayMM, buf));
	}

	//cprintf(C_BLUE, "Estimated link length:     ");
	/* (RTT - deltas) / 2 * c / ri
	    c = 299792458 - speed of light in m/s
	    ri = 1.4682 - refractive index for fiber g.652. However,
			experimental measurements using long (~5km) and
			short (few m) fibers gave a value 1.4827
	    */
	//cprintf(C_WHITE, "%10.2f meters\n",
	//	crtt / 2 / 1e6 * 299.792458 / 1.4827);


	/* delayAsymmetry */
	pcprintf(24, 20, C_WHITE, "%19s nsec",   timeIntervalToString_ns_dot_ps(ppi->portDS->delayAsymmetry, buf));
	/* delayCoefficient */
	pcprintf(25, 23, C_WHITE, "%s", relativeDifferenceToString(ppi->asymmetryCorrectionPortDS.scaledDelayCoefficient, buf));
	/* fpa */
	pcprintf(25, 51, C_WHITE, "%Lu", ppi->asymmetryCorrectionPortDS.scaledDelayCoefficient); /* print as unsigned! */

	/* ingressLatency */
	pcprintf(26, 20, C_WHITE, "%19s nsec",   timeIntervalToString_ns_dot_ps(ppi->timestampCorrectionPortDS.ingressLatency, buf));
	/* egressLatency */
	pcprintf(27, 20, C_WHITE, "%19s nsec",   timeIntervalToString_ns_dot_ps(ppi->timestampCorrectionPortDS.egressLatency, buf));
	/* semistaticLatency */
	pcprintf(28, 20, C_WHITE, "%19s nsec",   timeIntervalToString_ns_dot_ps(ppi->timestampCorrectionPortDS.semistaticLatency, buf));

	/*if (0) {
		cprintf(C_BLUE, "Fiber asymmetry:   ");
		cprintf(C_WHITE, "%.3f nsec\n",
			ss.fiber_asymmetry/1000.0);
	}*/

	/* offsetFromMaster */
	pcprintf(29, 20, C_WHITE, "%19s nsec", timeIntervalToString_ns_dot_ps (ppg->currentDS->offsetFromMaster, buf));
	row_offset = 30;
	if (wr_servo) {
		/* Phase setpoint */
		pcprintf(30, 20, C_WHITE, "%19s nsec", convert_ps_to_str_ns(buf, (int64_t) wr_servo->cur_setpoint_ps));


		/* Skew */
		pcprintf(31, 20, C_WHITE, "%19s nsec", convert_ps_to_str_ns(buf, wr_servo->skew_ps));
		row_offset += 2;
	}

	 /* Update counter */
	pcprintf(row_offset, 23, C_WHITE, "%16u times", ppi->servo->update_count);
	if (ppi->servo->update_count != pe_info->last_count) {
		pe_info->lastt = time(NULL);
		pe_info->last_count = ppi->servo->update_count;
	}

	if (wr_servo) {
		/* Master PHY delays TX */
		pcprintf(33, 26, C_WHITE,"%22s", optimized_pp_time_toString_ps_as_ns(&wr_servo_ext->delta_txm, buf));
		cprintf(C_BLUE, "  RX:");
		/* print and clear till the end of a line */
		cprintf(C_WHITE,"%22s\e[K", optimized_pp_time_toString_ps_as_ns(&wr_servo_ext->delta_rxm, buf));

		/* Slave  PHY delays TX */
		pcprintf(34, 26, C_WHITE,"%22s", optimized_pp_time_toString_ps_as_ns(&wr_servo_ext->delta_txs, buf));
		cprintf(C_BLUE, "  RX:");
		/* print and clear till the end of a line */
		cprintf(C_WHITE,"%22s\e[K", optimized_pp_time_toString_ps_as_ns(&wr_servo_ext->delta_rxs, buf));
	}

}

int wrc_log_stats(void)
{
#if 0
	struct hal_port_state state;
	int tx, rx;
	struct spll_aux_clock_status aux_stat;
	uint64_t sec;
	uint32_t nsec;
	struct wr_servo_state *s =
			&((struct wr_data *)ppi->ext_data)->servo_state;
	static uint32_t last_jiffies;
	int n_out;
	int i;

	if (!wrc_stat_running)
		return 0;

	if (!last_jiffies)
		last_jiffies = timer_get_tics() - 1 -  WRC_MONITOR_REFRESH_PERIOD;
	/* stats update condition for Slave mode */
	if (wrc_stats_last == s->update_count && ptp_mode == WRC_MODE_SLAVE)
		return 0;
	/* stats update condition for Master mode */
	if (time_before(timer_get_tics(), last_jiffies + WRC_MONITOR_REFRESH_PERIOD) &&
			ptp_mode != WRC_MODE_SLAVE)
		return 0;
	last_jiffies = timer_get_tics();

	/* Print only one time */
	if (wrc_stat_running == -1)
		wrc_stat_running = 0;

	wrc_stats_last = s->update_count;

	shw_pps_gen_get_time(&sec, &nsec);
	wrpc_get_port_state(&state, NULL);
	minic_get_stats(&tx, &rx);
	pp_printf("lnk:%d rx:%d tx:%d ", state.state, rx, tx);
	pp_printf("lock:%d ", state.locked ? 1 : 0);
	pp_printf("ptp:%s ", wrc_ptp_state());
	if (ptp_mode == WRC_MODE_SLAVE) {
		pp_printf("sv:%d ", (s->flags & WR_FLAG_VALID) ? 1 : 0);
		pp_printf("ss:'%s' ", s->servo_state_name);
	}

	spll_get_num_channels(NULL, &n_out);

	for (i = 0; i < n_out; i++) {
		aux_stat = spll_get_aux_status(i);
		pp_printf("aux%d:%08x%08x ", i, aux_stat.flags, aux_stat.phase);
	}
	
	/* fixme: clock is not always 125 MHz */
	pp_printf("sec:%d nsec:%d ", (uint32_t) sec, nsec);
	if (ptp_mode == WRC_MODE_SLAVE) {
		pp_printf("mu:%s ", print64(s->picos_mu, 0));
		pp_printf("dms:%s ", print64(s->delta_ms, 0));
		pp_printf("dtxm:%d drxm:%d ", (int32_t) s->delta_tx_m,
			(int32_t) s->delta_rx_m);
		pp_printf("dtxs:%d drxs:%d ", (int32_t) s->delta_tx_s,
			(int32_t) s->delta_rx_s);
		int64_t total_asymmetry = s->picos_mu -
				  2LL * s->delta_ms;
		pp_printf("asym:%d ", (int32_t) (total_asymmetry));
		pp_printf("crtt:%s ", print64(s->picos_mu -
					s->delta_tx_m -
					s->delta_rx_m -
					s->delta_tx_s -
					s->delta_rx_s, 0));
		pp_printf("cko:%d ", (int32_t) (s->offset));
		pp_printf("setp:%d ", (int32_t) (s->cur_setpoint));
		pp_printf("ucnt:%d ", (int32_t) s->update_count);
		pp_printf("bslide:%d ", ep_get_bitslide(&wrc_endpoint_dev));
	}
	
	pp_printf("hd:%d md:%d ad:%d ", spll_get_dac(-1), spll_get_dac(0),
		spll_get_dac(1));

	if (1) {
		int32_t temp;

		temp = wrc_temp_get("pcb");
		pp_printf("temp: %d.%04d C", temp >> 16,
			  (int)((temp & 0xffff) * 10 * 1000 >> 16));
	}

	pp_printf("\n");
#endif
	return 1;
}


int wrc_wr_diags(void)
{
#if 0
	struct hal_port_state ps;
	static uint32_t last_jiffies;
	int tx, rx;
	uint64_t sec;
	uint32_t nsec;
	int n_out;
	uint32_t aux_stat = 0;
	int temp = 0, valid = 0, snapshot = 0, i;

	struct pp_instance *ppi = ppg->pp_instances;
	valid    = wdiag_get_valid();
	snapshot = wdiag_get_snapshot();

	/* if the data is snapshot and there is already valid data, do not
	 * refresh */
	if (valid & snapshot)
	      return 0;

	/* ***************** lock data from reading by user **************** */
	if (!last_jiffies)
		last_jiffies = timer_get_tics() - 1 -  WRC_DIAG_REFRESH_PERIOD;
	/* stats update condition */
	if (time_before(timer_get_tics(), last_jiffies + WRC_DIAG_REFRESH_PERIOD))
		return 0;
	last_jiffies = timer_get_tics();

	/* ***************** lock data from reading by user **************** */
	wdiag_set_valid(0);
	
	/* frame statistics */
	minic_get_stats(&tx, &rx);
	wdiags_write_cnts(tx, rx);

	/* local time */
	shw_pps_gen_get_time(&sec, &nsec);
	wdiags_write_time(sec, nsec);
	
	/* port state (from hal) */
	wrpc_get_port_state(&ps, NULL);
	wdiags_write_port_state((ps.state  ? 1 : 0), (ps.locked ? 1 : 0));

	/* port PTP State (from ppsi)
	* see:
		ppsi/proto-ext-whiterabbit/wr-constants.h
		ppsi/include/ppsi/ieee1588_types.h
	0  : none
	1  : PPS_INITIALIZING
	2  : PPS_FAULTY
	3  : PPS_DISABLED
	4  : PPS_LISTENING
	5  : PPS_PRE_MASTER
	6  : PPS_MASTER
	7  : PPS_PASSIVE
	8  : PPS_UNCALIBRATED
	9  : PPS_SLAVE
	100: WRS_PRESENT
	101: WRS_S_LOCK
	102: WRS_M_LOCK
	103: WRS_LOCKED
	104, 108-116:WRS_CALIBRATION
	105: WRS_CALIBRATED
	106: WRS_RESP_CALIB_REQ
	107: WRS_WR_LINK_ON
	*/
	wdiags_write_ptp_state((uint8_t)ppi->state);

	/* servo state (if slave)s */
	if (ptp_mode == WRC_MODE_SLAVE){
		struct pp_servo *ss = ppi->servo;
// 			&((struct wr_data *)ppi->ext_data)->servo_state;
		int32_t asym   = (int32_t)(ss->picos_mu-2LL * ss->delta_ms);
		int wr_mode    = (ss->flags & WR_FLAG_VALID) ? 1 : 0;
		int servostate = ss->state;
		/* see ppsi/proto-ext-whiterabbit/wr-constants.c:
		0: WR_UNINITIALIZED = 0,
		1: WR_SYNC_NSEC,
		2: WR_SYNC_TAI,
		3: WR_SYNC_PHASE,
		4: WR_TRACK_PHASE,
		5: WR_WAIT_OFFSET_STABLE */
		
		wdiags_write_servo_state(wr_mode, servostate, ss->picos_mu,
					 ss->delta_ms, asym, ss->offset,
					 ss->cur_setpoint, ss->update_count);
	}
	
	/* auxiliar channels (if any) */
	spll_get_num_channels(NULL, &n_out);
	if (n_out > 8) n_out = 8; /* hardware limit. */
	for (i = 0; i < n_out; i++) {
		aux_stat |= ((SPLL_AUX_SLAVE_LOCKED | SPLL_AUX_TRACKING_READY) & spll_get_aux_status(i).flags) << i;
	}
	wdiags_write_aux_state(aux_stat);
	
	/* temperature */
	temp = wrc_temp_get("pcb");
	wdiags_write_temp(temp);

	/* **************** unlock data from reading by user  ************** */
	wdiag_set_valid(1);
#endif
	return 1;
}

