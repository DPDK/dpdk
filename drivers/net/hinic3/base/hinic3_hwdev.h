/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC3_HWDEV_H_
#define _HINIC3_HWDEV_H_

#include <ethdev_pci.h>
#include "hinic3_mgmt.h"

struct cfg_mgmt_info;

struct hinic3_hwif;
struct hinic3_aeqs;
struct hinic3_mbox;
struct hinic3_msg_pf_to_mgmt;

#define MGMT_VERSION_MAX_LEN 32

enum hinic3_set_arm_type {
	HINIC3_SET_ARM_CMDQ,
	HINIC3_SET_ARM_SQ,
	HINIC3_SET_ARM_TYPE_NUM
};

struct hinic3_page_addr {
	void *virt_addr;
	uint64_t phys_addr;
};

struct ffm_intr_info {
	uint8_t node_id;
	/* Error level of the interrupt source. */
	uint8_t err_level;
	/* Classification by interrupt source properties. */
	uint16_t err_type;
	uint32_t err_csr_addr;
	uint32_t err_csr_value;
};

struct link_event_stats {
	RTE_ATOMIC(int32_t)link_down_stats;
	RTE_ATOMIC(int32_t)link_up_stats;
};

enum hinic3_fault_err_level {
	FAULT_LEVEL_FATAL,
	FAULT_LEVEL_SERIOUS_RESET,
	FAULT_LEVEL_SERIOUS_FLR,
	FAULT_LEVEL_GENERAL,
	FAULT_LEVEL_SUGGESTION,
	FAULT_LEVEL_MAX
};

enum hinic3_fault_type {
	FAULT_TYPE_CHIP,
	FAULT_TYPE_UCODE,
	FAULT_TYPE_MEM_RD_TIMEOUT,
	FAULT_TYPE_MEM_WR_TIMEOUT,
	FAULT_TYPE_REG_RD_TIMEOUT,
	FAULT_TYPE_REG_WR_TIMEOUT,
	FAULT_TYPE_PHY_FAULT,
	FAULT_TYPE_MAX
};

struct fault_event_stats {
	RTE_ATOMIC(int32_t)chip_fault_stats[22][FAULT_LEVEL_MAX];
	RTE_ATOMIC(int32_t)fault_type_stat[FAULT_TYPE_MAX];
	RTE_ATOMIC(int32_t)pcie_fault_stats;
};

struct hinic3_hw_stats {
	RTE_ATOMIC(int32_t)heart_lost_stats;
	struct link_event_stats link_event_stats;
	struct fault_event_stats fault_event_stats;
};

#define HINIC3_CHIP_FAULT_SIZE (110 * 1024)
#define MAX_DRV_BUF_SIZE       4096

struct nic_cmd_chip_fault_stats {
	uint32_t offset;
	uint8_t chip_fault_stats[MAX_DRV_BUF_SIZE];
};

struct hinic3_board_info {
	uint8_t board_type;
	uint8_t port_num;
	uint8_t port_speed;
	uint8_t pcie_width;
	uint8_t host_num;
	uint8_t pf_num;
	uint16_t vf_total_num;
	uint8_t tile_num;
	uint8_t qcm_num;
	uint8_t core_num;
	uint8_t work_mode;
	uint8_t service_mode;
	uint8_t pcie_mode;
	uint8_t boot_sel;
	uint8_t board_id;
	uint32_t cfg_addr;
	uint32_t service_en_bitmap;
	uint8_t scenes_id;
	uint8_t cfg_template_id;
	uint16_t rsvd0;
};

struct hinic3_handler_info {
	uint16_t cmd;
	void *buf_in;
	uint16_t in_size;
	void *buf_out;
	uint16_t *out_size;
	uint16_t dst_func;
	uint16_t direction;
	uint16_t ack_type;
};

struct hinic3_hwdev {
	void *dev_handle; /**< Pointer to hinic3_nic_dev. */
	struct rte_pci_device *pci_dev;	  /**< Pointer to rte_pci_device. */
	struct rte_eth_dev *eth_dev;	  /**< Pointer to rte_eth_dev. */

	uint16_t port_id;

	uint32_t wq_page_size;

	struct hinic3_hwif *hwif;
	struct cfg_mgmt_info *cfg_mgmt;

	struct hinic3_cmdqs *cmdqs;
	struct hinic3_aeqs *aeqs;
	struct hinic3_mbox *func_to_func;
	struct hinic3_msg_pf_to_mgmt *pf_to_mgmt;
	struct hinic3_hw_stats hw_stats;
	uint8_t *chip_fault_stats;

	struct hinic3_board_info board_info;
	char mgmt_ver[MGMT_VERSION_MAX_LEN];

	uint16_t max_vfs;
	uint16_t link_status;
};

bool hinic3_is_vfio_iommu_enable(const struct rte_eth_dev *rte_dev);

int hinic3_vf_handle_pf_comm_mbox(struct hinic3_hwdev *hwdev,
				  __rte_unused void *pri_handle,
				  struct hinic3_handler_info *handler_info);

void hinic3_pf_handle_mgmt_comm_event(struct hinic3_hwdev *hwdev,
				      __rte_unused void *pri_handle,
				      struct hinic3_handler_info *handler_info);

int hinic3_init_hwdev(struct hinic3_hwdev *hwdev);

void hinic3_free_hwdev(struct hinic3_hwdev *hwdev);

const struct rte_memzone *
hinic3_dma_zone_reserve(const struct rte_eth_dev *dev, const char *ring_name,
			uint16_t queue_id, size_t size, unsigned int align, int socket_id);

int hinic3_memzone_free(const struct rte_memzone *mz);

#endif /* _HINIC3_HWDEV_H_ */
