/*-
 *   BSD LICENSE
 *
 *   Copyright(c) Broadcom Limited.
 *   All rights reserved.
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
 *     * Neither the name of Broadcom Corporation nor the names of its
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

#include <inttypes.h>
#include <stdbool.h>

#include <rte_dev.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_cycles.h>

#define DRV_MODULE_NAME		"bnxt"
static const char bnxt_version[] =
	"Broadcom Cumulus driver " DRV_MODULE_NAME "\n";

static struct rte_pci_id bnxt_pci_id_map[] = {
#define RTE_PCI_DEV_ID_DECL_BNXT(vend, dev) {RTE_PCI_DEVICE(vend, dev)},
#include "rte_pci_dev_ids.h"
	{.device_id = 0},
};

/*
 * Initialization
 */

static int
bnxt_dev_init(struct rte_eth_dev *eth_dev)
{
	static int version_printed;
	int rc;

	if (version_printed++ == 0)
		RTE_LOG(INFO, PMD, "%s", bnxt_version);

	if (eth_dev->pci_dev->addr.function >= 2 &&
			eth_dev->pci_dev->addr.function < 4) {
		RTE_LOG(ERR, PMD, "Function not enabled %x:\n",
			eth_dev->pci_dev->addr.function);
		rc = -ENOMEM;
		goto error;
	}

	rte_eth_copy_pci_info(eth_dev, eth_dev->pci_dev);
	rc = -EPERM;

error:
	return rc;
}

static int
bnxt_dev_uninit(struct rte_eth_dev *eth_dev __rte_unused) {
	return 0;
}

static struct eth_driver bnxt_rte_pmd = {
	.pci_drv = {
		    .name = "rte_" DRV_MODULE_NAME "_pmd",
		    .id_table = bnxt_pci_id_map,
		    .drv_flags = RTE_PCI_DRV_NEED_MAPPING,
		    },
	.eth_dev_init = bnxt_dev_init,
	.eth_dev_uninit = bnxt_dev_uninit,
	.dev_private_size = 32 /* this must be non-zero apparently */,
};

static int bnxt_rte_pmd_init(const char *name, const char *params __rte_unused)
{
	RTE_LOG(INFO, PMD, "bnxt_rte_pmd_init() called for %s\n", name);
	rte_eth_driver_register(&bnxt_rte_pmd);
	return 0;
}

static struct rte_driver bnxt_pmd_drv = {
	.name = "eth_bnxt",
	.type = PMD_PDEV,
	.init = bnxt_rte_pmd_init,
};

PMD_REGISTER_DRIVER(bnxt_pmd_drv);
