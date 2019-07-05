/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _DPI_RAWDEV_H_
#define _DPI_RAWDEV_H_

#define DPI_QUEUE_OPEN	0x1
#define DPI_QUEUE_CLOSE	0x2

/* DPI VF register offsets from VF_BAR0 */
#define DPI_VDMA_EN             (0x0)
#define DPI_VDMA_REQQ_CTL       (0x8)
#define DPI_VDMA_DBELL          (0x10)
#define DPI_VDMA_SADDR          (0x18)
#define DPI_VDMA_COUNTS         (0x20)
#define DPI_VDMA_NADDR          (0x28)
#define DPI_VDMA_IWBUSY         (0x30)
#define DPI_VDMA_CNT            (0x38)
#define DPI_VF_INT              (0x100)
#define DPI_VF_INT_W1S          (0x108)
#define DPI_VF_INT_ENA_W1C      (0x110)
#define DPI_VF_INT_ENA_W1S      (0x118)

#define DPI_MAX_VFS             8
#define DPI_DMA_CMD_SIZE        64
#define DPI_CHUNK_SIZE		1024
#define DPI_QUEUE_STOP		0x0
#define DPI_QUEUE_START		0x1

struct dpi_vf_s {
	struct rte_pci_device *dev;
	uint8_t state;
	uint16_t vf_id;
	uint8_t domain;
	uintptr_t vf_bar0;
	uintptr_t vf_bar2;

	uint16_t pool_size_m1;
	uint16_t index;
	uint64_t *base_ptr;
	void *chunk_pool;
	struct otx2_mbox *mbox;
};

struct dpi_rawdev_conf_s {
	void *chunk_pool;
};

enum dpi_dma_queue_result_e {
	DPI_DMA_QUEUE_SUCCESS = 0,
	DPI_DMA_QUEUE_NO_MEMORY = -1,
	DPI_DMA_QUEUE_INVALID_PARAM = -2,
};

#endif /* _DPI_RAWDEV_H_ */
