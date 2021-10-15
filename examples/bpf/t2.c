/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

/*
 * eBPF program sample.
 * Accepts pointer to struct rte_mbuf as an input parameter.
 * cleanup mbuf's vlan_tci and all related RX flags
 * (RTE_MBUF_F_RX_VLAN_PKT | RTE_MBUF_F_RX_VLAN_STRIPPED).
 * Doesn't touch contents of packet data.
 * To compile:
 * clang -O2 -target bpf -Wno-int-to-void-pointer-cast -c t2.c
 *
 * NOTE: if DPDK is not installed system-wide, add compiler flag with path
 * to DPDK rte_mbuf.h file, e.g. "clang -I/path/to/dpdk/headers -O2 ..."
 */

#include <stdint.h>
#include <stddef.h>
#include <rte_config.h>
#include <rte_mbuf_core.h>

uint64_t
entry(void *pkt)
{
	struct rte_mbuf *mb;

	mb = pkt;
	mb->vlan_tci = 0;
	mb->ol_flags &= ~(RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED);

	return 1;
}
