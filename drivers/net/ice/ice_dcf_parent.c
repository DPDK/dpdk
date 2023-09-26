/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <rte_spinlock.h>

#include "ice_dcf_ethdev.h"
#include "ice_generic_flow.h"

#define ICE_DCF_VSI_UPDATE_SERVICE_INTERVAL	100000 /* us */
static rte_spinlock_t vsi_update_lock = RTE_SPINLOCK_INITIALIZER;

struct ice_dcf_reset_event_param {
	struct ice_dcf_hw *dcf_hw;

	bool vfr; /* VF reset event */
	uint16_t vf_id; /* The reset VF ID */
};

static __rte_always_inline void
ice_dcf_update_vsi_ctx(struct ice_hw *hw, uint16_t vsi_handle,
		       uint16_t vsi_map)
{
	struct ice_vsi_ctx *vsi_ctx;
	bool first_update = false;
	uint16_t new_vsi_num;

	if (unlikely(vsi_handle >= ICE_MAX_VSI)) {
		PMD_DRV_LOG(ERR, "Invalid vsi handle %u", vsi_handle);
		return;
	}

	vsi_ctx = hw->vsi_ctx[vsi_handle];

	if (vsi_map & VIRTCHNL_DCF_VF_VSI_VALID) {
		if (!vsi_ctx) {
			vsi_ctx = ice_malloc(hw, sizeof(*vsi_ctx));
			if (!vsi_ctx) {
				PMD_DRV_LOG(ERR, "No memory for vsi context %u",
					    vsi_handle);
				return;
			}
			hw->vsi_ctx[vsi_handle] = vsi_ctx;
			first_update = true;
		}

		new_vsi_num = (vsi_map & VIRTCHNL_DCF_VF_VSI_ID_M) >>
			VIRTCHNL_DCF_VF_VSI_ID_S;

		/* Redirect rules if vsi mapping table changes. */
		if (!first_update) {
			struct ice_flow_redirect rd;

			memset(&rd, 0, sizeof(struct ice_flow_redirect));
			rd.type = ICE_FLOW_REDIRECT_VSI;
			rd.vsi_handle = vsi_handle;
			rd.new_vsi_num = new_vsi_num;
			ice_flow_redirect((struct ice_adapter *)hw->back, &rd);
		} else {
			vsi_ctx->vsi_num = new_vsi_num;
		}

		PMD_DRV_LOG(DEBUG, "VF%u is assigned with vsi number %u",
			    vsi_handle, vsi_ctx->vsi_num);
	} else {
		hw->vsi_ctx[vsi_handle] = NULL;

		ice_free(hw, vsi_ctx);

		PMD_DRV_LOG(NOTICE, "VF%u is disabled", vsi_handle);
	}
}

static void
ice_dcf_update_vf_vsi_map(struct ice_hw *hw, uint16_t num_vfs,
			  uint16_t *vf_vsi_map)
{
	uint16_t vf_id;

	for (vf_id = 0; vf_id < num_vfs; vf_id++)
		ice_dcf_update_vsi_ctx(hw, vf_id, vf_vsi_map[vf_id]);
}

static void
ice_dcf_update_pf_vsi_map(struct ice_hw *hw, uint16_t pf_vsi_idx,
			uint16_t pf_vsi_num)
{
	struct ice_vsi_ctx *vsi_ctx;

	if (unlikely(pf_vsi_idx >= ICE_MAX_VSI)) {
		PMD_DRV_LOG(ERR, "Invalid vsi handle %u", pf_vsi_idx);
		return;
	}

	vsi_ctx = hw->vsi_ctx[pf_vsi_idx];

	if (!vsi_ctx)
		vsi_ctx = ice_malloc(hw, sizeof(*vsi_ctx));

	if (!vsi_ctx) {
		PMD_DRV_LOG(ERR, "No memory for vsi context %u",
				pf_vsi_idx);
		return;
	}

	vsi_ctx->vsi_num = pf_vsi_num;
	hw->vsi_ctx[pf_vsi_idx] = vsi_ctx;

	PMD_DRV_LOG(DEBUG, "VF%u is assigned with vsi number %u",
			pf_vsi_idx, vsi_ctx->vsi_num);
}

static uint32_t
ice_dcf_vsi_update_service_handler(void *param)
{
	struct ice_dcf_reset_event_param *reset_param = param;
	struct ice_dcf_hw *hw = reset_param->dcf_hw;
	struct ice_dcf_adapter *adapter =
		container_of(hw, struct ice_dcf_adapter, real_hw);
	struct ice_adapter *parent_adapter = &adapter->parent;

	__atomic_fetch_add(&hw->vsi_update_thread_num, 1,
		__ATOMIC_RELAXED);

	rte_thread_detach(rte_thread_self());

	rte_delay_us(ICE_DCF_VSI_UPDATE_SERVICE_INTERVAL);

	rte_spinlock_lock(&vsi_update_lock);

	if (!ice_dcf_handle_vsi_update_event(hw)) {
		__atomic_store_n(&parent_adapter->dcf_state_on, true,
				 __ATOMIC_RELAXED);
		ice_dcf_update_vf_vsi_map(&adapter->parent.hw,
					  hw->num_vfs, hw->vf_vsi_map);
	}

	if (reset_param->vfr && adapter->repr_infos) {
		struct rte_eth_dev *vf_rep_eth_dev =
			adapter->repr_infos[reset_param->vf_id].vf_rep_eth_dev;
		if (vf_rep_eth_dev && vf_rep_eth_dev->data->dev_started) {
			PMD_DRV_LOG(DEBUG, "VF%u representor is resetting",
				    reset_param->vf_id);
			ice_dcf_vf_repr_init_vlan(vf_rep_eth_dev);
		}
	}

	if (hw->tm_conf.committed)
		ice_dcf_replay_vf_bw(hw, reset_param->vf_id);

	rte_spinlock_unlock(&vsi_update_lock);

	free(param);

	__atomic_fetch_sub(&hw->vsi_update_thread_num, 1,
		__ATOMIC_RELEASE);

	return 0;
}

static void
start_vsi_reset_thread(struct ice_dcf_hw *dcf_hw, bool vfr, uint16_t vf_id)
{
	struct ice_dcf_reset_event_param *param;
	char name[RTE_THREAD_INTERNAL_NAME_SIZE];
	rte_thread_t thread;
	int ret;

	param = malloc(sizeof(*param));
	if (!param) {
		PMD_DRV_LOG(ERR, "Failed to allocate the memory for reset handling");
		return;
	}

	param->dcf_hw = dcf_hw;
	param->vfr = vfr;
	param->vf_id = vf_id;

	snprintf(name, sizeof(name), "ice-rst%u", vf_id);
	ret = rte_thread_create_internal_control(&thread, name,
				     ice_dcf_vsi_update_service_handler, param);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to start the thread for reset handling");
		free(param);
	}
}

static uint32_t
ice_dcf_convert_link_speed(enum virtchnl_link_speed virt_link_speed)
{
	uint32_t speed;

	switch (virt_link_speed) {
	case VIRTCHNL_LINK_SPEED_100MB:
		speed = 100;
		break;
	case VIRTCHNL_LINK_SPEED_1GB:
		speed = 1000;
		break;
	case VIRTCHNL_LINK_SPEED_10GB:
		speed = 10000;
		break;
	case VIRTCHNL_LINK_SPEED_40GB:
		speed = 40000;
		break;
	case VIRTCHNL_LINK_SPEED_20GB:
		speed = 20000;
		break;
	case VIRTCHNL_LINK_SPEED_25GB:
		speed = 25000;
		break;
	case VIRTCHNL_LINK_SPEED_2_5GB:
		speed = 2500;
		break;
	case VIRTCHNL_LINK_SPEED_5GB:
		speed = 5000;
		break;
	default:
		speed = 0;
		break;
	}

	return speed;
}

void
ice_dcf_handle_pf_event_msg(struct ice_dcf_hw *dcf_hw,
			    uint8_t *msg, uint16_t msglen)
{
	struct virtchnl_pf_event *pf_msg = (struct virtchnl_pf_event *)msg;
	struct ice_dcf_adapter *adapter =
		container_of(dcf_hw, struct ice_dcf_adapter, real_hw);
	struct ice_adapter *parent_adapter = &adapter->parent;

	if (msglen < sizeof(struct virtchnl_pf_event)) {
		PMD_DRV_LOG(DEBUG, "Invalid event message length : %u", msglen);
		return;
	}

	switch (pf_msg->event) {
	case VIRTCHNL_EVENT_RESET_IMPENDING:
		PMD_DRV_LOG(DEBUG, "VIRTCHNL_EVENT_RESET_IMPENDING event");
		dcf_hw->resetting = true;
		break;
	case VIRTCHNL_EVENT_LINK_CHANGE:
		PMD_DRV_LOG(DEBUG, "VIRTCHNL_EVENT_LINK_CHANGE event");
		dcf_hw->link_up = pf_msg->event_data.link_event.link_status;
		if (dcf_hw->vf_res->vf_cap_flags &
			VIRTCHNL_VF_CAP_ADV_LINK_SPEED) {
			dcf_hw->link_speed =
				pf_msg->event_data.link_event_adv.link_speed;
		} else {
			enum virtchnl_link_speed speed;
			speed = pf_msg->event_data.link_event.link_speed;
			dcf_hw->link_speed = ice_dcf_convert_link_speed(speed);
		}
		ice_dcf_link_update(dcf_hw->eth_dev, 0);
		rte_eth_dev_callback_process(dcf_hw->eth_dev,
			RTE_ETH_EVENT_INTR_LSC, NULL);
		break;
	case VIRTCHNL_EVENT_PF_DRIVER_CLOSE:
		PMD_DRV_LOG(DEBUG, "VIRTCHNL_EVENT_PF_DRIVER_CLOSE event");
		break;
	case VIRTCHNL_EVENT_DCF_VSI_MAP_UPDATE:
		PMD_DRV_LOG(DEBUG, "VIRTCHNL_EVENT_DCF_VSI_MAP_UPDATE event : VF%u with VSI num %u",
			    pf_msg->event_data.vf_vsi_map.vf_id,
			    pf_msg->event_data.vf_vsi_map.vsi_id);
		__atomic_store_n(&parent_adapter->dcf_state_on, false,
				 __ATOMIC_RELAXED);
		start_vsi_reset_thread(dcf_hw, true,
				       pf_msg->event_data.vf_vsi_map.vf_id);
		break;
	default:
		PMD_DRV_LOG(ERR, "Unknown event received %u", pf_msg->event);
		break;
	}
}

static int
ice_dcf_query_port_ets(struct ice_hw *parent_hw, struct ice_dcf_hw *real_hw)
{
	int ret;

	real_hw->ets_config = (struct ice_aqc_port_ets_elem *)
			ice_malloc(real_hw, sizeof(*real_hw->ets_config));
	if (!real_hw->ets_config)
		return ICE_ERR_NO_MEMORY;

	ret = ice_aq_query_port_ets(parent_hw->port_info,
			real_hw->ets_config, sizeof(*real_hw->ets_config),
			NULL);
	if (ret) {
		PMD_DRV_LOG(ERR, "DCF Query Port ETS failed");
		rte_free(real_hw->ets_config);
		real_hw->ets_config = NULL;
		return ret;
	}

	return ICE_SUCCESS;
}

static int
ice_dcf_init_parent_hw(struct ice_hw *hw)
{
	struct ice_aqc_get_phy_caps_data *pcaps;
	enum ice_status status;

	status = ice_aq_get_fw_ver(hw, NULL);
	if (status)
		return status;

	status = ice_get_caps(hw);
	if (status)
		return status;

	hw->port_info = (struct ice_port_info *)
			ice_malloc(hw, sizeof(*hw->port_info));
	if (!hw->port_info)
		return ICE_ERR_NO_MEMORY;

	/* set the back pointer to HW */
	hw->port_info->hw = hw;

	/* Initialize port_info struct with switch configuration data */
	status = ice_get_initial_sw_cfg(hw);
	if (status)
		goto err_unroll_alloc;

	pcaps = (struct ice_aqc_get_phy_caps_data *)
		ice_malloc(hw, sizeof(*pcaps));
	if (!pcaps) {
		status = ICE_ERR_NO_MEMORY;
		goto err_unroll_alloc;
	}

	/* Initialize port_info struct with PHY capabilities */
	status = ice_aq_get_phy_caps(hw->port_info, false,
				     ICE_AQC_REPORT_TOPO_CAP_MEDIA, pcaps, NULL);
	ice_free(hw, pcaps);
	if (status)
		goto err_unroll_alloc;

	/* Initialize port_info struct with link information */
	status = ice_aq_get_link_info(hw->port_info, true, NULL, NULL);
	if (status)
		goto err_unroll_alloc;

	status = ice_init_fltr_mgmt_struct(hw);
	if (status)
		goto err_unroll_alloc;

	status = ice_init_hw_tbls(hw);
	if (status)
		goto err_unroll_fltr_mgmt_struct;

	PMD_INIT_LOG(INFO,
		     "firmware %d.%d.%d api %d.%d.%d build 0x%08x",
		     hw->fw_maj_ver, hw->fw_min_ver, hw->fw_patch,
		     hw->api_maj_ver, hw->api_min_ver, hw->api_patch,
		     hw->fw_build);

	return ICE_SUCCESS;

err_unroll_fltr_mgmt_struct:
	ice_cleanup_fltr_mgmt_struct(hw);
err_unroll_alloc:
	ice_free(hw, hw->port_info);
	hw->port_info = NULL;
	hw->switch_info = NULL;

	return status;
}

static void ice_dcf_uninit_parent_hw(struct ice_hw *hw)
{
	ice_cleanup_fltr_mgmt_struct(hw);

	ice_free_seg(hw);
	ice_free_hw_tbls(hw);

	ice_free(hw, hw->port_info);
	hw->port_info = NULL;
	hw->switch_info = NULL;

	ice_clear_all_vsi_ctx(hw);
}

static int
ice_dcf_load_pkg(struct ice_adapter *adapter)
{
	struct ice_dcf_adapter *dcf_adapter =
			container_of(&adapter->hw, struct ice_dcf_adapter, parent.hw);
	struct virtchnl_pkg_info pkg_info;
	struct dcf_virtchnl_cmd vc_cmd;
	bool use_dsn;
	uint64_t dsn = 0;

	vc_cmd.v_op = VIRTCHNL_OP_DCF_GET_PKG_INFO;
	vc_cmd.req_msglen = 0;
	vc_cmd.req_msg = NULL;
	vc_cmd.rsp_buflen = sizeof(pkg_info);
	vc_cmd.rsp_msgbuf = (uint8_t *)&pkg_info;

	use_dsn = ice_dcf_execute_virtchnl_cmd(&dcf_adapter->real_hw, &vc_cmd) == 0;
	if (use_dsn)
		rte_memcpy(&dsn, pkg_info.dsn, sizeof(dsn));

	return ice_load_pkg(adapter, use_dsn, dsn);
}

int
ice_dcf_init_parent_adapter(struct rte_eth_dev *eth_dev)
{
	struct ice_dcf_adapter *adapter = eth_dev->data->dev_private;
	struct ice_adapter *parent_adapter = &adapter->parent;
	struct ice_hw *parent_hw = &parent_adapter->hw;
	struct ice_dcf_hw *hw = &adapter->real_hw;
	const struct rte_ether_addr *mac;
	int err;

	parent_adapter->pf.adapter = parent_adapter;
	parent_adapter->pf.dev_data = eth_dev->data;
	/* create a dummy main_vsi */
	parent_adapter->pf.main_vsi =
		rte_zmalloc(NULL, sizeof(struct ice_vsi), 0);
	if (!parent_adapter->pf.main_vsi)
		return -ENOMEM;
	parent_adapter->pf.main_vsi->adapter = parent_adapter;
	parent_adapter->pf.adapter_stopped = 1;

	parent_hw->back = parent_adapter;
	parent_hw->mac_type = ICE_MAC_GENERIC;
	parent_hw->vendor_id = ICE_INTEL_VENDOR_ID;

	ice_init_lock(&parent_hw->adminq.sq_lock);
	ice_init_lock(&parent_hw->adminq.rq_lock);
	parent_hw->aq_send_cmd_fn = ice_dcf_send_aq_cmd;
	parent_hw->aq_send_cmd_param = &adapter->real_hw;
	parent_hw->dcf_enabled = true;

	err = ice_dcf_init_parent_hw(parent_hw);
	if (err) {
		PMD_INIT_LOG(ERR, "failed to init the DCF parent hardware with error %d",
			     err);
		return err;
	}

	if (hw->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_QOS) {
		err = ice_dcf_query_port_ets(parent_hw, hw);
		if (err) {
			PMD_INIT_LOG(ERR, "failed to query port ets with error %d",
				     err);
			goto uninit_hw;
		}
	}

	err = ice_dcf_load_pkg(parent_adapter);
	if (err) {
		PMD_INIT_LOG(ERR, "failed to load package with error %d",
			     err);
		goto uninit_hw;
	}

	parent_adapter->pf.main_vsi->idx = hw->num_vfs;
	ice_dcf_update_pf_vsi_map(parent_hw,
			parent_adapter->pf.main_vsi->idx, hw->pf_vsi_id);

	ice_dcf_update_vf_vsi_map(parent_hw, hw->num_vfs, hw->vf_vsi_map);

	if (ice_devargs_check(eth_dev->device->devargs, ICE_DCF_DEVARG_ACL))
		parent_adapter->disabled_engine_mask |= BIT(ICE_FLOW_ENGINE_ACL);

	parent_adapter->disabled_engine_mask |= BIT(ICE_FLOW_ENGINE_FDIR);
	parent_adapter->disabled_engine_mask |= BIT(ICE_FLOW_ENGINE_HASH);

	err = ice_flow_init(parent_adapter);
	if (err) {
		PMD_INIT_LOG(ERR, "Failed to initialize flow");
		goto uninit_hw;
	}

	mac = (const struct rte_ether_addr *)hw->avf.mac.addr;
	if (rte_is_valid_assigned_ether_addr(mac))
		rte_ether_addr_copy(mac, &parent_adapter->pf.dev_addr);
	else
		rte_eth_random_addr(parent_adapter->pf.dev_addr.addr_bytes);

	eth_dev->data->mac_addrs = &parent_adapter->pf.dev_addr;

	return 0;

uninit_hw:
	ice_dcf_uninit_parent_hw(parent_hw);
	return err;
}

void
ice_dcf_uninit_parent_adapter(struct rte_eth_dev *eth_dev)
{
	struct ice_dcf_adapter *adapter = eth_dev->data->dev_private;
	struct ice_adapter *parent_adapter = &adapter->parent;
	struct ice_hw *parent_hw = &parent_adapter->hw;

	eth_dev->data->mac_addrs = NULL;
	rte_free(parent_adapter->pf.main_vsi);
	parent_adapter->pf.main_vsi = NULL;

	ice_flow_uninit(parent_adapter);
	ice_dcf_uninit_parent_hw(parent_hw);
}
