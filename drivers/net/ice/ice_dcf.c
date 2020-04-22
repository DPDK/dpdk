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
#include <rte_ethdev_driver.h>
#include <rte_ethdev_pci.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_dev.h>

#include "ice_dcf.h"

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
	       VF_BASE_MODE_OFFLOADS;

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
		if ((!desc_cmd.pending && !buff_cmd.pending) ||
		    (!desc_cmd.pending && desc_cmd.v_ret != IAVF_SUCCESS) ||
		    (!buff_cmd.pending && buff_cmd.v_ret != IAVF_SUCCESS))
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

int
ice_dcf_init_hw(struct rte_eth_dev *eth_dev, struct ice_dcf_hw *hw)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	int ret;

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

	hw->eth_dev = eth_dev;
	rte_intr_callback_register(&pci_dev->intr_handle,
				   ice_dcf_dev_interrupt_handler, hw);
	rte_intr_enable(&pci_dev->intr_handle);
	ice_dcf_enable_irq0(hw);

	return 0;

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

	ice_dcf_disable_irq0(hw);
	rte_intr_disable(intr_handle);
	rte_intr_callback_unregister(intr_handle,
				     ice_dcf_dev_interrupt_handler, hw);

	ice_dcf_mode_disable(hw);
	iavf_shutdown_adminq(&hw->avf);

	rte_free(hw->arq_buf);
	rte_free(hw->vf_vsi_map);
	rte_free(hw->vf_res);
}
