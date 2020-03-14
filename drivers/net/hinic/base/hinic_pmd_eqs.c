/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#include "hinic_compat.h"
#include "hinic_csr.h"
#include "hinic_pmd_hwdev.h"
#include "hinic_pmd_hwif.h"
#include "hinic_pmd_mgmt.h"
#include "hinic_pmd_eqs.h"

#define AEQ_CTRL_0_INTR_IDX_SHIFT		0
#define AEQ_CTRL_0_DMA_ATTR_SHIFT		12
#define AEQ_CTRL_0_PCI_INTF_IDX_SHIFT		20
#define AEQ_CTRL_0_INTR_MODE_SHIFT		31

#define AEQ_CTRL_0_INTR_IDX_MASK		0x3FFU
#define AEQ_CTRL_0_DMA_ATTR_MASK		0x3FU
#define AEQ_CTRL_0_PCI_INTF_IDX_MASK		0x3U
#define AEQ_CTRL_0_INTR_MODE_MASK		0x1U

#define AEQ_CTRL_0_SET(val, member)		\
				(((val) & AEQ_CTRL_0_##member##_MASK) << \
				AEQ_CTRL_0_##member##_SHIFT)

#define AEQ_CTRL_0_CLEAR(val, member)		\
				((val) & (~(AEQ_CTRL_0_##member##_MASK \
					<< AEQ_CTRL_0_##member##_SHIFT)))

#define AEQ_CTRL_1_LEN_SHIFT			0
#define AEQ_CTRL_1_ELEM_SIZE_SHIFT		24
#define AEQ_CTRL_1_PAGE_SIZE_SHIFT		28

#define AEQ_CTRL_1_LEN_MASK			0x1FFFFFU
#define AEQ_CTRL_1_ELEM_SIZE_MASK		0x3U
#define AEQ_CTRL_1_PAGE_SIZE_MASK		0xFU

#define AEQ_CTRL_1_SET(val, member)		\
				(((val) & AEQ_CTRL_1_##member##_MASK) << \
				AEQ_CTRL_1_##member##_SHIFT)

#define AEQ_CTRL_1_CLEAR(val, member)		\
				((val) & (~(AEQ_CTRL_1_##member##_MASK \
					<< AEQ_CTRL_1_##member##_SHIFT)))

#define EQ_CONS_IDX_CONS_IDX_SHIFT		0
#define EQ_CONS_IDX_XOR_CHKSUM_SHIFT		24
#define EQ_CONS_IDX_INT_ARMED_SHIFT		31

#define EQ_CONS_IDX_CONS_IDX_MASK		0x1FFFFFU
#define EQ_CONS_IDX_XOR_CHKSUM_MASK		0xFU
#define EQ_CONS_IDX_INT_ARMED_MASK		0x1U

#define EQ_CONS_IDX_SET(val, member)		\
				(((val) & EQ_CONS_IDX_##member##_MASK) << \
				EQ_CONS_IDX_##member##_SHIFT)

#define EQ_CONS_IDX_CLEAR(val, member)		\
				((val) & (~(EQ_CONS_IDX_##member##_MASK \
					<< EQ_CONS_IDX_##member##_SHIFT)))

#define EQ_WRAPPED(eq)			((u32)(eq)->wrapped << EQ_VALID_SHIFT)

#define EQ_CONS_IDX(eq)		((eq)->cons_idx | \
				((u32)(eq)->wrapped << EQ_WRAPPED_SHIFT))

#define EQ_CONS_IDX_REG_ADDR(eq)	\
				(HINIC_CSR_AEQ_CONS_IDX_ADDR((eq)->q_id))

#define EQ_PROD_IDX_REG_ADDR(eq)	\
				(HINIC_CSR_AEQ_PROD_IDX_ADDR((eq)->q_id))

#define GET_EQ_NUM_PAGES(eq, size)		\
		((u16)(ALIGN((eq)->eq_len * (u32)(eq)->elem_size, (size)) \
		/ (size)))

#define GET_EQ_NUM_ELEMS(eq, pg_size)	((pg_size) / (u32)(eq)->elem_size)

#define PAGE_IN_4K(page_size)		((page_size) >> 12)
#define EQ_SET_HW_PAGE_SIZE_VAL(eq) ((u32)ilog2(PAGE_IN_4K((eq)->page_size)))

#define ELEMENT_SIZE_IN_32B(eq)		(((eq)->elem_size) >> 5)
#define EQ_SET_HW_ELEM_SIZE_VAL(eq)	((u32)ilog2(ELEMENT_SIZE_IN_32B(eq)))

#define AEQ_DMA_ATTR_DEFAULT			0

#define EQ_WRAPPED_SHIFT			20

#define	EQ_VALID_SHIFT				31

#define aeq_to_aeqs(eq) \
		container_of((eq) - (eq)->q_id, struct hinic_aeqs, aeq[0])

static u8 eq_cons_idx_checksum_set(u32 val)
{
	u8 checksum = 0;
	u8 idx;

	for (idx = 0; idx < 32; idx += 4)
		checksum ^= ((val >> idx) & 0xF);

	return (checksum & 0xF);
}

/**
 * set_eq_cons_idx - write the cons idx to the hw
 * @eq: The event queue to update the cons idx for
 * @arm_state: indicate whether report interrupts when generate eq element
 */
static void set_eq_cons_idx(struct hinic_eq *eq, u32 arm_state)
{
	u32 eq_cons_idx, eq_wrap_ci, val;
	u32 addr = EQ_CONS_IDX_REG_ADDR(eq);

	eq_wrap_ci = EQ_CONS_IDX(eq);

	/* Read Modify Write */
	val = hinic_hwif_read_reg(eq->hwdev->hwif, addr);

	val = EQ_CONS_IDX_CLEAR(val, CONS_IDX) &
		EQ_CONS_IDX_CLEAR(val, INT_ARMED) &
		EQ_CONS_IDX_CLEAR(val, XOR_CHKSUM);

	/* Just aeq0 use int_arm mode for pmd drv to recv
	 * asyn event&mbox recv data
	 */
	if (eq->q_id == 0)
		eq_cons_idx = EQ_CONS_IDX_SET(eq_wrap_ci, CONS_IDX) |
			EQ_CONS_IDX_SET(arm_state, INT_ARMED);
	else
		eq_cons_idx = EQ_CONS_IDX_SET(eq_wrap_ci, CONS_IDX) |
			EQ_CONS_IDX_SET(HINIC_EQ_NOT_ARMED, INT_ARMED);

	val |= eq_cons_idx;

	val |= EQ_CONS_IDX_SET(eq_cons_idx_checksum_set(val), XOR_CHKSUM);

	hinic_hwif_write_reg(eq->hwdev->hwif, addr, val);
}

/**
 * eq_update_ci - update the cons idx of event queue
 * @eq: the event queue to update the cons idx for
 */
void eq_update_ci(struct hinic_eq *eq)
{
	set_eq_cons_idx(eq, HINIC_EQ_ARMED);
}

/**
 * set_eq_ctrls - setting eq's ctrls registers
 * @eq: the event queue for setting
 */
static void set_aeq_ctrls(struct hinic_eq *eq)
{
	struct hinic_hwif *hwif = eq->hwdev->hwif;
	struct irq_info *eq_irq = &eq->eq_irq;
	u32 addr, val, ctrl0, ctrl1, page_size_val, elem_size;
	u32 pci_intf_idx = HINIC_PCI_INTF_IDX(hwif);

	/* set ctrl0 */
	addr = HINIC_CSR_AEQ_CTRL_0_ADDR(eq->q_id);

	val = hinic_hwif_read_reg(hwif, addr);

	val = AEQ_CTRL_0_CLEAR(val, INTR_IDX) &
		AEQ_CTRL_0_CLEAR(val, DMA_ATTR) &
		AEQ_CTRL_0_CLEAR(val, PCI_INTF_IDX) &
		AEQ_CTRL_0_CLEAR(val, INTR_MODE);

	ctrl0 = AEQ_CTRL_0_SET(eq_irq->msix_entry_idx, INTR_IDX) |
		AEQ_CTRL_0_SET(AEQ_DMA_ATTR_DEFAULT, DMA_ATTR)	|
		AEQ_CTRL_0_SET(pci_intf_idx, PCI_INTF_IDX)	|
		AEQ_CTRL_0_SET(HINIC_INTR_MODE_ARMED, INTR_MODE);

	val |= ctrl0;

	hinic_hwif_write_reg(hwif, addr, val);

	/* set ctrl1 */
	addr = HINIC_CSR_AEQ_CTRL_1_ADDR(eq->q_id);

	page_size_val = EQ_SET_HW_PAGE_SIZE_VAL(eq);
	elem_size = EQ_SET_HW_ELEM_SIZE_VAL(eq);

	ctrl1 = AEQ_CTRL_1_SET(eq->eq_len, LEN)		|
		AEQ_CTRL_1_SET(elem_size, ELEM_SIZE)	|
		AEQ_CTRL_1_SET(page_size_val, PAGE_SIZE);

	hinic_hwif_write_reg(hwif, addr, ctrl1);
}

/**
 * aeq_elements_init - initialize all the elements in the aeq
 * @eq: the event queue
 * @init_val: value to init with it the elements
 */
static void aeq_elements_init(struct hinic_eq *eq, u32 init_val)
{
	struct hinic_aeq_elem *aeqe;
	u16 i;

	for (i = 0; i < eq->eq_len; i++) {
		aeqe = GET_AEQ_ELEM(eq, i);
		aeqe->desc = cpu_to_be32(init_val);
	}

	rte_wmb();	/* Write the init values */
}

/**
 * alloc_eq_pages - allocate the pages for the queue
 * @eq: the event queue
 */
static int alloc_eq_pages(struct hinic_eq *eq)
{
	struct hinic_hwif *hwif = eq->hwdev->hwif;
	u32 init_val;
	u64 dma_addr_size, virt_addr_size;
	u16 pg_num, i;
	int err;

	dma_addr_size = eq->num_pages * sizeof(*eq->dma_addr);
	virt_addr_size = eq->num_pages * sizeof(*eq->virt_addr);

	eq->dma_addr = kzalloc(dma_addr_size, GFP_KERNEL);
	if (!eq->dma_addr) {
		PMD_DRV_LOG(ERR, "Allocate dma addr array failed");
		return -ENOMEM;
	}

	eq->virt_addr = kzalloc(virt_addr_size, GFP_KERNEL);
	if (!eq->virt_addr) {
		PMD_DRV_LOG(ERR, "Allocate virt addr array failed");
		err = -ENOMEM;
		goto virt_addr_alloc_err;
	}

	for (pg_num = 0; pg_num < eq->num_pages; pg_num++) {
		eq->virt_addr[pg_num] =
			(u8 *)dma_zalloc_coherent_aligned(eq->hwdev,
					eq->page_size, &eq->dma_addr[pg_num],
					SOCKET_ID_ANY);
		if (!eq->virt_addr[pg_num]) {
			err = -ENOMEM;
			goto dma_alloc_err;
		}

		hinic_hwif_write_reg(hwif,
				     HINIC_EQ_HI_PHYS_ADDR_REG(eq->type,
				     eq->q_id, pg_num),
				     upper_32_bits(eq->dma_addr[pg_num]));

		hinic_hwif_write_reg(hwif,
				     HINIC_EQ_LO_PHYS_ADDR_REG(eq->type,
				     eq->q_id, pg_num),
				     lower_32_bits(eq->dma_addr[pg_num]));
	}

	init_val = EQ_WRAPPED(eq);

	aeq_elements_init(eq, init_val);

	return 0;

dma_alloc_err:
	for (i = 0; i < pg_num; i++)
		dma_free_coherent(eq->hwdev, eq->page_size,
				  eq->virt_addr[i], eq->dma_addr[i]);

virt_addr_alloc_err:
	kfree(eq->dma_addr);
	return err;
}

/**
 * free_eq_pages - free the pages of the queue
 * @eq: the event queue
 */
static void free_eq_pages(struct hinic_eq *eq)
{
	struct hinic_hwdev *hwdev = eq->hwdev;
	u16 pg_num;

	for (pg_num = 0; pg_num < eq->num_pages; pg_num++)
		dma_free_coherent(hwdev, eq->page_size,
				  eq->virt_addr[pg_num],
				  eq->dma_addr[pg_num]);

	kfree(eq->virt_addr);
	kfree(eq->dma_addr);
}

#define MSIX_ENTRY_IDX_0 (0)

/**
 * init_aeq - initialize aeq
 * @eq:	the event queue
 * @hwdev: the pointer to the private hardware device object
 * @q_id: Queue id number
 * @q_len: the number of EQ elements
 * @type: the type of the event queue, ceq or aeq
 * @page_size: the page size of the event queue
 * @entry: msix entry associated with the event queue
 * Return: 0 - Success, Negative - failure
 */
static int init_aeq(struct hinic_eq *eq, struct hinic_hwdev *hwdev, u16 q_id,
		   u16 q_len, u32 page_size,
		   __rte_unused struct irq_info *entry)
{
	int err = 0;

	eq->hwdev = hwdev;
	eq->q_id = q_id;
	eq->type = HINIC_AEQ;
	eq->page_size = page_size;
	eq->eq_len = q_len;

	/* clear eq_len to force eqe drop in hardware */
	hinic_hwif_write_reg(eq->hwdev->hwif,
			     HINIC_CSR_AEQ_CTRL_1_ADDR(eq->q_id), 0);

	/* Clear PI and CI, also clear the ARM bit */
	hinic_hwif_write_reg(eq->hwdev->hwif, EQ_CONS_IDX_REG_ADDR(eq), 0);
	hinic_hwif_write_reg(eq->hwdev->hwif, EQ_PROD_IDX_REG_ADDR(eq), 0);

	eq->cons_idx = 0;
	eq->wrapped = 0;

	eq->elem_size = HINIC_AEQE_SIZE;
	eq->num_pages = GET_EQ_NUM_PAGES(eq, page_size);
	eq->num_elem_in_pg = GET_EQ_NUM_ELEMS(eq, page_size);

	if (eq->num_elem_in_pg & (eq->num_elem_in_pg - 1)) {
		PMD_DRV_LOG(ERR, "Number element in eq page is not power of 2");
		return -EINVAL;
	}

	if (eq->num_pages > HINIC_EQ_MAX_PAGES) {
		PMD_DRV_LOG(ERR, "Too many pages for eq, num_pages: %d",
			eq->num_pages);
		return -EINVAL;
	}

	err = alloc_eq_pages(eq);
	if (err) {
		PMD_DRV_LOG(ERR, "Allocate pages for eq failed");
		return err;
	}

	/* pmd use MSIX_ENTRY_IDX_0 */
	eq->eq_irq.msix_entry_idx = MSIX_ENTRY_IDX_0;

	set_aeq_ctrls(eq);
	set_eq_cons_idx(eq, HINIC_EQ_ARMED);

	if (eq->q_id == 0)
		hinic_set_msix_state(hwdev, 0, HINIC_MSIX_ENABLE);

	eq->poll_retry_nr = HINIC_RETRY_NUM;

	return 0;
}

/**
 * remove_aeq - remove aeq
 * @eq:	the event queue
 */
static void remove_aeq(struct hinic_eq *eq)
{
	struct irq_info *entry = &eq->eq_irq;

	if (eq->q_id == 0)
		hinic_set_msix_state(eq->hwdev, entry->msix_entry_idx,
				     HINIC_MSIX_DISABLE);

	/* clear eq_len to avoid hw access host memory */
	hinic_hwif_write_reg(eq->hwdev->hwif,
			     HINIC_CSR_AEQ_CTRL_1_ADDR(eq->q_id), 0);

	/* update cons_idx to avoid invalid interrupt */
	eq->cons_idx = (u16)hinic_hwif_read_reg(eq->hwdev->hwif,
						EQ_PROD_IDX_REG_ADDR(eq));
	set_eq_cons_idx(eq, HINIC_EQ_NOT_ARMED);

	free_eq_pages(eq);
}

/**
 * hinic_aeqs_init - init all the aeqs
 * @hwdev: the pointer to the private hardware device object
 * @num_aeqs: number of aeq
 * @msix_entries: msix entries associated with the event queues
 * Return: 0 - Success, Negative - failure
 */
static int
hinic_aeqs_init(struct hinic_hwdev *hwdev, u16 num_aeqs,
		struct irq_info *msix_entries)
{
	struct hinic_aeqs *aeqs;
	int err;
	u16 i, q_id;

	aeqs = kzalloc(sizeof(*aeqs), GFP_KERNEL);
	if (!aeqs)
		return -ENOMEM;

	hwdev->aeqs = aeqs;
	aeqs->hwdev = hwdev;
	aeqs->num_aeqs = num_aeqs;

	for (q_id = HINIC_AEQN_START; q_id < num_aeqs; q_id++) {
		err = init_aeq(&aeqs->aeq[q_id], hwdev, q_id,
			      HINIC_DEFAULT_AEQ_LEN, HINIC_EQ_PAGE_SIZE,
			      &msix_entries[q_id]);
		if (err) {
			PMD_DRV_LOG(ERR, "Init aeq %d failed", q_id);
			goto init_aeq_err;
		}
	}

	return 0;

init_aeq_err:
	for (i = 0; i < q_id; i++)
		remove_aeq(&aeqs->aeq[i]);

	kfree(aeqs);

	return err;
}

/**
 * hinic_aeqs_free - free all the aeqs
 * @hwdev: the pointer to the private hardware device object
 */
static void hinic_aeqs_free(struct hinic_hwdev *hwdev)
{
	struct hinic_aeqs *aeqs = hwdev->aeqs;
	u16 q_id;

	/* hinic pmd use aeq[1~3], aeq[0] used in kernel only */
	for (q_id = HINIC_AEQN_START; q_id < aeqs->num_aeqs ; q_id++)
		remove_aeq(&aeqs->aeq[q_id]);

	kfree(aeqs);
}

void hinic_dump_aeq_info(struct hinic_hwdev *hwdev)
{
	struct hinic_eq *eq;
	u32 addr, ci, pi;
	int q_id;

	for (q_id = 0; q_id < hwdev->aeqs->num_aeqs; q_id++) {
		eq = &hwdev->aeqs->aeq[q_id];
		addr = EQ_CONS_IDX_REG_ADDR(eq);
		ci = hinic_hwif_read_reg(hwdev->hwif, addr);
		addr = EQ_PROD_IDX_REG_ADDR(eq);
		pi = hinic_hwif_read_reg(hwdev->hwif, addr);
		PMD_DRV_LOG(ERR, "aeq id: %d, ci: 0x%x, pi: 0x%x",
			q_id, ci, pi);
	}
}

int hinic_comm_aeqs_init(struct hinic_hwdev *hwdev)
{
	int rc;
	u16 num_aeqs;
	struct irq_info aeq_irqs[HINIC_MAX_AEQS];

	num_aeqs = HINIC_HWIF_NUM_AEQS(hwdev->hwif);
	if (num_aeqs < HINIC_MAX_AEQS) {
		PMD_DRV_LOG(ERR, "Warning: PMD need %d AEQs, Chip have %d",
			HINIC_MAX_AEQS, num_aeqs);
		return HINIC_ERROR;
	}

	memset(aeq_irqs, 0, sizeof(aeq_irqs));
	rc = hinic_aeqs_init(hwdev, num_aeqs, aeq_irqs);
	if (rc != HINIC_OK)
		PMD_DRV_LOG(ERR, "Initialize aeqs failed, rc: %d", rc);

	return rc;
}

void hinic_comm_aeqs_free(struct hinic_hwdev *hwdev)
{
	hinic_aeqs_free(hwdev);
}
