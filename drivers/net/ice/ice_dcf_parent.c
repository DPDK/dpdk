/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>

#include <rte_spinlock.h>

#include "ice_dcf_ethdev.h"
#include "ice_generic_flow.h"

#define ICE_DCF_VSI_UPDATE_SERVICE_INTERVAL	100000 /* us */
static rte_spinlock_t vsi_update_lock = RTE_SPINLOCK_INITIALIZER;

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

static void*
ice_dcf_vsi_update_service_handler(void *param)
{
	struct ice_dcf_hw *hw = param;

	usleep(ICE_DCF_VSI_UPDATE_SERVICE_INTERVAL);

	rte_spinlock_lock(&vsi_update_lock);

	if (!ice_dcf_handle_vsi_update_event(hw)) {
		struct ice_dcf_adapter *dcf_ad =
			container_of(hw, struct ice_dcf_adapter, real_hw);

		ice_dcf_update_vf_vsi_map(&dcf_ad->parent.hw,
					  hw->num_vfs, hw->vf_vsi_map);
	}

	rte_spinlock_unlock(&vsi_update_lock);

	return NULL;
}

void
ice_dcf_handle_pf_event_msg(struct ice_dcf_hw *dcf_hw,
			    uint8_t *msg, uint16_t msglen)
{
	struct virtchnl_pf_event *pf_msg = (struct virtchnl_pf_event *)msg;
	pthread_t thread;

	if (msglen < sizeof(struct virtchnl_pf_event)) {
		PMD_DRV_LOG(DEBUG, "Invalid event message length : %u", msglen);
		return;
	}

	switch (pf_msg->event) {
	case VIRTCHNL_EVENT_RESET_IMPENDING:
		PMD_DRV_LOG(DEBUG, "VIRTCHNL_EVENT_RESET_IMPENDING event");
		pthread_create(&thread, NULL,
			       ice_dcf_vsi_update_service_handler, dcf_hw);
		break;
	case VIRTCHNL_EVENT_LINK_CHANGE:
		PMD_DRV_LOG(DEBUG, "VIRTCHNL_EVENT_LINK_CHANGE event");
		break;
	case VIRTCHNL_EVENT_PF_DRIVER_CLOSE:
		PMD_DRV_LOG(DEBUG, "VIRTCHNL_EVENT_PF_DRIVER_CLOSE event");
		break;
	case VIRTCHNL_EVENT_DCF_VSI_MAP_UPDATE:
		PMD_DRV_LOG(DEBUG, "VIRTCHNL_EVENT_DCF_VSI_MAP_UPDATE event : VF%u with VSI num %u",
			    pf_msg->event_data.vf_vsi_map.vf_id,
			    pf_msg->event_data.vf_vsi_map.vsi_id);
		pthread_create(&thread, NULL,
			       ice_dcf_vsi_update_service_handler, dcf_hw);
		break;
	default:
		PMD_DRV_LOG(ERR, "Unknown event received %u", pf_msg->event);
		break;
	}
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
				     ICE_AQC_REPORT_TOPO_CAP, pcaps, NULL);
	ice_free(hw, pcaps);
	if (status)
		goto err_unroll_alloc;

	/* Initialize port_info struct with link information */
	status = ice_aq_get_link_info(hw->port_info, false, NULL, NULL);
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

	return status;
}

static void ice_dcf_uninit_parent_hw(struct ice_hw *hw)
{
	ice_cleanup_fltr_mgmt_struct(hw);

	ice_free_seg(hw);
	ice_free_hw_tbls(hw);

	ice_free(hw, hw->port_info);
	hw->port_info = NULL;

	ice_clear_all_vsi_ctx(hw);
}

static int
ice_dcf_request_pkg_name(struct ice_hw *hw, char *pkg_name)
{
	struct ice_dcf_adapter *dcf_adapter =
			container_of(hw, struct ice_dcf_adapter, parent.hw);
	struct virtchnl_pkg_info pkg_info;
	struct dcf_virtchnl_cmd vc_cmd;
	uint64_t dsn;

	vc_cmd.v_op = VIRTCHNL_OP_DCF_GET_PKG_INFO;
	vc_cmd.req_msglen = 0;
	vc_cmd.req_msg = NULL;
	vc_cmd.rsp_buflen = sizeof(pkg_info);
	vc_cmd.rsp_msgbuf = (uint8_t *)&pkg_info;

	if (ice_dcf_execute_virtchnl_cmd(&dcf_adapter->real_hw, &vc_cmd))
		goto pkg_file_direct;

	rte_memcpy(&dsn, pkg_info.dsn, sizeof(dsn));

	snprintf(pkg_name, ICE_MAX_PKG_FILENAME_SIZE,
		 ICE_PKG_FILE_SEARCH_PATH_UPDATES "ice-%016llx.pkg",
		 (unsigned long long)dsn);
	if (!access(pkg_name, 0))
		return 0;

	snprintf(pkg_name, ICE_MAX_PKG_FILENAME_SIZE,
		 ICE_PKG_FILE_SEARCH_PATH_DEFAULT "ice-%016llx.pkg",
		 (unsigned long long)dsn);
	if (!access(pkg_name, 0))
		return 0;

pkg_file_direct:
	snprintf(pkg_name,
		 ICE_MAX_PKG_FILENAME_SIZE, "%s", ICE_PKG_FILE_UPDATES);
	if (!access(pkg_name, 0))
		return 0;

	snprintf(pkg_name,
		 ICE_MAX_PKG_FILENAME_SIZE, "%s", ICE_PKG_FILE_DEFAULT);
	if (!access(pkg_name, 0))
		return 0;

	return -1;
}

static int
ice_dcf_load_pkg(struct ice_hw *hw)
{
	char pkg_name[ICE_MAX_PKG_FILENAME_SIZE];
	uint8_t *pkg_buf;
	uint32_t buf_len;
	struct stat st;
	FILE *fp;
	int err;

	if (ice_dcf_request_pkg_name(hw, pkg_name)) {
		PMD_INIT_LOG(ERR, "Failed to locate the package file");
		return -ENOENT;
	}

	PMD_INIT_LOG(DEBUG, "DDP package name: %s", pkg_name);

	err = stat(pkg_name, &st);
	if (err) {
		PMD_INIT_LOG(ERR, "Failed to get file status");
		return err;
	}

	buf_len = st.st_size;
	pkg_buf = rte_malloc(NULL, buf_len, 0);
	if (!pkg_buf) {
		PMD_INIT_LOG(ERR, "failed to allocate buffer of size %u for package",
			     buf_len);
		return -1;
	}

	fp = fopen(pkg_name, "rb");
	if (!fp)  {
		PMD_INIT_LOG(ERR, "failed to open file: %s", pkg_name);
		err = -1;
		goto ret;
	}

	err = fread(pkg_buf, buf_len, 1, fp);
	fclose(fp);
	if (err != 1) {
		PMD_INIT_LOG(ERR, "failed to read package data");
		err = -1;
		goto ret;
	}

	err = ice_copy_and_init_pkg(hw, pkg_buf, buf_len);
	if (err)
		PMD_INIT_LOG(ERR, "ice_copy_and_init_hw failed: %d", err);

ret:
	rte_free(pkg_buf);
	return err;
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

	parent_adapter->eth_dev = eth_dev;
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

	err = ice_dcf_load_pkg(parent_hw);
	if (err) {
		PMD_INIT_LOG(ERR, "failed to load package with error %d",
			     err);
		goto uninit_hw;
	}
	parent_adapter->active_pkg_type = ice_load_pkg_type(parent_hw);

	err = ice_flow_init(parent_adapter);
	if (err) {
		PMD_INIT_LOG(ERR, "Failed to initialize flow");
		goto uninit_hw;
	}

	ice_dcf_update_vf_vsi_map(parent_hw, hw->num_vfs, hw->vf_vsi_map);

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

	ice_flow_uninit(parent_adapter);
	ice_dcf_uninit_parent_hw(parent_hw);
}
