/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include <rte_atomic.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include "hinic3_compat.h"
#include "hinic3_csr.h"
#include "hinic3_eqs.h"
#include "hinic3_hwdev.h"
#include "hinic3_hwif.h"
#include "hinic3_mbox.h"
#include "hinic3_mgmt.h"
#include "hinic3_nic_event.h"

/* Indicate AEQ_CTRL_0 shift. */
#define AEQ_CTRL_0_INTR_IDX_SHIFT     0
#define AEQ_CTRL_0_DMA_ATTR_SHIFT     12
#define AEQ_CTRL_0_PCI_INTF_IDX_SHIFT 20
#define AEQ_CTRL_0_INTR_MODE_SHIFT    31

/* Indicate AEQ_CTRL_0 mask. */
#define AEQ_CTRL_0_INTR_IDX_MASK     0x3FFU
#define AEQ_CTRL_0_DMA_ATTR_MASK     0x3FU
#define AEQ_CTRL_0_PCI_INTF_IDX_MASK 0x7U
#define AEQ_CTRL_0_INTR_MODE_MASK    0x1U

/* Set and clear the AEQ_CTRL_0 bit fields. */
#define AEQ_CTRL_0_SET(val, member) \
	(((val) & AEQ_CTRL_0_##member##_MASK) << AEQ_CTRL_0_##member##_SHIFT)
#define AEQ_CTRL_0_CLEAR(val, member) \
	((val) & (~(AEQ_CTRL_0_##member##_MASK << AEQ_CTRL_0_##member##_SHIFT)))

/* Indicate AEQ_CTRL_1 shift. */
#define AEQ_CTRL_1_LEN_SHIFT	   0
#define AEQ_CTRL_1_ELEM_SIZE_SHIFT 24
#define AEQ_CTRL_1_PAGE_SIZE_SHIFT 28

/* Indicate AEQ_CTRL_1 mask. */
#define AEQ_CTRL_1_LEN_MASK	  0x1FFFFFU
#define AEQ_CTRL_1_ELEM_SIZE_MASK 0x3U
#define AEQ_CTRL_1_PAGE_SIZE_MASK 0xFU

/* Set and clear the AEQ_CTRL_1 bit fields. */
#define AEQ_CTRL_1_SET(val, member) \
	(((val) & AEQ_CTRL_1_##member##_MASK) << AEQ_CTRL_1_##member##_SHIFT)
#define AEQ_CTRL_1_CLEAR(val, member) \
	((val) & (~(AEQ_CTRL_1_##member##_MASK << AEQ_CTRL_1_##member##_SHIFT)))

#define HINIC3_EQ_PROD_IDX_MASK	      0xFFFFF
#define HINIC3_TASK_PROCESS_EQE_LIMIT 1024
#define HINIC3_EQ_UPDATE_CI_STEP      64

/* Indicate EQ_ELEM_DESC shift. */
#define EQ_ELEM_DESC_TYPE_SHIFT	   0
#define EQ_ELEM_DESC_SRC_SHIFT	   7
#define EQ_ELEM_DESC_SIZE_SHIFT	   8
#define EQ_ELEM_DESC_WRAPPED_SHIFT 31

/* Indicate EQ_ELEM_DESC mask. */
#define EQ_ELEM_DESC_TYPE_MASK	  0x7FU
#define EQ_ELEM_DESC_SRC_MASK	  0x1U
#define EQ_ELEM_DESC_SIZE_MASK	  0xFFU
#define EQ_ELEM_DESC_WRAPPED_MASK 0x1U

/* Get the AEQ_CTRL_1 bit fields. */
#define EQ_ELEM_DESC_GET(val, member)               \
	(((val) >> EQ_ELEM_DESC_##member##_SHIFT) & \
	 EQ_ELEM_DESC_##member##_MASK)

/* Indicate EQ_CI_SIMPLE_INDIR shift. */
#define EQ_CI_SIMPLE_INDIR_CI_SHIFT	 0
#define EQ_CI_SIMPLE_INDIR_ARMED_SHIFT	 21
#define EQ_CI_SIMPLE_INDIR_AEQ_IDX_SHIFT 30

/* Indicate EQ_CI_SIMPLE_INDIR mask. */
#define EQ_CI_SIMPLE_INDIR_CI_MASK	0x1FFFFFU
#define EQ_CI_SIMPLE_INDIR_ARMED_MASK	0x1U
#define EQ_CI_SIMPLE_INDIR_AEQ_IDX_MASK 0x3U

/* Set and clear the EQ_CI_SIMPLE_INDIR bit fields. */
#define EQ_CI_SIMPLE_INDIR_SET(val, member)           \
	(((val) & EQ_CI_SIMPLE_INDIR_##member##_MASK) \
	 << EQ_CI_SIMPLE_INDIR_##member##_SHIFT)
#define EQ_CI_SIMPLE_INDIR_CLEAR(val, member)          \
	((val) & (~(EQ_CI_SIMPLE_INDIR_##member##_MASK \
		    << EQ_CI_SIMPLE_INDIR_##member##_SHIFT)))

#define EQ_WRAPPED(eq) ((u32)(eq)->wrapped << EQ_VALID_SHIFT)

#define EQ_CONS_IDX(eq)                                                    \
	({                                                                 \
		typeof(eq) __eq = (eq);                                    \
		__eq->cons_idx | ((u32)__eq->wrapped << EQ_WRAPPED_SHIFT); \
	})
#define GET_EQ_NUM_PAGES(eq, size)                                     \
	({                                                             \
		typeof(eq) __eq = (eq);                                \
		typeof(size) __size = (size);                          \
		(u16)(RTE_ALIGN((u32)(__eq->eq_len * __eq->elem_size), \
				__size) /                              \
		      __size);                                         \
	})

#define GET_EQ_NUM_ELEMS(eq, pg_size) ((pg_size) / (u32)(eq)->elem_size)

#define GET_EQ_ELEMENT(eq, idx)                                         \
	({                                                              \
		typeof(eq) __eq = (eq);                                 \
		typeof(idx) __idx = (idx);                              \
		((u8 *)__eq->virt_addr[__idx / __eq->num_elem_in_pg]) + \
			(u32)((__idx & (__eq->num_elem_in_pg - 1)) *    \
			      __eq->elem_size);                         \
	})

#define GET_AEQ_ELEM(eq, idx) \
	((struct hinic3_aeq_elem *)GET_EQ_ELEMENT((eq), (idx)))

#define PAGE_IN_4K(page_size)	    ((page_size) >> 12)
#define EQ_SET_HW_PAGE_SIZE_VAL(eq) ((u32)ilog2(PAGE_IN_4K((eq)->page_size)))

#define ELEMENT_SIZE_IN_32B(eq)	    (((eq)->elem_size) >> 5)
#define EQ_SET_HW_ELEM_SIZE_VAL(eq) ((u32)ilog2(ELEMENT_SIZE_IN_32B(eq)))

#define AEQ_DMA_ATTR_DEFAULT 0

#define EQ_WRAPPED_SHIFT 20

#define EQ_VALID_SHIFT 31

#define aeq_to_aeqs(eq)                                                      \
	({                                                                   \
		typeof(eq) __eq = (eq);                                      \
		container_of(__eq - __eq->q_id, struct hinic3_aeqs, aeq[0]); \
	})

#define AEQ_MSIX_ENTRY_IDX_0 0

/**
 * Write the consumer idx to hw.
 *
 * @param[in] eq
 * The event queue to update the cons idx.
 * @param[in] arm_state
 * Indicate whether report interrupts when generate eq element.
 */
static void
set_eq_cons_idx(struct hinic3_eq *eq, u32 arm_state)
{
	u32 eq_wrap_ci = 0;
	u32 val = 0;
	u32 addr = HINIC3_CSR_AEQ_CI_SIMPLE_INDIR_ADDR;

	eq_wrap_ci = EQ_CONS_IDX(eq);

	/* Dpdk pmd driver only aeq0 use int_arm mode. */
	if (eq->q_id != 0)
		val = EQ_CI_SIMPLE_INDIR_SET(HINIC3_EQ_NOT_ARMED, ARMED);
	else
		val = EQ_CI_SIMPLE_INDIR_SET(arm_state, ARMED);

	val = val | EQ_CI_SIMPLE_INDIR_SET(eq_wrap_ci, CI) |
	      EQ_CI_SIMPLE_INDIR_SET(eq->q_id, AEQ_IDX);

	hinic3_hwif_write_reg(eq->hwdev->hwif, addr, val);
}

/**
 * Set aeq's ctrls registers.
 *
 * @param[in] eq
 * The event queue for setting.
 */
static void
set_aeq_ctrls(struct hinic3_eq *eq)
{
	struct hinic3_hwif *hwif = eq->hwdev->hwif;
	struct irq_info *eq_irq = &eq->eq_irq;
	u32 addr, val, ctrl0, ctrl1, page_size_val, elem_size;
	u32 pci_intf_idx = HINIC3_PCI_INTF_IDX(hwif);

	/* Set AEQ ctrl0. */
	addr = HINIC3_CSR_AEQ_CTRL_0_ADDR;

	val = hinic3_hwif_read_reg(hwif, addr);

	val = AEQ_CTRL_0_CLEAR(val, INTR_IDX) &
	      AEQ_CTRL_0_CLEAR(val, DMA_ATTR) &
	      AEQ_CTRL_0_CLEAR(val, PCI_INTF_IDX) &
	      AEQ_CTRL_0_CLEAR(val, INTR_MODE);

	ctrl0 = AEQ_CTRL_0_SET(eq_irq->msix_entry_idx, INTR_IDX) |
		AEQ_CTRL_0_SET(AEQ_DMA_ATTR_DEFAULT, DMA_ATTR) |
		AEQ_CTRL_0_SET(pci_intf_idx, PCI_INTF_IDX) |
		AEQ_CTRL_0_SET(HINIC3_INTR_MODE_ARMED, INTR_MODE);

	val |= ctrl0;

	hinic3_hwif_write_reg(hwif, addr, val);

	/* Set AEQ ctrl1. */
	addr = HINIC3_CSR_AEQ_CTRL_1_ADDR;

	page_size_val = EQ_SET_HW_PAGE_SIZE_VAL(eq);
	elem_size = EQ_SET_HW_ELEM_SIZE_VAL(eq);

	ctrl1 = AEQ_CTRL_1_SET(eq->eq_len, LEN) |
		AEQ_CTRL_1_SET(elem_size, ELEM_SIZE) |
		AEQ_CTRL_1_SET(page_size_val, PAGE_SIZE);

	hinic3_hwif_write_reg(hwif, addr, ctrl1);
}

/**
 * Initialize all the elements in the aeq.
 *
 * @param[in] eq
 * The event queue.
 * @param[in] init_val
 * Value to init.
 */
static void
aeq_elements_init(struct hinic3_eq *eq, u32 init_val)
{
	struct hinic3_aeq_elem *aeqe = NULL;
	u32 i;

	for (i = 0; i < eq->eq_len; i++) {
		aeqe = GET_AEQ_ELEM(eq, i);
		aeqe->desc = cpu_to_be32(init_val);
	}

	rte_atomic_thread_fence(rte_memory_order_release); /**< Write the init values. */
}

/**
 * Set the pages for the event queue.
 *
 * @param[in] eq
 * The event queue.
 * @return
 * 0 on success, non-zero on failure.
 */
static int
set_eq_pages(struct hinic3_eq *eq)
{
	struct hinic3_hwif *hwif = eq->hwdev->hwif;
	u32 reg, init_val;
	u16 pg_num, i;
	int err;

	for (pg_num = 0; pg_num < eq->num_pages; pg_num++) {
		/* Allocate memory for each page. */
		eq->eq_mz[pg_num] = hinic3_dma_zone_reserve(eq->hwdev->eth_dev,
			"eq_mz", eq->q_id, eq->page_size,
			eq->page_size, SOCKET_ID_ANY);
		if (!eq->eq_mz[pg_num]) {
			err = -ENOMEM;
			goto dma_alloc_err;
		}

		/* Write physical memory address and virtual memory address. */
		eq->dma_addr[pg_num] = eq->eq_mz[pg_num]->iova;
		eq->virt_addr[pg_num] = eq->eq_mz[pg_num]->addr;

		reg = HINIC3_AEQ_HI_PHYS_ADDR_REG(pg_num);
		hinic3_hwif_write_reg(hwif, reg,
				      upper_32_bits(eq->dma_addr[pg_num]));

		reg = HINIC3_AEQ_LO_PHYS_ADDR_REG(pg_num);
		hinic3_hwif_write_reg(hwif, reg,
				      lower_32_bits(eq->dma_addr[pg_num]));
	}
	/* Calculate the number of elements that can be accommodated. */
	eq->num_elem_in_pg = GET_EQ_NUM_ELEMS(eq, eq->page_size);
	if (eq->num_elem_in_pg & (eq->num_elem_in_pg - 1)) {
		PMD_DRV_LOG(ERR, "Number element in eq page != power of 2");
		err = -EINVAL;
		goto dma_alloc_err;
	}
	init_val = EQ_WRAPPED(eq);

	/* Initialize elements in the queue. */
	aeq_elements_init(eq, init_val);

	return 0;

dma_alloc_err:
	for (i = 0; i < pg_num; i++)
		hinic3_memzone_free(eq->eq_mz[i]);

	return err;
}

/**
 * Allocate the pages for the event queue.
 *
 * @param[in] eq
 * The event queue.
 * @return
 * 0 on success, non-zero on failure.
 */
static int
alloc_eq_pages(struct hinic3_eq *eq)
{
	u64 dma_addr_size, virt_addr_size, eq_mz_size;
	int err;

	/* Calculate the size of the memory to be allocated. */
	dma_addr_size = eq->num_pages * sizeof(*eq->dma_addr);
	virt_addr_size = eq->num_pages * sizeof(*eq->virt_addr);
	eq_mz_size = eq->num_pages * sizeof(*eq->eq_mz);

	eq->dma_addr = rte_zmalloc("eq_dma", dma_addr_size,
				   HINIC3_MEM_ALLOC_ALIGN_MIN);
	if (!eq->dma_addr)
		return -ENOMEM;

	eq->virt_addr = rte_zmalloc("eq_va", virt_addr_size,
				    HINIC3_MEM_ALLOC_ALIGN_MIN);
	if (!eq->virt_addr) {
		err = -ENOMEM;
		goto virt_addr_alloc_err;
	}

	eq->eq_mz =
		rte_zmalloc("eq_mz", eq_mz_size, HINIC3_MEM_ALLOC_ALIGN_MIN);
	if (!eq->eq_mz) {
		err = -ENOMEM;
		goto eq_mz_alloc_err;
	}
	err = set_eq_pages(eq);
	if (err != 0)
		goto eq_pages_err;

	return 0;

eq_pages_err:
	rte_free(eq->eq_mz);

eq_mz_alloc_err:
	rte_free(eq->virt_addr);

virt_addr_alloc_err:
	rte_free(eq->dma_addr);

	return err;
}

/**
 * Free the pages of the event queue.
 *
 * @param[in] eq
 * The event queue.
 */
static void
free_eq_pages(struct hinic3_eq *eq)
{
	u16 pg_num;

	for (pg_num = 0; pg_num < eq->num_pages; pg_num++)
		hinic3_memzone_free(eq->eq_mz[pg_num]);

	rte_free(eq->eq_mz);
	rte_free(eq->virt_addr);
	rte_free(eq->dma_addr);
}

static u32
get_page_size(struct hinic3_eq *eq)
{
	u32 total_size;
	u16 count, n = 0;

	/* Total memory size. */
	total_size = RTE_ALIGN((eq->eq_len * eq->elem_size),
			       HINIC3_MIN_EQ_PAGE_SIZE);
	if (total_size <= (HINIC3_EQ_MAX_PAGES * HINIC3_MIN_EQ_PAGE_SIZE))
		return HINIC3_MIN_EQ_PAGE_SIZE;
	/* Total number of pages. */
	count = (u16)(RTE_ALIGN((total_size / HINIC3_EQ_MAX_PAGES),
				HINIC3_MIN_EQ_PAGE_SIZE) /
		      HINIC3_MIN_EQ_PAGE_SIZE);

	/* Whether count is a power of 2. */
	if (!(count & (count - 1)))
		return HINIC3_MIN_EQ_PAGE_SIZE * count;

	while (count) {
		count >>= 1;
		n++;
	}

	return ((u32)HINIC3_MIN_EQ_PAGE_SIZE) << n;
}

/**
 * Initialize AEQ.
 *
 * @param[in] eq
 * The event queue.
 * @param[in] hwdev
 * The pointer to the private hardware device.
 * @param[in] q_id
 * Queue id number.
 * @param[in] q_len
 * The number of EQ elements.
 * @return
 * 0 on success, non-zero on failure.
 */
static int
init_aeq(struct hinic3_eq *eq, struct hinic3_hwdev *hwdev, u16 q_id, u32 q_len)
{
	int err = 0;

	eq->hwdev = hwdev;
	eq->q_id = q_id;
	eq->eq_len = q_len;

	/* Indirect access should set q_id first. */
	hinic3_hwif_write_reg(hwdev->hwif, HINIC3_AEQ_INDIR_IDX_ADDR, eq->q_id);
	rte_atomic_thread_fence(rte_memory_order_release); /**< write index before config. */

	/* Clear eq_len to force eqe drop in hardware. */
	hinic3_hwif_write_reg(eq->hwdev->hwif, HINIC3_CSR_AEQ_CTRL_1_ADDR, 0);
	rte_atomic_thread_fence(rte_memory_order_release);
	/* Init aeq pi to 0 before allocating aeq pages. */
	hinic3_hwif_write_reg(eq->hwdev->hwif, HINIC3_CSR_AEQ_PROD_IDX_ADDR, 0);

	eq->cons_idx = 0;
	eq->wrapped = 0;

	eq->elem_size = HINIC3_AEQE_SIZE;
	eq->page_size = get_page_size(eq);
	eq->orig_page_size = eq->page_size;
	eq->num_pages = GET_EQ_NUM_PAGES(eq, eq->page_size);
	if (eq->num_pages > HINIC3_EQ_MAX_PAGES) {
		PMD_DRV_LOG(ERR, "Too many pages: %d for aeq", eq->num_pages);
		return -EINVAL;
	}

	err = alloc_eq_pages(eq);
	if (err) {
		PMD_DRV_LOG(ERR, "Allocate pages for eq failed");
		return err;
	}

	/* Pmd driver uses AEQ_MSIX_ENTRY_IDX_0. */
	eq->eq_irq.msix_entry_idx = AEQ_MSIX_ENTRY_IDX_0;
	set_aeq_ctrls(eq);

	set_eq_cons_idx(eq, HINIC3_EQ_ARMED);

	if (eq->q_id == 0)
		hinic3_set_msix_state(hwdev, 0, HINIC3_MSIX_ENABLE);

	eq->poll_retry_nr = HINIC3_RETRY_NUM;

	return 0;
}

/**
 * Remove AEQ.
 *
 * @param[in] eq
 * The event queue.
 */
static void
remove_aeq(struct hinic3_eq *eq)
{
	struct irq_info *entry = &eq->eq_irq;

	if (eq->q_id == 0)
		hinic3_set_msix_state(eq->hwdev, entry->msix_entry_idx,
				      HINIC3_MSIX_DISABLE);

	/* Indirect access should set q_id first. */
	hinic3_hwif_write_reg(eq->hwdev->hwif, HINIC3_AEQ_INDIR_IDX_ADDR,
			      eq->q_id);

	rte_atomic_thread_fence(rte_memory_order_release); /**< Write index before config. */

	/* Clear eq_len to avoid hw access host memory. */
	hinic3_hwif_write_reg(eq->hwdev->hwif, HINIC3_CSR_AEQ_CTRL_1_ADDR, 0);

	/* Update cons_idx to avoid invalid interrupt. */
	eq->cons_idx = hinic3_hwif_read_reg(eq->hwdev->hwif,
					    HINIC3_CSR_AEQ_PROD_IDX_ADDR);
	set_eq_cons_idx(eq, HINIC3_EQ_NOT_ARMED);

	free_eq_pages(eq);
}

/**
 * Init all AEQs.
 *
 * @param[in] hwdev
 * The pointer to the private hardware device object
 * @return
 * 0 on success, non-zero on failure.
 */
int
hinic3_aeqs_init(struct hinic3_hwdev *hwdev)
{
	struct hinic3_aeqs *aeqs = NULL;
	u16 num_aeqs;
	int err;
	u16 i, q_id;

	if (!hwdev)
		return -EINVAL;

	num_aeqs = HINIC3_HWIF_NUM_AEQS(hwdev->hwif);
	if (num_aeqs > HINIC3_MAX_AEQS) {
		PMD_DRV_LOG(INFO, "Adjust aeq num to %d", HINIC3_MAX_AEQS);
		num_aeqs = HINIC3_MAX_AEQS;
	} else if (num_aeqs < HINIC3_MIN_AEQS) {
		PMD_DRV_LOG(ERR, "PMD needs %d AEQs, Chip has %d",
			    HINIC3_MIN_AEQS, num_aeqs);
		return -EINVAL;
	}

	aeqs = rte_zmalloc("hinic3_aeqs", sizeof(*aeqs),
			   HINIC3_MEM_ALLOC_ALIGN_MIN);
	if (!aeqs)
		return -ENOMEM;

	hwdev->aeqs = aeqs;
	aeqs->hwdev = hwdev;
	aeqs->num_aeqs = num_aeqs;

	for (q_id = 0; q_id < num_aeqs; q_id++) {
		err = init_aeq(&aeqs->aeq[q_id], hwdev, q_id,
			       HINIC3_DEFAULT_AEQ_LEN);
		if (err) {
			PMD_DRV_LOG(ERR, "Init aeq %d failed", q_id);
			goto init_aeq_err;
		}
	}

	return 0;

init_aeq_err:
	for (i = 0; i < q_id; i++)
		remove_aeq(&aeqs->aeq[i]);

	rte_free(aeqs);
	return err;
}

/**
 * Free all AEQs.
 *
 * @param[in] hwdev
 * The pointer to the private hardware device.
 */
void
hinic3_aeqs_free(struct hinic3_hwdev *hwdev)
{
	struct hinic3_aeqs *aeqs = hwdev->aeqs;
	u16 q_id;

	for (q_id = 0; q_id < aeqs->num_aeqs; q_id++)
		remove_aeq(&aeqs->aeq[q_id]);

	rte_free(aeqs);
}

void
hinic3_dump_aeq_info(struct hinic3_hwdev *hwdev)
{
	struct hinic3_aeq_elem *aeqe_pos = NULL;
	struct hinic3_eq *eq = NULL;
	u32 addr, ci, pi, ctrl0, idx;
	int q_id;

	for (q_id = 0; q_id < hwdev->aeqs->num_aeqs; q_id++) {
		eq = &hwdev->aeqs->aeq[q_id];
		/* Indirect access should set q_id first. */
		hinic3_hwif_write_reg(eq->hwdev->hwif,
				      HINIC3_AEQ_INDIR_IDX_ADDR, eq->q_id);
		/* Write index before config. */
		rte_atomic_thread_fence(rte_memory_order_release);

		addr = HINIC3_CSR_AEQ_CTRL_0_ADDR;

		ctrl0 = hinic3_hwif_read_reg(hwdev->hwif, addr);

		idx = hinic3_hwif_read_reg(hwdev->hwif,
					   HINIC3_AEQ_INDIR_IDX_ADDR);

		addr = HINIC3_CSR_AEQ_CONS_IDX_ADDR;
		ci = hinic3_hwif_read_reg(hwdev->hwif, addr);
		addr = HINIC3_CSR_AEQ_PROD_IDX_ADDR;
		pi = hinic3_hwif_read_reg(hwdev->hwif, addr);
		aeqe_pos = GET_AEQ_ELEM(eq, eq->cons_idx);
		PMD_DRV_LOG(ERR,
			    "Aeq id: %d, idx: %u, ctrl0: 0x%08x, wrap: %d,"
			    " pi: 0x%x, ci: 0x%08x,  desc: 0x%x",
			    q_id, idx, ctrl0, eq->wrapped, pi, ci,
			    be32_to_cpu(aeqe_pos->desc));
	}
}

static int
aeq_elem_handler(struct hinic3_eq *eq, u32 aeqe_desc,
		 struct hinic3_aeq_elem *aeqe_pos, void *param)
{
	enum hinic3_aeq_type event;
	u8 data[HINIC3_AEQE_DATA_SIZE];
	u8 size;

	event = EQ_ELEM_DESC_GET(aeqe_desc, TYPE);
	if (EQ_ELEM_DESC_GET(aeqe_desc, SRC)) {
		/* SW event uses only the first 8B. */
		memcpy(data, aeqe_pos->aeqe_data, HINIC3_AEQE_DATA_SIZE);
		hinic3_be32_to_cpu(data, HINIC3_AEQE_DATA_SIZE);
		/* Just support HINIC3_STATELESS_EVENT. */
		return hinic3_nic_sw_aeqe_handler(eq->hwdev, event, data);
	}

	memcpy(data, aeqe_pos->aeqe_data, HINIC3_AEQE_DATA_SIZE);
	hinic3_be32_to_cpu(data, HINIC3_AEQE_DATA_SIZE);
	size = EQ_ELEM_DESC_GET(aeqe_desc, SIZE);

	if (event == HINIC3_MSG_FROM_MGMT_CPU) {
		return hinic3_mgmt_msg_aeqe_handler(eq->hwdev, data, size,
						    param);
	} else if (event == HINIC3_MBX_FROM_FUNC) {
		return hinic3_mbox_func_aeqe_handler(eq->hwdev, data, size,
						     param);
	} else {
		PMD_DRV_LOG(ERR, "AEQ hw event not support %d", event);
		return -EINVAL;
	}
}

/**
 * Poll one or continue aeqe, and call dedicated process.
 *
 * @param[in] eq
 * Pointer to the event queue.
 * @param[in] timeout
 * equal to 0 - Poll all aeqe in eq, used in interrupt mode.
 * Greater than 0 - Poll aeq until get aeqe with 'last' field set to 1, used in
 * polling mode.
 * @param[in] param
 * Customized parameter.
 * @return
 * 0 on success, non-zero on failure.
 */
int
hinic3_aeq_poll_msg(struct hinic3_eq *eq, u32 timeout, void *param)
{
	struct hinic3_aeq_elem *aeqe_pos = NULL;
	u32 aeqe_desc = 0;
	u32 eqe_cnt = 0;
	int err = -EFAULT;
	int done = HINIC3_MSG_HANDLER_RES;
	unsigned long end;
	u16 i;

	for (i = 0; ((timeout == 0) && (i < eq->eq_len)) ||
		    ((timeout > 0) && (done != 0) && (i < eq->eq_len));
	     i++) {
		err = -EIO;
		end = jiffies + msecs_to_jiffies(timeout);
		do {
			aeqe_pos = GET_AEQ_ELEM(eq, eq->cons_idx);

			/* Data in HW is in Big endian format. */
			aeqe_desc = be32_to_cpu(aeqe_pos->desc);

			/*
			 * HW updates wrapped bit, when it adds eq element
			 * event.
			 */
			if (EQ_ELEM_DESC_GET(aeqe_desc, WRAPPED) !=
			    eq->wrapped) {
				err = 0;
				rte_atomic_thread_fence(rte_memory_order_acquire);
				break;
			}

			if (timeout != 0)
				usleep(HINIC3_AEQE_DESC_SIZE);
		} while (time_before(jiffies, end));

		if (err != 0) /**< Poll time out. */
			break;
		/* Handle the middle element of the event queue. */
		done = aeq_elem_handler(eq, aeqe_desc, aeqe_pos, param);

		eq->cons_idx++;
		if (eq->cons_idx == eq->eq_len) {
			eq->cons_idx = 0;
			eq->wrapped = !eq->wrapped;
		}

		if (++eqe_cnt >= HINIC3_EQ_UPDATE_CI_STEP) {
			eqe_cnt = 0;
			set_eq_cons_idx(eq, HINIC3_EQ_NOT_ARMED);
		}
	}
	/* Set the consumer index of the event queue. */
	set_eq_cons_idx(eq, HINIC3_EQ_ARMED);

	return err;
}

void
hinic3_dev_handle_aeq_event(struct hinic3_hwdev *hwdev, void *param)
{
	struct hinic3_eq *aeq = &hwdev->aeqs->aeq[0];

	/* Clear resend timer cnt register. */
	hinic3_misx_intr_clear_resend_bit(hwdev, aeq->eq_irq.msix_entry_idx,
					  MSIX_RESEND_TIMER_CLEAR);
	hinic3_aeq_poll_msg(aeq, 0, param);
}
