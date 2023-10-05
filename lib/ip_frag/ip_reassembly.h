/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2021 Intel Corporation
 */

#ifndef _IP_REASSEMBLY_H_
#define _IP_REASSEMBLY_H_

/*
 * IP Fragmentation and Reassembly
 * Implementation of IP packet fragmentation and reassembly.
 */

#include <rte_ip_frag.h>

enum {
	IP_LAST_FRAG_IDX,    /* index of last fragment */
	IP_FIRST_FRAG_IDX,   /* index of first fragment */
	IP_MIN_FRAG_NUM,     /* minimum number of fragments */
	IP_MAX_FRAG_NUM = RTE_LIBRTE_IP_FRAG_MAX_FRAG,
	/* maximum number of fragments per packet */
};

/* fragmented mbuf */
struct ip_frag {
	uint16_t ofs;        /* offset into the packet */
	uint16_t len;        /* length of fragment */
	struct rte_mbuf *mb; /* fragment mbuf */
};

/*
 * key: <src addr, dst_addr, id> to uniquely identify fragmented datagram.
 */
struct ip_frag_key {
	uint64_t src_dst[4];
	/* src and dst address, only first 8 bytes used for IPv4 */
	union {
		uint64_t id_key_len; /* combined for easy fetch */
		__extension__
		struct {
			uint32_t id;      /* packet id */
			uint32_t key_len; /* src/dst key length */
		};
	};
};

/*
 * Fragmented packet to reassemble.
 * First two entries in the frags[] array are for the last and first fragments.
 */
struct ip_frag_pkt {
	RTE_TAILQ_ENTRY(ip_frag_pkt) lru;      /* LRU list */
	struct ip_frag_key key;                /* fragmentation key */
	uint64_t start;                        /* creation timestamp */
	uint32_t total_size;                   /* expected reassembled size */
	uint32_t frag_size;                    /* size of fragments received */
	uint32_t last_idx;                     /* index of next entry to fill */
	struct ip_frag frags[IP_MAX_FRAG_NUM]; /* fragments */
} __rte_cache_aligned;

 /* fragments tailq */
RTE_TAILQ_HEAD(ip_pkt_list, ip_frag_pkt);

/* fragmentation table statistics */
struct ip_frag_tbl_stat {
	uint64_t find_num;     /* total # of find/insert attempts. */
	uint64_t add_num;      /* # of add ops. */
	uint64_t del_num;      /* # of del ops. */
	uint64_t reuse_num;    /* # of reuse (del/add) ops. */
	uint64_t fail_total;   /* total # of add failures. */
	uint64_t fail_nospace; /* # of 'no space' add failures. */
} __rte_cache_aligned;

/* fragmentation table */
struct rte_ip_frag_tbl {
	uint64_t max_cycles;     /* ttl for table entries. */
	uint32_t entry_mask;     /* hash value mask. */
	uint32_t max_entries;    /* max entries allowed. */
	uint32_t use_entries;    /* entries in use. */
	uint32_t bucket_entries; /* hash associativity. */
	uint32_t nb_entries;     /* total size of the table. */
	uint32_t nb_buckets;     /* num of associativity lines. */
	struct ip_frag_pkt *last;     /* last used entry. */
	struct ip_pkt_list lru;       /* LRU list for table entries. */
	struct ip_frag_tbl_stat stat; /* statistics counters. */
	__extension__ struct ip_frag_pkt pkt[]; /* hash table. */
};

#endif /* _IP_REASSEMBLY_H_ */
