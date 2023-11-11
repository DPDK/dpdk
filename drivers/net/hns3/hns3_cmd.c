/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 HiSilicon Limited.
 */

#include <ethdev_pci.h>
#include <rte_io.h>

#include "hns3_common.h"
#include "hns3_regs.h"
#include "hns3_intr.h"
#include "hns3_logs.h"

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
	static uint64_t hns3_dma_memzone_id;
	const struct rte_memzone *mz = NULL;
	char z_name[RTE_MEMZONE_NAMESIZE];

	snprintf(z_name, sizeof(z_name), "hns3_dma_%" PRIu64,
		__atomic_fetch_add(&hns3_dma_memzone_id, 1, __ATOMIC_RELAXED));
	mz = rte_memzone_reserve_bounded(z_name, size, SOCKET_ID_ANY,
					 RTE_MEMZONE_IOVA_CONTIG, alignment,
					 RTE_PGSIZE_2M);
	if (mz == NULL)
		return -ENOMEM;

	ring->buf_size = size;
	ring->desc = mz->addr;
	ring->desc_dma_addr = mz->iova;
	ring->zone = (const void *)mz;
	hns3_dbg(hw, "cmd ring memzone name: %s", mz->name);

	return 0;
}

static void
hns3_free_dma_mem(struct hns3_cmq_ring *ring)
{
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
hns3_free_cmd_desc(__rte_unused struct hns3_hw *hw, struct hns3_cmq_ring *ring)
{
	if (ring->desc)
		hns3_free_dma_mem(ring);
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
	uint32_t addr;
	int clean;

	head = hns3_read_dev(hw, HNS3_CMDQ_TX_HEAD_REG);
	addr = hns3_read_dev(hw, HNS3_CMDQ_TX_ADDR_L_REG);
	if (!is_valid_csq_clean_head(csq, head) || addr == 0) {
		hns3_err(hw, "wrong cmd addr(%0x) head (%u, %u-%u)", addr, head,
			 csq->next_to_use, csq->next_to_clean);
		if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
			__atomic_store_n(&hw->reset.disable_cmd, 1,
					 __ATOMIC_RELAXED);
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
				  HNS3_OPC_QUERY_64_BIT_REG,
				  HNS3_OPC_QUERY_CLEAR_MPF_RAS_INT,
				  HNS3_OPC_QUERY_CLEAR_PF_RAS_INT,
				  HNS3_OPC_QUERY_CLEAR_ALL_MPF_MSIX_INT,
				  HNS3_OPC_QUERY_CLEAR_ALL_PF_MSIX_INT,
				  HNS3_OPC_QUERY_ALL_ERR_INFO,};
	uint32_t i;

	for (i = 0; i < RTE_DIM(spec_opcode); i++)
		if (spec_opcode[i] == opcode)
			return true;

	return false;
}

static int
hns3_cmd_convert_err_code(uint16_t desc_ret)
{
	static const struct {
		uint16_t imp_errcode;
		int linux_errcode;
	} hns3_cmdq_status[] = {
		{HNS3_CMD_EXEC_SUCCESS, 0},
		{HNS3_CMD_NO_AUTH, -EPERM},
		{HNS3_CMD_NOT_SUPPORTED, -EOPNOTSUPP},
		{HNS3_CMD_QUEUE_FULL, -EXFULL},
		{HNS3_CMD_NEXT_ERR, -ENOSR},
		{HNS3_CMD_UNEXE_ERR, -ENOTBLK},
		{HNS3_CMD_PARA_ERR, -EINVAL},
		{HNS3_CMD_RESULT_ERR, -ERANGE},
		{HNS3_CMD_TIMEOUT, -ETIME},
		{HNS3_CMD_HILINK_ERR, -ENOLINK},
		{HNS3_CMD_QUEUE_ILLEGAL, -ENXIO},
		{HNS3_CMD_INVALID, -EBADR},
		{HNS3_CMD_ROH_CHECK_FAIL, -EINVAL}
	};

	uint32_t i;

	for (i = 0; i < RTE_DIM(hns3_cmdq_status); i++)
		if (hns3_cmdq_status[i].imp_errcode == desc_ret)
			return hns3_cmdq_status[i].linux_errcode;

	return -EREMOTEIO;
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

		if (__atomic_load_n(&hw->reset.disable_cmd, __ATOMIC_RELAXED)) {
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

	if (__atomic_load_n(&hw->reset.disable_cmd, __ATOMIC_RELAXED))
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

static const char *
hns3_get_caps_name(uint32_t caps_id)
{
	const struct {
		enum HNS3_CAPS_BITS caps;
		const char *name;
	} dev_caps[] = {
		{ HNS3_CAPS_FD_QUEUE_REGION_B, "fd_queue_region" },
		{ HNS3_CAPS_PTP_B,             "ptp"             },
		{ HNS3_CAPS_SIMPLE_BD_B,       "simple_bd"       },
		{ HNS3_CAPS_TX_PUSH_B,         "tx_push"         },
		{ HNS3_CAPS_PHY_IMP_B,         "phy_imp"         },
		{ HNS3_CAPS_TQP_TXRX_INDEP_B,  "tqp_txrx_indep"  },
		{ HNS3_CAPS_HW_PAD_B,          "hw_pad"          },
		{ HNS3_CAPS_STASH_B,           "stash"           },
		{ HNS3_CAPS_UDP_TUNNEL_CSUM_B, "udp_tunnel_csum" },
		{ HNS3_CAPS_RAS_IMP_B,         "ras_imp"         },
		{ HNS3_CAPS_RXD_ADV_LAYOUT_B,  "rxd_adv_layout"  },
		{ HNS3_CAPS_TM_B,              "tm_capability"   },
		{ HNS3_CAPS_FC_AUTO_B,         "fc_autoneg"      }
	};
	uint32_t i;

	for (i = 0; i < RTE_DIM(dev_caps); i++) {
		if (dev_caps[i].caps == caps_id)
			return dev_caps[i].name;
	}

	return "unknown";
}

static void
hns3_mask_capability(struct hns3_hw *hw,
		     struct hns3_query_version_cmd *cmd)
{
#define MAX_CAPS_BIT	64

	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	uint64_t caps_org, caps_new, caps_masked;
	uint32_t i;

	if (hns->dev_caps_mask == 0)
		return;

	memcpy(&caps_org, &cmd->caps[0], sizeof(caps_org));
	caps_org = rte_le_to_cpu_64(caps_org);
	caps_new = caps_org ^ (caps_org & hns->dev_caps_mask);
	caps_masked = caps_org ^ caps_new;
	caps_new = rte_cpu_to_le_64(caps_new);
	memcpy(&cmd->caps[0], &caps_new, sizeof(caps_new));

	for (i = 0; i < MAX_CAPS_BIT; i++) {
		if (!(caps_masked & BIT_ULL(i)))
			continue;
		hns3_info(hw, "mask capability: id-%u, name-%s.",
			  i, hns3_get_caps_name(i));
	}
}

static void
hns3_parse_capability(struct hns3_hw *hw,
		      struct hns3_query_version_cmd *cmd)
{
	uint32_t caps = rte_le_to_cpu_32(cmd->caps[0]);

	if (hns3_get_bit(caps, HNS3_CAPS_FD_QUEUE_REGION_B))
		hns3_set_bit(hw->capability, HNS3_DEV_SUPPORT_FD_QUEUE_REGION_B,
			     1);
	if (hns3_get_bit(caps, HNS3_CAPS_PTP_B)) {
		/*
		 * PTP depends on special packet type reported by hardware which
		 * enabled rxd advanced layout, so if the hardware doesn't
		 * support rxd advanced layout, driver should ignore the PTP
		 * capability.
		 */
		if (hns3_get_bit(caps, HNS3_CAPS_RXD_ADV_LAYOUT_B))
			hns3_set_bit(hw->capability, HNS3_DEV_SUPPORT_PTP_B, 1);
		else
			hns3_warn(hw, "ignore PTP capability due to lack of "
				  "rxd advanced layout capability.");
	}
	if (hns3_get_bit(caps, HNS3_CAPS_SIMPLE_BD_B))
		hns3_set_bit(hw->capability, HNS3_DEV_SUPPORT_SIMPLE_BD_B, 1);
	if (hns3_get_bit(caps, HNS3_CAPS_TX_PUSH_B))
		hns3_set_bit(hw->capability, HNS3_DEV_SUPPORT_TX_PUSH_B, 1);
	if (hns3_get_bit(caps, HNS3_CAPS_PHY_IMP_B))
		hns3_set_bit(hw->capability, HNS3_DEV_SUPPORT_COPPER_B, 1);
	if (hns3_get_bit(caps, HNS3_CAPS_TQP_TXRX_INDEP_B))
		hns3_set_bit(hw->capability, HNS3_DEV_SUPPORT_INDEP_TXRX_B, 1);
	if (hns3_get_bit(caps, HNS3_CAPS_STASH_B))
		hns3_set_bit(hw->capability, HNS3_DEV_SUPPORT_STASH_B, 1);
	if (hns3_get_bit(caps, HNS3_CAPS_RXD_ADV_LAYOUT_B))
		hns3_set_bit(hw->capability, HNS3_DEV_SUPPORT_RXD_ADV_LAYOUT_B,
			     1);
	if (hns3_get_bit(caps, HNS3_CAPS_UDP_TUNNEL_CSUM_B))
		hns3_set_bit(hw->capability,
				HNS3_DEV_SUPPORT_OUTER_UDP_CKSUM_B, 1);
	if (hns3_get_bit(caps, HNS3_CAPS_RAS_IMP_B))
		hns3_set_bit(hw->capability, HNS3_DEV_SUPPORT_RAS_IMP_B, 1);
	if (hns3_get_bit(caps, HNS3_CAPS_TM_B))
		hns3_set_bit(hw->capability, HNS3_DEV_SUPPORT_TM_B, 1);
	if (hns3_get_bit(caps, HNS3_CAPS_FC_AUTO_B))
		hns3_set_bit(hw->capability, HNS3_DEV_SUPPORT_FC_AUTO_B, 1);
	if (hns3_get_bit(caps, HNS3_CAPS_GRO_B))
		hns3_set_bit(hw->capability, HNS3_DEV_SUPPORT_GRO_B, 1);
}

static uint32_t
hns3_build_api_caps(void)
{
	uint32_t api_caps = 0;

	hns3_set_bit(api_caps, HNS3_API_CAP_FLEX_RSS_TBL_B, 1);

	return rte_cpu_to_le_32(api_caps);
}

static void
hns3_set_dcb_capability(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct rte_pci_device *pci_dev;
	struct rte_eth_dev *eth_dev;
	uint16_t device_id;

	if (hns->is_vf)
		return;

	eth_dev = &rte_eth_devices[hw->data->port_id];
	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	device_id = pci_dev->id.device_id;

	if (device_id == HNS3_DEV_ID_25GE_RDMA ||
	    device_id == HNS3_DEV_ID_50GE_RDMA ||
	    device_id == HNS3_DEV_ID_100G_RDMA_MACSEC ||
	    device_id == HNS3_DEV_ID_200G_RDMA)
		hns3_set_bit(hw->capability, HNS3_DEV_SUPPORT_DCB_B, 1);
}

static void
hns3_set_default_capability(struct hns3_hw *hw)
{
	hns3_set_dcb_capability(hw);

	/*
	 * The firmware of the network engines with HIP08 do not report some
	 * capabilities, like GRO. Set default capabilities for it.
	 */
	if (hw->revision < PCI_REVISION_ID_HIP09_A)
		hns3_set_bit(hw->capability, HNS3_DEV_SUPPORT_GRO_B, 1);
}

static int
hns3_cmd_query_firmware_version_and_capability(struct hns3_hw *hw)
{
	struct hns3_query_version_cmd *resp;
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_QUERY_FW_VER, 1);
	resp = (struct hns3_query_version_cmd *)desc.data;
	resp->api_caps = hns3_build_api_caps();

	/* Initialize the cmd function */
	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		return ret;

	hw->fw_version = rte_le_to_cpu_32(resp->firmware);

	hns3_set_default_capability(hw);

	/*
	 * Make sure mask the capability before parse capability because it
	 * may overwrite resp's data.
	 */
	hns3_mask_capability(hw, resp);
	hns3_parse_capability(hw, resp);

	return 0;
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

static void
hns3_update_dev_lsc_cap(struct hns3_hw *hw, int fw_compact_cmd_result)
{
	struct rte_eth_dev *dev = &rte_eth_devices[hw->data->port_id];

	if (hw->adapter_state != HNS3_NIC_UNINITIALIZED)
		return;

	if (fw_compact_cmd_result != 0) {
		/*
		 * If fw_compact_cmd_result is not zero, it means firmware don't
		 * support link status change interrupt.
		 * Framework already set RTE_ETH_DEV_INTR_LSC bit because driver
		 * declared RTE_PCI_DRV_INTR_LSC in drv_flags. It need to clear
		 * the RTE_ETH_DEV_INTR_LSC capability when detect firmware
		 * don't support link status change interrupt.
		 */
		dev->data->dev_flags &= ~RTE_ETH_DEV_INTR_LSC;
	}
}

static void
hns3_set_fc_autoneg_cap(struct hns3_adapter *hns, int fw_compact_cmd_result)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_mac *mac = &hw->mac;

	if (mac->media_type == HNS3_MEDIA_TYPE_COPPER) {
		hns->pf.support_fc_autoneg = true;
		return;
	}

	/*
	 * Flow control auto-negotiation requires the cooperation of the driver
	 * and firmware.
	 */
	hns->pf.support_fc_autoneg = (hns3_dev_get_support(hw, FC_AUTO) &&
					fw_compact_cmd_result == 0) ?
					true : false;
}

static int
hns3_apply_fw_compat_cmd_result(struct hns3_hw *hw, int result)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);

	if (result != 0 && hns3_dev_get_support(hw, COPPER)) {
		hns3_err(hw, "firmware fails to initialize the PHY, ret = %d.",
			 result);
		return result;
	}

	hns3_update_dev_lsc_cap(hw, result);
	hns3_set_fc_autoneg_cap(hns, result);

	return 0;
}

static int
hns3_firmware_compat_config(struct hns3_hw *hw, bool is_init)
{
	struct hns3_firmware_compat_cmd *req;
	struct hns3_cmd_desc desc;
	uint32_t compat = 0;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_FIRMWARE_COMPAT_CFG, false);
	req = (struct hns3_firmware_compat_cmd *)desc.data;

	if (is_init) {
		hns3_set_bit(compat, HNS3_LINK_EVENT_REPORT_EN_B, 1);
		hns3_set_bit(compat, HNS3_NCSI_ERROR_REPORT_EN_B, 0);
		hns3_set_bit(compat, HNS3_LLRS_FEC_EN_B, 1);
		if (hns3_dev_get_support(hw, COPPER))
			hns3_set_bit(compat, HNS3_FIRMWARE_PHY_DRIVER_EN_B, 1);
		if (hns3_dev_get_support(hw, FC_AUTO))
			hns3_set_bit(compat, HNS3_MAC_FC_AUTONEG_EN_B, 1);
	}
	req->compat = rte_cpu_to_le_32(compat);

	return hns3_cmd_send(hw, &desc, 1);
}

int
hns3_cmd_init(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	uint32_t version;
	int ret;

	rte_spinlock_lock(&hw->cmq.csq.lock);
	rte_spinlock_lock(&hw->cmq.crq.lock);

	hw->cmq.csq.next_to_clean = 0;
	hw->cmq.csq.next_to_use = 0;
	hw->cmq.crq.next_to_clean = 0;
	hw->cmq.crq.next_to_use = 0;
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
	__atomic_store_n(&hw->reset.disable_cmd, 0, __ATOMIC_RELAXED);

	ret = hns3_cmd_query_firmware_version_and_capability(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "firmware version query failed %d", ret);
		goto err_cmd_init;
	}

	version = hw->fw_version;
	PMD_INIT_LOG(INFO, "The firmware version is %lu.%lu.%lu.%lu",
		     hns3_get_field(version, HNS3_FW_VERSION_BYTE3_M,
				    HNS3_FW_VERSION_BYTE3_S),
		     hns3_get_field(version, HNS3_FW_VERSION_BYTE2_M,
				    HNS3_FW_VERSION_BYTE2_S),
		     hns3_get_field(version, HNS3_FW_VERSION_BYTE1_M,
				    HNS3_FW_VERSION_BYTE1_S),
		     hns3_get_field(version, HNS3_FW_VERSION_BYTE0_M,
				    HNS3_FW_VERSION_BYTE0_S));

	if (hns->is_vf)
		return 0;

	/*
	 * Requiring firmware to enable some features, fiber port can still
	 * work without it, but copper port can't work because the firmware
	 * fails to take over the PHY.
	 */
	ret = hns3_firmware_compat_config(hw, true);
	if (ret)
		PMD_INIT_LOG(WARNING, "firmware compatible features not "
			     "supported, ret = %d.", ret);

	/*
	 * Perform some corresponding operations based on the firmware
	 * compatibility configuration result.
	 */
	ret = hns3_apply_fw_compat_cmd_result(hw, ret);
	if (ret)
		goto err_cmd_init;

	return 0;

err_cmd_init:
	__atomic_store_n(&hw->reset.disable_cmd, 1, __ATOMIC_RELAXED);
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
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);

	if (!hns->is_vf)
		(void)hns3_firmware_compat_config(hw, false);

	__atomic_store_n(&hw->reset.disable_cmd, 1, __ATOMIC_RELAXED);

	/*
	 * A delay is added to ensure that the register cleanup operations
	 * will not be performed concurrently with the firmware command and
	 * ensure that all the reserved commands are executed.
	 * Concurrency may occur in two scenarios: asynchronous command and
	 * timeout command. If the command fails to be executed due to busy
	 * scheduling, the command will be processed in the next scheduling
	 * of the firmware.
	 */
	rte_delay_ms(HNS3_CMDQ_CLEAR_WAIT_TIME);

	rte_spinlock_lock(&hw->cmq.csq.lock);
	rte_spinlock_lock(&hw->cmq.crq.lock);
	hns3_cmd_clear_regs(hw);
	rte_spinlock_unlock(&hw->cmq.crq.lock);
	rte_spinlock_unlock(&hw->cmq.csq.lock);
}
