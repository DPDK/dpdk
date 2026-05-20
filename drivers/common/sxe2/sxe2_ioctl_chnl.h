/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef SXE2_IOCTL_CHNL_H
#define SXE2_IOCTL_CHNL_H

#include <rte_version.h>
#include <bus_pci_driver.h>

#include "sxe2_internal_ver.h"

#define SXE2_COM_INVAL_uint32_t 0xFFFFFFFF

#define SXE2_COM_PCI_OFFSET_SHIFT 40

#define SXE2_COM_PCI_INDEX_TO_OFFSET(index)	((uint64_t)(index) << SXE2_COM_PCI_OFFSET_SHIFT)
#define SXE2_COM_PCI_OFFSET_MASK	(((uint64_t)(1) << SXE2_COM_PCI_OFFSET_SHIFT) - 1)
#define SXE2_COM_PCI_OFFSET_GEN(index, off) ((((uint64_t)(index)) << SXE2_COM_PCI_OFFSET_SHIFT) | \
		(((uint64_t)(off)) & SXE2_COM_PCI_OFFSET_MASK))

#define SXE2_DRV_TRACE_ID_COUNT_MASK 0x003FFFFFFFFFFFFFLLU

#define SXE2_DRV_CMD_DFLT_TIMEOUT (30)

#define SXE2_COM_VER_MAJOR 1
#define SXE2_COM_VER_MINOR 0
#define SXE2_COM_VER       SXE2_MK_VER(SXE2_COM_VER_MAJOR, SXE2_COM_VER_MINOR)

enum SXE2_COM_CMD {
	SXE2_DEVICE_HANDSHAKE = 1,
	SXE2_DEVICE_IO_IRQS_REQ,
	SXE2_DEVICE_EVT_IRQ_REQ,
	SXE2_DEVICE_RST_IRQ_REQ,
	SXE2_DEVICE_EVT_CAUSE_GET,
	SXE2_DEVICE_DMA_MAP,
	SXE2_DEVICE_DMA_UNMAP,
	SXE2_DEVICE_PASSTHROUGH,
	SXE2_DEVICE_MAX,
};

#define SXE2_CMD_TYPE 'S'

#define SXE2_COM_CMD_HANDSHAKE     _IO(SXE2_CMD_TYPE, SXE2_DEVICE_HANDSHAKE)
#define SXE2_COM_CMD_IO_IRQS_REQ   _IO(SXE2_CMD_TYPE, SXE2_DEVICE_IO_IRQS_REQ)
#define SXE2_COM_CMD_EVT_IRQ_REQ   _IO(SXE2_CMD_TYPE, SXE2_DEVICE_EVT_IRQ_REQ)
#define SXE2_COM_CMD_RST_IRQ_REQ   _IO(SXE2_CMD_TYPE, SXE2_DEVICE_RST_IRQ_REQ)
#define SXE2_COM_CMD_EVT_CAUSE_GET _IO(SXE2_CMD_TYPE, SXE2_DEVICE_EVT_CAUSE_GET)
#define SXE2_COM_CMD_DMA_MAP       _IO(SXE2_CMD_TYPE, SXE2_DEVICE_DMA_MAP)
#define SXE2_COM_CMD_DMA_UNMAP     _IO(SXE2_CMD_TYPE, SXE2_DEVICE_DMA_UNMAP)
#define SXE2_COM_CMD_PASSTHROUGH   _IO(SXE2_CMD_TYPE, SXE2_DEVICE_PASSTHROUGH)

enum sxe2_com_cap {
	SXE2_COM_CAP_IOMMU_MAP = 0,
};

struct sxe2_ioctl_cmd_common_hdr {
	uint32_t dpdk_ver;
	uint32_t drv_ver;
	uint32_t msg_len;
	uint32_t cap;
	uint8_t  reserved[32];
};

struct sxe2_drv_cmd_params {
	uint64_t trace_id;
	uint32_t timeout;
	uint32_t opcode;
	uint16_t vsi_id;
	uint16_t repr_id;
	uint32_t req_len;
	uint32_t resp_len;
	void *req_data;
	void *resp_data;
	uint8_t    resv[32];
};

struct sxe2_ioctl_irq_set {
	uint32_t  cnt;
	uint8_t   resv[4];
	uint32_t  base_irq_in_com;
	int32_t *event_fd;
};

enum sxe2_com_event_cause {
	SXE2_COM_EC_LINK_CHG = 0,
	SXE2_COM_SW_MODE_LEGACY,
	SXE2_COM_SW_MODE_SWITCHDEV,
	SXE2_COM_FC_ST_CHANGE,

	SXE2_COM_EC_RESET = 62,
	SXE2_COM_EC_MAX = 63,
};

struct sxe2_ioctl_other_evt_set {
	int32_t eventfd;
	uint8_t  resv[4];
	uint64_t filter_table;
};

struct sxe2_ioctl_other_evt_get {
	uint64_t evt_cause;
	uint8_t  resv[8];
};

struct sxe2_ioctl_reset_sub_set {
	int32_t eventfd;
	uint8_t  resv[4];
};

struct sxe2_ioctl_iommu_dma_map {
	uint64_t vaddr;
	uint64_t iova;
	uint64_t size;
	uint8_t  resv[4];
};

struct sxe2_ioctl_iommu_dma_unmap {
	uint64_t iova;
};

union sxe2_drv_trace_info {
	uint64_t id;
	struct {
		uint64_t count : 54;
		uint64_t cpu_id : 10;
	} sxe2_drv_trace_id_param;
};

#endif /* SXE2_IOCTL_CHNL_H */
