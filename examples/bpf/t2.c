/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

/*
 * eBPF program sample.
 * Accepts pointer to struct rte_mbuf as an input parameter.
 * cleanup mbuf's vlan_tci and all related RX flags
 * (PKT_RX_VLAN_PKT | PKT_RX_VLAN_STRIPPED).
 * Doesn't touch contents of packet data.
 * To compile:
 * clang -O2 -I${RTE_SDK}/${RTE_TARGET}/include \
 * -target bpf -Wno-int-to-void-pointer-cast -c t2.c
 */

#include <stdint.h>
#include <stddef.h>
#include <rte_config.h>
#include "mbuf.h"

uint64_t
entry(void *pkt)
{
	struct rte_mbuf *mb;

	mb = pkt;
	mb->vlan_tci = 0;
	mb->ol_flags &= ~(PKT_RX_VLAN_PKT | PKT_RX_VLAN_STRIPPED);

	return 1;
}
