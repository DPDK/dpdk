/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_DPI_H_
#define _ROC_DPI_H_

#define ROC_DPI_LF_RINGS       2
#define ROC_DPI_MAX_LFS	       256
#define ROC_DPI_LF_CHAN_TBL_SZ 4096

struct roc_dpi_lf_ring_cfg {
	uint8_t ring_idx;
	uint8_t xtype;
	uint8_t rport;
	uint8_t wport;
	uint8_t pri;
	uint8_t isize;
};

union roc_dpi_lf_ccfg {
	uint64_t u;
	struct {
		uint64_t vf_func : 12;
		uint64_t pf_func : 4;
		uint64_t st : 8;
		uint64_t rsvd_24_31 : 8;
		uint64_t pasid : 20;
		uint64_t pasid_ctrl : 2;
		uint64_t th : 1;
		uint64_t ph : 2;
		uint64_t rsvd_57_62 : 6;
		uint64_t valid : 1;
	};
	struct {
		uint32_t type;
		uint16_t src_key;
		uint16_t dst_key;
	};
};

struct roc_dpi_lf_que {
	const struct plt_memzone *mz;
	uint64_t *cmd_base;
	struct roc_dpi_lf *lf;
	uint16_t qsize;
	uint16_t widx;
	uint8_t cmd_len;
	uint8_t first_skip;
	uint8_t later_skip;
} __plt_cache_aligned;

struct roc_dpi_lf {
	struct roc_dpi_lf_que queue[ROC_DPI_LF_RINGS];
	uintptr_t rbase;
	struct dev *dev;
	uint16_t chan_tbl;
	uint16_t chan_tbl_sz;
	uint16_t slot;
	uint16_t blk_addr;
	int group_id;
};

struct roc_dpi {
	struct plt_pci_device *pci_dev;
	const struct plt_memzone *mz;
	struct roc_dpi_lf *lfs;
	uint8_t *rbase;
	uint16_t vfid;
	uint8_t priority;
	uint16_t nr_lfs;

#define ROC_DPI_MEM_SZ (4 * 1024)
	uint8_t reserved[ROC_DPI_MEM_SZ] __plt_cache_aligned;
} __plt_cache_aligned;

int __roc_api roc_dpi_dev_init(struct roc_dpi *roc_dpi, uint8_t offset);
int __roc_api roc_dpi_dev_fini(struct roc_dpi *roc_dpi);

int __roc_api roc_dpi_rsrc_init(struct roc_dpi *roc_dpi);
int __roc_api roc_dpi_rsrc_fini(struct roc_dpi *roc_dpi);
int __roc_api roc_dpi_configure(struct roc_dpi *dpi, uint32_t chunk_sz, uint64_t aura,
				uint64_t chunk_base);
int __roc_api roc_dpi_configure_v2(struct roc_dpi *roc_dpi, uint32_t chunk_sz, uint64_t aura,
				   uint64_t chunk_base);
int __roc_api roc_dpi_enable(struct roc_dpi *dpi);
int __roc_api roc_dpi_wait_queue_idle(struct roc_dpi *dpi);
int __roc_api roc_dpi_disable(struct roc_dpi *dpi);
int __roc_api roc_dpi_reset(struct roc_dpi *dpi);

int __roc_api roc_dpi_lf_ring_init(struct roc_dpi_lf_que *que, struct roc_dpi_lf_ring_cfg *rcfg);
void __roc_api roc_dpi_lf_ring_fini(struct roc_dpi_lf_que *que);
int __roc_api roc_dpi_lf_pffunc_cfg(struct roc_dpi_lf *lf);
int __roc_api roc_dpi_lf_ring_chan_cfg(struct roc_dpi_lf_que *que, union roc_dpi_lf_ccfg *cfg);
int __roc_api roc_dpi_lf_chan_tbl_alloc(struct roc_dpi_lf *lf, uint16_t tbl_sz);
int __roc_api roc_dpi_lf_chan_tbl_free(struct roc_dpi_lf *lf);
int __roc_api roc_dpi_lf_chan_tbl_select(struct roc_dpi_lf *lf);
int __roc_api roc_dpi_lf_chan_tbl_ena_dis(struct roc_dpi_lf *lf, bool ena);
int __roc_api roc_dpi_lf_chan_tbl_update(struct roc_dpi_lf *lf, uint64_t *config, uint16_t offset,
					 uint16_t entries);
int __roc_api roc_dpi_lf_dump(struct roc_dpi_lf *lf, FILE *file);

int __roc_api roc_dpi_access_pair_group_create(struct roc_dpi_lf *lf, plt_uuid_t domain_id,
					       plt_uuid_t token, int16_t *group_id);
int __roc_api roc_dpi_access_pair_group_destroy(struct roc_dpi_lf *lf, int16_t group_id);
int __roc_api roc_dpi_access_pair_group_join(struct roc_dpi_lf *lf, plt_uuid_t domain_id,
					     plt_uuid_t token, int16_t group_id);
int __roc_api roc_dpi_access_pair_group_leave(struct roc_dpi_lf *lf, int16_t group_id);
int __roc_api roc_dpi_access_pair_group_handler_get(struct roc_dpi_lf *lf, int16_t group_id,
						    plt_uuid_t domain_id, uint16_t *handler);
#endif
