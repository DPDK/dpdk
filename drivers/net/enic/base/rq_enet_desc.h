/*
 * Copyright 2008-2010 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 *
 * Copyright (c) 2014, Cisco Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in
 * the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _RQ_ENET_DESC_H_
#define _RQ_ENET_DESC_H_

/* Ethernet receive queue descriptor: 16B */
struct rq_enet_desc {
	__le64 address;
	__le16 length_type;
	u8 reserved[6];
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
	u64 address, u8 type, u16 length)
{
	desc->address = cpu_to_le64(address);
	desc->length_type = cpu_to_le16((length & RQ_ENET_LEN_MASK) |
		((type & RQ_ENET_TYPE_MASK) << RQ_ENET_LEN_BITS));
}

static inline void rq_enet_desc_dec(struct rq_enet_desc *desc,
	u64 *address, u8 *type, u16 *length)
{
	*address = le64_to_cpu(desc->address);
	*length = le16_to_cpu(desc->length_type) & RQ_ENET_LEN_MASK;
	*type = (u8)((le16_to_cpu(desc->length_type) >> RQ_ENET_LEN_BITS) &
		RQ_ENET_TYPE_MASK);
}

#endif /* _RQ_ENET_DESC_H_ */
