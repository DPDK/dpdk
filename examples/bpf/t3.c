/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

/*
 * eBPF program sample.
 * Accepts pointer to struct rte_mbuf as an input parameter.
 * Dump the mbuf into stdout if it is an ARP packet (aka tcpdump 'arp').
 *
 * To compile on x86:
 * clang -O2 -U __GNUC__ -target bpf -Wno-int-to-void-pointer-cast -c t3.c
 *
 * To compile on ARM:
 * clang -O2 -I/usr/include/aarch64-linux-gnu -target bpf \
 * -Wno-int-to-void-pointer-cast -c t3.c
 *
 * NOTE: if DPDK is not installed system-wide, add compiler flag with path
 * to DPDK rte_mbuf.h file to above commands,
 * e.g. "clang -I/path/to/dpdk/headers -O2 ..."
 */

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <net/ethernet.h>
#include <rte_config.h>
#include <rte_mbuf_core.h>
#include <arpa/inet.h>

extern void rte_pktmbuf_dump(FILE *, const struct rte_mbuf *, unsigned int);

uint64_t
entry(const void *pkt)
{
	const struct rte_mbuf *mb;
	const struct ether_header *eth;

	mb = pkt;
	eth = rte_pktmbuf_mtod(mb, const struct ether_header *);

	if (eth->ether_type == htons(ETHERTYPE_ARP))
		rte_pktmbuf_dump(stdout, mb, 64);

	return 1;
}
