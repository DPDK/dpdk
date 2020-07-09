/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#include <rte_bus_pci.h>

#include "hinic_compat.h"
#include "hinic_csr.h"
#include "hinic_pmd_hwdev.h"
#include "hinic_pmd_hwif.h"

#define HINIC_CFG_REGS_BAR	0
#define HINIC_INTR_MSI_BAR	2
#define HINIC_DB_MEM_BAR	4

#define PAGE_SIZE_4K		0x1000
#define PAGE_SIZE_64K		0x10000

#define	HINIC_MSIX_CNT_RESEND_TIMER_SHIFT	29
#define	HINIC_MSIX_CNT_RESEND_TIMER_MASK	0x7U

#define HINIC_MSIX_CNT_SET(val, member)		\
		(((val) & HINIC_MSIX_CNT_##member##_MASK) << \
		HINIC_MSIX_CNT_##member##_SHIFT)

/**
 * hwif_ready - test if the HW initialization passed
 * @hwdev: the pointer to the private hardware device object
 * Return: 0 - success, negative - failure
 */
static int hwif_ready(struct hinic_hwdev *hwdev)
{
	u32 addr, attr0, attr1;

	addr   = HINIC_CSR_FUNC_ATTR1_ADDR;
	attr1  = hinic_hwif_read_reg(hwdev->hwif, addr);
	if (!HINIC_AF1_GET(attr1, MGMT_INIT_STATUS))
		return -EBUSY;

	addr   = HINIC_CSR_FUNC_ATTR0_ADDR;
	attr0  = hinic_hwif_read_reg(hwdev->hwif, addr);
	if ((HINIC_AF0_GET(attr0, FUNC_TYPE) == TYPE_VF) &&
	     !HINIC_AF1_GET(attr1, PF_INIT_STATUS))
		return -EBUSY;

	return 0;
}

/**
 * set_hwif_attr - set the attributes as members in hwif
 * @hwif: the hardware interface of a pci function device
 * @attr0: the first attribute that was read from the hw
 * @attr1: the second attribute that was read from the hw
 * @attr2: the third attribute that was read from the hw
 */
static void set_hwif_attr(struct hinic_hwif *hwif, u32 attr0, u32 attr1,
			  u32 attr2)
{
	hwif->attr.func_global_idx = HINIC_AF0_GET(attr0, FUNC_GLOBAL_IDX);
	hwif->attr.port_to_port_idx = HINIC_AF0_GET(attr0, P2P_IDX);
	hwif->attr.pci_intf_idx = HINIC_AF0_GET(attr0, PCI_INTF_IDX);
	hwif->attr.vf_in_pf = HINIC_AF0_GET(attr0, VF_IN_PF);
	hwif->attr.func_type = HINIC_AF0_GET(attr0, FUNC_TYPE);

	hwif->attr.ppf_idx = HINIC_AF1_GET(attr1, PPF_IDX);

	hwif->attr.num_aeqs = BIT(HINIC_AF1_GET(attr1, AEQS_PER_FUNC));
	hwif->attr.num_ceqs = BIT(HINIC_AF1_GET(attr1, CEQS_PER_FUNC));
	hwif->attr.num_irqs = BIT(HINIC_AF1_GET(attr1, IRQS_PER_FUNC));
	hwif->attr.num_dma_attr = BIT(HINIC_AF1_GET(attr1, DMA_ATTR_PER_FUNC));

	hwif->attr.global_vf_id_of_pf = HINIC_AF2_GET(attr2,
						      GLOBAL_VF_ID_OF_PF);
}

/**
 * get_hwif_attr - read and set the attributes as members in hwif
 * @hwif: the hardware interface of a pci function device
 */
static void get_hwif_attr(struct hinic_hwif *hwif)
{
	u32 addr, attr0, attr1, attr2;

	addr   = HINIC_CSR_FUNC_ATTR0_ADDR;
	attr0  = hinic_hwif_read_reg(hwif, addr);

	addr   = HINIC_CSR_FUNC_ATTR1_ADDR;
	attr1  = hinic_hwif_read_reg(hwif, addr);

	addr   = HINIC_CSR_FUNC_ATTR2_ADDR;
	attr2  = hinic_hwif_read_reg(hwif, addr);

	set_hwif_attr(hwif, attr0, attr1, attr2);
}

void hinic_set_pf_status(struct hinic_hwif *hwif, enum hinic_pf_status status)
{
	u32 attr5 = HINIC_AF5_SET(status, PF_STATUS);
	u32 addr  = HINIC_CSR_FUNC_ATTR5_ADDR;

	if (hwif->attr.func_type == TYPE_VF) {
		PMD_DRV_LOG(INFO, "VF doesn't support to set attr5");
		return;
	}

	hinic_hwif_write_reg(hwif, addr, attr5);
}

enum hinic_pf_status hinic_get_pf_status(struct hinic_hwif *hwif)
{
	u32 attr5 = hinic_hwif_read_reg(hwif, HINIC_CSR_FUNC_ATTR5_ADDR);

	return HINIC_AF5_GET(attr5, PF_STATUS);
}

static enum hinic_doorbell_ctrl
hinic_get_doorbell_ctrl_status(struct hinic_hwif *hwif)
{
	u32 attr4 = hinic_hwif_read_reg(hwif, HINIC_CSR_FUNC_ATTR4_ADDR);

	return HINIC_AF4_GET(attr4, DOORBELL_CTRL);
}

static enum hinic_outbound_ctrl
hinic_get_outbound_ctrl_status(struct hinic_hwif *hwif)
{
	u32 attr4 = hinic_hwif_read_reg(hwif, HINIC_CSR_FUNC_ATTR4_ADDR);

	return HINIC_AF4_GET(attr4, OUTBOUND_CTRL);
}

void hinic_enable_doorbell(struct hinic_hwif *hwif)
{
	u32 addr, attr4;

	addr = HINIC_CSR_FUNC_ATTR4_ADDR;
	attr4 = hinic_hwif_read_reg(hwif, addr);

	attr4 = HINIC_AF4_CLEAR(attr4, DOORBELL_CTRL);
	attr4 |= HINIC_AF4_SET(ENABLE_DOORBELL, DOORBELL_CTRL);

	hinic_hwif_write_reg(hwif, addr, attr4);
}

void hinic_disable_doorbell(struct hinic_hwif *hwif)
{
	u32 addr, attr4;

	addr = HINIC_CSR_FUNC_ATTR4_ADDR;
	attr4 = hinic_hwif_read_reg(hwif, addr);

	attr4 = HINIC_AF4_CLEAR(attr4, DOORBELL_CTRL);
	attr4 |= HINIC_AF4_SET(DISABLE_DOORBELL, DOORBELL_CTRL);

	hinic_hwif_write_reg(hwif, addr, attr4);
}

/**
 * set_ppf - try to set hwif as ppf and set the type of hwif in this case
 * @hwif: the hardware interface of a pci function device
 */
static void set_ppf(struct hinic_hwif *hwif)
{
	struct hinic_func_attr *attr = &hwif->attr;
	u32 addr, val, ppf_election;

	/* Read Modify Write */
	addr  = HINIC_CSR_PPF_ELECTION_ADDR;

	val = hinic_hwif_read_reg(hwif, addr);
	val = HINIC_PPF_ELECTION_CLEAR(val, IDX);

	ppf_election =  HINIC_PPF_ELECTION_SET(attr->func_global_idx, IDX);
	val |= ppf_election;

	hinic_hwif_write_reg(hwif, addr, val);

	/* Check PPF */
	val = hinic_hwif_read_reg(hwif, addr);

	attr->ppf_idx = HINIC_PPF_ELECTION_GET(val, IDX);
	if (attr->ppf_idx == attr->func_global_idx)
		attr->func_type = TYPE_PPF;
}

static void init_db_area_idx(struct hinic_hwif *hwif)
{
	struct hinic_free_db_area *free_db_area = &hwif->free_db_area;
	u32 db_max_areas = hwif->db_max_areas;
	u32 i;

	for (i = 0; i < db_max_areas; i++)
		free_db_area->db_idx[i] = i;

	free_db_area->alloc_pos = 0;
	free_db_area->return_pos = 0;

	free_db_area->num_free = db_max_areas;

	spin_lock_init(&free_db_area->idx_lock);
}

static int get_db_idx(struct hinic_hwif *hwif, u32 *idx)
{
	struct hinic_free_db_area *free_db_area = &hwif->free_db_area;
	u32 pos;
	u32 pg_idx;

	spin_lock(&free_db_area->idx_lock);

	if (free_db_area->num_free == 0) {
		spin_unlock(&free_db_area->idx_lock);
		return -ENOMEM;
	}

	free_db_area->num_free--;

	pos = free_db_area->alloc_pos++;
	pos &= (hwif->db_max_areas - 1);

	pg_idx = free_db_area->db_idx[pos];

	free_db_area->db_idx[pos] = 0xFFFFFFFF;

	spin_unlock(&free_db_area->idx_lock);

	*idx = pg_idx;

	return 0;
}

static void free_db_idx(struct hinic_hwif *hwif, u32 idx)
{
	struct hinic_free_db_area *free_db_area = &hwif->free_db_area;
	u32 pos;

	spin_lock(&free_db_area->idx_lock);

	pos = free_db_area->return_pos++;
	pos &= (hwif->db_max_areas - 1);

	free_db_area->db_idx[pos] = idx;

	free_db_area->num_free++;

	spin_unlock(&free_db_area->idx_lock);
}

void hinic_free_db_addr(void *hwdev, void __iomem *db_base)
{
	struct hinic_hwif *hwif = ((struct hinic_hwdev *)hwdev)->hwif;
	u32 idx = DB_IDX(db_base, hwif->db_base);

	free_db_idx(hwif, idx);
}

int hinic_alloc_db_addr(void *hwdev, void __iomem **db_base)
{
	struct hinic_hwif *hwif = ((struct hinic_hwdev *)hwdev)->hwif;
	u32 idx;
	int err;

	err = get_db_idx(hwif, &idx);
	if (err)
		return -EFAULT;

	*db_base = hwif->db_base + idx * HINIC_DB_PAGE_SIZE;

	return 0;
}

void hinic_set_msix_state(void *hwdev, u16 msix_idx, enum hinic_msix_state flag)
{
	struct hinic_hwdev *hw = hwdev;
	struct hinic_hwif *hwif = hw->hwif;
	u32 offset = msix_idx * HINIC_PCI_MSIX_ENTRY_SIZE
		+ HINIC_PCI_MSIX_ENTRY_VECTOR_CTRL;
	u32 mask_bits;

	/* vfio-pci does not mmap msi-x vector table to user space,
	 * we can not access the space when kernel driver is vfio-pci
	 */
	if (hw->pcidev_hdl->kdrv == RTE_KDRV_VFIO)
		return;

	mask_bits = readl(hwif->intr_regs_base + offset);
	mask_bits &= ~HINIC_PCI_MSIX_ENTRY_CTRL_MASKBIT;
	if (flag)
		mask_bits |= HINIC_PCI_MSIX_ENTRY_CTRL_MASKBIT;

	writel(mask_bits, hwif->intr_regs_base + offset);
}

static void disable_all_msix(struct hinic_hwdev *hwdev)
{
	u16 num_irqs = hwdev->hwif->attr.num_irqs;
	u16 i;

	for (i = 0; i < num_irqs; i++)
		hinic_set_msix_state(hwdev, i, HINIC_MSIX_DISABLE);
}

/**
 * Wait for up enable or disable doorbell flush finished.
 * @hwif: the hardware interface of a pci function device.
 * @states: Disable or Enable.
 */
int wait_until_doorbell_flush_states(struct hinic_hwif *hwif,
					enum hinic_doorbell_ctrl states)
{
	unsigned long end;
	enum hinic_doorbell_ctrl db_ctrl;

	end = jiffies +
		msecs_to_jiffies(HINIC_WAIT_DOORBELL_AND_OUTBOUND_TIMEOUT);
	do {
		db_ctrl = hinic_get_doorbell_ctrl_status(hwif);
		if (db_ctrl == states)
			return 0;

		rte_delay_ms(1);
	} while (time_before(jiffies, end));

	return -ETIMEDOUT;
}

static int wait_until_doorbell_and_outbound_enabled(struct hinic_hwif *hwif)
{
	unsigned long end;
	enum hinic_doorbell_ctrl db_ctrl;
	enum hinic_outbound_ctrl outbound_ctrl;

	end = jiffies +
		msecs_to_jiffies(HINIC_WAIT_DOORBELL_AND_OUTBOUND_TIMEOUT);
	do {
		db_ctrl = hinic_get_doorbell_ctrl_status(hwif);
		outbound_ctrl = hinic_get_outbound_ctrl_status(hwif);

		if (outbound_ctrl == ENABLE_OUTBOUND &&
		    db_ctrl == ENABLE_DOORBELL)
			return 0;

		rte_delay_ms(1);
	} while (time_before(jiffies, end));

	return -ETIMEDOUT;
}

u16 hinic_global_func_id(void *hwdev)
{
	struct hinic_hwif *hwif = ((struct hinic_hwdev *)hwdev)->hwif;

	return hwif->attr.func_global_idx;
}

enum func_type hinic_func_type(void *hwdev)
{
	struct hinic_hwif *hwif = ((struct hinic_hwdev *)hwdev)->hwif;

	return hwif->attr.func_type;
}

u8 hinic_ppf_idx(void *hwdev)
{
	struct hinic_hwif *hwif = ((struct hinic_hwdev *)hwdev)->hwif;

	return hwif->attr.ppf_idx;
}

/**
 * hinic_dma_attr_entry_num - get number id of DMA attribute table.
 * @hwdev: the pointer to the private hardware device object.
 * Return: The number id of DMA attribute table.
 */
u8 hinic_dma_attr_entry_num(void *hwdev)
{
	struct hinic_hwif *hwif = ((struct hinic_hwdev *)hwdev)->hwif;
	return hwif->attr.num_dma_attr;
}

/**
 * hinic_init_hwif - initialize the hw interface
 * @hwdev: the pointer to the private hardware device object
 * @cfg_reg_base: base physical address of configuration registers
 * @intr_reg_base: base physical address of msi-x vector table
 * @db_base_phy: base physical address of doorbell registers
 * @db_base: base virtual address of doorbell registers
 * @dwqe_mapping: direct wqe io mapping address
 * Return: 0 - success, negative - failure
 */
static int hinic_init_hwif(struct hinic_hwdev *hwdev, void *cfg_reg_base,
		    void *intr_reg_base, u64 db_base_phy,
		    void *db_base, __rte_unused void *dwqe_mapping)
{
	struct hinic_hwif *hwif;
	struct rte_pci_device *pci_dev;
	u64 db_bar_len;
	int err;

	pci_dev = (struct rte_pci_device *)(hwdev->pcidev_hdl);
	db_bar_len = pci_dev->mem_resource[HINIC_DB_MEM_BAR].len;

	hwif = hwdev->hwif;

	hwif->cfg_regs_base = (u8 __iomem *)cfg_reg_base;
	hwif->intr_regs_base = (u8 __iomem *)intr_reg_base;

	hwif->db_base_phy = db_base_phy;
	hwif->db_base = (u8 __iomem *)db_base;
	hwif->db_max_areas = db_bar_len / HINIC_DB_PAGE_SIZE;
	if (hwif->db_max_areas > HINIC_DB_MAX_AREAS)
		hwif->db_max_areas = HINIC_DB_MAX_AREAS;

	init_db_area_idx(hwif);

	get_hwif_attr(hwif);

	err = hwif_ready(hwdev);
	if (err) {
		PMD_DRV_LOG(ERR, "Hwif is not ready");
		goto hwif_ready_err;
	}

	err = wait_until_doorbell_and_outbound_enabled(hwif);
	if (err) {
		PMD_DRV_LOG(ERR, "Hw doorbell/outbound is disabled");
		goto hwif_ready_err;
	}

	if (!HINIC_IS_VF(hwdev))
		set_ppf(hwif);

	/* disable mgmt cpu report any event */
	hinic_set_pf_status(hwdev->hwif, HINIC_PF_STATUS_INIT);

	return 0;

hwif_ready_err:
	spin_lock_deinit(&hwif->free_db_area.idx_lock);

	return err;
}

#define HINIC_HWIF_ATTR_REG_PRINT_NUM        (6)
#define HINIC_HWIF_APICMD_REG_PRINT_NUM      (2)
#define HINIC_HWIF_EQ_REG_PRINT_NUM          (2)

static void hinic_parse_hwif_attr(struct hinic_hwdev *hwdev)
{
	struct hinic_hwif *hwif = hwdev->hwif;

	PMD_DRV_LOG(INFO, "Device %s hwif attribute:", hwdev->pcidev_hdl->name);
	PMD_DRV_LOG(INFO, "func_idx: %u, p2p_idx: %u, pciintf_idx: %u, "
		    "vf_in_pf: %u, ppf_idx: %u, global_vf_id: %u, func_type: %u",
		    hwif->attr.func_global_idx,
		    hwif->attr.port_to_port_idx, hwif->attr.pci_intf_idx,
		    hwif->attr.vf_in_pf, hwif->attr.ppf_idx,
		    hwif->attr.global_vf_id_of_pf, hwif->attr.func_type);
	PMD_DRV_LOG(INFO, "num_aeqs:%u, num_ceqs:%u, num_irqs:%u, dma_attr:%u",
		    hwif->attr.num_aeqs, hwif->attr.num_ceqs,
		    hwif->attr.num_irqs, hwif->attr.num_dma_attr);
}

static void hinic_get_mmio(struct hinic_hwdev *hwdev, void **cfg_regs_base,
			   void **intr_base, void **db_base)
{
	struct rte_pci_device *pci_dev = hwdev->pcidev_hdl;
	uint64_t bar0_size;
	uint64_t bar2_size;
	uint64_t bar0_phy_addr;
	uint64_t pagesize = sysconf(_SC_PAGESIZE);

	*cfg_regs_base = pci_dev->mem_resource[HINIC_CFG_REGS_BAR].addr;
	*intr_base = pci_dev->mem_resource[HINIC_INTR_MSI_BAR].addr;
	*db_base = pci_dev->mem_resource[HINIC_DB_MEM_BAR].addr;

	bar0_size = pci_dev->mem_resource[HINIC_CFG_REGS_BAR].len;
	bar2_size = pci_dev->mem_resource[HINIC_INTR_MSI_BAR].len;

	if (pagesize == PAGE_SIZE_64K && (bar0_size % pagesize != 0)) {
		bar0_phy_addr =
			pci_dev->mem_resource[HINIC_CFG_REGS_BAR].phys_addr;
		if (bar0_phy_addr % pagesize != 0 &&
		(bar0_size + bar2_size <= pagesize) &&
		bar2_size >= bar0_size) {
			*cfg_regs_base = (void *)((uint8_t *)(*intr_base)
				+ bar2_size);
		}
	}
}

void hinic_hwif_res_free(struct hinic_hwdev *hwdev)
{
	rte_free(hwdev->hwif);
	hwdev->hwif = NULL;
}

int hinic_hwif_res_init(struct hinic_hwdev *hwdev)
{
	int err = HINIC_ERROR;
	void *cfg_regs_base, *db_base, *intr_base = NULL;

	/* hinic related init */
	hwdev->hwif = rte_zmalloc("hinic_hwif", sizeof(*hwdev->hwif),
				  RTE_CACHE_LINE_SIZE);
	if (!hwdev->hwif) {
		PMD_DRV_LOG(ERR, "Allocate hwif failed, dev_name: %s",
			    hwdev->pcidev_hdl->name);
		return -ENOMEM;
	}

	hinic_get_mmio(hwdev, &cfg_regs_base, &intr_base, &db_base);

	err = hinic_init_hwif(hwdev, cfg_regs_base,
			      intr_base, 0, db_base, NULL);
	if (err) {
		PMD_DRV_LOG(ERR, "Initialize hwif failed, dev_name: %s",
			    hwdev->pcidev_hdl->name);
		goto init_hwif_err;
	}

	/* disable msix interrupt in hw device */
	disable_all_msix(hwdev);

	/* print hwif attributes */
	hinic_parse_hwif_attr(hwdev);

	return HINIC_OK;

init_hwif_err:
	rte_free(hwdev->hwif);
	hwdev->hwif = NULL;

	return err;
}

/**
 * hinic_misx_intr_clear_resend_bit - clear interrupt resend configuration
 * @hwdev: the hardware interface of a nic device
 * @msix_idx: Index of msix interrupt
 * @clear_resend_en: enable flag of clear resend configuration
 */
void hinic_misx_intr_clear_resend_bit(void *hwdev, u16 msix_idx,
				      u8 clear_resend_en)
{
	struct hinic_hwif *hwif = ((struct hinic_hwdev *)hwdev)->hwif;
	u32 msix_ctrl = 0, addr;

	msix_ctrl = HINIC_MSIX_CNT_SET(clear_resend_en, RESEND_TIMER);

	addr = HINIC_CSR_MSIX_CNT_ADDR(msix_idx);

	hinic_hwif_write_reg(hwif, addr, msix_ctrl);
}
