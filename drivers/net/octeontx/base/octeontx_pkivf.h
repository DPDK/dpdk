/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef	__OCTEONTX_PKI_H__
#define	__OCTEONTX_PKI_H__

#include <stdint.h>

#include <octeontx_mbox.h>

#define OCTEONTX_PKI_COPROC                     5

/* PKI messages */

#define MBOX_PKI_PORT_OPEN			1
#define MBOX_PKI_PORT_START			2
#define MBOX_PKI_PORT_STOP			3
#define MBOX_PKI_PORT_CLOSE			4
#define MBOX_PKI_PORT_CONFIG			5
#define MBOX_PKI_PORT_OPT_PARSER_CONFIG		6
#define MBOX_PKI_PORT_CUSTOM_PARSER_CONFIG	7
#define MBOX_PKI_PORT_PKTBUF_CONFIG		8
#define MBOX_PKI_PORT_HASH_CONFIG		9
#define MBOX_PKI_PORT_ERRCHK_CONFIG		10
#define MBOX_PKI_PORT_CREATE_QOS		11
#define MBOX_PKI_PORT_MODIFY_QOS		12
#define MBOX_PKI_PORT_DELETE_QOS		13
#define MBOX_PKI_PORT_PKTDROP_CONFIG		14
#define MBOX_PKI_PORT_WQE_GEN_CONFIG		15
#define MBOX_PKI_BACKPRESSURE_CONFIG		16
#define MBOX_PKI_PORT_GET_STATS			17
#define MBOX_PKI_PORT_RESET_STATS		18
#define MBOX_PKI_GET_PORT_CONFIG		19
#define MBOX_PKI_GET_PORT_QOS_CONFIG		20

#define MBOX_PKI_MAX_QOS_ENTRY 64

/* pki pkind parse mode */
enum  {
	MBOX_PKI_PARSE_LA_TO_LG = 0,
	MBOX_PKI_PARSE_LB_TO_LG = 1,
	MBOX_PKI_PARSE_LC_TO_LG = 3,
	MBOX_PKI_PARSE_LG = 0x3f,
	MBOX_PKI_PARSE_NOTHING = 0x7f
};

/* Interface types: */
enum {
	OCTTX_PORT_TYPE_NET, /* Network interface ports */
	OCTTX_PORT_TYPE_INT, /* CPU internal interface ports */
	OCTTX_PORT_TYPE_PCI, /* DPI/PCIe interface ports */
	OCTTX_PORT_TYPE_MAX
};

/* pki port config */
typedef struct mbox_pki_port_type {
	uint8_t port_type;
} mbox_pki_port_t;

/* pki port config */
typedef struct mbox_pki_port_cfg {
	uint8_t port_type;
	struct {
		uint8_t fcs_pres:1;
		uint8_t fcs_skip:1;
		uint8_t parse_mode:1;
		uint8_t mpls_parse:1;
		uint8_t inst_hdr_parse:1;
		uint8_t fulc_parse:1;
		uint8_t dsa_parse:1;
		uint8_t hg2_parse:1;
		uint8_t hg_parse:1;
	} mmask;
	uint8_t fcs_pres;
	uint8_t fcs_skip;
	uint8_t parse_mode;
	uint8_t mpls_parse;
	uint8_t inst_hdr_parse;
	uint8_t fulc_parse;
	uint8_t dsa_parse;
	uint8_t hg2_parse;
	uint8_t hg_parse;
} mbox_pki_prt_cfg_t;

/* pki Flow/style packet buffer config */
typedef struct mbox_pki_port_pktbuf_cfg {
	uint8_t port_type;
	struct {
		uint16_t f_mbuff_size:1;
		uint16_t f_wqe_skip:1;
		uint16_t f_first_skip:1;
		uint16_t f_later_skip:1;
		uint16_t f_pkt_outside_wqe:1;
		uint16_t f_wqe_endian:1;
		uint16_t f_cache_mode:1;
	} mmask;
	uint16_t mbuff_size;
	uint16_t wqe_skip;
	uint16_t first_skip;
	uint16_t later_skip;
	uint8_t pkt_outside_wqe;
	uint8_t wqe_endian;
	uint8_t cache_mode;
} mbox_pki_pktbuf_cfg_t;

/* pki flow/style tag config */
typedef struct mbox_pki_port_hash_cfg {
	uint8_t port_type;
	uint32_t tag_slf:1;
	uint32_t tag_sle:1;
	uint32_t tag_sld:1;
	uint32_t tag_slc:1;
	uint32_t tag_dlf:1;
	uint32_t tag_dle:1;
	uint32_t tag_dld:1;
	uint32_t tag_dlc:1;
	uint32_t tag_prt:1;
	uint32_t tag_vlan0:1;
	uint32_t tag_vlan1:1;
	uint32_t tag_ip_pctl:1;
	uint32_t tag_sync:1;
	uint32_t tag_spi:1;
	uint32_t tag_gtp:1;
	uint32_t tag_vni:1;
} mbox_pki_hash_cfg_t;

/* pki flow/style errcheck config */
typedef struct mbox_pki_port_errcheck_cfg {
	uint8_t port_type;
	struct {
		uint32_t f_ip6_udp_opt:1;
		uint32_t f_lenerr_en:1;
		uint32_t f_maxerr_en:1;
		uint32_t f_minerr_en:1;
		uint32_t f_fcs_chk:1;
		uint32_t f_fcs_strip:1;
		uint32_t f_len_lf:1;
		uint32_t f_len_le:1;
		uint32_t f_len_ld:1;
		uint32_t f_len_lc:1;
		uint32_t f_csum_lf:1;
		uint32_t f_csum_le:1;
		uint32_t f_csum_ld:1;
		uint32_t f_csum_lc:1;
		uint32_t f_min_frame_len;
		uint32_t f_max_frame_len;
	} mmask;
	uint64_t ip6_udp_opt:1;
	uint64_t lenerr_en:1;
	uint64_t maxerr_en:1;
	uint64_t minerr_en:1;
	uint64_t fcs_chk:1;
	uint64_t fcs_strip:1;
	uint64_t len_lf:1;
	uint64_t len_le:1;
	uint64_t len_ld:1;
	uint64_t len_lc:1;
	uint64_t csum_lf:1;
	uint64_t csum_le:1;
	uint64_t csum_ld:1;
	uint64_t csum_lc:1;
	uint64_t min_frame_len;
	uint64_t max_frame_len;
} mbox_pki_errcheck_cfg_t;

/* CACHE MODE*/
enum {
	MBOX_PKI_OPC_MODE_STT = 0LL,
	MBOX_PKI_OPC_MODE_STF = 1LL,
	MBOX_PKI_OPC_MODE_STF1_STT = 2LL,
	MBOX_PKI_OPC_MODE_STF2_STT = 3LL
};

/* PKI QPG QOS*/
enum {
	MBOX_PKI_QPG_QOS_NONE = 0,
	MBOX_PKI_QPG_QOS_VLAN,
	MBOX_PKI_QPG_QOS_MPLS,
	MBOX_PKI_QPG_QOS_DSA_SRC,
	MBOX_PKI_QPG_QOS_DIFFSERV,
	MBOX_PKI_QPG_QOS_HIGIG,
};

struct mbox_pki_qos_entry {
	uint16_t port_add;
	uint16_t ggrp_ok;
	uint16_t ggrp_bad;
	uint16_t gaura;
	uint8_t grptag_ok;
	uint8_t grptag_bad;
};

/* pki flow/style enable qos */
typedef struct mbox_pki_port_create_qos {
	uint8_t port_type;
	uint8_t qpg_qos;
	uint8_t num_entry;
	uint8_t tag_type;
	uint8_t drop_policy;
	struct mbox_pki_qos_entry qos_entry[MBOX_PKI_MAX_QOS_ENTRY];
} mbox_pki_qos_cfg_t;

/* pki flow/style enable qos */
typedef struct mbox_pki_port_modify_qos_entry {
	uint8_t port_type;
	uint16_t index;
	struct {
		uint8_t f_port_add:1;
		uint8_t f_grp_ok:1;
		uint8_t f_grp_bad:1;
		uint8_t f_gaura:1;
		uint8_t f_grptag_ok:1;
		uint8_t f_grptag_bad:1;
		uint8_t f_tag_type:1;
	} mmask;
	uint8_t tag_type;
	struct mbox_pki_qos_entry qos_entry;
} mbox_pki_mod_qos_t;

/* pki flow/style enable qos */
typedef struct mbox_pki_port_delete_qos_entry {
	uint8_t port_type;
	uint16_t index;
} mbox_pki_del_qos_t;

/* PKI maximum constants */
#define PKI_VF_MAX			(1)
#define PKI_MAX_PKTLEN			(32768)

/* pki pkind parse mode */
enum  {
	PKI_PARSE_LA_TO_LG = 0,
	PKI_PARSE_LB_TO_LG = 1,
	PKI_PARSE_LC_TO_LG = 3,
	PKI_PARSE_LG = 0x3f,
	PKI_PARSE_NOTHING = 0x7f
};

/* pki port config */
typedef struct pki_port_cfg {
	uint8_t port_type;
	struct {
		uint8_t fcs_pres:1;
		uint8_t fcs_skip:1;
		uint8_t parse_mode:1;
		uint8_t mpls_parse:1;
		uint8_t inst_hdr_parse:1;
		uint8_t fulc_parse:1;
		uint8_t dsa_parse:1;
		uint8_t hg2_parse:1;
		uint8_t hg_parse:1;
	} mmask;
	uint8_t fcs_pres;
	uint8_t fcs_skip;
	uint8_t parse_mode;
	uint8_t mpls_parse;
	uint8_t inst_hdr_parse;
	uint8_t fulc_parse;
	uint8_t dsa_parse;
	uint8_t hg2_parse;
	uint8_t hg_parse;
} pki_prt_cfg_t;


/* pki Flow/style packet buffer config */
typedef struct pki_port_pktbuf_cfg {
	uint8_t port_type;
	struct {
		uint16_t f_mbuff_size:1;
		uint16_t f_wqe_skip:1;
		uint16_t f_first_skip:1;
		uint16_t f_later_skip:1;
		uint16_t f_pkt_outside_wqe:1;
		uint16_t f_wqe_endian:1;
		uint16_t f_cache_mode:1;
	} mmask;
	uint16_t mbuff_size;
	uint16_t wqe_skip;
	uint16_t first_skip;
	uint16_t later_skip;
	uint8_t pkt_outside_wqe;
	uint8_t wqe_endian;
	uint8_t cache_mode;
} pki_pktbuf_cfg_t;

/* pki flow/style tag config */
typedef struct pki_port_hash_cfg {
	uint8_t port_type;
	uint32_t tag_slf:1;
	uint32_t tag_sle:1;
	uint32_t tag_sld:1;
	uint32_t tag_slc:1;
	uint32_t tag_dlf:1;
	uint32_t tag_dle:1;
	uint32_t tag_dld:1;
	uint32_t tag_dlc:1;
	uint32_t tag_prt:1;
	uint32_t tag_vlan0:1;
	uint32_t tag_vlan1:1;
	uint32_t tag_ip_pctl:1;
	uint32_t tag_sync:1;
	uint32_t tag_spi:1;
	uint32_t tag_gtp:1;
	uint32_t tag_vni:1;
} pki_hash_cfg_t;

/* pki flow/style errcheck config */
typedef struct pki_port_errcheck_cfg {
	uint8_t port_type;
	struct {
		uint32_t f_ip6_udp_opt:1;
		uint32_t f_lenerr_en:1;
		uint32_t f_maxerr_en:1;
		uint32_t f_minerr_en:1;
		uint32_t f_fcs_chk:1;
		uint32_t f_fcs_strip:1;
		uint32_t f_len_lf:1;
		uint32_t f_len_le:1;
		uint32_t f_len_ld:1;
		uint32_t f_len_lc:1;
		uint32_t f_csum_lf:1;
		uint32_t f_csum_le:1;
		uint32_t f_csum_ld:1;
		uint32_t f_csum_lc:1;
		uint32_t f_min_frame_len;
		uint32_t f_max_frame_len;
	} mmask;
	uint64_t ip6_udp_opt:1;
	uint64_t lenerr_en:1;
	uint64_t maxerr_en:1;
	uint64_t minerr_en:1;
	uint64_t fcs_chk:1;
	uint64_t fcs_strip:1;
	uint64_t len_lf:1;
	uint64_t len_le:1;
	uint64_t len_ld:1;
	uint64_t len_lc:1;
	uint64_t csum_lf:1;
	uint64_t csum_le:1;
	uint64_t csum_ld:1;
	uint64_t csum_lc:1;
	uint64_t min_frame_len;
	uint64_t max_frame_len;
} pki_errchk_cfg_t;


/* CACHE MODE*/
enum {
	PKI_OPC_MODE_STT = 0LL,
	PKI_OPC_MODE_STF = 1LL,
	PKI_OPC_MODE_STF1_STT = 2LL,
	PKI_OPC_MODE_STF2_STT = 3LL
};

/* PKI QPG QOS*/
enum {
	PKI_QPG_QOS_NONE = 0,
	PKI_QPG_QOS_VLAN,
	PKI_QPG_QOS_MPLS,
	PKI_QPG_QOS_DSA_SRC,
	PKI_QPG_QOS_DIFFSERV,
	PKI_QPG_QOS_HIGIG,
};

struct pki_qos_entry {
	uint16_t port_add;
	uint16_t ggrp_ok;
	uint16_t ggrp_bad;
	uint16_t gaura;
	uint8_t grptag_ok;
	uint8_t grptag_bad;
	uint8_t ena_red;
	uint8_t ena_drop;
};

#define PKO_MAX_QOS_ENTRY 64

/* pki flow/style enable qos */
typedef struct pki_port_create_qos {
	uint8_t port_type;
	uint8_t qpg_qos;
	uint8_t num_entry;
	uint8_t tag_type;
	uint8_t drop_policy;
	struct pki_qos_entry qos_entry[PKO_MAX_QOS_ENTRY];
} pki_qos_cfg_t;

/* pki flow/style enable qos */
typedef struct pki_port_delete_qos_entry {
	uint8_t port_type;
	uint16_t index;
} pki_del_qos_t;

/* pki flow/style enable qos */
typedef struct pki_port_modify_qos_entry {
	uint8_t port_type;
	uint16_t index;
	struct {
		uint8_t f_port_add:1;
		uint8_t f_grp_ok:1;
		uint8_t f_grp_bad:1;
		uint8_t f_gaura:1;
		uint8_t f_grptag_ok:1;
		uint8_t f_grptag_bad:1;
		uint8_t f_tag_type:1;
	} mmask;
	uint8_t tag_type;
	struct pki_qos_entry qos_entry;
} pki_mod_qos_t;

static inline int
octeontx_pki_port_modify_qos(int port, pki_mod_qos_t *qos_cfg)
{
	struct octeontx_mbox_hdr hdr;
	int res;

	mbox_pki_mod_qos_t q_cfg = *(mbox_pki_mod_qos_t *)qos_cfg;
	int len = sizeof(mbox_pki_mod_qos_t);

	hdr.coproc = OCTEONTX_PKI_COPROC;
	hdr.msg = MBOX_PKI_PORT_MODIFY_QOS;
	hdr.vfid = port;

	res = octeontx_mbox_send(&hdr, &q_cfg, len, NULL, 0);
	if (res < 0)
		return -EACCES;

	return res;
}

static inline int
octeontx_pki_port_delete_qos(int port, pki_del_qos_t *qos_cfg)
{
	struct octeontx_mbox_hdr hdr;
	int res;

	mbox_pki_del_qos_t q_cfg = *(mbox_pki_del_qos_t *)qos_cfg;
	int len = sizeof(mbox_pki_del_qos_t);

	hdr.coproc = OCTEONTX_PKI_COPROC;
	hdr.msg = MBOX_PKI_PORT_DELETE_QOS;
	hdr.vfid = port;

	res = octeontx_mbox_send(&hdr, &q_cfg, len, NULL, 0);
	if (res < 0)
		return -EACCES;

	return res;
}

static inline int
octeontx_pki_port_close(int port)
{
	struct octeontx_mbox_hdr hdr;
	int res;

	mbox_pki_port_t ptype;
	int len = sizeof(mbox_pki_port_t);
	memset(&ptype, 0, len);
	ptype.port_type = OCTTX_PORT_TYPE_NET;

	hdr.coproc = OCTEONTX_PKI_COPROC;
	hdr.msg = MBOX_PKI_PORT_CLOSE;
	hdr.vfid = port;

	res = octeontx_mbox_send(&hdr, &ptype, len, NULL, 0);
	if (res < 0)
		return -EACCES;

	return res;
}

static inline int
octeontx_pki_port_start(int port)
{
	struct octeontx_mbox_hdr hdr;
	int res;

	mbox_pki_port_t ptype;
	int len = sizeof(mbox_pki_port_t);
	memset(&ptype, 0, len);
	ptype.port_type = OCTTX_PORT_TYPE_NET;

	hdr.coproc = OCTEONTX_PKI_COPROC;
	hdr.msg = MBOX_PKI_PORT_START;
	hdr.vfid = port;

	res = octeontx_mbox_send(&hdr, &ptype, len, NULL, 0);
	if (res < 0)
		return -EACCES;

	return res;
}

static inline int
octeontx_pki_port_stop(int port)
{
	struct octeontx_mbox_hdr hdr;
	int res;

	mbox_pki_port_t ptype;
	int len = sizeof(mbox_pki_port_t);
	memset(&ptype, 0, len);
	ptype.port_type = OCTTX_PORT_TYPE_NET;

	hdr.coproc = OCTEONTX_PKI_COPROC;
	hdr.msg = MBOX_PKI_PORT_STOP;
	hdr.vfid = port;

	res = octeontx_mbox_send(&hdr, &ptype, len, NULL, 0);
	if (res < 0)
		return -EACCES;

	return res;
}

int octeontx_pki_port_open(int port);
int octeontx_pki_port_hash_config(int port, pki_hash_cfg_t *hash_cfg);
int octeontx_pki_port_pktbuf_config(int port, pki_pktbuf_cfg_t *buf_cfg);
int octeontx_pki_port_create_qos(int port, pki_qos_cfg_t *qos_cfg);
int octeontx_pki_port_close(int port);
int octeontx_pki_port_errchk_config(int port, pki_errchk_cfg_t *cfg);

#endif /* __OCTEONTX_PKI_H__ */
