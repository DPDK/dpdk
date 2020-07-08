/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2016 - 2018 Cavium Inc.
 * All rights reserved.
 * www.cavium.com
 */

#include <limits.h>
#include <rte_alarm.h>
#include <rte_string_fns.h>

#include "qede_ethdev.h"
/* ######### DEBUG ###########*/
#include "qede_debug.h"

/* Alarm timeout. */
#define QEDE_ALARM_TIMEOUT_US 100000

/* Global variable to hold absolute path of fw file */
char qede_fw_file[PATH_MAX];

static const char * const QEDE_DEFAULT_FIRMWARE =
	"/lib/firmware/qed/qed_init_values-8.40.33.0.bin";

static void
qed_update_pf_params(struct ecore_dev *edev, struct ecore_pf_params *params)
{
	int i;

	for (i = 0; i < edev->num_hwfns; i++) {
		struct ecore_hwfn *p_hwfn = &edev->hwfns[i];
		p_hwfn->pf_params = *params;
	}
}

static void qed_init_pci(struct ecore_dev *edev, struct rte_pci_device *pci_dev)
{
	edev->regview = pci_dev->mem_resource[0].addr;
	edev->doorbells = pci_dev->mem_resource[2].addr;
	edev->db_size = pci_dev->mem_resource[2].len;
}

static int
qed_probe(struct ecore_dev *edev, struct rte_pci_device *pci_dev,
	  uint32_t dp_module, uint8_t dp_level, bool is_vf)
{
	struct ecore_hw_prepare_params hw_prepare_params;
	int rc;

	ecore_init_struct(edev);
	edev->drv_type = DRV_ID_DRV_TYPE_LINUX;
	/* Protocol type is always fixed to PROTOCOL_ETH */

	if (is_vf)
		edev->b_is_vf = true;

	ecore_init_dp(edev, dp_module, dp_level, NULL);
	qed_init_pci(edev, pci_dev);

	memset(&hw_prepare_params, 0, sizeof(hw_prepare_params));

	if (is_vf)
		hw_prepare_params.acquire_retry_cnt = ECORE_VF_ACQUIRE_THRESH;

	hw_prepare_params.personality = ECORE_PCI_ETH;
	hw_prepare_params.drv_resc_alloc = false;
	hw_prepare_params.chk_reg_fifo = false;
	hw_prepare_params.initiate_pf_flr = true;
	hw_prepare_params.allow_mdump = false;
	hw_prepare_params.b_en_pacing = false;
	hw_prepare_params.epoch = OSAL_GET_EPOCH(ECORE_LEADING_HWFN(edev));
	rc = ecore_hw_prepare(edev, &hw_prepare_params);
	if (rc) {
		DP_ERR(edev, "hw prepare failed\n");
		return rc;
	}

	return rc;
}

static int qed_nic_setup(struct ecore_dev *edev)
{
	int rc;

	rc = ecore_resc_alloc(edev);
	if (rc)
		return rc;

	DP_INFO(edev, "Allocated qed resources\n");
	ecore_resc_setup(edev);

	return rc;
}

#ifdef CONFIG_ECORE_ZIPPED_FW
static int qed_alloc_stream_mem(struct ecore_dev *edev)
{
	int i;

	for_each_hwfn(edev, i) {
		struct ecore_hwfn *p_hwfn = &edev->hwfns[i];

		p_hwfn->stream = OSAL_ZALLOC(p_hwfn->p_dev, GFP_KERNEL,
					     sizeof(*p_hwfn->stream));
		if (!p_hwfn->stream)
			return -ENOMEM;
	}

	return 0;
}

static void qed_free_stream_mem(struct ecore_dev *edev)
{
	int i;

	for_each_hwfn(edev, i) {
		struct ecore_hwfn *p_hwfn = &edev->hwfns[i];

		if (!p_hwfn->stream)
			return;

		OSAL_FREE(p_hwfn->p_dev, p_hwfn->stream);
	}
}
#endif

#ifdef CONFIG_ECORE_BINARY_FW
static int qed_load_firmware_data(struct ecore_dev *edev)
{
	int fd;
	struct stat st;
	const char *fw = RTE_LIBRTE_QEDE_FW;

	if (strcmp(fw, "") == 0)
		strcpy(qede_fw_file, QEDE_DEFAULT_FIRMWARE);
	else
		strcpy(qede_fw_file, fw);

	fd = open(qede_fw_file, O_RDONLY);
	if (fd < 0) {
		DP_ERR(edev, "Can't open firmware file\n");
		return -ENOENT;
	}

	if (fstat(fd, &st) < 0) {
		DP_ERR(edev, "Can't stat firmware file\n");
		close(fd);
		return -1;
	}

	edev->firmware = rte_zmalloc("qede_fw", st.st_size,
				    RTE_CACHE_LINE_SIZE);
	if (!edev->firmware) {
		DP_ERR(edev, "Can't allocate memory for firmware\n");
		close(fd);
		return -ENOMEM;
	}

	if (read(fd, edev->firmware, st.st_size) != st.st_size) {
		DP_ERR(edev, "Can't read firmware data\n");
		close(fd);
		return -1;
	}

	edev->fw_len = st.st_size;
	if (edev->fw_len < 104) {
		DP_ERR(edev, "Invalid fw size: %" PRIu64 "\n",
			  edev->fw_len);
		close(fd);
		return -EINVAL;
	}

	close(fd);
	return 0;
}
#endif

static void qed_handle_bulletin_change(struct ecore_hwfn *hwfn)
{
	uint8_t mac[ETH_ALEN], is_mac_exist, is_mac_forced;

	is_mac_exist = ecore_vf_bulletin_get_forced_mac(hwfn, mac,
						      &is_mac_forced);
	if (is_mac_exist && is_mac_forced)
		rte_memcpy(hwfn->hw_info.hw_mac_addr, mac, ETH_ALEN);

	/* Always update link configuration according to bulletin */
	qed_link_update(hwfn);
}

static void qede_vf_task(void *arg)
{
	struct ecore_hwfn *p_hwfn = arg;
	uint8_t change = 0;

	/* Read the bulletin board, and re-schedule the task */
	ecore_vf_read_bulletin(p_hwfn, &change);
	if (change)
		qed_handle_bulletin_change(p_hwfn);

	rte_eal_alarm_set(QEDE_ALARM_TIMEOUT_US, qede_vf_task, p_hwfn);
}

static void qed_start_iov_task(struct ecore_dev *edev)
{
	struct ecore_hwfn *p_hwfn;
	int i;

	for_each_hwfn(edev, i) {
		p_hwfn = &edev->hwfns[i];
		if (!IS_PF(edev))
			rte_eal_alarm_set(QEDE_ALARM_TIMEOUT_US, qede_vf_task,
					  p_hwfn);
	}
}

static void qed_stop_iov_task(struct ecore_dev *edev)
{
	struct ecore_hwfn *p_hwfn;
	int i;

	for_each_hwfn(edev, i) {
		p_hwfn = &edev->hwfns[i];
		if (!IS_PF(edev))
			rte_eal_alarm_cancel(qede_vf_task, p_hwfn);
	}
}
static int qed_slowpath_start(struct ecore_dev *edev,
			      struct qed_slowpath_params *params)
{
	struct ecore_drv_load_params drv_load_params;
	struct ecore_hw_init_params hw_init_params;
	struct ecore_mcp_drv_version drv_version;
	const uint8_t *data = NULL;
	struct ecore_hwfn *hwfn;
	struct ecore_ptt *p_ptt;
	int rc;

	if (IS_PF(edev)) {
#ifdef CONFIG_ECORE_BINARY_FW
		rc = qed_load_firmware_data(edev);
		if (rc) {
			DP_ERR(edev, "Failed to find fw file %s\n",
				qede_fw_file);
			goto err;
		}
#endif
		hwfn = ECORE_LEADING_HWFN(edev);
		if (edev->num_hwfns == 1) { /* skip aRFS for 100G device */
			p_ptt = ecore_ptt_acquire(hwfn);
			if (p_ptt) {
				ECORE_LEADING_HWFN(edev)->p_arfs_ptt = p_ptt;
			} else {
				DP_ERR(edev, "Failed to acquire PTT for flowdir\n");
				rc = -ENOMEM;
				goto err;
			}
		}
	}

	rc = qed_nic_setup(edev);
	if (rc)
		goto err;

	/* set int_coalescing_mode */
	edev->int_coalescing_mode = ECORE_COAL_MODE_ENABLE;

#ifdef CONFIG_ECORE_ZIPPED_FW
	if (IS_PF(edev)) {
		/* Allocate stream for unzipping */
		rc = qed_alloc_stream_mem(edev);
		if (rc) {
			DP_ERR(edev, "Failed to allocate stream memory\n");
			goto err1;
		}
	}
#endif

	qed_start_iov_task(edev);

#ifdef CONFIG_ECORE_BINARY_FW
	if (IS_PF(edev)) {
		data = (const uint8_t *)edev->firmware + sizeof(u32);

		/* ############### DEBUG ################## */
		qed_dbg_pf_init(edev);
	}
#endif


	/* Start the slowpath */
	memset(&hw_init_params, 0, sizeof(hw_init_params));
	hw_init_params.b_hw_start = true;
	hw_init_params.int_mode = params->int_mode;
	hw_init_params.allow_npar_tx_switch = true;
	hw_init_params.bin_fw_data = data;

	memset(&drv_load_params, 0, sizeof(drv_load_params));
	drv_load_params.mfw_timeout_val = ECORE_LOAD_REQ_LOCK_TO_DEFAULT;
	drv_load_params.avoid_eng_reset = false;
	drv_load_params.override_force_load = ECORE_OVERRIDE_FORCE_LOAD_ALWAYS;
	hw_init_params.avoid_eng_affin = false;
	hw_init_params.p_drv_load_params = &drv_load_params;

	rc = ecore_hw_init(edev, &hw_init_params);
	if (rc) {
		DP_ERR(edev, "ecore_hw_init failed\n");
		goto err2;
	}

	DP_INFO(edev, "HW inited and function started\n");

	if (IS_PF(edev)) {
		hwfn = ECORE_LEADING_HWFN(edev);
		drv_version.version = (params->drv_major << 24) |
		    (params->drv_minor << 16) |
		    (params->drv_rev << 8) | (params->drv_eng);
		strlcpy((char *)drv_version.name, (const char *)params->name,
			sizeof(drv_version.name));
		rc = ecore_mcp_send_drv_version(hwfn, hwfn->p_main_ptt,
						&drv_version);
		if (rc) {
			DP_ERR(edev, "Failed sending drv version command\n");
			goto err3;
		}
	}

	ecore_reset_vport_stats(edev);

	return 0;

err3:
	ecore_hw_stop(edev);
err2:
	qed_stop_iov_task(edev);
#ifdef CONFIG_ECORE_ZIPPED_FW
	qed_free_stream_mem(edev);
err1:
#endif
	ecore_resc_free(edev);
err:
#ifdef CONFIG_ECORE_BINARY_FW
	if (IS_PF(edev)) {
		if (edev->firmware)
			rte_free(edev->firmware);
		edev->firmware = NULL;
	}
#endif
	qed_stop_iov_task(edev);

	return rc;
}

static int
qed_fill_dev_info(struct ecore_dev *edev, struct qed_dev_info *dev_info)
{
	struct ecore_hwfn *p_hwfn = ECORE_LEADING_HWFN(edev);
	struct ecore_ptt *ptt = NULL;
	struct ecore_tunnel_info *tun = &edev->tunnel;

	memset(dev_info, 0, sizeof(struct qed_dev_info));

	if (tun->vxlan.tun_cls == ECORE_TUNN_CLSS_MAC_VLAN &&
	    tun->vxlan.b_mode_enabled)
		dev_info->vxlan_enable = true;

	if (tun->l2_gre.b_mode_enabled && tun->ip_gre.b_mode_enabled &&
	    tun->l2_gre.tun_cls == ECORE_TUNN_CLSS_MAC_VLAN &&
	    tun->ip_gre.tun_cls == ECORE_TUNN_CLSS_MAC_VLAN)
		dev_info->gre_enable = true;

	if (tun->l2_geneve.b_mode_enabled && tun->ip_geneve.b_mode_enabled &&
	    tun->l2_geneve.tun_cls == ECORE_TUNN_CLSS_MAC_VLAN &&
	    tun->ip_geneve.tun_cls == ECORE_TUNN_CLSS_MAC_VLAN)
		dev_info->geneve_enable = true;

	dev_info->num_hwfns = edev->num_hwfns;
	dev_info->is_mf_default = IS_MF_DEFAULT(&edev->hwfns[0]);
	dev_info->mtu = ECORE_LEADING_HWFN(edev)->hw_info.mtu;
	dev_info->dev_type = edev->type;

	rte_memcpy(&dev_info->hw_mac, &edev->hwfns[0].hw_info.hw_mac_addr,
	       RTE_ETHER_ADDR_LEN);

	dev_info->fw_major = FW_MAJOR_VERSION;
	dev_info->fw_minor = FW_MINOR_VERSION;
	dev_info->fw_rev = FW_REVISION_VERSION;
	dev_info->fw_eng = FW_ENGINEERING_VERSION;

	if (IS_PF(edev)) {
		dev_info->b_inter_pf_switch =
			OSAL_GET_BIT(ECORE_MF_INTER_PF_SWITCH, &edev->mf_bits);
		if (!OSAL_GET_BIT(ECORE_MF_DISABLE_ARFS, &edev->mf_bits))
			dev_info->b_arfs_capable = true;
		dev_info->tx_switching = false;

		dev_info->smart_an = ecore_mcp_is_smart_an_supported(p_hwfn);

		ptt = ecore_ptt_acquire(ECORE_LEADING_HWFN(edev));
		if (ptt) {
			ecore_mcp_get_mfw_ver(ECORE_LEADING_HWFN(edev), ptt,
					      &dev_info->mfw_rev, NULL);

			ecore_mcp_get_mbi_ver(ECORE_LEADING_HWFN(edev), ptt,
					      &dev_info->mbi_version);

			ecore_mcp_get_flash_size(ECORE_LEADING_HWFN(edev), ptt,
						 &dev_info->flash_size);

			/* Workaround to allow PHY-read commands for
			 * B0 bringup.
			 */
			if (ECORE_IS_BB_B0(edev))
				dev_info->flash_size = 0xffffffff;

			ecore_ptt_release(ECORE_LEADING_HWFN(edev), ptt);
		}
	} else {
		ecore_mcp_get_mfw_ver(ECORE_LEADING_HWFN(edev), ptt,
				      &dev_info->mfw_rev, NULL);
	}

	return 0;
}

int
qed_fill_eth_dev_info(struct ecore_dev *edev, struct qed_dev_eth_info *info)
{
	uint8_t queues = 0;
	int i;

	memset(info, 0, sizeof(*info));

	info->num_tc = 1 /* @@@TBD aelior MULTI_COS */;

	if (IS_PF(edev)) {
		int max_vf_vlan_filters = 0;

		info->num_queues = 0;
		for_each_hwfn(edev, i)
			info->num_queues +=
			FEAT_NUM(&edev->hwfns[i], ECORE_PF_L2_QUE);

		if (IS_ECORE_SRIOV(edev))
			max_vf_vlan_filters = edev->p_iov_info->total_vfs *
					      ECORE_ETH_VF_NUM_VLAN_FILTERS;
		info->num_vlan_filters = RESC_NUM(&edev->hwfns[0], ECORE_VLAN) -
					 max_vf_vlan_filters;

		rte_memcpy(&info->port_mac, &edev->hwfns[0].hw_info.hw_mac_addr,
			   RTE_ETHER_ADDR_LEN);
	} else {
		ecore_vf_get_num_rxqs(ECORE_LEADING_HWFN(edev),
				      &info->num_queues);
		if (ECORE_IS_CMT(edev)) {
			ecore_vf_get_num_rxqs(&edev->hwfns[1], &queues);
			info->num_queues += queues;
		}

		ecore_vf_get_num_vlan_filters(&edev->hwfns[0],
					      (u8 *)&info->num_vlan_filters);

		ecore_vf_get_port_mac(&edev->hwfns[0],
				      (uint8_t *)&info->port_mac);

		info->is_legacy = ecore_vf_get_pre_fp_hsi(&edev->hwfns[0]);
	}

	qed_fill_dev_info(edev, &info->common);

	if (IS_VF(edev))
		memset(&info->common.hw_mac, 0, RTE_ETHER_ADDR_LEN);

	return 0;
}

static void qed_set_name(struct ecore_dev *edev, char name[NAME_SIZE])
{
	int i;

	rte_memcpy(edev->name, name, NAME_SIZE);
	for_each_hwfn(edev, i) {
		snprintf(edev->hwfns[i].name, NAME_SIZE, "%s-%d", name, i);
	}
}

static uint32_t
qed_sb_init(struct ecore_dev *edev, struct ecore_sb_info *sb_info,
	    void *sb_virt_addr, dma_addr_t sb_phy_addr, uint16_t sb_id)
{
	struct ecore_hwfn *p_hwfn;
	int hwfn_index;
	uint16_t rel_sb_id;
	uint8_t n_hwfns = edev->num_hwfns;
	uint32_t rc;

	hwfn_index = sb_id % n_hwfns;
	p_hwfn = &edev->hwfns[hwfn_index];
	rel_sb_id = sb_id / n_hwfns;

	DP_INFO(edev, "hwfn [%d] <--[init]-- SB %04x [0x%04x upper]\n",
		hwfn_index, rel_sb_id, sb_id);

	rc = ecore_int_sb_init(p_hwfn, p_hwfn->p_main_ptt, sb_info,
			       sb_virt_addr, sb_phy_addr, rel_sb_id);

	return rc;
}

static void qed_fill_link(struct ecore_hwfn *hwfn,
			  __rte_unused struct ecore_ptt *ptt,
			  struct qed_link_output *if_link)
{
	struct ecore_mcp_link_params params;
	struct ecore_mcp_link_state link;
	struct ecore_mcp_link_capabilities link_caps;
	uint8_t change = 0;

	memset(if_link, 0, sizeof(*if_link));

	/* Prepare source inputs */
	if (IS_PF(hwfn->p_dev)) {
		rte_memcpy(&params, ecore_mcp_get_link_params(hwfn),
		       sizeof(params));
		rte_memcpy(&link, ecore_mcp_get_link_state(hwfn), sizeof(link));
		rte_memcpy(&link_caps, ecore_mcp_get_link_capabilities(hwfn),
		       sizeof(link_caps));
	} else {
		ecore_vf_read_bulletin(hwfn, &change);
		ecore_vf_get_link_params(hwfn, &params);
		ecore_vf_get_link_state(hwfn, &link);
		ecore_vf_get_link_caps(hwfn, &link_caps);
	}

	/* Set the link parameters to pass to protocol driver */
	if (link.link_up)
		if_link->link_up = true;

	if (link.link_up)
		if_link->speed = link.speed;

	if_link->duplex = QEDE_DUPLEX_FULL;

	/* Fill up the native advertised speed cap mask */
	if_link->adv_speed = params.speed.advertised_speeds;

	if (params.speed.autoneg)
		if_link->supported_caps |= QEDE_SUPPORTED_AUTONEG;

	if (params.pause.autoneg || params.pause.forced_rx ||
	    params.pause.forced_tx)
		if_link->supported_caps |= QEDE_SUPPORTED_PAUSE;

	if (params.pause.autoneg)
		if_link->pause_config |= QED_LINK_PAUSE_AUTONEG_ENABLE;

	if (params.pause.forced_rx)
		if_link->pause_config |= QED_LINK_PAUSE_RX_ENABLE;

	if (params.pause.forced_tx)
		if_link->pause_config |= QED_LINK_PAUSE_TX_ENABLE;

	if (link_caps.default_eee == ECORE_MCP_EEE_UNSUPPORTED) {
		if_link->eee_supported = false;
	} else {
		if_link->eee_supported = true;
		if_link->eee_active = link.eee_active;
		if_link->sup_caps = link_caps.eee_speed_caps;
		/* MFW clears adv_caps on eee disable; use configured value */
		if_link->eee.adv_caps = link.eee_adv_caps ? link.eee_adv_caps :
					params.eee.adv_caps;
		if_link->eee.lp_adv_caps = link.eee_lp_adv_caps;
		if_link->eee.enable = params.eee.enable;
		if_link->eee.tx_lpi_enable = params.eee.tx_lpi_enable;
		if_link->eee.tx_lpi_timer = params.eee.tx_lpi_timer;
	}
}

static void
qed_get_current_link(struct ecore_dev *edev, struct qed_link_output *if_link)
{
	struct ecore_hwfn *hwfn;
	struct ecore_ptt *ptt;

	hwfn = &edev->hwfns[0];
	if (IS_PF(edev)) {
		ptt = ecore_ptt_acquire(hwfn);
		if (!ptt)
			DP_NOTICE(hwfn, true, "Failed to fill link; No PTT\n");

			qed_fill_link(hwfn, ptt, if_link);

		if (ptt)
			ecore_ptt_release(hwfn, ptt);
	} else {
		qed_fill_link(hwfn, NULL, if_link);
	}
}

static int qed_set_link(struct ecore_dev *edev, struct qed_link_params *params)
{
	struct ecore_hwfn *hwfn;
	struct ecore_ptt *ptt;
	struct ecore_mcp_link_params *link_params;
	int rc;

	if (IS_VF(edev))
		return 0;

	/* The link should be set only once per PF */
	hwfn = &edev->hwfns[0];

	ptt = ecore_ptt_acquire(hwfn);
	if (!ptt)
		return -EBUSY;

	link_params = ecore_mcp_get_link_params(hwfn);
	if (params->override_flags & QED_LINK_OVERRIDE_SPEED_AUTONEG)
		link_params->speed.autoneg = params->autoneg;

	if (params->override_flags & QED_LINK_OVERRIDE_PAUSE_CONFIG) {
		if (params->pause_config & QED_LINK_PAUSE_AUTONEG_ENABLE)
			link_params->pause.autoneg = true;
		else
			link_params->pause.autoneg = false;
		if (params->pause_config & QED_LINK_PAUSE_RX_ENABLE)
			link_params->pause.forced_rx = true;
		else
			link_params->pause.forced_rx = false;
		if (params->pause_config & QED_LINK_PAUSE_TX_ENABLE)
			link_params->pause.forced_tx = true;
		else
			link_params->pause.forced_tx = false;
	}

	if (params->override_flags & QED_LINK_OVERRIDE_EEE_CONFIG)
		memcpy(&link_params->eee, &params->eee,
		       sizeof(link_params->eee));

	rc = ecore_mcp_set_link(hwfn, ptt, params->link_up);

	ecore_ptt_release(hwfn, ptt);

	return rc;
}

void qed_link_update(struct ecore_hwfn *hwfn)
{
	struct ecore_dev *edev = hwfn->p_dev;
	struct qede_dev *qdev = (struct qede_dev *)edev;
	struct rte_eth_dev *dev = (struct rte_eth_dev *)qdev->ethdev;

	if (!qede_link_update(dev, 0))
		_rte_eth_dev_callback_process(dev,
					      RTE_ETH_EVENT_INTR_LSC, NULL);
}

static int qed_drain(struct ecore_dev *edev)
{
	struct ecore_hwfn *hwfn;
	struct ecore_ptt *ptt;
	int i, rc;

	if (IS_VF(edev))
		return 0;

	for_each_hwfn(edev, i) {
		hwfn = &edev->hwfns[i];
		ptt = ecore_ptt_acquire(hwfn);
		if (!ptt) {
			DP_ERR(hwfn, "Failed to drain NIG; No PTT\n");
			return -EBUSY;
		}
		rc = ecore_mcp_drain(hwfn, ptt);
		if (rc)
			return rc;
		ecore_ptt_release(hwfn, ptt);
	}

	return 0;
}

static int qed_nic_stop(struct ecore_dev *edev)
{
	int i, rc;

	rc = ecore_hw_stop(edev);
	for (i = 0; i < edev->num_hwfns; i++) {
		struct ecore_hwfn *p_hwfn = &edev->hwfns[i];

		if (p_hwfn->b_sp_dpc_enabled)
			p_hwfn->b_sp_dpc_enabled = false;
	}
	return rc;
}

static int qed_slowpath_stop(struct ecore_dev *edev)
{
#ifdef CONFIG_QED_SRIOV
	int i;
#endif

	if (!edev)
		return -ENODEV;

	if (IS_PF(edev)) {
#ifdef CONFIG_ECORE_ZIPPED_FW
		qed_free_stream_mem(edev);
#endif

#ifdef CONFIG_QED_SRIOV
		if (IS_QED_ETH_IF(edev))
			qed_sriov_disable(edev, true);
#endif
	}

	qed_nic_stop(edev);

	ecore_resc_free(edev);
	qed_stop_iov_task(edev);

	return 0;
}

static void qed_remove(struct ecore_dev *edev)
{
	if (!edev)
		return;

	ecore_hw_remove(edev);
}

static int qed_send_drv_state(struct ecore_dev *edev, bool active)
{
	struct ecore_hwfn *hwfn = ECORE_LEADING_HWFN(edev);
	struct ecore_ptt *ptt;
	int status = 0;

	ptt = ecore_ptt_acquire(hwfn);
	if (!ptt)
		return -EAGAIN;

	status = ecore_mcp_ov_update_driver_state(hwfn, ptt, active ?
						  ECORE_OV_DRIVER_STATE_ACTIVE :
						ECORE_OV_DRIVER_STATE_DISABLED);

	ecore_ptt_release(hwfn, ptt);

	return status;
}

static int qed_get_sb_info(struct ecore_dev *edev, struct ecore_sb_info *sb,
			   u16 qid, struct ecore_sb_info_dbg *sb_dbg)
{
	struct ecore_hwfn *hwfn = &edev->hwfns[qid % edev->num_hwfns];
	struct ecore_ptt *ptt;
	int rc;

	if (IS_VF(edev))
		return -EINVAL;

	ptt = ecore_ptt_acquire(hwfn);
	if (!ptt) {
		DP_ERR(hwfn, "Can't acquire PTT\n");
		return -EAGAIN;
	}

	memset(sb_dbg, 0, sizeof(*sb_dbg));
	rc = ecore_int_get_sb_dbg(hwfn, ptt, sb, sb_dbg);

	ecore_ptt_release(hwfn, ptt);
	return rc;
}

const struct qed_common_ops qed_common_ops_pass = {
	INIT_STRUCT_FIELD(probe, &qed_probe),
	INIT_STRUCT_FIELD(update_pf_params, &qed_update_pf_params),
	INIT_STRUCT_FIELD(slowpath_start, &qed_slowpath_start),
	INIT_STRUCT_FIELD(set_name, &qed_set_name),
	INIT_STRUCT_FIELD(chain_alloc, &ecore_chain_alloc),
	INIT_STRUCT_FIELD(chain_free, &ecore_chain_free),
	INIT_STRUCT_FIELD(sb_init, &qed_sb_init),
	INIT_STRUCT_FIELD(get_sb_info, &qed_get_sb_info),
	INIT_STRUCT_FIELD(get_link, &qed_get_current_link),
	INIT_STRUCT_FIELD(set_link, &qed_set_link),
	INIT_STRUCT_FIELD(drain, &qed_drain),
	INIT_STRUCT_FIELD(slowpath_stop, &qed_slowpath_stop),
	INIT_STRUCT_FIELD(remove, &qed_remove),
	INIT_STRUCT_FIELD(send_drv_state, &qed_send_drv_state),
	/* ############### DEBUG ####################*/

	INIT_STRUCT_FIELD(dbg_get_debug_engine, &qed_get_debug_engine),
	INIT_STRUCT_FIELD(dbg_set_debug_engine, &qed_set_debug_engine),

	INIT_STRUCT_FIELD(dbg_protection_override,
			  &qed_dbg_protection_override),
	INIT_STRUCT_FIELD(dbg_protection_override_size,
			  &qed_dbg_protection_override_size),

	INIT_STRUCT_FIELD(dbg_grc, &qed_dbg_grc),
	INIT_STRUCT_FIELD(dbg_grc_size, &qed_dbg_grc_size),

	INIT_STRUCT_FIELD(dbg_idle_chk, &qed_dbg_idle_chk),
	INIT_STRUCT_FIELD(dbg_idle_chk_size, &qed_dbg_idle_chk_size),

	INIT_STRUCT_FIELD(dbg_mcp_trace, &qed_dbg_mcp_trace),
	INIT_STRUCT_FIELD(dbg_mcp_trace_size, &qed_dbg_mcp_trace_size),

	INIT_STRUCT_FIELD(dbg_fw_asserts, &qed_dbg_fw_asserts),
	INIT_STRUCT_FIELD(dbg_fw_asserts_size, &qed_dbg_fw_asserts_size),

	INIT_STRUCT_FIELD(dbg_ilt, &qed_dbg_ilt),
	INIT_STRUCT_FIELD(dbg_ilt_size, &qed_dbg_ilt_size),

	INIT_STRUCT_FIELD(dbg_reg_fifo_size, &qed_dbg_reg_fifo_size),
	INIT_STRUCT_FIELD(dbg_reg_fifo, &qed_dbg_reg_fifo),

	INIT_STRUCT_FIELD(dbg_igu_fifo_size, &qed_dbg_igu_fifo_size),
	INIT_STRUCT_FIELD(dbg_igu_fifo, &qed_dbg_igu_fifo),
};

const struct qed_eth_ops qed_eth_ops_pass = {
	INIT_STRUCT_FIELD(common, &qed_common_ops_pass),
	INIT_STRUCT_FIELD(fill_dev_info, &qed_fill_eth_dev_info),
};

const struct qed_eth_ops *qed_get_eth_ops(void)
{
	return &qed_eth_ops_pass;
}
