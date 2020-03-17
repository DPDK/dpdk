/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2019 Hisilicon Limited.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/queue.h>
#include <inttypes.h>
#include <unistd.h>
#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_dev.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_pci.h>
#include <rte_io.h>

#include "hns3_ethdev.h"
#include "hns3_regs.h"
#include "hns3_intr.h"
#include "hns3_logs.h"

#define hns3_is_csq(ring) ((ring)->flag & HNS3_TYPE_CSQ)

#define cmq_ring_to_dev(ring)   (&(ring)->dev->pdev->dev)

static int
hns3_ring_space(struct hns3_cmq_ring *ring)
{
	int ntu = ring->next_to_use;
	int ntc = ring->next_to_clean;
	int used = (ntu - ntc + ring->desc_num) % ring->desc_num;

	return ring->desc_num - used - 1;
}

static bool
is_valid_csq_clean_head(struct hns3_cmq_ring *ring, int head)
{
	int ntu = ring->next_to_use;
	int ntc = ring->next_to_clean;

	if (ntu > ntc)
		return head >= ntc && head <= ntu;

	return head >= ntc || head <= ntu;
}

/*
 * hns3_allocate_dma_mem - Specific memory alloc for command function.
 * Malloc a memzone, which is a contiguous portion of physical memory identified
 * by a name.
 * @ring: pointer to the ring structure
 * @size: size of memory requested
 * @alignment: what to align the allocation to
 */
static int
hns3_allocate_dma_mem(struct hns3_hw *hw, struct hns3_cmq_ring *ring,
		      uint64_t size, uint32_t alignment)
{
	const struct rte_memzone *mz = NULL;
	char z_name[RTE_MEMZONE_NAMESIZE];

	snprintf(z_name, sizeof(z_name), "hns3_dma_%" PRIu64, rte_rand());
	mz = rte_memzone_reserve_bounded(z_name, size, SOCKET_ID_ANY,
					 RTE_MEMZONE_IOVA_CONTIG, alignment,
					 RTE_PGSIZE_2M);
	if (mz == NULL)
		return -ENOMEM;

	ring->buf_size = size;
	ring->desc = mz->addr;
	ring->desc_dma_addr = mz->iova;
	ring->zone = (const void *)mz;
	hns3_dbg(hw, "memzone %s allocated with physical address: %" PRIu64,
		 mz->name, ring->desc_dma_addr);

	return 0;
}

static void
hns3_free_dma_mem(struct hns3_hw *hw, struct hns3_cmq_ring *ring)
{
	hns3_dbg(hw, "memzone %s to be freed with physical address: %" PRIu64,
		 ((const struct rte_memzone *)ring->zone)->name,
		 ring->desc_dma_addr);
	rte_memzone_free((const struct rte_memzone *)ring->zone);
	ring->buf_size = 0;
	ring->desc = NULL;
	ring->desc_dma_addr = 0;
	ring->zone = NULL;
}

static int
hns3_alloc_cmd_desc(struct hns3_hw *hw, struct hns3_cmq_ring *ring)
{
	int size  = ring->desc_num * sizeof(struct hns3_cmd_desc);

	if (hns3_allocate_dma_mem(hw, ring, size, HNS3_CMD_DESC_ALIGNMENT)) {
		hns3_err(hw, "allocate dma mem failed");
		return -ENOMEM;
	}

	return 0;
}

static void
hns3_free_cmd_desc(struct hns3_hw *hw, struct hns3_cmq_ring *ring)
{
	if (ring->desc)
		hns3_free_dma_mem(hw, ring);
}

static int
hns3_alloc_cmd_queue(struct hns3_hw *hw, int ring_type)
{
	struct hns3_cmq_ring *ring =
		(ring_type == HNS3_TYPE_CSQ) ? &hw->cmq.csq : &hw->cmq.crq;
	int ret;

	ring->ring_type = ring_type;
	ring->hw = hw;

	ret = hns3_alloc_cmd_desc(hw, ring);
	if (ret)
		hns3_err(hw, "descriptor %s alloc error %d",
			    (ring_type == HNS3_TYPE_CSQ) ? "CSQ" : "CRQ", ret);

	return ret;
}

void
hns3_cmd_reuse_desc(struct hns3_cmd_desc *desc, bool is_read)
{
	desc->flag = rte_cpu_to_le_16(HNS3_CMD_FLAG_NO_INTR | HNS3_CMD_FLAG_IN);
	if (is_read)
		desc->flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_WR);
	else
		desc->flag &= rte_cpu_to_le_16(~HNS3_CMD_FLAG_WR);
}

void
hns3_cmd_setup_basic_desc(struct hns3_cmd_desc *desc,
			  enum hns3_opcode_type opcode, bool is_read)
{
	memset((void *)desc, 0, sizeof(struct hns3_cmd_desc));
	desc->opcode = rte_cpu_to_le_16(opcode);
	desc->flag = rte_cpu_to_le_16(HNS3_CMD_FLAG_NO_INTR | HNS3_CMD_FLAG_IN);

	if (is_read)
		desc->flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_WR);
}

static void
hns3_cmd_clear_regs(struct hns3_hw *hw)
{
	hns3_write_dev(hw, HNS3_CMDQ_TX_ADDR_L_REG, 0);
	hns3_write_dev(hw, HNS3_CMDQ_TX_ADDR_H_REG, 0);
	hns3_write_dev(hw, HNS3_CMDQ_TX_DEPTH_REG, 0);
	hns3_write_dev(hw, HNS3_CMDQ_TX_HEAD_REG, 0);
	hns3_write_dev(hw, HNS3_CMDQ_TX_TAIL_REG, 0);
	hns3_write_dev(hw, HNS3_CMDQ_RX_ADDR_L_REG, 0);
	hns3_write_dev(hw, HNS3_CMDQ_RX_ADDR_H_REG, 0);
	hns3_write_dev(hw, HNS3_CMDQ_RX_DEPTH_REG, 0);
	hns3_write_dev(hw, HNS3_CMDQ_RX_HEAD_REG, 0);
	hns3_write_dev(hw, HNS3_CMDQ_RX_TAIL_REG, 0);
}

static void
hns3_cmd_config_regs(struct hns3_cmq_ring *ring)
{
	uint64_t dma = ring->desc_dma_addr;

	if (ring->ring_type == HNS3_TYPE_CSQ) {
		hns3_write_dev(ring->hw, HNS3_CMDQ_TX_ADDR_L_REG,
			       lower_32_bits(dma));
		hns3_write_dev(ring->hw, HNS3_CMDQ_TX_ADDR_H_REG,
			       upper_32_bits(dma));
		hns3_write_dev(ring->hw, HNS3_CMDQ_TX_DEPTH_REG,
			       ring->desc_num >> HNS3_NIC_CMQ_DESC_NUM_S |
			       HNS3_NIC_SW_RST_RDY);
		hns3_write_dev(ring->hw, HNS3_CMDQ_TX_HEAD_REG, 0);
		hns3_write_dev(ring->hw, HNS3_CMDQ_TX_TAIL_REG, 0);
	} else {
		hns3_write_dev(ring->hw, HNS3_CMDQ_RX_ADDR_L_REG,
			       lower_32_bits(dma));
		hns3_write_dev(ring->hw, HNS3_CMDQ_RX_ADDR_H_REG,
			       upper_32_bits(dma));
		hns3_write_dev(ring->hw, HNS3_CMDQ_RX_DEPTH_REG,
			       ring->desc_num >> HNS3_NIC_CMQ_DESC_NUM_S);
		hns3_write_dev(ring->hw, HNS3_CMDQ_RX_HEAD_REG, 0);
		hns3_write_dev(ring->hw, HNS3_CMDQ_RX_TAIL_REG, 0);
	}
}

static void
hns3_cmd_init_regs(struct hns3_hw *hw)
{
	hns3_cmd_config_regs(&hw->cmq.csq);
	hns3_cmd_config_regs(&hw->cmq.crq);
}

static int
hns3_cmd_csq_clean(struct hns3_hw *hw)
{
	struct hns3_cmq_ring *csq = &hw->cmq.csq;
	uint32_t head;
	int clean;

	head = hns3_read_dev(hw, HNS3_CMDQ_TX_HEAD_REG);

	if (!is_valid_csq_clean_head(csq, head)) {
		hns3_err(hw, "wrong cmd head (%u, %u-%u)", head,
			    csq->next_to_use, csq->next_to_clean);
		if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
			rte_atomic16_set(&hw->reset.disable_cmd, 1);
			hns3_schedule_delayed_reset(HNS3_DEV_HW_TO_ADAPTER(hw));
		}

		return -EIO;
	}

	clean = (head - csq->next_to_clean + csq->desc_num) % csq->desc_num;
	csq->next_to_clean = head;
	return clean;
}

static int
hns3_cmd_csq_done(struct hns3_hw *hw)
{
	uint32_t head = hns3_read_dev(hw, HNS3_CMDQ_TX_HEAD_REG);

	return head == hw->cmq.csq.next_to_use;
}

static bool
hns3_is_special_opcode(uint16_t opcode)
{
	/*
	 * These commands have several descriptors,
	 * and use the first one to save opcode and return value.
	 */
	uint16_t spec_opcode[] = {HNS3_OPC_STATS_64_BIT,
				  HNS3_OPC_STATS_32_BIT,
				  HNS3_OPC_STATS_MAC,
				  HNS3_OPC_STATS_MAC_ALL,
				  HNS3_OPC_QUERY_32_BIT_REG,
				  HNS3_OPC_QUERY_64_BIT_REG};
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(spec_opcode); i++)
		if (spec_opcode[i] == opcode)
			return true;

	return false;
}

static int
hns3_cmd_convert_err_code(uint16_t desc_ret)
{
	switch (desc_ret) {
	case HNS3_CMD_EXEC_SUCCESS:
		return 0;
	case HNS3_CMD_NO_AUTH:
		return -EPERM;
	case HNS3_CMD_NOT_SUPPORTED:
		return -EOPNOTSUPP;
	case HNS3_CMD_QUEUE_FULL:
		return -EXFULL;
	case HNS3_CMD_NEXT_ERR:
		return -ENOSR;
	case HNS3_CMD_UNEXE_ERR:
		return -ENOTBLK;
	case HNS3_CMD_PARA_ERR:
		return -EINVAL;
	case HNS3_CMD_RESULT_ERR:
		return -ERANGE;
	case HNS3_CMD_TIMEOUT:
		return -ETIME;
	case HNS3_CMD_HILINK_ERR:
		return -ENOLINK;
	case HNS3_CMD_QUEUE_ILLEGAL:
		return -ENXIO;
	case HNS3_CMD_INVALID:
		return -EBADR;
	default:
		return -EREMOTEIO;
	}
}

static int
hns3_cmd_get_hardware_reply(struct hns3_hw *hw,
			    struct hns3_cmd_desc *desc, int num, int ntc)
{
	uint16_t opcode, desc_ret;
	int current_ntc = ntc;
	int handle;

	opcode = rte_le_to_cpu_16(desc[0].opcode);
	for (handle = 0; handle < num; handle++) {
		/* Get the result of hardware write back */
		desc[handle] = hw->cmq.csq.desc[current_ntc];

		current_ntc++;
		if (current_ntc == hw->cmq.csq.desc_num)
			current_ntc = 0;
	}

	if (likely(!hns3_is_special_opcode(opcode)))
		desc_ret = rte_le_to_cpu_16(desc[num - 1].retval);
	else
		desc_ret = rte_le_to_cpu_16(desc[0].retval);

	hw->cmq.last_status = desc_ret;
	return hns3_cmd_convert_err_code(desc_ret);
}

static int hns3_cmd_poll_reply(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	uint32_t timeout = 0;

	do {
		if (hns3_cmd_csq_done(hw))
			return 0;

		if (rte_atomic16_read(&hw->reset.disable_cmd)) {
			hns3_err(hw,
				 "Don't wait for reply because of disable_cmd");
			return -EBUSY;
		}

		if (is_reset_pending(hns)) {
			hns3_err(hw, "Don't wait for reply because of reset pending");
			return -EIO;
		}

		rte_delay_us(1);
		timeout++;
	} while (timeout < hw->cmq.tx_timeout);
	hns3_err(hw, "Wait for reply timeout");
	return -ETIME;
}

/*
 * hns3_cmd_send - send command to command queue
 *
 * @param hw
 *   pointer to the hw struct
 * @param desc
 *   prefilled descriptor for describing the command
 * @param num
 *   the number of descriptors to be sent
 * @return
 *   - -EBUSY if detect device is in resetting
 *   - -EIO   if detect cmd csq corrupted (due to reset) or
 *            there is reset pending
 *   - -ENOMEM/-ETIME/...(Non-Zero) if other error case
 *   - Zero   if operation completed successfully
 *
 * Note -BUSY/-EIO only used in reset case
 *
 * Note this is the main send command for command queue, it
 * sends the queue, cleans the queue, etc
 */
int
hns3_cmd_send(struct hns3_hw *hw, struct hns3_cmd_desc *desc, int num)
{
	struct hns3_cmd_desc *desc_to_use;
	int handle = 0;
	int retval;
	uint32_t ntc;

	if (rte_atomic16_read(&hw->reset.disable_cmd))
		return -EBUSY;

	rte_spinlock_lock(&hw->cmq.csq.lock);

	/* Clean the command send queue */
	retval = hns3_cmd_csq_clean(hw);
	if (retval < 0) {
		rte_spinlock_unlock(&hw->cmq.csq.lock);
		return retval;
	}

	if (num > hns3_ring_space(&hw->cmq.csq)) {
		rte_spinlock_unlock(&hw->cmq.csq.lock);
		return -ENOMEM;
	}

	/*
	 * Record the location of desc in the ring for this time
	 * which will be use for hardware to write back
	 */
	ntc = hw->cmq.csq.next_to_use;

	while (handle < num) {
		desc_to_use = &hw->cmq.csq.desc[hw->cmq.csq.next_to_use];
		*desc_to_use = desc[handle];
		(hw->cmq.csq.next_to_use)++;
		if (hw->cmq.csq.next_to_use == hw->cmq.csq.desc_num)
			hw->cmq.csq.next_to_use = 0;
		handle++;
	}

	/* Write to hardware */
	hns3_write_dev(hw, HNS3_CMDQ_TX_TAIL_REG, hw->cmq.csq.next_to_use);

	/*
	 * If the command is sync, wait for the firmware to write back,
	 * if multi descriptors to be sent, use the first one to check.
	 */
	if (HNS3_CMD_SEND_SYNC(rte_le_to_cpu_16(desc->flag))) {
		retval = hns3_cmd_poll_reply(hw);
		if (!retval)
			retval = hns3_cmd_get_hardware_reply(hw, desc, num,
							     ntc);
	}

	rte_spinlock_unlock(&hw->cmq.csq.lock);
	return retval;
}

static enum hns3_cmd_status
hns3_cmd_query_firmware_version(struct hns3_hw *hw, uint32_t *version)
{
	struct hns3_query_version_cmd *resp;
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_QUERY_FW_VER, 1);
	resp = (struct hns3_query_version_cmd *)desc.data;

	/* Initialize the cmd function */
	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret == 0)
		*version = rte_le_to_cpu_32(resp->firmware);

	return ret;
}

int
hns3_cmd_init_queue(struct hns3_hw *hw)
{
	int ret;

	/* Setup the lock for command queue */
	rte_spinlock_init(&hw->cmq.csq.lock);
	rte_spinlock_init(&hw->cmq.crq.lock);

	/*
	 * Clear up all command register,
	 * in case there are some residual values
	 */
	hns3_cmd_clear_regs(hw);

	/* Setup the queue entries for use cmd queue */
	hw->cmq.csq.desc_num = HNS3_NIC_CMQ_DESC_NUM;
	hw->cmq.crq.desc_num = HNS3_NIC_CMQ_DESC_NUM;

	/* Setup Tx write back timeout */
	hw->cmq.tx_timeout = HNS3_CMDQ_TX_TIMEOUT;

	/* Setup queue rings */
	ret = hns3_alloc_cmd_queue(hw, HNS3_TYPE_CSQ);
	if (ret) {
		PMD_INIT_LOG(ERR, "CSQ ring setup error %d", ret);
		return ret;
	}

	ret = hns3_alloc_cmd_queue(hw, HNS3_TYPE_CRQ);
	if (ret) {
		PMD_INIT_LOG(ERR, "CRQ ring setup error %d", ret);
		goto err_crq;
	}

	return 0;

err_crq:
	hns3_free_cmd_desc(hw, &hw->cmq.csq);

	return ret;
}

int
hns3_cmd_init(struct hns3_hw *hw)
{
	int ret;

	rte_spinlock_lock(&hw->cmq.csq.lock);
	rte_spinlock_lock(&hw->cmq.crq.lock);

	hw->cmq.csq.next_to_clean = 0;
	hw->cmq.csq.next_to_use = 0;
	hw->cmq.crq.next_to_clean = 0;
	hw->cmq.crq.next_to_use = 0;
	hw->mbx_resp.head = 0;
	hw->mbx_resp.tail = 0;
	hw->mbx_resp.lost = 0;
	hns3_cmd_init_regs(hw);

	rte_spinlock_unlock(&hw->cmq.crq.lock);
	rte_spinlock_unlock(&hw->cmq.csq.lock);

	/*
	 * Check if there is new reset pending, because the higher level
	 * reset may happen when lower level reset is being processed.
	 */
	if (is_reset_pending(HNS3_DEV_HW_TO_ADAPTER(hw))) {
		PMD_INIT_LOG(ERR, "New reset pending, keep disable cmd");
		ret = -EBUSY;
		goto err_cmd_init;
	}
	rte_atomic16_clear(&hw->reset.disable_cmd);

	ret = hns3_cmd_query_firmware_version(hw, &hw->fw_version);
	if (ret) {
		PMD_INIT_LOG(ERR, "firmware version query failed %d", ret);
		goto err_cmd_init;
	}

	PMD_INIT_LOG(INFO, "The firmware version is %08x", hw->fw_version);

	return 0;

err_cmd_init:
	rte_atomic16_set(&hw->reset.disable_cmd, 1);
	return ret;
}

static void
hns3_destroy_queue(struct hns3_hw *hw, struct hns3_cmq_ring *ring)
{
	rte_spinlock_lock(&ring->lock);

	hns3_free_cmd_desc(hw, ring);

	rte_spinlock_unlock(&ring->lock);
}

void
hns3_cmd_destroy_queue(struct hns3_hw *hw)
{
	hns3_destroy_queue(hw, &hw->cmq.csq);
	hns3_destroy_queue(hw, &hw->cmq.crq);
}

void
hns3_cmd_uninit(struct hns3_hw *hw)
{
	rte_spinlock_lock(&hw->cmq.csq.lock);
	rte_spinlock_lock(&hw->cmq.crq.lock);
	rte_atomic16_set(&hw->reset.disable_cmd, 1);
	hns3_cmd_clear_regs(hw);
	rte_spinlock_unlock(&hw->cmq.crq.lock);
	rte_spinlock_unlock(&hw->cmq.csq.lock);
}
