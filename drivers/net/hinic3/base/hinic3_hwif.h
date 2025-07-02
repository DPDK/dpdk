/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC3_HWIF_H_
#define _HINIC3_HWIF_H_

#define HINIC3_WAIT_DOORBELL_AND_OUTBOUND_TIMEOUT 60000
#define HINIC3_PCIE_LINK_DOWN			  0xFFFFFFFF

/* PCIe bar space. */
#define HINIC3_VF_PCI_CFG_REG_BAR 0
#define HINIC3_PF_PCI_CFG_REG_BAR 1

#define HINIC3_PCI_INTR_REG_BAR 2
#define HINIC3_PCI_MGMT_REG_BAR 3 /**< Only PF has mgmt bar. */
#define HINIC3_PCI_DB_BAR	4

/* Doorbell or direct wqe page size is 4K. */
#define HINIC3_DB_PAGE_SIZE 0x00001000ULL
#define HINIC3_DWQE_OFFSET  0x00000800ULL

enum func_type { TYPE_PF, TYPE_VF, TYPE_PPF, TYPE_UNKNOWN };
#define MSIX_RESEND_TIMER_CLEAR 1

/* Message signaled interrupt status. */
enum hinic3_msix_state { HINIC3_MSIX_ENABLE, HINIC3_MSIX_DISABLE };

enum hinic3_msix_auto_mask {
	HINIC3_CLR_MSIX_AUTO_MASK,
	HINIC3_SET_MSIX_AUTO_MASK,
};

struct hinic3_func_attr {
	u16 func_global_idx;
	u8 port_to_port_idx;
	u8 pci_intf_idx;
	u8 vf_in_pf;
	enum func_type func_type;

	u8 mpf_idx;

	u8 ppf_idx;

	u16 num_irqs; /**< Max: 2 ^ 15. */
	u8 num_aeqs;  /**< Max: 2 ^ 3. */
	u8 num_ceqs;  /**< Max: 2 ^ 7. */

	u16 num_queue;	 /**< Max: 2 ^ 8. */
	u8 num_dma_attr; /**< Max: 2 ^ 6. */
	u8 msix_flex_en;

	u16 global_vf_id_of_pf;
};

/* Structure for hardware interface. */
struct hinic3_hwif {
	/* Configure virtual address, PF is bar1, VF is bar0/1. */
	u8 *cfg_regs_base;
	/* For PF bar3 virtual address, if function is VF should set NULL. */
	u8 *mgmt_regs_base;
	u8 *db_base;
	u64 db_dwqe_len;

	struct hinic3_func_attr attr;

	void *pdev;
};

enum hinic3_outbound_ctrl { ENABLE_OUTBOUND = 0x0, DISABLE_OUTBOUND = 0x1 };

enum hinic3_doorbell_ctrl { ENABLE_DOORBELL = 0x0, DISABLE_DOORBELL = 0x1 };

enum hinic3_pf_status {
	HINIC3_PF_STATUS_INIT = 0x0,
	HINIC3_PF_STATUS_ACTIVE_FLAG = 0x11,
	HINIC3_PF_STATUS_FLR_START_FLAG = 0x12,
	HINIC3_PF_STATUS_FLR_FINISH_FLAG = 0x13
};

/* Type of doorbell. */
enum hinic3_db_type {
	HINIC3_DB_TYPE_CMDQ = 0x0,
	HINIC3_DB_TYPE_SQ = 0x1,
	HINIC3_DB_TYPE_RQ = 0x2,
	HINIC3_DB_TYPE_MAX = 0x3
};

/* Get the attributes of the hardware interface. */
#define HINIC3_HWIF_NUM_AEQS(hwif)	   ((hwif)->attr.num_aeqs)
#define HINIC3_HWIF_NUM_IRQS(hwif)	   ((hwif)->attr.num_irqs)
#define HINIC3_HWIF_GLOBAL_IDX(hwif)	   ((hwif)->attr.func_global_idx)
#define HINIC3_HWIF_GLOBAL_VF_OFFSET(hwif) ((hwif)->attr.global_vf_id_of_pf)
#define HINIC3_HWIF_PPF_IDX(hwif)	   ((hwif)->attr.ppf_idx)
#define HINIC3_PCI_INTF_IDX(hwif)	   ((hwif)->attr.pci_intf_idx)

/* Func type judgment. */
#define HINIC3_FUNC_TYPE(dev) ((dev)->hwif->attr.func_type)
#define HINIC3_IS_PF(dev)     (HINIC3_FUNC_TYPE(dev) == TYPE_PF)
#define HINIC3_IS_VF(dev)     (HINIC3_FUNC_TYPE(dev) == TYPE_VF)
#define HINIC3_IS_PPF(dev)    (HINIC3_FUNC_TYPE(dev) == TYPE_PPF)

u32 hinic3_hwif_read_reg(struct hinic3_hwif *hwif, u32 reg);

void hinic3_hwif_write_reg(struct hinic3_hwif *hwif, u32 reg, u32 val);

void hinic3_set_msix_auto_mask_state(void *hwdev, u16 msix_idx,
				     enum hinic3_msix_auto_mask flag);

void hinic3_set_msix_state(void *hwdev, u16 msix_idx,
			   enum hinic3_msix_state flag);

void hinic3_misx_intr_clear_resend_bit(void *hwdev, u16 msix_idx,
				       u8 clear_resend_en);

u16 hinic3_global_func_id(void *hwdev);

u8 hinic3_pf_id_of_vf(void *hwdev);

u8 hinic3_pcie_itf_id(void *hwdev);

enum func_type hinic3_func_type(void *hwdev);

u16 hinic3_glb_pf_vf_offset(void *hwdev);
void hinic3_update_msix_info(struct hinic3_hwif *hwif);
void hinic3_set_pf_status(struct hinic3_hwif *hwif,
			  enum hinic3_pf_status status);

enum hinic3_pf_status hinic3_get_pf_status(struct hinic3_hwif *hwif);

int hinic3_alloc_db_addr(void *hwdev, void **db_base,
			 enum hinic3_db_type queue_type);

void hinic3_disable_doorbell(struct hinic3_hwif *hwif);

void hinic3_enable_doorbell(struct hinic3_hwif *hwif);

int hinic3_init_hwif(void *dev);

void hinic3_free_hwif(void *dev);

#endif /**< _HINIC3_HWIF_H_ */
