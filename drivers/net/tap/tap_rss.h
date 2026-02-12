/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#ifndef _TAP_RSS_H_
#define _TAP_RSS_H_

/* Size of the map from BPF classid to queue table */
#ifndef TAP_RSS_MAX
#define TAP_RSS_MAX	32
#endif

/* Standard Toeplitz hash key size */
#define TAP_RSS_HASH_KEY_SIZE 40

/* hashed fields for RSS */
enum hash_field {
	HASH_FIELD_IPV4_L3,	/* IPv4 src/dst addr */
	HASH_FIELD_IPV4_L3_L4,	/* IPv4 src/dst addr + L4 src/dst ports */
	HASH_FIELD_IPV6_L3,	/* IPv6 src/dst addr */
	HASH_FIELD_IPV6_L3_L4,	/* IPv6 src/dst addr + L4 src/dst ports */
};

struct rss_key {
	__u32 hash_fields;
	__u8 key[TAP_RSS_HASH_KEY_SIZE];
	__u32 nb_queues;
	__u32 queues[TAP_MAX_QUEUES];
} __attribute__((packed));

#endif /* _TAP_RSS_H_ */
