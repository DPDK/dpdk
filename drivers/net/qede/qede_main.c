/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#include <limits.h>
#include <time.h>
#include <rte_alarm.h>

#include "qede_ethdev.h"

static uint8_t npar_tx_switching = 1;

/* Alarm timeout. */
#define QEDE_ALARM_TIMEOUT_US 100000

/* Global variable to hold absolute path of fw file */
char fw_file[PATH_MAX];

const char *QEDE_DEFAULT_FIRMWARE =
	"/lib/firmware/qed/qed_init_values-8.10.9.0.bin";

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
}

static int
qed_probe(struct ecore_dev *edev, struct rte_pci_device *pci_dev,
	  enum qed_protocol protocol, uint32_t dp_module,
	  uint8_t dp_level, bool is_vf)
{
	struct ecore_hw_prepare_params hw_prepare_params;
	struct qede_dev *qdev = (struct qede_dev *)edev;
	int rc;

	ecore_init_struct(edev);
	qdev->protocol = protocol;
	if (is_vf) {
		edev->b_is_vf = true;
		edev->b_hw_channel = true; /* @DPDK */
	}
	ecore_init_dp(edev, dp_module, dp_level, NULL);
	qed_init_pci(edev, pci_dev);

	memset(&hw_prepare_params, 0, sizeof(hw_prepare_params));
	hw_prepare_params.personality = ECORE_PCI_ETH;
	hw_prepare_params.drv_resc_alloc = false;
	hw_prepare_params.chk_reg_fifo = false;
	rc = ecore_hw_prepare(edev, &hw_prepare_params);
	if (rc) {
		DP_ERR(edev, "hw prepare failed\n");
		return rc;
	}

	return rc;
}

static int qed_nic_setup(struct ecore_dev *edev)
{
	int rc, i;

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
		strcpy(fw_file, QEDE_DEFAULT_FIRMWARE);
	else
		strcpy(fw_file, fw);

	fd = open(fw_file, O_RDONLY);
	if (fd < 0) {
		DP_NOTICE(edev, false, "Can't open firmware file\n");
		return -ENOENT;
	}

	if (fstat(fd, &st) < 0) {
		DP_NOTICE(edev, false, "Can't stat firmware file\n");
		return -1;
	}

	edev->firmware = rte_zmalloc("qede_fw", st.st_size,
				    RTE_CACHE_LINE_SIZE);
	if (!edev->firmware) {
		DP_NOTICE(edev, false, "Can't allocate memory for firmware\n");
		close(fd);
		return -ENOMEM;
	}

	if (read(fd, edev->firmware, st.st_size) != st.st_size) {
		DP_NOTICE(edev, false, "Can't read firmware data\n");
		close(fd);
		return -1;
	}

	edev->fw_len = st.st_size;
	if (edev->fw_len < 104) {
		DP_NOTICE(edev, false, "Invalid fw size: %" PRIu64 "\n",
			  edev->fw_len);
		return -EINVAL;
	}

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
	bool allow_npar_tx_switching;
	const uint8_t *data = NULL;
	struct ecore_hwfn *hwfn;
	struct ecore_mcp_drv_version drv_version;
	struct ecore_hw_init_params hw_init_params;
	struct qede_dev *qdev = (struct qede_dev *)edev;
	int rc;
#ifdef QED_ENC_SUPPORTED
	struct ecore_tunn_start_params tunn_info;
#endif

#ifdef CONFIG_ECORE_BINARY_FW
	if (IS_PF(edev)) {
		rc = qed_load_firmware_data(edev);
		if (rc) {
			DP_NOTICE(edev, true,
				  "Failed to find fw file %s\n", fw_file);
			goto err;
		}
	}
#endif

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
			DP_NOTICE(edev, true,
			"Failed to allocate stream memory\n");
			goto err2;
		}
	}

	qed_start_iov_task(edev);
#endif

#ifdef CONFIG_ECORE_BINARY_FW
	if (IS_PF(edev))
		data = (const uint8_t *)edev->firmware + sizeof(u32);
#endif

	allow_npar_tx_switching = npar_tx_switching ? true : false;

	/* Start the slowpath */
	memset(&hw_init_params, 0, sizeof(hw_init_params));
#ifdef QED_ENC_SUPPORTED
	memset(&tunn_info, 0, sizeof(tunn_info));
	tunn_info.tunn_mode |= 1 << QED_MODE_VXLAN_TUNN |
	    1 << QED_MODE_L2GRE_TUNN |
	    1 << QED_MODE_IPGRE_TUNN |
	    1 << QED_MODE_L2GENEVE_TUNN | 1 << QED_MODE_IPGENEVE_TUNN;
	tunn_info.tunn_clss_vxlan = QED_TUNN_CLSS_MAC_VLAN;
	tunn_info.tunn_clss_l2gre = QED_TUNN_CLSS_MAC_VLAN;
	tunn_info.tunn_clss_ipgre = QED_TUNN_CLSS_MAC_VLAN;
	hw_init_params.p_tunn = &tunn_info;
#endif
	hw_init_params.b_hw_start = true;
	hw_init_params.int_mode = ECORE_INT_MODE_MSIX;
	hw_init_params.allow_npar_tx_switch = allow_npar_tx_switching;
	hw_init_params.bin_fw_data = data;
	hw_init_params.epoch = (u32)time(NULL);
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
		/* TBD: strlcpy() */
		strncpy((char *)drv_version.name, (const char *)params->name,
			MCP_DRV_VER_STR_SIZE - 4);
		rc = ecore_mcp_send_drv_version(hwfn, hwfn->p_main_ptt,
						&drv_version);
		if (rc) {
			DP_NOTICE(edev, true,
				  "Failed sending drv version command\n");
			return rc;
		}
	}

	ecore_reset_vport_stats(edev);

	return 0;

	ecore_hw_stop(edev);
err2:
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
	struct ecore_ptt *ptt = NULL;

	memset(dev_info, 0, sizeof(struct qed_dev_info));
	dev_info->num_hwfns = edev->num_hwfns;
	dev_info->is_mf_default = IS_MF_DEFAULT(&edev->hwfns[0]);
	rte_memcpy(&dev_info->hw_mac, &edev->hwfns[0].hw_info.hw_mac_addr,
	       ETHER_ADDR_LEN);

	if (IS_PF(edev)) {
		dev_info->fw_major = FW_MAJOR_VERSION;
		dev_info->fw_minor = FW_MINOR_VERSION;
		dev_info->fw_rev = FW_REVISION_VERSION;
		dev_info->fw_eng = FW_ENGINEERING_VERSION;
		dev_info->mf_mode = edev->mf_mode;
		dev_info->tx_switching = false;
	} else {
		ecore_vf_get_fw_version(&edev->hwfns[0], &dev_info->fw_major,
					&dev_info->fw_minor, &dev_info->fw_rev,
					&dev_info->fw_eng);
	}

	if (IS_PF(edev)) {
		ptt = ecore_ptt_acquire(ECORE_LEADING_HWFN(edev));
		if (ptt) {
			ecore_mcp_get_mfw_ver(ECORE_LEADING_HWFN(edev), ptt,
					      &dev_info->mfw_rev, NULL);

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
	struct qede_dev *qdev = (struct qede_dev *)edev;
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

		if (edev->p_iov_info)
			max_vf_vlan_filters = edev->p_iov_info->total_vfs *
					      ECORE_ETH_VF_NUM_VLAN_FILTERS;
		info->num_vlan_filters = RESC_NUM(&edev->hwfns[0], ECORE_VLAN) -
					 max_vf_vlan_filters;

		rte_memcpy(&info->port_mac, &edev->hwfns[0].hw_info.hw_mac_addr,
			   ETHER_ADDR_LEN);
	} else {
		ecore_vf_get_num_rxqs(ECORE_LEADING_HWFN(edev),
				      &info->num_queues);
		if (edev->num_hwfns > 1) {
			ecore_vf_get_num_rxqs(&edev->hwfns[1], &queues);
			info->num_queues += queues;
			/* Restrict 100G VF to advertise 16 queues till the
			 * required support is available to go beyond 16.
			 */
			info->num_queues = RTE_MIN(info->num_queues,
						   ECORE_MAX_VF_CHAINS_PER_PF);
		}

		ecore_vf_get_num_vlan_filters(&edev->hwfns[0],
					      (u8 *)&info->num_vlan_filters);

		ecore_vf_get_port_mac(&edev->hwfns[0],
				      (uint8_t *)&info->port_mac);
	}

	qed_fill_dev_info(edev, &info->common);

	if (IS_VF(edev))
		memset(&info->common.hw_mac, 0, ETHER_ADDR_LEN);

	return 0;
}

static void
qed_set_id(struct ecore_dev *edev, char name[NAME_SIZE],
	   const char ver_str[NAME_SIZE])
{
	int i;

	rte_memcpy(edev->name, name, NAME_SIZE);
	for_each_hwfn(edev, i) {
		snprintf(edev->hwfns[i].name, NAME_SIZE, "%s-%d", name, i);
	}
	memcpy(edev->ver_str, ver_str, NAME_SIZE);
	edev->drv_type = DRV_ID_DRV_TYPE_LINUX;
}

static uint32_t
qed_sb_init(struct ecore_dev *edev, struct ecore_sb_info *sb_info,
	    void *sb_virt_addr, dma_addr_t sb_phy_addr,
	    uint16_t sb_id, enum qed_sb_type type)
{
	struct ecore_hwfn *p_hwfn;
	int hwfn_index;
	uint16_t rel_sb_id;
	uint8_t n_hwfns;
	uint32_t rc;

	/* RoCE uses single engine and CMT uses two engines. When using both
	 * we force only a single engine. Storage uses only engine 0 too.
	 */
	if (type == QED_SB_TYPE_L2_QUEUE)
		n_hwfns = edev->num_hwfns;
	else
		n_hwfns = 1;

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
			  struct qed_link_output *if_link)
{
	struct ecore_mcp_link_params params;
	struct ecore_mcp_link_state link;
	struct ecore_mcp_link_capabilities link_caps;
	uint32_t media_type;
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
}

static void
qed_get_current_link(struct ecore_dev *edev, struct qed_link_output *if_link)
{
	qed_fill_link(&edev->hwfns[0], if_link);

#ifdef CONFIG_QED_SRIOV
	for_each_hwfn(cdev, i)
		qed_inform_vf_link_state(&cdev->hwfns[i]);
#endif
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

	rc = ecore_mcp_set_link(hwfn, ptt, params->link_up);

	ecore_ptt_release(hwfn, ptt);

	return rc;
}

void qed_link_update(struct ecore_hwfn *hwfn)
{
	struct qed_link_output if_link;

	qed_fill_link(hwfn, &if_link);
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
			DP_NOTICE(hwfn, true, "Failed to drain NIG; No PTT\n");
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

static int qed_nic_reset(struct ecore_dev *edev)
{
	int rc;

	rc = ecore_hw_reset(edev);
	if (rc)
		return rc;

	ecore_resc_free(edev);

	return 0;
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
		qed_nic_stop(edev);
	}

	qed_nic_reset(edev);
	qed_stop_iov_task(edev);

	return 0;
}

static void qed_remove(struct ecore_dev *edev)
{
	if (!edev)
		return;

	ecore_hw_remove(edev);
}

const struct qed_common_ops qed_common_ops_pass = {
	INIT_STRUCT_FIELD(probe, &qed_probe),
	INIT_STRUCT_FIELD(update_pf_params, &qed_update_pf_params),
	INIT_STRUCT_FIELD(slowpath_start, &qed_slowpath_start),
	INIT_STRUCT_FIELD(set_id, &qed_set_id),
	INIT_STRUCT_FIELD(chain_alloc, &ecore_chain_alloc),
	INIT_STRUCT_FIELD(chain_free, &ecore_chain_free),
	INIT_STRUCT_FIELD(sb_init, &qed_sb_init),
	INIT_STRUCT_FIELD(get_link, &qed_get_current_link),
	INIT_STRUCT_FIELD(set_link, &qed_set_link),
	INIT_STRUCT_FIELD(drain, &qed_drain),
	INIT_STRUCT_FIELD(slowpath_stop, &qed_slowpath_stop),
	INIT_STRUCT_FIELD(remove, &qed_remove),
};
