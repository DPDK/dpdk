/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <unistd.h>

#include "ntos_drv.h"
#include "ntnic_virt_queue.h"
#include "ntnic_mod_reg.h"
#include "ntlog.h"

#define MAX_VIRT_QUEUES 128

#define LAST_QUEUE 127
#define DISABLE 0
#define ENABLE 1
#define RX_AM_DISABLE DISABLE
#define RX_AM_ENABLE ENABLE
#define RX_UW_DISABLE DISABLE
#define RX_UW_ENABLE ENABLE
#define RX_Q_DISABLE DISABLE
#define RX_Q_ENABLE ENABLE
#define RX_AM_POLL_SPEED 5
#define RX_UW_POLL_SPEED 9
#define INIT_QUEUE 1

#define TX_AM_DISABLE DISABLE
#define TX_AM_ENABLE ENABLE
#define TX_UW_DISABLE DISABLE
#define TX_UW_ENABLE ENABLE
#define TX_Q_DISABLE DISABLE
#define TX_Q_ENABLE ENABLE
#define TX_AM_POLL_SPEED 5
#define TX_UW_POLL_SPEED 8

enum nthw_virt_queue_usage {
	NTHW_VIRTQ_UNUSED = 0
};

struct nthw_virt_queue {
	enum nthw_virt_queue_usage usage;
};

static struct nthw_virt_queue rxvq[MAX_VIRT_QUEUES];
static struct nthw_virt_queue txvq[MAX_VIRT_QUEUES];

static void dbs_init_rx_queue(nthw_dbs_t *p_nthw_dbs, uint32_t queue, uint32_t start_idx,
	uint32_t start_ptr)
{
	uint32_t busy;
	uint32_t init;
	uint32_t dummy;

	do {
		get_rx_init(p_nthw_dbs, &init, &dummy, &busy);
	} while (busy != 0);

	set_rx_init(p_nthw_dbs, start_idx, start_ptr, INIT_QUEUE, queue);

	do {
		get_rx_init(p_nthw_dbs, &init, &dummy, &busy);
	} while (busy != 0);
}

static void dbs_init_tx_queue(nthw_dbs_t *p_nthw_dbs, uint32_t queue, uint32_t start_idx,
	uint32_t start_ptr)
{
	uint32_t busy;
	uint32_t init;
	uint32_t dummy;

	do {
		get_tx_init(p_nthw_dbs, &init, &dummy, &busy);
	} while (busy != 0);

	set_tx_init(p_nthw_dbs, start_idx, start_ptr, INIT_QUEUE, queue);

	do {
		get_tx_init(p_nthw_dbs, &init, &dummy, &busy);
	} while (busy != 0);
}

static int nthw_virt_queue_init(struct fpga_info_s *p_fpga_info)
{
	assert(p_fpga_info);

	nthw_fpga_t *const p_fpga = p_fpga_info->mp_fpga;
	nthw_dbs_t *p_nthw_dbs;
	int res = 0;
	uint32_t i;

	p_fpga_info->mp_nthw_dbs = NULL;

	p_nthw_dbs = nthw_dbs_new();

	if (p_nthw_dbs == NULL)
		return -1;

	res = dbs_init(NULL, p_fpga, 0);/* Check that DBS exists in FPGA */

	if (res) {
		free(p_nthw_dbs);
		return res;
	}

	res = dbs_init(p_nthw_dbs, p_fpga, 0);	/* Create DBS module */

	if (res) {
		free(p_nthw_dbs);
		return res;
	}

	p_fpga_info->mp_nthw_dbs = p_nthw_dbs;

	for (i = 0; i < MAX_VIRT_QUEUES; ++i) {
		rxvq[i].usage = NTHW_VIRTQ_UNUSED;
		txvq[i].usage = NTHW_VIRTQ_UNUSED;
	}

	dbs_reset(p_nthw_dbs);

	for (i = 0; i < NT_DBS_RX_QUEUES_MAX; ++i)
		dbs_init_rx_queue(p_nthw_dbs, i, 0, 0);

	for (i = 0; i < NT_DBS_TX_QUEUES_MAX; ++i)
		dbs_init_tx_queue(p_nthw_dbs, i, 0, 0);

	set_rx_control(p_nthw_dbs, LAST_QUEUE, RX_AM_DISABLE, RX_AM_POLL_SPEED, RX_UW_DISABLE,
		RX_UW_POLL_SPEED, RX_Q_DISABLE);
	set_rx_control(p_nthw_dbs, LAST_QUEUE, RX_AM_ENABLE, RX_AM_POLL_SPEED, RX_UW_ENABLE,
		RX_UW_POLL_SPEED, RX_Q_DISABLE);
	set_rx_control(p_nthw_dbs, LAST_QUEUE, RX_AM_ENABLE, RX_AM_POLL_SPEED, RX_UW_ENABLE,
		RX_UW_POLL_SPEED, RX_Q_ENABLE);

	set_tx_control(p_nthw_dbs, LAST_QUEUE, TX_AM_DISABLE, TX_AM_POLL_SPEED, TX_UW_DISABLE,
		TX_UW_POLL_SPEED, TX_Q_DISABLE);
	set_tx_control(p_nthw_dbs, LAST_QUEUE, TX_AM_ENABLE, TX_AM_POLL_SPEED, TX_UW_ENABLE,
		TX_UW_POLL_SPEED, TX_Q_DISABLE);
	set_tx_control(p_nthw_dbs, LAST_QUEUE, TX_AM_ENABLE, TX_AM_POLL_SPEED, TX_UW_ENABLE,
		TX_UW_POLL_SPEED, TX_Q_ENABLE);

	return 0;
}

static struct sg_ops_s sg_ops = {
	.nthw_virt_queue_init = nthw_virt_queue_init
};

void sg_init(void)
{
	NT_LOG(INF, NTNIC, "SG ops initialized");
	register_sg_ops(&sg_ops);
}
