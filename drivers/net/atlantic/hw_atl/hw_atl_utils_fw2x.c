// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
/* Copyright (C) 2014-2017 aQuantia Corporation. */

/* File hw_atl_utils_fw2x.c: Definition of firmware 2.x functions for
 * Atlantic hardware abstraction layer.
 */

#include <rte_ether.h>
#include <pthread.h>
#include "../atl_hw_regs.h"

#include "../atl_types.h"
#include "hw_atl_utils.h"
#include "hw_atl_llh.h"

#define HW_ATL_FW2X_MPI_EFUSE_ADDR	0x364
#define HW_ATL_FW2X_MPI_MBOX_ADDR	0x360
#define HW_ATL_FW2X_MPI_RPC_ADDR	0x334

#define HW_ATL_FW2X_MPI_CONTROL_ADDR	0x368
#define HW_ATL_FW2X_MPI_CONTROL2_ADDR	0x36C
#define HW_ATL_FW2X_MPI_LED_ADDR	0x31c

#define HW_ATL_FW2X_MPI_STATE_ADDR	0x370
#define HW_ATL_FW2X_MPI_STATE2_ADDR	0x374

#define HW_ATL_FW2X_CAP_SLEEP_PROXY BIT(CAPS_HI_SLEEP_PROXY)
#define HW_ATL_FW2X_CAP_WOL BIT(CAPS_HI_WOL)

#define HW_ATL_FW2X_CAP_EEE_1G_MASK   BIT(CAPS_HI_1000BASET_FD_EEE)
#define HW_ATL_FW2X_CAP_EEE_2G5_MASK  BIT(CAPS_HI_2P5GBASET_FD_EEE)
#define HW_ATL_FW2X_CAP_EEE_5G_MASK   BIT(CAPS_HI_5GBASET_FD_EEE)
#define HW_ATL_FW2X_CAP_EEE_10G_MASK  BIT(CAPS_HI_10GBASET_FD_EEE)

#define HAL_ATLANTIC_WOL_FILTERS_COUNT     8
#define HAL_ATLANTIC_UTILS_FW2X_MSG_WOL    0x0E

#define HW_ATL_FW_FEATURE_LED 0x03010026

struct fw2x_msg_wol_pattern {
	u8 mask[16];
	u32 crc;
} __rte_packed;

struct fw2x_msg_wol {
	u32 msg_id;
	u8 hw_addr[6];
	u8 magic_packet_enabled;
	u8 filter_count;
	struct fw2x_msg_wol_pattern filter[HAL_ATLANTIC_WOL_FILTERS_COUNT];
	u8 link_up_enabled;
	u8 link_down_enabled;
	u16 reserved;
	u32 link_up_timeout;
	u32 link_down_timeout;
} __rte_packed;

static int aq_fw2x_set_link_speed(struct aq_hw_s *self, u32 speed);
static int aq_fw2x_set_state(struct aq_hw_s *self,
			     enum hal_atl_utils_fw_state_e state);

static int aq_fw2x_init(struct aq_hw_s *self)
{
	int err = 0;
	struct hw_aq_atl_utils_mbox mbox;

	/* check 10 times by 1ms */
	AQ_HW_WAIT_FOR(0U != (self->mbox_addr =
		       aq_hw_read_reg(self, HW_ATL_FW2X_MPI_MBOX_ADDR)),
		       1000U, 10U);
	AQ_HW_WAIT_FOR(0U != (self->rpc_addr =
		       aq_hw_read_reg(self, HW_ATL_FW2X_MPI_RPC_ADDR)),
		       1000U, 100U);

	/* Read caps */
	hw_atl_utils_mpi_read_stats(self, &mbox);

	self->caps_lo = mbox.info.caps_lo;

	return err;
}

static int aq_fw2x_deinit(struct aq_hw_s *self)
{
	int err = aq_fw2x_set_link_speed(self, 0);

	if (!err)
		err = aq_fw2x_set_state(self, MPI_DEINIT);

	return err;
}

static enum hw_atl_fw2x_rate link_speed_mask_2fw2x_ratemask(u32 speed)
{
	enum hw_atl_fw2x_rate rate = 0;

	if (speed & AQ_NIC_RATE_10G)
		rate |= FW2X_RATE_10G;

	if (speed & AQ_NIC_RATE_5G)
		rate |= FW2X_RATE_5G;

	if (speed & AQ_NIC_RATE_5G5R)
		rate |= FW2X_RATE_5G;

	if (speed & AQ_NIC_RATE_2G5)
		rate |= FW2X_RATE_2G5;

	if (speed & AQ_NIC_RATE_1G)
		rate |= FW2X_RATE_1G;

	if (speed & AQ_NIC_RATE_100M)
		rate |= FW2X_RATE_100M;

	return rate;
}

static u32 fw2x_to_eee_mask(u32 speed)
{
	u32 rate = 0;

	if (speed & HW_ATL_FW2X_CAP_EEE_10G_MASK)
		rate |= AQ_NIC_RATE_EEE_10G;

	if (speed & HW_ATL_FW2X_CAP_EEE_5G_MASK)
		rate |= AQ_NIC_RATE_EEE_5G;

	if (speed & HW_ATL_FW2X_CAP_EEE_2G5_MASK)
		rate |= AQ_NIC_RATE_EEE_2G5;

	if (speed & HW_ATL_FW2X_CAP_EEE_1G_MASK)
		rate |= AQ_NIC_RATE_EEE_1G;

	return rate;
}

static int aq_fw2x_set_link_speed(struct aq_hw_s *self, u32 speed)
{
	u32 rate_mask = link_speed_mask_2fw2x_ratemask(speed);
	u32 reg_val = aq_hw_read_reg(self, HW_ATL_FW2X_MPI_CONTROL_ADDR);
	u32 val = rate_mask | ((BIT(CAPS_LO_SMBUS_READ) |
				BIT(CAPS_LO_SMBUS_WRITE) |
				BIT(CAPS_LO_MACSEC)) & reg_val);

	aq_hw_write_reg(self, HW_ATL_FW2X_MPI_CONTROL_ADDR, val);

	return 0;
}

static void aq_fw2x_set_mpi_flow_control(struct aq_hw_s *self, u32 *mpi_state)
{
	if (self->aq_nic_cfg->flow_control & AQ_NIC_FC_RX)
		*mpi_state |= BIT(CAPS_HI_PAUSE);
	else
		*mpi_state &= ~BIT(CAPS_HI_PAUSE);

	if (self->aq_nic_cfg->flow_control & AQ_NIC_FC_TX)
		*mpi_state |= BIT(CAPS_HI_ASYMMETRIC_PAUSE);
	else
		*mpi_state &= ~BIT(CAPS_HI_ASYMMETRIC_PAUSE);
}

static int aq_fw2x_set_state(struct aq_hw_s *self,
			     enum hal_atl_utils_fw_state_e state)
{
	u32 mpi_state = aq_hw_read_reg(self, HW_ATL_FW2X_MPI_CONTROL2_ADDR);

	switch (state) {
	case MPI_INIT:
		mpi_state &= ~BIT(CAPS_HI_LINK_DROP);
		aq_fw2x_set_mpi_flow_control(self, &mpi_state);
		break;
	case MPI_DEINIT:
		mpi_state |= BIT(CAPS_HI_LINK_DROP);
		break;
	case MPI_RESET:
	case MPI_POWER:
		/* No actions */
		break;
	}
	aq_hw_write_reg(self, HW_ATL_FW2X_MPI_CONTROL2_ADDR, mpi_state);
	return 0;
}

static int aq_fw2x_update_link_status(struct aq_hw_s *self)
{
	u32 mpi_state = aq_hw_read_reg(self, HW_ATL_FW2X_MPI_STATE_ADDR);
	u32 speed = mpi_state & (FW2X_RATE_100M | FW2X_RATE_1G |
				FW2X_RATE_2G5 | FW2X_RATE_5G | FW2X_RATE_10G);
	struct aq_hw_link_status_s *link_status = &self->aq_link_status;

	if (speed) {
		if (speed & FW2X_RATE_10G)
			link_status->mbps = 10000;
		else if (speed & FW2X_RATE_5G)
			link_status->mbps = 5000;
		else if (speed & FW2X_RATE_2G5)
			link_status->mbps = 2500;
		else if (speed & FW2X_RATE_1G)
			link_status->mbps = 1000;
		else if (speed & FW2X_RATE_100M)
			link_status->mbps = 100;
		else
			link_status->mbps = 10000;
	} else {
		link_status->mbps = 0;
	}

	return 0;
}

static
int aq_fw2x_get_mac_permanent(struct aq_hw_s *self, u8 *mac)
{
	int err = 0;
	u32 h = 0U;
	u32 l = 0U;
	u32 mac_addr[2] = { 0 };
	u32 efuse_addr = aq_hw_read_reg(self, HW_ATL_FW2X_MPI_EFUSE_ADDR);

	pthread_mutex_lock(&self->mbox_mutex);

	if (efuse_addr != 0) {
		err = hw_atl_utils_fw_downld_dwords(self,
						    efuse_addr + (40U * 4U),
						    mac_addr,
						    ARRAY_SIZE(mac_addr));
		if (err)
			goto exit;
		mac_addr[0] = rte_constant_bswap32(mac_addr[0]);
		mac_addr[1] = rte_constant_bswap32(mac_addr[1]);
	}

	rte_ether_addr_copy((struct rte_ether_addr *)mac_addr,
			(struct rte_ether_addr *)mac);

	if ((mac[0] & 0x01U) || ((mac[0] | mac[1] | mac[2]) == 0x00U)) {
		unsigned int rnd = (uint32_t)rte_rand();

		//get_random_bytes(&rnd, sizeof(unsigned int));

		l = 0xE3000000U
			| (0xFFFFU & rnd)
			| (0x00 << 16);
		h = 0x8001300EU;

		mac[5] = (u8)(0xFFU & l);
		l >>= 8;
		mac[4] = (u8)(0xFFU & l);
		l >>= 8;
		mac[3] = (u8)(0xFFU & l);
		l >>= 8;
		mac[2] = (u8)(0xFFU & l);
		mac[1] = (u8)(0xFFU & h);
		h >>= 8;
		mac[0] = (u8)(0xFFU & h);
	}

exit:
	pthread_mutex_unlock(&self->mbox_mutex);

	return err;
}

static int aq_fw2x_update_stats(struct aq_hw_s *self)
{
	int err = 0;
	u32 mpi_opts = aq_hw_read_reg(self, HW_ATL_FW2X_MPI_CONTROL2_ADDR);
	u32 orig_stats_val = mpi_opts & BIT(CAPS_HI_STATISTICS);


	pthread_mutex_lock(&self->mbox_mutex);

	/* Toggle statistics bit for FW to update */
	mpi_opts = mpi_opts ^ BIT(CAPS_HI_STATISTICS);
	aq_hw_write_reg(self, HW_ATL_FW2X_MPI_CONTROL2_ADDR, mpi_opts);

	/* Wait FW to report back */
	AQ_HW_WAIT_FOR(orig_stats_val !=
		       (aq_hw_read_reg(self, HW_ATL_FW2X_MPI_STATE2_ADDR) &
				       BIT(CAPS_HI_STATISTICS)),
		       1U, 10000U);
	if (err)
		goto exit;

	err = hw_atl_utils_update_stats(self);

exit:
	pthread_mutex_unlock(&self->mbox_mutex);

	return err;

}

static int aq_fw2x_get_temp(struct aq_hw_s *self, int *temp)
{
	int err = 0;
	u32 mpi_opts = aq_hw_read_reg(self, HW_ATL_FW2X_MPI_CONTROL2_ADDR);
	u32 temp_val = mpi_opts & BIT(CAPS_HI_TEMPERATURE);
	u32 temp_res;

	pthread_mutex_lock(&self->mbox_mutex);

	/* Toggle statistics bit for FW to 0x36C.18 (CAPS_HI_TEMPERATURE) */
	mpi_opts = mpi_opts ^ BIT(CAPS_HI_TEMPERATURE);
	aq_hw_write_reg(self, HW_ATL_FW2X_MPI_CONTROL2_ADDR, mpi_opts);

	/* Wait FW to report back */
	AQ_HW_WAIT_FOR(temp_val !=
			(aq_hw_read_reg(self, HW_ATL_FW2X_MPI_STATE2_ADDR) &
					BIT(CAPS_HI_TEMPERATURE)), 1U, 10000U);
	err = hw_atl_utils_fw_downld_dwords(self,
				self->mbox_addr +
				offsetof(struct hw_aq_atl_utils_mbox, info) +
				offsetof(struct hw_aq_info, phy_temperature),
				&temp_res,
				sizeof(temp_res) / sizeof(u32));


	pthread_mutex_unlock(&self->mbox_mutex);

	if (err)
		return err;

	*temp = temp_res  * 100 / 256;
	return 0;
}

static int aq_fw2x_get_cable_len(struct aq_hw_s *self, int *cable_len)
{
	int err = 0;
	u32 cable_len_res;

	err = hw_atl_utils_fw_downld_dwords(self,
				self->mbox_addr +
				offsetof(struct hw_aq_atl_utils_mbox, info) +
				offsetof(struct hw_aq_info, phy_temperature),
				&cable_len_res,
				sizeof(cable_len_res) / sizeof(u32));

	if (err)
		return err;

	*cable_len = (cable_len_res >> 16) & 0xFF;
	return 0;
}

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

static int aq_fw2x_set_sleep_proxy(struct aq_hw_s *self, u8 *mac)
{
	int err = 0;
	struct hw_aq_atl_utils_fw_rpc *rpc = NULL;
	struct offload_info *cfg = NULL;
	unsigned int rpc_size = 0U;
	u32 mpi_opts;

	rpc_size = sizeof(rpc->msg_id) + sizeof(*cfg);

	err = hw_atl_utils_fw_rpc_wait(self, &rpc);
	if (err < 0)
		goto err_exit;

	memset(rpc, 0, rpc_size);
	cfg = (struct offload_info *)(&rpc->msg_id + 1);

	memcpy(cfg->mac_addr, mac, ETH_ALEN);
	cfg->len = sizeof(*cfg);

	/* Clear bit 0x36C.23 */
	mpi_opts = aq_hw_read_reg(self, HW_ATL_FW2X_MPI_CONTROL2_ADDR);
	mpi_opts &= ~HW_ATL_FW2X_CAP_SLEEP_PROXY;

	aq_hw_write_reg(self, HW_ATL_FW2X_MPI_CONTROL2_ADDR, mpi_opts);

	err = hw_atl_utils_fw_rpc_call(self, rpc_size);
	if (err < 0)
		goto err_exit;

	/* Set bit 0x36C.23 */
	mpi_opts |= HW_ATL_FW2X_CAP_SLEEP_PROXY;
	aq_hw_write_reg(self, HW_ATL_FW2X_MPI_CONTROL2_ADDR, mpi_opts);

	AQ_HW_WAIT_FOR((aq_hw_read_reg(self, HW_ATL_FW2X_MPI_STATE2_ADDR) &
			HW_ATL_FW2X_CAP_SLEEP_PROXY), 1U, 10000U);
err_exit:
	return err;
}

static int aq_fw2x_set_wol_params(struct aq_hw_s *self, u8 *mac)
{
	int err = 0;
	struct fw2x_msg_wol *msg = NULL;
	u32 mpi_opts;

	struct hw_aq_atl_utils_fw_rpc *rpc = NULL;

	err = hw_atl_utils_fw_rpc_wait(self, &rpc);
	if (err < 0)
		goto err_exit;

	msg = (struct fw2x_msg_wol *)rpc;

	msg->msg_id = HAL_ATLANTIC_UTILS_FW2X_MSG_WOL;
	msg->magic_packet_enabled = true;
	memcpy(msg->hw_addr, mac, ETH_ALEN);

	mpi_opts = aq_hw_read_reg(self, HW_ATL_FW2X_MPI_CONTROL2_ADDR);
	mpi_opts &= ~(HW_ATL_FW2X_CAP_SLEEP_PROXY | HW_ATL_FW2X_CAP_WOL);

	aq_hw_write_reg(self, HW_ATL_FW2X_MPI_CONTROL2_ADDR, mpi_opts);

	err = hw_atl_utils_fw_rpc_call(self, sizeof(*msg));
	if (err < 0)
		goto err_exit;

	/* Set bit 0x36C.24 */
	mpi_opts |= HW_ATL_FW2X_CAP_WOL;
	aq_hw_write_reg(self, HW_ATL_FW2X_MPI_CONTROL2_ADDR, mpi_opts);

	AQ_HW_WAIT_FOR((aq_hw_read_reg(self, HW_ATL_FW2X_MPI_STATE2_ADDR) &
			HW_ATL_FW2X_CAP_WOL), 1U, 10000U);
err_exit:
	return err;
}

static int aq_fw2x_set_power(struct aq_hw_s *self,
			     unsigned int power_state __rte_unused,
			     u8 *mac)
{
	int err = 0;

	if (self->aq_nic_cfg->wol & AQ_NIC_WOL_ENABLED) {
		err = aq_fw2x_set_sleep_proxy(self, mac);
		if (err < 0)
			goto err_exit;
		err = aq_fw2x_set_wol_params(self, mac);
		if (err < 0)
			goto err_exit;
	}
err_exit:
	return err;
}

static int aq_fw2x_set_eee_rate(struct aq_hw_s *self, u32 speed)
{
	u32 mpi_opts = aq_hw_read_reg(self, HW_ATL_FW2X_MPI_CONTROL2_ADDR);
	mpi_opts &= ~(HW_ATL_FW2X_CAP_EEE_1G_MASK |
		HW_ATL_FW2X_CAP_EEE_2G5_MASK | HW_ATL_FW2X_CAP_EEE_5G_MASK |
		HW_ATL_FW2X_CAP_EEE_10G_MASK);

	if (speed & AQ_NIC_RATE_EEE_10G)
		mpi_opts |= HW_ATL_FW2X_CAP_EEE_10G_MASK;

	if (speed & AQ_NIC_RATE_EEE_5G)
		mpi_opts |= HW_ATL_FW2X_CAP_EEE_5G_MASK;

	if (speed & AQ_NIC_RATE_EEE_2G5)
		mpi_opts |= HW_ATL_FW2X_CAP_EEE_2G5_MASK;

	if (speed & AQ_NIC_RATE_EEE_1G)
		mpi_opts |= HW_ATL_FW2X_CAP_EEE_1G_MASK;

	aq_hw_write_reg(self, HW_ATL_FW2X_MPI_CONTROL2_ADDR, mpi_opts);

	return 0;
}

static int aq_fw2x_get_eee_rate(struct aq_hw_s *self, u32 *rate,
					u32 *supported_rates)
{
	int err = 0;
	u32 caps_hi;
	u32 mpi_state;

	err = hw_atl_utils_fw_downld_dwords(self,
				self->mbox_addr +
				offsetof(struct hw_aq_atl_utils_mbox, info) +
				offsetof(struct hw_aq_info, caps_hi),
				&caps_hi,
				sizeof(caps_hi) / sizeof(u32));

	if (err)
		return err;

	*supported_rates = fw2x_to_eee_mask(caps_hi);

	mpi_state = aq_hw_read_reg(self, HW_ATL_FW2X_MPI_STATE2_ADDR);
	*rate = fw2x_to_eee_mask(mpi_state);

	return err;
}

static int aq_fw2x_get_flow_control(struct aq_hw_s *self, u32 *fc)
{
	u32 mpi_state = aq_hw_read_reg(self, HW_ATL_FW2X_MPI_CONTROL2_ADDR);

	*fc = ((mpi_state & BIT(CAPS_HI_PAUSE)) ? AQ_NIC_FC_RX : 0) |
	      ((mpi_state & BIT(CAPS_HI_ASYMMETRIC_PAUSE)) ? AQ_NIC_FC_TX : 0);

	return 0;
}

static int aq_fw2x_set_flow_control(struct aq_hw_s *self)
{
	u32 mpi_state = aq_hw_read_reg(self, HW_ATL_FW2X_MPI_CONTROL2_ADDR);

	aq_fw2x_set_mpi_flow_control(self, &mpi_state);

	aq_hw_write_reg(self, HW_ATL_FW2X_MPI_CONTROL2_ADDR, mpi_state);

	return 0;
}

static int aq_fw2x_led_control(struct aq_hw_s *self, u32 mode)
{
	if (self->fw_ver_actual < HW_ATL_FW_FEATURE_LED)
		return -EOPNOTSUPP;

	aq_hw_write_reg(self, HW_ATL_FW2X_MPI_LED_ADDR, mode);
	return 0;
}

static int aq_fw2x_get_eeprom(struct aq_hw_s *self, int dev_addr,
			      u32 *data, u32 len, u32 offset)
{
	u32 bytes_remains = len % sizeof(u32);
	u32 num_dwords = len / sizeof(u32);
	struct smbus_request request;
	u32 result = 0;
	u32 mpi_opts;
	int err = 0;

	if ((self->caps_lo & BIT(CAPS_LO_SMBUS_READ)) == 0)
		return -EOPNOTSUPP;

	pthread_mutex_lock(&self->mbox_mutex);

	request.msg_id = 0;
	request.device_id = dev_addr;
	request.address = offset;
	request.length = len;

	/* Write SMBUS request to cfg memory */
	err = hw_atl_utils_fw_upload_dwords(self, self->rpc_addr,
				(u32 *)(void *)&request,
				sizeof(request) / sizeof(u32));

	if (err < 0)
		goto exit;

	/* Toggle 0x368.CAPS_LO_SMBUS_READ bit */
	mpi_opts = aq_hw_read_reg(self, HW_ATL_FW2X_MPI_CONTROL_ADDR);
	mpi_opts ^= BIT(CAPS_LO_SMBUS_READ);

	aq_hw_write_reg(self, HW_ATL_FW2X_MPI_CONTROL_ADDR, mpi_opts);

	/* Wait until REQUEST_BIT matched in 0x370 */

	AQ_HW_WAIT_FOR((aq_hw_read_reg(self, HW_ATL_FW2X_MPI_STATE_ADDR) &
		BIT(CAPS_LO_SMBUS_READ)) == (mpi_opts & BIT(CAPS_LO_SMBUS_READ)),
		10U, 10000U);

	if (err < 0)
		goto exit;

	err = hw_atl_utils_fw_downld_dwords(self, self->rpc_addr + sizeof(u32),
			&result,
			sizeof(result) / sizeof(u32));

	if (err < 0)
		goto exit;

	if (result) {
		err = -EIO;
		goto exit;
	}

	if (num_dwords) {
		err = hw_atl_utils_fw_downld_dwords(self,
			self->rpc_addr + sizeof(u32) * 2,
			data,
			num_dwords);

		if (err < 0)
			goto exit;
	}

	if (bytes_remains) {
		u32 val = 0;

		err = hw_atl_utils_fw_downld_dwords(self,
			self->rpc_addr + (sizeof(u32) * 2) +
			(num_dwords * sizeof(u32)),
			&val,
			1);

		if (err < 0)
			goto exit;

		rte_memcpy((u8 *)data + len - bytes_remains,
				&val, bytes_remains);
	}

exit:
	pthread_mutex_unlock(&self->mbox_mutex);

	return err;
}


static int aq_fw2x_set_eeprom(struct aq_hw_s *self, int dev_addr,
			      u32 *data, u32 len, u32 offset)
{
	struct smbus_request request;
	u32 mpi_opts, result = 0;
	int err = 0;

	if ((self->caps_lo & BIT(CAPS_LO_SMBUS_WRITE)) == 0)
		return -EOPNOTSUPP;

	request.msg_id = 0;
	request.device_id = dev_addr;
	request.address = offset;
	request.length = len;

	pthread_mutex_lock(&self->mbox_mutex);

	/* Write SMBUS request to cfg memory */
	err = hw_atl_utils_fw_upload_dwords(self, self->rpc_addr,
				(u32 *)(void *)&request,
				sizeof(request) / sizeof(u32));

	if (err < 0)
		goto exit;

	/* Write SMBUS data to cfg memory */
	u32 num_dwords = len / sizeof(u32);
	u32 bytes_remains = len % sizeof(u32);

	if (num_dwords) {
		err = hw_atl_utils_fw_upload_dwords(self,
			self->rpc_addr + sizeof(request),
			(u32 *)(void *)data,
			num_dwords);

		if (err < 0)
			goto exit;
	}

	if (bytes_remains) {
		u32 val = 0;

		rte_memcpy(&val, (u8 *)data + (sizeof(u32) * num_dwords),
			   bytes_remains);

		err = hw_atl_utils_fw_upload_dwords(self,
			self->rpc_addr + sizeof(request) +
			(num_dwords * sizeof(u32)),
			&val,
			1);

		if (err < 0)
			goto exit;
	}

	/* Toggle 0x368.CAPS_LO_SMBUS_WRITE bit */
	mpi_opts = aq_hw_read_reg(self, HW_ATL_FW2X_MPI_CONTROL_ADDR);
	mpi_opts ^= BIT(CAPS_LO_SMBUS_WRITE);

	aq_hw_write_reg(self, HW_ATL_FW2X_MPI_CONTROL_ADDR, mpi_opts);

	/* Wait until REQUEST_BIT matched in 0x370 */
	AQ_HW_WAIT_FOR((aq_hw_read_reg(self, HW_ATL_FW2X_MPI_STATE_ADDR) &
		BIT(CAPS_LO_SMBUS_WRITE)) == (mpi_opts & BIT(CAPS_LO_SMBUS_WRITE)),
		10U, 10000U);

	if (err < 0)
		goto exit;

	/* Read status of write operation */
	err = hw_atl_utils_fw_downld_dwords(self, self->rpc_addr + sizeof(u32),
				&result,
				sizeof(result) / sizeof(u32));

	if (err < 0)
		goto exit;

	if (result) {
		err = -EIO;
		goto exit;
	}

exit:
	pthread_mutex_unlock(&self->mbox_mutex);

	return err;
}

static int aq_fw2x_send_macsec_request(struct aq_hw_s *self,
				struct macsec_msg_fw_request *req,
				struct macsec_msg_fw_response *response)
{
	int err = 0;
	u32 mpi_opts = 0;

	if (!req || !response)
		return 0;

	if ((self->caps_lo & BIT(CAPS_LO_MACSEC)) == 0)
		return -EOPNOTSUPP;

	pthread_mutex_lock(&self->mbox_mutex);

	/* Write macsec request to cfg memory */
	err = hw_atl_utils_fw_upload_dwords(self, self->rpc_addr,
		(u32 *)(void *)req,
		RTE_ALIGN(sizeof(*req) / sizeof(u32), sizeof(u32)));

	if (err < 0)
		goto exit;

	/* Toggle 0x368.CAPS_LO_MACSEC bit */
	mpi_opts = aq_hw_read_reg(self, HW_ATL_FW2X_MPI_CONTROL_ADDR);
	mpi_opts ^= BIT(CAPS_LO_MACSEC);

	aq_hw_write_reg(self, HW_ATL_FW2X_MPI_CONTROL_ADDR, mpi_opts);

	/* Wait until REQUEST_BIT matched in 0x370 */
	AQ_HW_WAIT_FOR((aq_hw_read_reg(self, HW_ATL_FW2X_MPI_STATE_ADDR) &
		BIT(CAPS_LO_MACSEC)) == (mpi_opts & BIT(CAPS_LO_MACSEC)),
		1000U, 10000U);

	if (err < 0)
		goto exit;

	/* Read status of write operation */
	err = hw_atl_utils_fw_downld_dwords(self, self->rpc_addr + sizeof(u32),
		(u32 *)(void *)response,
		RTE_ALIGN(sizeof(*response) / sizeof(u32), sizeof(u32)));

exit:
	pthread_mutex_unlock(&self->mbox_mutex);

	return err;
}

const struct aq_fw_ops aq_fw_2x_ops = {
	.init = aq_fw2x_init,
	.deinit = aq_fw2x_deinit,
	.reset = NULL,
	.get_mac_permanent = aq_fw2x_get_mac_permanent,
	.set_link_speed = aq_fw2x_set_link_speed,
	.set_state = aq_fw2x_set_state,
	.update_link_status = aq_fw2x_update_link_status,
	.update_stats = aq_fw2x_update_stats,
	.set_power = aq_fw2x_set_power,
	.get_temp = aq_fw2x_get_temp,
	.get_cable_len = aq_fw2x_get_cable_len,
	.set_eee_rate = aq_fw2x_set_eee_rate,
	.get_eee_rate = aq_fw2x_get_eee_rate,
	.get_flow_control = aq_fw2x_get_flow_control,
	.set_flow_control = aq_fw2x_set_flow_control,
	.led_control = aq_fw2x_led_control,
	.get_eeprom = aq_fw2x_get_eeprom,
	.set_eeprom = aq_fw2x_set_eeprom,
	.send_macsec_req = aq_fw2x_send_macsec_request,
};
