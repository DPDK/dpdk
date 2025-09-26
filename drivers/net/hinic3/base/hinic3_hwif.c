/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include "hinic3_compat.h"
#include "hinic3_csr.h"
#include "hinic3_hwif.h"

#define WAIT_HWIF_READY_TIMEOUT 10000

#define DB_IDX(db, db_base) \
	((uint32_t)(((ulong)(db) - (ulong)(db_base)) / HINIC3_DB_PAGE_SIZE))

#define HINIC3_AF0_FUNC_GLOBAL_IDX_SHIFT 0
#define HINIC3_AF0_P2P_IDX_SHIFT	 12
#define HINIC3_AF0_PCI_INTF_IDX_SHIFT	 17
#define HINIC3_AF0_VF_IN_PF_SHIFT	 20
#define HINIC3_AF0_FUNC_TYPE_SHIFT	 28

#define HINIC3_AF0_FUNC_GLOBAL_IDX_MASK 0xFFF
#define HINIC3_AF0_P2P_IDX_MASK		0x1F
#define HINIC3_AF0_PCI_INTF_IDX_MASK	0x7
#define HINIC3_AF0_VF_IN_PF_MASK	0xFF
#define HINIC3_AF0_FUNC_TYPE_MASK	0x1

#define HINIC3_AF0_GET(val, member) \
	(((val) >> HINIC3_AF0_##member##_SHIFT) & HINIC3_AF0_##member##_MASK)

#define HINIC3_AF1_PPF_IDX_SHIFT	  0
#define HINIC3_AF1_AEQS_PER_FUNC_SHIFT	  8
#define HINIC3_AF1_MGMT_INIT_STATUS_SHIFT 30
#define HINIC3_AF1_PF_INIT_STATUS_SHIFT	  31

#define HINIC3_AF1_PPF_IDX_MASK		 0x3F
#define HINIC3_AF1_AEQS_PER_FUNC_MASK	 0x3
#define HINIC3_AF1_MGMT_INIT_STATUS_MASK 0x1
#define HINIC3_AF1_PF_INIT_STATUS_MASK	 0x1

#define HINIC3_AF1_GET(val, member) \
	(((val) >> HINIC3_AF1_##member##_SHIFT) & HINIC3_AF1_##member##_MASK)

#define HINIC3_AF2_CEQS_PER_FUNC_SHIFT	   0
#define HINIC3_AF2_DMA_ATTR_PER_FUNC_SHIFT 9
#define HINIC3_AF2_IRQS_PER_FUNC_SHIFT	   16

#define HINIC3_AF2_CEQS_PER_FUNC_MASK	  0x1FF
#define HINIC3_AF2_DMA_ATTR_PER_FUNC_MASK 0x7
#define HINIC3_AF2_IRQS_PER_FUNC_MASK	  0x7FF

#define HINIC3_AF2_GET(val, member) \
	(((val) >> HINIC3_AF2_##member##_SHIFT) & HINIC3_AF2_##member##_MASK)

#define HINIC3_AF3_GLOBAL_VF_ID_OF_NXT_PF_SHIFT 0
#define HINIC3_AF3_GLOBAL_VF_ID_OF_PF_SHIFT	16

#define HINIC3_AF3_GLOBAL_VF_ID_OF_NXT_PF_MASK 0xFFF
#define HINIC3_AF3_GLOBAL_VF_ID_OF_PF_MASK     0xFFF

#define HINIC3_AF3_GET(val, member) \
	(((val) >> HINIC3_AF3_##member##_SHIFT) & HINIC3_AF3_##member##_MASK)

#define HINIC3_AF4_DOORBELL_CTRL_SHIFT 0
#define HINIC3_AF4_DOORBELL_CTRL_MASK  0x1

#define HINIC3_AF4_GET(val, member) \
	(((val) >> HINIC3_AF4_##member##_SHIFT) & HINIC3_AF4_##member##_MASK)

#define HINIC3_AF4_SET(val, member) \
	(((val) & HINIC3_AF4_##member##_MASK) << HINIC3_AF4_##member##_SHIFT)

#define HINIC3_AF4_CLEAR(val, member) \
	((val) & (~(HINIC3_AF4_##member##_MASK << HINIC3_AF4_##member##_SHIFT)))

#define HINIC3_AF5_OUTBOUND_CTRL_SHIFT 0
#define HINIC3_AF5_OUTBOUND_CTRL_MASK  0x1

#define HINIC3_AF5_GET(val, member) \
	(((val) >> HINIC3_AF5_##member##_SHIFT) & HINIC3_AF5_##member##_MASK)

#define HINIC3_AF5_SET(val, member) \
	(((val) & HINIC3_AF5_##member##_MASK) << HINIC3_AF5_##member##_SHIFT)

#define HINIC3_AF5_CLEAR(val, member) \
	((val) & (~(HINIC3_AF5_##member##_MASK << HINIC3_AF5_##member##_SHIFT)))

#define HINIC3_AF6_PF_STATUS_SHIFT 0
#define HINIC3_AF6_PF_STATUS_MASK  0xFFFF

#define HINIC3_AF6_FUNC_MAX_QUEUE_SHIFT 23
#define HINIC3_AF6_FUNC_MAX_QUEUE_MASK	0x1FF

#define HINIC3_AF6_MSIX_FLEX_EN_SHIFT 22
#define HINIC3_AF6_MSIX_FLEX_EN_MASK  0x1

#define HINIC3_AF6_SET(val, member)                  \
	((((uint32_t)(val)) & HINIC3_AF6_##member##_MASK) \
	 << HINIC3_AF6_##member##_SHIFT)

#define HINIC3_AF6_GET(val, member) \
	(((val) >> HINIC3_AF6_##member##_SHIFT) & HINIC3_AF6_##member##_MASK)

#define HINIC3_AF6_CLEAR(val, member) \
	((val) & (~(HINIC3_AF6_##member##_MASK << HINIC3_AF6_##member##_SHIFT)))

#define HINIC3_PPF_ELECTION_IDX_SHIFT 0

#define HINIC3_PPF_ELECTION_IDX_MASK 0x3F

#define HINIC3_PPF_ELECTION_SET(val, member)           \
	(((val) & HINIC3_PPF_ELECTION_##member##_MASK) \
	 << HINIC3_PPF_ELECTION_##member##_SHIFT)

#define HINIC3_PPF_ELECTION_GET(val, member)               \
	(((val) >> HINIC3_PPF_ELECTION_##member##_SHIFT) & \
	 HINIC3_PPF_ELECTION_##member##_MASK)

#define HINIC3_PPF_ELECTION_CLEAR(val, member)          \
	((val) & (~(HINIC3_PPF_ELECTION_##member##_MASK \
		    << HINIC3_PPF_ELECTION_##member##_SHIFT)))

#define HINIC3_MPF_ELECTION_IDX_SHIFT 0

#define HINIC3_MPF_ELECTION_IDX_MASK 0x1F

#define HINIC3_MPF_ELECTION_SET(val, member)           \
	(((val) & HINIC3_MPF_ELECTION_##member##_MASK) \
	 << HINIC3_MPF_ELECTION_##member##_SHIFT)

#define HINIC3_MPF_ELECTION_GET(val, member)               \
	(((val) >> HINIC3_MPF_ELECTION_##member##_SHIFT) & \
	 HINIC3_MPF_ELECTION_##member##_MASK)

#define HINIC3_MPF_ELECTION_CLEAR(val, member)          \
	((val) & (~(HINIC3_MPF_ELECTION_##member##_MASK \
		    << HINIC3_MPF_ELECTION_##member##_SHIFT)))

#define HINIC3_GET_REG_FLAG(reg) ((reg) & (~(HINIC3_REGS_FLAG_MASK)))

#define HINIC3_GET_REG_ADDR(reg) ((reg) & (HINIC3_REGS_FLAG_MASK))

#define HINIC3_IS_VF_DEV(pdev) ((pdev)->id.device_id == HINIC3_DEV_ID_VF)

uint32_t
hinic3_hwif_read_reg(struct hinic3_hwif *hwif, uint32_t reg)
{
	if (HINIC3_GET_REG_FLAG(reg) == HINIC3_MGMT_REGS_FLAG)
		return rte_be_to_cpu_32(rte_read32(hwif->mgmt_regs_base +
					HINIC3_GET_REG_ADDR(reg)));
	else
		return rte_be_to_cpu_32(rte_read32(hwif->cfg_regs_base +
					HINIC3_GET_REG_ADDR(reg)));
}

void
hinic3_hwif_write_reg(struct hinic3_hwif *hwif, uint32_t reg, uint32_t val)
{
	if (HINIC3_GET_REG_FLAG(reg) == HINIC3_MGMT_REGS_FLAG)
		rte_write32(rte_cpu_to_be_32(val),
			    hwif->mgmt_regs_base + HINIC3_GET_REG_ADDR(reg));
	else
		rte_write32(rte_cpu_to_be_32(val),
			    hwif->cfg_regs_base + HINIC3_GET_REG_ADDR(reg));
}

/**
 * Judge whether HW initialization ok.
 *
 * @param[in] hwdev
 * The pointer to the private hardware device.
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hwif_ready(struct hinic3_hwdev *hwdev)
{
	uint32_t addr, attr1;

	addr = HINIC3_CSR_FUNC_ATTR1_ADDR;
	attr1 = hinic3_hwif_read_reg(hwdev->hwif, addr);
	if (attr1 == HINIC3_PCIE_LINK_DOWN)
		return -EBUSY;

	if (!HINIC3_AF1_GET(attr1, MGMT_INIT_STATUS))
		return -EBUSY;

	return 0;
}

static int
wait_hwif_ready(struct hinic3_hwdev *hwdev)
{
	ulong timeout = 0;

	do {
		if (!hwif_ready(hwdev))
			return 0;

		rte_delay_ms(1);
		timeout++;
	} while (timeout <= WAIT_HWIF_READY_TIMEOUT);

	PMD_DRV_LOG(ERR, "Hwif is not ready ,dev_name: %s", hwdev->eth_dev->data->name);
	return -EBUSY;
}

/**
 * set the attributes as members in hwif
 *
 * @param[in] hwif
 * The hardware interface of a pci function device.
 */
static void
set_hwif_attr(struct hinic3_hwif *hwif, uint32_t attr0, uint32_t attr1, uint32_t attr2,
	      uint32_t attr3)
{
	hwif->attr.func_global_idx = HINIC3_AF0_GET(attr0, FUNC_GLOBAL_IDX);
	hwif->attr.port_to_port_idx = HINIC3_AF0_GET(attr0, P2P_IDX);
	hwif->attr.pci_intf_idx = HINIC3_AF0_GET(attr0, PCI_INTF_IDX);
	hwif->attr.vf_in_pf = HINIC3_AF0_GET(attr0, VF_IN_PF);
	hwif->attr.func_type = HINIC3_AF0_GET(attr0, FUNC_TYPE);

	hwif->attr.ppf_idx = HINIC3_AF1_GET(attr1, PPF_IDX);
	hwif->attr.num_aeqs = RTE_BIT32(HINIC3_AF1_GET(attr1, AEQS_PER_FUNC));

	hwif->attr.num_ceqs = (uint8_t)HINIC3_AF2_GET(attr2, CEQS_PER_FUNC);
	hwif->attr.num_irqs = HINIC3_AF2_GET(attr2, IRQS_PER_FUNC);
	hwif->attr.num_dma_attr = RTE_BIT32(HINIC3_AF2_GET(attr2, DMA_ATTR_PER_FUNC));

	hwif->attr.global_vf_id_of_pf = HINIC3_AF3_GET(attr3, GLOBAL_VF_ID_OF_PF);
}

/**
 * Read and set the attributes as members in hwif.
 *
 * @param[in] hwif
 * The hardware interface of a pci function device.
 */
static void
update_hwif_attr(struct hinic3_hwif *hwif)
{
	uint32_t addr, attr0, attr1, attr2, attr3;

	addr = HINIC3_CSR_FUNC_ATTR0_ADDR;
	attr0 = hinic3_hwif_read_reg(hwif, addr);

	addr = HINIC3_CSR_FUNC_ATTR1_ADDR;
	attr1 = hinic3_hwif_read_reg(hwif, addr);

	addr = HINIC3_CSR_FUNC_ATTR2_ADDR;
	attr2 = hinic3_hwif_read_reg(hwif, addr);

	addr = HINIC3_CSR_FUNC_ATTR3_ADDR;
	attr3 = hinic3_hwif_read_reg(hwif, addr);

	set_hwif_attr(hwif, attr0, attr1, attr2, attr3);
}

/**
 * Update message signaled interrupt information.
 *
 * @param[in] hwif
 * The hardware interface of a pci function device.
 */
void
hinic3_update_msix_info(struct hinic3_hwif *hwif)
{
	uint32_t attr6 = hinic3_hwif_read_reg(hwif, HINIC3_CSR_FUNC_ATTR6_ADDR);
	hwif->attr.num_queue = HINIC3_AF6_GET(attr6, FUNC_MAX_QUEUE);
	hwif->attr.msix_flex_en = HINIC3_AF6_GET(attr6, MSIX_FLEX_EN);
	PMD_DRV_LOG(DEBUG, "msix_flex_en: %u, queue msix: %u",
		    hwif->attr.msix_flex_en, hwif->attr.num_queue);
}

void
hinic3_set_pf_status(struct hinic3_hwif *hwif, enum hinic3_pf_status status)
{
	uint32_t attr6 = hinic3_hwif_read_reg(hwif, HINIC3_CSR_FUNC_ATTR6_ADDR);

	attr6 = HINIC3_AF6_CLEAR(attr6, PF_STATUS);
	attr6 |= HINIC3_AF6_SET(status, PF_STATUS);

	if (hwif->attr.func_type == TYPE_VF)
		return;

	hinic3_hwif_write_reg(hwif, HINIC3_CSR_FUNC_ATTR6_ADDR, attr6);
}

enum hinic3_pf_status
hinic3_get_pf_status(struct hinic3_hwif *hwif)
{
	uint32_t attr6 = hinic3_hwif_read_reg(hwif, HINIC3_CSR_FUNC_ATTR6_ADDR);

	return HINIC3_AF6_GET(attr6, PF_STATUS);
}

static enum hinic3_doorbell_ctrl
hinic3_get_doorbell_ctrl_status(struct hinic3_hwif *hwif)
{
	uint32_t attr4 = hinic3_hwif_read_reg(hwif, HINIC3_CSR_FUNC_ATTR4_ADDR);

	return HINIC3_AF4_GET(attr4, DOORBELL_CTRL);
}

static enum hinic3_outbound_ctrl
hinic3_get_outbound_ctrl_status(struct hinic3_hwif *hwif)
{
	uint32_t attr5 = hinic3_hwif_read_reg(hwif, HINIC3_CSR_FUNC_ATTR5_ADDR);

	return HINIC3_AF5_GET(attr5, OUTBOUND_CTRL);
}

void
hinic3_enable_doorbell(struct hinic3_hwif *hwif)
{
	uint32_t addr, attr4;

	addr = HINIC3_CSR_FUNC_ATTR4_ADDR;
	attr4 = hinic3_hwif_read_reg(hwif, addr);

	attr4 = HINIC3_AF4_CLEAR(attr4, DOORBELL_CTRL);
	attr4 |= HINIC3_AF4_SET(ENABLE_DOORBELL, DOORBELL_CTRL);

	hinic3_hwif_write_reg(hwif, addr, attr4);
}

void
hinic3_disable_doorbell(struct hinic3_hwif *hwif)
{
	uint32_t addr, attr4;

	addr = HINIC3_CSR_FUNC_ATTR4_ADDR;
	attr4 = hinic3_hwif_read_reg(hwif, addr);

	attr4 = HINIC3_AF4_CLEAR(attr4, DOORBELL_CTRL);
	attr4 |= HINIC3_AF4_SET(DISABLE_DOORBELL, DOORBELL_CTRL);

	hinic3_hwif_write_reg(hwif, addr, attr4);
}

/**
 * Try to set hwif as ppf and set the type of hwif in this case.
 *
 * @param[in] hwif
 * The hardware interface of a pci function device
 */
static void
set_ppf(struct hinic3_hwif *hwif)
{
	struct hinic3_func_attr *attr = &hwif->attr;
	uint32_t addr, val, ppf_election;

	addr = HINIC3_CSR_PPF_ELECTION_ADDR;

	val = hinic3_hwif_read_reg(hwif, addr);
	val = HINIC3_PPF_ELECTION_CLEAR(val, IDX);

	ppf_election = HINIC3_PPF_ELECTION_SET(attr->func_global_idx, IDX);
	val |= ppf_election;

	hinic3_hwif_write_reg(hwif, addr, val);

	/* Check PPF. */
	val = hinic3_hwif_read_reg(hwif, addr);

	attr->ppf_idx = HINIC3_PPF_ELECTION_GET(val, IDX);
	if (attr->ppf_idx == attr->func_global_idx)
		attr->func_type = TYPE_PPF;
}

/**
 * Get the mpf index from the hwif.
 *
 * @param[in] hwif
 * The hardware interface of a pci function device.
 */
static void
get_mpf(struct hinic3_hwif *hwif)
{
	struct hinic3_func_attr *attr = &hwif->attr;
	uint32_t mpf_election, addr;

	addr = HINIC3_CSR_GLOBAL_MPF_ELECTION_ADDR;

	mpf_election = hinic3_hwif_read_reg(hwif, addr);
	attr->mpf_idx = HINIC3_MPF_ELECTION_GET(mpf_election, IDX);
}

/**
 * Try to set hwif as mpf and set the mpf idx in hwif.
 *
 * @param[in] hwif
 * The hardware interface of a pci function device.
 */
static void
set_mpf(struct hinic3_hwif *hwif)
{
	struct hinic3_func_attr *attr = &hwif->attr;
	uint32_t addr, val, mpf_election;

	addr = HINIC3_CSR_GLOBAL_MPF_ELECTION_ADDR;

	val = hinic3_hwif_read_reg(hwif, addr);

	val = HINIC3_MPF_ELECTION_CLEAR(val, IDX);
	mpf_election = HINIC3_MPF_ELECTION_SET(attr->func_global_idx, IDX);

	val |= mpf_election;
	hinic3_hwif_write_reg(hwif, addr, val);
}

int
hinic3_alloc_db_addr(struct hinic3_hwdev *hwdev, void **db_base,
		     enum hinic3_db_type queue_type)
{
	struct hinic3_hwif *hwif = NULL;

	if (!hwdev || !db_base)
		return -EINVAL;

	hwif = hwdev->hwif;
	*db_base = hwif->db_base + queue_type * HINIC3_DB_PAGE_SIZE;

	return 0;
}

void
hinic3_set_msix_auto_mask_state(struct hinic3_hwdev *hwdev, uint16_t msix_idx,
				enum hinic3_msix_auto_mask flag)
{
	struct hinic3_hwif *hwif = NULL;
	uint32_t mask_bits;
	uint32_t addr;

	if (!hwdev)
		return;

	hwif = hwdev->hwif;

	if (flag)
		mask_bits = HINIC3_MSI_CLR_INDIR_SET(1, AUTO_MSK_SET);
	else
		mask_bits = HINIC3_MSI_CLR_INDIR_SET(1, AUTO_MSK_CLR);

	mask_bits = mask_bits |
		    HINIC3_MSI_CLR_INDIR_SET(msix_idx, SIMPLE_INDIR_IDX);

	addr = HINIC3_CSR_FUNC_MSI_CLR_WR_ADDR;
	hinic3_hwif_write_reg(hwif, addr, mask_bits);
}

/**
 * Set msix state.
 *
 * @param[in] hwdev
 * The pointer to the private hardware device object.
 * @param[in] msix_idx
 * MSIX(Message Signaled Interrupts) index.
 * @param[in] flag
 * MSIX state flag, 0-enable, 1-disable.
 */
void
hinic3_set_msix_state(struct hinic3_hwdev *hwdev, uint16_t msix_idx, enum hinic3_msix_state flag)
{
	struct hinic3_hwif *hwif = NULL;
	uint32_t mask_bits;
	uint32_t addr;
	uint8_t int_msk = 1;

	if (!hwdev)
		return;

	hwif = hwdev->hwif;

	if (flag)
		mask_bits = HINIC3_MSI_CLR_INDIR_SET(int_msk, INT_MSK_SET);
	else
		mask_bits = HINIC3_MSI_CLR_INDIR_SET(int_msk, INT_MSK_CLR);
	mask_bits = mask_bits |
		    HINIC3_MSI_CLR_INDIR_SET(msix_idx, SIMPLE_INDIR_IDX);

	addr = HINIC3_CSR_FUNC_MSI_CLR_WR_ADDR;
	hinic3_hwif_write_reg(hwif, addr, mask_bits);
}

static void
disable_all_msix(struct hinic3_hwdev *hwdev)
{
	uint16_t num_irqs = hwdev->hwif->attr.num_irqs;
	uint16_t i;

	for (i = 0; i < num_irqs; i++)
		hinic3_set_msix_state(hwdev, i, HINIC3_MSIX_DISABLE);
}

/**
 * Clear msix resend bit.
 *
 * @param[in] hwdev
 * The pointer to the private hardware device object.
 * @param[in] msix_idx
 * MSIX(Message Signaled Interrupts) index
 * @param[in] clear_resend_en
 * Clear resend en flag, 1-clear.
 */
void
hinic3_misx_intr_clear_resend_bit(struct hinic3_hwdev *hwdev,
				  uint16_t msix_idx, uint8_t clear_resend_en)
{
	struct hinic3_hwif *hwif = NULL;
	uint32_t msix_ctrl = 0, addr;

	if (!hwdev)
		return;

	hwif = hwdev->hwif;

	msix_ctrl = HINIC3_MSI_CLR_INDIR_SET(msix_idx, SIMPLE_INDIR_IDX) |
		    HINIC3_MSI_CLR_INDIR_SET(clear_resend_en, RESEND_TIMER_CLR);

	addr = HINIC3_CSR_FUNC_MSI_CLR_WR_ADDR;
	hinic3_hwif_write_reg(hwif, addr, msix_ctrl);
}

static int
wait_until_doorbell_and_outbound_enabled(struct hinic3_hwif *hwif)
{
	enum hinic3_doorbell_ctrl db_ctrl;
	enum hinic3_outbound_ctrl outbound_ctrl;
	uint32_t cnt = 0;

	while (cnt < HINIC3_WAIT_DOORBELL_AND_OUTBOUND_TIMEOUT) {
		db_ctrl = hinic3_get_doorbell_ctrl_status(hwif);
		outbound_ctrl = hinic3_get_outbound_ctrl_status(hwif);
		if (outbound_ctrl == ENABLE_OUTBOUND &&
		    db_ctrl == ENABLE_DOORBELL)
			return 0;

		rte_delay_ms(1);
		cnt++;
	}

	return -EFAULT;
}

static int
hinic3_get_bar_addr(struct hinic3_hwdev *hwdev)
{
	struct rte_pci_device *pci_dev = hwdev->pci_dev;
	struct hinic3_hwif *hwif = hwdev->hwif;
	void *cfg_regs_base = NULL;
	void *mgmt_reg_base = NULL;
	void *db_base = NULL;
	int cfg_bar;

	cfg_bar = HINIC3_IS_VF_DEV(pci_dev) ? HINIC3_VF_PCI_CFG_REG_BAR
					    : HINIC3_PF_PCI_CFG_REG_BAR;
	cfg_regs_base = pci_dev->mem_resource[cfg_bar].addr;

	if (cfg_regs_base == NULL) {
		PMD_DRV_LOG(ERR,
			    "mem_resource addr is null, cfg_regs_base is NULL");
		return -EFAULT;
	}
	if (!HINIC3_IS_VF_DEV(pci_dev)) {
		mgmt_reg_base =
			pci_dev->mem_resource[HINIC3_PCI_MGMT_REG_BAR].addr;
		if (mgmt_reg_base == NULL) {
			PMD_DRV_LOG(ERR, "mgmt_reg_base addr is null");
			return -EFAULT;
		}
	}
	db_base = pci_dev->mem_resource[HINIC3_PCI_DB_BAR].addr;
	if (db_base == NULL) {
		PMD_DRV_LOG(ERR, "mem_resource addr is null, db_base is NULL");
		return -EFAULT;
	}
	/* If function is VF, mgmt_regs_base will be NULL. */
	if (!mgmt_reg_base)
		hwif->cfg_regs_base =
			(uint8_t *)cfg_regs_base + HINIC3_VF_CFG_REG_OFFSET;
	else
		hwif->cfg_regs_base = cfg_regs_base;
	hwif->mgmt_regs_base = mgmt_reg_base;
	hwif->db_base = db_base;
	hwif->db_dwqe_len = pci_dev->mem_resource[HINIC3_PCI_DB_BAR].len;

	return 0;
}

/**
 * Initialize the hw interface.
 *
 * @param[in] hwdev
 * The pointer to the private hardware device object.
 * @return
 * 0 on success, non-zero on failure.
 */
int
hinic3_init_hwif(struct hinic3_hwdev *hwdev)
{
	struct hinic3_hwif *hwif;
	int err;
	uint32_t attr4, attr5;

	hwif = rte_zmalloc("hinic_hwif", sizeof(struct hinic3_hwif),
			   RTE_CACHE_LINE_SIZE);
	if (hwif == NULL)
		return -ENOMEM;

	hwdev->hwif = hwif;

	err = hinic3_get_bar_addr(hwdev);
	if (err) {
		PMD_DRV_LOG(ERR, "get bar addr fail");
		goto hwif_ready_err;
	}

	err = wait_hwif_ready(hwdev);
	if (err) {
		PMD_DRV_LOG(ERR, "Chip status is not ready");
		goto hwif_ready_err;
	}

	update_hwif_attr(hwif);

	err = wait_until_doorbell_and_outbound_enabled(hwif);
	if (err) {
		attr4 = hinic3_hwif_read_reg(hwif, HINIC3_CSR_FUNC_ATTR4_ADDR);
		attr5 = hinic3_hwif_read_reg(hwif, HINIC3_CSR_FUNC_ATTR5_ADDR);
		PMD_DRV_LOG(ERR,
			    "Hw doorbell/outbound is disabled, attr4 0x%x attr5 0x%x",
			    attr4, attr5);
		goto hwif_ready_err;
	}

	if (!HINIC3_IS_VF(hwdev)) {
		set_ppf(hwif);

		if (HINIC3_IS_PPF(hwdev))
			set_mpf(hwif);

		get_mpf(hwif);
	}

	disable_all_msix(hwdev);
	/* Disable mgmt cpu reporting any event. */
	hinic3_set_pf_status(hwdev->hwif, HINIC3_PF_STATUS_INIT);

	PMD_DRV_LOG(DEBUG,
		    "global_func_idx: %d, func_type: %d, host_id: %d, ppf: %d, mpf: %d init success",
		    hwif->attr.func_global_idx, hwif->attr.func_type,
		    hwif->attr.pci_intf_idx, hwif->attr.ppf_idx,
		    hwif->attr.mpf_idx);

	return 0;

hwif_ready_err:
	rte_free(hwdev->hwif);
	hwdev->hwif = NULL;

	return err;
}

/**
 * Free the hw interface.
 *
 * @param[in] dev
 * The pointer to the private hardware device.
 */
void
hinic3_deinit_hwif(struct hinic3_hwdev *hwdev)
{
	rte_free(hwdev->hwif);
}

uint16_t
hinic3_global_func_id(struct hinic3_hwdev *hwdev)
{
	struct hinic3_hwif *hwif = NULL;

	if (!hwdev)
		return 0;

	hwif = hwdev->hwif;

	return hwif->attr.func_global_idx;
}

uint8_t
hinic3_pf_id_of_vf(struct hinic3_hwdev *hwdev)
{
	struct hinic3_hwif *hwif = NULL;

	if (!hwdev)
		return 0;

	hwif = hwdev->hwif;

	return hwif->attr.port_to_port_idx;
}

uint8_t
hinic3_pcie_itf_id(struct hinic3_hwdev *hwdev)
{
	struct hinic3_hwif *hwif = NULL;

	if (!hwdev)
		return 0;

	hwif = hwdev->hwif;

	return hwif->attr.pci_intf_idx;
}

enum func_type
hinic3_func_type(struct hinic3_hwdev *hwdev)
{
	struct hinic3_hwif *hwif = NULL;

	if (!hwdev)
		return 0;

	hwif = hwdev->hwif;

	return hwif->attr.func_type;
}

uint16_t
hinic3_glb_pf_vf_offset(struct hinic3_hwdev *hwdev)
{
	struct hinic3_hwif *hwif = NULL;

	if (!hwdev)
		return 0;

	hwif = hwdev->hwif;

	return hwif->attr.global_vf_id_of_pf;
}
