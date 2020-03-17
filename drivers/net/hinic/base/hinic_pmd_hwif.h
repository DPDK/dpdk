/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC_PMD_HWIF_H_
#define _HINIC_PMD_HWIF_H_

#define HINIC_WAIT_DOORBELL_AND_OUTBOUND_TIMEOUT	30000

#define HINIC_HWIF_NUM_AEQS(hwif)		((hwif)->attr.num_aeqs)
#define HINIC_HWIF_NUM_CEQS(hwif)		((hwif)->attr.num_ceqs)
#define HINIC_HWIF_NUM_IRQS(hwif)		((hwif)->attr.num_irqs)
#define HINIC_HWIF_GLOBAL_IDX(hwif)		((hwif)->attr.func_global_idx)
#define HINIC_HWIF_GLOBAL_VF_OFFSET(hwif) ((hwif)->attr.global_vf_id_of_pf)
#define HINIC_HWIF_PPF_IDX(hwif)		((hwif)->attr.ppf_idx)
#define HINIC_PCI_INTF_IDX(hwif)		((hwif)->attr.pci_intf_idx)

#define HINIC_FUNC_TYPE(dev)		((dev)->hwif->attr.func_type)
#define HINIC_IS_PF(dev)		(HINIC_FUNC_TYPE(dev) == TYPE_PF)
#define HINIC_IS_VF(dev)		(HINIC_FUNC_TYPE(dev) == TYPE_VF)
#define HINIC_IS_PPF(dev)		(HINIC_FUNC_TYPE(dev) == TYPE_PPF)

enum func_type {
	TYPE_PF,
	TYPE_VF,
	TYPE_PPF,
};

enum hinic_msix_state {
	HINIC_MSIX_ENABLE,
	HINIC_MSIX_DISABLE,
};

/* Defines the IRQ information structure */
struct irq_info {
	u16 msix_entry_idx; /* IRQ corresponding index number */
	u32 irq_id;         /* the IRQ number from OS */
};

struct hinic_free_db_area {
	u32		db_idx[HINIC_DB_MAX_AREAS];

	u32		num_free;

	u32		alloc_pos;
	u32		return_pos;
	/* spinlock for idx */
	spinlock_t	idx_lock;
};

struct hinic_func_attr {
	u16			func_global_idx;
	u8			port_to_port_idx;
	u8			pci_intf_idx;
	u8			vf_in_pf;
	enum func_type		func_type;

	u8			mpf_idx;

	u8			ppf_idx;

	u16			num_irqs;	/* max: 2 ^ 15 */
	u8			num_aeqs;	/* max: 2 ^ 3 */
	u8			num_ceqs;	/* max: 2 ^ 7 */

	u8			num_dma_attr;	/* max: 2 ^ 6 */

	u16			global_vf_id_of_pf;
};

struct hinic_hwif {
	u8 __iomem			*cfg_regs_base;
	u8 __iomem			*intr_regs_base;
	u64				db_base_phy;
	u8 __iomem			*db_base;
	u64				db_max_areas;
	struct hinic_free_db_area	free_db_area;
	struct hinic_func_attr		attr;
};

static inline u32 hinic_hwif_read_reg(struct hinic_hwif *hwif, u32 reg)
{
	return be32_to_cpu(readl(hwif->cfg_regs_base + reg));
}

static inline void
hinic_hwif_write_reg(struct hinic_hwif *hwif, u32 reg, u32 val)
{
	writel(cpu_to_be32(val), hwif->cfg_regs_base + reg);
}

u16 hinic_global_func_id(void *hwdev);	/* func_attr.glb_func_idx */

enum func_type hinic_func_type(void *hwdev);

void hinic_set_pf_status(struct hinic_hwif *hwif, enum hinic_pf_status status);

enum hinic_pf_status hinic_get_pf_status(struct hinic_hwif *hwif);

void hinic_enable_doorbell(struct hinic_hwif *hwif);

void hinic_disable_doorbell(struct hinic_hwif *hwif);

int hinic_alloc_db_addr(void *hwdev, void __iomem **db_base);

void hinic_free_db_addr(void *hwdev, void __iomem *db_base);

int wait_until_doorbell_flush_states(struct hinic_hwif *hwif,
					enum hinic_doorbell_ctrl states);

void hinic_set_msix_state(void *hwdev, u16 msix_idx,
			  enum hinic_msix_state flag);

void hinic_misx_intr_clear_resend_bit(void *hwdev, u16 msix_idx,
				      u8 clear_resend_en);

u8 hinic_ppf_idx(void *hwdev);

int hinic_hwif_res_init(struct hinic_hwdev *hwdev);

void hinic_hwif_res_free(struct hinic_hwdev *hwdev);

u8 hinic_dma_attr_entry_num(void *hwdev);

#endif /* _HINIC_PMD_HWIF_H_ */
