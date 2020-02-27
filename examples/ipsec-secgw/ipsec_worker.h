/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */
#ifndef _IPSEC_WORKER_H_
#define _IPSEC_WORKER_H_

#include "ipsec.h"

enum pkt_type {
	PKT_TYPE_PLAIN_IPV4 = 1,
	PKT_TYPE_IPSEC_IPV4,
	PKT_TYPE_PLAIN_IPV6,
	PKT_TYPE_IPSEC_IPV6,
	PKT_TYPE_INVALID
};

enum {
	PKT_DROPPED = 0,
	PKT_FORWARDED,
	PKT_POSTED	/* for lookaside case */
};

struct route_table {
	struct rt_ctx *rt4_ctx;
	struct rt_ctx *rt6_ctx;
};

/*
 * Conf required by event mode worker with tx internal port
 */
struct lcore_conf_ev_tx_int_port_wrkr {
	struct ipsec_ctx inbound;
	struct ipsec_ctx outbound;
	struct route_table rt;
} __rte_cache_aligned;

void ipsec_poll_mode_worker(void);

int ipsec_launch_one_lcore(void *args);

#endif /* _IPSEC_WORKER_H_ */
