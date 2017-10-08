/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium Inc. 2017. All rights reserved.
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
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include <rte_eal.h>
#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_pci.h>
#include <rte_spinlock.h>

#include "../octeontx_logs.h"
#include "octeontx_io.h"
#include "octeontx_pkovf.h"

struct octeontx_pko_iomem {
	uint8_t		*va;
	phys_addr_t	iova;
	size_t		size;
};

#define PKO_IOMEM_NULL (struct octeontx_pko_iomem){0, 0, 0}

struct octeontx_pko_fc_ctl_s {
	int64_t buf_cnt;
	int64_t padding[(PKO_DQ_FC_STRIDE / 8) - 1];
};

struct octeontx_pkovf {
	uint8_t		*bar0;
	uint8_t		*bar2;
	uint16_t	domain;
	uint16_t	vfid;
};

struct octeontx_pko_vf_ctl_s {
	rte_spinlock_t lock;

	struct octeontx_pko_iomem fc_iomem;
	struct octeontx_pko_fc_ctl_s *fc_ctl;
	struct octeontx_pkovf pko[PKO_VF_MAX];
	struct {
		uint64_t chanid;
	} dq_map[PKO_VF_MAX * PKO_VF_NUM_DQ];
};

static struct octeontx_pko_vf_ctl_s pko_vf_ctl;

static void
octeontx_pkovf_setup(void)
{
	static bool init_once;

	if (!init_once) {
		unsigned int i;

		rte_spinlock_init(&pko_vf_ctl.lock);

		pko_vf_ctl.fc_iomem = PKO_IOMEM_NULL;
		pko_vf_ctl.fc_ctl = NULL;

		for (i = 0; i < PKO_VF_MAX; i++) {
			pko_vf_ctl.pko[i].bar0 = NULL;
			pko_vf_ctl.pko[i].bar2 = NULL;
			pko_vf_ctl.pko[i].domain = ~(uint16_t)0;
			pko_vf_ctl.pko[i].vfid = ~(uint16_t)0;
		}

		for (i = 0; i < (PKO_VF_MAX * PKO_VF_NUM_DQ); i++)
			pko_vf_ctl.dq_map[i].chanid = 0;

		init_once = true;
	}
}

/* PKOVF pcie device*/
static int
pkovf_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	uint64_t val;
	uint16_t vfid;
	uint16_t domain;
	uint8_t *bar0;
	uint8_t *bar2;
	struct octeontx_pkovf *res;

	RTE_SET_USED(pci_drv);

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (pci_dev->mem_resource[0].addr == NULL ||
	    pci_dev->mem_resource[2].addr == NULL) {
		octeontx_log_err("Empty bars %p %p",
			pci_dev->mem_resource[0].addr,
			pci_dev->mem_resource[2].addr);
		return -ENODEV;
	}
	bar0 = pci_dev->mem_resource[0].addr;
	bar2 = pci_dev->mem_resource[2].addr;

	octeontx_pkovf_setup();

	/* get vfid and domain */
	val = octeontx_read64(bar0 + PKO_VF_DQ_FC_CONFIG);
	domain = (val >> 7) & 0xffff;
	vfid = (val >> 23) & 0xffff;

	if (unlikely(vfid >= PKO_VF_MAX)) {
		octeontx_log_err("pko: Invalid vfid %d", vfid);
		return -EINVAL;
	}

	res = &pko_vf_ctl.pko[vfid];
	res->vfid = vfid;
	res->domain = domain;
	res->bar0 = bar0;
	res->bar2 = bar2;

	octeontx_log_dbg("Domain=%d group=%d", res->domain, res->vfid);
	return 0;
}

#define PCI_VENDOR_ID_CAVIUM               0x177D
#define PCI_DEVICE_ID_OCTEONTX_PKO_VF      0xA049

static const struct rte_pci_id pci_pkovf_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
				PCI_DEVICE_ID_OCTEONTX_PKO_VF)
	},
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver pci_pkovf = {
	.id_table = pci_pkovf_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = pkovf_probe,
};

RTE_PMD_REGISTER_PCI(octeontx_pkovf, pci_pkovf);
