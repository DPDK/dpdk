/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2018 Atomic Rules LLC
 */

#include <unistd.h>
#include <pthread.h>

#include <rte_string_fns.h>
#include <rte_malloc.h>

#include "ark_pktgen.h"
#include "ark_logs.h"

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
	{{"dg-mode"}, OTBOOL, {1} },
	{{"run"}, OTBOOL, {0} },
	{{"pause"}, OTBOOL, {0} },
	{{"reset"}, OTBOOL, {0} },
	{{"dump"}, OTBOOL, {0} },
	{{"gen_forever"}, OTBOOL, {0} },
	{{"en_slaved_start"}, OTBOOL, {0} },
	{{"vary_length"}, OTBOOL, {0} },
	{{"incr_payload"}, OTBOOL, {0} },
	{{"incr_first_byte"}, OTBOOL, {0} },
	{{"ins_seq_num"}, OTBOOL, {0} },
	{{"ins_time_stamp"}, OTBOOL, {1} },
	{{"ins_udp_hdr"}, OTBOOL, {0} },
	{{"num_pkts"}, OTLONG, .v.LONG = 100000000},
	{{"payload_byte"}, OTINT, {0x55} },
	{{"pkt_spacing"}, OTINT, {130} },
	{{"pkt_size_min"}, OTINT, {2006} },
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
	{{"bytes_per_cycle"}, OTINT, {10} },
	{{"shaping"}, OTBOOL, {0} },
	{{"dst_ip"}, OTSTRING, .v.STR = "169.254.10.240"},
	{{"dst_port"}, OTINT, {65536} },
	{{"src_port"}, OTINT, {65536} },
};

ark_pkt_gen_t
ark_pktgen_init(void *adr, int ord, int l2_mode)
{
	struct ark_pkt_gen_inst *inst =
		rte_malloc("ark_pkt_gen_inst_pmd",
			   sizeof(struct ark_pkt_gen_inst), 0);
	if (inst == NULL) {
		ARK_PMD_LOG(ERR, "Failed to malloc ark_pkt_gen_inst.\n");
		return inst;
	}
	inst->regs = (struct ark_pkt_gen_regs *)adr;
	inst->ordinal = ord;
	inst->l2_mode = l2_mode;
	return inst;
}

void
ark_pktgen_uninit(ark_pkt_gen_t handle)
{
	rte_free(handle);
}

void
ark_pktgen_run(ark_pkt_gen_t handle)
{
	struct ark_pkt_gen_inst *inst = (struct ark_pkt_gen_inst *)handle;

	inst->regs->pkt_start_stop = 1;
}

uint32_t
ark_pktgen_paused(ark_pkt_gen_t handle)
{
	struct ark_pkt_gen_inst *inst = (struct ark_pkt_gen_inst *)handle;
	uint32_t r = inst->regs->pkt_start_stop;

	return (((r >> 24) & 1) == 1) || (((r >> 16) & 1) == 1)  || (r == 0);
}

void
ark_pktgen_pause(ark_pkt_gen_t handle)
{
	struct ark_pkt_gen_inst *inst = (struct ark_pkt_gen_inst *)handle;
	int cnt = 0;

	inst->regs->pkt_start_stop = 0;

	while (!ark_pktgen_paused(handle)) {
		usleep(1000);
		if (cnt++ > 100) {
			ARK_PMD_LOG(NOTICE, "Pktgen %d failed to pause.\n",
				    inst->ordinal);
			break;
		}
	}
	ARK_PMD_LOG(DEBUG, "Pktgen %d paused.\n", inst->ordinal);
}

void
ark_pktgen_reset(ark_pkt_gen_t handle)
{
	struct ark_pkt_gen_inst *inst = (struct ark_pkt_gen_inst *)handle;

	if (!ark_pktgen_is_running(handle) &&
	    !ark_pktgen_paused(handle)) {
		ARK_PMD_LOG(DEBUG, "Pktgen %d is not running"
			      " and is not paused. No need to reset.\n",
			      inst->ordinal);
		return;
	}

	if (ark_pktgen_is_running(handle) &&
	    !ark_pktgen_paused(handle)) {
		ARK_PMD_LOG(DEBUG,
			      "Pktgen %d is not paused. Pausing first.\n",
			      inst->ordinal);
		ark_pktgen_pause(handle);
	}

	ARK_PMD_LOG(DEBUG, "Resetting pktgen %d.\n", inst->ordinal);
	inst->regs->pkt_start_stop = (1 << 8);
}

uint32_t
ark_pktgen_tx_done(ark_pkt_gen_t handle)
{
	struct ark_pkt_gen_inst *inst = (struct ark_pkt_gen_inst *)handle;
	uint32_t r = inst->regs->pkt_start_stop;

	return (((r >> 24) & 1) == 1);
}

uint32_t
ark_pktgen_is_running(ark_pkt_gen_t handle)
{
	struct ark_pkt_gen_inst *inst = (struct ark_pkt_gen_inst *)handle;
	uint32_t r = inst->regs->pkt_start_stop;

	return ((r & 1) == 1);
}

uint32_t
ark_pktgen_is_gen_forever(ark_pkt_gen_t handle)
{
	struct ark_pkt_gen_inst *inst = (struct ark_pkt_gen_inst *)handle;
	uint32_t r = inst->regs->pkt_ctrl;

	return (((r >> 24) & 1) == 1);
}

void
ark_pktgen_wait_done(ark_pkt_gen_t handle)
{
	struct ark_pkt_gen_inst *inst = (struct ark_pkt_gen_inst *)handle;
	int wait_cycle = 10;

	if (ark_pktgen_is_gen_forever(handle))
		ARK_PMD_LOG(NOTICE, "Pktgen wait_done will not terminate"
			    " because gen_forever=1\n");

	while (!ark_pktgen_tx_done(handle) && (wait_cycle > 0)) {
		usleep(1000);
		wait_cycle--;
		ARK_PMD_LOG(DEBUG,
			      "Waiting for pktgen %d to finish sending...\n",
			      inst->ordinal);
	}
	ARK_PMD_LOG(DEBUG, "Pktgen %d done.\n", inst->ordinal);
}

uint32_t
ark_pktgen_get_pkts_sent(ark_pkt_gen_t handle)
{
	struct ark_pkt_gen_inst *inst = (struct ark_pkt_gen_inst *)handle;
	return inst->regs->pkts_sent;
}

void
ark_pktgen_set_payload_byte(ark_pkt_gen_t handle, uint32_t b)
{
	struct ark_pkt_gen_inst *inst = (struct ark_pkt_gen_inst *)handle;
	inst->regs->pkt_payload = b;
}

void
ark_pktgen_set_pkt_spacing(ark_pkt_gen_t handle, uint32_t x)
{
	struct ark_pkt_gen_inst *inst = (struct ark_pkt_gen_inst *)handle;
	inst->regs->pkt_spacing = x;
}

void
ark_pktgen_set_pkt_size_min(ark_pkt_gen_t handle, uint32_t x)
{
	struct ark_pkt_gen_inst *inst = (struct ark_pkt_gen_inst *)handle;
	inst->regs->pkt_size_min = x;
}

void
ark_pktgen_set_pkt_size_max(ark_pkt_gen_t handle, uint32_t x)
{
	struct ark_pkt_gen_inst *inst = (struct ark_pkt_gen_inst *)handle;
	inst->regs->pkt_size_max = x;
}

void
ark_pktgen_set_pkt_size_incr(ark_pkt_gen_t handle, uint32_t x)
{
	struct ark_pkt_gen_inst *inst = (struct ark_pkt_gen_inst *)handle;
	inst->regs->pkt_size_incr = x;
}

void
ark_pktgen_set_num_pkts(ark_pkt_gen_t handle, uint32_t x)
{
	struct ark_pkt_gen_inst *inst = (struct ark_pkt_gen_inst *)handle;
	inst->regs->num_pkts = x;
}

void
ark_pktgen_set_src_mac_addr(ark_pkt_gen_t handle, uint64_t mac_addr)
{
	struct ark_pkt_gen_inst *inst = (struct ark_pkt_gen_inst *)handle;
	inst->regs->src_mac_addr_h = (mac_addr >> 32) & 0xffff;
	inst->regs->src_mac_addr_l = mac_addr & 0xffffffff;
}

void
ark_pktgen_set_dst_mac_addr(ark_pkt_gen_t handle, uint64_t mac_addr)
{
	struct ark_pkt_gen_inst *inst = (struct ark_pkt_gen_inst *)handle;
	inst->regs->dst_mac_addr_h = (mac_addr >> 32) & 0xffff;
	inst->regs->dst_mac_addr_l = mac_addr & 0xffffffff;
}

void
ark_pktgen_set_eth_type(ark_pkt_gen_t handle, uint32_t x)
{
	struct ark_pkt_gen_inst *inst = (struct ark_pkt_gen_inst *)handle;
	inst->regs->eth_type = x;
}

void
ark_pktgen_set_hdr_dW(ark_pkt_gen_t handle, uint32_t *hdr)
{
	uint32_t i;
	struct ark_pkt_gen_inst *inst = (struct ark_pkt_gen_inst *)handle;

	for (i = 0; i < 7; i++)
		inst->regs->hdr_dw[i] = hdr[i];
}

void
ark_pktgen_set_start_offset(ark_pkt_gen_t handle, uint32_t x)
{
	struct ark_pkt_gen_inst *inst = (struct ark_pkt_gen_inst *)handle;

	inst->regs->start_offset = x;
}

static struct OPTIONS *
options(const char *id)
{
	unsigned int i;

	for (i = 0; i < sizeof(toptions) / sizeof(struct OPTIONS); i++) {
		if (strcmp(id, toptions[i].opt) == 0)
			return &toptions[i];
	}

	ARK_PMD_LOG(ERR,
		    "Pktgen: Could not find requested option!, "
		    "option = %s\n",
		    id
		    );
	return NULL;
}

static int pmd_set_arg(char *arg, char *val);
static int
pmd_set_arg(char *arg, char *val)
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
			strlcpy(o->v.STR, val, ARK_MAX_STR_LEN);
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
ark_pktgen_parse(char *args)
{
	char *argv, *v;
	const char toks[] = " =\n\t\v\f \r";
	argv = strtok(args, toks);
	v = strtok(NULL, toks);
	while (argv && v) {
		pmd_set_arg(argv, v);
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

static void
ark_pktgen_set_pkt_ctrl(ark_pkt_gen_t handle,
			uint32_t gen_forever,
			uint32_t en_slaved_start,
			uint32_t vary_length,
			uint32_t incr_payload,
			uint32_t incr_first_byte,
			uint32_t ins_seq_num,
			uint32_t ins_udp_hdr,
			uint32_t ins_time_stamp)
{
	uint32_t r;
	struct ark_pkt_gen_inst *inst = (struct ark_pkt_gen_inst *)handle;

	if (!inst->l2_mode)
		ins_udp_hdr = 0;

	r = ((gen_forever << 24) |
	     (en_slaved_start << 20) |
	     (vary_length << 16) |
	     (incr_payload << 12) |
	     (incr_first_byte << 8) |
	     (ins_time_stamp << 5) |
	     (ins_seq_num << 4) |
	     ins_udp_hdr);

	inst->regs->bytes_per_cycle = options("bytes_per_cycle")->v.INT;
	if (options("shaping")->v.BOOL)
		r = r | (1 << 28);	/* enable shaping */

	inst->regs->pkt_ctrl = r;
}

void
ark_pktgen_setup(ark_pkt_gen_t handle)
{
	uint32_t hdr[7];
	int32_t dst_ip = parse_ipv4_string(options("dst_ip")->v.STR);

	if (!options("pause")->v.BOOL &&
	    (!options("reset")->v.BOOL &&
	     (options("configure")->v.BOOL))) {
		ark_pktgen_set_payload_byte(handle,
					    options("payload_byte")->v.INT);
		ark_pktgen_set_src_mac_addr(handle,
					    options("src_mac_addr")->v.INT);
		ark_pktgen_set_dst_mac_addr(handle,
					    options("dst_mac_addr")->v.LONG);
		ark_pktgen_set_eth_type(handle,
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
		ark_pktgen_set_hdr_dW(handle, hdr);
		ark_pktgen_set_num_pkts(handle,
					options("num_pkts")->v.INT);
		ark_pktgen_set_pkt_size_min(handle,
					    options("pkt_size_min")->v.INT);
		ark_pktgen_set_pkt_size_max(handle,
					    options("pkt_size_max")->v.INT);
		ark_pktgen_set_pkt_size_incr(handle,
					     options("pkt_size_incr")->v.INT);
		ark_pktgen_set_pkt_spacing(handle,
					   options("pkt_spacing")->v.INT);
		ark_pktgen_set_start_offset(handle,
					    options("start_offset")->v.INT);
		ark_pktgen_set_pkt_ctrl(handle,
					options("gen_forever")->v.BOOL,
					options("en_slaved_start")->v.BOOL,
					options("vary_length")->v.BOOL,
					options("incr_payload")->v.BOOL,
					options("incr_first_byte")->v.BOOL,
					options("ins_seq_num")->v.INT,
					options("ins_udp_hdr")->v.BOOL,
					options("ins_time_stamp")->v.INT);
	}

	if (options("pause")->v.BOOL)
		ark_pktgen_pause(handle);

	if (options("reset")->v.BOOL)
		ark_pktgen_reset(handle);
	if (options("run")->v.BOOL) {
		ARK_PMD_LOG(DEBUG, "Starting packet generator on port %d\n",
				options("port")->v.INT);
		ark_pktgen_run(handle);
	}
}

void *
ark_pktgen_delay_start(void *arg)
{
	struct ark_pkt_gen_inst *inst = (struct ark_pkt_gen_inst *)arg;

	/* This function is used exclusively for regression testing, We
	 * perform a blind sleep here to ensure that the external test
	 * application has time to setup the test before we generate packets
	 */
	pthread_detach(pthread_self());
	usleep(100000);
	ark_pktgen_run(inst);
	return NULL;
}
