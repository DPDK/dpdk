/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017-2023 NXP
 */

/* System headers */
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/types.h>

#include <dpaa_ethdev.h>
#include <dpaa_flow.h>
#include <rte_dpaa_logs.h>
#include <fmlib/fm_port_ext.h>
#include <fmlib/fm_vsp_ext.h>

#define FMC_OUTPUT_FORMAT_VER 0x106

#define FMC_NAME_LEN             64
#define FMC_FMAN_NUM              2
#define FMC_PORTS_PER_FMAN       16
#define FMC_SCHEMES_NUM          32
#define FMC_SCHEME_PROTOCOLS_NUM 16
#define FMC_CC_NODES_NUM        512
#define FMC_REPLICATORS_NUM      16
#define FMC_PLC_NUM              64
#define MAX_SP_CODE_SIZE      0x7C0
#define FMC_MANIP_MAX            64
#define FMC_HMANIP_MAX          512
#define FMC_INSERT_MAX           56
#define FM_PCD_MAX_REPS          64

typedef struct fmc_port_t {
	e_fm_port_type type;
	unsigned int number;
	struct fm_pcd_net_env_params_t distinction_units;
	struct ioc_fm_port_pcd_params_t pcd_param;
	struct ioc_fm_port_pcd_prs_params_t prs_param;
	struct ioc_fm_port_pcd_kg_params_t kg_param;
	struct ioc_fm_port_pcd_cc_params_t cc_param;
	char name[FMC_NAME_LEN];
	char cctree_name[FMC_NAME_LEN];
	t_handle handle;
	t_handle env_id_handle;
	t_handle env_id_dev_id;
	t_handle cctree_handle;
	t_handle cctree_dev_id;

	unsigned int schemes_count;
	unsigned int schemes[FMC_SCHEMES_NUM];
	unsigned int ccnodes_count;
	unsigned int ccnodes[FMC_CC_NODES_NUM];
	unsigned int htnodes_count;
	unsigned int htnodes[FMC_CC_NODES_NUM];

	unsigned int replicators_count;
	unsigned int replicators[FMC_REPLICATORS_NUM];
	ioc_fm_port_vsp_alloc_params_t vsp_param;

	unsigned int ccroot_count;
	unsigned int ccroot[FMC_CC_NODES_NUM];
	enum ioc_fm_pcd_engine ccroot_type[FMC_CC_NODES_NUM];
	unsigned int ccroot_manip[FMC_CC_NODES_NUM];

	unsigned int reasm_index;
} fmc_port;

typedef struct fmc_fman_t {
	unsigned int number;
	unsigned int port_count;
	unsigned int ports[FMC_PORTS_PER_FMAN];
	char name[FMC_NAME_LEN];
	t_handle handle;
	char pcd_name[FMC_NAME_LEN];
	t_handle pcd_handle;
	unsigned int kg_payload_offset;

	unsigned int offload_support;

	unsigned int reasm_count;
	struct fm_pcd_manip_params_t reasm[FMC_MANIP_MAX];
	char reasm_name[FMC_MANIP_MAX][FMC_NAME_LEN];
	t_handle reasm_handle[FMC_MANIP_MAX];
	t_handle reasm_dev_id[FMC_MANIP_MAX];

	unsigned int frag_count;
	struct fm_pcd_manip_params_t frag[FMC_MANIP_MAX];
	char frag_name[FMC_MANIP_MAX][FMC_NAME_LEN];
	t_handle frag_handle[FMC_MANIP_MAX];
	t_handle frag_dev_id[FMC_MANIP_MAX];

	unsigned int hdr_count;
	struct fm_pcd_manip_params_t hdr[FMC_HMANIP_MAX];
	uint8_t insert_data[FMC_HMANIP_MAX][FMC_INSERT_MAX];
	char hdr_name[FMC_HMANIP_MAX][FMC_NAME_LEN];
	t_handle hdr_handle[FMC_HMANIP_MAX];
	t_handle hdr_dev_id[FMC_HMANIP_MAX];
	unsigned int hdr_has_next[FMC_HMANIP_MAX];
	unsigned int hdr_next[FMC_HMANIP_MAX];
} fmc_fman;

typedef enum fmc_apply_order_e {
	fmcengine_start,
	fmcengine_end,
	fmcport_start,
	fmcport_end,
	fmcscheme,
	fmcccnode,
	fmchtnode,
	fmccctree,
	fmcpolicer,
	fmcreplicator,
	fmcmanipulation
} fmc_apply_order_e;

typedef struct fmc_apply_order_t {
	fmc_apply_order_e type;
	unsigned int index;
} fmc_apply_order;

struct fmc_model_t {
	unsigned int format_version;
	unsigned int sp_enable;
	t_fm_pcd_prs_sw_params sp;
	uint8_t spcode[MAX_SP_CODE_SIZE];

	unsigned int fman_count;
	fmc_fman fman[FMC_FMAN_NUM];

	unsigned int port_count;
	fmc_port port[FMC_FMAN_NUM * FMC_PORTS_PER_FMAN];

	unsigned int scheme_count;
	char scheme_name[FMC_SCHEMES_NUM][FMC_NAME_LEN];
	t_handle scheme_handle[FMC_SCHEMES_NUM];
	t_handle scheme_dev_id[FMC_SCHEMES_NUM];
	struct fm_pcd_kg_scheme_params_t scheme[FMC_SCHEMES_NUM];

	unsigned int ccnode_count;
	char ccnode_name[FMC_CC_NODES_NUM][FMC_NAME_LEN];
	t_handle ccnode_handle[FMC_CC_NODES_NUM];
	t_handle ccnode_dev_id[FMC_CC_NODES_NUM];
	struct fm_pcd_cc_node_params_t ccnode[FMC_CC_NODES_NUM];
	uint8_t cckeydata[FMC_CC_NODES_NUM][FM_PCD_MAX_NUM_OF_KEYS]
					[FM_PCD_MAX_SIZE_OF_KEY];
	unsigned char ccmask[FMC_CC_NODES_NUM][FM_PCD_MAX_NUM_OF_KEYS]
						[FM_PCD_MAX_SIZE_OF_KEY];
	unsigned int
		ccentry_action_index[FMC_CC_NODES_NUM][FM_PCD_MAX_NUM_OF_KEYS];
	enum ioc_fm_pcd_engine
		ccentry_action_type[FMC_CC_NODES_NUM][FM_PCD_MAX_NUM_OF_KEYS];
	unsigned char ccentry_frag[FMC_CC_NODES_NUM][FM_PCD_MAX_NUM_OF_KEYS];
	unsigned int ccentry_manip[FMC_CC_NODES_NUM][FM_PCD_MAX_NUM_OF_KEYS];
	unsigned int ccmiss_action_index[FMC_CC_NODES_NUM];
	enum ioc_fm_pcd_engine ccmiss_action_type[FMC_CC_NODES_NUM];
	unsigned char ccmiss_frag[FMC_CC_NODES_NUM];
	unsigned int ccmiss_manip[FMC_CC_NODES_NUM];

	unsigned int htnode_count;
	char htnode_name[FMC_CC_NODES_NUM][FMC_NAME_LEN];
	t_handle htnode_handle[FMC_CC_NODES_NUM];
	t_handle htnode_dev_id[FMC_CC_NODES_NUM];
	struct fm_pcd_hash_table_params_t htnode[FMC_CC_NODES_NUM];

	unsigned int htentry_count[FMC_CC_NODES_NUM];
	struct ioc_fm_pcd_cc_key_params_t
		htentry[FMC_CC_NODES_NUM][FM_PCD_MAX_NUM_OF_KEYS];
	uint8_t htkeydata[FMC_CC_NODES_NUM][FM_PCD_MAX_NUM_OF_KEYS]
					[FM_PCD_MAX_SIZE_OF_KEY];
	unsigned int
		htentry_action_index[FMC_CC_NODES_NUM][FM_PCD_MAX_NUM_OF_KEYS];
	enum ioc_fm_pcd_engine
		htentry_action_type[FMC_CC_NODES_NUM][FM_PCD_MAX_NUM_OF_KEYS];
	unsigned char htentry_frag[FMC_CC_NODES_NUM][FM_PCD_MAX_NUM_OF_KEYS];
	unsigned int htentry_manip[FMC_CC_NODES_NUM][FM_PCD_MAX_NUM_OF_KEYS];

	unsigned int htmiss_action_index[FMC_CC_NODES_NUM];
	enum ioc_fm_pcd_engine htmiss_action_type[FMC_CC_NODES_NUM];
	unsigned char htmiss_frag[FMC_CC_NODES_NUM];
	unsigned int htmiss_manip[FMC_CC_NODES_NUM];

	unsigned int replicator_count;
	char replicator_name[FMC_REPLICATORS_NUM][FMC_NAME_LEN];
	t_handle replicator_handle[FMC_REPLICATORS_NUM];
	t_handle replicator_dev_id[FMC_REPLICATORS_NUM];
	struct fm_pcd_frm_replic_group_params_t replicator[FMC_REPLICATORS_NUM];
	unsigned int
	 repentry_action_index[FMC_REPLICATORS_NUM][FM_PCD_MAX_REPS];
	unsigned char repentry_frag[FMC_REPLICATORS_NUM][FM_PCD_MAX_REPS];
	unsigned int repentry_manip[FMC_REPLICATORS_NUM][FM_PCD_MAX_REPS];

	unsigned int policer_count;
	char policer_name[FMC_PLC_NUM][FMC_NAME_LEN];
	struct fm_pcd_plcr_profile_params_t policer[FMC_PLC_NUM];
	t_handle policer_handle[FMC_PLC_NUM];
	t_handle policer_dev_id[FMC_PLC_NUM];
	unsigned int policer_action_index[FMC_PLC_NUM][3];

	unsigned int apply_order_count;
	fmc_apply_order apply_order[FMC_FMAN_NUM *
		FMC_PORTS_PER_FMAN *
		(FMC_SCHEMES_NUM + FMC_CC_NODES_NUM)];
};

struct fmc_model_t *g_fmc_model;

static int
dpaa_port_fmc_port_parse(struct fman_if *fif,
	const struct fmc_model_t *fmc_model,
	int apply_idx)
{
	int current_port = fmc_model->apply_order[apply_idx].index;
	const fmc_port *pport = &fmc_model->port[current_port];
	uint32_t num;

	if (pport->type == e_FM_PORT_TYPE_OH_OFFLINE_PARSING &&
	    pport->number == fif->mac_idx &&
	    (fif->mac_type == fman_offline_internal ||
	     fif->mac_type == fman_onic))
		return current_port;

	if (fif->mac_type == fman_mac_1g) {
		if (pport->type != e_FM_PORT_TYPE_RX)
			return -ENODEV;
		num = pport->number + DPAA_1G_MAC_START_IDX;
		if (fif->mac_idx == num)
			return current_port;

		return -ENODEV;
	}

	if (fif->mac_type == fman_mac_2_5g) {
		if (pport->type != e_FM_PORT_TYPE_RX_2_5G)
			return -ENODEV;
		num = pport->number + DPAA_2_5G_MAC_START_IDX;
		if (fif->mac_idx == num)
			return current_port;

		return -ENODEV;
	}

	if (fif->mac_type == fman_mac_10g) {
		if (pport->type != e_FM_PORT_TYPE_RX_10G)
			return -ENODEV;
		num = pport->number + DPAA_10G_MAC_START_IDX;
		if (fif->mac_idx == num)
			return current_port;

		return -ENODEV;
	}

	DPAA_PMD_ERR("Invalid MAC(mac_idx=%d) type(%d)",
		fif->mac_idx, fif->mac_type);

	return -EINVAL;
}

static int
dpaa_fq_is_in_kernel(uint32_t fqid,
	struct fman_if *fif)
{
	if (!fif->is_shared_mac)
		return false;

	if ((fqid == fif->fqid_rx_def ||
		(fqid >= fif->fqid_rx_pcd &&
		fqid < (fif->fqid_rx_pcd + fif->fqid_rx_pcd_count)) ||
		fqid == fif->fqid_rx_err ||
		fqid == fif->fqid_tx_err))
		return true;

	return false;
}

static int
dpaa_vsp_id_is_in_kernel(uint8_t vsp_id,
	struct fman_if *fif)
{
	if (!fif->is_shared_mac)
		return false;

	if (vsp_id == fif->base_profile_id)
		return true;

	return false;
}

static uint8_t
dpaa_enqueue_vsp_id(struct fman_if *fif,
	const struct ioc_fm_pcd_cc_next_enqueue_params_t *eq_param)
{
	if (eq_param->override_fqid)
		return eq_param->new_relative_storage_profile_id;

	return fif->base_profile_id;
}

static int
dpaa_kg_storage_is_in_kernel(struct fman_if *fif,
	const struct ioc_fm_pcd_kg_storage_profile_t *kg_storage)
{
	if (!fif->is_shared_mac)
		return false;

	if (!kg_storage->direct ||
		(kg_storage->direct &&
		kg_storage->profile_select.direct_relative_profile_id ==
		fif->base_profile_id))
		return true;

	return false;
}

static void
dpaa_fmc_remove_fq_from_allocated(uint32_t *fqids,
	uint16_t *rxq_idx, uint32_t rm_fqid)
{
	uint32_t i;

	for (i = 0; i < (*rxq_idx); i++) {
		if (fqids[i] != rm_fqid)
			continue;
		DPAA_PMD_WARN("Remove fq(0x%08x) allocated.",
			rm_fqid);
		if ((*rxq_idx) > (i + 1)) {
			memmove(&fqids[i], &fqids[i + 1],
				((*rxq_idx) - (i + 1)) * sizeof(uint32_t));
		}
		(*rxq_idx)--;
		break;
	}
}

static int
dpaa_port_fmc_scheme_parse(struct fman_if *fif,
	const struct fmc_model_t *fmc,
	int apply_idx,
	uint16_t *rxq_idx, int max_nb_rxq,
	uint32_t *fqids, int8_t *vspids)
{
	int scheme_idx = fmc->apply_order[apply_idx].index;
	int k, found = 0;
	uint32_t i, num_rxq, fqid, rxq_idx_start = *rxq_idx;
	const struct fm_pcd_kg_scheme_params_t *scheme;
	const struct ioc_fm_pcd_kg_key_extract_and_hash_params_t *params;
	const struct ioc_fm_pcd_kg_storage_profile_t *kg_storage;
	uint8_t vsp_id;

	scheme = &fmc->scheme[scheme_idx];
	params = &scheme->key_ext_and_hash;
	num_rxq = params->hash_dist_num_of_fqids;
	kg_storage = &scheme->storage_profile;

	if (scheme->override_storage_profile && kg_storage->direct)
		vsp_id = kg_storage->profile_select.direct_relative_profile_id;
	else
		vsp_id = fif->base_profile_id;

	if (dpaa_kg_storage_is_in_kernel(fif, kg_storage)) {
		DPAA_PMD_WARN("Scheme[%d]'s VSP is in kernel",
			scheme_idx);
		/* The FQ may be allocated from previous CC or scheme,
		 * find and remove it.
		 */
		for (i = 0; i < num_rxq; i++) {
			fqid = scheme->base_fqid + i;
			DPAA_PMD_WARN("Removed fqid(0x%08x) of Scheme[%d]",
				fqid, scheme_idx);
			dpaa_fmc_remove_fq_from_allocated(fqids,
				rxq_idx, fqid);
			if (!dpaa_fq_is_in_kernel(fqid, fif)) {
				char reason_msg[128];
				char result_msg[128];

				sprintf(reason_msg,
					"NOT handled in kernel");
				sprintf(result_msg,
					"will DRAIN kernel pool!");
				DPAA_PMD_WARN("Traffic to FQ(%08x)(%s) %s",
					fqid, reason_msg, result_msg);
			}
		}

		return 0;
	}

	if (e_IOC_FM_PCD_DONE != scheme->next_engine) {
		/* Do nothing.*/
		DPAA_PMD_DEBUG("Will parse scheme[%d]'s next engine(%d)",
			scheme_idx, scheme->next_engine);
		return 0;
	}

	for (i = 0; i < num_rxq; i++) {
		fqid = scheme->base_fqid + i;
		found = 0;

		if (dpaa_fq_is_in_kernel(fqid, fif)) {
			DPAA_PMD_WARN("FQ(0x%08x) is handled in kernel.",
				fqid);
			/* The FQ may be allocated from previous CC or scheme,
			 * remove it.
			 */
			dpaa_fmc_remove_fq_from_allocated(fqids,
				rxq_idx, fqid);
			continue;
		}

		if ((*rxq_idx) >= max_nb_rxq) {
			DPAA_PMD_WARN("Too many queues(%d) >= MAX number(%d)",
				(*rxq_idx), max_nb_rxq);

			break;
		}

		for (k = 0; k < (*rxq_idx); k++) {
			if (fqids[k] == fqid) {
				found = 1;
				break;
			}
		}

		if (found)
			continue;
		fqids[(*rxq_idx)] = fqid;
		vspids[(*rxq_idx)] = vsp_id;

		(*rxq_idx)++;
	}

	return (*rxq_idx) - rxq_idx_start;
}

static int
dpaa_port_fmc_ccnode_parse(struct fman_if *fif,
	const struct fmc_model_t *fmc,
	int apply_idx,
	uint16_t *rxq_idx, int max_nb_rxq,
	uint32_t *fqids, int8_t *vspids)
{
	uint16_t j, k, found = 0;
	const struct ioc_keys_params_t *keys_params;
	const struct ioc_fm_pcd_cc_next_engine_params_t *params;
	uint32_t fqid, cc_idx = fmc->apply_order[apply_idx].index;
	uint32_t rxq_idx_start = *rxq_idx;
	uint8_t vsp_id;

	keys_params = &fmc->ccnode[cc_idx].keys_params;

	for (j = 0; j < keys_params->num_of_keys; ++j) {
		if ((*rxq_idx) >= max_nb_rxq) {
			DPAA_PMD_WARN("Too many queues(%d) >= MAX number(%d)",
				(*rxq_idx), max_nb_rxq);

			break;
		}
		found = 0;
		params = &keys_params->key_params[j].cc_next_engine_params;

		/* We read DPDK queue from last classification rule present in
		 * FMC policy file. Hence, this check is required here.
		 * Also, the last classification rule in FMC policy file must
		 * have userspace queue so that it can be used by DPDK
		 * application.
		 */
		if (params->next_engine != e_IOC_FM_PCD_DONE) {
			DPAA_PMD_WARN("CC next engine(%d) not support",
				params->next_engine);
			continue;
		}
		if (params->params.enqueue_params.action !=
			e_IOC_FM_PCD_ENQ_FRAME)
			continue;

		fqid = params->params.enqueue_params.new_fqid;
		vsp_id = dpaa_enqueue_vsp_id(fif,
			&params->params.enqueue_params);
		if (dpaa_fq_is_in_kernel(fqid, fif) ||
			dpaa_vsp_id_is_in_kernel(vsp_id, fif)) {
			DPAA_PMD_DEBUG("FQ(0x%08x)/VSP(%d) is in kernel.",
				fqid, vsp_id);
			/* The FQ may be allocated from previous CC or scheme,
			 * remove it.
			 */
			dpaa_fmc_remove_fq_from_allocated(fqids,
				rxq_idx, fqid);
			continue;
		}

		for (k = 0; k < (*rxq_idx); k++) {
			if (fqids[k] == fqid) {
				found = 1;
				break;
			}
		}
		if (found)
			continue;

		fqids[(*rxq_idx)] = fqid;
		vspids[(*rxq_idx)] = vsp_id;

		(*rxq_idx)++;
	}

	return (*rxq_idx) - rxq_idx_start;
}

int
dpaa_port_fmc_init(struct fman_if *fif,
	uint32_t *fqids, int8_t *vspids, int max_nb_rxq)
{
	int current_port = -1, ret;
	uint16_t rxq_idx = 0;
	const struct fmc_model_t *fmc;
	uint32_t i;

	if (!g_fmc_model) {
		size_t bytes_read;
		FILE *fp = fopen(FMC_FILE, "rb");

		if (!fp) {
			DPAA_PMD_ERR("%s not exists", FMC_FILE);
			return -ENOENT;
		}

		g_fmc_model = rte_malloc(NULL, sizeof(struct fmc_model_t), 64);
		if (!g_fmc_model) {
			DPAA_PMD_ERR("FMC memory alloc failed");
			fclose(fp);
			return -ENOBUFS;
		}

		bytes_read = fread(g_fmc_model,
				   sizeof(struct fmc_model_t), 1, fp);
		if (!bytes_read) {
			DPAA_PMD_ERR("No bytes read");
			fclose(fp);
			rte_free(g_fmc_model);
			g_fmc_model = NULL;
			return -EIO;
		}
		fclose(fp);
	}

	fmc = g_fmc_model;

	if (fmc->format_version != FMC_OUTPUT_FORMAT_VER) {
		DPAA_PMD_ERR("FMC version(0x%08x) != Supported ver(0x%08x)",
			fmc->format_version, FMC_OUTPUT_FORMAT_VER);
		return -EINVAL;
	}

	for (i = 0; i < fmc->apply_order_count; i++) {
		switch (fmc->apply_order[i].type) {
		case fmcengine_start:
			break;
		case fmcengine_end:
			break;
		case fmcport_start:
			current_port = dpaa_port_fmc_port_parse(fif,
				fmc, i);
			break;
		case fmcport_end:
			break;
		case fmcscheme:
			if (current_port < 0)
				break;

			ret = dpaa_port_fmc_scheme_parse(fif, fmc,
				i, &rxq_idx, max_nb_rxq, fqids, vspids);
			DPAA_PMD_INFO("%s %d RXQ(s) from scheme[%d]",
				ret >= 0 ? "Alloc" : "Remove",
				ret >= 0 ? ret : -ret,
				fmc->apply_order[i].index);

			break;
		case fmcccnode:
			if (current_port < 0)
				break;

			ret = dpaa_port_fmc_ccnode_parse(fif, fmc,
				i, &rxq_idx, max_nb_rxq, fqids, vspids);
			DPAA_PMD_INFO("%s %d RXQ(s) from cc[%d]",
				ret >= 0 ? "Alloc" : "Remove",
				ret >= 0 ? ret : -ret,
				fmc->apply_order[i].index);

			break;
		case fmchtnode:
			break;
		case fmcreplicator:
			break;
		case fmccctree:
			break;
		case fmcpolicer:
			break;
		case fmcmanipulation:
			break;
		default:
			break;
		}
	}

	return rxq_idx;
}
