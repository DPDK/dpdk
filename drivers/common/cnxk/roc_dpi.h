/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_DPI_H_
#define _ROC_DPI_H_

struct roc_dpi_args {
	uint8_t num_ssegs;
	uint8_t num_dsegs;
	uint8_t comp_type;
	uint8_t direction;
	uint8_t sdevice;
	uint8_t ddevice;
	uint8_t swap;
	uint8_t use_lock : 1;
	uint8_t tt : 7;
	uint16_t func;
	uint16_t grp;
	uint32_t tag;
	uint64_t comp_ptr;
};

struct roc_dpi {
	/* Input parameters */
	struct plt_pci_device *pci_dev;
	/* End of Input parameters */
	const struct plt_memzone *mz;
	uint8_t *rbase;
	uint16_t vfid;
	uint16_t pool_size_m1;
	uint16_t chunk_head;
	uint64_t *chunk_base;
	uint64_t *chunk_next;
	uint64_t aura_handle;
	plt_spinlock_t chunk_lock;
} __plt_cache_aligned;

int __roc_api roc_dpi_dev_init(struct roc_dpi *roc_dpi);
int __roc_api roc_dpi_dev_fini(struct roc_dpi *roc_dpi);

int __roc_api roc_dpi_configure(struct roc_dpi *dpi);
int __roc_api roc_dpi_enable(struct roc_dpi *dpi);
int __roc_api roc_dpi_disable(struct roc_dpi *dpi);

#endif
