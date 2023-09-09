/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_DPI_H_
#define _ROC_DPI_H_

struct roc_dpi {
	struct plt_pci_device *pci_dev;
	uint8_t *rbase;
	uint16_t vfid;
} __plt_cache_aligned;

int __roc_api roc_dpi_dev_init(struct roc_dpi *roc_dpi);
int __roc_api roc_dpi_dev_fini(struct roc_dpi *roc_dpi);

int __roc_api roc_dpi_configure(struct roc_dpi *dpi, uint32_t chunk_sz, uint64_t aura,
				uint64_t chunk_base);
int __roc_api roc_dpi_enable(struct roc_dpi *dpi);
int __roc_api roc_dpi_disable(struct roc_dpi *dpi);

#endif
