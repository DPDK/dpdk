/* SPDX-License-Identifier: BSD-3-Clause
 * BPF TX filter program for testing rte_bpf_eth_tx_elf_load
 */

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;

/*
 * Simple TX filter that accepts TCP packets
 *
 * BPF TX programs receive pointer to data and should return:
 *   0 = drop packet
 *   non-zero = rx/tx packet
 *
 * This filter checks:
 * 1. Packet is IPv4
 * 2. Protocol is TCP (IPPROTO_TCP = 6)
 */
__attribute__((section("filter"), used))
uint64_t
test_filter(void *pkt)
{
	uint8_t *data = pkt;

	/* Read version and IHL (first byte of IP header) */
	uint8_t version_ihl = data[14];

	/* Check IPv4 version (upper 4 bits should be 4) */
	if ((version_ihl >> 4) != 4)
		return 0;

	/* Protocol field (byte 9 of IP header) must be TCP (6) */
	uint8_t proto = data[14 + 9];
	return (proto == 6);
}

__attribute__((section("drop"), used))
uint64_t
test_drop(void *pkt)
{
	(void)pkt;
	return 0;
}

__attribute__((section("allow"), used))
uint64_t
test_allow(void *pkt)
{
	(void)pkt;
	return 1;
}
