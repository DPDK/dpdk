/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 HiSilicon Limited.
 */

#include <rte_alarm.h>
#include <bus_pci_driver.h>
#include <ethdev_pci.h>

#include "hns3_common.h"
#include "hns3_dump.h"
#include "hns3_logs.h"
#include "hns3_rxtx.h"
#include "hns3_intr.h"
#include "hns3_regs.h"
#include "hns3_dcb.h"
#include "hns3_mp.h"
#include "hns3_flow.h"
#include "hns3_ptp.h"
#include "hns3_ethdev.h"

#define HNS3_SERVICE_INTERVAL		1000000 /* us */
#define HNS3_SERVICE_QUICK_INTERVAL	10
#define HNS3_INVALID_PVID		0xFFFF

#define HNS3_FILTER_TYPE_VF		0
#define HNS3_FILTER_TYPE_PORT		1
#define HNS3_FILTER_FE_EGRESS_V1_B	BIT(0)
#define HNS3_FILTER_FE_NIC_INGRESS_B	BIT(0)
#define HNS3_FILTER_FE_NIC_EGRESS_B	BIT(1)
#define HNS3_FILTER_FE_ROCE_INGRESS_B	BIT(2)
#define HNS3_FILTER_FE_ROCE_EGRESS_B	BIT(3)
#define HNS3_FILTER_FE_EGRESS		(HNS3_FILTER_FE_NIC_EGRESS_B \
					| HNS3_FILTER_FE_ROCE_EGRESS_B)
#define HNS3_FILTER_FE_INGRESS		(HNS3_FILTER_FE_NIC_INGRESS_B \
					| HNS3_FILTER_FE_ROCE_INGRESS_B)

/* Reset related Registers */
#define HNS3_GLOBAL_RESET_BIT		0
#define HNS3_CORE_RESET_BIT		1
#define HNS3_IMP_RESET_BIT		2
#define HNS3_FUN_RST_ING_B		0

#define HNS3_VECTOR0_IMP_RESET_INT_B	1
#define HNS3_VECTOR0_IMP_CMDQ_ERR_B	4U
#define HNS3_VECTOR0_IMP_RD_POISON_B	5U
#define HNS3_VECTOR0_ALL_MSIX_ERR_B	6U
#define HNS3_VECTOR0_TRIGGER_IMP_RESET_B	7U

#define HNS3_RESET_WAIT_MS	100
#define HNS3_RESET_WAIT_CNT	200

enum hns3_evt_cause {
	HNS3_VECTOR0_EVENT_RST,
	HNS3_VECTOR0_EVENT_MBX,
	HNS3_VECTOR0_EVENT_ERR,
	HNS3_VECTOR0_EVENT_PTP,
	HNS3_VECTOR0_EVENT_OTHER,
};

#define HNS3_SPEEDS_SUPP_FEC (RTE_ETH_LINK_SPEED_10G | \
			      RTE_ETH_LINK_SPEED_25G | \
			      RTE_ETH_LINK_SPEED_40G | \
			      RTE_ETH_LINK_SPEED_50G | \
			      RTE_ETH_LINK_SPEED_100G | \
			      RTE_ETH_LINK_SPEED_200G)

static const struct rte_eth_fec_capa speed_fec_capa_tbl[] = {
	{ RTE_ETH_SPEED_NUM_10G, RTE_ETH_FEC_MODE_CAPA_MASK(NOFEC) |
			     RTE_ETH_FEC_MODE_CAPA_MASK(AUTO) |
			     RTE_ETH_FEC_MODE_CAPA_MASK(BASER) },

	{ RTE_ETH_SPEED_NUM_25G, RTE_ETH_FEC_MODE_CAPA_MASK(NOFEC) |
			     RTE_ETH_FEC_MODE_CAPA_MASK(AUTO) |
			     RTE_ETH_FEC_MODE_CAPA_MASK(BASER) |
			     RTE_ETH_FEC_MODE_CAPA_MASK(RS) },

	{ RTE_ETH_SPEED_NUM_40G, RTE_ETH_FEC_MODE_CAPA_MASK(NOFEC) |
			     RTE_ETH_FEC_MODE_CAPA_MASK(AUTO) |
			     RTE_ETH_FEC_MODE_CAPA_MASK(BASER) },

	{ RTE_ETH_SPEED_NUM_50G, RTE_ETH_FEC_MODE_CAPA_MASK(NOFEC) |
			     RTE_ETH_FEC_MODE_CAPA_MASK(AUTO) |
			     RTE_ETH_FEC_MODE_CAPA_MASK(BASER) |
			     RTE_ETH_FEC_MODE_CAPA_MASK(RS) },

	{ RTE_ETH_SPEED_NUM_100G, RTE_ETH_FEC_MODE_CAPA_MASK(NOFEC) |
			      RTE_ETH_FEC_MODE_CAPA_MASK(AUTO) |
			      RTE_ETH_FEC_MODE_CAPA_MASK(RS) },

	{ RTE_ETH_SPEED_NUM_200G, RTE_ETH_FEC_MODE_CAPA_MASK(AUTO) |
			      RTE_ETH_FEC_MODE_CAPA_MASK(RS) |
			      RTE_ETH_FEC_MODE_CAPA_MASK(LLRS) }
};

static enum hns3_reset_level hns3_get_reset_level(struct hns3_adapter *hns,
						 uint64_t *levels);
static int hns3_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu);
static int hns3_vlan_pvid_configure(struct hns3_adapter *hns, uint16_t pvid,
				    int on);
static int hns3_update_link_info(struct rte_eth_dev *eth_dev);
static bool hns3_update_link_status(struct hns3_hw *hw);

static int hns3_add_mc_mac_addr(struct hns3_hw *hw,
				struct rte_ether_addr *mac_addr);
static int hns3_remove_mc_mac_addr(struct hns3_hw *hw,
				   struct rte_ether_addr *mac_addr);
static int hns3_restore_fec(struct hns3_hw *hw);
static int hns3_query_dev_fec_info(struct hns3_hw *hw);
static int hns3_do_stop(struct hns3_adapter *hns);
static int hns3_check_port_speed(struct hns3_hw *hw, uint32_t link_speeds);
static int hns3_cfg_mac_mode(struct hns3_hw *hw, bool enable);


static void
hns3_pf_disable_irq0(struct hns3_hw *hw)
{
	hns3_write_dev(hw, HNS3_MISC_VECTOR_REG_BASE, 0);
}

static void
hns3_pf_enable_irq0(struct hns3_hw *hw)
{
	hns3_write_dev(hw, HNS3_MISC_VECTOR_REG_BASE, 1);
}

static enum hns3_evt_cause
hns3_proc_imp_reset_event(struct hns3_adapter *hns, bool is_delay,
			  uint32_t *vec_val)
{
	struct hns3_hw *hw = &hns->hw;

	__atomic_store_n(&hw->reset.disable_cmd, 1, __ATOMIC_RELAXED);
	hns3_atomic_set_bit(HNS3_IMP_RESET, &hw->reset.pending);
	*vec_val = BIT(HNS3_VECTOR0_IMPRESET_INT_B);
	if (!is_delay) {
		hw->reset.stats.imp_cnt++;
		hns3_warn(hw, "IMP reset detected, clear reset status");
	} else {
		hns3_schedule_delayed_reset(hns);
		hns3_warn(hw, "IMP reset detected, don't clear reset status");
	}

	return HNS3_VECTOR0_EVENT_RST;
}

static enum hns3_evt_cause
hns3_proc_global_reset_event(struct hns3_adapter *hns, bool is_delay,
			     uint32_t *vec_val)
{
	struct hns3_hw *hw = &hns->hw;

	__atomic_store_n(&hw->reset.disable_cmd, 1, __ATOMIC_RELAXED);
	hns3_atomic_set_bit(HNS3_GLOBAL_RESET, &hw->reset.pending);
	*vec_val = BIT(HNS3_VECTOR0_GLOBALRESET_INT_B);
	if (!is_delay) {
		hw->reset.stats.global_cnt++;
		hns3_warn(hw, "Global reset detected, clear reset status");
	} else {
		hns3_schedule_delayed_reset(hns);
		hns3_warn(hw,
			  "Global reset detected, don't clear reset status");
	}

	return HNS3_VECTOR0_EVENT_RST;
}

static enum hns3_evt_cause
hns3_check_event_cause(struct hns3_adapter *hns, uint32_t *clearval)
{
	struct hns3_hw *hw = &hns->hw;
	uint32_t vector0_int_stats;
	uint32_t cmdq_src_val;
	uint32_t hw_err_src_reg;
	uint32_t val;
	enum hns3_evt_cause ret;
	bool is_delay;

	/* fetch the events from their corresponding regs */
	vector0_int_stats = hns3_read_dev(hw, HNS3_VECTOR0_OTHER_INT_STS_REG);
	cmdq_src_val = hns3_read_dev(hw, HNS3_VECTOR0_CMDQ_SRC_REG);
	hw_err_src_reg = hns3_read_dev(hw, HNS3_RAS_PF_OTHER_INT_STS_REG);

	is_delay = clearval == NULL ? true : false;
	/*
	 * Assumption: If by any chance reset and mailbox events are reported
	 * together then we will only process reset event and defer the
	 * processing of the mailbox events. Since, we would have not cleared
	 * RX CMDQ event this time we would receive again another interrupt
	 * from H/W just for the mailbox.
	 */
	if (BIT(HNS3_VECTOR0_IMPRESET_INT_B) & vector0_int_stats) { /* IMP */
		ret = hns3_proc_imp_reset_event(hns, is_delay, &val);
		goto out;
	}

	/* Global reset */
	if (BIT(HNS3_VECTOR0_GLOBALRESET_INT_B) & vector0_int_stats) {
		ret = hns3_proc_global_reset_event(hns, is_delay, &val);
		goto out;
	}

	/* Check for vector0 1588 event source */
	if (BIT(HNS3_VECTOR0_1588_INT_B) & vector0_int_stats) {
		val = BIT(HNS3_VECTOR0_1588_INT_B);
		ret = HNS3_VECTOR0_EVENT_PTP;
		goto out;
	}

	/* check for vector0 msix event source */
	if (vector0_int_stats & HNS3_VECTOR0_REG_MSIX_MASK ||
	    hw_err_src_reg & HNS3_RAS_REG_NFE_MASK) {
		val = vector0_int_stats | hw_err_src_reg;
		ret = HNS3_VECTOR0_EVENT_ERR;
		goto out;
	}

	/* check for vector0 mailbox(=CMDQ RX) event source */
	if (BIT(HNS3_VECTOR0_RX_CMDQ_INT_B) & cmdq_src_val) {
		cmdq_src_val &= ~BIT(HNS3_VECTOR0_RX_CMDQ_INT_B);
		val = cmdq_src_val;
		ret = HNS3_VECTOR0_EVENT_MBX;
		goto out;
	}

	val = vector0_int_stats;
	ret = HNS3_VECTOR0_EVENT_OTHER;
out:

	if (clearval)
		*clearval = val;
	return ret;
}

static void
hns3_clear_event_cause(struct hns3_hw *hw, uint32_t event_type, uint32_t regclr)
{
	if (event_type == HNS3_VECTOR0_EVENT_RST ||
	    event_type == HNS3_VECTOR0_EVENT_PTP)
		hns3_write_dev(hw, HNS3_MISC_RESET_STS_REG, regclr);
	else if (event_type == HNS3_VECTOR0_EVENT_MBX)
		hns3_write_dev(hw, HNS3_VECTOR0_CMDQ_SRC_REG, regclr);
}

static void
hns3_clear_all_event_cause(struct hns3_hw *hw)
{
	uint32_t vector0_int_stats;

	vector0_int_stats = hns3_read_dev(hw, HNS3_VECTOR0_OTHER_INT_STS_REG);
	if (BIT(HNS3_VECTOR0_IMPRESET_INT_B) & vector0_int_stats)
		hns3_warn(hw, "Probe during IMP reset interrupt");

	if (BIT(HNS3_VECTOR0_GLOBALRESET_INT_B) & vector0_int_stats)
		hns3_warn(hw, "Probe during Global reset interrupt");

	hns3_clear_event_cause(hw, HNS3_VECTOR0_EVENT_RST,
			       BIT(HNS3_VECTOR0_IMPRESET_INT_B) |
			       BIT(HNS3_VECTOR0_GLOBALRESET_INT_B) |
			       BIT(HNS3_VECTOR0_CORERESET_INT_B));
	hns3_clear_event_cause(hw, HNS3_VECTOR0_EVENT_MBX, 0);
	hns3_clear_event_cause(hw, HNS3_VECTOR0_EVENT_PTP,
				BIT(HNS3_VECTOR0_1588_INT_B));
}

static void
hns3_handle_mac_tnl(struct hns3_hw *hw)
{
	struct hns3_cmd_desc desc;
	uint32_t status;
	int ret;

	/* query and clear mac tnl interrupt */
	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_QUERY_MAC_TNL_INT, true);
	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		hns3_err(hw, "failed to query mac tnl int, ret = %d.", ret);
		return;
	}

	status = rte_le_to_cpu_32(desc.data[0]);
	if (status) {
		hns3_warn(hw, "mac tnl int occurs, status = 0x%x.", status);
		hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_CLEAR_MAC_TNL_INT,
					  false);
		desc.data[0] = rte_cpu_to_le_32(HNS3_MAC_TNL_INT_CLR);
		ret = hns3_cmd_send(hw, &desc, 1);
		if (ret)
			hns3_err(hw, "failed to clear mac tnl int, ret = %d.",
				 ret);
	}
}

static void
hns3_delay_before_clear_event_cause(struct hns3_hw *hw, uint32_t event_type, uint32_t regclr)
{
#define IMPRESET_WAIT_MS_TIME	5

	if (event_type == HNS3_VECTOR0_EVENT_RST &&
	    regclr & BIT(HNS3_VECTOR0_IMPRESET_INT_B) &&
	    hw->revision >= PCI_REVISION_ID_HIP09_A) {
		rte_delay_ms(IMPRESET_WAIT_MS_TIME);
		hns3_dbg(hw, "wait firmware watchdog initialization completed.");
	}
}

static void
hns3_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	enum hns3_evt_cause event_cause;
	uint32_t clearval = 0;
	uint32_t vector0_int;
	uint32_t ras_int;
	uint32_t cmdq_int;

	/* Disable interrupt */
	hns3_pf_disable_irq0(hw);

	event_cause = hns3_check_event_cause(hns, &clearval);
	vector0_int = hns3_read_dev(hw, HNS3_VECTOR0_OTHER_INT_STS_REG);
	ras_int = hns3_read_dev(hw, HNS3_RAS_PF_OTHER_INT_STS_REG);
	cmdq_int = hns3_read_dev(hw, HNS3_VECTOR0_CMDQ_SRC_REG);
	hns3_delay_before_clear_event_cause(hw, event_cause, clearval);
	hns3_clear_event_cause(hw, event_cause, clearval);
	/* vector 0 interrupt is shared with reset and mailbox source events. */
	if (event_cause == HNS3_VECTOR0_EVENT_ERR) {
		hns3_warn(hw, "received interrupt: vector0_int_stat:0x%x "
			  "ras_int_stat:0x%x cmdq_int_stat:0x%x",
			  vector0_int, ras_int, cmdq_int);
		hns3_handle_mac_tnl(hw);
		hns3_handle_error(hns);
	} else if (event_cause == HNS3_VECTOR0_EVENT_RST) {
		hns3_warn(hw, "received reset interrupt");
		hns3_schedule_reset(hns);
	} else if (event_cause == HNS3_VECTOR0_EVENT_MBX) {
		hns3_dev_handle_mbx_msg(hw);
	} else if (event_cause != HNS3_VECTOR0_EVENT_PTP) {
		hns3_warn(hw, "received unknown event: vector0_int_stat:0x%x "
			  "ras_int_stat:0x%x cmdq_int_stat:0x%x",
			  vector0_int, ras_int, cmdq_int);
	}

	/* Enable interrupt if it is not cause by reset */
	hns3_pf_enable_irq0(hw);
}

static int
hns3_set_port_vlan_filter(struct hns3_adapter *hns, uint16_t vlan_id, int on)
{
#define HNS3_VLAN_ID_OFFSET_STEP	160
#define HNS3_VLAN_BYTE_SIZE		8
	struct hns3_vlan_filter_pf_cfg_cmd *req;
	struct hns3_hw *hw = &hns->hw;
	uint8_t vlan_offset_byte_val;
	struct hns3_cmd_desc desc;
	uint8_t vlan_offset_byte;
	uint8_t vlan_offset_base;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_VLAN_FILTER_PF_CFG, false);

	vlan_offset_base = vlan_id / HNS3_VLAN_ID_OFFSET_STEP;
	vlan_offset_byte = (vlan_id % HNS3_VLAN_ID_OFFSET_STEP) /
			   HNS3_VLAN_BYTE_SIZE;
	vlan_offset_byte_val = 1 << (vlan_id % HNS3_VLAN_BYTE_SIZE);

	req = (struct hns3_vlan_filter_pf_cfg_cmd *)desc.data;
	req->vlan_offset = vlan_offset_base;
	req->vlan_cfg = on ? 0 : 1;
	req->vlan_offset_bitmap[vlan_offset_byte] = vlan_offset_byte_val;

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		hns3_err(hw, "set port vlan id failed, vlan_id =%u, ret =%d",
			 vlan_id, ret);

	return ret;
}

static void
hns3_rm_dev_vlan_table(struct hns3_adapter *hns, uint16_t vlan_id)
{
	struct hns3_user_vlan_table *vlan_entry;
	struct hns3_pf *pf = &hns->pf;

	LIST_FOREACH(vlan_entry, &pf->vlan_list, next) {
		if (vlan_entry->vlan_id == vlan_id) {
			if (vlan_entry->hd_tbl_status)
				hns3_set_port_vlan_filter(hns, vlan_id, 0);
			LIST_REMOVE(vlan_entry, next);
			rte_free(vlan_entry);
			break;
		}
	}
}

static void
hns3_add_dev_vlan_table(struct hns3_adapter *hns, uint16_t vlan_id,
			bool writen_to_tbl)
{
	struct hns3_user_vlan_table *vlan_entry;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_pf *pf = &hns->pf;

	LIST_FOREACH(vlan_entry, &pf->vlan_list, next) {
		if (vlan_entry->vlan_id == vlan_id)
			return;
	}

	vlan_entry = rte_zmalloc("hns3_vlan_tbl", sizeof(*vlan_entry), 0);
	if (vlan_entry == NULL) {
		hns3_err(hw, "Failed to malloc hns3 vlan table");
		return;
	}

	vlan_entry->hd_tbl_status = writen_to_tbl;
	vlan_entry->vlan_id = vlan_id;

	LIST_INSERT_HEAD(&pf->vlan_list, vlan_entry, next);
}

static int
hns3_restore_vlan_table(struct hns3_adapter *hns)
{
	struct hns3_user_vlan_table *vlan_entry;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_pf *pf = &hns->pf;
	uint16_t vlan_id;
	int ret = 0;

	if (hw->port_base_vlan_cfg.state == HNS3_PORT_BASE_VLAN_ENABLE)
		return hns3_vlan_pvid_configure(hns,
						hw->port_base_vlan_cfg.pvid, 1);

	LIST_FOREACH(vlan_entry, &pf->vlan_list, next) {
		if (vlan_entry->hd_tbl_status) {
			vlan_id = vlan_entry->vlan_id;
			ret = hns3_set_port_vlan_filter(hns, vlan_id, 1);
			if (ret)
				break;
		}
	}

	return ret;
}

static int
hns3_vlan_filter_configure(struct hns3_adapter *hns, uint16_t vlan_id, int on)
{
	struct hns3_hw *hw = &hns->hw;
	bool writen_to_tbl = false;
	int ret = 0;

	/*
	 * When vlan filter is enabled, hardware regards packets without vlan
	 * as packets with vlan 0. So, to receive packets without vlan, vlan id
	 * 0 is not allowed to be removed by rte_eth_dev_vlan_filter.
	 */
	if (on == 0 && vlan_id == 0)
		return 0;

	/*
	 * When port base vlan enabled, we use port base vlan as the vlan
	 * filter condition. In this case, we don't update vlan filter table
	 * when user add new vlan or remove exist vlan, just update the
	 * vlan list. The vlan id in vlan list will be written in vlan filter
	 * table until port base vlan disabled
	 */
	if (hw->port_base_vlan_cfg.state == HNS3_PORT_BASE_VLAN_DISABLE) {
		ret = hns3_set_port_vlan_filter(hns, vlan_id, on);
		writen_to_tbl = true;
	}

	if (ret == 0) {
		if (on)
			hns3_add_dev_vlan_table(hns, vlan_id, writen_to_tbl);
		else
			hns3_rm_dev_vlan_table(hns, vlan_id);
	}
	return ret;
}

static int
hns3_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	rte_spinlock_lock(&hw->lock);
	ret = hns3_vlan_filter_configure(hns, vlan_id, on);
	rte_spinlock_unlock(&hw->lock);
	return ret;
}

static int
hns3_vlan_tpid_configure(struct hns3_adapter *hns, enum rte_vlan_type vlan_type,
			 uint16_t tpid)
{
	struct hns3_rx_vlan_type_cfg_cmd *rx_req;
	struct hns3_tx_vlan_type_cfg_cmd *tx_req;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_cmd_desc desc;
	int ret;

	if ((vlan_type != RTE_ETH_VLAN_TYPE_INNER &&
	     vlan_type != RTE_ETH_VLAN_TYPE_OUTER)) {
		hns3_err(hw, "Unsupported vlan type, vlan_type =%d", vlan_type);
		return -EINVAL;
	}

	if (tpid != RTE_ETHER_TYPE_VLAN) {
		hns3_err(hw, "Unsupported vlan tpid, vlan_type =%d", vlan_type);
		return -EINVAL;
	}

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_MAC_VLAN_TYPE_ID, false);
	rx_req = (struct hns3_rx_vlan_type_cfg_cmd *)desc.data;

	if (vlan_type == RTE_ETH_VLAN_TYPE_OUTER) {
		rx_req->ot_fst_vlan_type = rte_cpu_to_le_16(tpid);
		rx_req->ot_sec_vlan_type = rte_cpu_to_le_16(tpid);
	} else if (vlan_type == RTE_ETH_VLAN_TYPE_INNER) {
		rx_req->ot_fst_vlan_type = rte_cpu_to_le_16(tpid);
		rx_req->ot_sec_vlan_type = rte_cpu_to_le_16(tpid);
		rx_req->in_fst_vlan_type = rte_cpu_to_le_16(tpid);
		rx_req->in_sec_vlan_type = rte_cpu_to_le_16(tpid);
	}

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		hns3_err(hw, "Send rxvlan protocol type command fail, ret =%d",
			 ret);
		return ret;
	}

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_MAC_VLAN_INSERT, false);

	tx_req = (struct hns3_tx_vlan_type_cfg_cmd *)desc.data;
	tx_req->ot_vlan_type = rte_cpu_to_le_16(tpid);
	tx_req->in_vlan_type = rte_cpu_to_le_16(tpid);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		hns3_err(hw, "Send txvlan protocol type command fail, ret =%d",
			 ret);
	return ret;
}

static int
hns3_vlan_tpid_set(struct rte_eth_dev *dev, enum rte_vlan_type vlan_type,
		   uint16_t tpid)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	rte_spinlock_lock(&hw->lock);
	ret = hns3_vlan_tpid_configure(hns, vlan_type, tpid);
	rte_spinlock_unlock(&hw->lock);
	return ret;
}

static int
hns3_set_vlan_rx_offload_cfg(struct hns3_adapter *hns,
			     struct hns3_rx_vtag_cfg *vcfg)
{
	struct hns3_vport_vtag_rx_cfg_cmd *req;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_cmd_desc desc;
	uint16_t vport_id;
	uint8_t bitmap;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_VLAN_PORT_RX_CFG, false);

	req = (struct hns3_vport_vtag_rx_cfg_cmd *)desc.data;
	hns3_set_bit(req->vport_vlan_cfg, HNS3_REM_TAG1_EN_B,
		     vcfg->strip_tag1_en ? 1 : 0);
	hns3_set_bit(req->vport_vlan_cfg, HNS3_REM_TAG2_EN_B,
		     vcfg->strip_tag2_en ? 1 : 0);
	hns3_set_bit(req->vport_vlan_cfg, HNS3_SHOW_TAG1_EN_B,
		     vcfg->vlan1_vlan_prionly ? 1 : 0);
	hns3_set_bit(req->vport_vlan_cfg, HNS3_SHOW_TAG2_EN_B,
		     vcfg->vlan2_vlan_prionly ? 1 : 0);

	/* firmware will ignore this configuration for PCI_REVISION_ID_HIP08 */
	hns3_set_bit(req->vport_vlan_cfg, HNS3_DISCARD_TAG1_EN_B,
		     vcfg->strip_tag1_discard_en ? 1 : 0);
	hns3_set_bit(req->vport_vlan_cfg, HNS3_DISCARD_TAG2_EN_B,
		     vcfg->strip_tag2_discard_en ? 1 : 0);
	/*
	 * In current version VF is not supported when PF is driven by DPDK
	 * driver, just need to configure parameters for PF vport.
	 */
	vport_id = HNS3_PF_FUNC_ID;
	req->vf_offset = vport_id / HNS3_VF_NUM_PER_CMD;
	bitmap = 1 << (vport_id % HNS3_VF_NUM_PER_BYTE);
	req->vf_bitmap[req->vf_offset] = bitmap;

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		hns3_err(hw, "Send port rxvlan cfg command fail, ret =%d", ret);
	return ret;
}

static int
hns3_en_hw_strip_rxvtag(struct hns3_adapter *hns, bool enable)
{
	struct hns3_rx_vtag_cfg rxvlan_cfg;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	if (hw->port_base_vlan_cfg.state == HNS3_PORT_BASE_VLAN_DISABLE) {
		rxvlan_cfg.strip_tag1_en = false;
		rxvlan_cfg.strip_tag2_en = enable;
		rxvlan_cfg.strip_tag2_discard_en = false;
	} else {
		rxvlan_cfg.strip_tag1_en = enable;
		rxvlan_cfg.strip_tag2_en = true;
		rxvlan_cfg.strip_tag2_discard_en = true;
	}

	rxvlan_cfg.strip_tag1_discard_en = false;
	rxvlan_cfg.vlan1_vlan_prionly = false;
	rxvlan_cfg.vlan2_vlan_prionly = false;
	rxvlan_cfg.rx_vlan_offload_en = enable;

	ret = hns3_set_vlan_rx_offload_cfg(hns, &rxvlan_cfg);
	if (ret) {
		hns3_err(hw, "%s strip rx vtag failed, ret = %d.",
				enable ? "enable" : "disable", ret);
		return ret;
	}

	memcpy(&hns->pf.vtag_config.rx_vcfg, &rxvlan_cfg,
	       sizeof(struct hns3_rx_vtag_cfg));

	return ret;
}

static int
hns3_set_vlan_filter_ctrl(struct hns3_hw *hw, uint8_t vlan_type,
			  uint8_t fe_type, bool filter_en, uint8_t vf_id)
{
	struct hns3_vlan_filter_ctrl_cmd *req;
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_VLAN_FILTER_CTRL, false);

	req = (struct hns3_vlan_filter_ctrl_cmd *)desc.data;
	req->vlan_type = vlan_type;
	req->vlan_fe = filter_en ? fe_type : 0;
	req->vf_id = vf_id;

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		hns3_err(hw, "set vlan filter fail, ret =%d", ret);

	return ret;
}

static int
hns3_vlan_filter_init(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	int ret;

	ret = hns3_set_vlan_filter_ctrl(hw, HNS3_FILTER_TYPE_VF,
					HNS3_FILTER_FE_EGRESS, false,
					HNS3_PF_FUNC_ID);
	if (ret) {
		hns3_err(hw, "failed to init vf vlan filter, ret = %d", ret);
		return ret;
	}

	ret = hns3_set_vlan_filter_ctrl(hw, HNS3_FILTER_TYPE_PORT,
					HNS3_FILTER_FE_INGRESS, false,
					HNS3_PF_FUNC_ID);
	if (ret)
		hns3_err(hw, "failed to init port vlan filter, ret = %d", ret);

	return ret;
}

static int
hns3_enable_vlan_filter(struct hns3_adapter *hns, bool enable)
{
	struct hns3_hw *hw = &hns->hw;
	int ret;

	ret = hns3_set_vlan_filter_ctrl(hw, HNS3_FILTER_TYPE_PORT,
					HNS3_FILTER_FE_INGRESS, enable,
					HNS3_PF_FUNC_ID);
	if (ret)
		hns3_err(hw, "failed to %s port vlan filter, ret = %d",
			 enable ? "enable" : "disable", ret);

	return ret;
}

static int
hns3_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	struct rte_eth_rxmode *rxmode;
	unsigned int tmp_mask;
	bool enable;
	int ret = 0;

	rte_spinlock_lock(&hw->lock);
	rxmode = &dev->data->dev_conf.rxmode;
	tmp_mask = (unsigned int)mask;
	if (tmp_mask & RTE_ETH_VLAN_FILTER_MASK) {
		/* ignore vlan filter configuration during promiscuous mode */
		if (!dev->data->promiscuous) {
			/* Enable or disable VLAN filter */
			enable = rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_FILTER ?
				 true : false;

			ret = hns3_enable_vlan_filter(hns, enable);
			if (ret) {
				rte_spinlock_unlock(&hw->lock);
				hns3_err(hw, "failed to %s rx filter, ret = %d",
					 enable ? "enable" : "disable", ret);
				return ret;
			}
		}
	}

	if (tmp_mask & RTE_ETH_VLAN_STRIP_MASK) {
		/* Enable or disable VLAN stripping */
		enable = rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP ?
		    true : false;

		ret = hns3_en_hw_strip_rxvtag(hns, enable);
		if (ret) {
			rte_spinlock_unlock(&hw->lock);
			hns3_err(hw, "failed to %s rx strip, ret = %d",
				 enable ? "enable" : "disable", ret);
			return ret;
		}
	}

	rte_spinlock_unlock(&hw->lock);

	return ret;
}

static int
hns3_set_vlan_tx_offload_cfg(struct hns3_adapter *hns,
			     struct hns3_tx_vtag_cfg *vcfg)
{
	struct hns3_vport_vtag_tx_cfg_cmd *req;
	struct hns3_cmd_desc desc;
	struct hns3_hw *hw = &hns->hw;
	uint16_t vport_id;
	uint8_t bitmap;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_VLAN_PORT_TX_CFG, false);

	req = (struct hns3_vport_vtag_tx_cfg_cmd *)desc.data;
	req->def_vlan_tag1 = vcfg->default_tag1;
	req->def_vlan_tag2 = vcfg->default_tag2;
	hns3_set_bit(req->vport_vlan_cfg, HNS3_ACCEPT_TAG1_B,
		     vcfg->accept_tag1 ? 1 : 0);
	hns3_set_bit(req->vport_vlan_cfg, HNS3_ACCEPT_UNTAG1_B,
		     vcfg->accept_untag1 ? 1 : 0);
	hns3_set_bit(req->vport_vlan_cfg, HNS3_ACCEPT_TAG2_B,
		     vcfg->accept_tag2 ? 1 : 0);
	hns3_set_bit(req->vport_vlan_cfg, HNS3_ACCEPT_UNTAG2_B,
		     vcfg->accept_untag2 ? 1 : 0);
	hns3_set_bit(req->vport_vlan_cfg, HNS3_PORT_INS_TAG1_EN_B,
		     vcfg->insert_tag1_en ? 1 : 0);
	hns3_set_bit(req->vport_vlan_cfg, HNS3_PORT_INS_TAG2_EN_B,
		     vcfg->insert_tag2_en ? 1 : 0);
	hns3_set_bit(req->vport_vlan_cfg, HNS3_CFG_NIC_ROCE_SEL_B, 0);

	/* firmware will ignore this configuration for PCI_REVISION_ID_HIP08 */
	hns3_set_bit(req->vport_vlan_cfg, HNS3_TAG_SHIFT_MODE_EN_B,
		     vcfg->tag_shift_mode_en ? 1 : 0);

	/*
	 * In current version VF is not supported when PF is driven by DPDK
	 * driver, just need to configure parameters for PF vport.
	 */
	vport_id = HNS3_PF_FUNC_ID;
	req->vf_offset = vport_id / HNS3_VF_NUM_PER_CMD;
	bitmap = 1 << (vport_id % HNS3_VF_NUM_PER_BYTE);
	req->vf_bitmap[req->vf_offset] = bitmap;

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		hns3_err(hw, "Send port txvlan cfg command fail, ret =%d", ret);

	return ret;
}

static int
hns3_vlan_txvlan_cfg(struct hns3_adapter *hns, uint16_t port_base_vlan_state,
		     uint16_t pvid)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_tx_vtag_cfg txvlan_cfg;
	int ret;

	if (port_base_vlan_state == HNS3_PORT_BASE_VLAN_DISABLE) {
		txvlan_cfg.accept_tag1 = true;
		txvlan_cfg.insert_tag1_en = false;
		txvlan_cfg.default_tag1 = 0;
	} else {
		txvlan_cfg.accept_tag1 =
			hw->vlan_mode == HNS3_HW_SHIFT_AND_DISCARD_MODE;
		txvlan_cfg.insert_tag1_en = true;
		txvlan_cfg.default_tag1 = pvid;
	}

	txvlan_cfg.accept_untag1 = true;
	txvlan_cfg.accept_tag2 = true;
	txvlan_cfg.accept_untag2 = true;
	txvlan_cfg.insert_tag2_en = false;
	txvlan_cfg.default_tag2 = 0;
	txvlan_cfg.tag_shift_mode_en = true;

	ret = hns3_set_vlan_tx_offload_cfg(hns, &txvlan_cfg);
	if (ret) {
		hns3_err(hw, "pf vlan set pvid failed, pvid =%u ,ret =%d", pvid,
			 ret);
		return ret;
	}

	memcpy(&hns->pf.vtag_config.tx_vcfg, &txvlan_cfg,
	       sizeof(struct hns3_tx_vtag_cfg));

	return ret;
}


static void
hns3_rm_all_vlan_table(struct hns3_adapter *hns, bool is_del_list)
{
	struct hns3_user_vlan_table *vlan_entry;
	struct hns3_pf *pf = &hns->pf;

	LIST_FOREACH(vlan_entry, &pf->vlan_list, next) {
		if (vlan_entry->hd_tbl_status) {
			hns3_set_port_vlan_filter(hns, vlan_entry->vlan_id, 0);
			vlan_entry->hd_tbl_status = false;
		}
	}

	if (is_del_list) {
		vlan_entry = LIST_FIRST(&pf->vlan_list);
		while (vlan_entry) {
			LIST_REMOVE(vlan_entry, next);
			rte_free(vlan_entry);
			vlan_entry = LIST_FIRST(&pf->vlan_list);
		}
	}
}

static void
hns3_add_all_vlan_table(struct hns3_adapter *hns)
{
	struct hns3_user_vlan_table *vlan_entry;
	struct hns3_pf *pf = &hns->pf;

	LIST_FOREACH(vlan_entry, &pf->vlan_list, next) {
		if (!vlan_entry->hd_tbl_status) {
			hns3_set_port_vlan_filter(hns, vlan_entry->vlan_id, 1);
			vlan_entry->hd_tbl_status = true;
		}
	}
}

static void
hns3_remove_all_vlan_table(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	int ret;

	hns3_rm_all_vlan_table(hns, true);
	if (hw->port_base_vlan_cfg.pvid != HNS3_INVALID_PVID) {
		ret = hns3_set_port_vlan_filter(hns,
						hw->port_base_vlan_cfg.pvid, 0);
		if (ret) {
			hns3_err(hw, "Failed to remove all vlan table, ret =%d",
				 ret);
			return;
		}
	}
}

static int
hns3_update_vlan_filter_entries(struct hns3_adapter *hns,
			uint16_t port_base_vlan_state, uint16_t new_pvid)
{
	struct hns3_hw *hw = &hns->hw;
	uint16_t old_pvid;
	int ret;

	if (port_base_vlan_state == HNS3_PORT_BASE_VLAN_ENABLE) {
		old_pvid = hw->port_base_vlan_cfg.pvid;
		if (old_pvid != HNS3_INVALID_PVID) {
			ret = hns3_set_port_vlan_filter(hns, old_pvid, 0);
			if (ret) {
				hns3_err(hw, "failed to remove old pvid %u, "
						"ret = %d", old_pvid, ret);
				return ret;
			}
		}

		hns3_rm_all_vlan_table(hns, false);
		ret = hns3_set_port_vlan_filter(hns, new_pvid, 1);
		if (ret) {
			hns3_err(hw, "failed to add new pvid %u, ret = %d",
					new_pvid, ret);
			return ret;
		}
	} else {
		ret = hns3_set_port_vlan_filter(hns, new_pvid, 0);
		if (ret) {
			hns3_err(hw, "failed to remove pvid %u, ret = %d",
					new_pvid, ret);
			return ret;
		}

		hns3_add_all_vlan_table(hns);
	}
	return 0;
}

static int
hns3_en_pvid_strip(struct hns3_adapter *hns, int on)
{
	struct hns3_rx_vtag_cfg *old_cfg = &hns->pf.vtag_config.rx_vcfg;
	struct hns3_rx_vtag_cfg rx_vlan_cfg;
	bool rx_strip_en;
	int ret;

	rx_strip_en = old_cfg->rx_vlan_offload_en;
	if (on) {
		rx_vlan_cfg.strip_tag1_en = rx_strip_en;
		rx_vlan_cfg.strip_tag2_en = true;
		rx_vlan_cfg.strip_tag2_discard_en = true;
	} else {
		rx_vlan_cfg.strip_tag1_en = false;
		rx_vlan_cfg.strip_tag2_en = rx_strip_en;
		rx_vlan_cfg.strip_tag2_discard_en = false;
	}
	rx_vlan_cfg.strip_tag1_discard_en = false;
	rx_vlan_cfg.vlan1_vlan_prionly = false;
	rx_vlan_cfg.vlan2_vlan_prionly = false;
	rx_vlan_cfg.rx_vlan_offload_en = old_cfg->rx_vlan_offload_en;

	ret = hns3_set_vlan_rx_offload_cfg(hns, &rx_vlan_cfg);
	if (ret)
		return ret;

	memcpy(&hns->pf.vtag_config.rx_vcfg, &rx_vlan_cfg,
	       sizeof(struct hns3_rx_vtag_cfg));

	return ret;
}

static int
hns3_vlan_pvid_configure(struct hns3_adapter *hns, uint16_t pvid, int on)
{
	struct hns3_hw *hw = &hns->hw;
	uint16_t port_base_vlan_state;
	int ret, err;

	if (on == 0 && pvid != hw->port_base_vlan_cfg.pvid) {
		if (hw->port_base_vlan_cfg.pvid != HNS3_INVALID_PVID)
			hns3_warn(hw, "Invalid operation! As current pvid set "
				  "is %u, disable pvid %u is invalid",
				  hw->port_base_vlan_cfg.pvid, pvid);
		return 0;
	}

	port_base_vlan_state = on ? HNS3_PORT_BASE_VLAN_ENABLE :
				    HNS3_PORT_BASE_VLAN_DISABLE;
	ret = hns3_vlan_txvlan_cfg(hns, port_base_vlan_state, pvid);
	if (ret) {
		hns3_err(hw, "failed to config tx vlan for pvid, ret = %d",
			 ret);
		return ret;
	}

	ret = hns3_en_pvid_strip(hns, on);
	if (ret) {
		hns3_err(hw, "failed to config rx vlan strip for pvid, "
			 "ret = %d", ret);
		goto pvid_vlan_strip_fail;
	}

	if (pvid == HNS3_INVALID_PVID)
		goto out;
	ret = hns3_update_vlan_filter_entries(hns, port_base_vlan_state, pvid);
	if (ret) {
		hns3_err(hw, "failed to update vlan filter entries, ret = %d",
			 ret);
		goto vlan_filter_set_fail;
	}

out:
	hw->port_base_vlan_cfg.state = port_base_vlan_state;
	hw->port_base_vlan_cfg.pvid = on ? pvid : HNS3_INVALID_PVID;
	return ret;

vlan_filter_set_fail:
	err = hns3_en_pvid_strip(hns, hw->port_base_vlan_cfg.state ==
					HNS3_PORT_BASE_VLAN_ENABLE);
	if (err)
		hns3_err(hw, "fail to rollback pvid strip, ret = %d", err);

pvid_vlan_strip_fail:
	err = hns3_vlan_txvlan_cfg(hns, hw->port_base_vlan_cfg.state,
					hw->port_base_vlan_cfg.pvid);
	if (err)
		hns3_err(hw, "fail to rollback txvlan status, ret = %d", err);

	return ret;
}

static int
hns3_vlan_pvid_set(struct rte_eth_dev *dev, uint16_t pvid, int on)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	bool pvid_en_state_change;
	uint16_t pvid_state;
	int ret;

	if (pvid > RTE_ETHER_MAX_VLAN_ID) {
		hns3_err(hw, "Invalid vlan_id = %u > %d", pvid,
			 RTE_ETHER_MAX_VLAN_ID);
		return -EINVAL;
	}

	/*
	 * If PVID configuration state change, should refresh the PVID
	 * configuration state in struct hns3_tx_queue/hns3_rx_queue.
	 */
	pvid_state = hw->port_base_vlan_cfg.state;
	if ((on && pvid_state == HNS3_PORT_BASE_VLAN_ENABLE) ||
	    (!on && pvid_state == HNS3_PORT_BASE_VLAN_DISABLE))
		pvid_en_state_change = false;
	else
		pvid_en_state_change = true;

	rte_spinlock_lock(&hw->lock);
	ret = hns3_vlan_pvid_configure(hns, pvid, on);
	rte_spinlock_unlock(&hw->lock);
	if (ret)
		return ret;
	/*
	 * Only in HNS3_SW_SHIFT_AND_MODE the PVID related operation in Tx/Rx
	 * need be processed by PMD.
	 */
	if (pvid_en_state_change &&
	    hw->vlan_mode == HNS3_SW_SHIFT_AND_DISCARD_MODE)
		hns3_update_all_queues_pvid_proc_en(hw);

	return 0;
}

static int
hns3_default_vlan_config(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	int ret;

	/*
	 * When vlan filter is enabled, hardware regards packets without vlan
	 * as packets with vlan 0. Therefore, if vlan 0 is not in the vlan
	 * table, packets without vlan won't be received. So, add vlan 0 as
	 * the default vlan.
	 */
	ret = hns3_vlan_filter_configure(hns, 0, 1);
	if (ret)
		hns3_err(hw, "default vlan 0 config failed, ret =%d", ret);
	return ret;
}

static int
hns3_init_vlan_config(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	int ret;

	/*
	 * This function can be called in the initialization and reset process,
	 * when in reset process, it means that hardware had been reseted
	 * successfully and we need to restore the hardware configuration to
	 * ensure that the hardware configuration remains unchanged before and
	 * after reset.
	 */
	if (__atomic_load_n(&hw->reset.resetting, __ATOMIC_RELAXED) == 0) {
		hw->port_base_vlan_cfg.state = HNS3_PORT_BASE_VLAN_DISABLE;
		hw->port_base_vlan_cfg.pvid = HNS3_INVALID_PVID;
	}

	ret = hns3_vlan_filter_init(hns);
	if (ret) {
		hns3_err(hw, "vlan init fail in pf, ret =%d", ret);
		return ret;
	}

	ret = hns3_vlan_tpid_configure(hns, RTE_ETH_VLAN_TYPE_INNER,
				       RTE_ETHER_TYPE_VLAN);
	if (ret) {
		hns3_err(hw, "tpid set fail in pf, ret =%d", ret);
		return ret;
	}

	/*
	 * When in the reinit dev stage of the reset process, the following
	 * vlan-related configurations may differ from those at initialization,
	 * we will restore configurations to hardware in hns3_restore_vlan_table
	 * and hns3_restore_vlan_conf later.
	 */
	if (__atomic_load_n(&hw->reset.resetting, __ATOMIC_RELAXED) == 0) {
		ret = hns3_vlan_pvid_configure(hns, HNS3_INVALID_PVID, 0);
		if (ret) {
			hns3_err(hw, "pvid set fail in pf, ret =%d", ret);
			return ret;
		}

		ret = hns3_en_hw_strip_rxvtag(hns, false);
		if (ret) {
			hns3_err(hw, "rx strip configure fail in pf, ret =%d",
				 ret);
			return ret;
		}
	}

	return hns3_default_vlan_config(hns);
}

static int
hns3_restore_vlan_conf(struct hns3_adapter *hns)
{
	struct hns3_pf *pf = &hns->pf;
	struct hns3_hw *hw = &hns->hw;
	uint64_t offloads;
	bool enable;
	int ret;

	if (!hw->data->promiscuous) {
		/* restore vlan filter states */
		offloads = hw->data->dev_conf.rxmode.offloads;
		enable = offloads & RTE_ETH_RX_OFFLOAD_VLAN_FILTER ? true : false;
		ret = hns3_enable_vlan_filter(hns, enable);
		if (ret) {
			hns3_err(hw, "failed to restore vlan rx filter conf, "
				 "ret = %d", ret);
			return ret;
		}
	}

	ret = hns3_set_vlan_rx_offload_cfg(hns, &pf->vtag_config.rx_vcfg);
	if (ret) {
		hns3_err(hw, "failed to restore vlan rx conf, ret = %d", ret);
		return ret;
	}

	ret = hns3_set_vlan_tx_offload_cfg(hns, &pf->vtag_config.tx_vcfg);
	if (ret)
		hns3_err(hw, "failed to restore vlan tx conf, ret = %d", ret);

	return ret;
}

static int
hns3_dev_configure_vlan(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct rte_eth_dev_data *data = dev->data;
	struct rte_eth_txmode *txmode;
	struct hns3_hw *hw = &hns->hw;
	int mask;
	int ret;

	txmode = &data->dev_conf.txmode;
	if (txmode->hw_vlan_reject_tagged || txmode->hw_vlan_reject_untagged)
		hns3_warn(hw,
			  "hw_vlan_reject_tagged or hw_vlan_reject_untagged "
			  "configuration is not supported! Ignore these two "
			  "parameters: hw_vlan_reject_tagged(%u), "
			  "hw_vlan_reject_untagged(%u)",
			  txmode->hw_vlan_reject_tagged,
			  txmode->hw_vlan_reject_untagged);

	/* Apply vlan offload setting */
	mask = RTE_ETH_VLAN_STRIP_MASK | RTE_ETH_VLAN_FILTER_MASK;
	ret = hns3_vlan_offload_set(dev, mask);
	if (ret) {
		hns3_err(hw, "dev config rx vlan offload failed, ret = %d",
			 ret);
		return ret;
	}

	/*
	 * If pvid config is not set in rte_eth_conf, driver needn't to set
	 * VLAN pvid related configuration to hardware.
	 */
	if (txmode->pvid == 0 && txmode->hw_vlan_insert_pvid == 0)
		return 0;

	/* Apply pvid setting */
	ret = hns3_vlan_pvid_set(dev, txmode->pvid,
				 txmode->hw_vlan_insert_pvid);
	if (ret)
		hns3_err(hw, "dev config vlan pvid(%u) failed, ret = %d",
			 txmode->pvid, ret);

	return ret;
}

static int
hns3_config_tso(struct hns3_hw *hw, unsigned int tso_mss_min,
		unsigned int tso_mss_max)
{
	struct hns3_cfg_tso_status_cmd *req;
	struct hns3_cmd_desc desc;
	uint16_t tso_mss;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_TSO_GENERIC_CONFIG, false);

	req = (struct hns3_cfg_tso_status_cmd *)desc.data;

	tso_mss = 0;
	hns3_set_field(tso_mss, HNS3_TSO_MSS_MIN_M, HNS3_TSO_MSS_MIN_S,
		       tso_mss_min);
	req->tso_mss_min = rte_cpu_to_le_16(tso_mss);

	tso_mss = 0;
	hns3_set_field(tso_mss, HNS3_TSO_MSS_MIN_M, HNS3_TSO_MSS_MIN_S,
		       tso_mss_max);
	req->tso_mss_max = rte_cpu_to_le_16(tso_mss);

	return hns3_cmd_send(hw, &desc, 1);
}

static int
hns3_set_umv_space(struct hns3_hw *hw, uint16_t space_size,
		   uint16_t *allocated_size, bool is_alloc)
{
	struct hns3_umv_spc_alc_cmd *req;
	struct hns3_cmd_desc desc;
	int ret;

	req = (struct hns3_umv_spc_alc_cmd *)desc.data;
	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_MAC_VLAN_ALLOCATE, false);
	hns3_set_bit(req->allocate, HNS3_UMV_SPC_ALC_B, is_alloc ? 0 : 1);
	req->space_size = rte_cpu_to_le_32(space_size);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		PMD_INIT_LOG(ERR, "%s umv space failed for cmd_send, ret =%d",
			     is_alloc ? "allocate" : "free", ret);
		return ret;
	}

	if (is_alloc && allocated_size)
		*allocated_size = rte_le_to_cpu_32(desc.data[1]);

	return 0;
}

static int
hns3_init_umv_space(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	uint16_t allocated_size = 0;
	int ret;

	ret = hns3_set_umv_space(hw, pf->wanted_umv_size, &allocated_size,
				 true);
	if (ret)
		return ret;

	if (allocated_size < pf->wanted_umv_size)
		PMD_INIT_LOG(WARNING, "Alloc umv space failed, want %u, get %u",
			     pf->wanted_umv_size, allocated_size);

	pf->max_umv_size = (!!allocated_size) ? allocated_size :
						pf->wanted_umv_size;
	pf->used_umv_size = 0;
	return 0;
}

static int
hns3_uninit_umv_space(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	int ret;

	if (pf->max_umv_size == 0)
		return 0;

	ret = hns3_set_umv_space(hw, pf->max_umv_size, NULL, false);
	if (ret)
		return ret;

	pf->max_umv_size = 0;

	return 0;
}

static bool
hns3_is_umv_space_full(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	bool is_full;

	is_full = (pf->used_umv_size >= pf->max_umv_size);

	return is_full;
}

static void
hns3_update_umv_space(struct hns3_hw *hw, bool is_free)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;

	if (is_free) {
		if (pf->used_umv_size > 0)
			pf->used_umv_size--;
	} else
		pf->used_umv_size++;
}

static void
hns3_prepare_mac_addr(struct hns3_mac_vlan_tbl_entry_cmd *new_req,
		      const uint8_t *addr, bool is_mc)
{
	const unsigned char *mac_addr = addr;
	uint32_t high_val = ((uint32_t)mac_addr[3] << 24) |
			    ((uint32_t)mac_addr[2] << 16) |
			    ((uint32_t)mac_addr[1] << 8) |
			    (uint32_t)mac_addr[0];
	uint32_t low_val = ((uint32_t)mac_addr[5] << 8) | (uint32_t)mac_addr[4];

	hns3_set_bit(new_req->flags, HNS3_MAC_VLAN_BIT0_EN_B, 1);
	if (is_mc) {
		hns3_set_bit(new_req->entry_type, HNS3_MAC_VLAN_BIT0_EN_B, 0);
		hns3_set_bit(new_req->entry_type, HNS3_MAC_VLAN_BIT1_EN_B, 1);
		hns3_set_bit(new_req->mc_mac_en, HNS3_MAC_VLAN_BIT0_EN_B, 1);
	}

	new_req->mac_addr_hi32 = rte_cpu_to_le_32(high_val);
	new_req->mac_addr_lo16 = rte_cpu_to_le_16(low_val & 0xffff);
}

static int
hns3_get_mac_vlan_cmd_status(struct hns3_hw *hw, uint16_t cmdq_resp,
			     uint8_t resp_code,
			     enum hns3_mac_vlan_tbl_opcode op)
{
	if (cmdq_resp) {
		hns3_err(hw, "cmdq execute failed for get_mac_vlan_cmd_status,status=%u",
			 cmdq_resp);
		return -EIO;
	}

	if (op == HNS3_MAC_VLAN_ADD) {
		if (resp_code == 0 || resp_code == 1) {
			return 0;
		} else if (resp_code == HNS3_ADD_UC_OVERFLOW) {
			hns3_err(hw, "add mac addr failed for uc_overflow");
			return -ENOSPC;
		} else if (resp_code == HNS3_ADD_MC_OVERFLOW) {
			hns3_err(hw, "add mac addr failed for mc_overflow");
			return -ENOSPC;
		}

		hns3_err(hw, "add mac addr failed for undefined, code=%u",
			 resp_code);
		return -EIO;
	} else if (op == HNS3_MAC_VLAN_REMOVE) {
		if (resp_code == 0) {
			return 0;
		} else if (resp_code == 1) {
			hns3_dbg(hw, "remove mac addr failed for miss");
			return -ENOENT;
		}

		hns3_err(hw, "remove mac addr failed for undefined, code=%u",
			 resp_code);
		return -EIO;
	} else if (op == HNS3_MAC_VLAN_LKUP) {
		if (resp_code == 0) {
			return 0;
		} else if (resp_code == 1) {
			hns3_dbg(hw, "lookup mac addr failed for miss");
			return -ENOENT;
		}

		hns3_err(hw, "lookup mac addr failed for undefined, code=%u",
			 resp_code);
		return -EIO;
	}

	hns3_err(hw, "unknown opcode for get_mac_vlan_cmd_status, opcode=%u",
		 op);

	return -EINVAL;
}

static int
hns3_lookup_mac_vlan_tbl(struct hns3_hw *hw,
			 struct hns3_mac_vlan_tbl_entry_cmd *req,
			 struct hns3_cmd_desc *desc, uint8_t desc_num)
{
	uint8_t resp_code;
	uint16_t retval;
	int ret;
	int i;

	if (desc_num == HNS3_MC_MAC_VLAN_OPS_DESC_NUM) {
		for (i = 0; i < desc_num - 1; i++) {
			hns3_cmd_setup_basic_desc(&desc[i],
						  HNS3_OPC_MAC_VLAN_ADD, true);
			desc[i].flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);
			if (i == 0)
				memcpy(desc[i].data, req,
				sizeof(struct hns3_mac_vlan_tbl_entry_cmd));
		}
		hns3_cmd_setup_basic_desc(&desc[i], HNS3_OPC_MAC_VLAN_ADD,
					  true);
	} else {
		hns3_cmd_setup_basic_desc(&desc[0], HNS3_OPC_MAC_VLAN_ADD,
					  true);
		memcpy(desc[0].data, req,
		       sizeof(struct hns3_mac_vlan_tbl_entry_cmd));
	}
	ret = hns3_cmd_send(hw, desc, desc_num);
	if (ret) {
		hns3_err(hw, "lookup mac addr failed for cmd_send, ret =%d.",
			 ret);
		return ret;
	}
	resp_code = (rte_le_to_cpu_32(desc[0].data[0]) >> 8) & 0xff;
	retval = rte_le_to_cpu_16(desc[0].retval);

	return hns3_get_mac_vlan_cmd_status(hw, retval, resp_code,
					    HNS3_MAC_VLAN_LKUP);
}

static int
hns3_add_mac_vlan_tbl(struct hns3_hw *hw,
		      struct hns3_mac_vlan_tbl_entry_cmd *req,
		      struct hns3_cmd_desc *desc, uint8_t desc_num)
{
	uint8_t resp_code;
	uint16_t retval;
	int cfg_status;
	int ret;
	int i;

	if (desc_num == HNS3_UC_MAC_VLAN_OPS_DESC_NUM) {
		hns3_cmd_setup_basic_desc(desc, HNS3_OPC_MAC_VLAN_ADD, false);
		memcpy(desc->data, req,
		       sizeof(struct hns3_mac_vlan_tbl_entry_cmd));
		ret = hns3_cmd_send(hw, desc, desc_num);
		resp_code = (rte_le_to_cpu_32(desc->data[0]) >> 8) & 0xff;
		retval = rte_le_to_cpu_16(desc->retval);

		cfg_status = hns3_get_mac_vlan_cmd_status(hw, retval, resp_code,
							  HNS3_MAC_VLAN_ADD);
	} else {
		for (i = 0; i < desc_num; i++) {
			hns3_cmd_reuse_desc(&desc[i], false);
			if (i == desc_num - 1)
				desc[i].flag &=
					rte_cpu_to_le_16(~HNS3_CMD_FLAG_NEXT);
			else
				desc[i].flag |=
					rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);
		}
		memcpy(desc[0].data, req,
		       sizeof(struct hns3_mac_vlan_tbl_entry_cmd));
		desc[0].retval = 0;
		ret = hns3_cmd_send(hw, desc, desc_num);
		resp_code = (rte_le_to_cpu_32(desc[0].data[0]) >> 8) & 0xff;
		retval = rte_le_to_cpu_16(desc[0].retval);

		cfg_status = hns3_get_mac_vlan_cmd_status(hw, retval, resp_code,
							  HNS3_MAC_VLAN_ADD);
	}

	if (ret) {
		hns3_err(hw, "add mac addr failed for cmd_send, ret =%d", ret);
		return ret;
	}

	return cfg_status;
}

static int
hns3_remove_mac_vlan_tbl(struct hns3_hw *hw,
			 struct hns3_mac_vlan_tbl_entry_cmd *req)
{
	struct hns3_cmd_desc desc;
	uint8_t resp_code;
	uint16_t retval;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_MAC_VLAN_REMOVE, false);

	memcpy(desc.data, req, sizeof(struct hns3_mac_vlan_tbl_entry_cmd));

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		hns3_err(hw, "del mac addr failed for cmd_send, ret =%d", ret);
		return ret;
	}
	resp_code = (rte_le_to_cpu_32(desc.data[0]) >> 8) & 0xff;
	retval = rte_le_to_cpu_16(desc.retval);

	return hns3_get_mac_vlan_cmd_status(hw, retval, resp_code,
					    HNS3_MAC_VLAN_REMOVE);
}

static int
hns3_add_uc_mac_addr(struct hns3_hw *hw, struct rte_ether_addr *mac_addr)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_mac_vlan_tbl_entry_cmd req;
	struct hns3_pf *pf = &hns->pf;
	struct hns3_cmd_desc desc;
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE];
	uint16_t egress_port = 0;
	uint8_t vf_id;
	int ret;

	/* check if mac addr is valid */
	if (!rte_is_valid_assigned_ether_addr(mac_addr)) {
		hns3_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
				      mac_addr);
		hns3_err(hw, "Add unicast mac addr err! addr(%s) invalid",
			 mac_str);
		return -EINVAL;
	}

	memset(&req, 0, sizeof(req));

	/*
	 * In current version VF is not supported when PF is driven by DPDK
	 * driver, just need to configure parameters for PF vport.
	 */
	vf_id = HNS3_PF_FUNC_ID;
	hns3_set_field(egress_port, HNS3_MAC_EPORT_VFID_M,
		       HNS3_MAC_EPORT_VFID_S, vf_id);

	req.egress_port = rte_cpu_to_le_16(egress_port);

	hns3_prepare_mac_addr(&req, mac_addr->addr_bytes, false);

	/*
	 * Lookup the mac address in the mac_vlan table, and add
	 * it if the entry is inexistent. Repeated unicast entry
	 * is not allowed in the mac vlan table.
	 */
	ret = hns3_lookup_mac_vlan_tbl(hw, &req, &desc,
					HNS3_UC_MAC_VLAN_OPS_DESC_NUM);
	if (ret == -ENOENT) {
		if (!hns3_is_umv_space_full(hw)) {
			ret = hns3_add_mac_vlan_tbl(hw, &req, &desc,
						HNS3_UC_MAC_VLAN_OPS_DESC_NUM);
			if (!ret)
				hns3_update_umv_space(hw, false);
			return ret;
		}

		hns3_err(hw, "UC MAC table full(%u)", pf->used_umv_size);

		return -ENOSPC;
	}

	hns3_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE, mac_addr);

	/* check if we just hit the duplicate */
	if (ret == 0) {
		hns3_dbg(hw, "mac addr(%s) has been in the MAC table", mac_str);
		return 0;
	}

	hns3_err(hw, "PF failed to add unicast entry(%s) in the MAC table",
		 mac_str);

	return ret;
}

static int
hns3_remove_uc_mac_addr(struct hns3_hw *hw, struct rte_ether_addr *mac_addr)
{
	struct hns3_mac_vlan_tbl_entry_cmd req;
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE];
	int ret;

	/* check if mac addr is valid */
	if (!rte_is_valid_assigned_ether_addr(mac_addr)) {
		hns3_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
				      mac_addr);
		hns3_err(hw, "remove unicast mac addr err! addr(%s) invalid",
			 mac_str);
		return -EINVAL;
	}

	memset(&req, 0, sizeof(req));
	hns3_set_bit(req.entry_type, HNS3_MAC_VLAN_BIT0_EN_B, 0);
	hns3_prepare_mac_addr(&req, mac_addr->addr_bytes, false);
	ret = hns3_remove_mac_vlan_tbl(hw, &req);
	if (ret == -ENOENT) /* mac addr isn't existent in the mac vlan table. */
		return 0;
	else if (ret == 0)
		hns3_update_umv_space(hw, true);

	return ret;
}

static int
hns3_set_default_mac_addr(struct rte_eth_dev *dev,
			  struct rte_ether_addr *mac_addr)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_ether_addr *oaddr;
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE];
	int ret, ret_val;

	rte_spinlock_lock(&hw->lock);
	oaddr = (struct rte_ether_addr *)hw->mac.mac_addr;
	ret = hw->ops.del_uc_mac_addr(hw, oaddr);
	if (ret) {
		hns3_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
				       oaddr);
		hns3_warn(hw, "Remove old uc mac address(%s) fail: %d",
			  mac_str, ret);

		rte_spinlock_unlock(&hw->lock);
		return ret;
	}

	ret = hw->ops.add_uc_mac_addr(hw, mac_addr);
	if (ret) {
		hns3_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
				      mac_addr);
		hns3_err(hw, "Failed to set mac addr(%s): %d", mac_str, ret);
		goto err_add_uc_addr;
	}

	ret = hns3_pause_addr_cfg(hw, mac_addr->addr_bytes);
	if (ret) {
		hns3_err(hw, "Failed to configure mac pause address: %d", ret);
		goto err_pause_addr_cfg;
	}

	rte_ether_addr_copy(mac_addr,
			    (struct rte_ether_addr *)hw->mac.mac_addr);
	rte_spinlock_unlock(&hw->lock);

	return 0;

err_pause_addr_cfg:
	ret_val = hw->ops.del_uc_mac_addr(hw, mac_addr);
	if (ret_val) {
		hns3_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
				       mac_addr);
		hns3_warn(hw,
			  "Failed to roll back to del set mac addr(%s): %d",
			  mac_str, ret_val);
	}

err_add_uc_addr:
	ret_val = hw->ops.add_uc_mac_addr(hw, oaddr);
	if (ret_val) {
		hns3_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE, oaddr);
		hns3_warn(hw, "Failed to restore old uc mac addr(%s): %d",
			  mac_str, ret_val);
	}
	rte_spinlock_unlock(&hw->lock);

	return ret;
}

static void
hns3_update_desc_vfid(struct hns3_cmd_desc *desc, uint8_t vfid, bool clr)
{
#define HNS3_VF_NUM_IN_FIRST_DESC 192
	uint8_t word_num;
	uint8_t bit_num;

	if (vfid < HNS3_VF_NUM_IN_FIRST_DESC) {
		word_num = vfid / 32;
		bit_num = vfid % 32;
		if (clr)
			desc[1].data[word_num] &=
			    rte_cpu_to_le_32(~(1UL << bit_num));
		else
			desc[1].data[word_num] |=
			    rte_cpu_to_le_32(1UL << bit_num);
	} else {
		word_num = (vfid - HNS3_VF_NUM_IN_FIRST_DESC) / 32;
		bit_num = vfid % 32;
		if (clr)
			desc[2].data[word_num] &=
			    rte_cpu_to_le_32(~(1UL << bit_num));
		else
			desc[2].data[word_num] |=
			    rte_cpu_to_le_32(1UL << bit_num);
	}
}

static int
hns3_add_mc_mac_addr(struct hns3_hw *hw, struct rte_ether_addr *mac_addr)
{
	struct hns3_cmd_desc desc[HNS3_MC_MAC_VLAN_OPS_DESC_NUM];
	struct hns3_mac_vlan_tbl_entry_cmd req;
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE];
	uint8_t vf_id;
	int ret;
	int idx;

	/* Check if mac addr is valid */
	if (!rte_is_multicast_ether_addr(mac_addr)) {
		hns3_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
				      mac_addr);
		hns3_err(hw, "failed to add mc mac addr, addr(%s) invalid",
			 mac_str);
		return -EINVAL;
	}

	memset(&req, 0, sizeof(req));
	hns3_set_bit(req.entry_type, HNS3_MAC_VLAN_BIT0_EN_B, 0);
	hns3_prepare_mac_addr(&req, mac_addr->addr_bytes, true);
	ret = hns3_lookup_mac_vlan_tbl(hw, &req, desc,
					HNS3_MC_MAC_VLAN_OPS_DESC_NUM);
	if (ret) {
		/* This mac addr do not exist, add new entry for it */
		for (idx = 0; idx < HNS3_MC_MAC_VLAN_OPS_DESC_NUM; idx++)
			memset(desc[idx].data, 0, sizeof(desc[idx].data));
	}

	/*
	 * In current version VF is not supported when PF is driven by DPDK
	 * driver, just need to configure parameters for PF vport.
	 */
	vf_id = HNS3_PF_FUNC_ID;
	hns3_update_desc_vfid(desc, vf_id, false);
	ret = hns3_add_mac_vlan_tbl(hw, &req, desc,
					HNS3_MC_MAC_VLAN_OPS_DESC_NUM);
	if (ret) {
		if (ret == -ENOSPC)
			hns3_err(hw, "mc mac vlan table is full");
		hns3_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
				       mac_addr);
		hns3_err(hw, "failed to add mc mac addr(%s): %d", mac_str, ret);
	}

	return ret;
}

static int
hns3_remove_mc_mac_addr(struct hns3_hw *hw, struct rte_ether_addr *mac_addr)
{
	struct hns3_mac_vlan_tbl_entry_cmd req;
	struct hns3_cmd_desc desc[3];
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE];
	uint8_t vf_id;
	int ret;

	/* Check if mac addr is valid */
	if (!rte_is_multicast_ether_addr(mac_addr)) {
		hns3_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
				      mac_addr);
		hns3_err(hw, "Failed to rm mc mac addr, addr(%s) invalid",
			 mac_str);
		return -EINVAL;
	}

	memset(&req, 0, sizeof(req));
	hns3_set_bit(req.entry_type, HNS3_MAC_VLAN_BIT0_EN_B, 0);
	hns3_prepare_mac_addr(&req, mac_addr->addr_bytes, true);
	ret = hns3_lookup_mac_vlan_tbl(hw, &req, desc,
					HNS3_MC_MAC_VLAN_OPS_DESC_NUM);
	if (ret == 0) {
		/*
		 * This mac addr exist, remove this handle's VFID for it.
		 * In current version VF is not supported when PF is driven by
		 * DPDK driver, just need to configure parameters for PF vport.
		 */
		vf_id = HNS3_PF_FUNC_ID;
		hns3_update_desc_vfid(desc, vf_id, true);

		/* All the vfid is zero, so need to delete this entry */
		ret = hns3_remove_mac_vlan_tbl(hw, &req);
	} else if (ret == -ENOENT) {
		/* This mac addr doesn't exist. */
		return 0;
	}

	if (ret) {
		hns3_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
				      mac_addr);
		hns3_err(hw, "Failed to rm mc mac addr(%s): %d", mac_str, ret);
	}

	return ret;
}

static int
hns3_check_mq_mode(struct rte_eth_dev *dev)
{
	enum rte_eth_rx_mq_mode rx_mq_mode = dev->data->dev_conf.rxmode.mq_mode;
	enum rte_eth_tx_mq_mode tx_mq_mode = dev->data->dev_conf.txmode.mq_mode;
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_pf *pf = HNS3_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct rte_eth_dcb_rx_conf *dcb_rx_conf;
	struct rte_eth_dcb_tx_conf *dcb_tx_conf;
	uint8_t num_tc;
	int max_tc = 0;
	int i;

	if (((uint32_t)rx_mq_mode & RTE_ETH_MQ_RX_VMDQ_FLAG) ||
	    (tx_mq_mode == RTE_ETH_MQ_TX_VMDQ_DCB ||
	     tx_mq_mode == RTE_ETH_MQ_TX_VMDQ_ONLY)) {
		hns3_err(hw, "VMDQ is not supported, rx_mq_mode = %d, tx_mq_mode = %d.",
			 rx_mq_mode, tx_mq_mode);
		return -EOPNOTSUPP;
	}

	dcb_rx_conf = &dev->data->dev_conf.rx_adv_conf.dcb_rx_conf;
	dcb_tx_conf = &dev->data->dev_conf.tx_adv_conf.dcb_tx_conf;
	if ((uint32_t)rx_mq_mode & RTE_ETH_MQ_RX_DCB_FLAG) {
		if (dcb_rx_conf->nb_tcs > pf->tc_max) {
			hns3_err(hw, "nb_tcs(%u) > max_tc(%u) driver supported.",
				 dcb_rx_conf->nb_tcs, pf->tc_max);
			return -EINVAL;
		}

		if (!(dcb_rx_conf->nb_tcs == HNS3_4_TCS ||
		      dcb_rx_conf->nb_tcs == HNS3_8_TCS)) {
			hns3_err(hw, "on RTE_ETH_MQ_RX_DCB_RSS mode, "
				 "nb_tcs(%d) != %d or %d in rx direction.",
				 dcb_rx_conf->nb_tcs, HNS3_4_TCS, HNS3_8_TCS);
			return -EINVAL;
		}

		if (dcb_rx_conf->nb_tcs != dcb_tx_conf->nb_tcs) {
			hns3_err(hw, "num_tcs(%d) of tx is not equal to rx(%d)",
				 dcb_tx_conf->nb_tcs, dcb_rx_conf->nb_tcs);
			return -EINVAL;
		}

		for (i = 0; i < HNS3_MAX_USER_PRIO; i++) {
			if (dcb_rx_conf->dcb_tc[i] != dcb_tx_conf->dcb_tc[i]) {
				hns3_err(hw, "dcb_tc[%d] = %u in rx direction, "
					 "is not equal to one in tx direction.",
					 i, dcb_rx_conf->dcb_tc[i]);
				return -EINVAL;
			}
			if (dcb_rx_conf->dcb_tc[i] > max_tc)
				max_tc = dcb_rx_conf->dcb_tc[i];
		}

		num_tc = max_tc + 1;
		if (num_tc > dcb_rx_conf->nb_tcs) {
			hns3_err(hw, "max num_tc(%u) mapped > nb_tcs(%u)",
				 num_tc, dcb_rx_conf->nb_tcs);
			return -EINVAL;
		}
	}

	return 0;
}

static int
hns3_bind_ring_with_vector(struct hns3_hw *hw, uint16_t vector_id, bool en,
			   enum hns3_ring_type queue_type, uint16_t queue_id)
{
	struct hns3_cmd_desc desc;
	struct hns3_ctrl_vector_chain_cmd *req =
		(struct hns3_ctrl_vector_chain_cmd *)desc.data;
	enum hns3_opcode_type op;
	uint16_t tqp_type_and_id = 0;
	uint16_t type;
	uint16_t gl;
	int ret;

	op = en ? HNS3_OPC_ADD_RING_TO_VECTOR : HNS3_OPC_DEL_RING_TO_VECTOR;
	hns3_cmd_setup_basic_desc(&desc, op, false);
	req->int_vector_id = hns3_get_field(vector_id, HNS3_TQP_INT_ID_L_M,
					      HNS3_TQP_INT_ID_L_S);
	req->int_vector_id_h = hns3_get_field(vector_id, HNS3_TQP_INT_ID_H_M,
					      HNS3_TQP_INT_ID_H_S);

	if (queue_type == HNS3_RING_TYPE_RX)
		gl = HNS3_RING_GL_RX;
	else
		gl = HNS3_RING_GL_TX;

	type = queue_type;

	hns3_set_field(tqp_type_and_id, HNS3_INT_TYPE_M, HNS3_INT_TYPE_S,
		       type);
	hns3_set_field(tqp_type_and_id, HNS3_TQP_ID_M, HNS3_TQP_ID_S, queue_id);
	hns3_set_field(tqp_type_and_id, HNS3_INT_GL_IDX_M, HNS3_INT_GL_IDX_S,
		       gl);
	req->tqp_type_and_id[0] = rte_cpu_to_le_16(tqp_type_and_id);
	req->int_cause_num = 1;
	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		hns3_err(hw, "%s TQP %u fail, vector_id = %u, ret = %d.",
			 en ? "Map" : "Unmap", queue_id, vector_id, ret);
		return ret;
	}

	return 0;
}

static int
hns3_setup_dcb(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	if (!hns3_dev_get_support(hw, DCB)) {
		hns3_err(hw, "this port does not support dcb configurations.");
		return -EOPNOTSUPP;
	}

	if (hw->current_fc_status == HNS3_FC_STATUS_MAC_PAUSE) {
		hns3_err(hw, "MAC pause enabled, cannot config dcb info.");
		return -EOPNOTSUPP;
	}

	ret = hns3_dcb_configure(hns);
	if (ret)
		hns3_err(hw, "failed to config dcb: %d", ret);

	return ret;
}

static int
hns3_check_link_speed(struct hns3_hw *hw, uint32_t link_speeds)
{
	int ret;

	/*
	 * Some hardware doesn't support auto-negotiation, but users may not
	 * configure link_speeds (default 0), which means auto-negotiation.
	 * In this case, it should return success.
	 */
	if (link_speeds == RTE_ETH_LINK_SPEED_AUTONEG &&
	    hw->mac.support_autoneg == 0)
		return 0;

	if (link_speeds != RTE_ETH_LINK_SPEED_AUTONEG) {
		ret = hns3_check_port_speed(hw, link_speeds);
		if (ret)
			return ret;
	}

	return 0;
}

static int
hns3_check_dev_conf(struct rte_eth_dev *dev)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_eth_conf *conf = &dev->data->dev_conf;
	int ret;

	ret = hns3_check_mq_mode(dev);
	if (ret)
		return ret;

	return hns3_check_link_speed(hw, conf->link_speeds);
}

static int
hns3_dev_configure(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct rte_eth_conf *conf = &dev->data->dev_conf;
	enum rte_eth_rx_mq_mode mq_mode = conf->rxmode.mq_mode;
	struct hns3_hw *hw = &hns->hw;
	uint16_t nb_rx_q = dev->data->nb_rx_queues;
	uint16_t nb_tx_q = dev->data->nb_tx_queues;
	struct rte_eth_rss_conf rss_conf;
	bool gro_en;
	int ret;

	hw->cfg_max_queues = RTE_MAX(nb_rx_q, nb_tx_q);

	/*
	 * Some versions of hardware network engine does not support
	 * individually enable/disable/reset the Tx or Rx queue. These devices
	 * must enable/disable/reset Tx and Rx queues at the same time. When the
	 * numbers of Tx queues allocated by upper applications are not equal to
	 * the numbers of Rx queues, driver needs to setup fake Tx or Rx queues
	 * to adjust numbers of Tx/Rx queues. otherwise, network engine can not
	 * work as usual. But these fake queues are imperceptible, and can not
	 * be used by upper applications.
	 */
	ret = hns3_set_fake_rx_or_tx_queues(dev, nb_rx_q, nb_tx_q);
	if (ret) {
		hns3_err(hw, "fail to set Rx/Tx fake queues, ret = %d.", ret);
		hw->cfg_max_queues = 0;
		return ret;
	}

	hw->adapter_state = HNS3_NIC_CONFIGURING;
	ret = hns3_check_dev_conf(dev);
	if (ret)
		goto cfg_err;

	if ((uint32_t)mq_mode & RTE_ETH_MQ_RX_DCB_FLAG) {
		ret = hns3_setup_dcb(dev);
		if (ret)
			goto cfg_err;
	}

	if ((uint32_t)mq_mode & RTE_ETH_MQ_RX_RSS_FLAG) {
		conf->rxmode.offloads |= RTE_ETH_RX_OFFLOAD_RSS_HASH;
		rss_conf = conf->rx_adv_conf.rss_conf;
		ret = hns3_dev_rss_hash_update(dev, &rss_conf);
		if (ret)
			goto cfg_err;
	}

	ret = hns3_dev_mtu_set(dev, conf->rxmode.mtu);
	if (ret != 0)
		goto cfg_err;

	ret = hns3_mbuf_dyn_rx_timestamp_register(dev, conf);
	if (ret)
		goto cfg_err;

	ret = hns3_dev_configure_vlan(dev);
	if (ret)
		goto cfg_err;

	/* config hardware GRO */
	gro_en = conf->rxmode.offloads & RTE_ETH_RX_OFFLOAD_TCP_LRO ? true : false;
	ret = hns3_config_gro(hw, gro_en);
	if (ret)
		goto cfg_err;

	hns3_init_rx_ptype_tble(dev);
	hw->adapter_state = HNS3_NIC_CONFIGURED;

	return 0;

cfg_err:
	hw->cfg_max_queues = 0;
	(void)hns3_set_fake_rx_or_tx_queues(dev, 0, 0);
	hw->adapter_state = HNS3_NIC_INITIALIZED;

	return ret;
}

static int
hns3_set_mac_mtu(struct hns3_hw *hw, uint16_t new_mps)
{
	struct hns3_config_max_frm_size_cmd *req;
	struct hns3_cmd_desc desc;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_CONFIG_MAX_FRM_SIZE, false);

	req = (struct hns3_config_max_frm_size_cmd *)desc.data;
	req->max_frm_size = rte_cpu_to_le_16(new_mps);
	req->min_frm_size = RTE_ETHER_MIN_LEN;

	return hns3_cmd_send(hw, &desc, 1);
}

static int
hns3_config_mtu(struct hns3_hw *hw, uint16_t mps)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	int err;
	int ret;

	ret = hns3_set_mac_mtu(hw, mps);
	if (ret) {
		hns3_err(hw, "failed to set mtu, ret = %d", ret);
		return ret;
	}

	ret = hns3_buffer_alloc(hw);
	if (ret) {
		hns3_err(hw, "failed to allocate buffer, ret = %d", ret);
		goto rollback;
	}

	hns->pf.mps = mps;

	return 0;

rollback:
	err = hns3_set_mac_mtu(hw, hns->pf.mps);
	if (err)
		hns3_err(hw, "fail to rollback MTU, err = %d", err);

	return ret;
}

static int
hns3_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	uint32_t frame_size = mtu + HNS3_ETH_OVERHEAD;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	if (dev->data->dev_started) {
		hns3_err(hw, "Failed to set mtu, port %u must be stopped "
			 "before configuration", dev->data->port_id);
		return -EBUSY;
	}

	rte_spinlock_lock(&hw->lock);
	frame_size = RTE_MAX(frame_size, HNS3_DEFAULT_FRAME_LEN);

	/*
	 * Maximum value of frame_size is HNS3_MAX_FRAME_LEN, so it can safely
	 * assign to "uint16_t" type variable.
	 */
	ret = hns3_config_mtu(hw, (uint16_t)frame_size);
	if (ret) {
		rte_spinlock_unlock(&hw->lock);
		hns3_err(hw, "Failed to set mtu, port %u mtu %u: %d",
			 dev->data->port_id, mtu, ret);
		return ret;
	}

	rte_spinlock_unlock(&hw->lock);

	return 0;
}

static uint32_t
hns3_get_copper_port_speed_capa(uint32_t supported_speed)
{
	uint32_t speed_capa = 0;

	if (supported_speed & HNS3_PHY_LINK_SPEED_10M_HD_BIT)
		speed_capa |= RTE_ETH_LINK_SPEED_10M_HD;
	if (supported_speed & HNS3_PHY_LINK_SPEED_10M_BIT)
		speed_capa |= RTE_ETH_LINK_SPEED_10M;
	if (supported_speed & HNS3_PHY_LINK_SPEED_100M_HD_BIT)
		speed_capa |= RTE_ETH_LINK_SPEED_100M_HD;
	if (supported_speed & HNS3_PHY_LINK_SPEED_100M_BIT)
		speed_capa |= RTE_ETH_LINK_SPEED_100M;
	if (supported_speed & HNS3_PHY_LINK_SPEED_1000M_BIT)
		speed_capa |= RTE_ETH_LINK_SPEED_1G;

	return speed_capa;
}

static uint32_t
hns3_get_firber_port_speed_capa(uint32_t supported_speed)
{
	uint32_t speed_capa = 0;

	if (supported_speed & HNS3_FIBER_LINK_SPEED_1G_BIT)
		speed_capa |= RTE_ETH_LINK_SPEED_1G;
	if (supported_speed & HNS3_FIBER_LINK_SPEED_10G_BIT)
		speed_capa |= RTE_ETH_LINK_SPEED_10G;
	if (supported_speed & HNS3_FIBER_LINK_SPEED_25G_BIT)
		speed_capa |= RTE_ETH_LINK_SPEED_25G;
	if (supported_speed & HNS3_FIBER_LINK_SPEED_40G_BIT)
		speed_capa |= RTE_ETH_LINK_SPEED_40G;
	if (supported_speed & HNS3_FIBER_LINK_SPEED_50G_BIT)
		speed_capa |= RTE_ETH_LINK_SPEED_50G;
	if (supported_speed & HNS3_FIBER_LINK_SPEED_100G_BIT)
		speed_capa |= RTE_ETH_LINK_SPEED_100G;
	if (supported_speed & HNS3_FIBER_LINK_SPEED_200G_BIT)
		speed_capa |= RTE_ETH_LINK_SPEED_200G;

	return speed_capa;
}

uint32_t
hns3_get_speed_capa(struct hns3_hw *hw)
{
	struct hns3_mac *mac = &hw->mac;
	uint32_t speed_capa;

	if (mac->media_type == HNS3_MEDIA_TYPE_COPPER)
		speed_capa =
			hns3_get_copper_port_speed_capa(mac->supported_speed);
	else
		speed_capa =
			hns3_get_firber_port_speed_capa(mac->supported_speed);

	if (mac->support_autoneg == 0)
		speed_capa |= RTE_ETH_LINK_SPEED_FIXED;

	return speed_capa;
}

static int
hns3_update_port_link_info(struct rte_eth_dev *eth_dev)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	int ret;

	(void)hns3_update_link_status(hw);

	ret = hns3_update_link_info(eth_dev);
	if (ret)
		hw->mac.link_status = RTE_ETH_LINK_DOWN;

	return ret;
}

static void
hns3_setup_linkstatus(struct rte_eth_dev *eth_dev,
		      struct rte_eth_link *new_link)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct hns3_mac *mac = &hw->mac;

	switch (mac->link_speed) {
	case RTE_ETH_SPEED_NUM_10M:
	case RTE_ETH_SPEED_NUM_100M:
	case RTE_ETH_SPEED_NUM_1G:
	case RTE_ETH_SPEED_NUM_10G:
	case RTE_ETH_SPEED_NUM_25G:
	case RTE_ETH_SPEED_NUM_40G:
	case RTE_ETH_SPEED_NUM_50G:
	case RTE_ETH_SPEED_NUM_100G:
	case RTE_ETH_SPEED_NUM_200G:
		if (mac->link_status)
			new_link->link_speed = mac->link_speed;
		break;
	default:
		if (mac->link_status)
			new_link->link_speed = RTE_ETH_SPEED_NUM_UNKNOWN;
		break;
	}

	if (!mac->link_status)
		new_link->link_speed = RTE_ETH_SPEED_NUM_NONE;

	new_link->link_duplex = mac->link_duplex;
	new_link->link_status = mac->link_status ? RTE_ETH_LINK_UP : RTE_ETH_LINK_DOWN;
	new_link->link_autoneg = mac->link_autoneg;
}

static int
hns3_dev_link_update(struct rte_eth_dev *eth_dev, int wait_to_complete)
{
#define HNS3_LINK_CHECK_INTERVAL 100  /* 100ms */
#define HNS3_MAX_LINK_CHECK_TIMES 20  /* 2s (100 * 20ms) in total */

	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	uint32_t retry_cnt = HNS3_MAX_LINK_CHECK_TIMES;
	struct hns3_mac *mac = &hw->mac;
	struct rte_eth_link new_link;
	int ret;

	memset(&new_link, 0, sizeof(new_link));
	/* When port is stopped, report link down. */
	if (eth_dev->data->dev_started == 0) {
		new_link.link_autoneg = mac->link_autoneg;
		new_link.link_duplex = mac->link_duplex;
		new_link.link_speed = RTE_ETH_SPEED_NUM_NONE;
		new_link.link_status = RTE_ETH_LINK_DOWN;
		goto out;
	}

	do {
		ret = hns3_update_port_link_info(eth_dev);
		if (ret) {
			hns3_err(hw, "failed to get port link info, ret = %d.",
				 ret);
			break;
		}

		if (!wait_to_complete || mac->link_status == RTE_ETH_LINK_UP)
			break;

		rte_delay_ms(HNS3_LINK_CHECK_INTERVAL);
	} while (retry_cnt--);

	hns3_setup_linkstatus(eth_dev, &new_link);

out:
	return rte_eth_linkstatus_set(eth_dev, &new_link);
}

static int
hns3_dev_set_link_up(struct rte_eth_dev *dev)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret;

	/*
	 * The "tx_pkt_burst" will be restored. But the secondary process does
	 * not support the mechanism for notifying the primary process.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		hns3_err(hw, "secondary process does not support to set link up.");
		return -ENOTSUP;
	}

	/*
	 * If device isn't started Rx/Tx function is still disabled, setting
	 * link up is not allowed. But it is probably better to return success
	 * to reduce the impact on the upper layer.
	 */
	if (hw->adapter_state != HNS3_NIC_STARTED) {
		hns3_info(hw, "device isn't started, can't set link up.");
		return 0;
	}

	if (!hw->set_link_down)
		return 0;

	rte_spinlock_lock(&hw->lock);
	ret = hns3_cfg_mac_mode(hw, true);
	if (ret) {
		rte_spinlock_unlock(&hw->lock);
		hns3_err(hw, "failed to set link up, ret = %d", ret);
		return ret;
	}

	hw->set_link_down = false;
	hns3_start_tx_datapath(dev);
	rte_spinlock_unlock(&hw->lock);

	return 0;
}

static int
hns3_dev_set_link_down(struct rte_eth_dev *dev)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret;

	/*
	 * The "tx_pkt_burst" will be set to dummy function. But the secondary
	 * process does not support the mechanism for notifying the primary
	 * process.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		hns3_err(hw, "secondary process does not support to set link down.");
		return -ENOTSUP;
	}

	/*
	 * If device isn't started or the API has been called, link status is
	 * down, return success.
	 */
	if (hw->adapter_state != HNS3_NIC_STARTED || hw->set_link_down)
		return 0;

	rte_spinlock_lock(&hw->lock);
	hns3_stop_tx_datapath(dev);
	ret = hns3_cfg_mac_mode(hw, false);
	if (ret) {
		hns3_start_tx_datapath(dev);
		rte_spinlock_unlock(&hw->lock);
		hns3_err(hw, "failed to set link down, ret = %d", ret);
		return ret;
	}

	hw->set_link_down = true;
	rte_spinlock_unlock(&hw->lock);

	return 0;
}

static int
hns3_parse_func_status(struct hns3_hw *hw, struct hns3_func_status_cmd *status)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;

	if (!(status->pf_state & HNS3_PF_STATE_DONE))
		return -EINVAL;

	pf->is_main_pf = (status->pf_state & HNS3_PF_STATE_MAIN) ? true : false;

	return 0;
}

static int
hns3_query_function_status(struct hns3_hw *hw)
{
#define HNS3_QUERY_MAX_CNT		10
#define HNS3_QUERY_SLEEP_MSCOEND	1
	struct hns3_func_status_cmd *req;
	struct hns3_cmd_desc desc;
	int timeout = 0;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_QUERY_FUNC_STATUS, true);
	req = (struct hns3_func_status_cmd *)desc.data;

	do {
		ret = hns3_cmd_send(hw, &desc, 1);
		if (ret) {
			PMD_INIT_LOG(ERR, "query function status failed %d",
				     ret);
			return ret;
		}

		/* Check pf reset is done */
		if (req->pf_state)
			break;

		rte_delay_ms(HNS3_QUERY_SLEEP_MSCOEND);
	} while (timeout++ < HNS3_QUERY_MAX_CNT);

	return hns3_parse_func_status(hw, req);
}

static int
hns3_get_pf_max_tqp_num(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;

	if (pf->tqp_config_mode == HNS3_FLEX_MAX_TQP_NUM_MODE) {
		/*
		 * The total_tqps_num obtained from firmware is maximum tqp
		 * numbers of this port, which should be used for PF and VFs.
		 * There is no need for pf to have so many tqp numbers in
		 * most cases. RTE_LIBRTE_HNS3_MAX_TQP_NUM_PER_PF,
		 * coming from config file, is assigned to maximum queue number
		 * for the PF of this port by user. So users can modify the
		 * maximum queue number of PF according to their own application
		 * scenarios, which is more flexible to use. In addition, many
		 * memories can be saved due to allocating queue statistics
		 * room according to the actual number of queues required. The
		 * maximum queue number of PF for network engine with
		 * revision_id greater than 0x30 is assigned by config file.
		 */
		if (RTE_LIBRTE_HNS3_MAX_TQP_NUM_PER_PF <= 0) {
			hns3_err(hw, "RTE_LIBRTE_HNS3_MAX_TQP_NUM_PER_PF(%d) "
				 "must be greater than 0.",
				 RTE_LIBRTE_HNS3_MAX_TQP_NUM_PER_PF);
			return -EINVAL;
		}

		hw->tqps_num = RTE_MIN(RTE_LIBRTE_HNS3_MAX_TQP_NUM_PER_PF,
				       hw->total_tqps_num);
	} else {
		/*
		 * Due to the limitation on the number of PF interrupts
		 * available, the maximum queue number assigned to PF on
		 * the network engine with revision_id 0x21 is 64.
		 */
		hw->tqps_num = RTE_MIN(hw->total_tqps_num,
				       HNS3_MAX_TQP_NUM_HIP08_PF);
	}

	return 0;
}

static int
hns3_query_pf_resource(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	struct hns3_pf_res_cmd *req;
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_QUERY_PF_RSRC, true);
	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		PMD_INIT_LOG(ERR, "query pf resource failed %d", ret);
		return ret;
	}

	req = (struct hns3_pf_res_cmd *)desc.data;
	hw->total_tqps_num = rte_le_to_cpu_16(req->tqp_num) +
			     rte_le_to_cpu_16(req->ext_tqp_num);
	ret = hns3_get_pf_max_tqp_num(hw);
	if (ret)
		return ret;

	pf->pkt_buf_size = rte_le_to_cpu_16(req->buf_size) << HNS3_BUF_UNIT_S;
	pf->func_num = rte_le_to_cpu_16(req->pf_own_fun_number);

	if (req->tx_buf_size)
		pf->tx_buf_size =
		    rte_le_to_cpu_16(req->tx_buf_size) << HNS3_BUF_UNIT_S;
	else
		pf->tx_buf_size = HNS3_DEFAULT_TX_BUF;

	pf->tx_buf_size = roundup(pf->tx_buf_size, HNS3_BUF_SIZE_UNIT);

	if (req->dv_buf_size)
		pf->dv_buf_size =
		    rte_le_to_cpu_16(req->dv_buf_size) << HNS3_BUF_UNIT_S;
	else
		pf->dv_buf_size = HNS3_DEFAULT_DV;

	pf->dv_buf_size = roundup(pf->dv_buf_size, HNS3_BUF_SIZE_UNIT);

	hw->num_msi =
		hns3_get_field(rte_le_to_cpu_16(req->nic_pf_intr_vector_number),
			       HNS3_PF_VEC_NUM_M, HNS3_PF_VEC_NUM_S);

	return 0;
}

static void
hns3_parse_cfg(struct hns3_cfg *cfg, struct hns3_cmd_desc *desc)
{
	struct hns3_cfg_param_cmd *req;
	uint64_t mac_addr_tmp_high;
	uint8_t ext_rss_size_max;
	uint64_t mac_addr_tmp;
	uint32_t i;

	req = (struct hns3_cfg_param_cmd *)desc[0].data;

	/* get the configuration */
	cfg->tc_num = hns3_get_field(rte_le_to_cpu_32(req->param[0]),
				     HNS3_CFG_TC_NUM_M, HNS3_CFG_TC_NUM_S);

	cfg->phy_addr = hns3_get_field(rte_le_to_cpu_32(req->param[1]),
				       HNS3_CFG_PHY_ADDR_M,
				       HNS3_CFG_PHY_ADDR_S);
	cfg->media_type = hns3_get_field(rte_le_to_cpu_32(req->param[1]),
					 HNS3_CFG_MEDIA_TP_M,
					 HNS3_CFG_MEDIA_TP_S);
	/* get mac address */
	mac_addr_tmp = rte_le_to_cpu_32(req->param[2]);
	mac_addr_tmp_high = hns3_get_field(rte_le_to_cpu_32(req->param[3]),
					   HNS3_CFG_MAC_ADDR_H_M,
					   HNS3_CFG_MAC_ADDR_H_S);

	mac_addr_tmp |= (mac_addr_tmp_high << 31) << 1;

	cfg->default_speed = hns3_get_field(rte_le_to_cpu_32(req->param[3]),
					    HNS3_CFG_DEFAULT_SPEED_M,
					    HNS3_CFG_DEFAULT_SPEED_S);
	cfg->rss_size_max = hns3_get_field(rte_le_to_cpu_32(req->param[3]),
					   HNS3_CFG_RSS_SIZE_M,
					   HNS3_CFG_RSS_SIZE_S);

	for (i = 0; i < RTE_ETHER_ADDR_LEN; i++)
		cfg->mac_addr[i] = (mac_addr_tmp >> (8 * i)) & 0xff;

	req = (struct hns3_cfg_param_cmd *)desc[1].data;
	cfg->numa_node_map = rte_le_to_cpu_32(req->param[0]);

	cfg->speed_ability = hns3_get_field(rte_le_to_cpu_32(req->param[1]),
					    HNS3_CFG_SPEED_ABILITY_M,
					    HNS3_CFG_SPEED_ABILITY_S);
	cfg->umv_space = hns3_get_field(rte_le_to_cpu_32(req->param[1]),
					HNS3_CFG_UMV_TBL_SPACE_M,
					HNS3_CFG_UMV_TBL_SPACE_S);
	if (!cfg->umv_space)
		cfg->umv_space = HNS3_DEFAULT_UMV_SPACE_PER_PF;

	ext_rss_size_max = hns3_get_field(rte_le_to_cpu_32(req->param[2]),
					       HNS3_CFG_EXT_RSS_SIZE_M,
					       HNS3_CFG_EXT_RSS_SIZE_S);
	/*
	 * Field ext_rss_size_max obtained from firmware will be more flexible
	 * for future changes and expansions, which is an exponent of 2, instead
	 * of reading out directly. If this field is not zero, hns3 PF PMD
	 * uses it as rss_size_max under one TC. Device, whose revision
	 * id is greater than or equal to PCI_REVISION_ID_HIP09_A, obtains the
	 * maximum number of queues supported under a TC through this field.
	 */
	if (ext_rss_size_max)
		cfg->rss_size_max = 1U << ext_rss_size_max;
}

/* hns3_get_board_cfg: query the static parameter from NCL_config file in flash
 * @hw: pointer to struct hns3_hw
 * @hcfg: the config structure to be getted
 */
static int
hns3_get_board_cfg(struct hns3_hw *hw, struct hns3_cfg *hcfg)
{
	struct hns3_cmd_desc desc[HNS3_PF_CFG_DESC_NUM];
	struct hns3_cfg_param_cmd *req;
	uint32_t offset;
	uint32_t i;
	int ret;

	for (i = 0; i < HNS3_PF_CFG_DESC_NUM; i++) {
		offset = 0;
		req = (struct hns3_cfg_param_cmd *)desc[i].data;
		hns3_cmd_setup_basic_desc(&desc[i], HNS3_OPC_GET_CFG_PARAM,
					  true);
		hns3_set_field(offset, HNS3_CFG_OFFSET_M, HNS3_CFG_OFFSET_S,
			       i * HNS3_CFG_RD_LEN_BYTES);
		/* Len should be divided by 4 when send to hardware */
		hns3_set_field(offset, HNS3_CFG_RD_LEN_M, HNS3_CFG_RD_LEN_S,
			       HNS3_CFG_RD_LEN_BYTES / HNS3_CFG_RD_LEN_UNIT);
		req->offset = rte_cpu_to_le_32(offset);
	}

	ret = hns3_cmd_send(hw, desc, HNS3_PF_CFG_DESC_NUM);
	if (ret) {
		PMD_INIT_LOG(ERR, "get config failed %d.", ret);
		return ret;
	}

	hns3_parse_cfg(hcfg, desc);

	return 0;
}

static int
hns3_parse_speed(int speed_cmd, uint32_t *speed)
{
	switch (speed_cmd) {
	case HNS3_CFG_SPEED_10M:
		*speed = RTE_ETH_SPEED_NUM_10M;
		break;
	case HNS3_CFG_SPEED_100M:
		*speed = RTE_ETH_SPEED_NUM_100M;
		break;
	case HNS3_CFG_SPEED_1G:
		*speed = RTE_ETH_SPEED_NUM_1G;
		break;
	case HNS3_CFG_SPEED_10G:
		*speed = RTE_ETH_SPEED_NUM_10G;
		break;
	case HNS3_CFG_SPEED_25G:
		*speed = RTE_ETH_SPEED_NUM_25G;
		break;
	case HNS3_CFG_SPEED_40G:
		*speed = RTE_ETH_SPEED_NUM_40G;
		break;
	case HNS3_CFG_SPEED_50G:
		*speed = RTE_ETH_SPEED_NUM_50G;
		break;
	case HNS3_CFG_SPEED_100G:
		*speed = RTE_ETH_SPEED_NUM_100G;
		break;
	case HNS3_CFG_SPEED_200G:
		*speed = RTE_ETH_SPEED_NUM_200G;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int
hns3_get_capability(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct rte_pci_device *pci_dev;
	struct hns3_pf *pf = &hns->pf;
	struct rte_eth_dev *eth_dev;
	uint16_t device_id;
	int ret;

	eth_dev = &rte_eth_devices[hw->data->port_id];
	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	device_id = pci_dev->id.device_id;

	if (device_id == HNS3_DEV_ID_25GE_RDMA ||
	    device_id == HNS3_DEV_ID_50GE_RDMA ||
	    device_id == HNS3_DEV_ID_100G_RDMA_MACSEC ||
	    device_id == HNS3_DEV_ID_200G_RDMA)
		hns3_set_bit(hw->capability, HNS3_DEV_SUPPORT_DCB_B, 1);

	ret = hns3_get_pci_revision_id(hw, &hw->revision);
	if (ret)
		return ret;

	ret = hns3_query_mac_stats_reg_num(hw);
	if (ret)
		return ret;

	if (hw->revision < PCI_REVISION_ID_HIP09_A) {
		hns3_set_default_dev_specifications(hw);
		hw->intr.mapping_mode = HNS3_INTR_MAPPING_VEC_RSV_ONE;
		hw->intr.gl_unit = HNS3_INTR_COALESCE_GL_UINT_2US;
		hw->tso_mode = HNS3_TSO_SW_CAL_PSEUDO_H_CSUM;
		hw->vlan_mode = HNS3_SW_SHIFT_AND_DISCARD_MODE;
		hw->drop_stats_mode = HNS3_PKTS_DROP_STATS_MODE1;
		hw->min_tx_pkt_len = HNS3_HIP08_MIN_TX_PKT_LEN;
		pf->tqp_config_mode = HNS3_FIXED_MAX_TQP_NUM_MODE;
		hw->rss_info.ipv6_sctp_offload_supported = false;
		hw->udp_cksum_mode = HNS3_SPECIAL_PORT_SW_CKSUM_MODE;
		pf->support_multi_tc_pause = false;
		return 0;
	}

	ret = hns3_query_dev_specifications(hw);
	if (ret) {
		PMD_INIT_LOG(ERR,
			     "failed to query dev specifications, ret = %d",
			     ret);
		return ret;
	}

	hw->intr.mapping_mode = HNS3_INTR_MAPPING_VEC_ALL;
	hw->intr.gl_unit = HNS3_INTR_COALESCE_GL_UINT_1US;
	hw->tso_mode = HNS3_TSO_HW_CAL_PSEUDO_H_CSUM;
	hw->vlan_mode = HNS3_HW_SHIFT_AND_DISCARD_MODE;
	hw->drop_stats_mode = HNS3_PKTS_DROP_STATS_MODE2;
	pf->tqp_config_mode = HNS3_FLEX_MAX_TQP_NUM_MODE;
	hw->rss_info.ipv6_sctp_offload_supported = true;
	hw->udp_cksum_mode = HNS3_SPECIAL_PORT_HW_CKSUM_MODE;
	pf->support_multi_tc_pause = true;

	return 0;
}

static int
hns3_check_media_type(struct hns3_hw *hw, uint8_t media_type)
{
	int ret;

	switch (media_type) {
	case HNS3_MEDIA_TYPE_COPPER:
		if (!hns3_dev_get_support(hw, COPPER)) {
			PMD_INIT_LOG(ERR,
				     "Media type is copper, not supported.");
			ret = -EOPNOTSUPP;
		} else {
			ret = 0;
		}
		break;
	case HNS3_MEDIA_TYPE_FIBER:
	case HNS3_MEDIA_TYPE_BACKPLANE:
		ret = 0;
		break;
	default:
		PMD_INIT_LOG(ERR, "Unknown media type = %u!", media_type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int
hns3_get_board_configuration(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	struct hns3_cfg cfg;
	int ret;

	memset(&cfg, 0, sizeof(cfg));
	ret = hns3_get_board_cfg(hw, &cfg);
	if (ret) {
		PMD_INIT_LOG(ERR, "get board config failed %d", ret);
		return ret;
	}

	ret = hns3_check_media_type(hw, cfg.media_type);
	if (ret)
		return ret;

	hw->mac.media_type = cfg.media_type;
	hw->rss_size_max = cfg.rss_size_max;
	memcpy(hw->mac.mac_addr, cfg.mac_addr, RTE_ETHER_ADDR_LEN);
	hw->mac.phy_addr = cfg.phy_addr;
	hw->dcb_info.num_pg = 1;
	hw->dcb_info.hw_pfc_map = 0;

	ret = hns3_parse_speed(cfg.default_speed, &hw->mac.link_speed);
	if (ret) {
		PMD_INIT_LOG(ERR, "Get wrong speed %u, ret = %d",
			     cfg.default_speed, ret);
		return ret;
	}

	pf->tc_max = cfg.tc_num;
	if (pf->tc_max > HNS3_MAX_TC_NUM || pf->tc_max < 1) {
		PMD_INIT_LOG(WARNING,
			     "Get TC num(%u) from flash, set TC num to 1",
			     pf->tc_max);
		pf->tc_max = 1;
	}

	/* Dev does not support DCB */
	if (!hns3_dev_get_support(hw, DCB)) {
		pf->tc_max = 1;
		pf->pfc_max = 0;
	} else
		pf->pfc_max = pf->tc_max;

	hw->dcb_info.num_tc = 1;
	hw->alloc_rss_size = RTE_MIN(hw->rss_size_max,
				     hw->tqps_num / hw->dcb_info.num_tc);
	hns3_set_bit(hw->hw_tc_map, 0, 1);
	pf->tx_sch_mode = HNS3_FLAG_TC_BASE_SCH_MODE;

	pf->wanted_umv_size = cfg.umv_space;

	return ret;
}

static int
hns3_get_configuration(struct hns3_hw *hw)
{
	int ret;

	ret = hns3_query_function_status(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to query function status: %d.", ret);
		return ret;
	}

	/* Get device capability */
	ret = hns3_get_capability(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "failed to get device capability: %d.", ret);
		return ret;
	}

	/* Get pf resource */
	ret = hns3_query_pf_resource(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to query pf resource: %d", ret);
		return ret;
	}

	ret = hns3_get_board_configuration(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "failed to get board configuration: %d", ret);
		return ret;
	}

	ret = hns3_query_dev_fec_info(hw);
	if (ret)
		PMD_INIT_LOG(ERR,
			     "failed to query FEC information, ret = %d", ret);

	return ret;
}

static int
hns3_map_tqps_to_func(struct hns3_hw *hw, uint16_t func_id, uint16_t tqp_pid,
		      uint16_t tqp_vid, bool is_pf)
{
	struct hns3_tqp_map_cmd *req;
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_SET_TQP_MAP, false);

	req = (struct hns3_tqp_map_cmd *)desc.data;
	req->tqp_id = rte_cpu_to_le_16(tqp_pid);
	req->tqp_vf = func_id;
	req->tqp_flag = 1 << HNS3_TQP_MAP_EN_B;
	if (!is_pf)
		req->tqp_flag |= (1 << HNS3_TQP_MAP_TYPE_B);
	req->tqp_vid = rte_cpu_to_le_16(tqp_vid);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		PMD_INIT_LOG(ERR, "TQP map failed %d", ret);

	return ret;
}

static int
hns3_map_tqp(struct hns3_hw *hw)
{
	uint16_t i;
	int ret;

	/*
	 * In current version, VF is not supported when PF is driven by DPDK
	 * driver, so we assign total tqps_num tqps allocated to this port
	 * to PF.
	 */
	for (i = 0; i < hw->total_tqps_num; i++) {
		ret = hns3_map_tqps_to_func(hw, HNS3_PF_FUNC_ID, i, i, true);
		if (ret)
			return ret;
	}

	return 0;
}

static int
hns3_cfg_mac_speed_dup_hw(struct hns3_hw *hw, uint32_t speed, uint8_t duplex)
{
	struct hns3_config_mac_speed_dup_cmd *req;
	struct hns3_cmd_desc desc;
	int ret;

	req = (struct hns3_config_mac_speed_dup_cmd *)desc.data;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_CONFIG_SPEED_DUP, false);

	hns3_set_bit(req->speed_dup, HNS3_CFG_DUPLEX_B, !!duplex ? 1 : 0);

	switch (speed) {
	case RTE_ETH_SPEED_NUM_10M:
		hns3_set_field(req->speed_dup, HNS3_CFG_SPEED_M,
			       HNS3_CFG_SPEED_S, HNS3_CFG_SPEED_10M);
		break;
	case RTE_ETH_SPEED_NUM_100M:
		hns3_set_field(req->speed_dup, HNS3_CFG_SPEED_M,
			       HNS3_CFG_SPEED_S, HNS3_CFG_SPEED_100M);
		break;
	case RTE_ETH_SPEED_NUM_1G:
		hns3_set_field(req->speed_dup, HNS3_CFG_SPEED_M,
			       HNS3_CFG_SPEED_S, HNS3_CFG_SPEED_1G);
		break;
	case RTE_ETH_SPEED_NUM_10G:
		hns3_set_field(req->speed_dup, HNS3_CFG_SPEED_M,
			       HNS3_CFG_SPEED_S, HNS3_CFG_SPEED_10G);
		break;
	case RTE_ETH_SPEED_NUM_25G:
		hns3_set_field(req->speed_dup, HNS3_CFG_SPEED_M,
			       HNS3_CFG_SPEED_S, HNS3_CFG_SPEED_25G);
		break;
	case RTE_ETH_SPEED_NUM_40G:
		hns3_set_field(req->speed_dup, HNS3_CFG_SPEED_M,
			       HNS3_CFG_SPEED_S, HNS3_CFG_SPEED_40G);
		break;
	case RTE_ETH_SPEED_NUM_50G:
		hns3_set_field(req->speed_dup, HNS3_CFG_SPEED_M,
			       HNS3_CFG_SPEED_S, HNS3_CFG_SPEED_50G);
		break;
	case RTE_ETH_SPEED_NUM_100G:
		hns3_set_field(req->speed_dup, HNS3_CFG_SPEED_M,
			       HNS3_CFG_SPEED_S, HNS3_CFG_SPEED_100G);
		break;
	case RTE_ETH_SPEED_NUM_200G:
		hns3_set_field(req->speed_dup, HNS3_CFG_SPEED_M,
			       HNS3_CFG_SPEED_S, HNS3_CFG_SPEED_200G);
		break;
	default:
		PMD_INIT_LOG(ERR, "invalid speed (%u)", speed);
		return -EINVAL;
	}

	hns3_set_bit(req->mac_change_fec_en, HNS3_CFG_MAC_SPEED_CHANGE_EN_B, 1);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		PMD_INIT_LOG(ERR, "mac speed/duplex config cmd failed %d", ret);

	return ret;
}

static int
hns3_tx_buffer_calc(struct hns3_hw *hw, struct hns3_pkt_buf_alloc *buf_alloc)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	struct hns3_priv_buf *priv;
	uint32_t i, total_size;

	total_size = pf->pkt_buf_size;

	/* alloc tx buffer for all enabled tc */
	for (i = 0; i < HNS3_MAX_TC_NUM; i++) {
		priv = &buf_alloc->priv_buf[i];

		if (hw->hw_tc_map & BIT(i)) {
			if (total_size < pf->tx_buf_size)
				return -ENOMEM;

			priv->tx_buf_size = pf->tx_buf_size;
		} else
			priv->tx_buf_size = 0;

		total_size -= priv->tx_buf_size;
	}

	return 0;
}

static int
hns3_tx_buffer_alloc(struct hns3_hw *hw, struct hns3_pkt_buf_alloc *buf_alloc)
{
/* TX buffer size is unit by 128 byte */
#define HNS3_BUF_SIZE_UNIT_SHIFT	7
#define HNS3_BUF_SIZE_UPDATE_EN_MSK	BIT(15)
	struct hns3_tx_buff_alloc_cmd *req;
	struct hns3_cmd_desc desc;
	uint32_t buf_size;
	uint32_t i;
	int ret;

	req = (struct hns3_tx_buff_alloc_cmd *)desc.data;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_TX_BUFF_ALLOC, 0);
	for (i = 0; i < HNS3_MAX_TC_NUM; i++) {
		buf_size = buf_alloc->priv_buf[i].tx_buf_size;

		buf_size = buf_size >> HNS3_BUF_SIZE_UNIT_SHIFT;
		req->tx_pkt_buff[i] = rte_cpu_to_le_16(buf_size |
						HNS3_BUF_SIZE_UPDATE_EN_MSK);
	}

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		PMD_INIT_LOG(ERR, "tx buffer alloc cmd failed %d", ret);

	return ret;
}

static int
hns3_get_tc_num(struct hns3_hw *hw)
{
	int cnt = 0;
	uint8_t i;

	for (i = 0; i < HNS3_MAX_TC_NUM; i++)
		if (hw->hw_tc_map & BIT(i))
			cnt++;
	return cnt;
}

static uint32_t
hns3_get_rx_priv_buff_alloced(struct hns3_pkt_buf_alloc *buf_alloc)
{
	struct hns3_priv_buf *priv;
	uint32_t rx_priv = 0;
	int i;

	for (i = 0; i < HNS3_MAX_TC_NUM; i++) {
		priv = &buf_alloc->priv_buf[i];
		if (priv->enable)
			rx_priv += priv->buf_size;
	}
	return rx_priv;
}

static uint32_t
hns3_get_tx_buff_alloced(struct hns3_pkt_buf_alloc *buf_alloc)
{
	uint32_t total_tx_size = 0;
	uint32_t i;

	for (i = 0; i < HNS3_MAX_TC_NUM; i++)
		total_tx_size += buf_alloc->priv_buf[i].tx_buf_size;

	return total_tx_size;
}

/* Get the number of pfc enabled TCs, which have private buffer */
static int
hns3_get_pfc_priv_num(struct hns3_hw *hw, struct hns3_pkt_buf_alloc *buf_alloc)
{
	struct hns3_priv_buf *priv;
	int cnt = 0;
	uint8_t i;

	for (i = 0; i < HNS3_MAX_TC_NUM; i++) {
		priv = &buf_alloc->priv_buf[i];
		if ((hw->dcb_info.hw_pfc_map & BIT(i)) && priv->enable)
			cnt++;
	}

	return cnt;
}

/* Get the number of pfc disabled TCs, which have private buffer */
static int
hns3_get_no_pfc_priv_num(struct hns3_hw *hw,
			 struct hns3_pkt_buf_alloc *buf_alloc)
{
	struct hns3_priv_buf *priv;
	int cnt = 0;
	uint8_t i;

	for (i = 0; i < HNS3_MAX_TC_NUM; i++) {
		priv = &buf_alloc->priv_buf[i];
		if (hw->hw_tc_map & BIT(i) &&
		    !(hw->dcb_info.hw_pfc_map & BIT(i)) && priv->enable)
			cnt++;
	}

	return cnt;
}

static bool
hns3_is_rx_buf_ok(struct hns3_hw *hw, struct hns3_pkt_buf_alloc *buf_alloc,
		  uint32_t rx_all)
{
	uint32_t shared_buf_min, shared_buf_tc, shared_std, hi_thrd, lo_thrd;
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	uint32_t shared_buf, aligned_mps;
	uint32_t rx_priv;
	uint8_t tc_num;
	uint8_t i;

	tc_num = hns3_get_tc_num(hw);
	aligned_mps = roundup(pf->mps, HNS3_BUF_SIZE_UNIT);

	if (hns3_dev_get_support(hw, DCB))
		shared_buf_min = HNS3_BUF_MUL_BY * aligned_mps +
					pf->dv_buf_size;
	else
		shared_buf_min = aligned_mps + HNS3_NON_DCB_ADDITIONAL_BUF
					+ pf->dv_buf_size;

	shared_buf_tc = tc_num * aligned_mps + aligned_mps;
	shared_std = roundup(RTE_MAX(shared_buf_min, shared_buf_tc),
			     HNS3_BUF_SIZE_UNIT);

	rx_priv = hns3_get_rx_priv_buff_alloced(buf_alloc);
	if (rx_all < rx_priv + shared_std)
		return false;

	shared_buf = rounddown(rx_all - rx_priv, HNS3_BUF_SIZE_UNIT);
	buf_alloc->s_buf.buf_size = shared_buf;
	if (hns3_dev_get_support(hw, DCB)) {
		buf_alloc->s_buf.self.high = shared_buf - pf->dv_buf_size;
		buf_alloc->s_buf.self.low = buf_alloc->s_buf.self.high
			- roundup(aligned_mps / HNS3_BUF_DIV_BY,
				  HNS3_BUF_SIZE_UNIT);
	} else {
		buf_alloc->s_buf.self.high =
			aligned_mps + HNS3_NON_DCB_ADDITIONAL_BUF;
		buf_alloc->s_buf.self.low = aligned_mps;
	}

	if (hns3_dev_get_support(hw, DCB)) {
		hi_thrd = shared_buf - pf->dv_buf_size;

		if (tc_num <= NEED_RESERVE_TC_NUM)
			hi_thrd = hi_thrd * BUF_RESERVE_PERCENT /
				  BUF_MAX_PERCENT;

		if (tc_num)
			hi_thrd = hi_thrd / tc_num;

		hi_thrd = RTE_MAX(hi_thrd, HNS3_BUF_MUL_BY * aligned_mps);
		hi_thrd = rounddown(hi_thrd, HNS3_BUF_SIZE_UNIT);
		lo_thrd = hi_thrd - aligned_mps / HNS3_BUF_DIV_BY;
	} else {
		hi_thrd = aligned_mps + HNS3_NON_DCB_ADDITIONAL_BUF;
		lo_thrd = aligned_mps;
	}

	for (i = 0; i < HNS3_MAX_TC_NUM; i++) {
		buf_alloc->s_buf.tc_thrd[i].low = lo_thrd;
		buf_alloc->s_buf.tc_thrd[i].high = hi_thrd;
	}

	return true;
}

static bool
hns3_rx_buf_calc_all(struct hns3_hw *hw, bool max,
		     struct hns3_pkt_buf_alloc *buf_alloc)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	struct hns3_priv_buf *priv;
	uint32_t aligned_mps;
	uint32_t rx_all;
	uint8_t i;

	rx_all = pf->pkt_buf_size - hns3_get_tx_buff_alloced(buf_alloc);
	aligned_mps = roundup(pf->mps, HNS3_BUF_SIZE_UNIT);

	for (i = 0; i < HNS3_MAX_TC_NUM; i++) {
		priv = &buf_alloc->priv_buf[i];

		priv->enable = 0;
		priv->wl.low = 0;
		priv->wl.high = 0;
		priv->buf_size = 0;

		if (!(hw->hw_tc_map & BIT(i)))
			continue;

		priv->enable = 1;
		if (hw->dcb_info.hw_pfc_map & BIT(i)) {
			priv->wl.low = max ? aligned_mps : HNS3_BUF_SIZE_UNIT;
			priv->wl.high = roundup(priv->wl.low + aligned_mps,
						HNS3_BUF_SIZE_UNIT);
		} else {
			priv->wl.low = 0;
			priv->wl.high = max ? (aligned_mps * HNS3_BUF_MUL_BY) :
					aligned_mps;
		}

		priv->buf_size = priv->wl.high + pf->dv_buf_size;
	}

	return hns3_is_rx_buf_ok(hw, buf_alloc, rx_all);
}

static bool
hns3_drop_nopfc_buf_till_fit(struct hns3_hw *hw,
			     struct hns3_pkt_buf_alloc *buf_alloc)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	struct hns3_priv_buf *priv;
	int no_pfc_priv_num;
	uint32_t rx_all;
	uint8_t mask;
	int i;

	rx_all = pf->pkt_buf_size - hns3_get_tx_buff_alloced(buf_alloc);
	no_pfc_priv_num = hns3_get_no_pfc_priv_num(hw, buf_alloc);

	/* let the last to be cleared first */
	for (i = HNS3_MAX_TC_NUM - 1; i >= 0; i--) {
		priv = &buf_alloc->priv_buf[i];
		mask = BIT((uint8_t)i);
		if (hw->hw_tc_map & mask &&
		    !(hw->dcb_info.hw_pfc_map & mask)) {
			/* Clear the no pfc TC private buffer */
			priv->wl.low = 0;
			priv->wl.high = 0;
			priv->buf_size = 0;
			priv->enable = 0;
			no_pfc_priv_num--;
		}

		if (hns3_is_rx_buf_ok(hw, buf_alloc, rx_all) ||
		    no_pfc_priv_num == 0)
			break;
	}

	return hns3_is_rx_buf_ok(hw, buf_alloc, rx_all);
}

static bool
hns3_drop_pfc_buf_till_fit(struct hns3_hw *hw,
			   struct hns3_pkt_buf_alloc *buf_alloc)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	struct hns3_priv_buf *priv;
	uint32_t rx_all;
	int pfc_priv_num;
	uint8_t mask;
	int i;

	rx_all = pf->pkt_buf_size - hns3_get_tx_buff_alloced(buf_alloc);
	pfc_priv_num = hns3_get_pfc_priv_num(hw, buf_alloc);

	/* let the last to be cleared first */
	for (i = HNS3_MAX_TC_NUM - 1; i >= 0; i--) {
		priv = &buf_alloc->priv_buf[i];
		mask = BIT((uint8_t)i);
		if (hw->hw_tc_map & mask && hw->dcb_info.hw_pfc_map & mask) {
			/* Reduce the number of pfc TC with private buffer */
			priv->wl.low = 0;
			priv->enable = 0;
			priv->wl.high = 0;
			priv->buf_size = 0;
			pfc_priv_num--;
		}
		if (hns3_is_rx_buf_ok(hw, buf_alloc, rx_all) ||
		    pfc_priv_num == 0)
			break;
	}

	return hns3_is_rx_buf_ok(hw, buf_alloc, rx_all);
}

static bool
hns3_only_alloc_priv_buff(struct hns3_hw *hw,
			  struct hns3_pkt_buf_alloc *buf_alloc)
{
#define COMPENSATE_BUFFER	0x3C00
#define COMPENSATE_HALF_MPS_NUM	5
#define PRIV_WL_GAP		0x1800
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	uint32_t tc_num = hns3_get_tc_num(hw);
	uint32_t half_mps = pf->mps >> 1;
	struct hns3_priv_buf *priv;
	uint32_t min_rx_priv;
	uint32_t rx_priv;
	uint8_t i;

	rx_priv = pf->pkt_buf_size - hns3_get_tx_buff_alloced(buf_alloc);
	if (tc_num)
		rx_priv = rx_priv / tc_num;

	if (tc_num <= NEED_RESERVE_TC_NUM)
		rx_priv = rx_priv * BUF_RESERVE_PERCENT / BUF_MAX_PERCENT;

	/*
	 * Minimum value of private buffer in rx direction (min_rx_priv) is
	 * equal to "DV + 2.5 * MPS + 15KB". Driver only allocates rx private
	 * buffer if rx_priv is greater than min_rx_priv.
	 */
	min_rx_priv = pf->dv_buf_size + COMPENSATE_BUFFER +
			COMPENSATE_HALF_MPS_NUM * half_mps;
	min_rx_priv = roundup(min_rx_priv, HNS3_BUF_SIZE_UNIT);
	rx_priv = rounddown(rx_priv, HNS3_BUF_SIZE_UNIT);
	if (rx_priv < min_rx_priv)
		return false;

	for (i = 0; i < HNS3_MAX_TC_NUM; i++) {
		priv = &buf_alloc->priv_buf[i];
		priv->enable = 0;
		priv->wl.low = 0;
		priv->wl.high = 0;
		priv->buf_size = 0;

		if (!(hw->hw_tc_map & BIT(i)))
			continue;

		priv->enable = 1;
		priv->buf_size = rx_priv;
		priv->wl.high = rx_priv - pf->dv_buf_size;
		priv->wl.low = priv->wl.high - PRIV_WL_GAP;
	}

	buf_alloc->s_buf.buf_size = 0;

	return true;
}

/*
 * hns3_rx_buffer_calc: calculate the rx private buffer size for all TCs
 * @hw: pointer to struct hns3_hw
 * @buf_alloc: pointer to buffer calculation data
 * @return: 0: calculate successful, negative: fail
 */
static int
hns3_rx_buffer_calc(struct hns3_hw *hw, struct hns3_pkt_buf_alloc *buf_alloc)
{
	/* When DCB is not supported, rx private buffer is not allocated. */
	if (!hns3_dev_get_support(hw, DCB)) {
		struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
		struct hns3_pf *pf = &hns->pf;
		uint32_t rx_all = pf->pkt_buf_size;

		rx_all -= hns3_get_tx_buff_alloced(buf_alloc);
		if (!hns3_is_rx_buf_ok(hw, buf_alloc, rx_all))
			return -ENOMEM;

		return 0;
	}

	/*
	 * Try to allocate privated packet buffer for all TCs without share
	 * buffer.
	 */
	if (hns3_only_alloc_priv_buff(hw, buf_alloc))
		return 0;

	/*
	 * Try to allocate privated packet buffer for all TCs with share
	 * buffer.
	 */
	if (hns3_rx_buf_calc_all(hw, true, buf_alloc))
		return 0;

	/*
	 * For different application scenes, the enabled port number, TC number
	 * and no_drop TC number are different. In order to obtain the better
	 * performance, software could allocate the buffer size and configure
	 * the waterline by trying to decrease the private buffer size according
	 * to the order, namely, waterline of valid tc, pfc disabled tc, pfc
	 * enabled tc.
	 */
	if (hns3_rx_buf_calc_all(hw, false, buf_alloc))
		return 0;

	if (hns3_drop_nopfc_buf_till_fit(hw, buf_alloc))
		return 0;

	if (hns3_drop_pfc_buf_till_fit(hw, buf_alloc))
		return 0;

	return -ENOMEM;
}

static int
hns3_rx_priv_buf_alloc(struct hns3_hw *hw, struct hns3_pkt_buf_alloc *buf_alloc)
{
	struct hns3_rx_priv_buff_cmd *req;
	struct hns3_cmd_desc desc;
	uint32_t buf_size;
	int ret;
	int i;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_RX_PRIV_BUFF_ALLOC, false);
	req = (struct hns3_rx_priv_buff_cmd *)desc.data;

	/* Alloc private buffer TCs */
	for (i = 0; i < HNS3_MAX_TC_NUM; i++) {
		struct hns3_priv_buf *priv = &buf_alloc->priv_buf[i];

		req->buf_num[i] =
			rte_cpu_to_le_16(priv->buf_size >> HNS3_BUF_UNIT_S);
		req->buf_num[i] |= rte_cpu_to_le_16(1 << HNS3_TC0_PRI_BUF_EN_B);
	}

	buf_size = buf_alloc->s_buf.buf_size;
	req->shared_buf = rte_cpu_to_le_16((buf_size >> HNS3_BUF_UNIT_S) |
					   (1 << HNS3_TC0_PRI_BUF_EN_B));

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		PMD_INIT_LOG(ERR, "rx private buffer alloc cmd failed %d", ret);

	return ret;
}

static int
hns3_rx_priv_wl_config(struct hns3_hw *hw, struct hns3_pkt_buf_alloc *buf_alloc)
{
#define HNS3_RX_PRIV_WL_ALLOC_DESC_NUM 2
	struct hns3_rx_priv_wl_buf *req;
	struct hns3_priv_buf *priv;
	struct hns3_cmd_desc desc[HNS3_RX_PRIV_WL_ALLOC_DESC_NUM];
	int i, j;
	int ret;

	for (i = 0; i < HNS3_RX_PRIV_WL_ALLOC_DESC_NUM; i++) {
		hns3_cmd_setup_basic_desc(&desc[i], HNS3_OPC_RX_PRIV_WL_ALLOC,
					  false);
		req = (struct hns3_rx_priv_wl_buf *)desc[i].data;

		/* The first descriptor set the NEXT bit to 1 */
		if (i == 0)
			desc[i].flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);
		else
			desc[i].flag &= ~rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);

		for (j = 0; j < HNS3_TC_NUM_ONE_DESC; j++) {
			uint32_t idx = i * HNS3_TC_NUM_ONE_DESC + j;

			priv = &buf_alloc->priv_buf[idx];
			req->tc_wl[j].high = rte_cpu_to_le_16(priv->wl.high >>
							HNS3_BUF_UNIT_S);
			req->tc_wl[j].high |=
				rte_cpu_to_le_16(BIT(HNS3_RX_PRIV_EN_B));
			req->tc_wl[j].low = rte_cpu_to_le_16(priv->wl.low >>
							HNS3_BUF_UNIT_S);
			req->tc_wl[j].low |=
				rte_cpu_to_le_16(BIT(HNS3_RX_PRIV_EN_B));
		}
	}

	/* Send 2 descriptor at one time */
	ret = hns3_cmd_send(hw, desc, HNS3_RX_PRIV_WL_ALLOC_DESC_NUM);
	if (ret)
		PMD_INIT_LOG(ERR, "rx private waterline config cmd failed %d",
			     ret);
	return ret;
}

static int
hns3_common_thrd_config(struct hns3_hw *hw,
			struct hns3_pkt_buf_alloc *buf_alloc)
{
#define HNS3_RX_COM_THRD_ALLOC_DESC_NUM 2
	struct hns3_shared_buf *s_buf = &buf_alloc->s_buf;
	struct hns3_rx_com_thrd *req;
	struct hns3_cmd_desc desc[HNS3_RX_COM_THRD_ALLOC_DESC_NUM];
	struct hns3_tc_thrd *tc;
	int tc_idx;
	int i, j;
	int ret;

	for (i = 0; i < HNS3_RX_COM_THRD_ALLOC_DESC_NUM; i++) {
		hns3_cmd_setup_basic_desc(&desc[i], HNS3_OPC_RX_COM_THRD_ALLOC,
					  false);
		req = (struct hns3_rx_com_thrd *)&desc[i].data;

		/* The first descriptor set the NEXT bit to 1 */
		if (i == 0)
			desc[i].flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);
		else
			desc[i].flag &= ~rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);

		for (j = 0; j < HNS3_TC_NUM_ONE_DESC; j++) {
			tc_idx = i * HNS3_TC_NUM_ONE_DESC + j;
			tc = &s_buf->tc_thrd[tc_idx];

			req->com_thrd[j].high =
				rte_cpu_to_le_16(tc->high >> HNS3_BUF_UNIT_S);
			req->com_thrd[j].high |=
				 rte_cpu_to_le_16(BIT(HNS3_RX_PRIV_EN_B));
			req->com_thrd[j].low =
				rte_cpu_to_le_16(tc->low >> HNS3_BUF_UNIT_S);
			req->com_thrd[j].low |=
				 rte_cpu_to_le_16(BIT(HNS3_RX_PRIV_EN_B));
		}
	}

	/* Send 2 descriptors at one time */
	ret = hns3_cmd_send(hw, desc, HNS3_RX_COM_THRD_ALLOC_DESC_NUM);
	if (ret)
		PMD_INIT_LOG(ERR, "common threshold config cmd failed %d", ret);

	return ret;
}

static int
hns3_common_wl_config(struct hns3_hw *hw, struct hns3_pkt_buf_alloc *buf_alloc)
{
	struct hns3_shared_buf *buf = &buf_alloc->s_buf;
	struct hns3_rx_com_wl *req;
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_RX_COM_WL_ALLOC, false);

	req = (struct hns3_rx_com_wl *)desc.data;
	req->com_wl.high = rte_cpu_to_le_16(buf->self.high >> HNS3_BUF_UNIT_S);
	req->com_wl.high |= rte_cpu_to_le_16(BIT(HNS3_RX_PRIV_EN_B));

	req->com_wl.low = rte_cpu_to_le_16(buf->self.low >> HNS3_BUF_UNIT_S);
	req->com_wl.low |= rte_cpu_to_le_16(BIT(HNS3_RX_PRIV_EN_B));

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		PMD_INIT_LOG(ERR, "common waterline config cmd failed %d", ret);

	return ret;
}

int
hns3_buffer_alloc(struct hns3_hw *hw)
{
	struct hns3_pkt_buf_alloc pkt_buf;
	int ret;

	memset(&pkt_buf, 0, sizeof(pkt_buf));
	ret = hns3_tx_buffer_calc(hw, &pkt_buf);
	if (ret) {
		PMD_INIT_LOG(ERR,
			     "could not calc tx buffer size for all TCs %d",
			     ret);
		return ret;
	}

	ret = hns3_tx_buffer_alloc(hw, &pkt_buf);
	if (ret) {
		PMD_INIT_LOG(ERR, "could not alloc tx buffers %d", ret);
		return ret;
	}

	ret = hns3_rx_buffer_calc(hw, &pkt_buf);
	if (ret) {
		PMD_INIT_LOG(ERR,
			     "could not calc rx priv buffer size for all TCs %d",
			     ret);
		return ret;
	}

	ret = hns3_rx_priv_buf_alloc(hw, &pkt_buf);
	if (ret) {
		PMD_INIT_LOG(ERR, "could not alloc rx priv buffer %d", ret);
		return ret;
	}

	if (hns3_dev_get_support(hw, DCB)) {
		ret = hns3_rx_priv_wl_config(hw, &pkt_buf);
		if (ret) {
			PMD_INIT_LOG(ERR,
				     "could not configure rx private waterline %d",
				     ret);
			return ret;
		}

		ret = hns3_common_thrd_config(hw, &pkt_buf);
		if (ret) {
			PMD_INIT_LOG(ERR,
				     "could not configure common threshold %d",
				     ret);
			return ret;
		}
	}

	ret = hns3_common_wl_config(hw, &pkt_buf);
	if (ret)
		PMD_INIT_LOG(ERR, "could not configure common waterline %d",
			     ret);

	return ret;
}

static int
hns3_mac_init(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_mac *mac = &hw->mac;
	struct hns3_pf *pf = &hns->pf;
	int ret;

	pf->support_sfp_query = true;
	mac->link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
	ret = hns3_cfg_mac_speed_dup_hw(hw, mac->link_speed, mac->link_duplex);
	if (ret) {
		PMD_INIT_LOG(ERR, "Config mac speed dup fail ret = %d", ret);
		return ret;
	}

	mac->link_status = RTE_ETH_LINK_DOWN;

	return hns3_config_mtu(hw, pf->mps);
}

static int
hns3_get_mac_ethertype_cmd_status(uint16_t cmdq_resp, uint8_t resp_code)
{
#define HNS3_ETHERTYPE_SUCCESS_ADD		0
#define HNS3_ETHERTYPE_ALREADY_ADD		1
#define HNS3_ETHERTYPE_MGR_TBL_OVERFLOW		2
#define HNS3_ETHERTYPE_KEY_CONFLICT		3
	int return_status;

	if (cmdq_resp) {
		PMD_INIT_LOG(ERR,
			     "cmdq execute failed for get_mac_ethertype_cmd_status, status=%u.",
			     cmdq_resp);
		return -EIO;
	}

	switch (resp_code) {
	case HNS3_ETHERTYPE_SUCCESS_ADD:
	case HNS3_ETHERTYPE_ALREADY_ADD:
		return_status = 0;
		break;
	case HNS3_ETHERTYPE_MGR_TBL_OVERFLOW:
		PMD_INIT_LOG(ERR,
			     "add mac ethertype failed for manager table overflow.");
		return_status = -EIO;
		break;
	case HNS3_ETHERTYPE_KEY_CONFLICT:
		PMD_INIT_LOG(ERR, "add mac ethertype failed for key conflict.");
		return_status = -EIO;
		break;
	default:
		PMD_INIT_LOG(ERR,
			     "add mac ethertype failed for undefined, code=%u.",
			     resp_code);
		return_status = -EIO;
		break;
	}

	return return_status;
}

static int
hns3_add_mgr_tbl(struct hns3_hw *hw,
		 const struct hns3_mac_mgr_tbl_entry_cmd *req)
{
	struct hns3_cmd_desc desc;
	uint8_t resp_code;
	uint16_t retval;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_MAC_ETHTYPE_ADD, false);
	memcpy(desc.data, req, sizeof(struct hns3_mac_mgr_tbl_entry_cmd));

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		PMD_INIT_LOG(ERR,
			     "add mac ethertype failed for cmd_send, ret =%d.",
			     ret);
		return ret;
	}

	resp_code = (rte_le_to_cpu_32(desc.data[0]) >> 8) & 0xff;
	retval = rte_le_to_cpu_16(desc.retval);

	return hns3_get_mac_ethertype_cmd_status(retval, resp_code);
}

static void
hns3_prepare_mgr_tbl(struct hns3_mac_mgr_tbl_entry_cmd *mgr_table,
		     int *table_item_num)
{
	struct hns3_mac_mgr_tbl_entry_cmd *tbl;

	/*
	 * In current version, we add one item in management table as below:
	 * 0x0180C200000E -- LLDP MC address
	 */
	tbl = mgr_table;
	tbl->flags = HNS3_MAC_MGR_MASK_VLAN_B;
	tbl->ethter_type = rte_cpu_to_le_16(HNS3_MAC_ETHERTYPE_LLDP);
	tbl->mac_addr_hi32 = rte_cpu_to_le_32(htonl(0x0180C200));
	tbl->mac_addr_lo16 = rte_cpu_to_le_16(htons(0x000E));
	tbl->i_port_bitmap = 0x1;
	*table_item_num = 1;
}

static int
hns3_init_mgr_tbl(struct hns3_hw *hw)
{
#define HNS_MAC_MGR_TBL_MAX_SIZE	16
	struct hns3_mac_mgr_tbl_entry_cmd mgr_table[HNS_MAC_MGR_TBL_MAX_SIZE];
	int table_item_num;
	int ret;
	int i;

	memset(mgr_table, 0, sizeof(mgr_table));
	hns3_prepare_mgr_tbl(mgr_table, &table_item_num);
	for (i = 0; i < table_item_num; i++) {
		ret = hns3_add_mgr_tbl(hw, &mgr_table[i]);
		if (ret) {
			PMD_INIT_LOG(ERR, "add mac ethertype failed, ret =%d",
				     ret);
			return ret;
		}
	}

	return 0;
}

static void
hns3_promisc_param_init(struct hns3_promisc_param *param, bool en_uc,
			bool en_mc, bool en_bc, int vport_id)
{
	if (!param)
		return;

	memset(param, 0, sizeof(struct hns3_promisc_param));
	if (en_uc)
		param->enable = HNS3_PROMISC_EN_UC;
	if (en_mc)
		param->enable |= HNS3_PROMISC_EN_MC;
	if (en_bc)
		param->enable |= HNS3_PROMISC_EN_BC;
	param->vf_id = vport_id;
}

static int
hns3_cmd_set_promisc_mode(struct hns3_hw *hw, struct hns3_promisc_param *param)
{
	struct hns3_promisc_cfg_cmd *req;
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_CFG_PROMISC_MODE, false);

	req = (struct hns3_promisc_cfg_cmd *)desc.data;
	req->vf_id = param->vf_id;
	req->flag = (param->enable << HNS3_PROMISC_EN_B) |
	    HNS3_PROMISC_TX_EN_B | HNS3_PROMISC_RX_EN_B;

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		PMD_INIT_LOG(ERR, "Set promisc mode fail, ret = %d", ret);

	return ret;
}

static int
hns3_set_promisc_mode(struct hns3_hw *hw, bool en_uc_pmc, bool en_mc_pmc)
{
	struct hns3_promisc_param param;
	bool en_bc_pmc = true;
	uint8_t vf_id;

	/*
	 * In current version VF is not supported when PF is driven by DPDK
	 * driver, just need to configure parameters for PF vport.
	 */
	vf_id = HNS3_PF_FUNC_ID;

	hns3_promisc_param_init(&param, en_uc_pmc, en_mc_pmc, en_bc_pmc, vf_id);
	return hns3_cmd_set_promisc_mode(hw, &param);
}

static int
hns3_promisc_init(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	struct hns3_promisc_param param;
	uint16_t func_id;
	int ret;

	ret = hns3_set_promisc_mode(hw, false, false);
	if (ret) {
		PMD_INIT_LOG(ERR, "failed to set promisc mode, ret = %d", ret);
		return ret;
	}

	/*
	 * In current version VFs are not supported when PF is driven by DPDK
	 * driver. After PF has been taken over by DPDK, the original VF will
	 * be invalid. So, there is a possibility of entry residues. It should
	 * clear VFs's promisc mode to avoid unnecessary bandwidth usage
	 * during init.
	 */
	for (func_id = HNS3_1ST_VF_FUNC_ID; func_id < pf->func_num; func_id++) {
		hns3_promisc_param_init(&param, false, false, false, func_id);
		ret = hns3_cmd_set_promisc_mode(hw, &param);
		if (ret) {
			PMD_INIT_LOG(ERR, "failed to clear vf:%u promisc mode,"
					" ret = %d", func_id, ret);
			return ret;
		}
	}

	return 0;
}

static void
hns3_promisc_uninit(struct hns3_hw *hw)
{
	struct hns3_promisc_param param;
	uint16_t func_id;
	int ret;

	func_id = HNS3_PF_FUNC_ID;

	/*
	 * In current version VFs are not supported when PF is driven by
	 * DPDK driver, and VFs' promisc mode status has been cleared during
	 * init and their status will not change. So just clear PF's promisc
	 * mode status during uninit.
	 */
	hns3_promisc_param_init(&param, false, false, false, func_id);
	ret = hns3_cmd_set_promisc_mode(hw, &param);
	if (ret)
		PMD_INIT_LOG(ERR, "failed to clear promisc status during"
				" uninit, ret = %d", ret);
}

static int
hns3_dev_promiscuous_enable(struct rte_eth_dev *dev)
{
	bool allmulti = dev->data->all_multicast ? true : false;
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	uint64_t offloads;
	int err;
	int ret;

	rte_spinlock_lock(&hw->lock);
	ret = hns3_set_promisc_mode(hw, true, true);
	if (ret) {
		rte_spinlock_unlock(&hw->lock);
		hns3_err(hw, "failed to enable promiscuous mode, ret = %d",
			 ret);
		return ret;
	}

	/*
	 * When promiscuous mode was enabled, disable the vlan filter to let
	 * all packets coming in the receiving direction.
	 */
	offloads = dev->data->dev_conf.rxmode.offloads;
	if (offloads & RTE_ETH_RX_OFFLOAD_VLAN_FILTER) {
		ret = hns3_enable_vlan_filter(hns, false);
		if (ret) {
			hns3_err(hw, "failed to enable promiscuous mode due to "
				 "failure to disable vlan filter, ret = %d",
				 ret);
			err = hns3_set_promisc_mode(hw, false, allmulti);
			if (err)
				hns3_err(hw, "failed to restore promiscuous "
					 "status after disable vlan filter "
					 "failed during enabling promiscuous "
					 "mode, ret = %d", ret);
		}
	}

	rte_spinlock_unlock(&hw->lock);

	return ret;
}

static int
hns3_dev_promiscuous_disable(struct rte_eth_dev *dev)
{
	bool allmulti = dev->data->all_multicast ? true : false;
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	uint64_t offloads;
	int err;
	int ret;

	/* If now in all_multicast mode, must remain in all_multicast mode. */
	rte_spinlock_lock(&hw->lock);
	ret = hns3_set_promisc_mode(hw, false, allmulti);
	if (ret) {
		rte_spinlock_unlock(&hw->lock);
		hns3_err(hw, "failed to disable promiscuous mode, ret = %d",
			 ret);
		return ret;
	}
	/* when promiscuous mode was disabled, restore the vlan filter status */
	offloads = dev->data->dev_conf.rxmode.offloads;
	if (offloads & RTE_ETH_RX_OFFLOAD_VLAN_FILTER) {
		ret = hns3_enable_vlan_filter(hns, true);
		if (ret) {
			hns3_err(hw, "failed to disable promiscuous mode due to"
				 " failure to restore vlan filter, ret = %d",
				 ret);
			err = hns3_set_promisc_mode(hw, true, true);
			if (err)
				hns3_err(hw, "failed to restore promiscuous "
					 "status after enabling vlan filter "
					 "failed during disabling promiscuous "
					 "mode, ret = %d", ret);
		}
	}
	rte_spinlock_unlock(&hw->lock);

	return ret;
}

static int
hns3_dev_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	if (dev->data->promiscuous)
		return 0;

	rte_spinlock_lock(&hw->lock);
	ret = hns3_set_promisc_mode(hw, false, true);
	rte_spinlock_unlock(&hw->lock);
	if (ret)
		hns3_err(hw, "failed to enable allmulticast mode, ret = %d",
			 ret);

	return ret;
}

static int
hns3_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	/* If now in promiscuous mode, must remain in all_multicast mode. */
	if (dev->data->promiscuous)
		return 0;

	rte_spinlock_lock(&hw->lock);
	ret = hns3_set_promisc_mode(hw, false, false);
	rte_spinlock_unlock(&hw->lock);
	if (ret)
		hns3_err(hw, "failed to disable allmulticast mode, ret = %d",
			 ret);

	return ret;
}

static int
hns3_dev_promisc_restore(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	bool allmulti = hw->data->all_multicast ? true : false;
	int ret;

	if (hw->data->promiscuous) {
		ret = hns3_set_promisc_mode(hw, true, true);
		if (ret)
			hns3_err(hw, "failed to restore promiscuous mode, "
				 "ret = %d", ret);
		return ret;
	}

	ret = hns3_set_promisc_mode(hw, false, allmulti);
	if (ret)
		hns3_err(hw, "failed to restore allmulticast mode, ret = %d",
			 ret);
	return ret;
}

static int
hns3_get_sfp_info(struct hns3_hw *hw, struct hns3_mac *mac_info)
{
	struct hns3_sfp_info_cmd *resp;
	uint32_t local_pause, lp_pause;
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_GET_SFP_INFO, true);
	resp = (struct hns3_sfp_info_cmd *)desc.data;
	resp->query_type = HNS3_ACTIVE_QUERY;

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret == -EOPNOTSUPP) {
		hns3_warn(hw, "firmware does not support get SFP info,"
			  " ret = %d.", ret);
		return ret;
	} else if (ret) {
		hns3_err(hw, "get sfp info failed, ret = %d.", ret);
		return ret;
	}

	/*
	 * In some case, the speed of MAC obtained from firmware may be 0, it
	 * shouldn't be set to mac->speed.
	 */
	if (!rte_le_to_cpu_32(resp->sfp_speed))
		return 0;

	mac_info->link_speed = rte_le_to_cpu_32(resp->sfp_speed);
	/*
	 * if resp->supported_speed is 0, it means it's an old version
	 * firmware, do not update these params.
	 */
	if (resp->supported_speed) {
		mac_info->query_type = HNS3_ACTIVE_QUERY;
		mac_info->supported_speed =
					rte_le_to_cpu_32(resp->supported_speed);
		mac_info->support_autoneg = resp->autoneg_ability;
		mac_info->link_autoneg = (resp->autoneg == 0) ? RTE_ETH_LINK_FIXED
					: RTE_ETH_LINK_AUTONEG;
		mac_info->fec_capa = resp->fec_ability;
		local_pause = resp->pause_status & HNS3_FIBER_LOCAL_PAUSE_MASK;
		lp_pause = (resp->pause_status & HNS3_FIBER_LP_PAUSE_MASK) >>
						HNS3_FIBER_LP_PAUSE_S;
		mac_info->advertising =
				local_pause << HNS3_PHY_LINK_MODE_PAUSE_S;
		mac_info->lp_advertising =
				lp_pause << HNS3_PHY_LINK_MODE_PAUSE_S;
	} else {
		mac_info->query_type = HNS3_DEFAULT_QUERY;
	}

	return 0;
}

static uint8_t
hns3_check_speed_dup(uint8_t duplex, uint32_t speed)
{
	if (!(speed == RTE_ETH_SPEED_NUM_10M || speed == RTE_ETH_SPEED_NUM_100M))
		duplex = RTE_ETH_LINK_FULL_DUPLEX;

	return duplex;
}

static int
hns3_cfg_mac_speed_dup(struct hns3_hw *hw, uint32_t speed, uint8_t duplex)
{
	struct hns3_mac *mac = &hw->mac;
	int ret;

	duplex = hns3_check_speed_dup(duplex, speed);
	if (mac->link_speed == speed && mac->link_duplex == duplex)
		return 0;

	ret = hns3_cfg_mac_speed_dup_hw(hw, speed, duplex);
	if (ret)
		return ret;

	ret = hns3_port_shaper_update(hw, speed);
	if (ret)
		return ret;

	mac->link_speed = speed;
	mac->link_duplex = duplex;

	return 0;
}

static int
hns3_update_fiber_link_info(struct hns3_hw *hw)
{
	struct hns3_pf *pf = HNS3_DEV_HW_TO_PF(hw);
	struct hns3_mac *mac = &hw->mac;
	struct hns3_mac mac_info;
	int ret;

	/* If firmware do not support get SFP/qSFP speed, return directly */
	if (!pf->support_sfp_query)
		return 0;

	memset(&mac_info, 0, sizeof(struct hns3_mac));
	ret = hns3_get_sfp_info(hw, &mac_info);
	if (ret == -EOPNOTSUPP) {
		pf->support_sfp_query = false;
		return ret;
	} else if (ret)
		return ret;

	/* Do nothing if no SFP */
	if (mac_info.link_speed == RTE_ETH_SPEED_NUM_NONE)
		return 0;

	/*
	 * If query_type is HNS3_ACTIVE_QUERY, it is no need
	 * to reconfigure the speed of MAC. Otherwise, it indicates
	 * that the current firmware only supports to obtain the
	 * speed of the SFP, and the speed of MAC needs to reconfigure.
	 */
	mac->query_type = mac_info.query_type;
	if (mac->query_type == HNS3_ACTIVE_QUERY) {
		if (mac_info.link_speed != mac->link_speed) {
			ret = hns3_port_shaper_update(hw, mac_info.link_speed);
			if (ret)
				return ret;
		}

		mac->link_speed = mac_info.link_speed;
		mac->supported_speed = mac_info.supported_speed;
		mac->support_autoneg = mac_info.support_autoneg;
		mac->link_autoneg = mac_info.link_autoneg;
		mac->fec_capa = mac_info.fec_capa;
		mac->advertising = mac_info.advertising;
		mac->lp_advertising = mac_info.lp_advertising;

		return 0;
	}

	/* Config full duplex for SFP */
	return hns3_cfg_mac_speed_dup(hw, mac_info.link_speed,
				      RTE_ETH_LINK_FULL_DUPLEX);
}

static void
hns3_parse_copper_phy_params(struct hns3_cmd_desc *desc, struct hns3_mac *mac)
{
#define HNS3_PHY_SUPPORTED_SPEED_MASK   0x2f

	struct hns3_phy_params_bd0_cmd *req;
	uint32_t supported;

	req = (struct hns3_phy_params_bd0_cmd *)desc[0].data;
	mac->link_speed = rte_le_to_cpu_32(req->speed);
	mac->link_duplex = hns3_get_bit(req->duplex,
					   HNS3_PHY_DUPLEX_CFG_B);
	mac->link_autoneg = hns3_get_bit(req->autoneg,
					   HNS3_PHY_AUTONEG_CFG_B);
	mac->advertising = rte_le_to_cpu_32(req->advertising);
	mac->lp_advertising = rte_le_to_cpu_32(req->lp_advertising);
	supported = rte_le_to_cpu_32(req->supported);
	mac->supported_speed = supported & HNS3_PHY_SUPPORTED_SPEED_MASK;
	mac->support_autoneg = !!(supported & HNS3_PHY_LINK_MODE_AUTONEG_BIT);
}

static int
hns3_get_copper_phy_params(struct hns3_hw *hw, struct hns3_mac *mac)
{
	struct hns3_cmd_desc desc[HNS3_PHY_PARAM_CFG_BD_NUM];
	uint16_t i;
	int ret;

	for (i = 0; i < HNS3_PHY_PARAM_CFG_BD_NUM - 1; i++) {
		hns3_cmd_setup_basic_desc(&desc[i], HNS3_OPC_PHY_PARAM_CFG,
					  true);
		desc[i].flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);
	}
	hns3_cmd_setup_basic_desc(&desc[i], HNS3_OPC_PHY_PARAM_CFG, true);

	ret = hns3_cmd_send(hw, desc, HNS3_PHY_PARAM_CFG_BD_NUM);
	if (ret) {
		hns3_err(hw, "get phy parameters failed, ret = %d.", ret);
		return ret;
	}

	hns3_parse_copper_phy_params(desc, mac);

	return 0;
}

static int
hns3_update_copper_link_info(struct hns3_hw *hw)
{
	struct hns3_mac *mac = &hw->mac;
	struct hns3_mac mac_info;
	int ret;

	memset(&mac_info, 0, sizeof(struct hns3_mac));
	ret = hns3_get_copper_phy_params(hw, &mac_info);
	if (ret)
		return ret;

	if (mac_info.link_speed != mac->link_speed) {
		ret = hns3_port_shaper_update(hw, mac_info.link_speed);
		if (ret)
			return ret;
	}

	mac->link_speed = mac_info.link_speed;
	mac->link_duplex = mac_info.link_duplex;
	mac->link_autoneg = mac_info.link_autoneg;
	mac->supported_speed = mac_info.supported_speed;
	mac->advertising = mac_info.advertising;
	mac->lp_advertising = mac_info.lp_advertising;
	mac->support_autoneg = mac_info.support_autoneg;

	return 0;
}

static int
hns3_update_link_info(struct rte_eth_dev *eth_dev)
{
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;

	if (hw->mac.media_type == HNS3_MEDIA_TYPE_COPPER)
		return hns3_update_copper_link_info(hw);

	return hns3_update_fiber_link_info(hw);
}

static int
hns3_cfg_mac_mode(struct hns3_hw *hw, bool enable)
{
	struct hns3_config_mac_mode_cmd *req;
	struct hns3_cmd_desc desc;
	uint32_t loop_en = 0;
	uint8_t val = 0;
	int ret;

	req = (struct hns3_config_mac_mode_cmd *)desc.data;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_CONFIG_MAC_MODE, false);
	if (enable)
		val = 1;
	hns3_set_bit(loop_en, HNS3_MAC_TX_EN_B, val);
	hns3_set_bit(loop_en, HNS3_MAC_RX_EN_B, val);
	hns3_set_bit(loop_en, HNS3_MAC_PAD_TX_B, val);
	hns3_set_bit(loop_en, HNS3_MAC_PAD_RX_B, val);
	hns3_set_bit(loop_en, HNS3_MAC_1588_TX_B, 0);
	hns3_set_bit(loop_en, HNS3_MAC_1588_RX_B, 0);
	hns3_set_bit(loop_en, HNS3_MAC_APP_LP_B, 0);
	hns3_set_bit(loop_en, HNS3_MAC_LINE_LP_B, 0);
	hns3_set_bit(loop_en, HNS3_MAC_FCS_TX_B, val);
	hns3_set_bit(loop_en, HNS3_MAC_RX_FCS_B, val);

	/*
	 * If RTE_ETH_RX_OFFLOAD_KEEP_CRC offload is set, MAC will not strip CRC
	 * when receiving frames. Otherwise, CRC will be stripped.
	 */
	if (hw->data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC)
		hns3_set_bit(loop_en, HNS3_MAC_RX_FCS_STRIP_B, 0);
	else
		hns3_set_bit(loop_en, HNS3_MAC_RX_FCS_STRIP_B, val);
	hns3_set_bit(loop_en, HNS3_MAC_TX_OVERSIZE_TRUNCATE_B, val);
	hns3_set_bit(loop_en, HNS3_MAC_RX_OVERSIZE_TRUNCATE_B, val);
	hns3_set_bit(loop_en, HNS3_MAC_TX_UNDER_MIN_ERR_B, val);
	req->txrx_pad_fcs_loop_en = rte_cpu_to_le_32(loop_en);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		PMD_INIT_LOG(ERR, "mac enable fail, ret =%d.", ret);

	return ret;
}

static int
hns3_get_mac_link_status(struct hns3_hw *hw)
{
	struct hns3_link_status_cmd *req;
	struct hns3_cmd_desc desc;
	int link_status;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_QUERY_LINK_STATUS, true);
	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		hns3_err(hw, "get link status cmd failed %d", ret);
		return RTE_ETH_LINK_DOWN;
	}

	req = (struct hns3_link_status_cmd *)desc.data;
	link_status = req->status & HNS3_LINK_STATUS_UP_M;

	return !!link_status;
}

static bool
hns3_update_link_status(struct hns3_hw *hw)
{
	int state;

	state = hns3_get_mac_link_status(hw);
	if (state != hw->mac.link_status) {
		hw->mac.link_status = state;
		hns3_warn(hw, "Link status change to %s!", state ? "up" : "down");
		return true;
	}

	return false;
}

void
hns3_update_linkstatus_and_event(struct hns3_hw *hw, bool query)
{
	struct rte_eth_dev *dev = &rte_eth_devices[hw->data->port_id];
	struct rte_eth_link new_link;
	int ret;

	if (query)
		hns3_update_port_link_info(dev);

	memset(&new_link, 0, sizeof(new_link));
	hns3_setup_linkstatus(dev, &new_link);

	ret = rte_eth_linkstatus_set(dev, &new_link);
	if (ret == 0 && dev->data->dev_conf.intr_conf.lsc != 0)
		hns3_start_report_lse(dev);
}

static void
hns3_service_handler(void *param)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)param;
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;

	if (!hns3_is_reset_pending(hns)) {
		hns3_update_linkstatus_and_event(hw, true);
		hns3_update_hw_stats(hw);
	} else {
		hns3_warn(hw, "Cancel the query when reset is pending");
	}

	rte_eal_alarm_set(HNS3_SERVICE_INTERVAL, hns3_service_handler, eth_dev);
}

static int
hns3_init_hardware(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	int ret;

	/*
	 * All queue-related HW operations must be performed after the TCAM
	 * table is configured.
	 */
	ret = hns3_map_tqp(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to map tqp: %d", ret);
		return ret;
	}

	ret = hns3_init_umv_space(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init umv space: %d", ret);
		return ret;
	}

	ret = hns3_mac_init(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init MAC: %d", ret);
		goto err_mac_init;
	}

	ret = hns3_init_mgr_tbl(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init manager table: %d", ret);
		goto err_mac_init;
	}

	ret = hns3_promisc_init(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init promisc: %d",
			     ret);
		goto err_mac_init;
	}

	ret = hns3_init_vlan_config(hns);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init vlan: %d", ret);
		goto err_mac_init;
	}

	ret = hns3_dcb_init(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init dcb: %d", ret);
		goto err_mac_init;
	}

	ret = hns3_init_fd_config(hns);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init flow director: %d", ret);
		goto err_mac_init;
	}

	ret = hns3_config_tso(hw, HNS3_TSO_MSS_MIN, HNS3_TSO_MSS_MAX);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to config tso: %d", ret);
		goto err_mac_init;
	}

	ret = hns3_config_gro(hw, false);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to config gro: %d", ret);
		goto err_mac_init;
	}

	/*
	 * In the initialization clearing the all hardware mapping relationship
	 * configurations between queues and interrupt vectors is needed, so
	 * some error caused by the residual configurations, such as the
	 * unexpected interrupt, can be avoid.
	 */
	ret = hns3_init_ring_with_vector(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init ring intr vector: %d", ret);
		goto err_mac_init;
	}

	ret = hns3_ptp_init(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init PTP, ret = %d", ret);
		goto err_mac_init;
	}

	return 0;

err_mac_init:
	hns3_uninit_umv_space(hw);
	return ret;
}

static int
hns3_clear_hw(struct hns3_hw *hw)
{
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_CLEAR_HW_STATE, false);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret && ret != -EOPNOTSUPP)
		return ret;

	return 0;
}

static void
hns3_config_all_msix_error(struct hns3_hw *hw, bool enable)
{
	uint32_t val;

	/*
	 * The new firmware support report more hardware error types by
	 * msix mode. These errors are defined as RAS errors in hardware
	 * and belong to a different type from the MSI-x errors processed
	 * by the network driver.
	 *
	 * Network driver should open the new error report on initialization.
	 */
	val = hns3_read_dev(hw, HNS3_VECTOR0_OTER_EN_REG);
	hns3_set_bit(val, HNS3_VECTOR0_ALL_MSIX_ERR_B, enable ? 1 : 0);
	hns3_write_dev(hw, HNS3_VECTOR0_OTER_EN_REG, val);
}

static uint32_t
hns3_set_firber_default_support_speed(struct hns3_hw *hw)
{
	struct hns3_mac *mac = &hw->mac;

	switch (mac->link_speed) {
	case RTE_ETH_SPEED_NUM_1G:
		return HNS3_FIBER_LINK_SPEED_1G_BIT;
	case RTE_ETH_SPEED_NUM_10G:
		return HNS3_FIBER_LINK_SPEED_10G_BIT;
	case RTE_ETH_SPEED_NUM_25G:
		return HNS3_FIBER_LINK_SPEED_25G_BIT;
	case RTE_ETH_SPEED_NUM_40G:
		return HNS3_FIBER_LINK_SPEED_40G_BIT;
	case RTE_ETH_SPEED_NUM_50G:
		return HNS3_FIBER_LINK_SPEED_50G_BIT;
	case RTE_ETH_SPEED_NUM_100G:
		return HNS3_FIBER_LINK_SPEED_100G_BIT;
	case RTE_ETH_SPEED_NUM_200G:
		return HNS3_FIBER_LINK_SPEED_200G_BIT;
	default:
		hns3_warn(hw, "invalid speed %u Mbps.", mac->link_speed);
		return 0;
	}
}

/*
 * Validity of supported_speed for fiber and copper media type can be
 * guaranteed by the following policy:
 * Copper:
 *       Although the initialization of the phy in the firmware may not be
 *       completed, the firmware can guarantees that the supported_speed is
 *       an valid value.
 * Firber:
 *       If the version of firmware supports the active query way of the
 *       HNS3_OPC_GET_SFP_INFO opcode, the supported_speed can be obtained
 *       through it. If unsupported, use the SFP's speed as the value of the
 *       supported_speed.
 */
static int
hns3_get_port_supported_speed(struct rte_eth_dev *eth_dev)
{
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_mac *mac = &hw->mac;
	int ret;

	ret = hns3_update_link_info(eth_dev);
	if (ret)
		return ret;

	if (mac->media_type == HNS3_MEDIA_TYPE_FIBER ||
	    mac->media_type == HNS3_MEDIA_TYPE_BACKPLANE) {
		/*
		 * Some firmware does not support the report of supported_speed,
		 * and only report the effective speed of SFP/backplane. In this
		 * case, it is necessary to use the SFP/backplane's speed as the
		 * supported_speed.
		 */
		if (mac->supported_speed == 0)
			mac->supported_speed =
				hns3_set_firber_default_support_speed(hw);
	}

	return 0;
}

static int
hns3_init_pf(struct rte_eth_dev *eth_dev)
{
	struct rte_device *dev = eth_dev->device;
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev);
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	PMD_INIT_FUNC_TRACE();

	/* Get hardware io base address from pcie BAR2 IO space */
	hw->io_base = pci_dev->mem_resource[2].addr;

	/* Firmware command queue initialize */
	ret = hns3_cmd_init_queue(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init cmd queue: %d", ret);
		goto err_cmd_init_queue;
	}

	hns3_clear_all_event_cause(hw);

	/* Firmware command initialize */
	ret = hns3_cmd_init(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init cmd: %d", ret);
		goto err_cmd_init;
	}

	hns3_tx_push_init(eth_dev);

	/*
	 * To ensure that the hardware environment is clean during
	 * initialization, the driver actively clear the hardware environment
	 * during initialization, including PF and corresponding VFs' vlan, mac,
	 * flow table configurations, etc.
	 */
	ret = hns3_clear_hw(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "failed to clear hardware: %d", ret);
		goto err_cmd_init;
	}

	hns3_config_all_msix_error(hw, true);

	ret = rte_intr_callback_register(pci_dev->intr_handle,
					 hns3_interrupt_handler,
					 eth_dev);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to register intr: %d", ret);
		goto err_intr_callback_register;
	}

	/* Enable interrupt */
	rte_intr_enable(pci_dev->intr_handle);
	hns3_pf_enable_irq0(hw);

	/* Get configuration */
	ret = hns3_get_configuration(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to fetch configuration: %d", ret);
		goto err_get_config;
	}

	ret = hns3_stats_init(hw);
	if (ret)
		goto err_get_config;

	ret = hns3_init_hardware(hns);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init hardware: %d", ret);
		goto err_init_hw;
	}

	/* Initialize flow director filter list & hash */
	ret = hns3_fdir_filter_init(hns);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to alloc hashmap for fdir: %d", ret);
		goto err_fdir;
	}

	hns3_rss_set_default_args(hw);

	ret = hns3_enable_hw_error_intr(hns, true);
	if (ret) {
		PMD_INIT_LOG(ERR, "fail to enable hw error interrupts: %d",
			     ret);
		goto err_enable_intr;
	}

	ret = hns3_get_port_supported_speed(eth_dev);
	if (ret) {
		PMD_INIT_LOG(ERR, "failed to get speed capabilities supported "
			     "by device, ret = %d.", ret);
		goto err_supported_speed;
	}

	hns3_tm_conf_init(eth_dev);

	return 0;

err_supported_speed:
	(void)hns3_enable_hw_error_intr(hns, false);
err_enable_intr:
	hns3_fdir_filter_uninit(hns);
err_fdir:
	hns3_uninit_umv_space(hw);
	hns3_ptp_uninit(hw);
err_init_hw:
	hns3_stats_uninit(hw);
err_get_config:
	hns3_pf_disable_irq0(hw);
	rte_intr_disable(pci_dev->intr_handle);
	hns3_intr_unregister(pci_dev->intr_handle, hns3_interrupt_handler,
			     eth_dev);
err_intr_callback_register:
err_cmd_init:
	hns3_cmd_uninit(hw);
	hns3_cmd_destroy_queue(hw);
err_cmd_init_queue:
	hw->io_base = NULL;

	return ret;
}

static void
hns3_uninit_pf(struct rte_eth_dev *eth_dev)
{
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct rte_device *dev = eth_dev->device;
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev);
	struct hns3_hw *hw = &hns->hw;

	PMD_INIT_FUNC_TRACE();

	hns3_tm_conf_uninit(eth_dev);
	hns3_enable_hw_error_intr(hns, false);
	hns3_rss_uninit(hns);
	(void)hns3_config_gro(hw, false);
	hns3_promisc_uninit(hw);
	hns3_flow_uninit(eth_dev);
	hns3_fdir_filter_uninit(hns);
	hns3_uninit_umv_space(hw);
	hns3_ptp_uninit(hw);
	hns3_stats_uninit(hw);
	hns3_config_mac_tnl_int(hw, false);
	hns3_pf_disable_irq0(hw);
	rte_intr_disable(pci_dev->intr_handle);
	hns3_intr_unregister(pci_dev->intr_handle, hns3_interrupt_handler,
			     eth_dev);
	hns3_config_all_msix_error(hw, false);
	hns3_cmd_uninit(hw);
	hns3_cmd_destroy_queue(hw);
	hw->io_base = NULL;
}

static uint32_t
hns3_convert_link_speeds2bitmap_copper(uint32_t link_speeds)
{
	uint32_t speed_bit;

	switch (link_speeds & ~RTE_ETH_LINK_SPEED_FIXED) {
	case RTE_ETH_LINK_SPEED_10M:
		speed_bit = HNS3_PHY_LINK_SPEED_10M_BIT;
		break;
	case RTE_ETH_LINK_SPEED_10M_HD:
		speed_bit = HNS3_PHY_LINK_SPEED_10M_HD_BIT;
		break;
	case RTE_ETH_LINK_SPEED_100M:
		speed_bit = HNS3_PHY_LINK_SPEED_100M_BIT;
		break;
	case RTE_ETH_LINK_SPEED_100M_HD:
		speed_bit = HNS3_PHY_LINK_SPEED_100M_HD_BIT;
		break;
	case RTE_ETH_LINK_SPEED_1G:
		speed_bit = HNS3_PHY_LINK_SPEED_1000M_BIT;
		break;
	default:
		speed_bit = 0;
		break;
	}

	return speed_bit;
}

static uint32_t
hns3_convert_link_speeds2bitmap_fiber(uint32_t link_speeds)
{
	uint32_t speed_bit;

	switch (link_speeds & ~RTE_ETH_LINK_SPEED_FIXED) {
	case RTE_ETH_LINK_SPEED_1G:
		speed_bit = HNS3_FIBER_LINK_SPEED_1G_BIT;
		break;
	case RTE_ETH_LINK_SPEED_10G:
		speed_bit = HNS3_FIBER_LINK_SPEED_10G_BIT;
		break;
	case RTE_ETH_LINK_SPEED_25G:
		speed_bit = HNS3_FIBER_LINK_SPEED_25G_BIT;
		break;
	case RTE_ETH_LINK_SPEED_40G:
		speed_bit = HNS3_FIBER_LINK_SPEED_40G_BIT;
		break;
	case RTE_ETH_LINK_SPEED_50G:
		speed_bit = HNS3_FIBER_LINK_SPEED_50G_BIT;
		break;
	case RTE_ETH_LINK_SPEED_100G:
		speed_bit = HNS3_FIBER_LINK_SPEED_100G_BIT;
		break;
	case RTE_ETH_LINK_SPEED_200G:
		speed_bit = HNS3_FIBER_LINK_SPEED_200G_BIT;
		break;
	default:
		speed_bit = 0;
		break;
	}

	return speed_bit;
}

static int
hns3_check_port_speed(struct hns3_hw *hw, uint32_t link_speeds)
{
	struct hns3_mac *mac = &hw->mac;
	uint32_t supported_speed = mac->supported_speed;
	uint32_t speed_bit = 0;

	if (mac->media_type == HNS3_MEDIA_TYPE_COPPER)
		speed_bit = hns3_convert_link_speeds2bitmap_copper(link_speeds);
	else
		speed_bit = hns3_convert_link_speeds2bitmap_fiber(link_speeds);

	if (!(speed_bit & supported_speed)) {
		hns3_err(hw, "link_speeds(0x%x) exceeds the supported speed capability or is incorrect.",
			 link_speeds);
		return -EINVAL;
	}

	return 0;
}

static uint32_t
hns3_get_link_speed(uint32_t link_speeds)
{
	uint32_t speed = RTE_ETH_SPEED_NUM_NONE;

	if (link_speeds & RTE_ETH_LINK_SPEED_10M ||
	    link_speeds & RTE_ETH_LINK_SPEED_10M_HD)
		speed = RTE_ETH_SPEED_NUM_10M;
	if (link_speeds & RTE_ETH_LINK_SPEED_100M ||
	    link_speeds & RTE_ETH_LINK_SPEED_100M_HD)
		speed = RTE_ETH_SPEED_NUM_100M;
	if (link_speeds & RTE_ETH_LINK_SPEED_1G)
		speed = RTE_ETH_SPEED_NUM_1G;
	if (link_speeds & RTE_ETH_LINK_SPEED_10G)
		speed = RTE_ETH_SPEED_NUM_10G;
	if (link_speeds & RTE_ETH_LINK_SPEED_25G)
		speed = RTE_ETH_SPEED_NUM_25G;
	if (link_speeds & RTE_ETH_LINK_SPEED_40G)
		speed = RTE_ETH_SPEED_NUM_40G;
	if (link_speeds & RTE_ETH_LINK_SPEED_50G)
		speed = RTE_ETH_SPEED_NUM_50G;
	if (link_speeds & RTE_ETH_LINK_SPEED_100G)
		speed = RTE_ETH_SPEED_NUM_100G;
	if (link_speeds & RTE_ETH_LINK_SPEED_200G)
		speed = RTE_ETH_SPEED_NUM_200G;

	return speed;
}

static uint8_t
hns3_get_link_duplex(uint32_t link_speeds)
{
	if ((link_speeds & RTE_ETH_LINK_SPEED_10M_HD) ||
	    (link_speeds & RTE_ETH_LINK_SPEED_100M_HD))
		return RTE_ETH_LINK_HALF_DUPLEX;
	else
		return RTE_ETH_LINK_FULL_DUPLEX;
}

static int
hns3_set_copper_port_link_speed(struct hns3_hw *hw,
				struct hns3_set_link_speed_cfg *cfg)
{
	struct hns3_cmd_desc desc[HNS3_PHY_PARAM_CFG_BD_NUM];
	struct hns3_phy_params_bd0_cmd *req;
	uint16_t i;

	for (i = 0; i < HNS3_PHY_PARAM_CFG_BD_NUM - 1; i++) {
		hns3_cmd_setup_basic_desc(&desc[i], HNS3_OPC_PHY_PARAM_CFG,
					  false);
		desc[i].flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);
	}
	hns3_cmd_setup_basic_desc(&desc[i], HNS3_OPC_PHY_PARAM_CFG, false);
	req = (struct hns3_phy_params_bd0_cmd *)desc[0].data;
	req->autoneg = cfg->autoneg;

	/*
	 * The full speed capability is used to negotiate when
	 * auto-negotiation is enabled.
	 */
	if (cfg->autoneg) {
		req->advertising = HNS3_PHY_LINK_SPEED_10M_BIT |
				    HNS3_PHY_LINK_SPEED_10M_HD_BIT |
				    HNS3_PHY_LINK_SPEED_100M_BIT |
				    HNS3_PHY_LINK_SPEED_100M_HD_BIT |
				    HNS3_PHY_LINK_SPEED_1000M_BIT;
	} else {
		req->speed = cfg->speed;
		req->duplex = cfg->duplex;
	}

	return hns3_cmd_send(hw, desc, HNS3_PHY_PARAM_CFG_BD_NUM);
}

static int
hns3_set_autoneg(struct hns3_hw *hw, bool enable)
{
	struct hns3_config_auto_neg_cmd *req;
	struct hns3_cmd_desc desc;
	uint32_t flag = 0;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_CONFIG_AN_MODE, false);

	req = (struct hns3_config_auto_neg_cmd *)desc.data;
	if (enable)
		hns3_set_bit(flag, HNS3_MAC_CFG_AN_EN_B, 1);
	req->cfg_an_cmd_flag = rte_cpu_to_le_32(flag);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		hns3_err(hw, "autoneg set cmd failed, ret = %d.", ret);

	return ret;
}

static int
hns3_set_fiber_port_link_speed(struct hns3_hw *hw,
			       struct hns3_set_link_speed_cfg *cfg)
{
	int ret;

	if (hw->mac.support_autoneg) {
		ret = hns3_set_autoneg(hw, cfg->autoneg);
		if (ret) {
			hns3_err(hw, "failed to configure auto-negotiation.");
			return ret;
		}

		/*
		 * To enable auto-negotiation, we only need to open the switch
		 * of auto-negotiation, then firmware sets all speed
		 * capabilities.
		 */
		if (cfg->autoneg)
			return 0;
	}

	/*
	 * Some hardware doesn't support auto-negotiation, but users may not
	 * configure link_speeds (default 0), which means auto-negotiation.
	 * In this case, a warning message need to be printed, instead of
	 * an error.
	 */
	if (cfg->autoneg) {
		hns3_warn(hw, "auto-negotiation is not supported, use default fixed speed!");
		return 0;
	}

	return hns3_cfg_mac_speed_dup(hw, cfg->speed, cfg->duplex);
}

const char *
hns3_get_media_type_name(uint8_t media_type)
{
	if (media_type == HNS3_MEDIA_TYPE_FIBER)
		return "fiber";
	else if (media_type == HNS3_MEDIA_TYPE_COPPER)
		return "copper";
	else if (media_type == HNS3_MEDIA_TYPE_BACKPLANE)
		return "backplane";
	else
		return "unknown";
}

static int
hns3_set_port_link_speed(struct hns3_hw *hw,
			 struct hns3_set_link_speed_cfg *cfg)
{
	int ret;

	if (hw->mac.media_type == HNS3_MEDIA_TYPE_COPPER)
		ret = hns3_set_copper_port_link_speed(hw, cfg);
	else
		ret = hns3_set_fiber_port_link_speed(hw, cfg);

	if (ret) {
		hns3_err(hw, "failed to set %s port link speed, ret = %d.",
			 hns3_get_media_type_name(hw->mac.media_type),
			 ret);
		return ret;
	}

	return 0;
}

static int
hns3_apply_link_speed(struct hns3_hw *hw)
{
	struct rte_eth_conf *conf = &hw->data->dev_conf;
	struct hns3_set_link_speed_cfg cfg;

	memset(&cfg, 0, sizeof(struct hns3_set_link_speed_cfg));
	cfg.autoneg = (conf->link_speeds == RTE_ETH_LINK_SPEED_AUTONEG) ?
			RTE_ETH_LINK_AUTONEG : RTE_ETH_LINK_FIXED;
	if (cfg.autoneg != RTE_ETH_LINK_AUTONEG) {
		cfg.speed = hns3_get_link_speed(conf->link_speeds);
		cfg.duplex = hns3_get_link_duplex(conf->link_speeds);
	}

	return hns3_set_port_link_speed(hw, &cfg);
}

static int
hns3_do_start(struct hns3_adapter *hns, bool reset_queue)
{
	struct hns3_hw *hw = &hns->hw;
	bool link_en;
	int ret;

	ret = hns3_update_queue_map_configure(hns);
	if (ret) {
		hns3_err(hw, "failed to update queue mapping configuration, ret = %d",
			 ret);
		return ret;
	}

	/* Note: hns3_tm_conf_update must be called after configuring DCB. */
	ret = hns3_tm_conf_update(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "failed to update tm conf, ret = %d.", ret);
		return ret;
	}

	hns3_enable_rxd_adv_layout(hw);

	ret = hns3_init_queues(hns, reset_queue);
	if (ret) {
		PMD_INIT_LOG(ERR, "failed to init queues, ret = %d.", ret);
		return ret;
	}

	link_en = hw->set_link_down ? false : true;
	ret = hns3_cfg_mac_mode(hw, link_en);
	if (ret) {
		PMD_INIT_LOG(ERR, "failed to enable MAC, ret = %d", ret);
		goto err_config_mac_mode;
	}

	ret = hns3_apply_link_speed(hw);
	if (ret)
		goto err_set_link_speed;

	return hns3_restore_filter(hns);

err_set_link_speed:
	(void)hns3_cfg_mac_mode(hw, false);

err_config_mac_mode:
	hns3_dev_release_mbufs(hns);
	/*
	 * Here is exception handling, hns3_reset_all_tqps will have the
	 * corresponding error message if it is handled incorrectly, so it is
	 * not necessary to check hns3_reset_all_tqps return value, here keep
	 * ret as the error code causing the exception.
	 */
	(void)hns3_reset_all_tqps(hns);
	return ret;
}

static int
hns3_dev_start(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	bool old_state = hw->set_link_down;
	int ret;

	PMD_INIT_FUNC_TRACE();
	if (__atomic_load_n(&hw->reset.resetting, __ATOMIC_RELAXED))
		return -EBUSY;

	rte_spinlock_lock(&hw->lock);
	hw->adapter_state = HNS3_NIC_STARTING;

	/*
	 * If the dev_set_link_down() API has been called, the "set_link_down"
	 * flag can be cleared by dev_start() API. In addition, the flag should
	 * also be cleared before calling hns3_do_start() so that MAC can be
	 * enabled in dev_start stage.
	 */
	hw->set_link_down = false;
	ret = hns3_do_start(hns, true);
	if (ret)
		goto do_start_fail;

	ret = hns3_map_rx_interrupt(dev);
	if (ret)
		goto map_rx_inter_err;

	/*
	 * There are three register used to control the status of a TQP
	 * (contains a pair of Tx queue and Rx queue) in the new version network
	 * engine. One is used to control the enabling of Tx queue, the other is
	 * used to control the enabling of Rx queue, and the last is the master
	 * switch used to control the enabling of the tqp. The Tx register and
	 * TQP register must be enabled at the same time to enable a Tx queue.
	 * The same applies to the Rx queue. For the older network engine, this
	 * function only refresh the enabled flag, and it is used to update the
	 * status of queue in the dpdk framework.
	 */
	ret = hns3_start_all_txqs(dev);
	if (ret)
		goto map_rx_inter_err;

	ret = hns3_start_all_rxqs(dev);
	if (ret)
		goto start_all_rxqs_fail;

	hw->adapter_state = HNS3_NIC_STARTED;
	rte_spinlock_unlock(&hw->lock);

	hns3_rx_scattered_calc(dev);
	hns3_start_rxtx_datapath(dev);

	/* Enable interrupt of all rx queues before enabling queues */
	hns3_dev_all_rx_queue_intr_enable(hw, true);

	/*
	 * After finished the initialization, enable tqps to receive/transmit
	 * packets and refresh all queue status.
	 */
	hns3_start_tqps(hw);

	hns3_tm_dev_start_proc(hw);

	if (dev->data->dev_conf.intr_conf.lsc != 0)
		hns3_dev_link_update(dev, 0);
	rte_eal_alarm_set(HNS3_SERVICE_INTERVAL, hns3_service_handler, dev);

	hns3_info(hw, "hns3 dev start successful!");

	return 0;

start_all_rxqs_fail:
	hns3_stop_all_txqs(dev);
map_rx_inter_err:
	(void)hns3_do_stop(hns);
do_start_fail:
	hw->set_link_down = old_state;
	hw->adapter_state = HNS3_NIC_CONFIGURED;
	rte_spinlock_unlock(&hw->lock);

	return ret;
}

static int
hns3_do_stop(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	int ret;

	/*
	 * The "hns3_do_stop" function will also be called by .stop_service to
	 * prepare reset. At the time of global or IMP reset, the command cannot
	 * be sent to stop the tx/rx queues. The mbuf in Tx/Rx queues may be
	 * accessed during the reset process. So the mbuf can not be released
	 * during reset and is required to be released after the reset is
	 * completed.
	 */
	if (__atomic_load_n(&hw->reset.resetting,  __ATOMIC_RELAXED) == 0)
		hns3_dev_release_mbufs(hns);

	ret = hns3_cfg_mac_mode(hw, false);
	if (ret)
		return ret;
	hw->mac.link_status = RTE_ETH_LINK_DOWN;

	if (__atomic_load_n(&hw->reset.disable_cmd, __ATOMIC_RELAXED) == 0) {
		hns3_configure_all_mac_addr(hns, true);
		ret = hns3_reset_all_tqps(hns);
		if (ret) {
			hns3_err(hw, "failed to reset all queues ret = %d.",
				 ret);
			return ret;
		}
	}

	return 0;
}

static int
hns3_dev_stop(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;

	PMD_INIT_FUNC_TRACE();
	dev->data->dev_started = 0;

	hw->adapter_state = HNS3_NIC_STOPPING;
	hns3_stop_rxtx_datapath(dev);

	rte_spinlock_lock(&hw->lock);
	if (__atomic_load_n(&hw->reset.resetting, __ATOMIC_RELAXED) == 0) {
		hns3_tm_dev_stop_proc(hw);
		hns3_config_mac_tnl_int(hw, false);
		hns3_stop_tqps(hw);
		hns3_do_stop(hns);
		hns3_unmap_rx_interrupt(dev);
		hw->adapter_state = HNS3_NIC_CONFIGURED;
	}
	hns3_rx_scattered_reset(dev);
	rte_eal_alarm_cancel(hns3_service_handler, dev);
	hns3_stop_report_lse(dev);
	rte_spinlock_unlock(&hw->lock);

	return 0;
}

static int
hns3_dev_close(struct rte_eth_dev *eth_dev)
{
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	int ret = 0;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		hns3_mp_uninit(eth_dev);
		return 0;
	}

	if (hw->adapter_state == HNS3_NIC_STARTED)
		ret = hns3_dev_stop(eth_dev);

	hw->adapter_state = HNS3_NIC_CLOSING;
	hns3_reset_abort(hns);
	hw->adapter_state = HNS3_NIC_CLOSED;

	hns3_configure_all_mc_mac_addr(hns, true);
	hns3_remove_all_vlan_table(hns);
	hns3_vlan_txvlan_cfg(hns, HNS3_PORT_BASE_VLAN_DISABLE, 0);
	hns3_uninit_pf(eth_dev);
	hns3_free_all_queues(eth_dev);
	rte_free(hw->reset.wait_data);
	hns3_mp_uninit(eth_dev);
	hns3_warn(hw, "Close port %u finished", hw->data->port_id);

	return ret;
}

static void
hns3_get_autoneg_rxtx_pause(struct hns3_hw *hw, bool *rx_pause, bool *tx_pause)
{
	struct hns3_mac *mac = &hw->mac;
	uint32_t advertising = mac->advertising;
	uint32_t lp_advertising = mac->lp_advertising;
	*rx_pause = false;
	*tx_pause = false;

	if (advertising & lp_advertising & HNS3_PHY_LINK_MODE_PAUSE_BIT) {
		*rx_pause = true;
		*tx_pause = true;
	} else if (advertising & lp_advertising & HNS3_PHY_LINK_MODE_ASYM_PAUSE_BIT) {
		if (advertising & HNS3_PHY_LINK_MODE_PAUSE_BIT)
			*rx_pause = true;
		else if (lp_advertising & HNS3_PHY_LINK_MODE_PAUSE_BIT)
			*tx_pause = true;
	}
}

static enum hns3_fc_mode
hns3_get_autoneg_fc_mode(struct hns3_hw *hw)
{
	enum hns3_fc_mode current_mode;
	bool rx_pause = false;
	bool tx_pause = false;

	hns3_get_autoneg_rxtx_pause(hw, &rx_pause, &tx_pause);

	if (rx_pause && tx_pause)
		current_mode = HNS3_FC_FULL;
	else if (rx_pause)
		current_mode = HNS3_FC_RX_PAUSE;
	else if (tx_pause)
		current_mode = HNS3_FC_TX_PAUSE;
	else
		current_mode = HNS3_FC_NONE;

	return current_mode;
}

static enum hns3_fc_mode
hns3_get_current_fc_mode(struct rte_eth_dev *dev)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_pf *pf = HNS3_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct hns3_mac *mac = &hw->mac;

	/*
	 * When the flow control mode is obtained, the device may not complete
	 * auto-negotiation. It is necessary to wait for link establishment.
	 */
	(void)hns3_dev_link_update(dev, 1);

	/*
	 * If the link auto-negotiation of the nic is disabled, or the flow
	 * control auto-negotiation is not supported, the forced flow control
	 * mode is used.
	 */
	if (mac->link_autoneg == 0 || !pf->support_fc_autoneg)
		return hw->requested_fc_mode;

	return hns3_get_autoneg_fc_mode(hw);
}

int
hns3_flow_ctrl_get(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_pf *pf = HNS3_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	enum hns3_fc_mode current_mode;

	current_mode = hns3_get_current_fc_mode(dev);
	switch (current_mode) {
	case HNS3_FC_FULL:
		fc_conf->mode = RTE_ETH_FC_FULL;
		break;
	case HNS3_FC_TX_PAUSE:
		fc_conf->mode = RTE_ETH_FC_TX_PAUSE;
		break;
	case HNS3_FC_RX_PAUSE:
		fc_conf->mode = RTE_ETH_FC_RX_PAUSE;
		break;
	case HNS3_FC_NONE:
	default:
		fc_conf->mode = RTE_ETH_FC_NONE;
		break;
	}

	fc_conf->pause_time = pf->pause_time;
	fc_conf->autoneg = pf->support_fc_autoneg ? hw->mac.link_autoneg : 0;

	return 0;
}

static int
hns3_check_fc_autoneg_valid(struct hns3_hw *hw, uint8_t autoneg)
{
	struct hns3_pf *pf = HNS3_DEV_HW_TO_PF(hw);

	if (!pf->support_fc_autoneg) {
		if (autoneg != 0) {
			hns3_err(hw, "unsupported fc auto-negotiation.");
			return -EOPNOTSUPP;
		}

		return 0;
	}

	/*
	 * If flow control auto-negotiation of the NIC is supported, all
	 * auto-negotiation features are supported.
	 */
	if (autoneg != hw->mac.link_autoneg) {
		hns3_err(hw, "please use 'link_speeds' in struct rte_eth_conf to change autoneg!");
		return -EOPNOTSUPP;
	}

	return 0;
}

static int
hns3_flow_ctrl_set(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_pf *pf = HNS3_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	int ret;

	if (fc_conf->high_water || fc_conf->low_water ||
	    fc_conf->send_xon || fc_conf->mac_ctrl_frame_fwd) {
		hns3_err(hw, "Unsupported flow control settings specified, "
			 "high_water(%u), low_water(%u), send_xon(%u) and "
			 "mac_ctrl_frame_fwd(%u) must be set to '0'",
			 fc_conf->high_water, fc_conf->low_water,
			 fc_conf->send_xon, fc_conf->mac_ctrl_frame_fwd);
		return -EINVAL;
	}

	ret = hns3_check_fc_autoneg_valid(hw, fc_conf->autoneg);
	if (ret)
		return ret;

	if (!fc_conf->pause_time) {
		hns3_err(hw, "Invalid pause time %u setting.",
			 fc_conf->pause_time);
		return -EINVAL;
	}

	if (!(hw->current_fc_status == HNS3_FC_STATUS_NONE ||
	    hw->current_fc_status == HNS3_FC_STATUS_MAC_PAUSE)) {
		hns3_err(hw, "PFC is enabled. Cannot set MAC pause. "
			 "current_fc_status = %d", hw->current_fc_status);
		return -EOPNOTSUPP;
	}

	if (hw->num_tc > 1 && !pf->support_multi_tc_pause) {
		hns3_err(hw, "in multi-TC scenarios, MAC pause is not supported.");
		return -EOPNOTSUPP;
	}

	rte_spinlock_lock(&hw->lock);
	ret = hns3_fc_enable(dev, fc_conf);
	rte_spinlock_unlock(&hw->lock);

	return ret;
}

static int
hns3_priority_flow_ctrl_set(struct rte_eth_dev *dev,
			    struct rte_eth_pfc_conf *pfc_conf)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret;

	if (!hns3_dev_get_support(hw, DCB)) {
		hns3_err(hw, "This port does not support dcb configurations.");
		return -EOPNOTSUPP;
	}

	if (pfc_conf->fc.high_water || pfc_conf->fc.low_water ||
	    pfc_conf->fc.send_xon || pfc_conf->fc.mac_ctrl_frame_fwd) {
		hns3_err(hw, "Unsupported flow control settings specified, "
			 "high_water(%u), low_water(%u), send_xon(%u) and "
			 "mac_ctrl_frame_fwd(%u) must be set to '0'",
			 pfc_conf->fc.high_water, pfc_conf->fc.low_water,
			 pfc_conf->fc.send_xon,
			 pfc_conf->fc.mac_ctrl_frame_fwd);
		return -EINVAL;
	}
	if (pfc_conf->fc.autoneg) {
		hns3_err(hw, "Unsupported fc auto-negotiation setting.");
		return -EINVAL;
	}
	if (pfc_conf->fc.pause_time == 0) {
		hns3_err(hw, "Invalid pause time %u setting.",
			 pfc_conf->fc.pause_time);
		return -EINVAL;
	}

	if (!(hw->current_fc_status == HNS3_FC_STATUS_NONE ||
	    hw->current_fc_status == HNS3_FC_STATUS_PFC)) {
		hns3_err(hw, "MAC pause is enabled. Cannot set PFC."
			     "current_fc_status = %d", hw->current_fc_status);
		return -EOPNOTSUPP;
	}

	rte_spinlock_lock(&hw->lock);
	ret = hns3_dcb_pfc_enable(dev, pfc_conf);
	rte_spinlock_unlock(&hw->lock);

	return ret;
}

static int
hns3_get_dcb_info(struct rte_eth_dev *dev, struct rte_eth_dcb_info *dcb_info)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_pf *pf = HNS3_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	enum rte_eth_rx_mq_mode mq_mode = dev->data->dev_conf.rxmode.mq_mode;
	int i;

	rte_spinlock_lock(&hw->lock);
	if ((uint32_t)mq_mode & RTE_ETH_MQ_RX_DCB_FLAG)
		dcb_info->nb_tcs = pf->local_max_tc;
	else
		dcb_info->nb_tcs = 1;

	for (i = 0; i < HNS3_MAX_USER_PRIO; i++)
		dcb_info->prio_tc[i] = hw->dcb_info.prio_tc[i];
	for (i = 0; i < dcb_info->nb_tcs; i++)
		dcb_info->tc_bws[i] = hw->dcb_info.pg_info[0].tc_dwrr[i];

	for (i = 0; i < hw->num_tc; i++) {
		dcb_info->tc_queue.tc_rxq[0][i].base = hw->alloc_rss_size * i;
		dcb_info->tc_queue.tc_txq[0][i].base =
						hw->tc_queue[i].tqp_offset;
		dcb_info->tc_queue.tc_rxq[0][i].nb_queue = hw->alloc_rss_size;
		dcb_info->tc_queue.tc_txq[0][i].nb_queue =
						hw->tc_queue[i].tqp_count;
	}
	rte_spinlock_unlock(&hw->lock);

	return 0;
}

static int
hns3_reinit_dev(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	int ret;

	ret = hns3_cmd_init(hw);
	if (ret) {
		hns3_err(hw, "Failed to init cmd: %d", ret);
		return ret;
	}

	ret = hns3_init_hardware(hns);
	if (ret) {
		hns3_err(hw, "Failed to init hardware: %d", ret);
		return ret;
	}

	ret = hns3_reset_all_tqps(hns);
	if (ret) {
		hns3_err(hw, "Failed to reset all queues: %d", ret);
		return ret;
	}

	ret = hns3_enable_hw_error_intr(hns, true);
	if (ret) {
		hns3_err(hw, "fail to enable hw error interrupts: %d",
			     ret);
		return ret;
	}
	hns3_info(hw, "Reset done, driver initialization finished.");

	return 0;
}

static bool
is_pf_reset_done(struct hns3_hw *hw)
{
	uint32_t val, reg, reg_bit;

	switch (hw->reset.level) {
	case HNS3_IMP_RESET:
		reg = HNS3_GLOBAL_RESET_REG;
		reg_bit = HNS3_IMP_RESET_BIT;
		break;
	case HNS3_GLOBAL_RESET:
		reg = HNS3_GLOBAL_RESET_REG;
		reg_bit = HNS3_GLOBAL_RESET_BIT;
		break;
	case HNS3_FUNC_RESET:
		reg = HNS3_FUN_RST_ING;
		reg_bit = HNS3_FUN_RST_ING_B;
		break;
	case HNS3_FLR_RESET:
	default:
		hns3_err(hw, "Wait for unsupported reset level: %d",
			 hw->reset.level);
		return true;
	}
	val = hns3_read_dev(hw, reg);
	if (hns3_get_bit(val, reg_bit))
		return false;
	else
		return true;
}

bool
hns3_is_reset_pending(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	enum hns3_reset_level reset;

	/*
	 * Check the registers to confirm whether there is reset pending.
	 * Note: This check may lead to schedule reset task, but only primary
	 *       process can process the reset event. Therefore, limit the
	 *       checking under only primary process.
	 */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		hns3_check_event_cause(hns, NULL);

	reset = hns3_get_reset_level(hns, &hw->reset.pending);
	if (reset != HNS3_NONE_RESET && hw->reset.level != HNS3_NONE_RESET &&
	    hw->reset.level < reset) {
		hns3_warn(hw, "High level reset %d is pending", reset);
		return true;
	}
	reset = hns3_get_reset_level(hns, &hw->reset.request);
	if (reset != HNS3_NONE_RESET && hw->reset.level != HNS3_NONE_RESET &&
	    hw->reset.level < reset) {
		hns3_warn(hw, "High level reset %d is request", reset);
		return true;
	}
	return false;
}

static int
hns3_wait_hardware_ready(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_wait_data *wait_data = hw->reset.wait_data;
	struct timeval tv;

	if (wait_data->result == HNS3_WAIT_SUCCESS)
		return 0;
	else if (wait_data->result == HNS3_WAIT_TIMEOUT) {
		hns3_clock_gettime(&tv);
		hns3_warn(hw, "Reset step4 hardware not ready after reset time=%ld.%.6ld",
			  tv.tv_sec, tv.tv_usec);
		return -ETIME;
	} else if (wait_data->result == HNS3_WAIT_REQUEST)
		return -EAGAIN;

	wait_data->hns = hns;
	wait_data->check_completion = is_pf_reset_done;
	wait_data->end_ms = (uint64_t)HNS3_RESET_WAIT_CNT *
				HNS3_RESET_WAIT_MS + hns3_clock_gettime_ms();
	wait_data->interval = HNS3_RESET_WAIT_MS * USEC_PER_MSEC;
	wait_data->count = HNS3_RESET_WAIT_CNT;
	wait_data->result = HNS3_WAIT_REQUEST;
	rte_eal_alarm_set(wait_data->interval, hns3_wait_callback, wait_data);
	return -EAGAIN;
}

static int
hns3_func_reset_cmd(struct hns3_hw *hw, int func_id)
{
	struct hns3_cmd_desc desc;
	struct hns3_reset_cmd *req = (struct hns3_reset_cmd *)desc.data;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_CFG_RST_TRIGGER, false);
	hns3_set_bit(req->mac_func_reset, HNS3_CFG_RESET_FUNC_B, 1);
	req->fun_reset_vfid = func_id;

	return hns3_cmd_send(hw, &desc, 1);
}

static void
hns3_msix_process(struct hns3_adapter *hns, enum hns3_reset_level reset_level)
{
	struct hns3_hw *hw = &hns->hw;
	struct timeval tv;
	uint32_t val;

	hns3_clock_gettime(&tv);
	if (hns3_read_dev(hw, HNS3_GLOBAL_RESET_REG) ||
	    hns3_read_dev(hw, HNS3_FUN_RST_ING)) {
		hns3_warn(hw, "Don't process msix during resetting time=%ld.%.6ld",
			  tv.tv_sec, tv.tv_usec);
		return;
	}

	switch (reset_level) {
	case HNS3_IMP_RESET:
		val = hns3_read_dev(hw, HNS3_VECTOR0_OTER_EN_REG);
		hns3_set_bit(val, HNS3_VECTOR0_TRIGGER_IMP_RESET_B, 1);
		hns3_write_dev(hw, HNS3_VECTOR0_OTER_EN_REG, val);
		hns3_warn(hw, "IMP Reset requested time=%ld.%.6ld",
			  tv.tv_sec, tv.tv_usec);
		break;
	case HNS3_GLOBAL_RESET:
		val = hns3_read_dev(hw, HNS3_GLOBAL_RESET_REG);
		hns3_set_bit(val, HNS3_GLOBAL_RESET_BIT, 1);
		hns3_write_dev(hw, HNS3_GLOBAL_RESET_REG, val);
		hns3_warn(hw, "Global Reset requested time=%ld.%.6ld",
			  tv.tv_sec, tv.tv_usec);
		break;
	case HNS3_FUNC_RESET:
		hns3_warn(hw, "PF Reset requested time=%ld.%.6ld",
			  tv.tv_sec, tv.tv_usec);
		/* schedule again to check later */
		hns3_atomic_set_bit(HNS3_FUNC_RESET, &hw->reset.pending);
		hns3_schedule_reset(hns);
		break;
	default:
		hns3_warn(hw, "Unsupported reset level: %d", reset_level);
		return;
	}
	hns3_atomic_clear_bit(reset_level, &hw->reset.request);
}

static enum hns3_reset_level
hns3_get_reset_level(struct hns3_adapter *hns, uint64_t *levels)
{
	struct hns3_hw *hw = &hns->hw;
	enum hns3_reset_level reset_level = HNS3_NONE_RESET;

	/* Return the highest priority reset level amongst all */
	if (hns3_atomic_test_bit(HNS3_IMP_RESET, levels))
		reset_level = HNS3_IMP_RESET;
	else if (hns3_atomic_test_bit(HNS3_GLOBAL_RESET, levels))
		reset_level = HNS3_GLOBAL_RESET;
	else if (hns3_atomic_test_bit(HNS3_FUNC_RESET, levels))
		reset_level = HNS3_FUNC_RESET;
	else if (hns3_atomic_test_bit(HNS3_FLR_RESET, levels))
		reset_level = HNS3_FLR_RESET;

	if (hw->reset.level != HNS3_NONE_RESET && reset_level < hw->reset.level)
		return HNS3_NONE_RESET;

	return reset_level;
}

static void
hns3_record_imp_error(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	uint32_t reg_val;

	reg_val = hns3_read_dev(hw, HNS3_VECTOR0_OTER_EN_REG);
	if (hns3_get_bit(reg_val, HNS3_VECTOR0_IMP_RD_POISON_B)) {
		hns3_warn(hw, "Detected IMP RD poison!");
		hns3_set_bit(reg_val, HNS3_VECTOR0_IMP_RD_POISON_B, 0);
		hns3_write_dev(hw, HNS3_VECTOR0_OTER_EN_REG, reg_val);
	}

	if (hns3_get_bit(reg_val, HNS3_VECTOR0_IMP_CMDQ_ERR_B)) {
		hns3_warn(hw, "Detected IMP CMDQ error!");
		hns3_set_bit(reg_val, HNS3_VECTOR0_IMP_CMDQ_ERR_B, 0);
		hns3_write_dev(hw, HNS3_VECTOR0_OTER_EN_REG, reg_val);
	}
}

static int
hns3_prepare_reset(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	uint32_t reg_val;
	int ret;

	switch (hw->reset.level) {
	case HNS3_FUNC_RESET:
		ret = hns3_func_reset_cmd(hw, HNS3_PF_FUNC_ID);
		if (ret)
			return ret;

		/*
		 * After performaning pf reset, it is not necessary to do the
		 * mailbox handling or send any command to firmware, because
		 * any mailbox handling or command to firmware is only valid
		 * after hns3_cmd_init is called.
		 */
		__atomic_store_n(&hw->reset.disable_cmd, 1, __ATOMIC_RELAXED);
		hw->reset.stats.request_cnt++;
		break;
	case HNS3_IMP_RESET:
		hns3_record_imp_error(hns);
		reg_val = hns3_read_dev(hw, HNS3_VECTOR0_OTER_EN_REG);
		hns3_write_dev(hw, HNS3_VECTOR0_OTER_EN_REG, reg_val |
			       BIT(HNS3_VECTOR0_IMP_RESET_INT_B));
		break;
	default:
		break;
	}
	return 0;
}

static int
hns3_set_rst_done(struct hns3_hw *hw)
{
	struct hns3_pf_rst_done_cmd *req;
	struct hns3_cmd_desc desc;

	req = (struct hns3_pf_rst_done_cmd *)desc.data;
	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_PF_RST_DONE, false);
	req->pf_rst_done |= HNS3_PF_RESET_DONE_BIT;
	return hns3_cmd_send(hw, &desc, 1);
}

static int
hns3_stop_service(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	struct rte_eth_dev *eth_dev;

	eth_dev = &rte_eth_devices[hw->data->port_id];
	hw->mac.link_status = RTE_ETH_LINK_DOWN;
	if (hw->adapter_state == HNS3_NIC_STARTED) {
		rte_eal_alarm_cancel(hns3_service_handler, eth_dev);
		hns3_update_linkstatus_and_event(hw, false);
	}
	hns3_stop_rxtx_datapath(eth_dev);

	rte_spinlock_lock(&hw->lock);
	if (hns->hw.adapter_state == HNS3_NIC_STARTED ||
	    hw->adapter_state == HNS3_NIC_STOPPING) {
		hns3_enable_all_queues(hw, false);
		hns3_do_stop(hns);
		hw->reset.mbuf_deferred_free = true;
	} else
		hw->reset.mbuf_deferred_free = false;

	/*
	 * It is cumbersome for hardware to pick-and-choose entries for deletion
	 * from table space. Hence, for function reset software intervention is
	 * required to delete the entries
	 */
	if (__atomic_load_n(&hw->reset.disable_cmd, __ATOMIC_RELAXED) == 0)
		hns3_configure_all_mc_mac_addr(hns, true);
	rte_spinlock_unlock(&hw->lock);

	return 0;
}

static int
hns3_start_service(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	struct rte_eth_dev *eth_dev;

	if (hw->reset.level == HNS3_IMP_RESET ||
	    hw->reset.level == HNS3_GLOBAL_RESET)
		hns3_set_rst_done(hw);
	eth_dev = &rte_eth_devices[hw->data->port_id];
	hns3_start_rxtx_datapath(eth_dev);
	if (hw->adapter_state == HNS3_NIC_STARTED) {
		/*
		 * This API parent function already hold the hns3_hw.lock, the
		 * hns3_service_handler may report lse, in bonding application
		 * it will call driver's ops which may acquire the hns3_hw.lock
		 * again, thus lead to deadlock.
		 * We defer calls hns3_service_handler to avoid the deadlock.
		 */
		rte_eal_alarm_set(HNS3_SERVICE_QUICK_INTERVAL,
				  hns3_service_handler, eth_dev);

		/* Enable interrupt of all rx queues before enabling queues */
		hns3_dev_all_rx_queue_intr_enable(hw, true);
		/*
		 * Enable state of each rxq and txq will be recovered after
		 * reset, so we need to restore them before enable all tqps;
		 */
		hns3_restore_tqp_enable_state(hw);
		/*
		 * When finished the initialization, enable queues to receive
		 * and transmit packets.
		 */
		hns3_enable_all_queues(hw, true);
	}

	return 0;
}

static int
hns3_restore_conf(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	int ret;

	ret = hns3_configure_all_mac_addr(hns, false);
	if (ret)
		return ret;

	ret = hns3_configure_all_mc_mac_addr(hns, false);
	if (ret)
		goto err_mc_mac;

	ret = hns3_dev_promisc_restore(hns);
	if (ret)
		goto err_promisc;

	ret = hns3_restore_vlan_table(hns);
	if (ret)
		goto err_promisc;

	ret = hns3_restore_vlan_conf(hns);
	if (ret)
		goto err_promisc;

	ret = hns3_restore_ptp(hns);
	if (ret)
		goto err_promisc;

	ret = hns3_restore_rx_interrupt(hw);
	if (ret)
		goto err_promisc;

	ret = hns3_restore_gro_conf(hw);
	if (ret)
		goto err_promisc;

	ret = hns3_restore_fec(hw);
	if (ret)
		goto err_promisc;

	if (hns->hw.adapter_state == HNS3_NIC_STARTED) {
		ret = hns3_do_start(hns, false);
		if (ret)
			goto err_promisc;
		hns3_info(hw, "hns3 dev restart successful!");
	} else if (hw->adapter_state == HNS3_NIC_STOPPING)
		hw->adapter_state = HNS3_NIC_CONFIGURED;
	return 0;

err_promisc:
	hns3_configure_all_mc_mac_addr(hns, true);
err_mc_mac:
	hns3_configure_all_mac_addr(hns, true);
	return ret;
}

static void
hns3_reset_service(void *param)
{
	struct hns3_adapter *hns = (struct hns3_adapter *)param;
	struct hns3_hw *hw = &hns->hw;
	enum hns3_reset_level reset_level;
	struct timeval tv_delta;
	struct timeval tv_start;
	struct timeval tv;
	uint64_t msec;
	int ret;

	/*
	 * The interrupt is not triggered within the delay time.
	 * The interrupt may have been lost. It is necessary to handle
	 * the interrupt to recover from the error.
	 */
	if (__atomic_load_n(&hw->reset.schedule, __ATOMIC_RELAXED) ==
			    SCHEDULE_DEFERRED) {
		__atomic_store_n(&hw->reset.schedule, SCHEDULE_REQUESTED,
				  __ATOMIC_RELAXED);
		hns3_err(hw, "Handling interrupts in delayed tasks");
		hns3_interrupt_handler(&rte_eth_devices[hw->data->port_id]);
		reset_level = hns3_get_reset_level(hns, &hw->reset.pending);
		if (reset_level == HNS3_NONE_RESET) {
			hns3_err(hw, "No reset level is set, try IMP reset");
			hns3_atomic_set_bit(HNS3_IMP_RESET, &hw->reset.pending);
		}
	}
	__atomic_store_n(&hw->reset.schedule, SCHEDULE_NONE, __ATOMIC_RELAXED);

	/*
	 * Check if there is any ongoing reset in the hardware. This status can
	 * be checked from reset_pending. If there is then, we need to wait for
	 * hardware to complete reset.
	 *    a. If we are able to figure out in reasonable time that hardware
	 *       has fully resetted then, we can proceed with driver, client
	 *       reset.
	 *    b. else, we can come back later to check this status so re-sched
	 *       now.
	 */
	reset_level = hns3_get_reset_level(hns, &hw->reset.pending);
	if (reset_level != HNS3_NONE_RESET) {
		hns3_clock_gettime(&tv_start);
		ret = hns3_reset_process(hns, reset_level);
		hns3_clock_gettime(&tv);
		timersub(&tv, &tv_start, &tv_delta);
		msec = hns3_clock_calctime_ms(&tv_delta);
		if (msec > HNS3_RESET_PROCESS_MS)
			hns3_err(hw, "%d handle long time delta %" PRIu64 " ms time=%ld.%.6ld",
				 hw->reset.level, msec,
				 tv.tv_sec, tv.tv_usec);
		if (ret == -EAGAIN)
			return;
	}

	/* Check if we got any *new* reset requests to be honored */
	reset_level = hns3_get_reset_level(hns, &hw->reset.request);
	if (reset_level != HNS3_NONE_RESET)
		hns3_msix_process(hns, reset_level);
}

static uint32_t
hns3_get_speed_fec_capa(struct rte_eth_fec_capa *speed_fec_capa,
			uint32_t speed_capa)
{
	uint32_t speed_bit;
	uint32_t num = 0;
	uint32_t i;

	for (i = 0; i < RTE_DIM(speed_fec_capa_tbl); i++) {
		speed_bit =
			rte_eth_speed_bitflag(speed_fec_capa_tbl[i].speed,
					      RTE_ETH_LINK_FULL_DUPLEX);
		if ((speed_capa & speed_bit) == 0)
			continue;

		speed_fec_capa[num].speed = speed_fec_capa_tbl[i].speed;
		speed_fec_capa[num].capa = speed_fec_capa_tbl[i].capa;
		num++;
	}

	return num;
}

static int
hns3_fec_get_capability(struct rte_eth_dev *dev,
			struct rte_eth_fec_capa *speed_fec_capa,
			unsigned int num)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	unsigned int speed_num;
	uint32_t speed_capa;

	speed_capa = hns3_get_speed_capa(hw);
	/* speed_num counts number of speed capabilities */
	speed_num = rte_popcount32(speed_capa & HNS3_SPEEDS_SUPP_FEC);
	if (speed_num == 0)
		return -ENOTSUP;

	if (speed_fec_capa == NULL)
		return speed_num;

	if (num < speed_num) {
		hns3_err(hw, "not enough array size(%u) to store FEC capabilities, should not be less than %u",
			 num, speed_num);
		return -EINVAL;
	}

	return hns3_get_speed_fec_capa(speed_fec_capa, speed_capa);
}


static int
get_current_fec_auto_state(struct hns3_hw *hw, uint8_t *state)
{
	struct hns3_config_fec_cmd *req;
	struct hns3_cmd_desc desc;
	int ret;

	/*
	 * CMD(HNS3_OPC_CONFIG_FEC_MODE) read is not supported
	 * in device of link speed
	 * below 10 Gbps.
	 */
	if (hw->mac.link_speed < RTE_ETH_SPEED_NUM_10G) {
		*state = 0;
		return 0;
	}

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_CONFIG_FEC_MODE, true);
	req = (struct hns3_config_fec_cmd *)desc.data;
	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		hns3_err(hw, "get current fec auto state failed, ret = %d",
			 ret);
		return ret;
	}

	*state = req->fec_mode & (1U << HNS3_MAC_CFG_FEC_AUTO_EN_B);
	return 0;
}

static int
hns3_fec_get_internal(struct hns3_hw *hw, uint32_t *fec_capa)
{
	struct hns3_sfp_info_cmd *resp;
	uint32_t tmp_fec_capa;
	uint8_t auto_state;
	struct hns3_cmd_desc desc;
	int ret;

	/*
	 * If link is down and AUTO is enabled, AUTO is returned, otherwise,
	 * configured FEC mode is returned.
	 * If link is up, current FEC mode is returned.
	 */
	if (hw->mac.link_status == RTE_ETH_LINK_DOWN) {
		ret = get_current_fec_auto_state(hw, &auto_state);
		if (ret)
			return ret;

		if (auto_state == 0x1) {
			*fec_capa = RTE_ETH_FEC_MODE_CAPA_MASK(AUTO);
			return 0;
		}
	}

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_GET_SFP_INFO, true);
	resp = (struct hns3_sfp_info_cmd *)desc.data;
	resp->query_type = HNS3_ACTIVE_QUERY;

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret == -EOPNOTSUPP) {
		hns3_err(hw, "IMP do not support get FEC, ret = %d", ret);
		return ret;
	} else if (ret) {
		hns3_err(hw, "get FEC failed, ret = %d", ret);
		return ret;
	}

	/*
	 * FEC mode order defined in hns3 hardware is inconsistent with
	 * that defined in the ethdev library. So the sequence needs
	 * to be converted.
	 */
	switch (resp->active_fec) {
	case HNS3_MAC_FEC_OFF:
		tmp_fec_capa = RTE_ETH_FEC_MODE_CAPA_MASK(NOFEC);
		break;
	case HNS3_MAC_FEC_BASER:
		tmp_fec_capa = RTE_ETH_FEC_MODE_CAPA_MASK(BASER);
		break;
	case HNS3_MAC_FEC_RS:
		tmp_fec_capa = RTE_ETH_FEC_MODE_CAPA_MASK(RS);
		break;
	case HNS3_MAC_FEC_LLRS:
		tmp_fec_capa = RTE_ETH_FEC_MODE_CAPA_MASK(LLRS);
		break;
	default:
		tmp_fec_capa = RTE_ETH_FEC_MODE_CAPA_MASK(NOFEC);
		break;
	}

	*fec_capa = tmp_fec_capa;
	return 0;
}

static int
hns3_fec_get(struct rte_eth_dev *dev, uint32_t *fec_capa)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	return hns3_fec_get_internal(hw, fec_capa);
}

static int
hns3_set_fec_hw(struct hns3_hw *hw, uint32_t mode)
{
	struct hns3_config_fec_cmd *req;
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_CONFIG_FEC_MODE, false);

	req = (struct hns3_config_fec_cmd *)desc.data;
	switch (mode) {
	case RTE_ETH_FEC_MODE_CAPA_MASK(NOFEC):
		hns3_set_field(req->fec_mode, HNS3_MAC_CFG_FEC_MODE_M,
				HNS3_MAC_CFG_FEC_MODE_S, HNS3_MAC_FEC_OFF);
		break;
	case RTE_ETH_FEC_MODE_CAPA_MASK(BASER):
		hns3_set_field(req->fec_mode, HNS3_MAC_CFG_FEC_MODE_M,
				HNS3_MAC_CFG_FEC_MODE_S, HNS3_MAC_FEC_BASER);
		break;
	case RTE_ETH_FEC_MODE_CAPA_MASK(RS):
		hns3_set_field(req->fec_mode, HNS3_MAC_CFG_FEC_MODE_M,
				HNS3_MAC_CFG_FEC_MODE_S, HNS3_MAC_FEC_RS);
		break;
	case RTE_ETH_FEC_MODE_CAPA_MASK(LLRS):
		hns3_set_field(req->fec_mode, HNS3_MAC_CFG_FEC_MODE_M,
				HNS3_MAC_CFG_FEC_MODE_S, HNS3_MAC_FEC_LLRS);
		break;
	case RTE_ETH_FEC_MODE_CAPA_MASK(AUTO):
		hns3_set_bit(req->fec_mode, HNS3_MAC_CFG_FEC_AUTO_EN_B, 1);
		break;
	default:
		return 0;
	}
	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		hns3_err(hw, "set fec mode failed, ret = %d", ret);

	return ret;
}

static uint32_t
hns3_parse_hw_fec_capa(uint8_t hw_fec_capa)
{
	const struct {
		uint32_t hw_fec_capa;
		uint32_t fec_capa;
	} fec_capa_map[] = {
		{ HNS3_FIBER_FEC_AUTO_BIT, RTE_ETH_FEC_MODE_CAPA_MASK(AUTO) },
		{ HNS3_FIBER_FEC_BASER_BIT, RTE_ETH_FEC_MODE_CAPA_MASK(BASER) },
		{ HNS3_FIBER_FEC_RS_BIT, RTE_ETH_FEC_MODE_CAPA_MASK(RS) },
		{ HNS3_FIBER_FEC_LLRS_BIT, RTE_ETH_FEC_MODE_CAPA_MASK(LLRS) },
		{ HNS3_FIBER_FEC_NOFEC_BIT, RTE_ETH_FEC_MODE_CAPA_MASK(NOFEC) },
	};
	uint32_t capa = 0;
	uint32_t i;

	for (i = 0; i < RTE_DIM(fec_capa_map); i++) {
		if ((hw_fec_capa & fec_capa_map[i].hw_fec_capa) != 0)
			capa |= fec_capa_map[i].fec_capa;
	}

	return capa;
}

static uint32_t
hns3_get_current_speed_fec_cap(struct hns3_mac *mac)
{
	uint32_t i;

	if (mac->fec_capa != 0)
		return hns3_parse_hw_fec_capa(mac->fec_capa);

	for (i = 0; i < RTE_DIM(speed_fec_capa_tbl); i++) {
		if (mac->link_speed == speed_fec_capa_tbl[i].speed)
			return speed_fec_capa_tbl[i].capa;
	}

	return 0;
}

static int
hns3_fec_mode_valid(struct rte_eth_dev *dev, uint32_t mode)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(hns);
	uint32_t cur_capa;

	if (rte_popcount32(mode) != 1) {
		hns3_err(hw, "FEC mode(0x%x) should be only one bit set", mode);
		return -EINVAL;
	}

	/*
	 * Check whether the configured mode is within the FEC capability.
	 * If not, the configured mode will not be supported.
	 */
	cur_capa = hns3_get_current_speed_fec_cap(&hw->mac);
	if ((cur_capa & mode) == 0) {
		hns3_err(hw, "unsupported FEC mode(0x%x)", mode);
		return -EINVAL;
	}

	return 0;
}

static int
hns3_fec_set(struct rte_eth_dev *dev, uint32_t mode)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(hns);
	struct hns3_pf *pf = &hns->pf;
	int ret;

	ret = hns3_fec_mode_valid(dev, mode);
	if (ret != 0)
		return ret;

	rte_spinlock_lock(&hw->lock);
	ret = hns3_set_fec_hw(hw, mode);
	if (ret) {
		rte_spinlock_unlock(&hw->lock);
		return ret;
	}

	pf->fec_mode = mode;
	rte_spinlock_unlock(&hw->lock);

	return 0;
}

static int
hns3_restore_fec(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = &hns->pf;
	uint32_t mode = pf->fec_mode;
	int ret;

	ret = hns3_set_fec_hw(hw, mode);
	if (ret)
		hns3_err(hw, "restore fec mode(0x%x) failed, ret = %d",
			 mode, ret);

	return ret;
}

static int
hns3_query_dev_fec_info(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_pf *pf = HNS3_DEV_PRIVATE_TO_PF(hns);
	int ret;

	ret = hns3_fec_get_internal(hw, &pf->fec_mode);
	if (ret)
		hns3_err(hw, "query device FEC info failed, ret = %d", ret);

	return ret;
}

static bool
hns3_optical_module_existed(struct hns3_hw *hw)
{
	struct hns3_cmd_desc desc;
	bool existed;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_GET_SFP_EXIST, true);
	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		hns3_err(hw,
			 "fail to get optical module exist state, ret = %d.",
			 ret);
		return false;
	}
	existed = !!desc.data[0];

	return existed;
}

static int
hns3_get_module_eeprom_data(struct hns3_hw *hw, uint32_t offset,
				uint32_t len, uint8_t *data)
{
#define HNS3_SFP_INFO_CMD_NUM 6
#define HNS3_SFP_INFO_MAX_LEN \
	(HNS3_SFP_INFO_BD0_LEN + \
	(HNS3_SFP_INFO_CMD_NUM - 1) * HNS3_SFP_INFO_BDX_LEN)
	struct hns3_cmd_desc desc[HNS3_SFP_INFO_CMD_NUM];
	struct hns3_sfp_info_bd0_cmd *sfp_info_bd0;
	uint16_t read_len;
	uint16_t copy_len;
	int ret;
	int i;

	for (i = 0; i < HNS3_SFP_INFO_CMD_NUM; i++) {
		hns3_cmd_setup_basic_desc(&desc[i], HNS3_OPC_GET_SFP_EEPROM,
					  true);
		if (i < HNS3_SFP_INFO_CMD_NUM - 1)
			desc[i].flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);
	}

	sfp_info_bd0 = (struct hns3_sfp_info_bd0_cmd *)desc[0].data;
	sfp_info_bd0->offset = rte_cpu_to_le_16((uint16_t)offset);
	read_len = RTE_MIN(len, HNS3_SFP_INFO_MAX_LEN);
	sfp_info_bd0->read_len = rte_cpu_to_le_16((uint16_t)read_len);

	ret = hns3_cmd_send(hw, desc, HNS3_SFP_INFO_CMD_NUM);
	if (ret) {
		hns3_err(hw, "fail to get module EEPROM info, ret = %d.",
				ret);
		return ret;
	}

	/* The data format in BD0 is different with the others. */
	copy_len = RTE_MIN(len, HNS3_SFP_INFO_BD0_LEN);
	memcpy(data, sfp_info_bd0->data, copy_len);
	read_len = copy_len;

	for (i = 1; i < HNS3_SFP_INFO_CMD_NUM; i++) {
		if (read_len >= len)
			break;

		copy_len = RTE_MIN(len - read_len, HNS3_SFP_INFO_BDX_LEN);
		memcpy(data + read_len, desc[i].data, copy_len);
		read_len += copy_len;
	}

	return (int)read_len;
}

static int
hns3_get_module_eeprom(struct rte_eth_dev *dev,
		       struct rte_dev_eeprom_info *info)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(hns);
	uint32_t offset = info->offset;
	uint32_t len = info->length;
	uint8_t *data = info->data;
	uint32_t read_len = 0;

	if (hw->mac.media_type != HNS3_MEDIA_TYPE_FIBER)
		return -ENOTSUP;

	if (!hns3_optical_module_existed(hw)) {
		hns3_err(hw, "fail to read module EEPROM: no module is connected.");
		return -EIO;
	}

	while (read_len < len) {
		int ret;
		ret = hns3_get_module_eeprom_data(hw, offset + read_len,
						  len - read_len,
						  data + read_len);
		if (ret < 0)
			return -EIO;
		read_len += ret;
	}

	return 0;
}

static int
hns3_get_module_info(struct rte_eth_dev *dev,
		     struct rte_eth_dev_module_info *modinfo)
{
#define HNS3_SFF8024_ID_SFP		0x03
#define HNS3_SFF8024_ID_QSFP_8438	0x0c
#define HNS3_SFF8024_ID_QSFP_8436_8636	0x0d
#define HNS3_SFF8024_ID_QSFP28_8636	0x11
#define HNS3_SFF_8636_V1_3		0x03
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(hns);
	struct rte_dev_eeprom_info info;
	struct hns3_sfp_type sfp_type;
	int ret;

	memset(&sfp_type, 0, sizeof(sfp_type));
	memset(&info, 0, sizeof(info));
	info.data = (uint8_t *)&sfp_type;
	info.length = sizeof(sfp_type);
	ret = hns3_get_module_eeprom(dev, &info);
	if (ret)
		return ret;

	switch (sfp_type.type) {
	case HNS3_SFF8024_ID_SFP:
		modinfo->type = RTE_ETH_MODULE_SFF_8472;
		modinfo->eeprom_len = RTE_ETH_MODULE_SFF_8472_LEN;
		break;
	case HNS3_SFF8024_ID_QSFP_8438:
		modinfo->type = RTE_ETH_MODULE_SFF_8436;
		modinfo->eeprom_len = RTE_ETH_MODULE_SFF_8436_MAX_LEN;
		break;
	case HNS3_SFF8024_ID_QSFP_8436_8636:
		if (sfp_type.ext_type < HNS3_SFF_8636_V1_3) {
			modinfo->type = RTE_ETH_MODULE_SFF_8436;
			modinfo->eeprom_len = RTE_ETH_MODULE_SFF_8436_MAX_LEN;
		} else {
			modinfo->type = RTE_ETH_MODULE_SFF_8636;
			modinfo->eeprom_len = RTE_ETH_MODULE_SFF_8636_MAX_LEN;
		}
		break;
	case HNS3_SFF8024_ID_QSFP28_8636:
		modinfo->type = RTE_ETH_MODULE_SFF_8636;
		modinfo->eeprom_len = RTE_ETH_MODULE_SFF_8636_MAX_LEN;
		break;
	default:
		hns3_err(hw, "unknown module, type = %u, extra_type = %u.",
			 sfp_type.type, sfp_type.ext_type);
		return -EINVAL;
	}

	return 0;
}

static const struct eth_dev_ops hns3_eth_dev_ops = {
	.dev_configure      = hns3_dev_configure,
	.dev_start          = hns3_dev_start,
	.dev_stop           = hns3_dev_stop,
	.dev_close          = hns3_dev_close,
	.promiscuous_enable = hns3_dev_promiscuous_enable,
	.promiscuous_disable = hns3_dev_promiscuous_disable,
	.allmulticast_enable  = hns3_dev_allmulticast_enable,
	.allmulticast_disable = hns3_dev_allmulticast_disable,
	.mtu_set            = hns3_dev_mtu_set,
	.stats_get          = hns3_stats_get,
	.stats_reset        = hns3_stats_reset,
	.xstats_get         = hns3_dev_xstats_get,
	.xstats_get_names   = hns3_dev_xstats_get_names,
	.xstats_reset       = hns3_dev_xstats_reset,
	.xstats_get_by_id   = hns3_dev_xstats_get_by_id,
	.xstats_get_names_by_id = hns3_dev_xstats_get_names_by_id,
	.dev_infos_get          = hns3_dev_infos_get,
	.fw_version_get         = hns3_fw_version_get,
	.rx_queue_setup         = hns3_rx_queue_setup,
	.tx_queue_setup         = hns3_tx_queue_setup,
	.rx_queue_release       = hns3_dev_rx_queue_release,
	.tx_queue_release       = hns3_dev_tx_queue_release,
	.rx_queue_start         = hns3_dev_rx_queue_start,
	.rx_queue_stop          = hns3_dev_rx_queue_stop,
	.tx_queue_start         = hns3_dev_tx_queue_start,
	.tx_queue_stop          = hns3_dev_tx_queue_stop,
	.rx_queue_intr_enable   = hns3_dev_rx_queue_intr_enable,
	.rx_queue_intr_disable  = hns3_dev_rx_queue_intr_disable,
	.rxq_info_get           = hns3_rxq_info_get,
	.txq_info_get           = hns3_txq_info_get,
	.rx_burst_mode_get      = hns3_rx_burst_mode_get,
	.tx_burst_mode_get      = hns3_tx_burst_mode_get,
	.flow_ctrl_get          = hns3_flow_ctrl_get,
	.flow_ctrl_set          = hns3_flow_ctrl_set,
	.priority_flow_ctrl_set = hns3_priority_flow_ctrl_set,
	.mac_addr_add           = hns3_add_mac_addr,
	.mac_addr_remove        = hns3_remove_mac_addr,
	.mac_addr_set           = hns3_set_default_mac_addr,
	.set_mc_addr_list       = hns3_set_mc_mac_addr_list,
	.link_update            = hns3_dev_link_update,
	.dev_set_link_up        = hns3_dev_set_link_up,
	.dev_set_link_down      = hns3_dev_set_link_down,
	.rss_hash_update        = hns3_dev_rss_hash_update,
	.rss_hash_conf_get      = hns3_dev_rss_hash_conf_get,
	.reta_update            = hns3_dev_rss_reta_update,
	.reta_query             = hns3_dev_rss_reta_query,
	.flow_ops_get           = hns3_dev_flow_ops_get,
	.vlan_filter_set        = hns3_vlan_filter_set,
	.vlan_tpid_set          = hns3_vlan_tpid_set,
	.vlan_offload_set       = hns3_vlan_offload_set,
	.vlan_pvid_set          = hns3_vlan_pvid_set,
	.get_reg                = hns3_get_regs,
	.get_module_info        = hns3_get_module_info,
	.get_module_eeprom      = hns3_get_module_eeprom,
	.get_dcb_info           = hns3_get_dcb_info,
	.dev_supported_ptypes_get = hns3_dev_supported_ptypes_get,
	.fec_get_capability     = hns3_fec_get_capability,
	.fec_get                = hns3_fec_get,
	.fec_set                = hns3_fec_set,
	.tm_ops_get             = hns3_tm_ops_get,
	.tx_done_cleanup        = hns3_tx_done_cleanup,
	.timesync_enable            = hns3_timesync_enable,
	.timesync_disable           = hns3_timesync_disable,
	.timesync_read_rx_timestamp = hns3_timesync_read_rx_timestamp,
	.timesync_read_tx_timestamp = hns3_timesync_read_tx_timestamp,
	.timesync_adjust_time       = hns3_timesync_adjust_time,
	.timesync_read_time         = hns3_timesync_read_time,
	.timesync_write_time        = hns3_timesync_write_time,
	.eth_dev_priv_dump          = hns3_eth_dev_priv_dump,
	.eth_rx_descriptor_dump     = hns3_rx_descriptor_dump,
	.eth_tx_descriptor_dump     = hns3_tx_descriptor_dump,
};

static const struct hns3_reset_ops hns3_reset_ops = {
	.reset_service       = hns3_reset_service,
	.stop_service        = hns3_stop_service,
	.prepare_reset       = hns3_prepare_reset,
	.wait_hardware_ready = hns3_wait_hardware_ready,
	.reinit_dev          = hns3_reinit_dev,
	.restore_conf	     = hns3_restore_conf,
	.start_service       = hns3_start_service,
};

static void
hns3_init_hw_ops(struct hns3_hw *hw)
{
	hw->ops.add_mc_mac_addr = hns3_add_mc_mac_addr;
	hw->ops.del_mc_mac_addr = hns3_remove_mc_mac_addr;
	hw->ops.add_uc_mac_addr = hns3_add_uc_mac_addr;
	hw->ops.del_uc_mac_addr = hns3_remove_uc_mac_addr;
	hw->ops.bind_ring_with_vector = hns3_bind_ring_with_vector;
}

static int
hns3_dev_init(struct rte_eth_dev *eth_dev)
{
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	PMD_INIT_FUNC_TRACE();

	hns3_flow_init(eth_dev);

	hns3_set_rxtx_function(eth_dev);
	eth_dev->dev_ops = &hns3_eth_dev_ops;
	eth_dev->rx_queue_count = hns3_rx_queue_count;
	ret = hns3_mp_init(eth_dev);
	if (ret)
		goto err_mp_init;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		hns3_tx_push_init(eth_dev);
		return 0;
	}

	hw->adapter_state = HNS3_NIC_UNINITIALIZED;
	hns->is_vf = false;
	hw->data = eth_dev->data;
	hns3_parse_devargs(eth_dev);

	/*
	 * Set default max packet size according to the mtu
	 * default vale in DPDK frame.
	 */
	hns->pf.mps = hw->data->mtu + HNS3_ETH_OVERHEAD;

	ret = hns3_reset_init(hw);
	if (ret)
		goto err_init_reset;
	hw->reset.ops = &hns3_reset_ops;

	hns3_init_hw_ops(hw);
	ret = hns3_init_pf(eth_dev);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init pf: %d", ret);
		goto err_init_pf;
	}

	ret = hns3_init_mac_addrs(eth_dev);
	if (ret != 0)
		goto err_init_mac_addrs;

	hw->adapter_state = HNS3_NIC_INITIALIZED;

	if (__atomic_load_n(&hw->reset.schedule, __ATOMIC_RELAXED) ==
			    SCHEDULE_PENDING) {
		hns3_err(hw, "Reschedule reset service after dev_init");
		hns3_schedule_reset(hns);
	} else {
		/* IMP will wait ready flag before reset */
		hns3_notify_reset_ready(hw, false);
	}

	hns3_info(hw, "hns3 dev initialization successful!");
	return 0;

err_init_mac_addrs:
	hns3_uninit_pf(eth_dev);

err_init_pf:
	rte_free(hw->reset.wait_data);

err_init_reset:
	hns3_mp_uninit(eth_dev);

err_mp_init:
	eth_dev->dev_ops = NULL;
	eth_dev->rx_pkt_burst = NULL;
	eth_dev->rx_descriptor_status = NULL;
	eth_dev->tx_pkt_burst = NULL;
	eth_dev->tx_pkt_prepare = NULL;
	eth_dev->tx_descriptor_status = NULL;
	return ret;
}

static int
hns3_dev_uninit(struct rte_eth_dev *eth_dev)
{
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;

	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		hns3_mp_uninit(eth_dev);
		return 0;
	}

	if (hw->adapter_state < HNS3_NIC_CLOSING)
		hns3_dev_close(eth_dev);

	hw->adapter_state = HNS3_NIC_REMOVED;
	return 0;
}

static int
eth_hns3_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		   struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
					     sizeof(struct hns3_adapter),
					     hns3_dev_init);
}

static int
eth_hns3_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, hns3_dev_uninit);
}

static const struct rte_pci_id pci_id_hns3_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HNS3_DEV_ID_GE) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HNS3_DEV_ID_25GE) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HNS3_DEV_ID_25GE_RDMA) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HNS3_DEV_ID_50GE_RDMA) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HNS3_DEV_ID_100G_RDMA_MACSEC) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HNS3_DEV_ID_200G_RDMA) },
	{ .vendor_id = 0, }, /* sentinel */
};

static struct rte_pci_driver rte_hns3_pmd = {
	.id_table = pci_id_hns3_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = eth_hns3_pci_probe,
	.remove = eth_hns3_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_hns3, rte_hns3_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_hns3, pci_id_hns3_map);
RTE_PMD_REGISTER_KMOD_DEP(net_hns3, "* igb_uio | vfio-pci");
RTE_PMD_REGISTER_PARAM_STRING(net_hns3,
		HNS3_DEVARG_RX_FUNC_HINT "=vec|sve|simple|common "
		HNS3_DEVARG_TX_FUNC_HINT "=vec|sve|simple|common "
		HNS3_DEVARG_DEV_CAPS_MASK "=<1-65535> "
		HNS3_DEVARG_MBX_TIME_LIMIT_MS "=<uint16> ");
RTE_LOG_REGISTER_SUFFIX(hns3_logtype_init, init, NOTICE);
RTE_LOG_REGISTER_SUFFIX(hns3_logtype_driver, driver, NOTICE);
#ifdef RTE_ETHDEV_DEBUG_RX
RTE_LOG_REGISTER_SUFFIX(hns3_logtype_rx, rx, DEBUG);
#endif
#ifdef RTE_ETHDEV_DEBUG_TX
RTE_LOG_REGISTER_SUFFIX(hns3_logtype_tx, tx, DEBUG);
#endif
