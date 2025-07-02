/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC3_EQS_H_
#define _HINIC3_EQS_H_

#define HINIC3_MAX_AEQS	    4
#define HINIC3_MIN_AEQS	    2
#define HINIC3_EQ_MAX_PAGES 4

#define HINIC3_AEQE_SIZE 64

#define HINIC3_AEQE_DESC_SIZE 4
#define HINIC3_AEQE_DATA_SIZE (HINIC3_AEQE_SIZE - HINIC3_AEQE_DESC_SIZE)

/* Linux is 1K, dpdk is 64. */
#define HINIC3_DEFAULT_AEQ_LEN 64

#define HINIC3_MIN_EQ_PAGE_SIZE 0x1000	 /**< Min eq page size 1K Bytes. */
#define HINIC3_MAX_EQ_PAGE_SIZE 0x400000 /**< Max eq page size 4M Bytes */

#define HINIC3_MIN_AEQ_LEN 64
#define HINIC3_MAX_AEQ_LEN \
	((HINIC3_MAX_EQ_PAGE_SIZE / HINIC3_AEQE_SIZE) * HINIC3_EQ_MAX_PAGES)

#define EQ_IRQ_NAME_LEN 64

enum hinic3_eq_intr_mode { HINIC3_INTR_MODE_ARMED, HINIC3_INTR_MODE_ALWAYS };

enum hinic3_eq_ci_arm_state { HINIC3_EQ_NOT_ARMED, HINIC3_EQ_ARMED };

/* Structure for interrupt request information. */
struct irq_info {
	u16 msix_entry_idx; /**< IRQ corresponding index number. */
	u32 irq_id;	    /**< The IRQ number from OS. */
};

#define HINIC3_RETRY_NUM 10

enum hinic3_aeq_type {
	HINIC3_HW_INTER_INT = 0,
	HINIC3_MBX_FROM_FUNC = 1,
	HINIC3_MSG_FROM_MGMT_CPU = 2,
	HINIC3_API_RSP = 3,
	HINIC3_API_CHAIN_STS = 4,
	HINIC3_MBX_SEND_RSLT = 5,
	HINIC3_MAX_AEQ_EVENTS
};

/* Structure for EQ(Event Queue) information. */
struct hinic3_eq {
	struct hinic3_hwdev *hwdev;
	u16 q_id;
	u32 page_size;
	u32 orig_page_size;
	u32 eq_len;

	u32 cons_idx;
	u16 wrapped;

	u16 elem_size;
	u16 num_pages;
	u32 num_elem_in_pg;

	struct irq_info eq_irq;

	const struct rte_memzone **eq_mz;
	rte_iova_t *dma_addr;
	u8 **virt_addr;

	u16 poll_retry_nr;
};

struct hinic3_aeq_elem {
	u8 aeqe_data[HINIC3_AEQE_DATA_SIZE];
	u32 desc;
};

/* Structure for AEQs(Asynchronous Event Queues) information. */
struct hinic3_aeqs {
	struct hinic3_hwdev *hwdev;

	struct hinic3_eq aeq[HINIC3_MAX_AEQS];
	u16 num_aeqs;
};

int hinic3_aeqs_init(struct hinic3_hwdev *hwdev);

void hinic3_aeqs_free(struct hinic3_hwdev *hwdev);

void hinic3_dump_aeq_info(struct hinic3_hwdev *hwdev);

int hinic3_aeq_poll_msg(struct hinic3_eq *eq, u32 timeout, void *param);

void hinic3_dev_handle_aeq_event(struct hinic3_hwdev *hwdev, void *param);

#endif /**< _HINIC3_EQS_H_ */
