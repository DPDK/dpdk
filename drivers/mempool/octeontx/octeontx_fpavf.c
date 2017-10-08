/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium Inc. 2017. All Right reserved.
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

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>

#include <rte_atomic.h>
#include <rte_eal.h>
#include <rte_pci.h>
#include <rte_errno.h>
#include <rte_memory.h>
#include <rte_malloc.h>
#include <rte_spinlock.h>

#include "octeontx_fpavf.h"

struct fpavf_res {
	void		*pool_stack_base;
	void		*bar0;
	uint64_t	stack_ln_ptr;
	uint16_t	domain_id;
	uint16_t	vf_id;	/* gpool_id */
	uint16_t	sz128;	/* Block size in cache lines */
	bool		is_inuse;
};

struct octeontx_fpadev {
	rte_spinlock_t lock;
	uint8_t	total_gpool_cnt;
	struct fpavf_res pool[FPA_VF_MAX];
};

static struct octeontx_fpadev fpadev;

static void
octeontx_fpavf_setup(void)
{
	uint8_t i;
	static bool init_once;

	if (!init_once) {
		rte_spinlock_init(&fpadev.lock);
		fpadev.total_gpool_cnt = 0;

		for (i = 0; i < FPA_VF_MAX; i++) {

			fpadev.pool[i].domain_id = ~0;
			fpadev.pool[i].stack_ln_ptr = 0;
			fpadev.pool[i].sz128 = 0;
			fpadev.pool[i].bar0 = NULL;
			fpadev.pool[i].pool_stack_base = NULL;
			fpadev.pool[i].is_inuse = false;
		}
		init_once = 1;
	}
}

static int
octeontx_fpavf_identify(void *bar0)
{
	uint64_t val;
	uint16_t domain_id;
	uint16_t vf_id;
	uint64_t stack_ln_ptr;

	val = fpavf_read64((void *)((uintptr_t)bar0 +
				FPA_VF_VHAURA_CNT_THRESHOLD(0)));

	domain_id = (val >> 8) & 0xffff;
	vf_id = (val >> 24) & 0xffff;

	stack_ln_ptr = fpavf_read64((void *)((uintptr_t)bar0 +
					FPA_VF_VHPOOL_THRESHOLD(0)));
	if (vf_id >= FPA_VF_MAX) {
		fpavf_log_err("vf_id(%d) greater than max vf (32)\n", vf_id);
		return -1;
	}

	if (fpadev.pool[vf_id].is_inuse) {
		fpavf_log_err("vf_id %d is_inuse\n", vf_id);
		return -1;
	}

	fpadev.pool[vf_id].domain_id = domain_id;
	fpadev.pool[vf_id].vf_id = vf_id;
	fpadev.pool[vf_id].bar0 = bar0;
	fpadev.pool[vf_id].stack_ln_ptr = stack_ln_ptr;

	/* SUCCESS */
	return vf_id;
}

/* FPAVF pcie device aka mempool probe */
static int
fpavf_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	uint8_t *idreg;
	int res;
	struct fpavf_res *fpa;

	RTE_SET_USED(pci_drv);
	RTE_SET_USED(fpa);

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (pci_dev->mem_resource[0].addr == NULL) {
		fpavf_log_err("Empty bars %p ", pci_dev->mem_resource[0].addr);
		return -ENODEV;
	}
	idreg = pci_dev->mem_resource[0].addr;

	octeontx_fpavf_setup();

	res = octeontx_fpavf_identify(idreg);
	if (res < 0)
		return -1;

	fpa = &fpadev.pool[res];
	fpadev.total_gpool_cnt++;
	rte_wmb();

	fpavf_log_dbg("total_fpavfs %d bar0 %p domain %d vf %d stk_ln_ptr 0x%x",
		       fpadev.total_gpool_cnt, fpa->bar0, fpa->domain_id,
		       fpa->vf_id, (unsigned int)fpa->stack_ln_ptr);

	return 0;
}

static const struct rte_pci_id pci_fpavf_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
				PCI_DEVICE_ID_OCTEONTX_FPA_VF)
	},
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver pci_fpavf = {
	.id_table = pci_fpavf_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_IOVA_AS_VA,
	.probe = fpavf_probe,
};

RTE_PMD_REGISTER_PCI(octeontx_fpavf, pci_fpavf);
