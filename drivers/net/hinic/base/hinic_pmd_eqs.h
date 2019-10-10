/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC_PMD_EQS_H_
#define _HINIC_PMD_EQS_H_

#define HINIC_EQ_PAGE_SIZE		0x00001000

#define HINIC_AEQN_START		0
#define HINIC_MAX_AEQS			4

#define HINIC_EQ_MAX_PAGES		8

#define HINIC_AEQE_SIZE			64
#define HINIC_CEQE_SIZE			4

#define HINIC_AEQE_DESC_SIZE		4
#define HINIC_AEQE_DATA_SIZE		\
			(HINIC_AEQE_SIZE - HINIC_AEQE_DESC_SIZE)

#define HINIC_DEFAULT_AEQ_LEN		64

#define GET_EQ_ELEMENT(eq, idx)		\
		(((u8 *)(eq)->virt_addr[(idx) / (eq)->num_elem_in_pg]) + \
		(((u32)(idx) & ((eq)->num_elem_in_pg - 1)) * (eq)->elem_size))

#define GET_AEQ_ELEM(eq, idx)		\
			((struct hinic_aeq_elem *)GET_EQ_ELEMENT((eq), (idx)))

#define GET_CEQ_ELEM(eq, idx)	((u32 *)GET_EQ_ELEMENT((eq), (idx)))

enum hinic_eq_intr_mode {
	HINIC_INTR_MODE_ARMED,
	HINIC_INTR_MODE_ALWAYS,
};

enum hinic_eq_ci_arm_state {
	HINIC_EQ_NOT_ARMED,
	HINIC_EQ_ARMED,
};

enum hinic_aeq_type {
	HINIC_HW_INTER_INT = 0,
	HINIC_MBX_FROM_FUNC = 1,
	HINIC_MSG_FROM_MGMT_CPU = 2,
	HINIC_API_RSP = 3,
	HINIC_API_CHAIN_STS = 4,
	HINIC_MBX_SEND_RSLT = 5,
	HINIC_MAX_AEQ_EVENTS
};

#define HINIC_RETRY_NUM	(10)

struct hinic_eq {
	struct hinic_hwdev		*hwdev;
	u16				q_id;
	u16				type;
	u32				page_size;
	u16				eq_len;

	u16				cons_idx;
	u16				wrapped;

	u16				elem_size;
	u16				num_pages;
	u32				num_elem_in_pg;

	struct irq_info			eq_irq;

	dma_addr_t			*dma_addr;
	u8				**virt_addr;

	u16				poll_retry_nr;
};

struct hinic_aeq_elem {
	u8	aeqe_data[HINIC_AEQE_DATA_SIZE];
	u32	desc;
};

struct hinic_aeqs {
	struct hinic_hwdev	*hwdev;
	u16			poll_retry_nr;

	struct hinic_eq		aeq[HINIC_MAX_AEQS];
	u16			num_aeqs;
};

void eq_update_ci(struct hinic_eq *eq);

void hinic_dump_aeq_info(struct hinic_hwdev *hwdev);

int hinic_comm_aeqs_init(struct hinic_hwdev *hwdev);

void hinic_comm_aeqs_free(struct hinic_hwdev *hwdev);

#endif /* _HINIC_PMD_EQS_H_ */
