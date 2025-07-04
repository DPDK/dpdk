/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2021,2023 NXP
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include <eal_export.h>
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

#define DPAA2_MUX_FLOW_MAX_RULE_NUM 8
struct dpaa2_mux_flow {
	struct dpdmux_rule_cfg rule[DPAA2_MUX_FLOW_MAX_RULE_NUM];
};

TAILQ_HEAD(dpdmux_dev_list, dpaa2_dpdmux_dev);
static struct dpdmux_dev_list dpdmux_dev_list =
	TAILQ_HEAD_INITIALIZER(dpdmux_dev_list); /*!< DPDMUX device list */

static struct dpaa2_dpdmux_dev *get_dpdmux_from_id(uint32_t dpdmux_id)
{
	struct dpaa2_dpdmux_dev *dpdmux_dev = NULL;

	/* Get DPDMUX dev handle from list using index */
	TAILQ_FOREACH(dpdmux_dev, &dpdmux_dev_list, next) {
		if (dpdmux_dev->dpdmux_id == dpdmux_id)
			break;
	}

	return dpdmux_dev;
}

RTE_EXPORT_SYMBOL(rte_pmd_dpaa2_mux_flow_create)
int
rte_pmd_dpaa2_mux_flow_create(uint32_t dpdmux_id,
	struct rte_flow_item pattern[],
	struct rte_flow_action actions[])
{
	struct dpaa2_dpdmux_dev *dpdmux_dev;
	static struct dpkg_profile_cfg s_kg_cfg;
	struct dpkg_profile_cfg kg_cfg;
	const struct rte_flow_action_vf *vf_conf;
	struct dpdmux_cls_action dpdmux_action;
	uint8_t *key_va = NULL, *mask_va = NULL;
	void *key_cfg_va = NULL;
	uint64_t key_iova, mask_iova, key_cfg_iova;
	uint8_t key_size = 0;
	int ret = 0, loop = 0;
	static int s_i;
	struct dpkg_extract *extract;
	struct dpdmux_rule_cfg rule;

	memset(&kg_cfg, 0, sizeof(struct dpkg_profile_cfg));

	/* Find the DPDMUX from dpdmux_id in our list */
	dpdmux_dev = get_dpdmux_from_id(dpdmux_id);
	if (!dpdmux_dev) {
		DPAA2_PMD_ERR("Invalid dpdmux_id: %d", dpdmux_id);
		ret = -ENODEV;
		goto creation_error;
	}

	key_cfg_va = rte_zmalloc(NULL, DIST_PARAM_IOVA_SIZE,
				RTE_CACHE_LINE_SIZE);
	if (!key_cfg_va) {
		DPAA2_PMD_ERR("Unable to allocate key configure buffer");
		ret = -ENOMEM;
		goto creation_error;
	}

	key_cfg_iova = DPAA2_VADDR_TO_IOVA_AND_CHECK(key_cfg_va,
		DIST_PARAM_IOVA_SIZE);
	if (key_cfg_iova == RTE_BAD_IOVA) {
		DPAA2_PMD_ERR("%s: No IOMMU map for key cfg(%p)",
			__func__, key_cfg_va);
		ret = -ENOBUFS;
		goto creation_error;
	}

	key_va = rte_zmalloc(NULL, (2 * DIST_PARAM_IOVA_SIZE),
		RTE_CACHE_LINE_SIZE);
	if (!key_va) {
		DPAA2_PMD_ERR("Unable to allocate flow dist parameter");
		ret = -ENOMEM;
		goto creation_error;
	}

	key_iova = DPAA2_VADDR_TO_IOVA_AND_CHECK(key_va,
		(2 * DIST_PARAM_IOVA_SIZE));
	if (key_iova == RTE_BAD_IOVA) {
		DPAA2_PMD_ERR("%s: No IOMMU mapping for address(%p)",
			__func__, key_va);
		ret = -ENOBUFS;
		goto creation_error;
	}

	mask_va = key_va + DIST_PARAM_IOVA_SIZE;
	mask_iova = key_iova + DIST_PARAM_IOVA_SIZE;

	/* Currently taking only IP protocol as an extract type.
	 * This can be extended to other fields using pattern->type.
	 */
	memset(&kg_cfg, 0, sizeof(struct dpkg_profile_cfg));

	while (pattern[loop].type != RTE_FLOW_ITEM_TYPE_END) {
		if (kg_cfg.num_extracts >= DPKG_MAX_NUM_OF_EXTRACTS) {
			DPAA2_PMD_ERR("Too many extracts(%d)",
				kg_cfg.num_extracts);
			ret = -ENOTSUP;
			goto creation_error;
		}
		switch (pattern[loop].type) {
		case RTE_FLOW_ITEM_TYPE_IPV4:
		{
			const struct rte_flow_item_ipv4 *spec;
			const struct rte_flow_item_ipv4 *mask;

			extract = &kg_cfg.extracts[kg_cfg.num_extracts];
			extract->type = DPKG_EXTRACT_FROM_HDR;
			extract->extract.from_hdr.prot = NET_PROT_IP;
			extract->extract.from_hdr.field = NH_FLD_IP_PROTO;
			extract->extract.from_hdr.type = DPKG_FULL_FIELD;

			kg_cfg.num_extracts++;

			spec = pattern[loop].spec;
			mask = pattern[loop].mask;
			rte_memcpy(&key_va[key_size],
				&spec->hdr.next_proto_id, sizeof(uint8_t));
			if (mask) {
				rte_memcpy(&mask_va[key_size],
					&mask->hdr.next_proto_id,
					sizeof(uint8_t));
			} else {
				mask_va[key_size] = 0xff;
			}
			key_size += sizeof(uint8_t);
		}
		break;

		case RTE_FLOW_ITEM_TYPE_VLAN:
		{
			const struct rte_flow_item_vlan *spec;
			const struct rte_flow_item_vlan *mask;

			extract = &kg_cfg.extracts[kg_cfg.num_extracts];
			extract->type = DPKG_EXTRACT_FROM_HDR;
			extract->extract.from_hdr.prot = NET_PROT_VLAN;
			extract->extract.from_hdr.field = NH_FLD_VLAN_TCI;
			extract->extract.from_hdr.type = DPKG_FULL_FIELD;

			kg_cfg.num_extracts++;

			spec = pattern[loop].spec;
			mask = pattern[loop].mask;
			rte_memcpy(&key_va[key_size],
				&spec->tci, sizeof(uint16_t));
			if (mask) {
				rte_memcpy(&mask_va[key_size],
					&mask->tci, sizeof(uint16_t));
			} else {
				memset(&mask_va[key_size], 0xff,
					sizeof(rte_be16_t));
			}
			key_size += sizeof(uint16_t);
		}
		break;

		case RTE_FLOW_ITEM_TYPE_UDP:
		{
			const struct rte_flow_item_udp *spec;
			const struct rte_flow_item_udp *mask;

			extract = &kg_cfg.extracts[kg_cfg.num_extracts];
			extract->type = DPKG_EXTRACT_FROM_HDR;
			extract->extract.from_hdr.prot = NET_PROT_UDP;
			extract->extract.from_hdr.type = DPKG_FULL_FIELD;
			extract->extract.from_hdr.field = NH_FLD_UDP_PORT_DST;
			kg_cfg.num_extracts++;

			spec = pattern[loop].spec;
			mask = pattern[loop].mask;
			rte_memcpy(&key_va[key_size],
				&spec->hdr.dst_port, sizeof(rte_be16_t));
			if (mask) {
				rte_memcpy(&mask_va[key_size],
					&mask->hdr.dst_port,
					sizeof(rte_be16_t));
			} else {
				memset(&mask_va[key_size], 0xff,
					sizeof(rte_be16_t));
			}
			key_size += sizeof(rte_be16_t);
		}
		break;

		case RTE_FLOW_ITEM_TYPE_ETH:
		{
			const struct rte_flow_item_eth *spec;
			const struct rte_flow_item_eth *mask;

			extract = &kg_cfg.extracts[kg_cfg.num_extracts];
			extract->type = DPKG_EXTRACT_FROM_HDR;
			extract->extract.from_hdr.prot = NET_PROT_ETH;
			extract->extract.from_hdr.type = DPKG_FULL_FIELD;
			extract->extract.from_hdr.field = NH_FLD_ETH_TYPE;
			kg_cfg.num_extracts++;

			spec = pattern[loop].spec;
			mask = pattern[loop].mask;
			rte_memcpy(&key_va[key_size],
				&spec->type, sizeof(rte_be16_t));
			if (mask) {
				rte_memcpy(&mask_va[key_size],
					&mask->type, sizeof(rte_be16_t));
			} else {
				memset(&mask_va[key_size], 0xff,
					sizeof(rte_be16_t));
			}
			key_size += sizeof(rte_be16_t);
		}
		break;

		case RTE_FLOW_ITEM_TYPE_RAW:
		{
			const struct rte_flow_item_raw *spec;
			const struct rte_flow_item_raw *mask;

			spec = pattern[loop].spec;
			mask = pattern[loop].mask;
			extract = &kg_cfg.extracts[kg_cfg.num_extracts];
			extract->type = DPKG_EXTRACT_FROM_DATA;
			extract->extract.from_data.offset = spec->offset;
			extract->extract.from_data.size = spec->length;
			kg_cfg.num_extracts++;

			rte_memcpy(&key_va[key_size],
				spec->pattern, spec->length);
			if (mask && mask->pattern) {
				rte_memcpy(&mask_va[key_size],
					mask->pattern, spec->length);
			} else {
				memset(&mask_va[key_size], 0xff, spec->length);
			}

			key_size += spec->length;
		}
		break;

		default:
			DPAA2_PMD_ERR("Not supported pattern[%d] type: %d",
				loop, pattern[loop].type);
			ret = -ENOTSUP;
			goto creation_error;
		}
		loop++;
	}

	ret = dpkg_prepare_key_cfg(&kg_cfg, key_cfg_va);
	if (ret) {
		DPAA2_PMD_ERR("dpkg_prepare_key_cfg failed: err(%d)", ret);
		goto creation_error;
	}

	if (!s_i) {
		ret = dpdmux_set_custom_key(&dpdmux_dev->dpdmux,
				CMD_PRI_LOW, dpdmux_dev->token, key_cfg_iova);
		if (ret) {
			DPAA2_PMD_ERR("dpdmux_set_custom_key failed: err(%d)",
				ret);
			goto creation_error;
		}
		s_kg_cfg = kg_cfg;
	} else {
		if (memcmp(&s_kg_cfg, &kg_cfg,
			sizeof(struct dpkg_profile_cfg))) {
			DPAA2_PMD_ERR("%s: Single flow support only.",
				__func__);
			ret = -ENOTSUP;
			goto creation_error;
		}
	}

	vf_conf = actions[0].conf;
	if (vf_conf->id == 0 || vf_conf->id > dpdmux_dev->num_ifs) {
		DPAA2_PMD_ERR("Invalid destination id(%d)", vf_conf->id);
		goto creation_error;
	}
	dpdmux_action.dest_if = vf_conf->id;

	rule.key_iova = key_iova;
	rule.mask_iova = mask_iova;
	rule.key_size = key_size;
	rule.entry_index = s_i;
	s_i++;

	/* As now our key extract parameters are set, let us configure
	 * the rule.
	 */
	ret = dpdmux_add_custom_cls_entry(&dpdmux_dev->dpdmux,
			CMD_PRI_LOW, dpdmux_dev->token,
			&rule, &dpdmux_action);
	if (ret) {
		DPAA2_PMD_ERR("Add classification entry failed:err(%d)", ret);
		goto creation_error;
	}

creation_error:
	rte_free(key_cfg_va);
	rte_free(key_va);

	return ret;
}

int
rte_pmd_dpaa2_mux_flow_l2(uint32_t dpdmux_id,
	uint8_t mac_addr[6], uint16_t vlan_id, int dest_if)
{
	struct dpaa2_dpdmux_dev *dpdmux_dev;
	struct dpdmux_l2_rule rule;
	int ret, i;

	/* Find the DPDMUX from dpdmux_id in our list */
	dpdmux_dev = get_dpdmux_from_id(dpdmux_id);
	if (!dpdmux_dev) {
		DPAA2_PMD_ERR("Invalid dpdmux_id: %d", dpdmux_id);
		return -ENODEV;
	}

	for (i = 0; i < 6; i++)
		rule.mac_addr[i] = mac_addr[i];
	rule.vlan_id = vlan_id;

	ret = dpdmux_if_add_l2_rule(&dpdmux_dev->dpdmux, CMD_PRI_LOW,
			dpdmux_dev->token, dest_if, &rule);
	if (ret) {
		DPAA2_PMD_ERR("dpdmux_if_add_l2_rule failed:err(%d)", ret);
		return ret;
	}

	return 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_dpaa2_mux_rx_frame_len, 21.05)
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

/* dump the status of the dpaa2_mux counters on the console */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_dpaa2_mux_dump_counter, 24.11)
void
rte_pmd_dpaa2_mux_dump_counter(FILE *f, uint32_t dpdmux_id, int num_if)
{
	struct dpaa2_dpdmux_dev *dpdmux;
	uint64_t counter;
	int ret;
	int if_id;

	/* Find the DPDMUX from dpdmux_id in our list */
	dpdmux = get_dpdmux_from_id(dpdmux_id);
	if (!dpdmux) {
		DPAA2_PMD_ERR("Invalid dpdmux_id: %d", dpdmux_id);
		return;
	}

	for (if_id = 0; if_id < num_if; if_id++) {
		fprintf(f, "dpdmux.%d\n", if_id);

		ret = dpdmux_if_get_counter(&dpdmux->dpdmux, CMD_PRI_LOW,
			dpdmux->token, if_id, DPDMUX_CNT_ING_FRAME, &counter);
		if (!ret)
			fprintf(f, "DPDMUX_CNT_ING_FRAME %" PRIu64 "\n",
				counter);
		ret = dpdmux_if_get_counter(&dpdmux->dpdmux, CMD_PRI_LOW,
			dpdmux->token, if_id, DPDMUX_CNT_ING_BYTE, &counter);
		if (!ret)
			fprintf(f, "DPDMUX_CNT_ING_BYTE %" PRIu64 "\n",
				counter);
		ret = dpdmux_if_get_counter(&dpdmux->dpdmux, CMD_PRI_LOW,
			dpdmux->token, if_id, DPDMUX_CNT_ING_FLTR_FRAME,
			&counter);
		if (!ret)
			fprintf(f, "DPDMUX_CNT_ING_FLTR_FRAME %" PRIu64 "\n",
				counter);
		ret = dpdmux_if_get_counter(&dpdmux->dpdmux, CMD_PRI_LOW,
			dpdmux->token, if_id, DPDMUX_CNT_ING_FRAME_DISCARD,
			&counter);
		if (!ret)
			fprintf(f, "DPDMUX_CNT_ING_FRAME_DISCARD %" PRIu64 "\n",
				counter);
		ret = dpdmux_if_get_counter(&dpdmux->dpdmux, CMD_PRI_LOW,
			dpdmux->token, if_id, DPDMUX_CNT_ING_MCAST_FRAME,
			&counter);
		if (!ret)
			fprintf(f, "DPDMUX_CNT_ING_MCAST_FRAME %" PRIu64 "\n",
				counter);
		ret = dpdmux_if_get_counter(&dpdmux->dpdmux, CMD_PRI_LOW,
			dpdmux->token, if_id, DPDMUX_CNT_ING_MCAST_BYTE,
			&counter);
		if (!ret)
			fprintf(f, "DPDMUX_CNT_ING_MCAST_BYTE %" PRIu64 "\n",
				counter);
		ret = dpdmux_if_get_counter(&dpdmux->dpdmux, CMD_PRI_LOW,
			dpdmux->token, if_id, DPDMUX_CNT_ING_BCAST_FRAME,
			&counter);
		if (!ret)
			fprintf(f, "DPDMUX_CNT_ING_BCAST_FRAME %" PRIu64 "\n",
				counter);
		ret = dpdmux_if_get_counter(&dpdmux->dpdmux, CMD_PRI_LOW,
			dpdmux->token, if_id, DPDMUX_CNT_ING_BCAST_BYTES,
			&counter);
		if (!ret)
			fprintf(f, "DPDMUX_CNT_ING_BCAST_BYTES %" PRIu64 "\n",
				counter);
		ret = dpdmux_if_get_counter(&dpdmux->dpdmux, CMD_PRI_LOW,
			dpdmux->token, if_id, DPDMUX_CNT_EGR_FRAME, &counter);
		if (!ret)
			fprintf(f, "DPDMUX_CNT_EGR_FRAME %" PRIu64 "\n",
				counter);
		ret = dpdmux_if_get_counter(&dpdmux->dpdmux, CMD_PRI_LOW,
			dpdmux->token, if_id, DPDMUX_CNT_EGR_BYTE, &counter);
		if (!ret)
			fprintf(f, "DPDMUX_CNT_EGR_BYTE %" PRIu64 "\n",
				counter);
		ret = dpdmux_if_get_counter(&dpdmux->dpdmux, CMD_PRI_LOW,
			dpdmux->token, if_id, DPDMUX_CNT_EGR_FRAME_DISCARD,
			&counter);
		if (!ret)
			fprintf(f, "DPDMUX_CNT_EGR_FRAME_DISCARD %" PRIu64 "\n",
				counter);
	}
}

static int
dpaa2_create_dpdmux_device(int vdev_fd __rte_unused,
	struct vfio_device_info *obj_info __rte_unused,
	struct rte_dpaa2_device *obj)
{
	struct dpaa2_dpdmux_dev *dpdmux_dev;
	struct dpdmux_attr attr;
	int ret, dpdmux_id = obj->object_id;
	uint16_t maj_ver;
	uint16_t min_ver;
	uint8_t skip_reset_flags;

	PMD_INIT_FUNC_TRACE();

	/* Allocate DPAA2 dpdmux handle */
	dpdmux_dev = rte_zmalloc(NULL,
		sizeof(struct dpaa2_dpdmux_dev), RTE_CACHE_LINE_SIZE);
	if (!dpdmux_dev) {
		DPAA2_PMD_ERR("Memory allocation failed for DPDMUX Device");
		return -ENOMEM;
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

	if (attr.method != DPDMUX_METHOD_C_VLAN_MAC) {
		ret = dpdmux_if_set_default(&dpdmux_dev->dpdmux, CMD_PRI_LOW,
				dpdmux_dev->token, attr.default_if);
		if (ret) {
			DPAA2_PMD_ERR("setting default interface failed in %s",
				      __func__);
			goto init_err;
		}
		skip_reset_flags = DPDMUX_SKIP_DEFAULT_INTERFACE
			| DPDMUX_SKIP_UNICAST_RULES | DPDMUX_SKIP_MULTICAST_RULES;
	} else {
		skip_reset_flags = DPDMUX_SKIP_DEFAULT_INTERFACE;
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
				dpdmux_dev->token, skip_reset_flags);
		if (ret) {
			DPAA2_PMD_ERR("setting default interface failed in %s",
				      __func__);
			goto init_err;
		}
	}

	if (maj_ver >= 6 && min_ver >= 9) {
		struct dpdmux_error_cfg mux_err_cfg;

		memset(&mux_err_cfg, 0, sizeof(mux_err_cfg));
		/* Note: Discarded flag(DPDMUX_ERROR_DISC) has effect only when
		 * ERROR_ACTION is set to DPNI_ERROR_ACTION_SEND_TO_ERROR_QUEUE.
		 */
		mux_err_cfg.errors = DPDMUX_ALL_ERRORS;
		mux_err_cfg.error_action = DPDMUX_ERROR_ACTION_CONTINUE;

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

static void
dpaa2_close_dpdmux_device(int object_id)
{
	struct dpaa2_dpdmux_dev *dpdmux_dev;

	dpdmux_dev = get_dpdmux_from_id((uint32_t)object_id);

	if (dpdmux_dev) {
		dpdmux_close(&dpdmux_dev->dpdmux, CMD_PRI_LOW,
			     dpdmux_dev->token);
		TAILQ_REMOVE(&dpdmux_dev_list, dpdmux_dev, next);
		rte_free(dpdmux_dev);
	}
}

static struct rte_dpaa2_object rte_dpaa2_dpdmux_obj = {
	.dev_type = DPAA2_MUX,
	.create = dpaa2_create_dpdmux_device,
	.close = dpaa2_close_dpdmux_device,
};

RTE_PMD_REGISTER_DPAA2_OBJECT(dpdmux, rte_dpaa2_dpdmux_obj);
