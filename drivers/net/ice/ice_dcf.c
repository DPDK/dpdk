/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <inttypes.h>
#include <rte_byteorder.h>
#include <rte_common.h>

#include <rte_pci.h>
#include <rte_atomic.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <ethdev_driver.h>
#include <ethdev_pci.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_dev.h>

#include "ice_dcf.h"
#include "ice_rxtx.h"

#define ICE_DCF_AQ_LEN     32
#define ICE_DCF_AQ_BUF_SZ  4096

#define ICE_DCF_ARQ_MAX_RETRIES 200
#define ICE_DCF_ARQ_CHECK_TIME  2   /* msecs */

#define ICE_DCF_VF_RES_BUF_SZ	\
	(sizeof(struct virtchnl_vf_resource) +	\
		IAVF_MAX_VF_VSI * sizeof(struct virtchnl_vsi_resource))

static __rte_always_inline int
ice_dcf_send_cmd_req_no_irq(struct ice_dcf_hw *hw, enum virtchnl_ops op,
			    uint8_t *req_msg, uint16_t req_msglen)
{
	return iavf_aq_send_msg_to_pf(&hw->avf, op, IAVF_SUCCESS,
				      req_msg, req_msglen, NULL);
}

static int
ice_dcf_recv_cmd_rsp_no_irq(struct ice_dcf_hw *hw, enum virtchnl_ops op,
			    uint8_t *rsp_msgbuf, uint16_t rsp_buflen,
			    uint16_t *rsp_msglen)
{
	struct iavf_arq_event_info event;
	enum virtchnl_ops v_op;
	int i = 0;
	int err;

	event.buf_len = rsp_buflen;
	event.msg_buf = rsp_msgbuf;

	do {
		err = iavf_clean_arq_element(&hw->avf, &event, NULL);
		if (err != IAVF_SUCCESS)
			goto again;

		v_op = rte_le_to_cpu_32(event.desc.cookie_high);
		if (v_op != op)
			goto again;

		if (rsp_msglen != NULL)
			*rsp_msglen = event.msg_len;
		return rte_le_to_cpu_32(event.desc.cookie_low);

again:
		rte_delay_ms(ICE_DCF_ARQ_CHECK_TIME);
	} while (i++ < ICE_DCF_ARQ_MAX_RETRIES);

	return -EIO;
}

static __rte_always_inline void
ice_dcf_aq_cmd_clear(struct ice_dcf_hw *hw, struct dcf_virtchnl_cmd *cmd)
{
	rte_spinlock_lock(&hw->vc_cmd_queue_lock);

	TAILQ_REMOVE(&hw->vc_cmd_queue, cmd, next);

	rte_spinlock_unlock(&hw->vc_cmd_queue_lock);
}

static __rte_always_inline void
ice_dcf_vc_cmd_set(struct ice_dcf_hw *hw, struct dcf_virtchnl_cmd *cmd)
{
	cmd->v_ret = IAVF_ERR_NOT_READY;
	cmd->rsp_msglen = 0;
	cmd->pending = 1;

	rte_spinlock_lock(&hw->vc_cmd_queue_lock);

	TAILQ_INSERT_TAIL(&hw->vc_cmd_queue, cmd, next);

	rte_spinlock_unlock(&hw->vc_cmd_queue_lock);
}

static __rte_always_inline int
ice_dcf_vc_cmd_send(struct ice_dcf_hw *hw, struct dcf_virtchnl_cmd *cmd)
{
	return iavf_aq_send_msg_to_pf(&hw->avf,
				      cmd->v_op, IAVF_SUCCESS,
				      cmd->req_msg, cmd->req_msglen, NULL);
}

static __rte_always_inline void
ice_dcf_aq_cmd_handle(struct ice_dcf_hw *hw, struct iavf_arq_event_info *info)
{
	struct dcf_virtchnl_cmd *cmd;
	enum virtchnl_ops v_op;
	enum iavf_status v_ret;
	uint16_t aq_op;

	aq_op = rte_le_to_cpu_16(info->desc.opcode);
	if (unlikely(aq_op != iavf_aqc_opc_send_msg_to_vf)) {
		PMD_DRV_LOG(ERR,
			    "Request %u is not supported yet", aq_op);
		return;
	}

	v_op = rte_le_to_cpu_32(info->desc.cookie_high);
	if (v_op == VIRTCHNL_OP_EVENT) {
		if (hw->vc_event_msg_cb != NULL)
			hw->vc_event_msg_cb(hw,
					    info->msg_buf,
					    info->msg_len);
		return;
	}

	v_ret = rte_le_to_cpu_32(info->desc.cookie_low);

	rte_spinlock_lock(&hw->vc_cmd_queue_lock);

	TAILQ_FOREACH(cmd, &hw->vc_cmd_queue, next) {
		if (cmd->v_op == v_op && cmd->pending) {
			cmd->v_ret = v_ret;
			cmd->rsp_msglen = RTE_MIN(info->msg_len,
						  cmd->rsp_buflen);
			if (likely(cmd->rsp_msglen != 0))
				rte_memcpy(cmd->rsp_msgbuf, info->msg_buf,
					   cmd->rsp_msglen);

			/* prevent compiler reordering */
			rte_compiler_barrier();
			cmd->pending = 0;
			break;
		}
	}

	rte_spinlock_unlock(&hw->vc_cmd_queue_lock);
}

static void
ice_dcf_handle_virtchnl_msg(struct ice_dcf_hw *hw)
{
	struct iavf_arq_event_info info;
	uint16_t pending = 1;
	int ret;

	info.buf_len = ICE_DCF_AQ_BUF_SZ;
	info.msg_buf = hw->arq_buf;

	while (pending) {
		ret = iavf_clean_arq_element(&hw->avf, &info, &pending);
		if (ret != IAVF_SUCCESS)
			break;

		ice_dcf_aq_cmd_handle(hw, &info);
	}
}

static int
ice_dcf_init_check_api_version(struct ice_dcf_hw *hw)
{
#define ICE_CPF_VIRTCHNL_VERSION_MAJOR_START	1
#define ICE_CPF_VIRTCHNL_VERSION_MINOR_START	1
	struct virtchnl_version_info version, *pver;
	int err;

	version.major = VIRTCHNL_VERSION_MAJOR;
	version.minor = VIRTCHNL_VERSION_MINOR;
	err = ice_dcf_send_cmd_req_no_irq(hw, VIRTCHNL_OP_VERSION,
					  (uint8_t *)&version, sizeof(version));
	if (err) {
		PMD_INIT_LOG(ERR, "Failed to send OP_VERSION");
		return err;
	}

	pver = &hw->virtchnl_version;
	err = ice_dcf_recv_cmd_rsp_no_irq(hw, VIRTCHNL_OP_VERSION,
					  (uint8_t *)pver, sizeof(*pver), NULL);
	if (err) {
		PMD_INIT_LOG(ERR, "Failed to get response of OP_VERSION");
		return -1;
	}

	PMD_INIT_LOG(DEBUG,
		     "Peer PF API version: %u.%u", pver->major, pver->minor);

	if (pver->major < ICE_CPF_VIRTCHNL_VERSION_MAJOR_START ||
	    (pver->major == ICE_CPF_VIRTCHNL_VERSION_MAJOR_START &&
	     pver->minor < ICE_CPF_VIRTCHNL_VERSION_MINOR_START)) {
		PMD_INIT_LOG(ERR,
			     "VIRTCHNL API version should not be lower than (%u.%u)",
			     ICE_CPF_VIRTCHNL_VERSION_MAJOR_START,
			     ICE_CPF_VIRTCHNL_VERSION_MAJOR_START);
		return -1;
	} else if (pver->major > VIRTCHNL_VERSION_MAJOR ||
		   (pver->major == VIRTCHNL_VERSION_MAJOR &&
		    pver->minor > VIRTCHNL_VERSION_MINOR)) {
		PMD_INIT_LOG(ERR,
			     "PF/VF API version mismatch:(%u.%u)-(%u.%u)",
			     pver->major, pver->minor,
			     VIRTCHNL_VERSION_MAJOR, VIRTCHNL_VERSION_MINOR);
		return -1;
	}

	PMD_INIT_LOG(DEBUG, "Peer is supported PF host");

	return 0;
}

static int
ice_dcf_get_vf_resource(struct ice_dcf_hw *hw)
{
	uint32_t caps;
	int err, i;

	caps = VIRTCHNL_VF_OFFLOAD_WB_ON_ITR | VIRTCHNL_VF_OFFLOAD_RX_POLLING |
	       VIRTCHNL_VF_CAP_ADV_LINK_SPEED | VIRTCHNL_VF_CAP_DCF |
	       VIRTCHNL_VF_OFFLOAD_VLAN_V2 |
	       VF_BASE_MODE_OFFLOADS | VIRTCHNL_VF_OFFLOAD_RX_FLEX_DESC |
	       VIRTCHNL_VF_OFFLOAD_QOS;

	err = ice_dcf_send_cmd_req_no_irq(hw, VIRTCHNL_OP_GET_VF_RESOURCES,
					  (uint8_t *)&caps, sizeof(caps));
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to send msg OP_GET_VF_RESOURCE");
		return err;
	}

	err = ice_dcf_recv_cmd_rsp_no_irq(hw, VIRTCHNL_OP_GET_VF_RESOURCES,
					  (uint8_t *)hw->vf_res,
					  ICE_DCF_VF_RES_BUF_SZ, NULL);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to get response of OP_GET_VF_RESOURCE");
		return -1;
	}

	iavf_vf_parse_hw_config(&hw->avf, hw->vf_res);

	hw->vsi_res = NULL;
	for (i = 0; i < hw->vf_res->num_vsis; i++) {
		if (hw->vf_res->vsi_res[i].vsi_type == VIRTCHNL_VSI_SRIOV)
			hw->vsi_res = &hw->vf_res->vsi_res[i];
	}

	if (!hw->vsi_res) {
		PMD_DRV_LOG(ERR, "no LAN VSI found");
		return -1;
	}

	hw->vsi_id = hw->vsi_res->vsi_id;
	PMD_DRV_LOG(DEBUG, "VSI ID is %u", hw->vsi_id);

	return 0;
}

static int
ice_dcf_get_vf_vsi_map(struct ice_dcf_hw *hw)
{
	struct virtchnl_dcf_vsi_map *vsi_map;
	uint32_t valid_msg_len;
	uint16_t len;
	int err;

	err = ice_dcf_send_cmd_req_no_irq(hw, VIRTCHNL_OP_DCF_GET_VSI_MAP,
					  NULL, 0);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to send msg OP_DCF_GET_VSI_MAP");
		return err;
	}

	err = ice_dcf_recv_cmd_rsp_no_irq(hw, VIRTCHNL_OP_DCF_GET_VSI_MAP,
					  hw->arq_buf, ICE_DCF_AQ_BUF_SZ,
					  &len);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to get response of OP_DCF_GET_VSI_MAP");
		return err;
	}

	vsi_map = (struct virtchnl_dcf_vsi_map *)hw->arq_buf;
	valid_msg_len = (vsi_map->num_vfs - 1) * sizeof(vsi_map->vf_vsi[0]) +
			sizeof(*vsi_map);
	if (len != valid_msg_len) {
		PMD_DRV_LOG(ERR, "invalid vf vsi map response with length %u",
			    len);
		return -EINVAL;
	}

	if (hw->num_vfs != 0 && hw->num_vfs != vsi_map->num_vfs) {
		PMD_DRV_LOG(ERR, "The number VSI map (%u) doesn't match the number of VFs (%u)",
			    vsi_map->num_vfs, hw->num_vfs);
		return -EINVAL;
	}

	len = vsi_map->num_vfs * sizeof(vsi_map->vf_vsi[0]);

	if (!hw->vf_vsi_map) {
		hw->vf_vsi_map = rte_zmalloc("vf_vsi_ctx", len, 0);
		if (!hw->vf_vsi_map) {
			PMD_DRV_LOG(ERR, "Failed to alloc memory for VSI context");
			return -ENOMEM;
		}

		hw->num_vfs = vsi_map->num_vfs;
		hw->pf_vsi_id = vsi_map->pf_vsi;
	}

	if (!memcmp(hw->vf_vsi_map, vsi_map->vf_vsi, len)) {
		PMD_DRV_LOG(DEBUG, "VF VSI map doesn't change");
		return 1;
	}

	rte_memcpy(hw->vf_vsi_map, vsi_map->vf_vsi, len);
	return 0;
}

static int
ice_dcf_mode_disable(struct ice_dcf_hw *hw)
{
	int err;

	if (hw->resetting)
		return 0;

	err = ice_dcf_send_cmd_req_no_irq(hw, VIRTCHNL_OP_DCF_DISABLE,
					  NULL, 0);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to send msg OP_DCF_DISABLE");
		return err;
	}

	err = ice_dcf_recv_cmd_rsp_no_irq(hw, VIRTCHNL_OP_DCF_DISABLE,
					  hw->arq_buf, ICE_DCF_AQ_BUF_SZ, NULL);
	if (err) {
		PMD_DRV_LOG(ERR,
			    "Failed to get response of OP_DCF_DISABLE %d",
			    err);
		return -1;
	}

	return 0;
}

static int
ice_dcf_check_reset_done(struct ice_dcf_hw *hw)
{
#define ICE_DCF_RESET_WAIT_CNT       50
	struct iavf_hw *avf = &hw->avf;
	int i, reset;

	for (i = 0; i < ICE_DCF_RESET_WAIT_CNT; i++) {
		reset = IAVF_READ_REG(avf, IAVF_VFGEN_RSTAT) &
					IAVF_VFGEN_RSTAT_VFR_STATE_MASK;
		reset = reset >> IAVF_VFGEN_RSTAT_VFR_STATE_SHIFT;

		if (reset == VIRTCHNL_VFR_VFACTIVE ||
		    reset == VIRTCHNL_VFR_COMPLETED)
			break;

		rte_delay_ms(20);
	}

	if (i >= ICE_DCF_RESET_WAIT_CNT)
		return -1;

	return 0;
}

static inline void
ice_dcf_enable_irq0(struct ice_dcf_hw *hw)
{
	struct iavf_hw *avf = &hw->avf;

	/* Enable admin queue interrupt trigger */
	IAVF_WRITE_REG(avf, IAVF_VFINT_ICR0_ENA1,
		       IAVF_VFINT_ICR0_ENA1_ADMINQ_MASK);
	IAVF_WRITE_REG(avf, IAVF_VFINT_DYN_CTL01,
		       IAVF_VFINT_DYN_CTL01_INTENA_MASK |
		       IAVF_VFINT_DYN_CTL01_CLEARPBA_MASK |
		       IAVF_VFINT_DYN_CTL01_ITR_INDX_MASK);

	IAVF_WRITE_FLUSH(avf);
}

static inline void
ice_dcf_disable_irq0(struct ice_dcf_hw *hw)
{
	struct iavf_hw *avf = &hw->avf;

	/* Disable all interrupt types */
	IAVF_WRITE_REG(avf, IAVF_VFINT_ICR0_ENA1, 0);
	IAVF_WRITE_REG(avf, IAVF_VFINT_DYN_CTL01,
		       IAVF_VFINT_DYN_CTL01_ITR_INDX_MASK);

	IAVF_WRITE_FLUSH(avf);
}

static void
ice_dcf_dev_interrupt_handler(void *param)
{
	struct ice_dcf_hw *hw = param;

	ice_dcf_disable_irq0(hw);

	ice_dcf_handle_virtchnl_msg(hw);

	ice_dcf_enable_irq0(hw);
}

int
ice_dcf_execute_virtchnl_cmd(struct ice_dcf_hw *hw,
			     struct dcf_virtchnl_cmd *cmd)
{
	int i = 0;
	int err;

	if ((cmd->req_msg && !cmd->req_msglen) ||
	    (!cmd->req_msg && cmd->req_msglen) ||
	    (cmd->rsp_msgbuf && !cmd->rsp_buflen) ||
	    (!cmd->rsp_msgbuf && cmd->rsp_buflen))
		return -EINVAL;

	rte_spinlock_lock(&hw->vc_cmd_send_lock);
	ice_dcf_vc_cmd_set(hw, cmd);

	err = ice_dcf_vc_cmd_send(hw, cmd);
	if (err) {
		PMD_DRV_LOG(ERR, "fail to send cmd %d", cmd->v_op);
		goto ret;
	}

	do {
		if (!cmd->pending)
			break;

		rte_delay_ms(ICE_DCF_ARQ_CHECK_TIME);
	} while (i++ < ICE_DCF_ARQ_MAX_RETRIES);

	if (cmd->v_ret != IAVF_SUCCESS) {
		err = -1;
		PMD_DRV_LOG(ERR,
			    "No response (%d times) or return failure (%d) for cmd %d",
			    i, cmd->v_ret, cmd->v_op);
	}

ret:
	ice_dcf_aq_cmd_clear(hw, cmd);
	rte_spinlock_unlock(&hw->vc_cmd_send_lock);
	return err;
}

int
ice_dcf_send_aq_cmd(void *dcf_hw, struct ice_aq_desc *desc,
		    void *buf, uint16_t buf_size)
{
	struct dcf_virtchnl_cmd desc_cmd, buff_cmd;
	struct ice_dcf_hw *hw = dcf_hw;
	int err = 0;
	int i = 0;

	if ((buf && !buf_size) || (!buf && buf_size) ||
	    buf_size > ICE_DCF_AQ_BUF_SZ)
		return -EINVAL;

	desc_cmd.v_op = VIRTCHNL_OP_DCF_CMD_DESC;
	desc_cmd.req_msglen = sizeof(*desc);
	desc_cmd.req_msg = (uint8_t *)desc;
	desc_cmd.rsp_buflen = sizeof(*desc);
	desc_cmd.rsp_msgbuf = (uint8_t *)desc;

	if (buf == NULL)
		return ice_dcf_execute_virtchnl_cmd(hw, &desc_cmd);

	desc->flags |= rte_cpu_to_le_16(ICE_AQ_FLAG_BUF);

	buff_cmd.v_op = VIRTCHNL_OP_DCF_CMD_BUFF;
	buff_cmd.req_msglen = buf_size;
	buff_cmd.req_msg = buf;
	buff_cmd.rsp_buflen = buf_size;
	buff_cmd.rsp_msgbuf = buf;

	rte_spinlock_lock(&hw->vc_cmd_send_lock);
	ice_dcf_vc_cmd_set(hw, &desc_cmd);
	ice_dcf_vc_cmd_set(hw, &buff_cmd);

	if (ice_dcf_vc_cmd_send(hw, &desc_cmd) ||
	    ice_dcf_vc_cmd_send(hw, &buff_cmd)) {
		err = -1;
		PMD_DRV_LOG(ERR, "fail to send OP_DCF_CMD_DESC/BUFF");
		goto ret;
	}

	do {
		if (!desc_cmd.pending && !buff_cmd.pending)
			break;

		rte_delay_ms(ICE_DCF_ARQ_CHECK_TIME);
	} while (i++ < ICE_DCF_ARQ_MAX_RETRIES);

	if (desc_cmd.v_ret != IAVF_SUCCESS || buff_cmd.v_ret != IAVF_SUCCESS) {
		err = -1;
		PMD_DRV_LOG(ERR,
			    "No response (%d times) or return failure (desc: %d / buff: %d)",
			    i, desc_cmd.v_ret, buff_cmd.v_ret);
	}

ret:
	ice_dcf_aq_cmd_clear(hw, &desc_cmd);
	ice_dcf_aq_cmd_clear(hw, &buff_cmd);
	rte_spinlock_unlock(&hw->vc_cmd_send_lock);

	return err;
}

int
ice_dcf_handle_vsi_update_event(struct ice_dcf_hw *hw)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(hw->eth_dev);
	int err = 0;

	rte_spinlock_lock(&hw->vc_cmd_send_lock);

	rte_intr_disable(&pci_dev->intr_handle);
	ice_dcf_disable_irq0(hw);

	if (ice_dcf_get_vf_resource(hw) || ice_dcf_get_vf_vsi_map(hw) < 0)
		err = -1;

	rte_intr_enable(&pci_dev->intr_handle);
	ice_dcf_enable_irq0(hw);

	rte_spinlock_unlock(&hw->vc_cmd_send_lock);

	return err;
}

static int
ice_dcf_get_supported_rxdid(struct ice_dcf_hw *hw)
{
	int err;

	err = ice_dcf_send_cmd_req_no_irq(hw,
					  VIRTCHNL_OP_GET_SUPPORTED_RXDIDS,
					  NULL, 0);
	if (err) {
		PMD_INIT_LOG(ERR, "Failed to send OP_GET_SUPPORTED_RXDIDS");
		return -1;
	}

	err = ice_dcf_recv_cmd_rsp_no_irq(hw, VIRTCHNL_OP_GET_SUPPORTED_RXDIDS,
					  (uint8_t *)&hw->supported_rxdid,
					  sizeof(uint64_t), NULL);
	if (err) {
		PMD_INIT_LOG(ERR, "Failed to get response of OP_GET_SUPPORTED_RXDIDS");
		return -1;
	}

	return 0;
}

int
ice_dcf_init_hw(struct rte_eth_dev *eth_dev, struct ice_dcf_hw *hw)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	int ret, size;

	hw->avf.hw_addr = pci_dev->mem_resource[0].addr;
	hw->avf.back = hw;

	hw->avf.bus.bus_id = pci_dev->addr.bus;
	hw->avf.bus.device = pci_dev->addr.devid;
	hw->avf.bus.func = pci_dev->addr.function;

	hw->avf.device_id = pci_dev->id.device_id;
	hw->avf.vendor_id = pci_dev->id.vendor_id;
	hw->avf.subsystem_device_id = pci_dev->id.subsystem_device_id;
	hw->avf.subsystem_vendor_id = pci_dev->id.subsystem_vendor_id;

	hw->avf.aq.num_arq_entries = ICE_DCF_AQ_LEN;
	hw->avf.aq.num_asq_entries = ICE_DCF_AQ_LEN;
	hw->avf.aq.arq_buf_size = ICE_DCF_AQ_BUF_SZ;
	hw->avf.aq.asq_buf_size = ICE_DCF_AQ_BUF_SZ;

	rte_spinlock_init(&hw->vc_cmd_send_lock);
	rte_spinlock_init(&hw->vc_cmd_queue_lock);
	TAILQ_INIT(&hw->vc_cmd_queue);

	hw->arq_buf = rte_zmalloc("arq_buf", ICE_DCF_AQ_BUF_SZ, 0);
	if (hw->arq_buf == NULL) {
		PMD_INIT_LOG(ERR, "unable to allocate AdminQ buffer memory");
		goto err;
	}

	ret = iavf_set_mac_type(&hw->avf);
	if (ret) {
		PMD_INIT_LOG(ERR, "set_mac_type failed: %d", ret);
		goto err;
	}

	ret = ice_dcf_check_reset_done(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "VF is still resetting");
		goto err;
	}

	ret = iavf_init_adminq(&hw->avf);
	if (ret) {
		PMD_INIT_LOG(ERR, "init_adminq failed: %d", ret);
		goto err;
	}

	if (ice_dcf_init_check_api_version(hw)) {
		PMD_INIT_LOG(ERR, "check_api version failed");
		goto err_api;
	}

	hw->vf_res = rte_zmalloc("vf_res", ICE_DCF_VF_RES_BUF_SZ, 0);
	if (hw->vf_res == NULL) {
		PMD_INIT_LOG(ERR, "unable to allocate vf_res memory");
		goto err_api;
	}

	if (ice_dcf_get_vf_resource(hw)) {
		PMD_INIT_LOG(ERR, "Failed to get VF resource");
		goto err_alloc;
	}

	if (ice_dcf_get_vf_vsi_map(hw) < 0) {
		PMD_INIT_LOG(ERR, "Failed to get VF VSI map");
		ice_dcf_mode_disable(hw);
		goto err_alloc;
	}

	/* Allocate memory for RSS info */
	if (hw->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RSS_PF) {
		hw->rss_key = rte_zmalloc(NULL,
					  hw->vf_res->rss_key_size, 0);
		if (!hw->rss_key) {
			PMD_INIT_LOG(ERR, "unable to allocate rss_key memory");
			goto err_alloc;
		}
		hw->rss_lut = rte_zmalloc("rss_lut",
					  hw->vf_res->rss_lut_size, 0);
		if (!hw->rss_lut) {
			PMD_INIT_LOG(ERR, "unable to allocate rss_lut memory");
			goto err_rss;
		}
	}

	if (hw->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RX_FLEX_DESC) {
		if (ice_dcf_get_supported_rxdid(hw) != 0) {
			PMD_INIT_LOG(ERR, "failed to do get supported rxdid");
			goto err_rss;
		}
	}

	if (hw->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_QOS) {
		ice_dcf_tm_conf_init(eth_dev);
		size = sizeof(struct virtchnl_dcf_bw_cfg_list *) * hw->num_vfs;
		hw->qos_bw_cfg = rte_zmalloc("qos_bw_cfg", size, 0);
		if (!hw->qos_bw_cfg) {
			PMD_INIT_LOG(ERR, "no memory for qos_bw_cfg");
			goto err_rss;
		}
	}

	hw->eth_dev = eth_dev;
	rte_intr_callback_register(&pci_dev->intr_handle,
				   ice_dcf_dev_interrupt_handler, hw);
	rte_intr_enable(&pci_dev->intr_handle);
	ice_dcf_enable_irq0(hw);

	return 0;

err_rss:
	rte_free(hw->rss_key);
	rte_free(hw->rss_lut);
err_alloc:
	rte_free(hw->vf_res);
err_api:
	iavf_shutdown_adminq(&hw->avf);
err:
	rte_free(hw->arq_buf);

	return -1;
}

void
ice_dcf_uninit_hw(struct rte_eth_dev *eth_dev, struct ice_dcf_hw *hw)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;

	if (hw->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_QOS)
		if (hw->tm_conf.committed) {
			ice_dcf_clear_bw(hw);
			ice_dcf_tm_conf_uninit(eth_dev);
		}

	ice_dcf_disable_irq0(hw);
	rte_intr_disable(intr_handle);
	rte_intr_callback_unregister(intr_handle,
				     ice_dcf_dev_interrupt_handler, hw);

	ice_dcf_mode_disable(hw);
	iavf_shutdown_adminq(&hw->avf);

	rte_free(hw->arq_buf);
	hw->arq_buf = NULL;

	rte_free(hw->vf_vsi_map);
	hw->vf_vsi_map = NULL;

	rte_free(hw->vf_res);
	hw->vf_res = NULL;

	rte_free(hw->rss_lut);
	hw->rss_lut = NULL;

	rte_free(hw->rss_key);
	hw->rss_key = NULL;

	rte_free(hw->qos_bw_cfg);
	hw->qos_bw_cfg = NULL;

	rte_free(hw->ets_config);
	hw->ets_config = NULL;
}

static int
ice_dcf_configure_rss_key(struct ice_dcf_hw *hw)
{
	struct virtchnl_rss_key *rss_key;
	struct dcf_virtchnl_cmd args;
	int len, err;

	len = sizeof(*rss_key) + hw->vf_res->rss_key_size - 1;
	rss_key = rte_zmalloc("rss_key", len, 0);
	if (!rss_key)
		return -ENOMEM;

	rss_key->vsi_id = hw->vsi_res->vsi_id;
	rss_key->key_len = hw->vf_res->rss_key_size;
	rte_memcpy(rss_key->key, hw->rss_key, hw->vf_res->rss_key_size);

	args.v_op = VIRTCHNL_OP_CONFIG_RSS_KEY;
	args.req_msglen = len;
	args.req_msg = (uint8_t *)rss_key;
	args.rsp_msglen = 0;
	args.rsp_buflen = 0;
	args.rsp_msgbuf = NULL;
	args.pending = 0;

	err = ice_dcf_execute_virtchnl_cmd(hw, &args);
	if (err)
		PMD_INIT_LOG(ERR, "Failed to execute OP_CONFIG_RSS_KEY");

	rte_free(rss_key);
	return err;
}

static int
ice_dcf_configure_rss_lut(struct ice_dcf_hw *hw)
{
	struct virtchnl_rss_lut *rss_lut;
	struct dcf_virtchnl_cmd args;
	int len, err;

	len = sizeof(*rss_lut) + hw->vf_res->rss_lut_size - 1;
	rss_lut = rte_zmalloc("rss_lut", len, 0);
	if (!rss_lut)
		return -ENOMEM;

	rss_lut->vsi_id = hw->vsi_res->vsi_id;
	rss_lut->lut_entries = hw->vf_res->rss_lut_size;
	rte_memcpy(rss_lut->lut, hw->rss_lut, hw->vf_res->rss_lut_size);

	args.v_op = VIRTCHNL_OP_CONFIG_RSS_LUT;
	args.req_msglen = len;
	args.req_msg = (uint8_t *)rss_lut;
	args.rsp_msglen = 0;
	args.rsp_buflen = 0;
	args.rsp_msgbuf = NULL;
	args.pending = 0;

	err = ice_dcf_execute_virtchnl_cmd(hw, &args);
	if (err)
		PMD_INIT_LOG(ERR, "Failed to execute OP_CONFIG_RSS_LUT");

	rte_free(rss_lut);
	return err;
}

int
ice_dcf_init_rss(struct ice_dcf_hw *hw)
{
	struct rte_eth_dev *dev = hw->eth_dev;
	struct rte_eth_rss_conf *rss_conf;
	uint8_t i, j, nb_q;
	int ret;

	rss_conf = &dev->data->dev_conf.rx_adv_conf.rss_conf;
	nb_q = dev->data->nb_rx_queues;

	if (!(hw->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RSS_PF)) {
		PMD_DRV_LOG(DEBUG, "RSS is not supported");
		return -ENOTSUP;
	}
	if (dev->data->dev_conf.rxmode.mq_mode != ETH_MQ_RX_RSS) {
		PMD_DRV_LOG(WARNING, "RSS is enabled by PF by default");
		/* set all lut items to default queue */
		memset(hw->rss_lut, 0, hw->vf_res->rss_lut_size);
		return ice_dcf_configure_rss_lut(hw);
	}

	/* In IAVF, RSS enablement is set by PF driver. It is not supported
	 * to set based on rss_conf->rss_hf.
	 */

	/* configure RSS key */
	if (!rss_conf->rss_key)
		/* Calculate the default hash key */
		for (i = 0; i < hw->vf_res->rss_key_size; i++)
			hw->rss_key[i] = (uint8_t)rte_rand();
	else
		rte_memcpy(hw->rss_key, rss_conf->rss_key,
			   RTE_MIN(rss_conf->rss_key_len,
				   hw->vf_res->rss_key_size));

	/* init RSS LUT table */
	for (i = 0, j = 0; i < hw->vf_res->rss_lut_size; i++, j++) {
		if (j >= nb_q)
			j = 0;
		hw->rss_lut[i] = j;
	}
	/* send virtchnnl ops to configure rss*/
	ret = ice_dcf_configure_rss_lut(hw);
	if (ret)
		return ret;
	ret = ice_dcf_configure_rss_key(hw);
	if (ret)
		return ret;

	return 0;
}

#define IAVF_RXDID_LEGACY_0 0
#define IAVF_RXDID_LEGACY_1 1
#define IAVF_RXDID_COMMS_OVS_1 22

int
ice_dcf_configure_queues(struct ice_dcf_hw *hw)
{
	struct ice_rx_queue **rxq =
		(struct ice_rx_queue **)hw->eth_dev->data->rx_queues;
	struct ice_tx_queue **txq =
		(struct ice_tx_queue **)hw->eth_dev->data->tx_queues;
	struct virtchnl_vsi_queue_config_info *vc_config;
	struct virtchnl_queue_pair_info *vc_qp;
	struct dcf_virtchnl_cmd args;
	uint16_t i, size;
	int err;

	size = sizeof(*vc_config) +
	       sizeof(vc_config->qpair[0]) * hw->num_queue_pairs;
	vc_config = rte_zmalloc("cfg_queue", size, 0);
	if (!vc_config)
		return -ENOMEM;

	vc_config->vsi_id = hw->vsi_res->vsi_id;
	vc_config->num_queue_pairs = hw->num_queue_pairs;

	for (i = 0, vc_qp = vc_config->qpair;
	     i < hw->num_queue_pairs;
	     i++, vc_qp++) {
		vc_qp->txq.vsi_id = hw->vsi_res->vsi_id;
		vc_qp->txq.queue_id = i;
		if (i < hw->eth_dev->data->nb_tx_queues) {
			vc_qp->txq.ring_len = txq[i]->nb_tx_desc;
			vc_qp->txq.dma_ring_addr = txq[i]->tx_ring_dma;
		}
		vc_qp->rxq.vsi_id = hw->vsi_res->vsi_id;
		vc_qp->rxq.queue_id = i;

		if (i >= hw->eth_dev->data->nb_rx_queues)
			continue;

		vc_qp->rxq.max_pkt_size = rxq[i]->max_pkt_len;
		vc_qp->rxq.ring_len = rxq[i]->nb_rx_desc;
		vc_qp->rxq.dma_ring_addr = rxq[i]->rx_ring_dma;
		vc_qp->rxq.databuffer_size = rxq[i]->rx_buf_len;

#ifndef RTE_LIBRTE_ICE_16BYTE_RX_DESC
		if (hw->vf_res->vf_cap_flags &
		    VIRTCHNL_VF_OFFLOAD_RX_FLEX_DESC &&
		    hw->supported_rxdid &
		    BIT(IAVF_RXDID_COMMS_OVS_1)) {
			vc_qp->rxq.rxdid = IAVF_RXDID_COMMS_OVS_1;
			PMD_DRV_LOG(NOTICE, "request RXDID == %d in "
				    "Queue[%d]", vc_qp->rxq.rxdid, i);
		} else {
			PMD_DRV_LOG(ERR, "RXDID 16 is not supported");
			return -EINVAL;
		}
#else
		if (hw->vf_res->vf_cap_flags &
			VIRTCHNL_VF_OFFLOAD_RX_FLEX_DESC &&
			hw->supported_rxdid &
			BIT(IAVF_RXDID_LEGACY_0)) {
			vc_qp->rxq.rxdid = IAVF_RXDID_LEGACY_0;
			PMD_DRV_LOG(NOTICE, "request RXDID == %d in "
					"Queue[%d]", vc_qp->rxq.rxdid, i);
		} else {
			PMD_DRV_LOG(ERR, "RXDID == 0 is not supported");
			return -EINVAL;
		}
#endif
		ice_select_rxd_to_pkt_fields_handler(rxq[i], vc_qp->rxq.rxdid);
	}

	memset(&args, 0, sizeof(args));
	args.v_op = VIRTCHNL_OP_CONFIG_VSI_QUEUES;
	args.req_msg = (uint8_t *)vc_config;
	args.req_msglen = size;

	err = ice_dcf_execute_virtchnl_cmd(hw, &args);
	if (err)
		PMD_DRV_LOG(ERR, "Failed to execute command of"
			    " VIRTCHNL_OP_CONFIG_VSI_QUEUES");

	rte_free(vc_config);
	return err;
}

int
ice_dcf_config_irq_map(struct ice_dcf_hw *hw)
{
	struct virtchnl_irq_map_info *map_info;
	struct virtchnl_vector_map *vecmap;
	struct dcf_virtchnl_cmd args;
	int len, i, err;

	len = sizeof(struct virtchnl_irq_map_info) +
	      sizeof(struct virtchnl_vector_map) * hw->nb_msix;

	map_info = rte_zmalloc("map_info", len, 0);
	if (!map_info)
		return -ENOMEM;

	map_info->num_vectors = hw->nb_msix;
	for (i = 0; i < hw->nb_msix; i++) {
		vecmap = &map_info->vecmap[i];
		vecmap->vsi_id = hw->vsi_res->vsi_id;
		vecmap->rxitr_idx = 0;
		vecmap->vector_id = hw->msix_base + i;
		vecmap->txq_map = 0;
		vecmap->rxq_map = hw->rxq_map[hw->msix_base + i];
	}

	memset(&args, 0, sizeof(args));
	args.v_op = VIRTCHNL_OP_CONFIG_IRQ_MAP;
	args.req_msg = (u8 *)map_info;
	args.req_msglen = len;

	err = ice_dcf_execute_virtchnl_cmd(hw, &args);
	if (err)
		PMD_DRV_LOG(ERR, "fail to execute command OP_CONFIG_IRQ_MAP");

	rte_free(map_info);
	return err;
}

int
ice_dcf_switch_queue(struct ice_dcf_hw *hw, uint16_t qid, bool rx, bool on)
{
	struct virtchnl_queue_select queue_select;
	struct dcf_virtchnl_cmd args;
	int err;

	memset(&queue_select, 0, sizeof(queue_select));
	queue_select.vsi_id = hw->vsi_res->vsi_id;
	if (rx)
		queue_select.rx_queues |= 1 << qid;
	else
		queue_select.tx_queues |= 1 << qid;

	memset(&args, 0, sizeof(args));
	if (on)
		args.v_op = VIRTCHNL_OP_ENABLE_QUEUES;
	else
		args.v_op = VIRTCHNL_OP_DISABLE_QUEUES;

	args.req_msg = (u8 *)&queue_select;
	args.req_msglen = sizeof(queue_select);

	err = ice_dcf_execute_virtchnl_cmd(hw, &args);
	if (err)
		PMD_DRV_LOG(ERR, "Failed to execute command of %s",
			    on ? "OP_ENABLE_QUEUES" : "OP_DISABLE_QUEUES");

	return err;
}

int
ice_dcf_disable_queues(struct ice_dcf_hw *hw)
{
	struct virtchnl_queue_select queue_select;
	struct dcf_virtchnl_cmd args;
	int err;

	if (hw->resetting)
		return 0;

	memset(&queue_select, 0, sizeof(queue_select));
	queue_select.vsi_id = hw->vsi_res->vsi_id;

	queue_select.rx_queues = BIT(hw->eth_dev->data->nb_rx_queues) - 1;
	queue_select.tx_queues = BIT(hw->eth_dev->data->nb_tx_queues) - 1;

	memset(&args, 0, sizeof(args));
	args.v_op = VIRTCHNL_OP_DISABLE_QUEUES;
	args.req_msg = (u8 *)&queue_select;
	args.req_msglen = sizeof(queue_select);

	err = ice_dcf_execute_virtchnl_cmd(hw, &args);
	if (err)
		PMD_DRV_LOG(ERR,
			    "Failed to execute command of OP_DISABLE_QUEUES");

	return err;
}

int
ice_dcf_query_stats(struct ice_dcf_hw *hw,
				   struct virtchnl_eth_stats *pstats)
{
	struct virtchnl_queue_select q_stats;
	struct dcf_virtchnl_cmd args;
	int err;

	memset(&q_stats, 0, sizeof(q_stats));
	q_stats.vsi_id = hw->vsi_res->vsi_id;

	args.v_op = VIRTCHNL_OP_GET_STATS;
	args.req_msg = (uint8_t *)&q_stats;
	args.req_msglen = sizeof(q_stats);
	args.rsp_msglen = sizeof(*pstats);
	args.rsp_msgbuf = (uint8_t *)pstats;
	args.rsp_buflen = sizeof(*pstats);

	err = ice_dcf_execute_virtchnl_cmd(hw, &args);
	if (err) {
		PMD_DRV_LOG(ERR, "fail to execute command OP_GET_STATS");
		return err;
	}

	return 0;
}

int
ice_dcf_add_del_all_mac_addr(struct ice_dcf_hw *hw, bool add)
{
	struct virtchnl_ether_addr_list *list;
	struct rte_ether_addr *addr;
	struct dcf_virtchnl_cmd args;
	int len, err = 0;

	if (hw->resetting) {
		if (!add)
			return 0;

		PMD_DRV_LOG(ERR, "fail to add all MACs for VF resetting");
		return -EIO;
	}

	len = sizeof(struct virtchnl_ether_addr_list);
	addr = hw->eth_dev->data->mac_addrs;
	len += sizeof(struct virtchnl_ether_addr);

	list = rte_zmalloc(NULL, len, 0);
	if (!list) {
		PMD_DRV_LOG(ERR, "fail to allocate memory");
		return -ENOMEM;
	}

	rte_memcpy(list->list[0].addr, addr->addr_bytes,
			sizeof(addr->addr_bytes));
	PMD_DRV_LOG(DEBUG, "add/rm mac:" RTE_ETHER_ADDR_PRT_FMT,
			    RTE_ETHER_ADDR_BYTES(addr));

	list->vsi_id = hw->vsi_res->vsi_id;
	list->num_elements = 1;

	memset(&args, 0, sizeof(args));
	args.v_op = add ? VIRTCHNL_OP_ADD_ETH_ADDR :
			VIRTCHNL_OP_DEL_ETH_ADDR;
	args.req_msg = (uint8_t *)list;
	args.req_msglen  = len;
	err = ice_dcf_execute_virtchnl_cmd(hw, &args);
	if (err)
		PMD_DRV_LOG(ERR, "fail to execute command %s",
			    add ? "OP_ADD_ETHER_ADDRESS" :
			    "OP_DEL_ETHER_ADDRESS");
	rte_free(list);
	return err;
}
