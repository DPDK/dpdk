/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#ifndef _RQ_ENET_DESC_H_
#define _RQ_ENET_DESC_H_

#include <rte_byteorder.h>

/* Ethernet receive queue descriptor: 16B */
struct rq_enet_desc {
	uint64_t address;
	uint16_t length_type;
	uint8_t reserved[6];
};

enum rq_enet_type_types {
	RQ_ENET_TYPE_ONLY_SOP = 0,
	RQ_ENET_TYPE_NOT_SOP = 1,
	RQ_ENET_TYPE_RESV2 = 2,
	RQ_ENET_TYPE_RESV3 = 3,
};

#define RQ_ENET_ADDR_BITS		64
#define RQ_ENET_LEN_BITS		14
#define RQ_ENET_LEN_MASK		((1 << RQ_ENET_LEN_BITS) - 1)
#define RQ_ENET_TYPE_BITS		2
#define RQ_ENET_TYPE_MASK		((1 << RQ_ENET_TYPE_BITS) - 1)

static inline void rq_enet_desc_enc(volatile struct rq_enet_desc *desc,
	uint64_t address, uint8_t type, uint16_t length)
{
	desc->address = rte_cpu_to_le_64(address);
	desc->length_type = rte_cpu_to_le_16((length & RQ_ENET_LEN_MASK) |
		((type & RQ_ENET_TYPE_MASK) << RQ_ENET_LEN_BITS));
}

static inline void rq_enet_desc_dec(struct rq_enet_desc *desc,
	uint64_t *address, uint8_t *type, uint16_t *length)
{
	*address = rte_le_to_cpu_64(desc->address);
	*length = rte_le_to_cpu_16(desc->length_type) & RQ_ENET_LEN_MASK;
	*type = (uint8_t)((rte_le_to_cpu_16(desc->length_type) >>
		RQ_ENET_LEN_BITS) & RQ_ENET_TYPE_MASK);
}

#endif /* _RQ_ENET_DESC_H_ */
