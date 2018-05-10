/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

/*
 * eBPF program sample.
 * Accepts pointer to first segment packet data as an input parameter.
 * analog of tcpdump -s 1 -d 'dst 1.2.3.4 && udp && dst port 5000'
 * (000) ldh      [12]
 * (001) jeq      #0x800           jt 2    jf 12
 * (002) ld       [30]
 * (003) jeq      #0x1020304       jt 4    jf 12
 * (004) ldb      [23]
 * (005) jeq      #0x11            jt 6    jf 12
 * (006) ldh      [20]
 * (007) jset     #0x1fff          jt 12   jf 8
 * (008) ldxb     4*([14]&0xf)
 * (009) ldh      [x + 16]
 * (010) jeq      #0x1388          jt 11   jf 12
 * (011) ret      #1
 * (012) ret      #0
 *
 * To compile:
 * clang -O2 -target bpf -c t1.c
 */

#include <stdint.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

uint64_t
entry(void *pkt)
{
	struct ether_header *ether_header = (void *)pkt;

	if (ether_header->ether_type != __builtin_bswap16(0x0800))
		return 0;

	struct iphdr *iphdr = (void *)(ether_header + 1);
	if (iphdr->protocol != 17 || (iphdr->frag_off & 0x1ffff) != 0 ||
			iphdr->daddr != __builtin_bswap32(0x1020304))
		return 0;

	int hlen = iphdr->ihl * 4;
	struct udphdr *udphdr = (void *)iphdr + hlen;

	if (udphdr->dest !=  __builtin_bswap16(5000))
		return 0;

	return 1;
}
