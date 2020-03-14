/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#include "hinic_compat.h"
#include "hinic_csr.h"
#include "hinic_pmd_hwdev.h"
#include "hinic_pmd_cmd.h"
#include "hinic_pmd_hwif.h"
#include "hinic_pmd_api_cmd.h"

#define API_CMD_CHAIN_CELL_SIZE_SHIFT	6U

#define API_CMD_CELL_DESC_SIZE		8
#define API_CMD_CELL_DATA_ADDR_SIZE	8

#define API_CHAIN_NUM_CELLS		32
#define API_CHAIN_CELL_SIZE		128
#define API_CHAIN_RSP_DATA_SIZE		128

#define API_CHAIN_CELL_ALIGNMENT	8

#define API_CMD_TIMEOUT			10000

#define API_CMD_BUF_SIZE		2048UL

#define API_CMD_NODE_ALIGN_SIZE		512UL
#define API_PAYLOAD_ALIGN_SIZE		64

#define API_CHAIN_RESP_ALIGNMENT	64ULL

#define COMPLETION_TIMEOUT_DEFAULT		1000UL
#define POLLING_COMPLETION_TIMEOUT_DEFAULT	1000U

#define API_CMD_RESPONSE_DATA_PADDR(val)	be64_to_cpu(*((u64 *)(val)))

#define READ_API_CMD_PRIV_DATA(id, token)	(((id) << 16) + (token))
#define WRITE_API_CMD_PRIV_DATA(id)		((id) << 16)

#define MASKED_IDX(chain, idx)		((idx) & ((chain)->num_cells - 1))

#undef  SIZE_4BYTES
#undef  SIZE_8BYTES
#define SIZE_4BYTES(size)		(ALIGN((u32)(size), 4U) >> 2)
#define SIZE_8BYTES(size)		(ALIGN((u32)(size), 8U) >> 3)

enum api_cmd_data_format {
	SGL_DATA     = 1,
};

enum api_cmd_type {
	API_CMD_WRITE_TYPE = 0,
	API_CMD_READ_TYPE = 1,
};

enum api_cmd_bypass {
	NOT_BYPASS = 0,
	BYPASS = 1,
};

enum api_cmd_resp_aeq {
	NOT_TRIGGER = 0,
	TRIGGER     = 1,
};

static u8 xor_chksum_set(void *data)
{
	int idx;
	u8 checksum = 0;
	u8 *val = (u8 *)data;

	for (idx = 0; idx < 7; idx++)
		checksum ^= val[idx];

	return checksum;
}

static void set_prod_idx(struct hinic_api_cmd_chain *chain)
{
	enum hinic_api_cmd_chain_type chain_type = chain->chain_type;
	struct hinic_hwif *hwif = chain->hwdev->hwif;
	u32 hw_prod_idx_addr = HINIC_CSR_API_CMD_CHAIN_PI_ADDR(chain_type);
	u32 prod_idx = chain->prod_idx;

	hinic_hwif_write_reg(hwif, hw_prod_idx_addr, prod_idx);
}

static u32 get_hw_cons_idx(struct hinic_api_cmd_chain *chain)
{
	u32 addr, val;

	addr = HINIC_CSR_API_CMD_STATUS_0_ADDR(chain->chain_type);
	val  = hinic_hwif_read_reg(chain->hwdev->hwif, addr);

	return HINIC_API_CMD_STATUS_GET(val, CONS_IDX);
}

static void dump_api_chain_reg(struct hinic_api_cmd_chain *chain)
{
	u32 addr, val;

	addr = HINIC_CSR_API_CMD_STATUS_0_ADDR(chain->chain_type);
	val  = hinic_hwif_read_reg(chain->hwdev->hwif, addr);

	PMD_DRV_LOG(ERR, "chain type: 0x%x", chain->chain_type);
	PMD_DRV_LOG(ERR, "chain hw cpld error: 0x%x",
		HINIC_API_CMD_STATUS_GET(val, CPLD_ERR));
	PMD_DRV_LOG(ERR, "chain hw check error: 0x%x",
		HINIC_API_CMD_STATUS_GET(val, CHKSUM_ERR));
	PMD_DRV_LOG(ERR, "chain hw current fsm: 0x%x",
		HINIC_API_CMD_STATUS_GET(val, FSM));
	PMD_DRV_LOG(ERR, "chain hw current ci: 0x%x",
		HINIC_API_CMD_STATUS_GET(val, CONS_IDX));

	addr = HINIC_CSR_API_CMD_CHAIN_PI_ADDR(chain->chain_type);
	val  = hinic_hwif_read_reg(chain->hwdev->hwif, addr);
	PMD_DRV_LOG(ERR, "Chain hw current pi: 0x%x", val);
}

/**
 * chain_busy - check if the chain is still processing last requests
 * @chain: chain to check
 */
static int chain_busy(struct hinic_api_cmd_chain *chain)
{
	switch (chain->chain_type) {
	case HINIC_API_CMD_WRITE_ASYNC_TO_MGMT_CPU:
	case HINIC_API_CMD_PMD_WRITE_TO_MGMT:
		chain->cons_idx = get_hw_cons_idx(chain);
		if (chain->cons_idx == MASKED_IDX(chain, chain->prod_idx + 1)) {
			PMD_DRV_LOG(ERR, "API CMD chain %d is busy, cons_idx: %d, prod_idx: %d",
				chain->chain_type, chain->cons_idx,
				chain->prod_idx);
			dump_api_chain_reg(chain);
			return -EBUSY;
		}
		break;
	default:
		PMD_DRV_LOG(ERR, "Unknown Chain type");
		return -EINVAL;
	}

	return 0;
}

/**
 * get_cell_data_size - get the data size of specific cell type
 * @type: chain type
 */
static u16 get_cell_data_size(enum hinic_api_cmd_chain_type type,
				__rte_unused u16 cmd_size)
{
	u16 cell_data_size = 0;

	switch (type) {
	case HINIC_API_CMD_WRITE_ASYNC_TO_MGMT_CPU:
	case HINIC_API_CMD_PMD_WRITE_TO_MGMT:
		cell_data_size = ALIGN(API_CMD_CELL_DESC_SIZE +
					API_CMD_CELL_DATA_ADDR_SIZE,
					API_CHAIN_CELL_ALIGNMENT);
		break;
	default:
		break;
	}

	return cell_data_size;
}

/**
 * prepare_cell_ctrl - prepare the ctrl of the cell for the command
 * @cell_ctrl: the control of the cell to set the control into it
 * @cell_len: the size of the cell
 */
static void prepare_cell_ctrl(u64 *cell_ctrl, u16 cell_len)
{
	u64 ctrl;
	u8 chksum;

	/* Read Modify Write */
	ctrl = be64_to_cpu(*cell_ctrl);
	ctrl = HINIC_API_CMD_CELL_CTRL_CLEAR(ctrl, CELL_LEN) &
		HINIC_API_CMD_CELL_CTRL_CLEAR(ctrl, RD_DMA_ATTR_OFF) &
		HINIC_API_CMD_CELL_CTRL_CLEAR(ctrl, WR_DMA_ATTR_OFF) &
		HINIC_API_CMD_CELL_CTRL_CLEAR(ctrl, XOR_CHKSUM);

	ctrl |=  HINIC_API_CMD_CELL_CTRL_SET(SIZE_8BYTES(cell_len), CELL_LEN) |
		HINIC_API_CMD_CELL_CTRL_SET(0ULL, RD_DMA_ATTR_OFF) |
		HINIC_API_CMD_CELL_CTRL_SET(0ULL, WR_DMA_ATTR_OFF);

	chksum = xor_chksum_set(&ctrl);

	ctrl |= HINIC_API_CMD_CELL_CTRL_SET(chksum, XOR_CHKSUM);

	/* The data in the HW should be in Big Endian Format */
	*cell_ctrl = cpu_to_be64(ctrl);
}

/**
 * prepare_api_cmd - prepare API CMD command
 * @chain: chain for the command
 * @cell: the cell of the command
 * @dest: destination node on the card that will receive the command
 * @cmd: command data
 * @cmd_size: the command size
 */
static void prepare_api_cmd(struct hinic_api_cmd_chain *chain,
				struct hinic_api_cmd_cell *cell,
				enum hinic_node_id dest,
				void *cmd, u16 cmd_size)
{
	struct hinic_api_cmd_cell_ctxt	*cell_ctxt;
	u32 priv;

	cell_ctxt = &chain->cell_ctxt[chain->prod_idx];

	/* Clear all the members before changes */
	cell->desc = HINIC_API_CMD_DESC_CLEAR(cell->desc, API_TYPE) &
			HINIC_API_CMD_DESC_CLEAR(cell->desc, RD_WR) &
			HINIC_API_CMD_DESC_CLEAR(cell->desc, MGMT_BYPASS) &
			HINIC_API_CMD_DESC_CLEAR(cell->desc, RESP_AEQE_EN) &
			HINIC_API_CMD_DESC_CLEAR(cell->desc, DEST) &
			HINIC_API_CMD_DESC_CLEAR(cell->desc, SIZE) &
			HINIC_API_CMD_DESC_CLEAR(cell->desc, XOR_CHKSUM);

	switch (chain->chain_type) {
	case HINIC_API_CMD_WRITE_ASYNC_TO_MGMT_CPU:
	case HINIC_API_CMD_PMD_WRITE_TO_MGMT:
		priv =  WRITE_API_CMD_PRIV_DATA(chain->chain_type);
		cell->desc = HINIC_API_CMD_DESC_SET(SGL_DATA, API_TYPE) |
			HINIC_API_CMD_DESC_SET(API_CMD_WRITE_TYPE, RD_WR) |
			HINIC_API_CMD_DESC_SET(NOT_BYPASS, MGMT_BYPASS) |
			HINIC_API_CMD_DESC_SET(TRIGGER, RESP_AEQE_EN)	|
			HINIC_API_CMD_DESC_SET(priv, PRIV_DATA);
		break;
	default:
		PMD_DRV_LOG(ERR, "Unknown Chain type");
		return;
	}

	cell->desc |= HINIC_API_CMD_DESC_SET(dest, DEST)	 |
			HINIC_API_CMD_DESC_SET(SIZE_4BYTES(cmd_size), SIZE);
	cell->desc |= HINIC_API_CMD_DESC_SET(xor_chksum_set(&cell->desc),
						XOR_CHKSUM);

	/* The data in the HW should be in Big Endian Format */
	cell->desc = cpu_to_be64(cell->desc);

	memcpy(cell_ctxt->api_cmd_vaddr, cmd, cmd_size);
}

/**
 * prepare_cell - prepare cell ctrl and cmd in the current producer cell
 * @chain: chain for the command
 * @dest: destination node on the card that will receive the command
 * @cmd: command data
 * @cmd_size: the command size
 */
static void prepare_cell(struct hinic_api_cmd_chain *chain,
			 enum  hinic_node_id dest,
			 void *cmd, u16 cmd_size)
{
	struct hinic_api_cmd_cell *curr_node;
	u16 cell_size;

	curr_node = chain->curr_node;

	cell_size = get_cell_data_size(chain->chain_type, cmd_size);

	prepare_cell_ctrl(&curr_node->ctrl, cell_size);
	prepare_api_cmd(chain, curr_node, dest, cmd, cmd_size);
}

static inline void cmd_chain_prod_idx_inc(struct hinic_api_cmd_chain *chain)
{
	chain->prod_idx = MASKED_IDX(chain, chain->prod_idx + 1);
}

static void issue_api_cmd(struct hinic_api_cmd_chain *chain)
{
	set_prod_idx(chain);
}

/**
 * api_cmd_status_update - update the status of the chain
 * @chain: chain to update
 */
static void api_cmd_status_update(struct hinic_api_cmd_chain *chain)
{
	struct hinic_api_cmd_status *wb_status;
	enum hinic_api_cmd_chain_type chain_type;
	u64	status_header;
	u32	buf_desc;

	wb_status = chain->wb_status;

	buf_desc = be32_to_cpu(wb_status->buf_desc);
	if (HINIC_API_CMD_STATUS_GET(buf_desc, CHKSUM_ERR)) {
		PMD_DRV_LOG(ERR, "API CMD status Xor check error");
		return;
	}

	status_header = be64_to_cpu(wb_status->header);
	chain_type = HINIC_API_CMD_STATUS_HEADER_GET(status_header, CHAIN_ID);
	if (chain_type >= HINIC_API_CMD_MAX)
		return;

	if (chain_type != chain->chain_type)
		return;

	chain->cons_idx = HINIC_API_CMD_STATUS_GET(buf_desc, CONS_IDX);
}

/**
 * wait_for_status_poll - wait for write to mgmt command to complete
 * @chain: the chain of the command
 * Return: 0 - success, negative - failure
 */
static int wait_for_status_poll(struct hinic_api_cmd_chain *chain)
{
	unsigned long end;
	int err = -ETIMEDOUT;

	end = jiffies + msecs_to_jiffies(API_CMD_TIMEOUT);
	do {
		api_cmd_status_update(chain);

		/* SYNC API CMD cmd should start after prev cmd finished */
		if (chain->cons_idx == chain->prod_idx) {
			err = 0;
			break;
		}

		rte_delay_us(10);
	} while (time_before(jiffies, end));

	return err;
}

/**
 * wait_for_api_cmd_completion - wait for command to complete
 * @chain: chain for the command
 * Return: 0 - success, negative - failure
 */
static int wait_for_api_cmd_completion(struct hinic_api_cmd_chain *chain,
		       __rte_unused struct hinic_api_cmd_cell_ctxt *ctxt,
		       __rte_unused void *ack, __rte_unused u16 ack_size)
{
	int err = 0;

	/* poll api cmd status for debug*/
	switch (chain->chain_type) {
	case HINIC_API_CMD_PMD_WRITE_TO_MGMT:
		err = wait_for_status_poll(chain);
		if (err)
			PMD_DRV_LOG(ERR, "API CMD poll status timeout");
		break;
	case HINIC_API_CMD_WRITE_ASYNC_TO_MGMT_CPU:
		break;
	default:
		PMD_DRV_LOG(ERR, "Unknown API CMD chain type");
		err = -EINVAL;
		break;
	}

	if (err)
		dump_api_chain_reg(chain);

	return err;
}

static inline void update_api_cmd_ctxt(struct hinic_api_cmd_chain *chain,
				       struct hinic_api_cmd_cell_ctxt *ctxt)
{
	ctxt->status = 1;
	ctxt->saved_prod_idx = chain->prod_idx;
}

/**
 * api_cmd - API CMD command
 * @chain: chain for the command
 * @dest: destination node on the card that will receive the command
 * @cmd: command data
 * @cmd_size: the command size
 * @ack: pointer to messages to response
 * @ack_size: the size of ack message
 * Return: 0 - success, negative - failure
 */
static int api_cmd(struct hinic_api_cmd_chain *chain,
		   enum hinic_node_id dest,
		   void *cmd, u16 cmd_size, void *ack, u16 ack_size)
{
	struct hinic_api_cmd_cell_ctxt *ctxt;

	spin_lock(&chain->async_lock);

	ctxt = &chain->cell_ctxt[chain->prod_idx];
	if (chain_busy(chain)) {
		spin_unlock(&chain->async_lock);
		return -EBUSY;
	}
	update_api_cmd_ctxt(chain, ctxt);

	prepare_cell(chain, dest, cmd, cmd_size);

	cmd_chain_prod_idx_inc(chain);

	rte_wmb();/* issue the command */

	issue_api_cmd(chain);

	/* incremented prod idx, update ctxt */
	chain->curr_node = chain->cell_ctxt[chain->prod_idx].cell_vaddr;

	spin_unlock(&chain->async_lock);

	return wait_for_api_cmd_completion(chain, ctxt, ack, ack_size);
}

/**
 * hinic_api_cmd_write - Write API CMD command
 * @chain: chain for write command
 * @dest: destination node on the card that will receive the command
 * @cmd: command data
 * @size: the command size
 * Return: 0 - success, negative - failure
 */
int hinic_api_cmd_write(struct hinic_api_cmd_chain *chain,
			enum hinic_node_id dest, void *cmd, u16 size)
{
	/* Verify the chain type */
	return api_cmd(chain, dest, cmd, size, NULL, 0);
}

/**
 * api_cmd_hw_restart - restart the chain in the HW
 * @chain: the API CMD specific chain to restart
 */
static int api_cmd_hw_restart(struct hinic_api_cmd_chain *chain)
{
	struct hinic_hwif *hwif = chain->hwdev->hwif;
	unsigned long end;
	u32 reg_addr, val;
	int err;

	/* Read Modify Write */
	reg_addr = HINIC_CSR_API_CMD_CHAIN_REQ_ADDR(chain->chain_type);
	val = hinic_hwif_read_reg(hwif, reg_addr);

	val = HINIC_API_CMD_CHAIN_REQ_CLEAR(val, RESTART);
	val |= HINIC_API_CMD_CHAIN_REQ_SET(1, RESTART);

	hinic_hwif_write_reg(hwif, reg_addr, val);

	end = jiffies + msecs_to_jiffies(API_CMD_TIMEOUT);
	err = -ETIMEDOUT;
	do {
		val = hinic_hwif_read_reg(hwif, reg_addr);

		if (!HINIC_API_CMD_CHAIN_REQ_GET(val, RESTART)) {
			err = 0;
			break;
		}

		rte_delay_ms(1);
	} while (time_before(jiffies, end));

	return err;
}

/**
 * api_cmd_ctrl_init - set the control register of a chain
 * @chain: the API CMD specific chain to set control register for
 */
static void api_cmd_ctrl_init(struct hinic_api_cmd_chain *chain)
{
	struct hinic_hwif *hwif = chain->hwdev->hwif;
	u32 reg_addr, ctrl;
	u32 cell_size;

	/* Read Modify Write */
	reg_addr = HINIC_CSR_API_CMD_CHAIN_CTRL_ADDR(chain->chain_type);

	cell_size = (u32)ilog2(chain->cell_size >>
			       API_CMD_CHAIN_CELL_SIZE_SHIFT);

	ctrl = hinic_hwif_read_reg(hwif, reg_addr);

	ctrl = HINIC_API_CMD_CHAIN_CTRL_CLEAR(ctrl, AEQE_EN) &
		HINIC_API_CMD_CHAIN_CTRL_CLEAR(ctrl, CELL_SIZE);

	ctrl |= HINIC_API_CMD_CHAIN_CTRL_SET(0, AEQE_EN) |
		HINIC_API_CMD_CHAIN_CTRL_SET(cell_size, CELL_SIZE);

	hinic_hwif_write_reg(hwif, reg_addr, ctrl);
}

/**
 * api_cmd_set_status_addr - set the status address of a chain in the HW
 * @chain: the API CMD specific chain to set status address for
 */
static void api_cmd_set_status_addr(struct hinic_api_cmd_chain *chain)
{
	struct hinic_hwif *hwif = chain->hwdev->hwif;
	u32 addr, val;

	addr = HINIC_CSR_API_CMD_STATUS_HI_ADDR(chain->chain_type);
	val = upper_32_bits(chain->wb_status_paddr);
	hinic_hwif_write_reg(hwif, addr, val);

	addr = HINIC_CSR_API_CMD_STATUS_LO_ADDR(chain->chain_type);
	val = lower_32_bits(chain->wb_status_paddr);
	hinic_hwif_write_reg(hwif, addr, val);
}

/**
 * api_cmd_set_num_cells - set the number cells of a chain in the HW
 * @chain: the API CMD specific chain to set the number of cells for
 */
static void api_cmd_set_num_cells(struct hinic_api_cmd_chain *chain)
{
	struct hinic_hwif *hwif = chain->hwdev->hwif;
	u32 addr, val;

	addr = HINIC_CSR_API_CMD_CHAIN_NUM_CELLS_ADDR(chain->chain_type);
	val  = chain->num_cells;
	hinic_hwif_write_reg(hwif, addr, val);
}

/**
 * api_cmd_head_init - set the head cell of a chain in the HW
 * @chain: the API CMD specific chain to set the head for
 */
static void api_cmd_head_init(struct hinic_api_cmd_chain *chain)
{
	struct hinic_hwif *hwif = chain->hwdev->hwif;
	u32 addr, val;

	addr = HINIC_CSR_API_CMD_CHAIN_HEAD_HI_ADDR(chain->chain_type);
	val = upper_32_bits(chain->head_cell_paddr);
	hinic_hwif_write_reg(hwif, addr, val);

	addr = HINIC_CSR_API_CMD_CHAIN_HEAD_LO_ADDR(chain->chain_type);
	val = lower_32_bits(chain->head_cell_paddr);
	hinic_hwif_write_reg(hwif, addr, val);
}

/**
 * wait_for_ready_chain - wait for the chain to be ready
 * @chain: the API CMD specific chain to wait for
 * Return: 0 - success, negative - failure
 */
static int wait_for_ready_chain(struct hinic_api_cmd_chain *chain)
{
	struct hinic_hwif *hwif = chain->hwdev->hwif;
	unsigned long end;
	u32 addr, val;
	u32 hw_cons_idx;
	int err;

	end = jiffies + msecs_to_jiffies(API_CMD_TIMEOUT);

	addr = HINIC_CSR_API_CMD_STATUS_0_ADDR(chain->chain_type);
	err = -ETIMEDOUT;
	do {
		val = hinic_hwif_read_reg(hwif, addr);
		hw_cons_idx = HINIC_API_CMD_STATUS_GET(val, CONS_IDX);

		/* Wait for HW cons idx to be updated */
		if (hw_cons_idx == chain->cons_idx) {
			err = 0;
			break;
		}

		rte_delay_ms(1);
	} while (time_before(jiffies, end));

	return err;
}

/**
 * api_cmd_chain_hw_clean - clean the HW
 * @chain: the API CMD specific chain
 */
static void api_cmd_chain_hw_clean(struct hinic_api_cmd_chain *chain)
{
	struct hinic_hwif *hwif = chain->hwdev->hwif;
	u32 addr, ctrl;

	addr = HINIC_CSR_API_CMD_CHAIN_CTRL_ADDR(chain->chain_type);

	ctrl = hinic_hwif_read_reg(hwif, addr);
	ctrl = HINIC_API_CMD_CHAIN_CTRL_CLEAR(ctrl, RESTART_EN) &
	       HINIC_API_CMD_CHAIN_CTRL_CLEAR(ctrl, XOR_ERR)    &
	       HINIC_API_CMD_CHAIN_CTRL_CLEAR(ctrl, AEQE_EN)    &
	       HINIC_API_CMD_CHAIN_CTRL_CLEAR(ctrl, XOR_CHK_EN) &
	       HINIC_API_CMD_CHAIN_CTRL_CLEAR(ctrl, CELL_SIZE);

	hinic_hwif_write_reg(hwif, addr, ctrl);
}

/**
 * api_cmd_chain_hw_init - initialize the chain in the HW
 *(initialize API command csr)
 * @chain: the API CMD specific chain to initialize in HW
 * Return: 0 - success, negative - failure
 */
static int api_cmd_chain_hw_init(struct hinic_api_cmd_chain *chain)
{
	api_cmd_chain_hw_clean(chain);

	api_cmd_set_status_addr(chain);

	if (api_cmd_hw_restart(chain)) {
		PMD_DRV_LOG(ERR, "Restart api_cmd_hw failed");
		return -EBUSY;
	}

	api_cmd_ctrl_init(chain);
	api_cmd_set_num_cells(chain);
	api_cmd_head_init(chain);

	return wait_for_ready_chain(chain);
}

/**
 * free_cmd_buf - free the dma buffer of API CMD command
 * @chain: the API CMD specific chain of the cmd
 * @cell_idx: the cell index of the cmd
 */
static void free_cmd_buf(struct hinic_api_cmd_chain *chain, u32 cell_idx)
{
	struct hinic_api_cmd_cell_ctxt *cell_ctxt;
	void *dev = chain->hwdev;

	cell_ctxt = &chain->cell_ctxt[cell_idx];

	dma_free_coherent(dev, (API_CMD_BUF_SIZE + API_PAYLOAD_ALIGN_SIZE),
			  cell_ctxt->api_cmd_vaddr_free,
			  cell_ctxt->api_cmd_paddr_free);
}

/**
 * alloc_cmd_buf - allocate a dma buffer for API CMD command
 * @chain: the API CMD specific chain for the cmd
 * @cell: the cell in the HW for the cmd
 * @cell_idx: the index of the cell
 * Return: 0 - success, negative - failure
 */
static int alloc_cmd_buf(struct hinic_api_cmd_chain *chain,
			 struct hinic_api_cmd_cell *cell, u32 cell_idx)
{
	void *dev = chain->hwdev;
	struct hinic_api_cmd_cell_ctxt *cell_ctxt;
	dma_addr_t cmd_paddr = 0;
	void *cmd_vaddr;
	void *cmd_vaddr_alloc;
	int err = 0;

	cmd_vaddr_alloc = dma_zalloc_coherent(dev, (API_CMD_BUF_SIZE +
					      API_PAYLOAD_ALIGN_SIZE),
					      &cmd_paddr, SOCKET_ID_ANY);
	if (!cmd_vaddr_alloc) {
		PMD_DRV_LOG(ERR, "Allocate API CMD dma memory failed");
		return -ENOMEM;
	}

	cell_ctxt = &chain->cell_ctxt[cell_idx];

	cell_ctxt->api_cmd_paddr_free = cmd_paddr;
	cell_ctxt->api_cmd_vaddr_free = cmd_vaddr_alloc;
	cmd_vaddr = PTR_ALIGN(cmd_vaddr_alloc, API_PAYLOAD_ALIGN_SIZE);
	cmd_paddr = cmd_paddr + ((u64)cmd_vaddr - (u64)cmd_vaddr_alloc);

	cell_ctxt->api_cmd_vaddr = cmd_vaddr;
	cell_ctxt->api_cmd_paddr = cmd_paddr;

	/* set the cmd DMA address in the cell */
	switch (chain->chain_type) {
	case HINIC_API_CMD_PMD_WRITE_TO_MGMT:
	case HINIC_API_CMD_WRITE_ASYNC_TO_MGMT_CPU:
		cell->write.hw_cmd_paddr = cpu_to_be64(cmd_paddr);
		break;
	default:
		PMD_DRV_LOG(ERR, "Unknown API CMD chain type");
		free_cmd_buf(chain, cell_idx);
		err = -EINVAL;
		break;
	}

	return err;
}

/**
 * api_cmd_create_cell - create API CMD cell of specific chain
 * @chain: the API CMD specific chain to create its cell
 * @cell_idx: the cell index to create
 * @pre_node: previous cell
 * @node_vaddr: the virt addr of the cell
 * Return: 0 - success, negative - failure
 */
static int api_cmd_create_cell(struct hinic_api_cmd_chain *chain,
			       u32 cell_idx,
			       struct hinic_api_cmd_cell *pre_node,
			       struct hinic_api_cmd_cell **node_vaddr)
{
	void *dev = chain->hwdev;
	struct hinic_api_cmd_cell_ctxt *cell_ctxt;
	struct hinic_api_cmd_cell *node;
	dma_addr_t node_paddr = 0;
	void *node_vaddr_alloc;
	int err = 0;

	node_vaddr_alloc = dma_zalloc_coherent(dev, (chain->cell_size +
					       API_CMD_NODE_ALIGN_SIZE),
					       &node_paddr, SOCKET_ID_ANY);
	if (!node_vaddr_alloc) {
		PMD_DRV_LOG(ERR, "Allocate dma API CMD cell failed");
		return -ENOMEM;
	}

	cell_ctxt = &chain->cell_ctxt[cell_idx];

	cell_ctxt->cell_vaddr_free = node_vaddr_alloc;
	cell_ctxt->cell_paddr_free = node_paddr;
	node = (struct hinic_api_cmd_cell *)PTR_ALIGN(node_vaddr_alloc,
		API_CMD_NODE_ALIGN_SIZE);
	node_paddr = node_paddr + ((u64)node - (u64)node_vaddr_alloc);

	node->read.hw_wb_resp_paddr = 0;

	cell_ctxt->cell_vaddr = node;
	cell_ctxt->cell_paddr = node_paddr;

	if (!pre_node) {
		chain->head_node = node;
		chain->head_cell_paddr = node_paddr;
	} else {
		/* The data in the HW should be in Big Endian Format */
		pre_node->next_cell_paddr = cpu_to_be64(node_paddr);
	}

	/* Driver software should make sure that there is an empty
	 * API command cell at the end the chain
	 */
	node->next_cell_paddr = 0;

	switch (chain->chain_type) {
	case HINIC_API_CMD_WRITE_ASYNC_TO_MGMT_CPU:
	case HINIC_API_CMD_PMD_WRITE_TO_MGMT:
		err = alloc_cmd_buf(chain, node, cell_idx);
		if (err) {
			PMD_DRV_LOG(ERR, "Allocate cmd buffer failed");
			goto alloc_cmd_buf_err;
		}
		break;
	default:
		PMD_DRV_LOG(ERR, "Unsupported API CMD chain type");
		err = -EINVAL;
		goto alloc_cmd_buf_err;
	}

	*node_vaddr = node;

	return 0;

alloc_cmd_buf_err:
	dma_free_coherent(dev, (chain->cell_size + API_CMD_NODE_ALIGN_SIZE),
			  node_vaddr_alloc, cell_ctxt->cell_paddr_free);

	return err;
}

/**
 * api_cmd_destroy_cell - destroy API CMD cell of specific chain
 * @chain: the API CMD specific chain to destroy its cell
 * @cell_idx: the cell to destroy
 */
static void api_cmd_destroy_cell(struct hinic_api_cmd_chain *chain,
				 u32 cell_idx)
{
	void *dev = chain->hwdev;
	struct hinic_api_cmd_cell_ctxt *cell_ctxt;
	struct hinic_api_cmd_cell *node;
	dma_addr_t node_paddr;

	cell_ctxt = &chain->cell_ctxt[cell_idx];

	node = (struct hinic_api_cmd_cell *)(cell_ctxt->cell_vaddr_free);
	node_paddr = cell_ctxt->cell_paddr_free;

	if (cell_ctxt->api_cmd_vaddr) {
		switch (chain->chain_type) {
		case HINIC_API_CMD_PMD_WRITE_TO_MGMT:
		case HINIC_API_CMD_WRITE_ASYNC_TO_MGMT_CPU:
			free_cmd_buf(chain, cell_idx);
			break;
		default:
			break;
		}

		dma_free_coherent(dev, (chain->cell_size +
				  API_CMD_NODE_ALIGN_SIZE),
				  node, node_paddr);
	}
}

/**
 * api_cmd_destroy_cells - destroy API CMD cells of specific chain
 * @chain: the API CMD specific chain to destroy its cells
 * @num_cells: number of cells to destroy
 */
static void api_cmd_destroy_cells(struct hinic_api_cmd_chain *chain,
					 u32 num_cells)
{
	u32 cell_idx;

	for (cell_idx = 0; cell_idx < num_cells; cell_idx++)
		api_cmd_destroy_cell(chain, cell_idx);
}

/**
 * api_cmd_create_cells - create API CMD cells for specific chain
 * @chain: the API CMD specific chain
 * Return: 0 - success, negative - failure
 */
static int api_cmd_create_cells(struct hinic_api_cmd_chain *chain)
{
	struct hinic_api_cmd_cell *node = NULL, *pre_node = NULL;
	u32 cell_idx;
	int err;

	for (cell_idx = 0; cell_idx < chain->num_cells; cell_idx++) {
		err = api_cmd_create_cell(chain, cell_idx, pre_node, &node);
		if (err) {
			PMD_DRV_LOG(ERR, "Create API CMD cell failed");
			goto create_cell_err;
		}

		pre_node = node;
	}

	if (!node) {
		err = -EFAULT;
		goto create_cell_err;
	}

	/* set the Final node to point on the start */
	node->next_cell_paddr = cpu_to_be64(chain->head_cell_paddr);

	/* set the current node to be the head */
	chain->curr_node = chain->head_node;
	return 0;

create_cell_err:
	api_cmd_destroy_cells(chain, cell_idx);
	return err;
}

/**
 * api_chain_init - initialize API CMD specific chain
 * @chain: the API CMD specific chain to initialize
 * @attr: attributes to set in the chain
 * Return: 0 - success, negative - failure
 */
static int api_chain_init(struct hinic_api_cmd_chain *chain,
			  struct hinic_api_cmd_chain_attr *attr)
{
	void *dev = chain->hwdev;
	size_t cell_ctxt_size;
	int err;

	chain->chain_type  = attr->chain_type;
	chain->num_cells = attr->num_cells;
	chain->cell_size = attr->cell_size;
	chain->rsp_size = attr->rsp_size;

	chain->prod_idx  = 0;
	chain->cons_idx  = 0;

	spin_lock_init(&chain->async_lock);

	cell_ctxt_size = chain->num_cells * sizeof(*chain->cell_ctxt);
	chain->cell_ctxt = kzalloc(cell_ctxt_size, GFP_KERNEL);
	if (!chain->cell_ctxt) {
		PMD_DRV_LOG(ERR, "Allocate cell contexts for a chain failed");
		err = -ENOMEM;
		goto alloc_cell_ctxt_err;
	}

	chain->wb_status = (struct hinic_api_cmd_status *)
			   dma_zalloc_coherent(dev, sizeof(*chain->wb_status),
				&chain->wb_status_paddr, SOCKET_ID_ANY);
	if (!chain->wb_status) {
		PMD_DRV_LOG(ERR, "Allocate DMA wb status failed");
		err = -ENOMEM;
		goto alloc_wb_status_err;
	}

	return 0;

alloc_wb_status_err:
	kfree(chain->cell_ctxt);

alloc_cell_ctxt_err:

	return err;
}

/**
 * api_chain_free - free API CMD specific chain
 * @chain: the API CMD specific chain to free
 */
static void api_chain_free(struct hinic_api_cmd_chain *chain)
{
	void *dev = chain->hwdev;

	dma_free_coherent(dev, sizeof(*chain->wb_status),
			  chain->wb_status, chain->wb_status_paddr);
	kfree(chain->cell_ctxt);
}

/**
 * api_cmd_create_chain - create API CMD specific chain
 * @cmd_chain: the API CMD specific chain to create
 * @attr: attributes to set in the chain
 * Return: 0 - success, negative - failure
 */
static int api_cmd_create_chain(struct hinic_api_cmd_chain **cmd_chain,
				struct hinic_api_cmd_chain_attr *attr)
{
	struct hinic_hwdev *hwdev = attr->hwdev;
	struct hinic_api_cmd_chain *chain;
	int err;

	if (attr->num_cells & (attr->num_cells - 1)) {
		PMD_DRV_LOG(ERR, "Invalid number of cells, must be power of 2");
		return -EINVAL;
	}

	chain = kzalloc(sizeof(*chain), GFP_KERNEL);
	if (!chain) {
		PMD_DRV_LOG(ERR, "Allocate memory for the chain failed");
		return -ENOMEM;
	}

	chain->hwdev = hwdev;

	err = api_chain_init(chain, attr);
	if (err) {
		PMD_DRV_LOG(ERR, "Initialize chain failed");
		goto chain_init_err;
	}

	err = api_cmd_create_cells(chain);
	if (err) {
		PMD_DRV_LOG(ERR, "Create cells for API CMD chain failed");
		goto create_cells_err;
	}

	err = api_cmd_chain_hw_init(chain);
	if (err) {
		PMD_DRV_LOG(ERR, "Initialize chain hw info failed");
		goto chain_hw_init_err;
	}

	*cmd_chain = chain;
	return 0;

chain_hw_init_err:
	api_cmd_destroy_cells(chain, chain->num_cells);

create_cells_err:
	api_chain_free(chain);

chain_init_err:
	kfree(chain);
	return err;
}

/**
 * api_cmd_destroy_chain - destroy API CMD specific chain
 * @chain: the API CMD specific chain to destroy
 */
static void api_cmd_destroy_chain(struct hinic_api_cmd_chain *chain)
{
	api_cmd_destroy_cells(chain, chain->num_cells);
	api_chain_free(chain);
	kfree(chain);
}

/**
 * hinic_api_cmd_init - Initialize all the API CMD chains
 * @hwdev: the hardware interface of a pci function device
 * @chain: the API CMD chains that will be initialized
 * Return: 0 - success, negative - failure
 */
int hinic_api_cmd_init(struct hinic_hwdev *hwdev,
		       struct hinic_api_cmd_chain **chain)
{
	struct hinic_api_cmd_chain_attr attr;
	enum hinic_api_cmd_chain_type chain_type, i;
	int err;

	attr.hwdev = hwdev;
	attr.num_cells  = API_CHAIN_NUM_CELLS;
	attr.cell_size  = API_CHAIN_CELL_SIZE;
	attr.rsp_size	= API_CHAIN_RSP_DATA_SIZE;

	chain_type = HINIC_API_CMD_WRITE_ASYNC_TO_MGMT_CPU;
	for ( ; chain_type < HINIC_API_CMD_MAX; chain_type++) {
		attr.chain_type = chain_type;
		err = api_cmd_create_chain(&chain[chain_type], &attr);
		if (err) {
			PMD_DRV_LOG(ERR, "Create chain %d failed",
				chain_type);
			goto create_chain_err;
		}
	}

	return 0;

create_chain_err:
	i = HINIC_API_CMD_WRITE_ASYNC_TO_MGMT_CPU;
	for (; i < chain_type; i++)
		api_cmd_destroy_chain(chain[i]);

	return err;
}

/**
 * hinic_api_cmd_free - free the API CMD chains
 * @chain: the API CMD chains that will be freed
 */
void hinic_api_cmd_free(struct hinic_api_cmd_chain **chain)
{
	enum hinic_api_cmd_chain_type chain_type;

	chain_type = HINIC_API_CMD_WRITE_ASYNC_TO_MGMT_CPU;
	for ( ; chain_type < HINIC_API_CMD_MAX; chain_type++)
		api_cmd_destroy_chain(chain[chain_type]);
}
