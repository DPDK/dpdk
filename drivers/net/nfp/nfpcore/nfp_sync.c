/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2024 Corigine, Inc.
 * All rights reserved.
 */

#include "nfp_sync.h"

#include <rte_dev.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_spinlock.h>
#include <rte_string_fns.h>

#include "nfp_logs.h"

#define NFP_SYNC_ELEMENT_MAX    8
#define NFP_SYNC_PCI_MAX        32

struct nfp_sync_element {
	uint16_t count;
	/** Element ID, use ASCII - SYN<> */
	uint32_t magic;
	void *handle;
};

struct nfp_sync_common {
	char pci_name[PCI_PRI_STR_SIZE + 1];
	uint16_t avail;
	struct nfp_sync_element element[NFP_SYNC_ELEMENT_MAX];
};

struct nfp_sync {
	rte_spinlock_t spinlock;

	uint16_t alloc_count;

	struct nfp_sync_common process;

	struct nfp_sync_common pci[NFP_SYNC_PCI_MAX];

	const struct rte_memzone *mz;
};

struct nfp_sync *
nfp_sync_alloc(void)
{
	uint16_t i;
	struct nfp_sync *sync;
	const struct rte_memzone *mz;

	mz = rte_memzone_lookup("nfp_sync");
	if (mz != NULL) {
		sync = mz->addr;
		sync->alloc_count++;

		return sync;
	}

	mz = rte_memzone_reserve("nfp_sync",  sizeof(*sync), SOCKET_ID_ANY,
			RTE_MEMZONE_SIZE_HINT_ONLY);
	if (mz == NULL)
		return NULL;

	sync = mz->addr;

	memset(sync, 0, sizeof(*sync));

	rte_spinlock_init(&sync->spinlock);
	sync->alloc_count = 1;
	sync->mz = mz;

	for (i = 0; i < NFP_SYNC_PCI_MAX; i++)
		sync->pci[i].avail = NFP_SYNC_ELEMENT_MAX;

	sync->process.avail = NFP_SYNC_ELEMENT_MAX;

	return sync;
}

void
nfp_sync_free(struct nfp_sync *sync)
{
	uint16_t i;

	rte_spinlock_lock(&sync->spinlock);

	sync->alloc_count--;
	if (sync->alloc_count != 0) {
		rte_spinlock_unlock(&sync->spinlock);
		return;
	}

	if (sync->process.avail != NFP_SYNC_ELEMENT_MAX)
		PMD_DRV_LOG(ERR, "Sync process handle residue.");

	for (i = 0; i < NFP_SYNC_PCI_MAX; i++) {
		if (sync->pci[i].avail != NFP_SYNC_ELEMENT_MAX)
			PMD_DRV_LOG(ERR, "Sync %s pci handle residue.",
					sync->pci[i].pci_name);
	}

	rte_spinlock_unlock(&sync->spinlock);

	rte_memzone_free(sync->mz);
}

static void *
nfp_sync_element_alloc(struct nfp_sync_element *element,
		uint32_t magic,
		uint32_t size)
{
	void *handle;

	handle = rte_zmalloc(NULL, size, 0);
	if (handle == NULL)
		return NULL;

	element->handle = handle;
	element->count = 1;
	element->magic = magic;

	return handle;
}

static void
nfp_sync_element_free(struct nfp_sync_element *element,
		void *handle)
{
	element->count--;
	if (element->count != 0)
		return;

	rte_free(handle);
	element->handle = NULL;
	element->magic = 0;
}

static void *
nfp_sync_common_handle_alloc(struct nfp_sync_common *common,
		uint32_t magic,
		uint32_t size)
{
	uint16_t i;
	void *handle = NULL;
	uint16_t avail_slot = NFP_SYNC_ELEMENT_MAX;

	for (i = 0; i < NFP_SYNC_ELEMENT_MAX; i++) {
		if (common->element[i].magic != magic)
			continue;

		common->element[i].count++;

		return common->element[i].handle;
	}

	if (common->avail == 0)
		return NULL;

	for (i = 0; i < NFP_SYNC_ELEMENT_MAX; i++) {
		if (common->element[i].magic == 0) {
			avail_slot = i;
			break;
		}
	}

	handle = nfp_sync_element_alloc(&common->element[avail_slot], magic, size);
	if (handle == NULL)
		return NULL;

	common->avail--;

	return handle;
}

static void
nfp_sync_common_handle_free(struct nfp_sync_common *common,
		void *handle)
{
	uint16_t i;

	if (common->avail == NFP_SYNC_ELEMENT_MAX)
		return;

	for (i = 0; i < NFP_SYNC_ELEMENT_MAX; i++) {
		if (common->element[i].handle == handle)
			break;
	}

	if (i == NFP_SYNC_ELEMENT_MAX)
		return;

	nfp_sync_element_free(&common->element[i], handle);

	if (common->element[i].count == 0)
		common->avail++;
}

static void *
nfp_sync_process_inner_handle_alloc(struct nfp_sync *sync,
		uint32_t magic,
		uint32_t size)
{
	void *handle = NULL;

	rte_spinlock_lock(&sync->spinlock);

	handle = nfp_sync_common_handle_alloc(&sync->process, magic, size);
	if (handle == NULL)
		PMD_DRV_LOG(ERR, "Process handle alloc failed.");

	rte_spinlock_unlock(&sync->spinlock);

	return handle;
}

static void
nfp_sync_process_inner_handle_free(struct nfp_sync *sync,
		void *handle)
{
	rte_spinlock_lock(&sync->spinlock);

	nfp_sync_common_handle_free(&sync->process, handle);

	rte_spinlock_unlock(&sync->spinlock);
}

static uint16_t
nfp_sync_process_handle_count_get(struct nfp_sync *sync,
		void *handle)
{
	uint16_t i;
	uint16_t count = 0;

	rte_spinlock_lock(&sync->spinlock);

	for (i = 0; i < NFP_SYNC_ELEMENT_MAX; i++) {
		if (sync->process.element[i].handle == handle) {
			count = sync->process.element[i].count;
			break;
		}
	}

	rte_spinlock_unlock(&sync->spinlock);

	return count;
}

static void *
nfp_sync_pci_inner_handle_alloc(struct nfp_sync *sync,
		const char *pci_name,
		uint32_t magic,
		uint32_t size)
{
	uint16_t i;
	void *handle = NULL;
	uint16_t pci_avail_id = NFP_SYNC_PCI_MAX;

	rte_spinlock_lock(&sync->spinlock);

	for (i = 0; i < NFP_SYNC_PCI_MAX; i++) {
		if (strcmp(pci_name, sync->pci[i].pci_name) == 0) {
			pci_avail_id = i;
			goto common_alloc;
		}
	}

	for (i = 0; i < NFP_SYNC_PCI_MAX; i++) {
		if (strlen(sync->pci[i].pci_name) == 0) {
			pci_avail_id = i;
			rte_strlcpy(sync->pci[pci_avail_id].pci_name, pci_name, PCI_PRI_STR_SIZE);
			goto common_alloc;
		}
	}

	rte_spinlock_unlock(&sync->spinlock);

	return NULL;

common_alloc:
	handle = nfp_sync_common_handle_alloc(&sync->pci[pci_avail_id],
			magic, size);
	if (handle == NULL)
		PMD_DRV_LOG(ERR, "PCI handle alloc failed.");

	rte_spinlock_unlock(&sync->spinlock);

	return handle;
}

static void
nfp_sync_pci_inner_handle_free(struct nfp_sync *sync,
		const char *pci_name,
		void *handle)
{
	uint16_t i;
	char *name_tmp;

	rte_spinlock_lock(&sync->spinlock);

	for (i = 0; i < NFP_SYNC_PCI_MAX; i++) {
		name_tmp = sync->pci[i].pci_name;
		if (strlen(name_tmp) != 0 && strcmp(pci_name, name_tmp) == 0) {
			nfp_sync_common_handle_free(&sync->pci[i], handle);
			if (sync->pci[i].avail == NFP_SYNC_ELEMENT_MAX)
				name_tmp[0] = 0;
			break;
		}
	}

	rte_spinlock_unlock(&sync->spinlock);
}

static uint16_t
nfp_sync_pci_handle_count_get(struct nfp_sync *sync,
		const char *pci_name,
		void *handle)
{
	uint16_t i;
	uint16_t count = 0;
	struct nfp_sync_common *pci_common;

	rte_spinlock_lock(&sync->spinlock);

	for (i = 0; i < NFP_SYNC_PCI_MAX; i++) {
		if (strcmp(sync->pci[i].pci_name, pci_name) == 0)
			break;
	}

	if (i == NFP_SYNC_PCI_MAX) {
		rte_spinlock_unlock(&sync->spinlock);
		return 0;
	}

	pci_common = &sync->pci[i];

	for (i = 0; i < NFP_SYNC_ELEMENT_MAX; i++) {
		if (pci_common->element[i].handle == handle) {
			count = pci_common->element[i].count;
			break;
		}
	}

	rte_spinlock_unlock(&sync->spinlock);

	return count;
}

void *
nfp_sync_handle_alloc(struct nfp_sync *sync,
		struct rte_pci_device *pci_dev,
		uint32_t magic,
		uint32_t size)
{
	if (pci_dev == NULL)
		return nfp_sync_process_inner_handle_alloc(sync, magic, size);

	return nfp_sync_pci_inner_handle_alloc(sync, pci_dev->device.name,
			magic, size);
}

void
nfp_sync_handle_free(struct nfp_sync *sync,
		struct rte_pci_device *pci_dev,
		void *handle)
{
	if (pci_dev == NULL) {
		nfp_sync_process_inner_handle_free(sync, handle);
		return;
	}

	nfp_sync_pci_inner_handle_free(sync, pci_dev->device.name, handle);
}

uint16_t
nfp_sync_handle_count_get(struct nfp_sync *sync,
		struct rte_pci_device *pci_dev,
		void *handle)
{
	if (pci_dev == NULL)
		return nfp_sync_process_handle_count_get(sync, handle);

	return nfp_sync_pci_handle_count_get(sync, pci_dev->device.name, handle);
}
