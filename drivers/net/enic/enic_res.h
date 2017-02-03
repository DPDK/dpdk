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

#ifndef _ENIC_RES_H_
#define _ENIC_RES_H_

#include "wq_enet_desc.h"
#include "rq_enet_desc.h"
#include "vnic_wq.h"
#include "vnic_rq.h"

#define ENIC_MIN_WQ_DESCS		64
#define ENIC_MAX_WQ_DESCS		4096
#define ENIC_MIN_RQ_DESCS		64
#define ENIC_MAX_RQ_DESCS		4096

#define ENIC_MIN_MTU			68

/* Does not include (possible) inserted VLAN tag and FCS */
#define ENIC_DEFAULT_RX_MAX_PKT_SIZE	9022

/* Does not include (possible) inserted VLAN tag and FCS */
#define ENIC_TX_MAX_PKT_SIZE		9208

#define ENIC_MULTICAST_PERFECT_FILTERS	32
#define ENIC_UNICAST_PERFECT_FILTERS	32

#define ENIC_NON_TSO_MAX_DESC		16
#define ENIC_DEFAULT_RX_FREE_THRESH	32
#define ENIC_TX_XMIT_MAX		64

#define ENIC_SETTING(enic, f) ((enic->config.flags & VENETF_##f) ? 1 : 0)


struct enic;

int enic_get_vnic_config(struct enic *);
int enic_add_vlan(struct enic *enic, u16 vlanid);
int enic_del_vlan(struct enic *enic, u16 vlanid);
int enic_set_nic_cfg(struct enic *enic, u8 rss_default_cpu, u8 rss_hash_type,
	u8 rss_hash_bits, u8 rss_base_cpu, u8 rss_enable, u8 tso_ipid_split_en,
	u8 ig_vlan_strip_en);
int enic_set_rss_key(struct enic *enic, dma_addr_t key_pa, u64 len);
int enic_set_rss_cpu(struct enic *enic, dma_addr_t cpu_pa, u64 len);
void enic_get_res_counts(struct enic *enic);
void enic_init_vnic_resources(struct enic *enic);
int enic_alloc_vnic_resources(struct enic *);
void enic_free_vnic_resources(struct enic *);

#endif /* _ENIC_RES_H_ */
