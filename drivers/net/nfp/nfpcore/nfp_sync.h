/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2024 Corigine, Inc.
 * All rights reserved.
 */

#ifndef __NFP_SYNC_H__
#define __NFP_SYNC_H__

#include <stdint.h>

#include <bus_pci_driver.h>

#define NFP_SYNC_MAGIC_FL_SERVICE            0x53594e41 /**< ASCII - SYNA */

struct nfp_sync;

struct nfp_sync *nfp_sync_alloc(void);
void nfp_sync_free(struct nfp_sync *sync);

void *nfp_sync_handle_alloc(struct nfp_sync *sync,
		struct rte_pci_device *pci_dev,
		uint32_t magic,
		uint32_t size);
void nfp_sync_handle_free(struct nfp_sync *sync,
		struct rte_pci_device *pci_dev,
		void *handle);
uint16_t nfp_sync_handle_count_get(struct nfp_sync *sync,
		struct rte_pci_device *pci_dev,
		void *handle);

#endif /* __NFP_SYNC_H__ */
