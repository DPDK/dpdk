/* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <asm/types.h>
#include <linux/in.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/if_tunnel.h>
#include <linux/filter.h>
#include <linux/bpf.h>

#include "tap_rss.h"

/** Create IPv4 address */
#define IPv4(a, b, c, d) ((__u32)(((a) & 0xff) << 24) | \
		(((b) & 0xff) << 16) | \
		(((c) & 0xff) << 8)  | \
		((d) & 0xff))

#define PORT(a, b) ((__u16)(((a) & 0xff) << 8) | \
		((b) & 0xff))

/*
 * The queue number is offset by a unique QUEUE_OFFSET, to distinguish
 * packets that have gone through this rule (skb->cb[1] != 0) from others.
 */
#define QUEUE_OFFSET		0x7cafe800
#define PIN_GLOBAL_NS		2

#define KEY_IDX			0
#define BPF_MAP_ID_KEY	1

struct vlan_hdr {
	__be16 proto;
	__be16 tci;
};

struct bpf_elf_map __attribute__((section("maps"), used))
map_keys = {
	.type           =       BPF_MAP_TYPE_HASH,
	.id             =       BPF_MAP_ID_KEY,
	.size_key       =       sizeof(__u32),
	.size_value     =       sizeof(struct rss_key),
	.max_elem       =       256,
	.pinning        =       PIN_GLOBAL_NS,
};

__section("cls_q") int
match_q(struct __sk_buff *skb)
{
	__u32 queue = skb->cb[1];
	volatile __u32 q = 0xdeadbeef;
	__u32 match_queue = QUEUE_OFFSET + q;

	/* printt("match_q$i() queue = %d\n", queue); */

	if (queue != match_queue)
		return TC_ACT_OK;

	/* queue match */
	skb->cb[1] = 0;
	return TC_ACT_UNSPEC;
}


struct ipv4_l3_l4_tuple {
	__u32    src_addr;
	__u32    dst_addr;
	__u16    dport;
	__u16    sport;
} __attribute__((packed));

struct ipv6_l3_l4_tuple {
	__u8        src_addr[16];
	__u8        dst_addr[16];
	__u16       dport;
	__u16       sport;
} __attribute__((packed));

static const __u8 def_rss_key[TAP_RSS_HASH_KEY_SIZE] = {
	0xd1, 0x81, 0xc6, 0x2c,
	0xf7, 0xf4, 0xdb, 0x5b,
	0x19, 0x83, 0xa2, 0xfc,
	0x94, 0x3e, 0x1a, 0xdb,
	0xd9, 0x38, 0x9e, 0x6b,
	0xd1, 0x03, 0x9c, 0x2c,
	0xa7, 0x44, 0x99, 0xad,
	0x59, 0x3d, 0x56, 0xd9,
	0xf3, 0x25, 0x3c, 0x06,
	0x2a, 0xdc, 0x1f, 0xfc,
};

static __u32  __attribute__((always_inline))
rte_softrss_be(const __u32 *input_tuple, const uint8_t *rss_key,
		__u8 input_len)
{
	__u32 i, j, hash = 0;
#pragma unroll
	for (j = 0; j < input_len; j++) {
#pragma unroll
		for (i = 0; i < 32; i++) {
			if (input_tuple[j] & (1 << (31 - i))) {
				hash ^= ((const __u32 *)def_rss_key)[j] << i |
				(__u32)((uint64_t)
				(((const __u32 *)def_rss_key)[j + 1])
					>> (32 - i));
			}
		}
	}
	return hash;
}

static int __attribute__((always_inline))
rss_l3_l4(struct __sk_buff *skb)
{
	void *data_end = (void *)(long)skb->data_end;
	void *data = (void *)(long)skb->data;
	__u16 proto = (__u16)skb->protocol;
	__u32 key_idx = 0xdeadbeef;
	__u32 hash;
	struct rss_key *rsskey;
	__u64 off = ETH_HLEN;
	int j;
	__u8 *key = 0;
	__u32 len;
	__u32 queue = 0;

	rsskey = map_lookup_elem(&map_keys, &key_idx);
	if (!rsskey) {
		printt("hash(): rss key is not configured\n");
		return TC_ACT_OK;
	}
	key = (__u8 *)rsskey->key;

	/* Get correct proto for 802.1ad */
	if (skb->vlan_present && skb->vlan_proto == htons(ETH_P_8021AD)) {
		if (data + ETH_ALEN * 2 + sizeof(struct vlan_hdr) +
		    sizeof(proto) > data_end)
			return TC_ACT_OK;
		proto = *(__u16 *)(data + ETH_ALEN * 2 +
				   sizeof(struct vlan_hdr));
		off += sizeof(struct vlan_hdr);
	}

	if (proto == htons(ETH_P_IP)) {
		if (data + off + sizeof(struct iphdr) + sizeof(__u32)
			> data_end)
			return TC_ACT_OK;

		__u8 *src_dst_addr = data + off + offsetof(struct iphdr, saddr);
		__u8 *src_dst_port = data + off + sizeof(struct iphdr);
		struct ipv4_l3_l4_tuple v4_tuple = {
			.src_addr = IPv4(*(src_dst_addr + 0),
					*(src_dst_addr + 1),
					*(src_dst_addr + 2),
					*(src_dst_addr + 3)),
			.dst_addr = IPv4(*(src_dst_addr + 4),
					*(src_dst_addr + 5),
					*(src_dst_addr + 6),
					*(src_dst_addr + 7)),
			.sport = PORT(*(src_dst_port + 0),
					*(src_dst_port + 1)),
			.dport = PORT(*(src_dst_port + 2),
					*(src_dst_port + 3)),
		};
		__u8 input_len = sizeof(v4_tuple) / sizeof(__u32);
		if (rsskey->hash_fields & (1 << HASH_FIELD_IPV4_L3))
			input_len--;
		hash = rte_softrss_be((__u32 *)&v4_tuple, key, 3);
	} else if (proto == htons(ETH_P_IPV6)) {
		if (data + off + sizeof(struct ipv6hdr) +
					sizeof(__u32) > data_end)
			return TC_ACT_OK;
		__u8 *src_dst_addr = data + off +
					offsetof(struct ipv6hdr, saddr);
		__u8 *src_dst_port = data + off +
					sizeof(struct ipv6hdr);
		struct ipv6_l3_l4_tuple v6_tuple;
		for (j = 0; j < 4; j++)
			*((uint32_t *)&v6_tuple.src_addr + j) =
				__builtin_bswap32(*((uint32_t *)
						src_dst_addr + j));
		for (j = 0; j < 4; j++)
			*((uint32_t *)&v6_tuple.dst_addr + j) =
				__builtin_bswap32(*((uint32_t *)
						src_dst_addr + 4 + j));
		v6_tuple.sport = PORT(*(src_dst_port + 0),
			      *(src_dst_port + 1));
		v6_tuple.dport = PORT(*(src_dst_port + 2),
			      *(src_dst_port + 3));

		__u8 input_len = sizeof(v6_tuple) / sizeof(__u32);
		if (rsskey->hash_fields & (1 << HASH_FIELD_IPV6_L3))
			input_len--;
		hash = rte_softrss_be((__u32 *)&v6_tuple, key, 9);
	} else {
		return TC_ACT_PIPE;
	}

	queue = rsskey->queues[(hash % rsskey->nb_queues) &
				       (TAP_MAX_QUEUES - 1)];
	skb->cb[1] = QUEUE_OFFSET + queue;
	/* printt(">>>>> rss_l3_l4 hash=0x%x queue=%u\n", hash, queue); */

	return TC_ACT_RECLASSIFY;
}

#define RSS(L)						\
	__section(#L) int				\
		L ## _hash(struct __sk_buff *skb)	\
	{						\
		return rss_ ## L (skb);			\
	}

RSS(l3_l4)

BPF_LICENSE("Dual BSD/GPL");
