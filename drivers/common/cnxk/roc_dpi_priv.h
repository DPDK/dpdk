/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_DPI_PRIV_H_
#define _ROC_DPI_PRIV_H_

#define DPI_MAX_VFS 8

/* DPI PF DBDF information macros */
#define DPI_PF_DBDF_DEVICE   0
#define DPI_PF_DBDF_FUNCTION 0

#define DPI_QUEUE_OPEN	0x1
#define DPI_QUEUE_CLOSE 0x2
#define DPI_REG_DUMP	0x3
#define DPI_GET_REG_CFG 0x4
#define DPI_QUEUE_OPEN_V2 0x5

#define DPI_QUEUE_IDLE_TMO_MS 1E3
#define DPI_LF_RESET_TMO_US   10000

typedef union dpi_mbox_msg_t {
	uint64_t u[2];
	struct dpi_mbox_message_s {
		/* VF ID to configure */
		uint64_t vfid : 8;
		/* Command code */
		uint64_t cmd : 4;
		/* Command buffer size in 8-byte words */
		uint64_t csize : 16;
		/* aura of the command buffer */
		uint64_t aura : 20;
		/* SSO PF function */
		uint64_t sso_pf_func : 16;
		/* NPA PF function */
		uint64_t npa_pf_func : 16;
		/* WQE queue DMA completion status enable */
		uint64_t wqecs : 1;
		/* WQE queue DMA completion status offset */
		uint64_t wqecsoff : 8;
		/* Priority */
		uint64_t pri : 1;
	} s;
} dpi_mbox_msg_t;

struct dpi {
	struct plt_pci_device *pci_dev;
	struct dev dev;
	uint16_t lf_msix_off[ROC_DPI_MAX_LFS];
	uint8_t lf_blkaddr[ROC_DPI_MAX_LFS];
};

static inline struct dpi *
roc_dpi_to_dpi_priv(struct roc_dpi *roc_dpi)
{
	return (struct dpi *)&roc_dpi->reserved[0];
}

static inline struct roc_dpi *
dpi_priv_to_roc_dpi(struct dpi *dpi)
{
	return (struct roc_dpi *)((char *)dpi - offsetof(struct roc_dpi, reserved));
}

int dpi_lf_reset(struct roc_dpi_lf *lf);
void dpi_lf_ena_dis(struct roc_dpi_lf *lf, uint8_t enb);
int dpi_lfs_attach(struct dev *dev, uint8_t blkaddr, bool modify, uint16_t nb_lf);
int dpi_lfs_detach(struct dev *dev);
int dpi_lf_attach(struct dev *dev, uint8_t blkaddr, bool modify, uint16_t nb_lf);
int dpi_lf_detach(struct dev *dev);
int dpi_lf_init(struct roc_dpi_lf *lf, struct dev *dev, uint8_t slot);
int dpi_chan_tbl_alloc(struct dev *dev, uint8_t blk_addr, uint16_t tbl_sz);
int dpi_chan_tbl_free(struct dev *dev, uint8_t blk_addr, uint16_t tbl_num);
int dpi_chan_tbl_ena_dis(struct dev *dev, uint32_t dpi_blkaddr, uint16_t lfid, uint16_t chan_tbl,
			 bool enable);
int dpi_chan_tbl_update(struct dev *dev, uint8_t blk_addr, uint16_t chan_tbl, uint64_t *tbl,
			uint16_t off, uint16_t nb_entries);

#endif
