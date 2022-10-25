/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2021 NXP
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_flow_driver.h>
#include <rte_tailq.h>

#include <bus_fslmc_driver.h>
#include <fsl_dpdmux.h>
#include <fsl_dpkg.h>

#include <dpaa2_ethdev.h>
#include <dpaa2_pmd_logs.h>

struct dpaa2_dpdmux_dev {
	TAILQ_ENTRY(dpaa2_dpdmux_dev) next;
		/**< Pointer to Next device instance */
	struct fsl_mc_io dpdmux;  /** handle to DPDMUX portal object */
	uint16_t token;
	uint32_t dpdmux_id; /*HW ID for DPDMUX object */
	uint8_t num_ifs;   /* Number of interfaces in DPDMUX */
};

struct rte_flow {
	struct dpdmux_rule_cfg rule;
};

TAILQ_HEAD(dpdmux_dev_list, dpaa2_dpdmux_dev);
static struct dpdmux_dev_list dpdmux_dev_list =
	TAILQ_HEAD_INITIALIZER(dpdmux_dev_list); /*!< DPDMUX device list */

static struct dpaa2_dpdmux_dev *get_dpdmux_from_id(uint32_t dpdmux_id)
{
	struct dpaa2_dpdmux_dev *dpdmux_dev = NULL;

	/* Get DPBP dev handle from list using index */
	TAILQ_FOREACH(dpdmux_dev, &dpdmux_dev_list, next) {
		if (dpdmux_dev->dpdmux_id == dpdmux_id)
			break;
	}

	return dpdmux_dev;
}

struct rte_flow *
rte_pmd_dpaa2_mux_flow_create(uint32_t dpdmux_id,
			      struct rte_flow_item *pattern[],
			      struct rte_flow_action *actions[])
{
	struct dpaa2_dpdmux_dev *dpdmux_dev;
	struct dpkg_profile_cfg kg_cfg;
	const struct rte_flow_action_vf *vf_conf;
	struct dpdmux_cls_action dpdmux_action;
	struct rte_flow *flow = NULL;
	void *key_iova, *mask_iova, *key_cfg_iova = NULL;
	uint8_t key_size = 0;
	int ret;
	static int i;

	if (!pattern || !actions || !pattern[0] || !actions[0])
		return NULL;

	/* Find the DPDMUX from dpdmux_id in our list */
	dpdmux_dev = get_dpdmux_from_id(dpdmux_id);
	if (!dpdmux_dev) {
		DPAA2_PMD_ERR("Invalid dpdmux_id: %d", dpdmux_id);
		return NULL;
	}

	key_cfg_iova = rte_zmalloc(NULL, DIST_PARAM_IOVA_SIZE,
				   RTE_CACHE_LINE_SIZE);
	if (!key_cfg_iova) {
		DPAA2_PMD_ERR("Unable to allocate flow-dist parameters");
		return NULL;
	}
	flow = rte_zmalloc(NULL, sizeof(struct rte_flow) +
			   (2 * DIST_PARAM_IOVA_SIZE), RTE_CACHE_LINE_SIZE);
	if (!flow) {
		DPAA2_PMD_ERR(
			"Memory allocation failure for rule configuration\n");
		goto creation_error;
	}
	key_iova = (void *)((size_t)flow + sizeof(struct rte_flow));
	mask_iova = (void *)((size_t)key_iova + DIST_PARAM_IOVA_SIZE);

	/* Currently taking only IP protocol as an extract type.
	 * This can be extended to other fields using pattern->type.
	 */
	memset(&kg_cfg, 0, sizeof(struct dpkg_profile_cfg));

	switch (pattern[0]->type) {
	case RTE_FLOW_ITEM_TYPE_IPV4:
	{
		const struct rte_flow_item_ipv4 *spec;

		kg_cfg.extracts[0].extract.from_hdr.prot = NET_PROT_IP;
		kg_cfg.extracts[0].extract.from_hdr.field = NH_FLD_IP_PROTO;
		kg_cfg.extracts[0].type = DPKG_EXTRACT_FROM_HDR;
		kg_cfg.extracts[0].extract.from_hdr.type = DPKG_FULL_FIELD;
		kg_cfg.num_extracts = 1;

		spec = (const struct rte_flow_item_ipv4 *)pattern[0]->spec;
		memcpy(key_iova, (const void *)(&spec->hdr.next_proto_id),
			sizeof(uint8_t));
		memcpy(mask_iova, pattern[0]->mask, sizeof(uint8_t));
		key_size = sizeof(uint8_t);
	}
	break;

	case RTE_FLOW_ITEM_TYPE_UDP:
	{
		const struct rte_flow_item_udp *spec;
		uint16_t udp_dst_port;

		kg_cfg.extracts[0].extract.from_hdr.prot = NET_PROT_UDP;
		kg_cfg.extracts[0].extract.from_hdr.field = NH_FLD_UDP_PORT_DST;
		kg_cfg.extracts[0].type = DPKG_EXTRACT_FROM_HDR;
		kg_cfg.extracts[0].extract.from_hdr.type = DPKG_FULL_FIELD;
		kg_cfg.num_extracts = 1;

		spec = (const struct rte_flow_item_udp *)pattern[0]->spec;
		udp_dst_port = rte_constant_bswap16(spec->hdr.dst_port);
		memcpy((void *)key_iova, (const void *)&udp_dst_port,
							sizeof(rte_be16_t));
		memcpy(mask_iova, pattern[0]->mask, sizeof(uint16_t));
		key_size = sizeof(uint16_t);
	}
	break;

	case RTE_FLOW_ITEM_TYPE_ETH:
	{
		const struct rte_flow_item_eth *spec;
		uint16_t eth_type;

		kg_cfg.extracts[0].extract.from_hdr.prot = NET_PROT_ETH;
		kg_cfg.extracts[0].extract.from_hdr.field = NH_FLD_ETH_TYPE;
		kg_cfg.extracts[0].type = DPKG_EXTRACT_FROM_HDR;
		kg_cfg.extracts[0].extract.from_hdr.type = DPKG_FULL_FIELD;
		kg_cfg.num_extracts = 1;

		spec = (const struct rte_flow_item_eth *)pattern[0]->spec;
		eth_type = rte_constant_bswap16(spec->hdr.ether_type);
		memcpy((void *)key_iova, (const void *)&eth_type,
							sizeof(rte_be16_t));
		memcpy(mask_iova, pattern[0]->mask, sizeof(uint16_t));
		key_size = sizeof(uint16_t);
	}
	break;

	case RTE_FLOW_ITEM_TYPE_RAW:
	{
		const struct rte_flow_item_raw *spec;

		spec = (const struct rte_flow_item_raw *)pattern[0]->spec;
		kg_cfg.extracts[0].extract.from_data.offset = spec->offset;
		kg_cfg.extracts[0].extract.from_data.size = spec->length;
		kg_cfg.extracts[0].type = DPKG_EXTRACT_FROM_DATA;
		kg_cfg.num_extracts = 1;
		memcpy((void *)key_iova, (const void *)spec->pattern,
							spec->length);
		memcpy(mask_iova, pattern[0]->mask, spec->length);

		key_size = spec->length;
	}
	break;

	default:
		DPAA2_PMD_ERR("Not supported pattern type: %d",
				pattern[0]->type);
		goto creation_error;
	}

	ret = dpkg_prepare_key_cfg(&kg_cfg, key_cfg_iova);
	if (ret) {
		DPAA2_PMD_ERR("dpkg_prepare_key_cfg failed: err(%d)", ret);
		goto creation_error;
	}

	/* Multiple rules with same DPKG extracts (kg_cfg.extracts) like same
	 * offset and length values in raw is supported right now. Different
	 * values of kg_cfg may not work.
	 */
	if (i == 0) {
		ret = dpdmux_set_custom_key(&dpdmux_dev->dpdmux, CMD_PRI_LOW,
					    dpdmux_dev->token,
				(uint64_t)(DPAA2_VADDR_TO_IOVA(key_cfg_iova)));
		if (ret) {
			DPAA2_PMD_ERR("dpdmux_set_custom_key failed: err(%d)",
					ret);
			goto creation_error;
		}
	}
	/* As now our key extract parameters are set, let us configure
	 * the rule.
	 */
	flow->rule.key_iova = (uint64_t)(DPAA2_VADDR_TO_IOVA(key_iova));
	flow->rule.mask_iova = (uint64_t)(DPAA2_VADDR_TO_IOVA(mask_iova));
	flow->rule.key_size = key_size;
	flow->rule.entry_index = i++;

	vf_conf = (const struct rte_flow_action_vf *)(actions[0]->conf);
	if (vf_conf->id == 0 || vf_conf->id > dpdmux_dev->num_ifs) {
		DPAA2_PMD_ERR("Invalid destination id\n");
		goto creation_error;
	}
	dpdmux_action.dest_if = vf_conf->id;

	ret = dpdmux_add_custom_cls_entry(&dpdmux_dev->dpdmux, CMD_PRI_LOW,
					  dpdmux_dev->token, &flow->rule,
					  &dpdmux_action);
	if (ret) {
		DPAA2_PMD_ERR("dpdmux_add_custom_cls_entry failed: err(%d)",
			      ret);
		goto creation_error;
	}

	return flow;

creation_error:
	rte_free((void *)key_cfg_iova);
	rte_free((void *)flow);
	return NULL;
}

int
rte_pmd_dpaa2_mux_rx_frame_len(uint32_t dpdmux_id, uint16_t max_rx_frame_len)
{
	struct dpaa2_dpdmux_dev *dpdmux_dev;
	int ret;

	/* Find the DPDMUX from dpdmux_id in our list */
	dpdmux_dev = get_dpdmux_from_id(dpdmux_id);
	if (!dpdmux_dev) {
		DPAA2_PMD_ERR("Invalid dpdmux_id: %d", dpdmux_id);
		return -1;
	}

	ret = dpdmux_set_max_frame_length(&dpdmux_dev->dpdmux,
			CMD_PRI_LOW, dpdmux_dev->token, max_rx_frame_len);
	if (ret) {
		DPAA2_PMD_ERR("DPDMUX:Unable to set mtu. check config %d", ret);
		return ret;
	}

	DPAA2_PMD_INFO("dpdmux mtu set as %u",
			DPAA2_MAX_RX_PKT_LEN - RTE_ETHER_CRC_LEN);

	return ret;
}

static int
dpaa2_create_dpdmux_device(int vdev_fd __rte_unused,
			   struct vfio_device_info *obj_info __rte_unused,
			   int dpdmux_id)
{
	struct dpaa2_dpdmux_dev *dpdmux_dev;
	struct dpdmux_attr attr;
	int ret;
	uint16_t maj_ver;
	uint16_t min_ver;

	PMD_INIT_FUNC_TRACE();

	/* Allocate DPAA2 dpdmux handle */
	dpdmux_dev = rte_malloc(NULL, sizeof(struct dpaa2_dpdmux_dev), 0);
	if (!dpdmux_dev) {
		DPAA2_PMD_ERR("Memory allocation failed for DPDMUX Device");
		return -1;
	}

	/* Open the dpdmux object */
	dpdmux_dev->dpdmux.regs = dpaa2_get_mcp_ptr(MC_PORTAL_INDEX);
	ret = dpdmux_open(&dpdmux_dev->dpdmux, CMD_PRI_LOW, dpdmux_id,
			  &dpdmux_dev->token);
	if (ret) {
		DPAA2_PMD_ERR("Unable to open dpdmux object: err(%d)", ret);
		goto init_err;
	}

	ret = dpdmux_get_attributes(&dpdmux_dev->dpdmux, CMD_PRI_LOW,
				    dpdmux_dev->token, &attr);
	if (ret) {
		DPAA2_PMD_ERR("Unable to get dpdmux attr: err(%d)", ret);
		goto init_err;
	}

	ret = dpdmux_if_set_default(&dpdmux_dev->dpdmux, CMD_PRI_LOW,
				    dpdmux_dev->token, attr.default_if);
	if (ret) {
		DPAA2_PMD_ERR("setting default interface failed in %s",
			      __func__);
		goto init_err;
	}

	ret = dpdmux_get_api_version(&dpdmux_dev->dpdmux, CMD_PRI_LOW,
					&maj_ver, &min_ver);
	if (ret) {
		DPAA2_PMD_ERR("setting version failed in %s",
				__func__);
		goto init_err;
	}

	/* The new dpdmux_set/get_resetable() API are available starting with
	 * DPDMUX_VER_MAJOR==6 and DPDMUX_VER_MINOR==6
	 */
	if (maj_ver >= 6 && min_ver >= 6) {
		ret = dpdmux_set_resetable(&dpdmux_dev->dpdmux, CMD_PRI_LOW,
				dpdmux_dev->token,
				DPDMUX_SKIP_DEFAULT_INTERFACE |
				DPDMUX_SKIP_UNICAST_RULES |
				DPDMUX_SKIP_MULTICAST_RULES);
		if (ret) {
			DPAA2_PMD_ERR("setting default interface failed in %s",
				      __func__);
			goto init_err;
		}
	}

	if (maj_ver >= 6 && min_ver >= 9) {
		struct dpdmux_error_cfg mux_err_cfg;

		memset(&mux_err_cfg, 0, sizeof(mux_err_cfg));
		mux_err_cfg.error_action = DPDMUX_ERROR_ACTION_CONTINUE;
		mux_err_cfg.errors = DPDMUX_ERROR_DISC;

		ret = dpdmux_if_set_errors_behavior(&dpdmux_dev->dpdmux,
				CMD_PRI_LOW,
				dpdmux_dev->token, DPAA2_DPDMUX_DPMAC_IDX,
				&mux_err_cfg);
		if (ret) {
			DPAA2_PMD_ERR("dpdmux_if_set_errors_behavior %s err %d",
				      __func__, ret);
			goto init_err;
		}
	}

	dpdmux_dev->dpdmux_id = dpdmux_id;
	dpdmux_dev->num_ifs = attr.num_ifs;

	TAILQ_INSERT_TAIL(&dpdmux_dev_list, dpdmux_dev, next);

	return 0;

init_err:
	rte_free(dpdmux_dev);

	return -1;
}

static struct rte_dpaa2_object rte_dpaa2_dpdmux_obj = {
	.dev_type = DPAA2_MUX,
	.create = dpaa2_create_dpdmux_device,
};

RTE_PMD_REGISTER_DPAA2_OBJECT(dpdmux, rte_dpaa2_dpdmux_obj);
