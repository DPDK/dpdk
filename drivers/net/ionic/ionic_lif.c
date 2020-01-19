/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2018-2019 Pensando Systems, Inc. All rights reserved.
 */

#include <rte_malloc.h>
#include <rte_ethdev_driver.h>

#include "ionic.h"
#include "ionic_logs.h"
#include "ionic_lif.h"
#include "ionic_ethdev.h"

static void *
ionic_bus_map_dbpage(struct ionic_adapter *adapter, int page_num)
{
	char *vaddr = adapter->bars[IONIC_PCI_BAR_DBELL].vaddr;

	if (adapter->num_bars <= IONIC_PCI_BAR_DBELL)
		return NULL;

	return (void *)&vaddr[page_num << PAGE_SHIFT];
}

int
ionic_lif_alloc(struct ionic_lif *lif)
{
	struct ionic_adapter *adapter = lif->adapter;
	uint32_t socket_id = rte_socket_id();
	int dbpage_num;

	snprintf(lif->name, sizeof(lif->name), "lif%u", lif->index);

	IONIC_PRINT(DEBUG, "Allocating Lif Info");

	lif->kern_pid = 0;

	dbpage_num = ionic_db_page_num(lif, 0);

	lif->kern_dbpage = ionic_bus_map_dbpage(adapter, dbpage_num);
	if (!lif->kern_dbpage) {
		IONIC_PRINT(ERR, "Cannot map dbpage, aborting");
		return -ENOMEM;
	}

	lif->info_sz = RTE_ALIGN(sizeof(*lif->info), PAGE_SIZE);

	lif->info_z = rte_eth_dma_zone_reserve(lif->eth_dev,
		"lif_info", 0 /* queue_idx*/,
		lif->info_sz, IONIC_ALIGN, socket_id);
	if (!lif->info_z) {
		IONIC_PRINT(ERR, "Cannot allocate lif info memory");
		return -ENOMEM;
	}

	lif->info = lif->info_z->addr;
	lif->info_pa = lif->info_z->iova;

	return 0;
}

void
ionic_lif_free(struct ionic_lif *lif)
{
	if (lif->info) {
		rte_memzone_free(lif->info_z);
		lif->info = NULL;
	}
}

int
ionic_lif_init(struct ionic_lif *lif)
{
	struct ionic_dev *idev = &lif->adapter->idev;
	struct ionic_q_init_comp comp;
	int err;

	ionic_dev_cmd_lif_init(idev, lif->index, lif->info_pa);
	err = ionic_dev_cmd_wait_check(idev, IONIC_DEVCMD_TIMEOUT);
	ionic_dev_cmd_comp(idev, &comp);
	if (err)
		return err;

	lif->hw_index = comp.hw_index;

	lif->state |= IONIC_LIF_F_INITED;

	return 0;
}

void
ionic_lif_deinit(struct ionic_lif *lif)
{
	if (!(lif->state & IONIC_LIF_F_INITED))
		return;

	lif->state &= ~IONIC_LIF_F_INITED;
}

int
ionic_lif_identify(struct ionic_adapter *adapter)
{
	struct ionic_dev *idev = &adapter->idev;
	struct ionic_identity *ident = &adapter->ident;
	int err;
	unsigned int i;
	unsigned int lif_words = sizeof(ident->lif.words) /
		sizeof(ident->lif.words[0]);
	unsigned int cmd_words = sizeof(idev->dev_cmd->data) /
		sizeof(idev->dev_cmd->data[0]);
	unsigned int nwords;

	ionic_dev_cmd_lif_identify(idev, IONIC_LIF_TYPE_CLASSIC,
		IONIC_IDENTITY_VERSION_1);
	err = ionic_dev_cmd_wait_check(idev, IONIC_DEVCMD_TIMEOUT);
	if (err)
		return (err);

	nwords = RTE_MIN(lif_words, cmd_words);
	for (i = 0; i < nwords; i++)
		ident->lif.words[i] = ioread32(&idev->dev_cmd->data[i]);

	IONIC_PRINT(INFO, "capabilities 0x%" PRIx64 " ",
		ident->lif.capabilities);

	IONIC_PRINT(INFO, "eth.max_ucast_filters 0x%" PRIx32 " ",
		ident->lif.eth.max_ucast_filters);
	IONIC_PRINT(INFO, "eth.max_mcast_filters 0x%" PRIx32 " ",
		ident->lif.eth.max_mcast_filters);

	IONIC_PRINT(INFO, "eth.features 0x%" PRIx64 " ",
		ident->lif.eth.config.features);
	IONIC_PRINT(INFO, "eth.queue_count[IONIC_QTYPE_ADMINQ] 0x%" PRIx32 " ",
		ident->lif.eth.config.queue_count[IONIC_QTYPE_ADMINQ]);
	IONIC_PRINT(INFO, "eth.queue_count[IONIC_QTYPE_NOTIFYQ] 0x%" PRIx32 " ",
		ident->lif.eth.config.queue_count[IONIC_QTYPE_NOTIFYQ]);
	IONIC_PRINT(INFO, "eth.queue_count[IONIC_QTYPE_RXQ] 0x%" PRIx32 " ",
		ident->lif.eth.config.queue_count[IONIC_QTYPE_RXQ]);
	IONIC_PRINT(INFO, "eth.queue_count[IONIC_QTYPE_TXQ] 0x%" PRIx32 " ",
		ident->lif.eth.config.queue_count[IONIC_QTYPE_TXQ]);

	return 0;
}

int
ionic_lifs_size(struct ionic_adapter *adapter)
{
	struct ionic_identity *ident = &adapter->ident;
	uint32_t nlifs = ident->dev.nlifs;
	uint32_t nintrs, dev_nintrs = ident->dev.nintrs;

	adapter->max_ntxqs_per_lif =
		ident->lif.eth.config.queue_count[IONIC_QTYPE_TXQ];
	adapter->max_nrxqs_per_lif =
		ident->lif.eth.config.queue_count[IONIC_QTYPE_RXQ];

	nintrs = nlifs * 1 /* notifyq */;

	if (nintrs > dev_nintrs) {
		IONIC_PRINT(ERR, "At most %d intr queues supported, minimum required is %u",
			dev_nintrs, nintrs);
		return -ENOSPC;
	}

	adapter->nintrs = nintrs;

	return 0;
}
