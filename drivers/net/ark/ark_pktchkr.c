/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2018 Atomic Rules LLC
 */

#include <getopt.h>
#include <sys/time.h>
#include <locale.h>
#include <unistd.h>

#include <rte_ethdev_driver.h>
#include <rte_malloc.h>

#include "ark_pktchkr.h"
#include "ark_logs.h"

static int set_arg(char *arg, char *val);
static int ark_pktchkr_is_gen_forever(ark_pkt_chkr_t handle);

#define ARK_MAX_STR_LEN 64
union OPTV {
	int INT;
	int BOOL;
	uint64_t LONG;
	char STR[ARK_MAX_STR_LEN];
};

enum OPTYPE {
	OTINT,
	OTLONG,
	OTBOOL,
	OTSTRING
};

struct OPTIONS {
	char opt[ARK_MAX_STR_LEN];
	enum OPTYPE t;
	union OPTV v;
};

static struct OPTIONS toptions[] = {
	{{"configure"}, OTBOOL, {1} },
	{{"port"}, OTINT, {0} },
	{{"mac-dump"}, OTBOOL, {0} },
	{{"dg-mode"}, OTBOOL, {1} },
	{{"run"}, OTBOOL, {0} },
	{{"stop"}, OTBOOL, {0} },
	{{"dump"}, OTBOOL, {0} },
	{{"en_resync"}, OTBOOL, {0} },
	{{"tuser_err_val"}, OTINT, {1} },
	{{"gen_forever"}, OTBOOL, {0} },
	{{"en_slaved_start"}, OTBOOL, {0} },
	{{"vary_length"}, OTBOOL, {0} },
	{{"incr_payload"}, OTINT, {0} },
	{{"incr_first_byte"}, OTBOOL, {0} },
	{{"ins_seq_num"}, OTBOOL, {0} },
	{{"ins_time_stamp"}, OTBOOL, {1} },
	{{"ins_udp_hdr"}, OTBOOL, {0} },
	{{"num_pkts"}, OTLONG, .v.LONG = 10000000000000L},
	{{"payload_byte"}, OTINT, {0x55} },
	{{"pkt_spacing"}, OTINT, {60} },
	{{"pkt_size_min"}, OTINT, {2005} },
	{{"pkt_size_max"}, OTINT, {1514} },
	{{"pkt_size_incr"}, OTINT, {1} },
	{{"eth_type"}, OTINT, {0x0800} },
	{{"src_mac_addr"}, OTLONG, .v.LONG = 0xdC3cF6425060L},
	{{"dst_mac_addr"}, OTLONG, .v.LONG = 0x112233445566L},
	{{"hdr_dW0"}, OTINT, {0x0016e319} },
	{{"hdr_dW1"}, OTINT, {0x27150004} },
	{{"hdr_dW2"}, OTINT, {0x76967bda} },
	{{"hdr_dW3"}, OTINT, {0x08004500} },
	{{"hdr_dW4"}, OTINT, {0x005276ed} },
	{{"hdr_dW5"}, OTINT, {0x40004006} },
	{{"hdr_dW6"}, OTINT, {0x56cfc0a8} },
	{{"start_offset"}, OTINT, {0} },
	{{"dst_ip"}, OTSTRING, .v.STR = "169.254.10.240"},
	{{"dst_port"}, OTINT, {65536} },
	{{"src_port"}, OTINT, {65536} },
};

ark_pkt_chkr_t
ark_pktchkr_init(void *addr, int ord, int l2_mode)
{
	struct ark_pkt_chkr_inst *inst =
		rte_malloc("ark_pkt_chkr_inst",
			   sizeof(struct ark_pkt_chkr_inst), 0);
	if (inst == NULL) {
		PMD_DRV_LOG(ERR, "Failed to malloc ark_pkt_chkr_inst.\n");
		return inst;
	}
	inst->sregs = (struct ark_pkt_chkr_stat_regs *)addr;
	inst->cregs =
		(struct ark_pkt_chkr_ctl_regs *)(((uint8_t *)addr) + 0x100);
	inst->ordinal = ord;
	inst->l2_mode = l2_mode;
	return inst;
}

void
ark_pktchkr_uninit(ark_pkt_chkr_t handle)
{
	rte_free(handle);
}

void
ark_pktchkr_run(ark_pkt_chkr_t handle)
{
	struct ark_pkt_chkr_inst *inst = (struct ark_pkt_chkr_inst *)handle;

	inst->sregs->pkt_start_stop = 0;
	inst->sregs->pkt_start_stop = 0x1;
}

int
ark_pktchkr_stopped(ark_pkt_chkr_t handle)
{
	struct ark_pkt_chkr_inst *inst = (struct ark_pkt_chkr_inst *)handle;
	uint32_t r = inst->sregs->pkt_start_stop;

	return (((r >> 16) & 1) == 1);
}

void
ark_pktchkr_stop(ark_pkt_chkr_t handle)
{
	struct ark_pkt_chkr_inst *inst = (struct ark_pkt_chkr_inst *)handle;
	int wait_cycle = 10;

	inst->sregs->pkt_start_stop = 0;
	while (!ark_pktchkr_stopped(handle) && (wait_cycle > 0)) {
		usleep(1000);
		wait_cycle--;
		PMD_DEBUG_LOG(DEBUG, "Waiting for pktchk %d to stop...\n",
			      inst->ordinal);
	}
	PMD_DEBUG_LOG(DEBUG, "Pktchk %d stopped.\n", inst->ordinal);
}

int
ark_pktchkr_is_running(ark_pkt_chkr_t handle)
{
	struct ark_pkt_chkr_inst *inst = (struct ark_pkt_chkr_inst *)handle;
	uint32_t r = inst->sregs->pkt_start_stop;

	return ((r & 1) == 1);
}

static void
ark_pktchkr_set_pkt_ctrl(ark_pkt_chkr_t handle,
			 uint32_t gen_forever,
			 uint32_t vary_length,
			 uint32_t incr_payload,
			 uint32_t incr_first_byte,
			 uint32_t ins_seq_num,
			 uint32_t ins_udp_hdr,
			 uint32_t en_resync,
			 uint32_t tuser_err_val,
			 uint32_t ins_time_stamp)
{
	struct ark_pkt_chkr_inst *inst = (struct ark_pkt_chkr_inst *)handle;
	uint32_t r = (tuser_err_val << 16) | (en_resync << 0);

	inst->sregs->pkt_ctrl = r;
	if (!inst->l2_mode)
		ins_udp_hdr = 0;
	r = ((gen_forever << 24) |
	     (vary_length << 16) |
	     (incr_payload << 12) |
	     (incr_first_byte << 8) |
	     (ins_time_stamp << 5) |
	     (ins_seq_num << 4) |
	     ins_udp_hdr);
	inst->cregs->pkt_ctrl = r;
}

static
int
ark_pktchkr_is_gen_forever(ark_pkt_chkr_t handle)
{
	struct ark_pkt_chkr_inst *inst = (struct ark_pkt_chkr_inst *)handle;
	uint32_t r = inst->cregs->pkt_ctrl;

	return (((r >> 24) & 1) == 1);
}

int
ark_pktchkr_wait_done(ark_pkt_chkr_t handle)
{
	struct ark_pkt_chkr_inst *inst = (struct ark_pkt_chkr_inst *)handle;

	if (ark_pktchkr_is_gen_forever(handle)) {
		PMD_DEBUG_LOG(ERR, "Pktchk wait_done will not terminate"
			      " because gen_forever=1\n");
		return -1;
	}
	int wait_cycle = 10;

	while (!ark_pktchkr_stopped(handle) && (wait_cycle > 0)) {
		usleep(1000);
		wait_cycle--;
		PMD_DEBUG_LOG(DEBUG, "Waiting for packet checker %d's"
			      " internal pktgen to finish sending...\n",
			      inst->ordinal);
		PMD_DEBUG_LOG(DEBUG, "Pktchk %d's pktgen done.\n",
			      inst->ordinal);
	}
	return 0;
}

int
ark_pktchkr_get_pkts_sent(ark_pkt_chkr_t handle)
{
	struct ark_pkt_chkr_inst *inst = (struct ark_pkt_chkr_inst *)handle;

	return inst->cregs->pkts_sent;
}

void
ark_pktchkr_set_payload_byte(ark_pkt_chkr_t handle, uint32_t b)
{
	struct ark_pkt_chkr_inst *inst = (struct ark_pkt_chkr_inst *)handle;

	inst->cregs->pkt_payload = b;
}

void
ark_pktchkr_set_pkt_size_min(ark_pkt_chkr_t handle, uint32_t x)
{
	struct ark_pkt_chkr_inst *inst = (struct ark_pkt_chkr_inst *)handle;

	inst->cregs->pkt_size_min = x;
}

void
ark_pktchkr_set_pkt_size_max(ark_pkt_chkr_t handle, uint32_t x)
{
	struct ark_pkt_chkr_inst *inst = (struct ark_pkt_chkr_inst *)handle;

	inst->cregs->pkt_size_max = x;
}

void
ark_pktchkr_set_pkt_size_incr(ark_pkt_chkr_t handle, uint32_t x)
{
	struct ark_pkt_chkr_inst *inst = (struct ark_pkt_chkr_inst *)handle;

	inst->cregs->pkt_size_incr = x;
}

void
ark_pktchkr_set_num_pkts(ark_pkt_chkr_t handle, uint32_t x)
{
	struct ark_pkt_chkr_inst *inst = (struct ark_pkt_chkr_inst *)handle;

	inst->cregs->num_pkts = x;
}

void
ark_pktchkr_set_src_mac_addr(ark_pkt_chkr_t handle, uint64_t mac_addr)
{
	struct ark_pkt_chkr_inst *inst = (struct ark_pkt_chkr_inst *)handle;

	inst->cregs->src_mac_addr_h = (mac_addr >> 32) & 0xffff;
	inst->cregs->src_mac_addr_l = mac_addr & 0xffffffff;
}

void
ark_pktchkr_set_dst_mac_addr(ark_pkt_chkr_t handle, uint64_t mac_addr)
{
	struct ark_pkt_chkr_inst *inst = (struct ark_pkt_chkr_inst *)handle;

	inst->cregs->dst_mac_addr_h = (mac_addr >> 32) & 0xffff;
	inst->cregs->dst_mac_addr_l = mac_addr & 0xffffffff;
}

void
ark_pktchkr_set_eth_type(ark_pkt_chkr_t handle, uint32_t x)
{
	struct ark_pkt_chkr_inst *inst = (struct ark_pkt_chkr_inst *)handle;

	inst->cregs->eth_type = x;
}

void
ark_pktchkr_set_hdr_dW(ark_pkt_chkr_t handle, uint32_t *hdr)
{
	uint32_t i;
	struct ark_pkt_chkr_inst *inst = (struct ark_pkt_chkr_inst *)handle;

	for (i = 0; i < 7; i++)
		inst->cregs->hdr_dw[i] = hdr[i];
}

void
ark_pktchkr_dump_stats(ark_pkt_chkr_t handle)
{
	struct ark_pkt_chkr_inst *inst = (struct ark_pkt_chkr_inst *)handle;

	PMD_STATS_LOG(INFO, "pkts_rcvd      = (%'u)\n",
		      inst->sregs->pkts_rcvd);
	PMD_STATS_LOG(INFO, "bytes_rcvd     = (%'" PRIU64 ")\n",
		      inst->sregs->bytes_rcvd);
	PMD_STATS_LOG(INFO, "pkts_ok        = (%'u)\n",
		      inst->sregs->pkts_ok);
	PMD_STATS_LOG(INFO, "pkts_mismatch  = (%'u)\n",
		      inst->sregs->pkts_mismatch);
	PMD_STATS_LOG(INFO, "pkts_err       = (%'u)\n",
		      inst->sregs->pkts_err);
	PMD_STATS_LOG(INFO, "first_mismatch = (%'u)\n",
		      inst->sregs->first_mismatch);
	PMD_STATS_LOG(INFO, "resync_events  = (%'u)\n",
		      inst->sregs->resync_events);
	PMD_STATS_LOG(INFO, "pkts_missing   = (%'u)\n",
		      inst->sregs->pkts_missing);
	PMD_STATS_LOG(INFO, "min_latency    = (%'u)\n",
		      inst->sregs->min_latency);
	PMD_STATS_LOG(INFO, "max_latency    = (%'u)\n",
		      inst->sregs->max_latency);
}

static struct OPTIONS *
options(const char *id)
{
	unsigned int i;

	for (i = 0; i < sizeof(toptions) / sizeof(struct OPTIONS); i++) {
		if (strcmp(id, toptions[i].opt) == 0)
			return &toptions[i];
	}
	PMD_DRV_LOG(ERR,
		    "pktchkr: Could not find requested option!, option = %s\n",
		    id);
	return NULL;
}

static int
set_arg(char *arg, char *val)
{
	struct OPTIONS *o = options(arg);

	if (o) {
		switch (o->t) {
		case OTINT:
		case OTBOOL:
			o->v.INT = atoi(val);
			break;
		case OTLONG:
			o->v.INT = atoll(val);
			break;
		case OTSTRING:
			snprintf(o->v.STR, ARK_MAX_STR_LEN, "%s", val);
			break;
		}
		return 1;
	}
	return 0;
}

/******
 * Arg format = "opt0=v,opt_n=v ..."
 ******/
void
ark_pktchkr_parse(char *args)
{
	char *argv, *v;
	const char toks[] = "=\n\t\v\f \r";
	argv = strtok(args, toks);
	v = strtok(NULL, toks);
	while (argv && v) {
		set_arg(argv, v);
		argv = strtok(NULL, toks);
		v = strtok(NULL, toks);
	}
}

static int32_t parse_ipv4_string(char const *ip_address);
static int32_t
parse_ipv4_string(char const *ip_address)
{
	unsigned int ip[4];

	if (sscanf(ip_address, "%u.%u.%u.%u",
		   &ip[0], &ip[1], &ip[2], &ip[3]) != 4)
		return 0;
	return ip[3] + ip[2] * 0x100 + ip[1] * 0x10000ul + ip[0] * 0x1000000ul;
}

void
ark_pktchkr_setup(ark_pkt_chkr_t handle)
{
	uint32_t hdr[7];
	int32_t dst_ip = parse_ipv4_string(options("dst_ip")->v.STR);

	if (!options("stop")->v.BOOL && options("configure")->v.BOOL) {
		ark_pktchkr_set_payload_byte(handle,
					     options("payload_byte")->v.INT);
		ark_pktchkr_set_src_mac_addr(handle,
					     options("src_mac_addr")->v.INT);
		ark_pktchkr_set_dst_mac_addr(handle,
					     options("dst_mac_addr")->v.LONG);

		ark_pktchkr_set_eth_type(handle,
					 options("eth_type")->v.INT);
		if (options("dg-mode")->v.BOOL) {
			hdr[0] = options("hdr_dW0")->v.INT;
			hdr[1] = options("hdr_dW1")->v.INT;
			hdr[2] = options("hdr_dW2")->v.INT;
			hdr[3] = options("hdr_dW3")->v.INT;
			hdr[4] = options("hdr_dW4")->v.INT;
			hdr[5] = options("hdr_dW5")->v.INT;
			hdr[6] = options("hdr_dW6")->v.INT;
		} else {
			hdr[0] = dst_ip;
			hdr[1] = options("dst_port")->v.INT;
			hdr[2] = options("src_port")->v.INT;
			hdr[3] = 0;
			hdr[4] = 0;
			hdr[5] = 0;
			hdr[6] = 0;
		}
		ark_pktchkr_set_hdr_dW(handle, hdr);
		ark_pktchkr_set_num_pkts(handle,
					 options("num_pkts")->v.INT);
		ark_pktchkr_set_pkt_size_min(handle,
					     options("pkt_size_min")->v.INT);
		ark_pktchkr_set_pkt_size_max(handle,
					     options("pkt_size_max")->v.INT);
		ark_pktchkr_set_pkt_size_incr(handle,
					      options("pkt_size_incr")->v.INT);
		ark_pktchkr_set_pkt_ctrl(handle,
					 options("gen_forever")->v.BOOL,
					 options("vary_length")->v.BOOL,
					 options("incr_payload")->v.BOOL,
					 options("incr_first_byte")->v.BOOL,
					 options("ins_seq_num")->v.INT,
					 options("ins_udp_hdr")->v.BOOL,
					 options("en_resync")->v.BOOL,
					 options("tuser_err_val")->v.INT,
					 options("ins_time_stamp")->v.INT);
	}

	if (options("stop")->v.BOOL)
		ark_pktchkr_stop(handle);

	if (options("run")->v.BOOL) {
		PMD_DEBUG_LOG(DEBUG, "Starting packet checker on port %d\n",
			      options("port")->v.INT);
		ark_pktchkr_run(handle);
	}
}
