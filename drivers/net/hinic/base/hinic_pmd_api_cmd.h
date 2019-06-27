/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC_PMD_API_CMD_H_
#define _HINIC_PMD_API_CMD_H_

#define HINIC_API_CMD_CELL_CTRL_CELL_LEN_SHIFT			0
#define HINIC_API_CMD_CELL_CTRL_RD_DMA_ATTR_OFF_SHIFT		16
#define HINIC_API_CMD_CELL_CTRL_WR_DMA_ATTR_OFF_SHIFT		24
#define HINIC_API_CMD_CELL_CTRL_XOR_CHKSUM_SHIFT		56

#define HINIC_API_CMD_CELL_CTRL_CELL_LEN_MASK			0x3FU
#define HINIC_API_CMD_CELL_CTRL_RD_DMA_ATTR_OFF_MASK		0x3FU
#define HINIC_API_CMD_CELL_CTRL_WR_DMA_ATTR_OFF_MASK		0x3FU
#define HINIC_API_CMD_CELL_CTRL_XOR_CHKSUM_MASK			0xFFU

#define HINIC_API_CMD_CELL_CTRL_SET(val, member)		\
	((((u64)val) & HINIC_API_CMD_CELL_CTRL_##member##_MASK) << \
		HINIC_API_CMD_CELL_CTRL_##member##_SHIFT)

#define HINIC_API_CMD_CELL_CTRL_CLEAR(val, member)		\
	((val) & (~((u64)HINIC_API_CMD_CELL_CTRL_##member##_MASK <<	\
		HINIC_API_CMD_CELL_CTRL_##member##_SHIFT)))

#define HINIC_API_CMD_DESC_API_TYPE_SHIFT			0
#define HINIC_API_CMD_DESC_RD_WR_SHIFT				1
#define HINIC_API_CMD_DESC_MGMT_BYPASS_SHIFT			2
#define HINIC_API_CMD_DESC_RESP_AEQE_EN_SHIFT			3
#define HINIC_API_CMD_DESC_PRIV_DATA_SHIFT			8
#define HINIC_API_CMD_DESC_DEST_SHIFT				32
#define HINIC_API_CMD_DESC_SIZE_SHIFT				40
#define HINIC_API_CMD_DESC_XOR_CHKSUM_SHIFT			56

#define HINIC_API_CMD_DESC_API_TYPE_MASK			0x1U
#define HINIC_API_CMD_DESC_RD_WR_MASK				0x1U
#define HINIC_API_CMD_DESC_MGMT_BYPASS_MASK			0x1U
#define HINIC_API_CMD_DESC_RESP_AEQE_EN_MASK			0x1U
#define HINIC_API_CMD_DESC_DEST_MASK				0x1FU
#define HINIC_API_CMD_DESC_SIZE_MASK				0x7FFU
#define HINIC_API_CMD_DESC_XOR_CHKSUM_MASK			0xFFU
#define HINIC_API_CMD_DESC_PRIV_DATA_MASK			0xFFFFFFU

#define HINIC_API_CMD_DESC_SET(val, member)			\
	((((u64)val) & HINIC_API_CMD_DESC_##member##_MASK) << \
		HINIC_API_CMD_DESC_##member##_SHIFT)

#define HINIC_API_CMD_DESC_CLEAR(val, member)			\
	((val) & (~((u64)HINIC_API_CMD_DESC_##member##_MASK <<	\
		HINIC_API_CMD_DESC_##member##_SHIFT)))

#define HINIC_API_CMD_STATUS_HEADER_VALID_SHIFT			0
#define HINIC_API_CMD_STATUS_HEADER_CHAIN_ID_SHIFT		16

#define HINIC_API_CMD_STATUS_HEADER_VALID_MASK			0xFFU
#define HINIC_API_CMD_STATUS_HEADER_CHAIN_ID_MASK		0xFFU

#define HINIC_API_CMD_STATUS_VALID_CODE				0xFF

#define HINIC_API_CMD_STATUS_HEADER_GET(val, member)		\
	(((val) >> HINIC_API_CMD_STATUS_HEADER_##member##_SHIFT) &	\
		HINIC_API_CMD_STATUS_HEADER_##member##_MASK)

#define HINIC_API_CMD_CHAIN_REQ_RESTART_SHIFT			1
#define HINIC_API_CMD_CHAIN_REQ_WB_TRIGGER_SHIFT		2

#define HINIC_API_CMD_CHAIN_REQ_RESTART_MASK			0x1U
#define HINIC_API_CMD_CHAIN_REQ_WB_TRIGGER_MASK			0x1U

#define HINIC_API_CMD_CHAIN_REQ_SET(val, member)		\
	(((val) & HINIC_API_CMD_CHAIN_REQ_##member##_MASK) << \
		HINIC_API_CMD_CHAIN_REQ_##member##_SHIFT)

#define HINIC_API_CMD_CHAIN_REQ_GET(val, member)		\
	(((val) >> HINIC_API_CMD_CHAIN_REQ_##member##_SHIFT) & \
		HINIC_API_CMD_CHAIN_REQ_##member##_MASK)

#define HINIC_API_CMD_CHAIN_REQ_CLEAR(val, member)		\
	((val) & (~(HINIC_API_CMD_CHAIN_REQ_##member##_MASK <<	\
		HINIC_API_CMD_CHAIN_REQ_##member##_SHIFT)))

#define HINIC_API_CMD_CHAIN_CTRL_RESTART_EN_SHIFT		1
#define HINIC_API_CMD_CHAIN_CTRL_XOR_ERR_SHIFT			2
#define HINIC_API_CMD_CHAIN_CTRL_AEQE_EN_SHIFT			4
#define HINIC_API_CMD_CHAIN_CTRL_AEQ_ID_SHIFT			8
#define HINIC_API_CMD_CHAIN_CTRL_XOR_CHK_EN_SHIFT		28
#define HINIC_API_CMD_CHAIN_CTRL_CELL_SIZE_SHIFT		30

#define HINIC_API_CMD_CHAIN_CTRL_RESTART_EN_MASK		0x1U
#define HINIC_API_CMD_CHAIN_CTRL_XOR_ERR_MASK			0x1U
#define HINIC_API_CMD_CHAIN_CTRL_AEQE_EN_MASK			0x1U
#define HINIC_API_CMD_CHAIN_CTRL_AEQ_ID_MASK			0x3U
#define HINIC_API_CMD_CHAIN_CTRL_XOR_CHK_EN_MASK		0x3U
#define HINIC_API_CMD_CHAIN_CTRL_CELL_SIZE_MASK			0x3U

#define HINIC_API_CMD_CHAIN_CTRL_SET(val, member)		\
	(((val) & HINIC_API_CMD_CHAIN_CTRL_##member##_MASK) << \
		HINIC_API_CMD_CHAIN_CTRL_##member##_SHIFT)

#define HINIC_API_CMD_CHAIN_CTRL_CLEAR(val, member)		\
	((val) & (~(HINIC_API_CMD_CHAIN_CTRL_##member##_MASK <<	\
		HINIC_API_CMD_CHAIN_CTRL_##member##_SHIFT)))

#define HINIC_API_CMD_RESP_HEAD_VALID_MASK		0xFF
#define HINIC_API_CMD_RESP_HEAD_VALID_CODE		0xFF

#define HINIC_API_CMD_RESP_HEADER_VALID(val)	\
	(((val) & HINIC_API_CMD_RESP_HEAD_VALID_MASK) == \
		HINIC_API_CMD_RESP_HEAD_VALID_CODE)

#define HINIC_API_CMD_RESP_HEAD_STATUS_SHIFT		8
#define HINIC_API_CMD_RESP_HEAD_STATUS_MASK		0xFFU

#define HINIC_API_CMD_RESP_HEAD_ERR_CODE		0x1
#define HINIC_API_CMD_RESP_HEAD_ERR(val)	\
	((((val) >> HINIC_API_CMD_RESP_HEAD_STATUS_SHIFT) & \
		HINIC_API_CMD_RESP_HEAD_STATUS_MASK) ==	\
		HINIC_API_CMD_RESP_HEAD_ERR_CODE)

#define HINIC_API_CMD_RESP_HEAD_CHAIN_ID_SHIFT		16
#define HINIC_API_CMD_RESP_HEAD_CHAIN_ID_MASK		0xFF

#define HINIC_API_CMD_RESP_RESERVED			3
#define HINIC_API_CMD_RESP_HEAD_CHAIN_ID(val)	\
	(((val) >> HINIC_API_CMD_RESP_HEAD_CHAIN_ID_SHIFT) & \
		HINIC_API_CMD_RESP_HEAD_CHAIN_ID_MASK)

#define HINIC_API_CMD_RESP_HEAD_DRIVER_PRIV_SHIFT	40
#define HINIC_API_CMD_RESP_HEAD_DRIVER_PRIV_MASK	0xFFFFFFU

#define HINIC_API_CMD_RESP_HEAD_DRIVER_PRIV(val)	\
	(u16)(((val) >> HINIC_API_CMD_RESP_HEAD_DRIVER_PRIV_SHIFT) & \
		HINIC_API_CMD_RESP_HEAD_DRIVER_PRIV_MASK)

#define HINIC_API_CMD_STATUS_HEAD_VALID_MASK		0xFFU
#define HINIC_API_CMD_STATUS_HEAD_VALID_SHIFT		0

#define HINIC_API_CMD_STATUS_HEAD_CHAIN_ID_MASK		0xFFU
#define HINIC_API_CMD_STATUS_HEAD_CHAIN_ID_VALID_SHIFT	16

#define HINIC_API_CMD_STATUS_CONS_IDX_MASK		0xFFFFFFU
#define HINIC_API_CMD_STATUS_CONS_IDX_SHIFT		0

#define HINIC_API_CMD_STATUS_FSM_MASK			0xFU
#define HINIC_API_CMD_STATUS_FSM_SHIFT			24

#define HINIC_API_CMD_STATUS_CHKSUM_ERR_MASK		0x3U
#define HINIC_API_CMD_STATUS_CHKSUM_ERR_SHIFT		28

#define HINIC_API_CMD_STATUS_CPLD_ERR_MASK		0x1U
#define HINIC_API_CMD_STATUS_CPLD_ERR_SHIFT		30

#define HINIC_API_CMD_STATUS_CHAIN_ID(val) \
	(((val) >> HINIC_API_CMD_STATUS_HEAD_CHAIN_ID_VALID_SHIFT) & \
		HINIC_API_CMD_STATUS_HEAD_VALID_MASK)

#define HINIC_API_CMD_STATUS_CONS_IDX(val) \
		((val) & HINIC_API_CMD_STATUS_CONS_IDX_MASK)

#define HINIC_API_CMD_STATUS_CHKSUM_ERR(val) \
	(((val) >> HINIC_API_CMD_STATUS_CHKSUM_ERR_SHIFT) & \
		HINIC_API_CMD_STATUS_CHKSUM_ERR_MASK)

#define HINIC_API_CMD_STATUS_GET(val, member)			\
	(((val) >> HINIC_API_CMD_STATUS_##member##_SHIFT) & \
		HINIC_API_CMD_STATUS_##member##_MASK)

enum hinic_api_cmd_chain_type {
	/* read from mgmt cpu command with completion  */
	HINIC_API_CMD_WRITE_ASYNC_TO_MGMT_CPU	= 6,
	/* PMD business api chain */
	HINIC_API_CMD_PMD_WRITE_TO_MGMT         = 7,
	HINIC_API_CMD_MAX
};

enum hinic_node_id {
	HINIC_NODE_ID_MGMT_HOST = 21,
};

struct hinic_api_cmd_status {
	u64 header;
	u32 buf_desc;
	u32 cell_addr_hi;
	u32 cell_addr_lo;
	u32 rsvd0;
	u64 rsvd1;
};

/* HW struct */
struct hinic_api_cmd_cell {
	u64 ctrl;

	/* address is 64 bit in HW struct */
	u64 next_cell_paddr;

	u64 desc;

	/* HW struct */
	union {
		struct {
			u64 hw_cmd_paddr;
		} write;

		struct {
			u64 hw_wb_resp_paddr;
			u64 hw_cmd_paddr;
		} read;
	};
};

struct hinic_api_cmd_cell_ctxt {
	dma_addr_t			cell_paddr;
	struct hinic_api_cmd_cell	*cell_vaddr;

	dma_addr_t			cell_paddr_free;
	void				*cell_vaddr_free;

	dma_addr_t			api_cmd_paddr;
	void				*api_cmd_vaddr;

	dma_addr_t			api_cmd_paddr_free;
	void				*api_cmd_vaddr_free;

	int				status;

	u32				saved_prod_idx;
};

struct hinic_api_cmd_chain_attr {
	struct hinic_hwdev		*hwdev;
	enum hinic_api_cmd_chain_type	chain_type;

	u32				num_cells;
	u16				rsp_size;
	u16				cell_size;
};

struct hinic_api_cmd_chain {
	struct hinic_hwdev		*hwdev;
	enum hinic_api_cmd_chain_type	chain_type;

	u32				num_cells;
	u16				cell_size;
	u16				rsp_size;

	/* HW members is 24 bit format */
	u32				prod_idx;
	u32				cons_idx;

	/* Async cmd can not be scheduled */
	spinlock_t			async_lock;

	dma_addr_t			wb_status_paddr;
	struct hinic_api_cmd_status	*wb_status;

	dma_addr_t			head_cell_paddr;
	struct hinic_api_cmd_cell	*head_node;

	struct hinic_api_cmd_cell_ctxt	*cell_ctxt;
	struct hinic_api_cmd_cell	*curr_node;
};

int hinic_api_cmd_write(struct hinic_api_cmd_chain *chain,
			enum hinic_node_id dest, void *cmd, u16 size);

int hinic_api_cmd_init(struct hinic_hwdev *hwdev,
			       struct hinic_api_cmd_chain **chain);

void hinic_api_cmd_free(struct hinic_api_cmd_chain **chain);

#endif /* _HINIC_PMD_API_CMD_H_ */
