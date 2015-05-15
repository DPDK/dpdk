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
#ident "$Id: enic_res.c 171146 2014-05-02 07:08:20Z ssujith $"

#include "enic_compat.h"
#include "rte_ethdev.h"
#include "wq_enet_desc.h"
#include "rq_enet_desc.h"
#include "cq_enet_desc.h"
#include "vnic_resource.h"
#include "vnic_enet.h"
#include "vnic_dev.h"
#include "vnic_wq.h"
#include "vnic_rq.h"
#include "vnic_cq.h"
#include "vnic_intr.h"
#include "vnic_stats.h"
#include "vnic_nic.h"
#include "vnic_rss.h"
#include "enic_res.h"
#include "enic.h"

int enic_get_vnic_config(struct enic *enic)
{
	struct vnic_enet_config *c = &enic->config;
	int err;

	err = vnic_dev_get_mac_addr(enic->vdev, enic->mac_addr);
	if (err) {
		dev_err(enic_get_dev(enic),
			"Error getting MAC addr, %d\n", err);
		return err;
	}

#define GET_CONFIG(m) \
	do { \
		err = vnic_dev_spec(enic->vdev, \
			offsetof(struct vnic_enet_config, m), \
			sizeof(c->m), &c->m); \
		if (err) { \
			dev_err(enic_get_dev(enic), \
				"Error getting %s, %d\n", #m, err); \
			return err; \
		} \
	} while (0)

	GET_CONFIG(flags);
	GET_CONFIG(wq_desc_count);
	GET_CONFIG(rq_desc_count);
	GET_CONFIG(mtu);
	GET_CONFIG(intr_timer_type);
	GET_CONFIG(intr_mode);
	GET_CONFIG(intr_timer_usec);
	GET_CONFIG(loop_tag);
	GET_CONFIG(num_arfs);

	c->wq_desc_count =
		min_t(u32, ENIC_MAX_WQ_DESCS,
		max_t(u32, ENIC_MIN_WQ_DESCS,
		c->wq_desc_count));
	c->wq_desc_count &= 0xffffffe0; /* must be aligned to groups of 32 */

	c->rq_desc_count =
		min_t(u32, ENIC_MAX_RQ_DESCS,
		max_t(u32, ENIC_MIN_RQ_DESCS,
		c->rq_desc_count));
	c->rq_desc_count &= 0xffffffe0; /* must be aligned to groups of 32 */

	if (c->mtu == 0)
		c->mtu = 1500;
	c->mtu = min_t(u16, ENIC_MAX_MTU,
		max_t(u16, ENIC_MIN_MTU,
		c->mtu));

	c->intr_timer_usec = min_t(u32, c->intr_timer_usec,
		vnic_dev_get_intr_coal_timer_max(enic->vdev));

	dev_info(enic_get_dev(enic),
		"vNIC MAC addr %02x:%02x:%02x:%02x:%02x:%02x "
		"wq/rq %d/%d mtu %d\n",
		enic->mac_addr[0], enic->mac_addr[1], enic->mac_addr[2],
		enic->mac_addr[3], enic->mac_addr[4], enic->mac_addr[5],
		c->wq_desc_count, c->rq_desc_count, c->mtu);
	dev_info(enic_get_dev(enic), "vNIC csum tx/rx %s/%s "
		"rss %s intr mode %s type %s timer %d usec "
		"loopback tag 0x%04x\n",
		ENIC_SETTING(enic, TXCSUM) ? "yes" : "no",
		ENIC_SETTING(enic, RXCSUM) ? "yes" : "no",
		ENIC_SETTING(enic, RSS) ? "yes" : "no",
		c->intr_mode == VENET_INTR_MODE_INTX ? "INTx" :
		c->intr_mode == VENET_INTR_MODE_MSI ? "MSI" :
		c->intr_mode == VENET_INTR_MODE_ANY ? "any" :
		"unknown",
		c->intr_timer_type == VENET_INTR_TYPE_MIN ? "min" :
		c->intr_timer_type == VENET_INTR_TYPE_IDLE ? "idle" :
		"unknown",
		c->intr_timer_usec,
		c->loop_tag);

	return 0;
}

int enic_add_vlan(struct enic *enic, u16 vlanid)
{
	u64 a0 = vlanid, a1 = 0;
	int wait = 1000;
	int err;

	err = vnic_dev_cmd(enic->vdev, CMD_VLAN_ADD, &a0, &a1, wait);
	if (err)
		dev_err(enic_get_dev(enic), "Can't add vlan id, %d\n", err);

	return err;
}

int enic_del_vlan(struct enic *enic, u16 vlanid)
{
	u64 a0 = vlanid, a1 = 0;
	int wait = 1000;
	int err;

	err = vnic_dev_cmd(enic->vdev, CMD_VLAN_DEL, &a0, &a1, wait);
	if (err)
		dev_err(enic_get_dev(enic), "Can't delete vlan id, %d\n", err);

	return err;
}

int enic_set_nic_cfg(struct enic *enic, u8 rss_default_cpu, u8 rss_hash_type,
	u8 rss_hash_bits, u8 rss_base_cpu, u8 rss_enable, u8 tso_ipid_split_en,
	u8 ig_vlan_strip_en)
{
	u64 a0, a1;
	u32 nic_cfg;
	int wait = 1000;

	vnic_set_nic_cfg(&nic_cfg, rss_default_cpu,
		rss_hash_type, rss_hash_bits, rss_base_cpu,
		rss_enable, tso_ipid_split_en, ig_vlan_strip_en);

	a0 = nic_cfg;
	a1 = 0;

	return vnic_dev_cmd(enic->vdev, CMD_NIC_CFG, &a0, &a1, wait);
}

int enic_set_rss_key(struct enic *enic, dma_addr_t key_pa, u64 len)
{
	u64 a0 = (u64)key_pa, a1 = len;
	int wait = 1000;

	return vnic_dev_cmd(enic->vdev, CMD_RSS_KEY, &a0, &a1, wait);
}

int enic_set_rss_cpu(struct enic *enic, dma_addr_t cpu_pa, u64 len)
{
	u64 a0 = (u64)cpu_pa, a1 = len;
	int wait = 1000;

	return vnic_dev_cmd(enic->vdev, CMD_RSS_CPU, &a0, &a1, wait);
}

void enic_free_vnic_resources(struct enic *enic)
{
	unsigned int i;

	for (i = 0; i < enic->wq_count; i++)
		vnic_wq_free(&enic->wq[i]);
	for (i = 0; i < enic->rq_count; i++)
		vnic_rq_free(&enic->rq[i]);
	for (i = 0; i < enic->cq_count; i++)
		vnic_cq_free(&enic->cq[i]);
	vnic_intr_free(&enic->intr);
}

void enic_get_res_counts(struct enic *enic)
{
	enic->wq_count = vnic_dev_get_res_count(enic->vdev, RES_TYPE_WQ);
	enic->rq_count = vnic_dev_get_res_count(enic->vdev, RES_TYPE_RQ);
	enic->cq_count = vnic_dev_get_res_count(enic->vdev, RES_TYPE_CQ);
	enic->intr_count = vnic_dev_get_res_count(enic->vdev,
		RES_TYPE_INTR_CTRL);

	dev_info(enic_get_dev(enic),
		"vNIC resources avail: wq %d rq %d cq %d intr %d\n",
		enic->wq_count, enic->rq_count,
		enic->cq_count, enic->intr_count);
}
