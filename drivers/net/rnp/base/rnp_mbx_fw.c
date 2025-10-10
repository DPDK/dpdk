/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#include <strings.h>

#include "rnp_mbx_fw.h"
#include "rnp_fw_cmd.h"
#include "rnp_mbx.h"
#include "../rnp.h"

#define RNP_MBX_API_MAX_RETRY	(10)
#define RNP_POLL_WAIT_MS	(10)

static int rnp_mbx_fw_post_req(struct rnp_eth_port *port,
			       struct rnp_mbx_fw_cmd_req *req,
			       struct rnp_mbx_req_cookie *cookie)
{
	const struct rnp_mbx_ops *ops = RNP_DEV_PP_TO_MBX_OPS(port->eth_dev);
	struct rnp_hw *hw = port->hw;
	u32 timeout_cnt;
	int err = 0;

	cookie->done = 0;

	spin_lock(&hw->fw_info.fw_lock);

	/* down_interruptible(&pf_cpu_lock); */
	err = ops->write(hw, (u32 *)req,
			(req->datalen + RNP_MBX_REQ_HDR_LEN) / 4, RNP_MBX_FW);
	if (err) {
		RNP_PMD_LOG(ERR, "rnp_write_mbx failed!");
		goto quit;
	}

	timeout_cnt = cookie->timeout_ms / RNP_POLL_WAIT_MS;
	while (timeout_cnt > 0) {
		mdelay(RNP_POLL_WAIT_MS);
		timeout_cnt--;
		if (cookie->done)
			break;
	}
quit:
	spin_unlock(&hw->fw_info.fw_lock);
	return err;
}

static int
rnp_fw_send_cmd_wait(struct rnp_eth_port *port,
		     struct rnp_mbx_fw_cmd_req *req,
		     struct rnp_mbx_fw_cmd_reply *reply)
{
	const struct rnp_mbx_ops *ops = RNP_DEV_PP_TO_MBX_OPS(port->eth_dev);
	struct rnp_hw *hw = port->hw;
	u16 try_count = 0;
	int err = 0;

	if (ops == NULL || ops->write_posted == NULL)
		return -EINVAL;
	spin_lock(&hw->fw_info.fw_lock);
	err = ops->write_posted(hw, (u32 *)req,
			(req->datalen + RNP_MBX_REQ_HDR_LEN) / 4, RNP_MBX_FW);
	if (err) {
		RNP_PMD_LOG(ERR, "%s: write_posted failed!"
				" err:0x%x", __func__, err);
		spin_unlock(&hw->fw_info.fw_lock);
		return err;
	}
	/* ignore non target information */
fw_api_try:
	err = ops->read_posted(hw, (u32 *)reply,
			sizeof(*reply) / 4, RNP_MBX_FW);
	if (err) {
		RNP_PMD_LOG(ERR,
				"%s: read_posted failed! err:0x%x"
				" req-op:0x%x",
				__func__,
				err,
				req->opcode);
		goto err_quit;
	}
	if (req->opcode != reply->opcode) {
		try_count++;
		if (try_count < RNP_MBX_API_MAX_RETRY)
			goto fw_api_try;
		RNP_PMD_LOG(ERR,
				"%s: read reply msg failed! err:0x%x"
				" req-op:0x%x",
				__func__,
				err,
				req->opcode);
		err = -EIO;
	}
	if (reply->error_code) {
		RNP_PMD_LOG(ERR,
				"%s: reply err:0x%x. req-op:0x%x",
				__func__,
				reply->error_code,
				req->opcode);
		err = -reply->error_code;
		goto err_quit;
	}
	spin_unlock(&hw->fw_info.fw_lock);

	return err;
err_quit:

	spin_unlock(&hw->fw_info.fw_lock);
	RNP_PMD_LOG(ERR,
			"%s:PF[%d]: req:%08x_%08x_%08x_%08x "
			"reply:%08x_%08x_%08x_%08x",
			__func__,
			hw->mbx.pf_num,
			((int *)req)[0],
			((int *)req)[1],
			((int *)req)[2],
			((int *)req)[3],
			((int *)reply)[0],
			((int *)reply)[1],
			((int *)reply)[2],
			((int *)reply)[3]);

	return err;
}

static int
rnp_fw_send_norep_cmd(struct rnp_eth_port *port,
		      struct rnp_fw_req_arg *arg)
{
	const struct rnp_mbx_ops *ops = RNP_DEV_PP_TO_MBX_OPS(port->eth_dev);
	struct rnp_mbx_fw_cmd_req req;
	struct rnp_hw *hw = port->hw;
	int err = 0;

	if (ops == NULL || ops->write_posted == NULL)
		return -EINVAL;
	memset(&req, 0, sizeof(req));
	spin_lock(&hw->fw_info.fw_lock);
	rnp_build_fwcmd_req(&req, arg, &req);
	err = ops->write_posted(hw, (u32 *)&req,
			(req.datalen + RNP_MBX_REQ_HDR_LEN) / 4, RNP_MBX_FW);
	if (err) {
		RNP_PMD_LOG(ERR, "%s: write_posted failed!"
				" err:0x%x", __func__, err);
		spin_unlock(&hw->fw_info.fw_lock);
		return err;
	}
	spin_unlock(&hw->fw_info.fw_lock);

	return 0;
}

static int
rnp_fw_send_cmd(struct rnp_eth_port *port,
		struct rnp_fw_req_arg *arg,
		void *respond)
{
	struct rnp_mbx_fw_cmd_reply reply;
	struct rnp_mbx_fw_cmd_req req;
	struct rnp_hw *hw = port->hw;
	int err = 0;

	memset(&req, 0, sizeof(req));
	memset(&reply, 0, sizeof(reply));
	if (hw->fw_info.fw_irq_en) {
		struct rnp_mbx_req_cookie *cookie = rnp_dma_mem_alloc(hw,
				&hw->fw_info.mem, sizeof(*cookie),
				hw->fw_info.cookie_name);
		if (!cookie)
			return -ENOMEM;
		memset(cookie->priv, 0, cookie->priv_len);
		rnp_build_fwcmd_req(&req, arg, cookie);
		err = rnp_mbx_fw_post_req(port, &req, cookie);
		if (err)
			return err;
		if (respond)
			memcpy(respond, cookie->priv, sizeof(cookie->priv));
	} else {
		rnp_build_fwcmd_req(&req, arg, &req);
		err = rnp_fw_send_cmd_wait(port, &req, &reply);
		if (err)
			return err;
		if (respond)
			memcpy(respond, reply.data, RNP_FW_REP_DATA_NUM);
	}

	return 0;
}

int rnp_fw_init(struct rnp_hw *hw)
{
	struct rnp_fw_info *fw_info = &hw->fw_info;
	struct rnp_mbx_req_cookie *cookie = NULL;

	snprintf(fw_info->cookie_name, RTE_MEMZONE_NAMESIZE,
			"fw_req_cookie_%s",
			hw->device_name);
	fw_info->cookie_pool = rnp_dma_mem_alloc(hw, &fw_info->mem,
			sizeof(struct rnp_mbx_req_cookie),
			fw_info->cookie_name);
	cookie = (struct rnp_mbx_req_cookie *)fw_info->cookie_pool;
	if (cookie == NULL)
		return -ENOMEM;
	cookie->timeout_ms = 1000;
	cookie->magic = RNP_COOKIE_MAGIC;
	cookie->priv_len = RNP_MAX_SHARE_MEM;
	spin_lock_init(&fw_info->fw_lock);
	fw_info->fw_irq_en = false;

	return 0;
}

static int
rnp_fw_get_phy_capability(struct rnp_eth_port *port,
			  struct rnp_phy_abilities_rep *abil)
{
	u8 data[RNP_FW_REP_DATA_NUM] = {0};
	struct rnp_fw_req_arg arg;
	int err;

	RTE_BUILD_BUG_ON(sizeof(*abil) != RNP_FW_REP_DATA_NUM);

	memset(&arg, 0, sizeof(arg));
	arg.opcode = RNP_GET_PHY_ABALITY;
	err = rnp_fw_send_cmd(port, &arg, &data);
	if (err)
		return err;
	memcpy(abil, &data, sizeof(*abil));

	return 0;
}

#define RNP_MAX_LANE_MASK	(0xf)
int rnp_mbx_fw_get_capability(struct rnp_eth_port *port)
{
	struct rnp_phy_abilities_rep ability;
	u8 port_ids[4] = {0, 0, 0, 0};
	struct rnp_hw *hw = port->hw;
	u32 is_sgmii_bits = 0;
	bool is_sgmii = false;
	u16 lane_bit = 0;
	u32 lane_cnt = 0;
	int err = -EIO;
	u16 temp_mask;
	u8 lane_idx;
	u32 idx;

	memset(&ability, 0, sizeof(ability));
	err = rnp_fw_get_phy_capability(port, &ability);
	if (!err) {
		memcpy(&port_ids, &ability.port_ids, 4);
		hw->lane_mask = ability.lane_mask;
		hw->nic_mode = ability.nic_mode;
		/* get phy<->lane mapping info */
		lane_cnt = rte_popcount32(hw->lane_mask);
		if (lane_cnt > RNP_MAX_PORT_OF_PF) {
			RNP_PMD_LOG(ERR, "firmware invalid lane_mask");
			return -EINVAL;
		}
		temp_mask = hw->lane_mask;
		if (temp_mask == 0 || temp_mask > RNP_MAX_LANE_MASK) {
			RNP_PMD_LOG(ERR, "lane_mask is invalid 0x%.2x", temp_mask);
			return -EINVAL;
		}
		if (ability.e.ports_is_sgmii_valid)
			is_sgmii_bits = ability.e.lane_is_sgmii;
		for (idx = 0; idx < lane_cnt; idx++) {
			hw->phy_port_ids[idx] = port_ids[idx];
			if (temp_mask == 0) {
				RNP_PMD_LOG(ERR, "temp_mask is zero at idx=%d", idx);
				return -EINVAL;
			}
			lane_bit = rte_ffs32(temp_mask) - 1;
			lane_idx = port_ids[idx] % lane_cnt;
			hw->lane_of_port[lane_idx] = lane_bit;
			is_sgmii = lane_bit & is_sgmii_bits ? 1 : 0;
			hw->lane_is_sgmii[lane_idx] = is_sgmii;
			temp_mask &= ~(1ULL << lane_bit);
		}
		hw->max_port_num = lane_cnt;
	}
	if (lane_cnt <= 0 || lane_cnt > 4)
		return -EIO;

	RNP_PMD_LOG(INFO, "nic-mode:%u lane_cnt:%u lane_mask:%x"
			  " pfvfnum:%x, fw_version:0x%08x,"
			  " ports:%u-%u-%u-%u ncsi_en:%u",
			  hw->nic_mode,
			  lane_cnt,
			  hw->lane_mask,
			  hw->pf_vf_num,
			  ability.fw_version,
			  port_ids[0],
			  port_ids[1],
			  port_ids[2],
			  port_ids[3],
			  ability.e.ncsi_en);

	return err;
}

int rnp_mbx_fw_reset_phy(struct rnp_hw *hw)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(hw->back->eth_dev);
	struct rnp_fw_req_arg arg;
	int err;

	memset(&arg, 0, sizeof(arg));
	arg.opcode = RNP_RESET_PHY;
	err = rnp_fw_send_cmd(port, &arg, NULL);
	if (err) {
		RNP_PMD_LOG(ERR, "%s: failed. err:%d", __func__, err);
		return err;
	}

	return 0;
}

int
rnp_mbx_fw_get_macaddr(struct rnp_eth_port *port,
		       u8 *mac_addr)
{
	u8 data[RNP_FW_REP_DATA_NUM] = {0};
	u32 nr_lane = port->attr.nr_lane;
	struct rnp_mac_addr_rep *mac;
	struct rnp_fw_req_arg arg;
	int err;

	if (!mac_addr)
		return -EINVAL;
	RTE_BUILD_BUG_ON(sizeof(*mac) != RNP_FW_REP_DATA_NUM);
	memset(&arg, 0, sizeof(arg));
	mac = (struct rnp_mac_addr_rep *)&data;
	arg.opcode = RNP_GET_MAC_ADDRESS;
	arg.param0 = nr_lane;
	arg.param1 = port->hw->pf_vf_num;

	err = rnp_fw_send_cmd(port, &arg, &data);
	if (err) {
		RNP_PMD_LOG(ERR, "%s: failed. err:%d", __func__, err);
		return err;
	}
	if (RTE_BIT32(nr_lane) & mac->lanes) {
		memcpy(mac_addr, mac->addrs[nr_lane].mac,
				sizeof(mac->addrs[0].mac));
		return 0;
	}

	return -ENOMSG;
}

int
rnp_mbx_fw_get_lane_stat(struct rnp_eth_port *port)
{
	struct rnp_phy_meta *phy_meta = &port->attr.phy_meta;
	u8 data[RNP_FW_REP_DATA_NUM] = {0};
	struct rnp_lane_stat_rep *lane_stat;
	u32 nr_lane = port->attr.nr_lane;
	struct rnp_fw_req_arg arg;
	u32 user_set_speed = 0;
	int err;

	RTE_BUILD_BUG_ON(sizeof(*lane_stat) != RNP_FW_REP_DATA_NUM);
	memset(&arg, 0, sizeof(arg));
	lane_stat = (struct rnp_lane_stat_rep *)&data;
	arg.opcode = RNP_GET_LANE_STATUS;
	arg.param0 = nr_lane;

	err = rnp_fw_send_cmd(port, &arg, &data);
	if (err) {
		RNP_PMD_LOG(ERR, "%s: failed. err:%d", __func__, err);
		return err;
	}
	phy_meta->supported_link = lane_stat->supported_link;
	phy_meta->is_backplane = lane_stat->is_backplane;
	phy_meta->phy_identifier = lane_stat->phy_addr;
	phy_meta->link_autoneg = lane_stat->autoneg;
	phy_meta->link_duplex = lane_stat->duplex;
	phy_meta->phy_type = lane_stat->phy_type;
	phy_meta->is_sgmii = lane_stat->is_sgmii;
	phy_meta->fec = lane_stat->fec;

	if (phy_meta->is_sgmii) {
		phy_meta->media_type = RNP_MEDIA_TYPE_COPPER;
		phy_meta->supported_link |=
			RNP_SPEED_CAP_100M_HALF | RNP_SPEED_CAP_10M_HALF;
	} else if (phy_meta->is_backplane) {
		phy_meta->media_type = RNP_MEDIA_TYPE_BACKPLANE;
	} else {
		phy_meta->media_type = RNP_MEDIA_TYPE_FIBER;
	}
	if (phy_meta->phy_type == RNP_PHY_TYPE_10G_TP) {
		phy_meta->supported_link |= RNP_SPEED_CAP_1GB_FULL;
		phy_meta->supported_link |= RNP_SPEED_CAP_10GB_FULL;
	}
	if (!phy_meta->link_autoneg &&
	    phy_meta->media_type == RNP_MEDIA_TYPE_COPPER) {
		/* firmware don't support upload info just use user info */
		user_set_speed = port->eth_dev->data->dev_conf.link_speeds;
		if (!(user_set_speed & RTE_ETH_LINK_SPEED_FIXED))
			phy_meta->link_autoneg = 1;
	}

	return 0;
}

static void
rnp_link_sync_init(struct rnp_hw *hw, bool en)
{
	RNP_E_REG_WR(hw, RNP_FW_LINK_SYNC, en ? RNP_LINK_MAGIC_CODE : 0);
}

int
rnp_mbx_fw_pf_link_event_en(struct rnp_eth_port *port, bool en)
{
	struct rnp_eth_adapter *adapter = NULL;
	struct rnp_hw *hw = port->hw;
	struct rnp_fw_req_arg arg;
	int err;

	adapter = hw->back;
	memset(&arg, 0, sizeof(arg));
	arg.opcode = RNP_SET_EVENT_MASK;
	arg.param0 = RNP_FW_EVENT_LINK_UP;
	arg.param1 = !!en;

	err = rnp_fw_send_norep_cmd(port, &arg);
	if (err) {
		RNP_PMD_LOG(ERR, "%s: failed. err:%d", __func__, err);
		return err;
	}
	rnp_link_sync_init(hw, en);
	adapter->intr_registered = en;
	hw->fw_info.fw_irq_en = en;

	return 0;
}

int
rnp_mbx_fw_lane_link_event_en(struct rnp_eth_port *port, bool en)
{
	u16 nr_lane = port->attr.nr_lane;
	struct rnp_fw_req_arg arg;
	int err;

	memset(&arg, 0, sizeof(arg));
	arg.opcode = RNP_SET_LANE_EVENT_EN;
	arg.param0 = nr_lane;
	arg.param1 = RNP_FW_EVENT_LINK_UP;
	arg.param2 = !!en;

	err = rnp_fw_send_norep_cmd(port, &arg);
	if (err) {
		RNP_PMD_LOG(ERR, "%s: failed. err:%d", __func__, err);
		return err;
	}

	return 0;
}

int
rnp_rcv_msg_from_fw(struct rnp_eth_adapter *adapter, u32 *msgbuf)
{
	const struct rnp_mbx_ops *ops = RNP_DEV_PP_TO_MBX_OPS(adapter->eth_dev);
	struct rnp_hw *hw = &adapter->hw;
	int retval;

	retval = ops->read(hw, msgbuf, RNP_MBX_MSG_BLOCK_SIZE, RNP_MBX_FW);
	if (retval) {
		RNP_PMD_ERR("Error receiving message from FW");
		return retval;
	}

	return 0;
}

static void rnp_link_stat_reset(struct rnp_hw *hw, u16 lane)
{
	u32 state;

	spin_lock(&hw->link_sync);
	state = RNP_E_REG_RD(hw, RNP_FW_LINK_SYNC);
	state &= ~RNP_LINK_MAGIC_MASK;
	state |= RNP_LINK_MAGIC_CODE;
	state &= ~RTE_BIT32(lane);

	RNP_E_REG_WR(hw, RNP_FW_LINK_SYNC, state);
	rte_spinlock_unlock(&hw->link_sync);
}

int rnp_mbx_fw_ifup_down(struct rnp_eth_port *port, bool up)
{
	u16 nr_lane = port->attr.nr_lane;
	struct rnp_hw *hw = port->hw;
	struct rnp_fw_req_arg arg;
	int err;

	memset(&arg, 0, sizeof(arg));
	arg.opcode = RNP_IFUP_DOWN;
	arg.param0 = nr_lane;
	arg.param1 = up;

	err = rnp_fw_send_norep_cmd(port, &arg);
	/* force firmware send irq event to dpdk */
	if (!err && up)
		rnp_link_stat_reset(hw, nr_lane);
	return err;
}
