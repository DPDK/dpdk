/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017-2019,2021 NXP
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

#define DPAA_MAX_NUM_ETH_DEV	8

static inline
ioc_fm_pcd_extract_entry_t *
SCH_EXT_ARR(ioc_fm_pcd_kg_scheme_params_t *scheme_params, int hdr_idx)
{
return &scheme_params->param.key_ext_and_hash.extract_array[hdr_idx];
}

#define SCH_EXT_HDR(scheme_params, hdr_idx) \
	SCH_EXT_ARR(scheme_params, hdr_idx)->extract_params.extract_by_hdr

#define SCH_EXT_FULL_FLD(scheme_params, hdr_idx) \
	SCH_EXT_HDR(scheme_params, hdr_idx).extract_by_hdr_type.full_field

/* FM global info */
struct dpaa_fm_info {
	t_handle fman_handle;
	t_handle pcd_handle;
};

/*FM model to read and write from file */
struct dpaa_fm_model {
	uint32_t dev_count;
	uint8_t device_order[DPAA_MAX_NUM_ETH_DEV];
	t_fm_port_params fm_port_params[DPAA_MAX_NUM_ETH_DEV];
	t_handle netenv_devid[DPAA_MAX_NUM_ETH_DEV];
	t_handle scheme_devid[DPAA_MAX_NUM_ETH_DEV][2];
};

static struct dpaa_fm_info fm_info;
static struct dpaa_fm_model fm_model;
static const char *fm_log = "/tmp/fmdpdk.bin";

static inline uint8_t fm_default_vsp_id(struct fman_if *fif)
{
	/* Avoid being same as base profile which could be used
	 * for kernel interface of shared mac.
	 */
	if (fif->base_profile_id)
		return 0;
	else
		return DPAA_DEFAULT_RXQ_VSP_ID;
}

static void fm_prev_cleanup(void)
{
	uint32_t fman_id = 0, i = 0, devid;
	struct dpaa_if dpaa_intf = {0};
	t_fm_pcd_params fm_pcd_params = {0};
	PMD_INIT_FUNC_TRACE();

	fm_info.fman_handle = fm_open(fman_id);
	if (!fm_info.fman_handle) {
		printf("\n%s- unable to open FMAN", __func__);
		return;
	}

	fm_pcd_params.h_fm = fm_info.fman_handle;
	fm_pcd_params.prs_support = true;
	fm_pcd_params.kg_support = true;
	/* FM PCD Open */
	fm_info.pcd_handle = fm_pcd_open(&fm_pcd_params);
	if (!fm_info.pcd_handle) {
		printf("\n%s- unable to open PCD", __func__);
		return;
	}

	while (i < fm_model.dev_count) {
		devid = fm_model.device_order[i];
		/* FM Port Open */
		fm_model.fm_port_params[devid].h_fm = fm_info.fman_handle;
		dpaa_intf.port_handle =
				fm_port_open(&fm_model.fm_port_params[devid]);
		dpaa_intf.scheme_handle[0] = create_device(fm_info.pcd_handle,
					fm_model.scheme_devid[devid][0]);
		dpaa_intf.scheme_count = 1;
		if (fm_model.scheme_devid[devid][1]) {
			dpaa_intf.scheme_handle[1] =
				create_device(fm_info.pcd_handle,
					fm_model.scheme_devid[devid][1]);
			if (dpaa_intf.scheme_handle[1])
				dpaa_intf.scheme_count++;
		}

		dpaa_intf.netenv_handle = create_device(fm_info.pcd_handle,
					fm_model.netenv_devid[devid]);
		i++;
		if (!dpaa_intf.netenv_handle ||
			!dpaa_intf.scheme_handle[0] ||
			!dpaa_intf.port_handle)
			continue;

		if (dpaa_fm_deconfig(&dpaa_intf, NULL))
			printf("\nDPAA FM deconfig failed\n");
	}

	if (dpaa_fm_term())
		printf("\nDPAA FM term failed\n");

	memset(&fm_model, 0, sizeof(struct dpaa_fm_model));
}

void dpaa_write_fm_config_to_file(void)
{
	size_t bytes_write;
	FILE *fp = fopen(fm_log, "wb");
	PMD_INIT_FUNC_TRACE();

	if (!fp) {
		DPAA_PMD_ERR("File open failed");
		return;
	}
	bytes_write = fwrite(&fm_model, sizeof(struct dpaa_fm_model), 1, fp);
	if (!bytes_write) {
		DPAA_PMD_WARN("No bytes write");
		fclose(fp);
		return;
	}
	fclose(fp);
}

static void dpaa_read_fm_config_from_file(void)
{
	size_t bytes_read;
	FILE *fp = fopen(fm_log, "rb");
	PMD_INIT_FUNC_TRACE();

	if (!fp)
		return;
	DPAA_PMD_INFO("Previous DPDK-FM config instance present, cleaning up.");

	bytes_read = fread(&fm_model, sizeof(struct dpaa_fm_model), 1, fp);
	if (!bytes_read) {
		DPAA_PMD_WARN("No bytes read");
		fclose(fp);
		return;
	}
	fclose(fp);

	/*FM cleanup from previous configured app */
	fm_prev_cleanup();
}

static inline int
set_hash_params_eth(ioc_fm_pcd_kg_scheme_params_t *scheme_params, int hdr_idx)
{
	int k;

	for (k = 0; k < 2; k++) {
		SCH_EXT_ARR(scheme_params, hdr_idx)->type =
						e_IOC_FM_PCD_EXTRACT_BY_HDR;
		SCH_EXT_HDR(scheme_params, hdr_idx).hdr =
						HEADER_TYPE_ETH;
		SCH_EXT_HDR(scheme_params, hdr_idx).hdr_index =
						e_IOC_FM_PCD_HDR_INDEX_NONE;
		SCH_EXT_HDR(scheme_params, hdr_idx).type =
						e_IOC_FM_PCD_EXTRACT_FULL_FIELD;
		if (k == 0)
			SCH_EXT_FULL_FLD(scheme_params, hdr_idx).eth =
						IOC_NET_HF_ETH_SA;
		else
			SCH_EXT_FULL_FLD(scheme_params, hdr_idx).eth =
						IOC_NET_HF_ETH_DA;
		hdr_idx++;
	}
	return hdr_idx;
}

static inline int
set_hash_params_ipv4(ioc_fm_pcd_kg_scheme_params_t *scheme_params, int hdr_idx)
{
	int k;

	for (k = 0; k < 2; k++) {
		SCH_EXT_ARR(scheme_params, hdr_idx)->type =
						e_IOC_FM_PCD_EXTRACT_BY_HDR;
		SCH_EXT_HDR(scheme_params, hdr_idx).hdr =
						HEADER_TYPE_IPV4;
		SCH_EXT_HDR(scheme_params, hdr_idx).hdr_index =
						e_IOC_FM_PCD_HDR_INDEX_NONE;
		SCH_EXT_HDR(scheme_params, hdr_idx).type =
						e_IOC_FM_PCD_EXTRACT_FULL_FIELD;
		if (k == 0)
			SCH_EXT_FULL_FLD(scheme_params, hdr_idx).ipv4 =
					ioc_net_hf_ipv_4_src_ip;
		else
			SCH_EXT_FULL_FLD(scheme_params, hdr_idx).ipv4 =
					ioc_net_hf_ipv_4_dst_ip;
		hdr_idx++;
	}
	return hdr_idx;
}

static inline int
set_hash_params_ipv6(ioc_fm_pcd_kg_scheme_params_t *scheme_params, int hdr_idx)
{
	int k;

	for (k = 0; k < 2; k++) {
		SCH_EXT_ARR(scheme_params, hdr_idx)->type =
						e_IOC_FM_PCD_EXTRACT_BY_HDR;
		SCH_EXT_HDR(scheme_params, hdr_idx).hdr =
							HEADER_TYPE_IPV6;
		SCH_EXT_HDR(scheme_params, hdr_idx).hdr_index =
						e_IOC_FM_PCD_HDR_INDEX_NONE;
		SCH_EXT_HDR(scheme_params, hdr_idx).type =
						e_IOC_FM_PCD_EXTRACT_FULL_FIELD;
		if (k == 0)
			SCH_EXT_FULL_FLD(scheme_params, hdr_idx).ipv6 =
					ioc_net_hf_ipv_6_src_ip;
		else
			SCH_EXT_FULL_FLD(scheme_params, hdr_idx).ipv6 =
					ioc_net_hf_ipv_6_dst_ip;
		hdr_idx++;
	}
	return hdr_idx;
}

static inline int
set_hash_params_udp(ioc_fm_pcd_kg_scheme_params_t *scheme_params, int hdr_idx)
{
	int k;

	for (k = 0; k < 2; k++) {
		SCH_EXT_ARR(scheme_params, hdr_idx)->type =
						e_IOC_FM_PCD_EXTRACT_BY_HDR;
		SCH_EXT_HDR(scheme_params, hdr_idx).hdr =
						HEADER_TYPE_UDP;
		SCH_EXT_HDR(scheme_params, hdr_idx).hdr_index =
						e_IOC_FM_PCD_HDR_INDEX_NONE;
		SCH_EXT_HDR(scheme_params, hdr_idx).type =
						e_IOC_FM_PCD_EXTRACT_FULL_FIELD;
		if (k == 0)
			SCH_EXT_FULL_FLD(scheme_params, hdr_idx).udp =
					IOC_NET_HF_UDP_PORT_SRC;
		else
			SCH_EXT_FULL_FLD(scheme_params, hdr_idx).udp =
					IOC_NET_HF_UDP_PORT_DST;
		hdr_idx++;
	}
	return hdr_idx;
}

static inline int
set_hash_params_tcp(ioc_fm_pcd_kg_scheme_params_t *scheme_params, int hdr_idx)
{
	int k;

	for (k = 0; k < 2; k++) {
		SCH_EXT_ARR(scheme_params, hdr_idx)->type =
						e_IOC_FM_PCD_EXTRACT_BY_HDR;
		SCH_EXT_HDR(scheme_params, hdr_idx).hdr =
						HEADER_TYPE_TCP;
		SCH_EXT_HDR(scheme_params, hdr_idx).hdr_index =
						e_IOC_FM_PCD_HDR_INDEX_NONE;
		SCH_EXT_HDR(scheme_params, hdr_idx).type =
						e_IOC_FM_PCD_EXTRACT_FULL_FIELD;
		if (k == 0)
			SCH_EXT_FULL_FLD(scheme_params, hdr_idx).tcp =
					IOC_NET_HF_TCP_PORT_SRC;
		else
			SCH_EXT_FULL_FLD(scheme_params, hdr_idx).tcp =
					IOC_NET_HF_TCP_PORT_DST;
		hdr_idx++;
	}
	return hdr_idx;
}

static inline int
set_hash_params_sctp(ioc_fm_pcd_kg_scheme_params_t *scheme_params, int hdr_idx)
{
	int k;

	for (k = 0; k < 2; k++) {
		SCH_EXT_ARR(scheme_params, hdr_idx)->type =
						e_IOC_FM_PCD_EXTRACT_BY_HDR;
		SCH_EXT_HDR(scheme_params, hdr_idx).hdr =
						HEADER_TYPE_SCTP;
		SCH_EXT_HDR(scheme_params, hdr_idx).hdr_index =
						e_IOC_FM_PCD_HDR_INDEX_NONE;
		SCH_EXT_HDR(scheme_params, hdr_idx).type =
						e_IOC_FM_PCD_EXTRACT_FULL_FIELD;
		if (k == 0)
			SCH_EXT_FULL_FLD(scheme_params, hdr_idx).sctp =
					IOC_NET_HF_SCTP_PORT_SRC;
		else
			SCH_EXT_FULL_FLD(scheme_params, hdr_idx).sctp =
					IOC_NET_HF_SCTP_PORT_DST;
		hdr_idx++;
	}
	return hdr_idx;
}

/* Set scheme params for hash distribution */
static int set_scheme_params(ioc_fm_pcd_kg_scheme_params_t *scheme_params,
	ioc_fm_pcd_net_env_params_t *dist_units,
	struct dpaa_if *dpaa_intf,
	struct fman_if *fif)
{
	int dist_idx, hdr_idx = 0;
	PMD_INIT_FUNC_TRACE();

	if (fif->num_profiles) {
		scheme_params->param.override_storage_profile = true;
		scheme_params->param.storage_profile.direct = true;
		scheme_params->param.storage_profile.profile_select
			.direct_relative_profile_id = fm_default_vsp_id(fif);
	}

	scheme_params->param.use_hash = 1;
	scheme_params->param.modify = false;
	scheme_params->param.always_direct = false;
	scheme_params->param.scheme_counter.update = 1;
	scheme_params->param.scheme_counter.value = 0;
	scheme_params->param.next_engine = e_IOC_FM_PCD_DONE;
	scheme_params->param.base_fqid = dpaa_intf->rx_queues[0].fqid;
	scheme_params->param.net_env_params.net_env_id =
		dpaa_intf->netenv_handle;
	scheme_params->param.net_env_params.num_of_distinction_units =
		dist_units->param.num_of_distinction_units;

	scheme_params->param.key_ext_and_hash.hash_dist_num_of_fqids =
			dpaa_intf->nb_rx_queues;
	scheme_params->param.key_ext_and_hash.num_of_used_extracts =
			2 * dist_units->param.num_of_distinction_units;

	for (dist_idx = 0; dist_idx <
		dist_units->param.num_of_distinction_units;
		dist_idx++) {
		switch (dist_units->param.units[dist_idx].hdrs[0].hdr) {
		case HEADER_TYPE_ETH:
			hdr_idx = set_hash_params_eth(scheme_params, hdr_idx);
			break;

		case HEADER_TYPE_IPV4:
			hdr_idx = set_hash_params_ipv4(scheme_params, hdr_idx);
			break;

		case HEADER_TYPE_IPV6:
			hdr_idx = set_hash_params_ipv6(scheme_params, hdr_idx);
			break;

		case HEADER_TYPE_UDP:
			hdr_idx = set_hash_params_udp(scheme_params, hdr_idx);
			break;

		case HEADER_TYPE_TCP:
			hdr_idx = set_hash_params_tcp(scheme_params, hdr_idx);
			break;

		case HEADER_TYPE_SCTP:
			hdr_idx = set_hash_params_sctp(scheme_params, hdr_idx);
			break;

		default:
			DPAA_PMD_ERR("Invalid Distinction Unit");
			return -1;
		}
	}

	return 0;
}

static void set_dist_units(ioc_fm_pcd_net_env_params_t *dist_units,
			   uint64_t req_dist_set)
{
	uint32_t loop = 0, dist_idx = 0, dist_field = 0;
	int l2_configured = 0, ipv4_configured = 0, ipv6_configured = 0;
	int udp_configured = 0, tcp_configured = 0, sctp_configured = 0;
	PMD_INIT_FUNC_TRACE();

	if (!req_dist_set)
		dist_units->param.units[dist_idx++].hdrs[0].hdr =
			HEADER_TYPE_ETH;

	while (req_dist_set) {
		if (req_dist_set % 2 != 0) {
			dist_field = 1U << loop;
			switch (dist_field) {
			case RTE_ETH_RSS_L2_PAYLOAD:

				if (l2_configured)
					break;
				l2_configured = 1;

				dist_units->param.units[dist_idx++].hdrs[0].hdr
					= HEADER_TYPE_ETH;
				break;

			case RTE_ETH_RSS_IPV4:
			case RTE_ETH_RSS_FRAG_IPV4:
			case RTE_ETH_RSS_NONFRAG_IPV4_OTHER:

				if (ipv4_configured)
					break;
				ipv4_configured = 1;
				dist_units->param.units[dist_idx++].hdrs[0].hdr
					= HEADER_TYPE_IPV4;
				break;

			case RTE_ETH_RSS_IPV6:
			case RTE_ETH_RSS_FRAG_IPV6:
			case RTE_ETH_RSS_NONFRAG_IPV6_OTHER:
			case RTE_ETH_RSS_IPV6_EX:

				if (ipv6_configured)
					break;
				ipv6_configured = 1;
				dist_units->param.units[dist_idx++].hdrs[0].hdr
					= HEADER_TYPE_IPV6;
				break;

			case RTE_ETH_RSS_NONFRAG_IPV4_TCP:
			case RTE_ETH_RSS_NONFRAG_IPV6_TCP:
			case RTE_ETH_RSS_IPV6_TCP_EX:

				if (tcp_configured)
					break;
				tcp_configured = 1;
				dist_units->param.units[dist_idx++].hdrs[0].hdr
					= HEADER_TYPE_TCP;
				break;

			case RTE_ETH_RSS_NONFRAG_IPV4_UDP:
			case RTE_ETH_RSS_NONFRAG_IPV6_UDP:
			case RTE_ETH_RSS_IPV6_UDP_EX:

				if (udp_configured)
					break;
				udp_configured = 1;
				dist_units->param.units[dist_idx++].hdrs[0].hdr
					= HEADER_TYPE_UDP;
				break;

			case RTE_ETH_RSS_NONFRAG_IPV4_SCTP:
			case RTE_ETH_RSS_NONFRAG_IPV6_SCTP:

				if (sctp_configured)
					break;
				sctp_configured = 1;

				dist_units->param.units[dist_idx++].hdrs[0].hdr
					= HEADER_TYPE_SCTP;
				break;

			default:
				DPAA_PMD_ERR("Bad flow distribution option");
			}
		}
		req_dist_set = req_dist_set >> 1;
		loop++;
	}

	/* Dist units is set to dist_idx */
	dist_units->param.num_of_distinction_units = dist_idx;
}

/* Apply PCD configuration on interface */
static inline int set_port_pcd(struct dpaa_if *dpaa_intf)
{
	int ret = 0;
	unsigned int idx;
	ioc_fm_port_pcd_params_t pcd_param;
	ioc_fm_port_pcd_prs_params_t prs_param;
	ioc_fm_port_pcd_kg_params_t  kg_param;

	PMD_INIT_FUNC_TRACE();

	/* PCD support for hash distribution */
	uint8_t pcd_support = e_FM_PORT_PCD_SUPPORT_PRS_AND_KG;

	memset(&pcd_param, 0, sizeof(pcd_param));
	memset(&prs_param, 0, sizeof(prs_param));
	memset(&kg_param, 0, sizeof(kg_param));

	/* Set parse params */
	prs_param.first_prs_hdr = HEADER_TYPE_ETH;

	/* Set kg params */
	for (idx = 0; idx < dpaa_intf->scheme_count; idx++)
		kg_param.scheme_ids[idx] = dpaa_intf->scheme_handle[idx];
	kg_param.num_schemes = dpaa_intf->scheme_count;

	/* Set pcd params */
	pcd_param.net_env_id = dpaa_intf->netenv_handle;
	pcd_param.pcd_support = pcd_support;
	pcd_param.p_kg_params = &kg_param;
	pcd_param.p_prs_params = &prs_param;

	/* FM PORT Disable */
	ret = fm_port_disable(dpaa_intf->port_handle);
	if (ret != E_OK) {
		DPAA_PMD_ERR("fm_port_disable: Failed");
		return ret;
	}

	/* FM PORT SetPCD */
	ret = fm_port_set_pcd(dpaa_intf->port_handle, &pcd_param);
	if (ret != E_OK) {
		DPAA_PMD_ERR("fm_port_set_pcd: Failed");
		return ret;
	}

	/* FM PORT Enable */
	ret = fm_port_enable(dpaa_intf->port_handle);
	if (ret != E_OK) {
		DPAA_PMD_ERR("fm_port_enable: Failed");
		goto fm_port_delete_pcd;
	}

	return 0;

fm_port_delete_pcd:
	/* FM PORT DeletePCD */
	ret = fm_port_delete_pcd(dpaa_intf->port_handle);
	if (ret != E_OK) {
		DPAA_PMD_ERR("fm_port_delete_pcd: Failed\n");
		return ret;
	}
	return -1;
}

/* Unset PCD NerEnv and scheme */
static inline void unset_pcd_netenv_scheme(struct dpaa_if *dpaa_intf)
{
	int ret;
	PMD_INIT_FUNC_TRACE();

	/* reduce scheme count */
	if (dpaa_intf->scheme_count)
		dpaa_intf->scheme_count--;

	DPAA_PMD_DEBUG("KG SCHEME DEL %d handle =%p",
		dpaa_intf->scheme_count,
		dpaa_intf->scheme_handle[dpaa_intf->scheme_count]);

	ret = fm_pcd_kg_scheme_delete(dpaa_intf->scheme_handle
					[dpaa_intf->scheme_count]);
	if (ret != E_OK)
		DPAA_PMD_ERR("fm_pcd_kg_scheme_delete: Failed");

	dpaa_intf->scheme_handle[dpaa_intf->scheme_count] = NULL;
}

/* Set PCD NetEnv and Scheme and default scheme */
static inline int set_default_scheme(struct dpaa_if *dpaa_intf)
{
	ioc_fm_pcd_kg_scheme_params_t scheme_params;
	int idx = dpaa_intf->scheme_count;
	PMD_INIT_FUNC_TRACE();

	/* Set PCD NetEnvCharacteristics */
	memset(&scheme_params, 0, sizeof(scheme_params));

	/* Adding 10 to default schemes as the number of interface would be
	 * lesser than 10 and the relative scheme ids should be unique for
	 * every scheme.
	 */
	scheme_params.param.scm_id.relative_scheme_id =
		10 + dpaa_intf->ifid;
	scheme_params.param.use_hash = 0;
	scheme_params.param.next_engine = e_IOC_FM_PCD_DONE;
	scheme_params.param.net_env_params.num_of_distinction_units = 0;
	scheme_params.param.net_env_params.net_env_id =
		dpaa_intf->netenv_handle;
	scheme_params.param.base_fqid = dpaa_intf->rx_queues[0].fqid;
	scheme_params.param.key_ext_and_hash.hash_dist_num_of_fqids = 1;
	scheme_params.param.key_ext_and_hash.num_of_used_extracts = 0;
	scheme_params.param.modify = false;
	scheme_params.param.always_direct = false;
	scheme_params.param.scheme_counter.update = 1;
	scheme_params.param.scheme_counter.value = 0;

	/* FM PCD KgSchemeSet */
	dpaa_intf->scheme_handle[idx] =
		fm_pcd_kg_scheme_set(fm_info.pcd_handle, &scheme_params);
	DPAA_PMD_DEBUG("KG SCHEME SET %d handle =%p",
		idx, dpaa_intf->scheme_handle[idx]);
	if (!dpaa_intf->scheme_handle[idx]) {
		DPAA_PMD_ERR("fm_pcd_kg_scheme_set: Failed");
		return -1;
	}

	fm_model.scheme_devid[dpaa_intf->ifid][idx] =
				get_device_id(dpaa_intf->scheme_handle[idx]);
	dpaa_intf->scheme_count++;
	return 0;
}


/* Set PCD NetEnv and Scheme and default scheme */
static inline int set_pcd_netenv_scheme(struct dpaa_if *dpaa_intf,
					uint64_t req_dist_set,
					struct fman_if *fif)
{
	int ret = -1;
	ioc_fm_pcd_net_env_params_t dist_units;
	ioc_fm_pcd_kg_scheme_params_t scheme_params;
	int idx = dpaa_intf->scheme_count;
	PMD_INIT_FUNC_TRACE();

	/* Set PCD NetEnvCharacteristics */
	memset(&dist_units, 0, sizeof(dist_units));
	memset(&scheme_params, 0, sizeof(scheme_params));

	/* Set dist unit header type */
	set_dist_units(&dist_units, req_dist_set);

	scheme_params.param.scm_id.relative_scheme_id = dpaa_intf->ifid;

	/* Set PCD Scheme params */
	ret = set_scheme_params(&scheme_params, &dist_units, dpaa_intf, fif);
	if (ret) {
		DPAA_PMD_ERR("Set scheme params: Failed");
		return -1;
	}

	/* FM PCD KgSchemeSet */
	dpaa_intf->scheme_handle[idx] =
		fm_pcd_kg_scheme_set(fm_info.pcd_handle, &scheme_params);
	DPAA_PMD_DEBUG("KG SCHEME SET %d handle =%p",
			idx, dpaa_intf->scheme_handle[idx]);
	if (!dpaa_intf->scheme_handle[idx]) {
		DPAA_PMD_ERR("fm_pcd_kg_scheme_set: Failed");
		return -1;
	}

	fm_model.scheme_devid[dpaa_intf->ifid][idx] =
				get_device_id(dpaa_intf->scheme_handle[idx]);
	dpaa_intf->scheme_count++;
	return 0;
}


static inline int get_port_type(struct fman_if *fif)
{
	if (fif->mac_type == fman_mac_1g)
		return e_FM_PORT_TYPE_RX;
	else if (fif->mac_type == fman_mac_2_5g)
		return e_FM_PORT_TYPE_RX_2_5G;
	else if (fif->mac_type == fman_mac_10g)
		return e_FM_PORT_TYPE_RX_10G;

	DPAA_PMD_ERR("MAC type unsupported");
	return -1;
}

static inline int set_fm_port_handle(struct dpaa_if *dpaa_intf,
				     uint64_t req_dist_set,
				     struct fman_if *fif)
{
	t_fm_port_params	fm_port_params;
	ioc_fm_pcd_net_env_params_t dist_units;
	PMD_INIT_FUNC_TRACE();

	/* FMAN mac indexes mappings (0 is unused,
	 * first 8 are for 1G, next for 10G ports
	 */
	uint8_t mac_idx[] = {-1, 0, 1, 2, 3, 4, 5, 6, 7, 0, 1};

	/* Memset FM port params */
	memset(&fm_port_params, 0, sizeof(fm_port_params));

	/* Set FM port params */
	fm_port_params.h_fm = fm_info.fman_handle;
	fm_port_params.port_type = get_port_type(fif);
	fm_port_params.port_id = mac_idx[fif->mac_idx];

	/* FM PORT Open */
	dpaa_intf->port_handle = fm_port_open(&fm_port_params);
	if (!dpaa_intf->port_handle) {
		DPAA_PMD_ERR("fm_port_open: Failed\n");
		return -1;
	}

	fm_model.fm_port_params[dpaa_intf->ifid] = fm_port_params;

	/* Set PCD NetEnvCharacteristics */
	memset(&dist_units, 0, sizeof(dist_units));

	/* Set dist unit header type */
	set_dist_units(&dist_units, req_dist_set);

	/* FM PCD NetEnvCharacteristicsSet */
	dpaa_intf->netenv_handle =
		fm_pcd_net_env_characteristics_set(fm_info.pcd_handle,
							&dist_units);
	if (!dpaa_intf->netenv_handle) {
		DPAA_PMD_ERR("fm_pcd_net_env_characteristics_set: Failed");
		return -1;
	}

	fm_model.netenv_devid[dpaa_intf->ifid] =
				get_device_id(dpaa_intf->netenv_handle);

	return 0;
}

/* De-Configure DPAA FM */
int dpaa_fm_deconfig(struct dpaa_if *dpaa_intf,
			struct fman_if *fif __rte_unused)
{
	int ret;
	unsigned int idx;

	PMD_INIT_FUNC_TRACE();

	/* FM PORT Disable */
	ret = fm_port_disable(dpaa_intf->port_handle);
	if (ret != E_OK) {
		DPAA_PMD_ERR("fm_port_disable: Failed");
		return ret;
	}

	/* FM PORT DeletePCD */
	ret = fm_port_delete_pcd(dpaa_intf->port_handle);
	if (ret != E_OK) {
		DPAA_PMD_ERR("fm_port_delete_pcd: Failed");
		return ret;
	}

	for (idx = 0; idx < dpaa_intf->scheme_count; idx++) {
		DPAA_PMD_DEBUG("KG SCHEME DEL %d, handle =%p",
			idx, dpaa_intf->scheme_handle[idx]);
		/* FM PCD KgSchemeDelete */
		ret = fm_pcd_kg_scheme_delete(dpaa_intf->scheme_handle[idx]);
		if (ret != E_OK) {
			DPAA_PMD_ERR("fm_pcd_kg_scheme_delete: Failed");
			return ret;
		}
		dpaa_intf->scheme_handle[idx] = NULL;
	}
	/* FM PCD NetEnvCharacteristicsDelete */
	ret = fm_pcd_net_env_characteristics_delete(dpaa_intf->netenv_handle);
	if (ret != E_OK) {
		DPAA_PMD_ERR("fm_pcd_net_env_characteristics_delete: Failed");
		return ret;
	}
	dpaa_intf->netenv_handle = NULL;

	if (fif && fif->is_shared_mac) {
		ret = fm_port_enable(dpaa_intf->port_handle);
		if (ret != E_OK) {
			DPAA_PMD_ERR("shared mac re-enable failed");
			return ret;
		}
	}

	/* FM PORT Close */
	fm_port_close(dpaa_intf->port_handle);
	dpaa_intf->port_handle = NULL;

	/* Set scheme count to 0 */
	dpaa_intf->scheme_count = 0;

	return 0;
}

int dpaa_fm_config(struct rte_eth_dev *dev, uint64_t req_dist_set)
{
	struct dpaa_if *dpaa_intf = dev->data->dev_private;
	struct fman_if *fif = dev->process_private;
	int ret;
	unsigned int i = 0;
	PMD_INIT_FUNC_TRACE();

	if (dpaa_intf->port_handle) {
		if (dpaa_fm_deconfig(dpaa_intf, fif))
			DPAA_PMD_ERR("DPAA FM deconfig failed");
	}

	if (!dev->data->nb_rx_queues)
		return 0;

	if (dev->data->nb_rx_queues & (dev->data->nb_rx_queues - 1)) {
		DPAA_PMD_ERR("No of queues should be power of 2");
		return -1;
	}

	dpaa_intf->nb_rx_queues = dev->data->nb_rx_queues;

	/* Open FM Port and set it in port info */
	ret = set_fm_port_handle(dpaa_intf, req_dist_set, fif);
	if (ret) {
		DPAA_PMD_ERR("Set FM Port handle: Failed");
		return -1;
	}

	if (fif->num_profiles) {
		for (i = 0; i < dpaa_intf->nb_rx_queues; i++)
			dpaa_intf->rx_queues[i].vsp_id =
				fm_default_vsp_id(fif);

		i = 0;
	}

	/* Set PCD netenv and scheme */
	if (req_dist_set) {
		ret = set_pcd_netenv_scheme(dpaa_intf, req_dist_set, fif);
		if (ret) {
			DPAA_PMD_ERR("Set PCD NetEnv and Scheme dist: Failed");
			goto unset_fm_port_handle;
		}
	}
	/* Set default netenv and scheme */
	if (!fif->is_shared_mac) {
		ret = set_default_scheme(dpaa_intf);
		if (ret) {
			DPAA_PMD_ERR("Set PCD NetEnv and Scheme: Failed");
			goto unset_pcd_netenv_scheme1;
		}
	}

	/* Set Port PCD */
	ret = set_port_pcd(dpaa_intf);
	if (ret) {
		DPAA_PMD_ERR("Set Port PCD: Failed");
		goto unset_pcd_netenv_scheme;
	}

	for (; i < fm_model.dev_count; i++)
		if (fm_model.device_order[i] == dpaa_intf->ifid)
			return 0;

	fm_model.device_order[fm_model.dev_count] = dpaa_intf->ifid;
	fm_model.dev_count++;

	return 0;

unset_pcd_netenv_scheme:
	unset_pcd_netenv_scheme(dpaa_intf);

unset_pcd_netenv_scheme1:
	unset_pcd_netenv_scheme(dpaa_intf);

unset_fm_port_handle:
	/* FM PORT Close */
	fm_port_close(dpaa_intf->port_handle);
	dpaa_intf->port_handle = NULL;
	return -1;
}

int dpaa_fm_init(void)
{
	t_handle fman_handle;
	t_handle pcd_handle;
	t_fm_pcd_params fm_pcd_params = {0};
	/* Hard-coded : fman id 0 since one fman is present in LS104x */
	int fman_id = 0, ret;
	PMD_INIT_FUNC_TRACE();

	dpaa_read_fm_config_from_file();

	/* FM Open */
	fman_handle = fm_open(fman_id);
	if (!fman_handle) {
		DPAA_PMD_ERR("fm_open: Failed");
		return -1;
	}

	/* FM PCD Open */
	fm_pcd_params.h_fm = fman_handle;
	fm_pcd_params.prs_support = true;
	fm_pcd_params.kg_support = true;
	pcd_handle = fm_pcd_open(&fm_pcd_params);
	if (!pcd_handle) {
		fm_close(fman_handle);
		DPAA_PMD_ERR("fm_pcd_open: Failed");
		return -1;
	}

	/* FM PCD Enable */
	ret = fm_pcd_enable(pcd_handle);
	if (ret) {
		fm_close(fman_handle);
		fm_pcd_close(pcd_handle);
		DPAA_PMD_ERR("fm_pcd_enable: Failed");
		return -1;
	}

	/* Set fman and pcd handle in fm info */
	fm_info.fman_handle = fman_handle;
	fm_info.pcd_handle = pcd_handle;

	return 0;
}


/* De-initialization of FM */
int dpaa_fm_term(void)
{
	int ret;

	PMD_INIT_FUNC_TRACE();

	if (fm_info.pcd_handle && fm_info.fman_handle) {
		/* FM PCD Disable */
		ret = fm_pcd_disable(fm_info.pcd_handle);
		if (ret) {
			DPAA_PMD_ERR("fm_pcd_disable: Failed");
			return -1;
		}

		/* FM PCD Close */
		fm_pcd_close(fm_info.pcd_handle);
		fm_info.pcd_handle = NULL;
	}

	if (fm_info.fman_handle) {
		/* FM Close */
		fm_close(fm_info.fman_handle);
		fm_info.fman_handle = NULL;
	}

	if (access(fm_log, F_OK) != -1) {
		ret = remove(fm_log);
		if (ret)
			DPAA_PMD_ERR("File remove: Failed");
	}
	return 0;
}

static int dpaa_port_vsp_configure(struct dpaa_if *dpaa_intf,
		uint8_t vsp_id, t_handle fman_handle,
		struct fman_if *fif, u32 mbuf_data_room_size)
{
	t_fm_vsp_params vsp_params;
	t_fm_buffer_prefix_content buf_prefix_cont;
	uint8_t mac_idx[] = {-1, 0, 1, 2, 3, 4, 5, 6, 7, 0, 1};
	uint8_t idx = mac_idx[fif->mac_idx];
	int ret;

	if (vsp_id == fif->base_profile_id && fif->is_shared_mac) {
		/* For shared interface, VSP of base
		 * profile is default pool located in kernel.
		 */
		dpaa_intf->vsp_bpid[vsp_id] = 0;
		return 0;
	}

	if (vsp_id >= DPAA_VSP_PROFILE_MAX_NUM) {
		DPAA_PMD_ERR("VSP ID %d exceeds MAX number %d",
			vsp_id, DPAA_VSP_PROFILE_MAX_NUM);
		return -1;
	}

	memset(&vsp_params, 0, sizeof(vsp_params));
	vsp_params.h_fm = fman_handle;
	vsp_params.relative_profile_id = vsp_id;
	vsp_params.port_params.port_id = idx;
	if (fif->mac_type == fman_mac_1g) {
		vsp_params.port_params.port_type = e_FM_PORT_TYPE_RX;
	} else if (fif->mac_type == fman_mac_2_5g) {
		vsp_params.port_params.port_type = e_FM_PORT_TYPE_RX_2_5G;
	} else if (fif->mac_type == fman_mac_10g) {
		vsp_params.port_params.port_type = e_FM_PORT_TYPE_RX_10G;
	} else {
		DPAA_PMD_ERR("Mac type %d error", fif->mac_type);
		return -1;
	}
	vsp_params.ext_buf_pools.num_of_pools_used = 1;
	vsp_params.ext_buf_pools.ext_buf_pool[0].id = dpaa_intf->vsp_bpid[vsp_id];
	vsp_params.ext_buf_pools.ext_buf_pool[0].size = mbuf_data_room_size;

	dpaa_intf->vsp_handle[vsp_id] = fm_vsp_config(&vsp_params);
	if (!dpaa_intf->vsp_handle[vsp_id]) {
		DPAA_PMD_ERR("fm_vsp_config error for profile %d", vsp_id);
		return -EINVAL;
	}

	/* configure the application buffer (structure, size and
	 * content)
	 */

	memset(&buf_prefix_cont, 0, sizeof(buf_prefix_cont));

	buf_prefix_cont.priv_data_size = 16;
	buf_prefix_cont.data_align = 64;
	buf_prefix_cont.pass_prs_result = true;
	buf_prefix_cont.pass_time_stamp = true;
	buf_prefix_cont.pass_hash_result = false;
	buf_prefix_cont.pass_all_other_pcdinfo = false;
	buf_prefix_cont.manip_ext_space =
		RTE_PKTMBUF_HEADROOM - DPAA_MBUF_HW_ANNOTATION;

	ret = fm_vsp_config_buffer_prefix_content(dpaa_intf->vsp_handle[vsp_id],
					       &buf_prefix_cont);
	if (ret != E_OK) {
		DPAA_PMD_ERR("fm_vsp_config_buffer_prefix_content error for profile %d err: %d",
			     vsp_id, ret);
		return ret;
	}

	/* initialize the FM VSP module */
	ret = fm_vsp_init(dpaa_intf->vsp_handle[vsp_id]);
	if (ret != E_OK) {
		DPAA_PMD_ERR("fm_vsp_init error for profile %d err:%d",
			 vsp_id, ret);
		return ret;
	}

	return 0;
}

int dpaa_port_vsp_update(struct dpaa_if *dpaa_intf,
		bool fmc_mode, uint8_t vsp_id, uint32_t bpid,
		struct fman_if *fif, u32 mbuf_data_room_size)
{
	int ret = 0;
	t_handle fman_handle;

	if (!fif->num_profiles)
		return 0;

	if (vsp_id >= fif->num_profiles)
		return 0;

	if (dpaa_intf->vsp_bpid[vsp_id] == bpid)
		return 0;

	if (dpaa_intf->vsp_handle[vsp_id]) {
		ret = fm_vsp_free(dpaa_intf->vsp_handle[vsp_id]);
		if (ret != E_OK) {
			DPAA_PMD_ERR("Error fm_vsp_free: err %d vsp_handle[%d]",
				     ret, vsp_id);
			return ret;
		}
		dpaa_intf->vsp_handle[vsp_id] = 0;
	}

	if (fmc_mode)
		fman_handle = fm_open(0);
	else
		fman_handle = fm_info.fman_handle;

	dpaa_intf->vsp_bpid[vsp_id] = bpid;

	return dpaa_port_vsp_configure(dpaa_intf, vsp_id, fman_handle, fif,
				       mbuf_data_room_size);
}

int dpaa_port_vsp_cleanup(struct dpaa_if *dpaa_intf, struct fman_if *fif)
{
	int idx, ret;

	for (idx = 0; idx < (uint8_t)fif->num_profiles; idx++) {
		if (dpaa_intf->vsp_handle[idx]) {
			ret = fm_vsp_free(dpaa_intf->vsp_handle[idx]);
			if (ret != E_OK) {
				DPAA_PMD_ERR("Error fm_vsp_free: err %d"
					     " vsp_handle[%d]", ret, idx);
				return ret;
			}
		}
	}

	return E_OK;
}
