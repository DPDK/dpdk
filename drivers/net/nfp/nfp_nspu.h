/*
 * Copyright (c) 2017 Netronome Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *  contributors may be used to endorse or promote products derived from this
 *  software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * vim:shiftwidth=8:noexpandtab
 *
 * @file dpdk/pmd/nfp_nspu.h
 *
 * Netronome NFP_NET PDM driver
 */

/*
 * NSP is the NFP Service Processor. NSPU is NSP Userspace interface.
 *
 * NFP NSP helps with firmware/hardware configuration. NSP is another component
 * in NFP programmable processor and accessing it from host requires to firstly
 * configure a specific NFP PCI expansion BAR.
 *
 * Once access is ready, configuration can be done reading and writing
 * from/to a specific PF PCI BAR window. This same interface will allow to
 * create other PCI BAR windows for accessing other NFP components.
 *
 * This file includes low-level functions, using the NSPU interface, and high
 * level functions, invoked by the PMD for using NSP services. This allows
 * firmware upload, vNIC PCI BARs mapping and other low-level configurations
 * like link setup.
 *
 * NSP access is done during initialization and it is not involved at all with
 * the fast path.
 */

#include <rte_spinlock.h>
#include "nfp_net_eth.h"

typedef struct {
	int nfp;        /* NFP device */
	int pcie_bar;   /* PF PCI BAR to work with */
	int exp_bar;    /* Expansion BAR number used by NSPU */
	int barsz;      /* PCIE BAR log2 size */
	uint64_t bufaddr;  /* commands buffer address */
	size_t buf_size;   /* commands buffer size */
	uint64_t windowsz; /* NSPU BAR window size */
	void *cfg_base; /* Expansion BARs address */
	void *mem_base; /* NSP interface */
	rte_spinlock_t nsp_lock;
} nspu_desc_t;

int nfp_nspu_init(nspu_desc_t *desc, int nfp, int pcie_bar, size_t pcie_barsz,
		  int exp_bar, void *exp_bar_cfg_base, void *exp_bar_mmap);
int nfp_nsp_get_abi_version(nspu_desc_t *desc, int *major, int *minor);
int nfp_nsp_fw_setup(nspu_desc_t *desc, const char *sym, uint64_t *pcie_offset);
int nfp_nsp_map_ctrl_bar(nspu_desc_t *desc, uint64_t *pcie_offset);
void nfp_nsp_map_queues_bar(nspu_desc_t *desc, uint64_t *pcie_offset);
int nfp_nsp_eth_config(nspu_desc_t *desc, int port, int up);
int nfp_nsp_eth_read_table(nspu_desc_t *desc, union eth_table_entry **table);
