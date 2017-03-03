/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium networks Ltd. 2017.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Cavium networks nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <rte_atomic.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_io.h>
#include <rte_pci.h>

#include "ssovf_evdev.h"

struct ssovf_res {
	uint16_t domain;
	uint16_t vfid;
	void *bar0;
	void *bar2;
};

struct ssowvf_res {
	uint16_t domain;
	uint16_t vfid;
	void *bar0;
	void *bar2;
	void *bar4;
};

struct ssowvf_identify {
	uint16_t domain;
	uint16_t vfid;
};

struct ssodev {
	uint8_t total_ssovfs;
	uint8_t total_ssowvfs;
	struct ssovf_res grp[SSO_MAX_VHGRP];
	struct ssowvf_res hws[SSO_MAX_VHWS];
};

static struct ssodev sdev;

/* Interface functions */
int
octeontx_ssovf_info(struct octeontx_ssovf_info *info)
{
	uint8_t i;
	uint16_t domain;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY || info == NULL)
		return -EINVAL;

	if (sdev.total_ssovfs == 0 || sdev.total_ssowvfs == 0)
		return -ENODEV;

	domain = sdev.grp[0].domain;
	for (i = 0; i < sdev.total_ssovfs; i++) {
		/* Check vfid's are contiguous and belong to same domain */
		if (sdev.grp[i].vfid != i ||
			sdev.grp[i].bar0 == NULL ||
			sdev.grp[i].domain != domain) {
			ssovf_log_err("GRP error, vfid=%d/%d domain=%d/%d %p",
				i, sdev.grp[i].vfid,
				domain, sdev.grp[i].domain,
				sdev.grp[i].bar0);
			return -EINVAL;
		}
	}

	for (i = 0; i < sdev.total_ssowvfs; i++) {
		/* Check vfid's are contiguous and belong to same domain */
		if (sdev.hws[i].vfid != i ||
			sdev.hws[i].bar0 == NULL ||
			sdev.hws[i].domain != domain) {
			ssovf_log_err("HWS error, vfid=%d/%d domain=%d/%d %p",
				i, sdev.hws[i].vfid,
				domain, sdev.hws[i].domain,
				sdev.hws[i].bar0);
			return -EINVAL;
		}
	}

	info->domain = domain;
	info->total_ssovfs = sdev.total_ssovfs;
	info->total_ssowvfs = sdev.total_ssowvfs;
	return 0;
}

void*
octeontx_ssovf_bar(enum octeontx_ssovf_type type, uint8_t id, uint8_t bar)
{
	if (rte_eal_process_type() != RTE_PROC_PRIMARY ||
			type > OCTEONTX_SSO_HWS)
		return NULL;

	if (type == OCTEONTX_SSO_GROUP) {
		if (id >= sdev.total_ssovfs)
			return NULL;
	} else {
		if (id >= sdev.total_ssowvfs)
			return NULL;
	}

	if (type == OCTEONTX_SSO_GROUP) {
		switch (bar) {
		case 0:
			return sdev.grp[id].bar0;
		case 2:
			return sdev.grp[id].bar2;
		default:
			return NULL;
		}
	} else {
		switch (bar) {
		case 0:
			return sdev.hws[id].bar0;
		case 2:
			return sdev.hws[id].bar2;
		case 4:
			return sdev.hws[id].bar4;
		default:
			return NULL;
		}
	}
}

/* SSOWVF pcie device aka event port probe */

static int
ssowvf_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	uint16_t vfid;
	struct ssowvf_res *res;
	struct ssowvf_identify *id;

	RTE_SET_USED(pci_drv);

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (pci_dev->mem_resource[0].addr == NULL ||
			pci_dev->mem_resource[2].addr == NULL ||
			pci_dev->mem_resource[4].addr == NULL) {
		ssovf_log_err("Empty bars %p %p %p",
				pci_dev->mem_resource[0].addr,
				pci_dev->mem_resource[2].addr,
				pci_dev->mem_resource[4].addr);
		return -ENODEV;
	}

	if (pci_dev->mem_resource[4].len != SSOW_BAR4_LEN) {
		ssovf_log_err("Bar4 len mismatch %d != %d",
			SSOW_BAR4_LEN, (int)pci_dev->mem_resource[4].len);
		return -EINVAL;
	}

	id = pci_dev->mem_resource[4].addr;
	vfid = id->vfid;
	if (vfid >= SSO_MAX_VHWS) {
		ssovf_log_err("Invalid vfid(%d/%d)", vfid, SSO_MAX_VHWS);
		return -EINVAL;
	}

	res = &sdev.hws[vfid];
	res->vfid = vfid;
	res->bar0 = pci_dev->mem_resource[0].addr;
	res->bar2 = pci_dev->mem_resource[2].addr;
	res->bar4 = pci_dev->mem_resource[4].addr;
	res->domain = id->domain;

	sdev.total_ssowvfs++;
	rte_wmb();
	ssovf_log_dbg("Domain=%d hws=%d total_ssowvfs=%d", res->domain,
			res->vfid, sdev.total_ssowvfs);
	return 0;
}

static const struct rte_pci_id pci_ssowvf_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
				PCI_DEVICE_ID_OCTEONTX_SSOWS_VF)
	},
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver pci_ssowvf = {
	.id_table = pci_ssowvf_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = ssowvf_probe,
};

RTE_PMD_REGISTER_PCI(octeontx_ssowvf, pci_ssowvf);

/* SSOVF pcie device aka event queue probe */

static int
ssovf_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	uint64_t val;
	uint16_t vfid;
	uint8_t *idreg;
	struct ssovf_res *res;

	RTE_SET_USED(pci_drv);

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (pci_dev->mem_resource[0].addr == NULL ||
			pci_dev->mem_resource[2].addr == NULL) {
		ssovf_log_err("Empty bars %p %p",
			pci_dev->mem_resource[0].addr,
			pci_dev->mem_resource[2].addr);
		return -ENODEV;
	}
	idreg = pci_dev->mem_resource[0].addr;
	idreg += SSO_VHGRP_AQ_THR;
	val = rte_read64(idreg);

	/* Write back the default value of aq_thr */
	rte_write64((1ULL << 33) - 1, idreg);
	vfid = (val >> 16) & 0xffff;
	if (vfid >= SSO_MAX_VHGRP) {
		ssovf_log_err("Invalid vfid (%d/%d)", vfid, SSO_MAX_VHGRP);
		return -EINVAL;
	}

	res = &sdev.grp[vfid];
	res->vfid = vfid;
	res->bar0 = pci_dev->mem_resource[0].addr;
	res->bar2 = pci_dev->mem_resource[2].addr;
	res->domain = val & 0xffff;

	sdev.total_ssovfs++;
	rte_wmb();
	ssovf_log_dbg("Domain=%d group=%d total_ssovfs=%d", res->domain,
			res->vfid, sdev.total_ssovfs);
	return 0;
}

static const struct rte_pci_id pci_ssovf_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
				PCI_DEVICE_ID_OCTEONTX_SSOGRP_VF)
	},
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver pci_ssovf = {
	.id_table = pci_ssovf_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = ssovf_probe,
};

RTE_PMD_REGISTER_PCI(octeontx_ssovf, pci_ssovf);
