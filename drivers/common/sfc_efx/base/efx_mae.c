/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Xilinx, Inc. All rights reserved.
 * All rights reserved.
 */

#include "efx.h"
#include "efx_impl.h"


#if EFSYS_OPT_MAE

static	__checkReturn			efx_rc_t
efx_mae_get_capabilities(
	__in				efx_nic_t *enp)
{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload,
	    MC_CMD_MAE_GET_CAPS_IN_LEN,
	    MC_CMD_MAE_GET_CAPS_OUT_LEN);
	struct efx_mae_s *maep = enp->en_maep;
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_MAE_GET_CAPS;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_MAE_GET_CAPS_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_MAE_GET_CAPS_OUT_LEN;

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_MAE_GET_CAPS_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	maep->em_max_n_outer_prios =
	    MCDI_OUT_DWORD(req, MAE_GET_CAPS_OUT_OUTER_PRIOS);

	maep->em_max_n_action_prios =
	    MCDI_OUT_DWORD(req, MAE_GET_CAPS_OUT_ACTION_PRIOS);

	maep->em_encap_types_supported = 0;

	if (MCDI_OUT_DWORD_FIELD(req, MAE_GET_CAPS_OUT_ENCAP_TYPES_SUPPORTED,
	    MAE_GET_CAPS_OUT_ENCAP_TYPE_VXLAN) != 0) {
		maep->em_encap_types_supported |=
		    (1U << EFX_TUNNEL_PROTOCOL_VXLAN);
	}

	if (MCDI_OUT_DWORD_FIELD(req, MAE_GET_CAPS_OUT_ENCAP_TYPES_SUPPORTED,
	    MAE_GET_CAPS_OUT_ENCAP_TYPE_GENEVE) != 0) {
		maep->em_encap_types_supported |=
		    (1U << EFX_TUNNEL_PROTOCOL_GENEVE);
	}

	if (MCDI_OUT_DWORD_FIELD(req, MAE_GET_CAPS_OUT_ENCAP_TYPES_SUPPORTED,
	    MAE_GET_CAPS_OUT_ENCAP_TYPE_NVGRE) != 0) {
		maep->em_encap_types_supported |=
		    (1U << EFX_TUNNEL_PROTOCOL_NVGRE);
	}

	maep->em_max_nfields =
	    MCDI_OUT_DWORD(req, MAE_GET_CAPS_OUT_MATCH_FIELD_COUNT);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

static	__checkReturn			efx_rc_t
efx_mae_get_outer_rule_caps(
	__in				efx_nic_t *enp,
	__in				unsigned int field_ncaps,
	__out_ecount(field_ncaps)	efx_mae_field_cap_t *field_caps)
{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload,
	    MC_CMD_MAE_GET_OR_CAPS_IN_LEN,
	    MC_CMD_MAE_GET_OR_CAPS_OUT_LENMAX_MCDI2);
	unsigned int mcdi_field_ncaps;
	unsigned int i;
	efx_rc_t rc;

	if (MC_CMD_MAE_GET_OR_CAPS_OUT_LEN(field_ncaps) >
	    MC_CMD_MAE_GET_OR_CAPS_OUT_LENMAX_MCDI2) {
		rc = EINVAL;
		goto fail1;
	}

	req.emr_cmd = MC_CMD_MAE_GET_OR_CAPS;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_MAE_GET_OR_CAPS_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_MAE_GET_OR_CAPS_OUT_LEN(field_ncaps);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail2;
	}

	if (req.emr_out_length_used < MC_CMD_MAE_GET_OR_CAPS_OUT_LENMIN) {
		rc = EMSGSIZE;
		goto fail3;
	}

	mcdi_field_ncaps = MCDI_OUT_DWORD(req, MAE_GET_OR_CAPS_OUT_COUNT);

	if (req.emr_out_length_used <
	    MC_CMD_MAE_GET_OR_CAPS_OUT_LEN(mcdi_field_ncaps)) {
		rc = EMSGSIZE;
		goto fail4;
	}

	if (mcdi_field_ncaps > field_ncaps) {
		rc = EMSGSIZE;
		goto fail5;
	}

	for (i = 0; i < mcdi_field_ncaps; ++i) {
		uint32_t match_flag;
		uint32_t mask_flag;

		field_caps[i].emfc_support = MCDI_OUT_INDEXED_DWORD_FIELD(req,
		    MAE_GET_OR_CAPS_OUT_FIELD_FLAGS, i,
		    MAE_FIELD_FLAGS_SUPPORT_STATUS);

		match_flag = MCDI_OUT_INDEXED_DWORD_FIELD(req,
		    MAE_GET_OR_CAPS_OUT_FIELD_FLAGS, i,
		    MAE_FIELD_FLAGS_MATCH_AFFECTS_CLASS);

		field_caps[i].emfc_match_affects_class =
		    (match_flag != 0) ? B_TRUE : B_FALSE;

		mask_flag = MCDI_OUT_INDEXED_DWORD_FIELD(req,
		    MAE_GET_OR_CAPS_OUT_FIELD_FLAGS, i,
		    MAE_FIELD_FLAGS_MASK_AFFECTS_CLASS);

		field_caps[i].emfc_mask_affects_class =
		    (mask_flag != 0) ? B_TRUE : B_FALSE;
	}

	return (0);

fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

static	__checkReturn			efx_rc_t
efx_mae_get_action_rule_caps(
	__in				efx_nic_t *enp,
	__in				unsigned int field_ncaps,
	__out_ecount(field_ncaps)	efx_mae_field_cap_t *field_caps)
{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload,
	    MC_CMD_MAE_GET_AR_CAPS_IN_LEN,
	    MC_CMD_MAE_GET_AR_CAPS_OUT_LENMAX_MCDI2);
	unsigned int mcdi_field_ncaps;
	unsigned int i;
	efx_rc_t rc;

	if (MC_CMD_MAE_GET_AR_CAPS_OUT_LEN(field_ncaps) >
	    MC_CMD_MAE_GET_AR_CAPS_OUT_LENMAX_MCDI2) {
		rc = EINVAL;
		goto fail1;
	}

	req.emr_cmd = MC_CMD_MAE_GET_AR_CAPS;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_MAE_GET_AR_CAPS_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_MAE_GET_AR_CAPS_OUT_LEN(field_ncaps);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail2;
	}

	if (req.emr_out_length_used < MC_CMD_MAE_GET_AR_CAPS_OUT_LENMIN) {
		rc = EMSGSIZE;
		goto fail3;
	}

	mcdi_field_ncaps = MCDI_OUT_DWORD(req, MAE_GET_AR_CAPS_OUT_COUNT);

	if (req.emr_out_length_used <
	    MC_CMD_MAE_GET_AR_CAPS_OUT_LEN(mcdi_field_ncaps)) {
		rc = EMSGSIZE;
		goto fail4;
	}

	if (mcdi_field_ncaps > field_ncaps) {
		rc = EMSGSIZE;
		goto fail5;
	}

	for (i = 0; i < mcdi_field_ncaps; ++i) {
		uint32_t match_flag;
		uint32_t mask_flag;

		field_caps[i].emfc_support = MCDI_OUT_INDEXED_DWORD_FIELD(req,
		    MAE_GET_AR_CAPS_OUT_FIELD_FLAGS, i,
		    MAE_FIELD_FLAGS_SUPPORT_STATUS);

		match_flag = MCDI_OUT_INDEXED_DWORD_FIELD(req,
		    MAE_GET_AR_CAPS_OUT_FIELD_FLAGS, i,
		    MAE_FIELD_FLAGS_MATCH_AFFECTS_CLASS);

		field_caps[i].emfc_match_affects_class =
		    (match_flag != 0) ? B_TRUE : B_FALSE;

		mask_flag = MCDI_OUT_INDEXED_DWORD_FIELD(req,
		    MAE_GET_AR_CAPS_OUT_FIELD_FLAGS, i,
		    MAE_FIELD_FLAGS_MASK_AFFECTS_CLASS);

		field_caps[i].emfc_mask_affects_class =
		    (mask_flag != 0) ? B_TRUE : B_FALSE;
	}

	return (0);

fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn			efx_rc_t
efx_mae_init(
	__in				efx_nic_t *enp)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(enp);
	efx_mae_field_cap_t *or_fcaps;
	size_t or_fcaps_size;
	efx_mae_field_cap_t *ar_fcaps;
	size_t ar_fcaps_size;
	efx_mae_t *maep;
	efx_rc_t rc;

	if (encp->enc_mae_supported == B_FALSE) {
		rc = ENOTSUP;
		goto fail1;
	}

	EFSYS_KMEM_ALLOC(enp->en_esip, sizeof (*maep), maep);
	if (maep == NULL) {
		rc = ENOMEM;
		goto fail2;
	}

	enp->en_maep = maep;

	rc = efx_mae_get_capabilities(enp);
	if (rc != 0)
		goto fail3;

	or_fcaps_size = maep->em_max_nfields * sizeof (*or_fcaps);
	EFSYS_KMEM_ALLOC(enp->en_esip, or_fcaps_size, or_fcaps);
	if (or_fcaps == NULL) {
		rc = ENOMEM;
		goto fail4;
	}

	maep->em_outer_rule_field_caps_size = or_fcaps_size;
	maep->em_outer_rule_field_caps = or_fcaps;

	rc = efx_mae_get_outer_rule_caps(enp, maep->em_max_nfields, or_fcaps);
	if (rc != 0)
		goto fail5;

	ar_fcaps_size = maep->em_max_nfields * sizeof (*ar_fcaps);
	EFSYS_KMEM_ALLOC(enp->en_esip, ar_fcaps_size, ar_fcaps);
	if (ar_fcaps == NULL) {
		rc = ENOMEM;
		goto fail6;
	}

	maep->em_action_rule_field_caps_size = ar_fcaps_size;
	maep->em_action_rule_field_caps = ar_fcaps;

	rc = efx_mae_get_action_rule_caps(enp, maep->em_max_nfields, ar_fcaps);
	if (rc != 0)
		goto fail7;

	return (0);

fail7:
	EFSYS_PROBE(fail5);
	EFSYS_KMEM_FREE(enp->en_esip, ar_fcaps_size, ar_fcaps);
fail6:
	EFSYS_PROBE(fail4);
fail5:
	EFSYS_PROBE(fail5);
	EFSYS_KMEM_FREE(enp->en_esip, or_fcaps_size, or_fcaps);
fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
	EFSYS_KMEM_FREE(enp->en_esip, sizeof (struct efx_mae_s), enp->en_maep);
	enp->en_maep = NULL;
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

					void
efx_mae_fini(
	__in				efx_nic_t *enp)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(enp);
	efx_mae_t *maep = enp->en_maep;

	if (encp->enc_mae_supported == B_FALSE)
		return;

	EFSYS_KMEM_FREE(enp->en_esip, maep->em_action_rule_field_caps_size,
	    maep->em_action_rule_field_caps);
	EFSYS_KMEM_FREE(enp->en_esip, maep->em_outer_rule_field_caps_size,
	    maep->em_outer_rule_field_caps);
	EFSYS_KMEM_FREE(enp->en_esip, sizeof (*maep), maep);
	enp->en_maep = NULL;
}

	__checkReturn			efx_rc_t
efx_mae_get_limits(
	__in				efx_nic_t *enp,
	__out				efx_mae_limits_t *emlp)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(enp);
	struct efx_mae_s *maep = enp->en_maep;
	efx_rc_t rc;

	if (encp->enc_mae_supported == B_FALSE) {
		rc = ENOTSUP;
		goto fail1;
	}

	emlp->eml_max_n_outer_prios = maep->em_max_n_outer_prios;
	emlp->eml_max_n_action_prios = maep->em_max_n_action_prios;
	emlp->eml_encap_types_supported = maep->em_encap_types_supported;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn			efx_rc_t
efx_mae_match_spec_init(
	__in				efx_nic_t *enp,
	__in				efx_mae_rule_type_t type,
	__in				uint32_t prio,
	__out				efx_mae_match_spec_t **specp)
{
	efx_mae_match_spec_t *spec;
	efx_rc_t rc;

	switch (type) {
	case EFX_MAE_RULE_OUTER:
		break;
	case EFX_MAE_RULE_ACTION:
		break;
	default:
		rc = ENOTSUP;
		goto fail1;
	}

	EFSYS_KMEM_ALLOC(enp->en_esip, sizeof (*spec), spec);
	if (spec == NULL) {
		rc = ENOMEM;
		goto fail2;
	}

	spec->emms_type = type;
	spec->emms_prio = prio;

	*specp = spec;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

					void
efx_mae_match_spec_fini(
	__in				efx_nic_t *enp,
	__in				efx_mae_match_spec_t *spec)
{
	EFSYS_KMEM_FREE(enp->en_esip, sizeof (*spec), spec);
}

/* Named identifiers which are valid indices to efx_mae_field_cap_t */
typedef enum efx_mae_field_cap_id_e {
	EFX_MAE_FIELD_ID_INGRESS_MPORT_SELECTOR = MAE_FIELD_INGRESS_PORT,
	EFX_MAE_FIELD_ID_ETHER_TYPE_BE = MAE_FIELD_ETHER_TYPE,
	EFX_MAE_FIELD_ID_ETH_SADDR_BE = MAE_FIELD_ETH_SADDR,
	EFX_MAE_FIELD_ID_ETH_DADDR_BE = MAE_FIELD_ETH_DADDR,
	EFX_MAE_FIELD_ID_VLAN0_TCI_BE = MAE_FIELD_VLAN0_TCI,
	EFX_MAE_FIELD_ID_VLAN0_PROTO_BE = MAE_FIELD_VLAN0_PROTO,
	EFX_MAE_FIELD_ID_VLAN1_TCI_BE = MAE_FIELD_VLAN1_TCI,
	EFX_MAE_FIELD_ID_VLAN1_PROTO_BE = MAE_FIELD_VLAN1_PROTO,
	EFX_MAE_FIELD_ID_SRC_IP4_BE = MAE_FIELD_SRC_IP4,
	EFX_MAE_FIELD_ID_DST_IP4_BE = MAE_FIELD_DST_IP4,
	EFX_MAE_FIELD_ID_IP_PROTO = MAE_FIELD_IP_PROTO,
	EFX_MAE_FIELD_ID_IP_TOS = MAE_FIELD_IP_TOS,
	EFX_MAE_FIELD_ID_IP_TTL = MAE_FIELD_IP_TTL,
	EFX_MAE_FIELD_ID_SRC_IP6_BE = MAE_FIELD_SRC_IP6,
	EFX_MAE_FIELD_ID_DST_IP6_BE = MAE_FIELD_DST_IP6,
	EFX_MAE_FIELD_ID_L4_SPORT_BE = MAE_FIELD_L4_SPORT,
	EFX_MAE_FIELD_ID_L4_DPORT_BE = MAE_FIELD_L4_DPORT,
	EFX_MAE_FIELD_ID_TCP_FLAGS_BE = MAE_FIELD_TCP_FLAGS,
	EFX_MAE_FIELD_ID_ENC_ETHER_TYPE_BE = MAE_FIELD_ENC_ETHER_TYPE,
	EFX_MAE_FIELD_ID_ENC_ETH_SADDR_BE = MAE_FIELD_ENC_ETH_SADDR,
	EFX_MAE_FIELD_ID_ENC_ETH_DADDR_BE = MAE_FIELD_ENC_ETH_DADDR,
	EFX_MAE_FIELD_ID_ENC_VLAN0_TCI_BE = MAE_FIELD_ENC_VLAN0_TCI,
	EFX_MAE_FIELD_ID_ENC_VLAN0_PROTO_BE = MAE_FIELD_ENC_VLAN0_PROTO,
	EFX_MAE_FIELD_ID_ENC_VLAN1_TCI_BE = MAE_FIELD_ENC_VLAN1_TCI,
	EFX_MAE_FIELD_ID_ENC_VLAN1_PROTO_BE = MAE_FIELD_ENC_VLAN1_PROTO,
	EFX_MAE_FIELD_ID_ENC_SRC_IP4_BE = MAE_FIELD_ENC_SRC_IP4,
	EFX_MAE_FIELD_ID_ENC_DST_IP4_BE = MAE_FIELD_ENC_DST_IP4,
	EFX_MAE_FIELD_ID_ENC_IP_PROTO = MAE_FIELD_ENC_IP_PROTO,
	EFX_MAE_FIELD_ID_ENC_IP_TOS = MAE_FIELD_ENC_IP_TOS,
	EFX_MAE_FIELD_ID_ENC_IP_TTL = MAE_FIELD_ENC_IP_TTL,
	EFX_MAE_FIELD_ID_ENC_SRC_IP6_BE = MAE_FIELD_ENC_SRC_IP6,
	EFX_MAE_FIELD_ID_ENC_DST_IP6_BE = MAE_FIELD_ENC_DST_IP6,
	EFX_MAE_FIELD_ID_ENC_L4_SPORT_BE = MAE_FIELD_ENC_L4_SPORT,
	EFX_MAE_FIELD_ID_ENC_L4_DPORT_BE = MAE_FIELD_ENC_L4_DPORT,
	EFX_MAE_FIELD_ID_ENC_VNET_ID_BE = MAE_FIELD_ENC_VNET_ID,
	EFX_MAE_FIELD_ID_OUTER_RULE_ID = MAE_FIELD_OUTER_RULE_ID,

	EFX_MAE_FIELD_CAP_NIDS
} efx_mae_field_cap_id_t;

typedef enum efx_mae_field_endianness_e {
	EFX_MAE_FIELD_LE = 0,
	EFX_MAE_FIELD_BE,

	EFX_MAE_FIELD_ENDIANNESS_NTYPES
} efx_mae_field_endianness_t;

/*
 * The following structure is a means to describe an MAE field.
 * The information in it is meant to be used internally by
 * APIs for addressing a given field in a mask-value pairs
 * structure and for validation purposes.
 *
 * A field may have an alternative one. This structure
 * has additional members to reference the alternative
 * field's mask. See efx_mae_match_spec_is_valid().
 */
typedef struct efx_mae_mv_desc_s {
	efx_mae_field_cap_id_t		emmd_field_cap_id;

	size_t				emmd_value_size;
	size_t				emmd_value_offset;
	size_t				emmd_mask_size;
	size_t				emmd_mask_offset;

	/*
	 * Having the alternative field's mask size set to 0
	 * means that there's no alternative field specified.
	 */
	size_t				emmd_alt_mask_size;
	size_t				emmd_alt_mask_offset;

	/* Primary field and the alternative one are of the same endianness. */
	efx_mae_field_endianness_t	emmd_endianness;
} efx_mae_mv_desc_t;

/* Indices to this array are provided by efx_mae_field_id_t */
static const efx_mae_mv_desc_t __efx_mae_action_rule_mv_desc_set[] = {
#define	EFX_MAE_MV_DESC(_name, _endianness)				\
	[EFX_MAE_FIELD_##_name] =					\
	{								\
		EFX_MAE_FIELD_ID_##_name,				\
		MAE_FIELD_MASK_VALUE_PAIRS_##_name##_LEN,		\
		MAE_FIELD_MASK_VALUE_PAIRS_##_name##_OFST,		\
		MAE_FIELD_MASK_VALUE_PAIRS_##_name##_MASK_LEN,		\
		MAE_FIELD_MASK_VALUE_PAIRS_##_name##_MASK_OFST,		\
		0, 0 /* no alternative field */,			\
		_endianness						\
	}

	EFX_MAE_MV_DESC(INGRESS_MPORT_SELECTOR, EFX_MAE_FIELD_LE),
	EFX_MAE_MV_DESC(ETHER_TYPE_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(ETH_SADDR_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(ETH_DADDR_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(VLAN0_TCI_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(VLAN0_PROTO_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(VLAN1_TCI_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(VLAN1_PROTO_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(SRC_IP4_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(DST_IP4_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(IP_PROTO, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(IP_TOS, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(IP_TTL, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(SRC_IP6_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(DST_IP6_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(L4_SPORT_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(L4_DPORT_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(TCP_FLAGS_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(ENC_VNET_ID_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(OUTER_RULE_ID, EFX_MAE_FIELD_LE),

#undef EFX_MAE_MV_DESC
};

/* Indices to this array are provided by efx_mae_field_id_t */
static const efx_mae_mv_desc_t __efx_mae_outer_rule_mv_desc_set[] = {
#define	EFX_MAE_MV_DESC(_name, _endianness)				\
	[EFX_MAE_FIELD_##_name] =					\
	{								\
		EFX_MAE_FIELD_ID_##_name,				\
		MAE_ENC_FIELD_PAIRS_##_name##_LEN,			\
		MAE_ENC_FIELD_PAIRS_##_name##_OFST,			\
		MAE_ENC_FIELD_PAIRS_##_name##_MASK_LEN,			\
		MAE_ENC_FIELD_PAIRS_##_name##_MASK_OFST,		\
		0, 0 /* no alternative field */,			\
		_endianness						\
	}

/* Same as EFX_MAE_MV_DESC(), but also indicates an alternative field. */
#define	EFX_MAE_MV_DESC_ALT(_name, _alt_name, _endianness)		\
	[EFX_MAE_FIELD_##_name] =					\
	{								\
		EFX_MAE_FIELD_ID_##_name,				\
		MAE_ENC_FIELD_PAIRS_##_name##_LEN,			\
		MAE_ENC_FIELD_PAIRS_##_name##_OFST,			\
		MAE_ENC_FIELD_PAIRS_##_name##_MASK_LEN,			\
		MAE_ENC_FIELD_PAIRS_##_name##_MASK_OFST,		\
		MAE_ENC_FIELD_PAIRS_##_alt_name##_MASK_LEN,		\
		MAE_ENC_FIELD_PAIRS_##_alt_name##_MASK_OFST,		\
		_endianness						\
	}

	EFX_MAE_MV_DESC(INGRESS_MPORT_SELECTOR, EFX_MAE_FIELD_LE),
	EFX_MAE_MV_DESC(ENC_ETHER_TYPE_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(ENC_ETH_SADDR_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(ENC_ETH_DADDR_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(ENC_VLAN0_TCI_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(ENC_VLAN0_PROTO_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(ENC_VLAN1_TCI_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(ENC_VLAN1_PROTO_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC_ALT(ENC_SRC_IP4_BE, ENC_SRC_IP6_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC_ALT(ENC_DST_IP4_BE, ENC_DST_IP6_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(ENC_IP_PROTO, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(ENC_IP_TOS, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(ENC_IP_TTL, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC_ALT(ENC_SRC_IP6_BE, ENC_SRC_IP4_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC_ALT(ENC_DST_IP6_BE, ENC_DST_IP4_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(ENC_L4_SPORT_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(ENC_L4_DPORT_BE, EFX_MAE_FIELD_BE),

#undef EFX_MAE_MV_DESC_ALT
#undef EFX_MAE_MV_DESC
};

	__checkReturn			efx_rc_t
efx_mae_mport_by_phy_port(
	__in				uint32_t phy_port,
	__out				efx_mport_sel_t *mportp)
{
	efx_dword_t dword;
	efx_rc_t rc;

	if (phy_port > EFX_MASK32(MAE_MPORT_SELECTOR_PPORT_ID)) {
		rc = EINVAL;
		goto fail1;
	}

	EFX_POPULATE_DWORD_2(dword,
	    MAE_MPORT_SELECTOR_TYPE, MAE_MPORT_SELECTOR_TYPE_PPORT,
	    MAE_MPORT_SELECTOR_PPORT_ID, phy_port);

	memset(mportp, 0, sizeof (*mportp));
	/*
	 * The constructed DWORD is little-endian,
	 * but the resulting value is meant to be
	 * passed to MCDIs, where it will undergo
	 * host-order to little endian conversion.
	 */
	mportp->sel = EFX_DWORD_FIELD(dword, EFX_DWORD_0);

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn			efx_rc_t
efx_mae_mport_by_pcie_function(
	__in				uint32_t pf,
	__in				uint32_t vf,
	__out				efx_mport_sel_t *mportp)
{
	efx_dword_t dword;
	efx_rc_t rc;

	EFX_STATIC_ASSERT(EFX_PCI_VF_INVALID ==
	    MAE_MPORT_SELECTOR_FUNC_VF_ID_NULL);

	if (pf > EFX_MASK32(MAE_MPORT_SELECTOR_FUNC_PF_ID)) {
		rc = EINVAL;
		goto fail1;
	}

	if (vf > EFX_MASK32(MAE_MPORT_SELECTOR_FUNC_VF_ID)) {
		rc = EINVAL;
		goto fail2;
	}

	EFX_POPULATE_DWORD_3(dword,
	    MAE_MPORT_SELECTOR_TYPE, MAE_MPORT_SELECTOR_TYPE_FUNC,
	    MAE_MPORT_SELECTOR_FUNC_PF_ID, pf,
	    MAE_MPORT_SELECTOR_FUNC_VF_ID, vf);

	memset(mportp, 0, sizeof (*mportp));
	/*
	 * The constructed DWORD is little-endian,
	 * but the resulting value is meant to be
	 * passed to MCDIs, where it will undergo
	 * host-order to little endian conversion.
	 */
	mportp->sel = EFX_DWORD_FIELD(dword, EFX_DWORD_0);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn			efx_rc_t
efx_mae_match_spec_field_set(
	__in				efx_mae_match_spec_t *spec,
	__in				efx_mae_field_id_t field_id,
	__in				size_t value_size,
	__in_bcount(value_size)		const uint8_t *value,
	__in				size_t mask_size,
	__in_bcount(mask_size)		const uint8_t *mask)
{
	const efx_mae_mv_desc_t *descp;
	unsigned int desc_set_nentries;
	uint8_t *mvp;
	efx_rc_t rc;

	switch (spec->emms_type) {
	case EFX_MAE_RULE_OUTER:
		desc_set_nentries =
		    EFX_ARRAY_SIZE(__efx_mae_outer_rule_mv_desc_set);
		descp = &__efx_mae_outer_rule_mv_desc_set[field_id];
		mvp = spec->emms_mask_value_pairs.outer;
		break;
	case EFX_MAE_RULE_ACTION:
		desc_set_nentries =
		    EFX_ARRAY_SIZE(__efx_mae_action_rule_mv_desc_set);
		descp = &__efx_mae_action_rule_mv_desc_set[field_id];
		mvp = spec->emms_mask_value_pairs.action;
		break;
	default:
		rc = ENOTSUP;
		goto fail1;
	}

	if ((unsigned int)field_id >= desc_set_nentries) {
		rc = EINVAL;
		goto fail2;
	}

	if (descp->emmd_mask_size == 0) {
		/* The ID points to a gap in the array of field descriptors. */
		rc = EINVAL;
		goto fail3;
	}

	if (value_size != descp->emmd_value_size) {
		rc = EINVAL;
		goto fail4;
	}

	if (mask_size != descp->emmd_mask_size) {
		rc = EINVAL;
		goto fail5;
	}

	if (descp->emmd_endianness == EFX_MAE_FIELD_BE) {
		unsigned int i;

		/*
		 * The mask/value are in network (big endian) order.
		 * The MCDI request field is also big endian.
		 */

		EFSYS_ASSERT3U(value_size, ==, mask_size);

		for (i = 0; i < value_size; ++i) {
			uint8_t *v_bytep = mvp + descp->emmd_value_offset + i;
			uint8_t *m_bytep = mvp + descp->emmd_mask_offset + i;

			/*
			 * Apply the mask (which may be all-zeros) to the value.
			 *
			 * If this API is provided with some value to set for a
			 * given field in one specification and with some other
			 * value to set for this field in another specification,
			 * then, if the two masks are all-zeros, the field will
			 * avoid being counted as a mismatch when comparing the
			 * specifications using efx_mae_match_specs_equal() API.
			 */
			*v_bytep = value[i] & mask[i];
			*m_bytep = mask[i];
		}
	} else {
		efx_dword_t dword;

		/*
		 * The mask/value are in host byte order.
		 * The MCDI request field is little endian.
		 */
		switch (value_size) {
		case 4:
			EFX_POPULATE_DWORD_1(dword,
			    EFX_DWORD_0, *(const uint32_t *)value);

			memcpy(mvp + descp->emmd_value_offset,
			    &dword, sizeof (dword));
			break;
		default:
			EFSYS_ASSERT(B_FALSE);
		}

		switch (mask_size) {
		case 4:
			EFX_POPULATE_DWORD_1(dword,
			    EFX_DWORD_0, *(const uint32_t *)mask);

			memcpy(mvp + descp->emmd_mask_offset,
			    &dword, sizeof (dword));
			break;
		default:
			EFSYS_ASSERT(B_FALSE);
		}
	}

	return (0);

fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn			efx_rc_t
efx_mae_match_spec_mport_set(
	__in				efx_mae_match_spec_t *spec,
	__in				const efx_mport_sel_t *valuep,
	__in_opt			const efx_mport_sel_t *maskp)
{
	uint32_t full_mask = UINT32_MAX;
	const uint8_t *vp;
	const uint8_t *mp;
	efx_rc_t rc;

	if (valuep == NULL) {
		rc = EINVAL;
		goto fail1;
	}

	vp = (const uint8_t *)&valuep->sel;
	if (maskp != NULL)
		mp = (const uint8_t *)&maskp->sel;
	else
		mp = (const uint8_t *)&full_mask;

	rc = efx_mae_match_spec_field_set(spec,
	    EFX_MAE_FIELD_INGRESS_MPORT_SELECTOR,
	    sizeof (valuep->sel), vp, sizeof (maskp->sel), mp);
	if (rc != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn			boolean_t
efx_mae_match_specs_equal(
	__in				const efx_mae_match_spec_t *left,
	__in				const efx_mae_match_spec_t *right)
{
	return ((memcmp(left, right, sizeof (*left)) == 0) ? B_TRUE : B_FALSE);
}

#define	EFX_MASK_BIT_IS_SET(_mask, _mask_page_nbits, _bit)		\
	    ((_mask)[(_bit) / (_mask_page_nbits)] &			\
		    (1ULL << ((_bit) & ((_mask_page_nbits) - 1))))

static					boolean_t
efx_mask_is_prefix(
	__in				size_t mask_nbytes,
	__in_bcount(mask_nbytes)	const uint8_t *maskp)
{
	boolean_t prev_bit_is_set = B_TRUE;
	unsigned int i;

	for (i = 0; i < 8 * mask_nbytes; ++i) {
		boolean_t bit_is_set = EFX_MASK_BIT_IS_SET(maskp, 8, i);

		if (!prev_bit_is_set && bit_is_set)
			return B_FALSE;

		prev_bit_is_set = bit_is_set;
	}

	return B_TRUE;
}

static					boolean_t
efx_mask_is_all_ones(
	__in				size_t mask_nbytes,
	__in_bcount(mask_nbytes)	const uint8_t *maskp)
{
	unsigned int i;
	uint8_t t = ~0;

	for (i = 0; i < mask_nbytes; ++i)
		t &= maskp[i];

	return (t == (uint8_t)(~0));
}

static					boolean_t
efx_mask_is_all_zeros(
	__in				size_t mask_nbytes,
	__in_bcount(mask_nbytes)	const uint8_t *maskp)
{
	unsigned int i;
	uint8_t t = 0;

	for (i = 0; i < mask_nbytes; ++i)
		t |= maskp[i];

	return (t == 0);
}

	__checkReturn			boolean_t
efx_mae_match_spec_is_valid(
	__in				efx_nic_t *enp,
	__in				const efx_mae_match_spec_t *spec)
{
	efx_mae_t *maep = enp->en_maep;
	unsigned int field_ncaps = maep->em_max_nfields;
	const efx_mae_field_cap_t *field_caps;
	const efx_mae_mv_desc_t *desc_setp;
	unsigned int desc_set_nentries;
	boolean_t is_valid = B_TRUE;
	efx_mae_field_id_t field_id;
	const uint8_t *mvp;

	switch (spec->emms_type) {
	case EFX_MAE_RULE_OUTER:
		field_caps = maep->em_outer_rule_field_caps;
		desc_setp = __efx_mae_outer_rule_mv_desc_set;
		desc_set_nentries =
		    EFX_ARRAY_SIZE(__efx_mae_outer_rule_mv_desc_set);
		mvp = spec->emms_mask_value_pairs.outer;
		break;
	case EFX_MAE_RULE_ACTION:
		field_caps = maep->em_action_rule_field_caps;
		desc_setp = __efx_mae_action_rule_mv_desc_set;
		desc_set_nentries =
		    EFX_ARRAY_SIZE(__efx_mae_action_rule_mv_desc_set);
		mvp = spec->emms_mask_value_pairs.action;
		break;
	default:
		return (B_FALSE);
	}

	if (field_caps == NULL)
		return (B_FALSE);

	for (field_id = 0; (unsigned int)field_id < desc_set_nentries;
	     ++field_id) {
		const efx_mae_mv_desc_t *descp = &desc_setp[field_id];
		efx_mae_field_cap_id_t field_cap_id = descp->emmd_field_cap_id;
		const uint8_t *alt_m_buf = mvp + descp->emmd_alt_mask_offset;
		const uint8_t *m_buf = mvp + descp->emmd_mask_offset;
		size_t alt_m_size = descp->emmd_alt_mask_size;
		size_t m_size = descp->emmd_mask_size;

		if (m_size == 0)
			continue; /* Skip array gap */

		if ((unsigned int)field_cap_id >= field_ncaps) {
			/*
			 * The FW has not reported capability status for
			 * this field. Make sure that its mask is zeroed.
			 */
			is_valid = efx_mask_is_all_zeros(m_size, m_buf);
			if (is_valid != B_FALSE)
				continue;
			else
				break;
		}

		switch (field_caps[field_cap_id].emfc_support) {
		case MAE_FIELD_SUPPORTED_MATCH_MASK:
			is_valid = B_TRUE;
			break;
		case MAE_FIELD_SUPPORTED_MATCH_PREFIX:
			is_valid = efx_mask_is_prefix(m_size, m_buf);
			break;
		case MAE_FIELD_SUPPORTED_MATCH_OPTIONAL:
			is_valid = (efx_mask_is_all_ones(m_size, m_buf) ||
			    efx_mask_is_all_zeros(m_size, m_buf));
			break;
		case MAE_FIELD_SUPPORTED_MATCH_ALWAYS:
			is_valid = efx_mask_is_all_ones(m_size, m_buf);

			if ((is_valid == B_FALSE) && (alt_m_size != 0)) {
				/*
				 * This field has an alternative one. The FW
				 * reports ALWAYS for both implying that one
				 * of them is required to have all-ones mask.
				 *
				 * The primary field's mask is incorrect; go
				 * on to check that of the alternative field.
				 */
				is_valid = efx_mask_is_all_ones(alt_m_size,
								alt_m_buf);
			}
			break;
		case MAE_FIELD_SUPPORTED_MATCH_NEVER:
		case MAE_FIELD_UNSUPPORTED:
		default:
			is_valid = efx_mask_is_all_zeros(m_size, m_buf);
			break;
		}

		if (is_valid == B_FALSE)
			break;
	}

	return (is_valid);
}

	__checkReturn			efx_rc_t
efx_mae_action_set_spec_init(
	__in				efx_nic_t *enp,
	__out				efx_mae_actions_t **specp)
{
	efx_mae_actions_t *spec;
	efx_rc_t rc;

	EFSYS_KMEM_ALLOC(enp->en_esip, sizeof (*spec), spec);
	if (spec == NULL) {
		rc = ENOMEM;
		goto fail1;
	}

	*specp = spec;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

					void
efx_mae_action_set_spec_fini(
	__in				efx_nic_t *enp,
	__in				efx_mae_actions_t *spec)
{
	EFSYS_KMEM_FREE(enp->en_esip, sizeof (*spec), spec);
}

static	__checkReturn			efx_rc_t
efx_mae_action_set_add_vlan_pop(
	__in				efx_mae_actions_t *spec,
	__in				size_t arg_size,
	__in_bcount(arg_size)		const uint8_t *arg)
{
	efx_rc_t rc;

	if (arg_size != 0) {
		rc = EINVAL;
		goto fail1;
	}

	if (arg != NULL) {
		rc = EINVAL;
		goto fail2;
	}

	if (spec->ema_n_vlan_tags_to_pop == EFX_MAE_VLAN_POP_MAX_NTAGS) {
		rc = ENOTSUP;
		goto fail3;
	}

	++spec->ema_n_vlan_tags_to_pop;

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

static	__checkReturn			efx_rc_t
efx_mae_action_set_add_vlan_push(
	__in				efx_mae_actions_t *spec,
	__in				size_t arg_size,
	__in_bcount(arg_size)		const uint8_t *arg)
{
	unsigned int n_tags = spec->ema_n_vlan_tags_to_push;
	efx_rc_t rc;

	if (arg_size != sizeof (*spec->ema_vlan_push_descs)) {
		rc = EINVAL;
		goto fail1;
	}

	if (arg == NULL) {
		rc = EINVAL;
		goto fail2;
	}

	if (n_tags == EFX_MAE_VLAN_PUSH_MAX_NTAGS) {
		rc = ENOTSUP;
		goto fail3;
	}

	memcpy(&spec->ema_vlan_push_descs[n_tags], arg, arg_size);
	++(spec->ema_n_vlan_tags_to_push);

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

static	__checkReturn			efx_rc_t
efx_mae_action_set_add_flag(
	__in				efx_mae_actions_t *spec,
	__in				size_t arg_size,
	__in_bcount(arg_size)		const uint8_t *arg)
{
	efx_rc_t rc;

	_NOTE(ARGUNUSED(spec))

	if (arg_size != 0) {
		rc = EINVAL;
		goto fail1;
	}

	if (arg != NULL) {
		rc = EINVAL;
		goto fail2;
	}

	/* This action does not have any arguments, so do nothing here. */

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

static	__checkReturn			efx_rc_t
efx_mae_action_set_add_mark(
	__in				efx_mae_actions_t *spec,
	__in				size_t arg_size,
	__in_bcount(arg_size)		const uint8_t *arg)
{
	efx_rc_t rc;

	if (arg_size != sizeof (spec->ema_mark_value)) {
		rc = EINVAL;
		goto fail1;
	}

	if (arg == NULL) {
		rc = EINVAL;
		goto fail2;
	}

	memcpy(&spec->ema_mark_value, arg, arg_size);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

static	__checkReturn			efx_rc_t
efx_mae_action_set_add_deliver(
	__in				efx_mae_actions_t *spec,
	__in				size_t arg_size,
	__in_bcount(arg_size)		const uint8_t *arg)
{
	efx_rc_t rc;

	if (arg_size != sizeof (spec->ema_deliver_mport)) {
		rc = EINVAL;
		goto fail1;
	}

	if (arg == NULL) {
		rc = EINVAL;
		goto fail2;
	}

	memcpy(&spec->ema_deliver_mport, arg, arg_size);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

typedef struct efx_mae_action_desc_s {
	/* Action specific handler */
	efx_rc_t	(*emad_add)(efx_mae_actions_t *,
				    size_t, const uint8_t *);
} efx_mae_action_desc_t;

static const efx_mae_action_desc_t efx_mae_actions[EFX_MAE_NACTIONS] = {
	[EFX_MAE_ACTION_VLAN_POP] = {
		.emad_add = efx_mae_action_set_add_vlan_pop
	},
	[EFX_MAE_ACTION_VLAN_PUSH] = {
		.emad_add = efx_mae_action_set_add_vlan_push
	},
	[EFX_MAE_ACTION_FLAG] = {
		.emad_add = efx_mae_action_set_add_flag
	},
	[EFX_MAE_ACTION_MARK] = {
		.emad_add = efx_mae_action_set_add_mark
	},
	[EFX_MAE_ACTION_DELIVER] = {
		.emad_add = efx_mae_action_set_add_deliver
	}
};

static const uint32_t efx_mae_action_ordered_map =
	(1U << EFX_MAE_ACTION_VLAN_POP) |
	(1U << EFX_MAE_ACTION_VLAN_PUSH) |
	(1U << EFX_MAE_ACTION_FLAG) |
	(1U << EFX_MAE_ACTION_MARK) |
	(1U << EFX_MAE_ACTION_DELIVER);

/*
 * These actions must not be added after DELIVER, but
 * they can have any place among the rest of
 * strictly ordered actions.
 */
static const uint32_t efx_mae_action_nonstrict_map =
	(1U << EFX_MAE_ACTION_FLAG) |
	(1U << EFX_MAE_ACTION_MARK);

static const uint32_t efx_mae_action_repeat_map =
	(1U << EFX_MAE_ACTION_VLAN_POP) |
	(1U << EFX_MAE_ACTION_VLAN_PUSH);

/*
 * Add an action to an action set.
 *
 * This has to be invoked in the desired action order.
 * An out-of-order action request will be turned down.
 */
static	__checkReturn			efx_rc_t
efx_mae_action_set_spec_populate(
	__in				efx_mae_actions_t *spec,
	__in				efx_mae_action_t type,
	__in				size_t arg_size,
	__in_bcount(arg_size)		const uint8_t *arg)
{
	uint32_t action_mask;
	efx_rc_t rc;

	EFX_STATIC_ASSERT(EFX_MAE_NACTIONS <=
	    (sizeof (efx_mae_action_ordered_map) * 8));
	EFX_STATIC_ASSERT(EFX_MAE_NACTIONS <=
	    (sizeof (efx_mae_action_repeat_map) * 8));

	EFX_STATIC_ASSERT(EFX_MAE_ACTION_DELIVER + 1 == EFX_MAE_NACTIONS);
	EFX_STATIC_ASSERT(EFX_MAE_ACTION_FLAG + 1 == EFX_MAE_ACTION_MARK);
	EFX_STATIC_ASSERT(EFX_MAE_ACTION_MARK + 1 == EFX_MAE_ACTION_DELIVER);

	if (type >= EFX_ARRAY_SIZE(efx_mae_actions)) {
		rc = EINVAL;
		goto fail1;
	}

	action_mask = (1U << type);

	if ((spec->ema_actions & action_mask) != 0) {
		/* The action set already contains this action. */
		if ((efx_mae_action_repeat_map & action_mask) == 0) {
			/* Cannot add another non-repeatable action. */
			rc = ENOTSUP;
			goto fail2;
		}
	}

	if ((efx_mae_action_ordered_map & action_mask) != 0) {
		uint32_t strict_ordered_map =
		    efx_mae_action_ordered_map & ~efx_mae_action_nonstrict_map;
		uint32_t later_actions_mask =
		    strict_ordered_map & ~(action_mask | (action_mask - 1));

		if ((spec->ema_actions & later_actions_mask) != 0) {
			/* Cannot add an action after later ordered actions. */
			rc = ENOTSUP;
			goto fail3;
		}
	}

	if (efx_mae_actions[type].emad_add != NULL) {
		rc = efx_mae_actions[type].emad_add(spec, arg_size, arg);
		if (rc != 0)
			goto fail4;
	}

	spec->ema_actions |= action_mask;

	return (0);

fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn			efx_rc_t
efx_mae_action_set_populate_vlan_pop(
	__in				efx_mae_actions_t *spec)
{
	return (efx_mae_action_set_spec_populate(spec,
	    EFX_MAE_ACTION_VLAN_POP, 0, NULL));
}

	__checkReturn			efx_rc_t
efx_mae_action_set_populate_vlan_push(
	__in				efx_mae_actions_t *spec,
	__in				uint16_t tpid_be,
	__in				uint16_t tci_be)
{
	efx_mae_action_vlan_push_t action;
	const uint8_t *arg = (const uint8_t *)&action;

	action.emavp_tpid_be = tpid_be;
	action.emavp_tci_be = tci_be;

	return (efx_mae_action_set_spec_populate(spec,
	    EFX_MAE_ACTION_VLAN_PUSH, sizeof (action), arg));
}

	__checkReturn			efx_rc_t
efx_mae_action_set_populate_flag(
	__in				efx_mae_actions_t *spec)
{
	return (efx_mae_action_set_spec_populate(spec,
	    EFX_MAE_ACTION_FLAG, 0, NULL));
}

	__checkReturn			efx_rc_t
efx_mae_action_set_populate_mark(
	__in				efx_mae_actions_t *spec,
	__in				uint32_t mark_value)
{
	const uint8_t *arg = (const uint8_t *)&mark_value;

	return (efx_mae_action_set_spec_populate(spec,
	    EFX_MAE_ACTION_MARK, sizeof (mark_value), arg));
}

	__checkReturn			efx_rc_t
efx_mae_action_set_populate_deliver(
	__in				efx_mae_actions_t *spec,
	__in				const efx_mport_sel_t *mportp)
{
	const uint8_t *arg;
	efx_rc_t rc;

	if (mportp == NULL) {
		rc = EINVAL;
		goto fail1;
	}

	arg = (const uint8_t *)&mportp->sel;

	return (efx_mae_action_set_spec_populate(spec,
	    EFX_MAE_ACTION_DELIVER, sizeof (mportp->sel), arg));

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn			efx_rc_t
efx_mae_action_set_populate_drop(
	__in				efx_mae_actions_t *spec)
{
	efx_mport_sel_t mport;
	const uint8_t *arg;
	efx_dword_t dword;

	EFX_POPULATE_DWORD_1(dword,
	    MAE_MPORT_SELECTOR_FLAT, MAE_MPORT_SELECTOR_NULL);

	/*
	 * The constructed DWORD is little-endian,
	 * but the resulting value is meant to be
	 * passed to MCDIs, where it will undergo
	 * host-order to little endian conversion.
	 */
	mport.sel = EFX_DWORD_FIELD(dword, EFX_DWORD_0);

	arg = (const uint8_t *)&mport.sel;

	return (efx_mae_action_set_spec_populate(spec,
	    EFX_MAE_ACTION_DELIVER, sizeof (mport.sel), arg));
}

	__checkReturn			boolean_t
efx_mae_action_set_specs_equal(
	__in				const efx_mae_actions_t *left,
	__in				const efx_mae_actions_t *right)
{
	return ((memcmp(left, right, sizeof (*left)) == 0) ? B_TRUE : B_FALSE);
}

	__checkReturn			efx_rc_t
efx_mae_match_specs_class_cmp(
	__in				efx_nic_t *enp,
	__in				const efx_mae_match_spec_t *left,
	__in				const efx_mae_match_spec_t *right,
	__out				boolean_t *have_same_classp)
{
	efx_mae_t *maep = enp->en_maep;
	unsigned int field_ncaps = maep->em_max_nfields;
	const efx_mae_field_cap_t *field_caps;
	const efx_mae_mv_desc_t *desc_setp;
	unsigned int desc_set_nentries;
	boolean_t have_same_class = B_TRUE;
	efx_mae_field_id_t field_id;
	const uint8_t *mvpl;
	const uint8_t *mvpr;
	efx_rc_t rc;

	switch (left->emms_type) {
	case EFX_MAE_RULE_OUTER:
		field_caps = maep->em_outer_rule_field_caps;
		desc_setp = __efx_mae_outer_rule_mv_desc_set;
		desc_set_nentries =
		    EFX_ARRAY_SIZE(__efx_mae_outer_rule_mv_desc_set);
		mvpl = left->emms_mask_value_pairs.outer;
		mvpr = right->emms_mask_value_pairs.outer;
		break;
	case EFX_MAE_RULE_ACTION:
		field_caps = maep->em_action_rule_field_caps;
		desc_setp = __efx_mae_action_rule_mv_desc_set;
		desc_set_nentries =
		    EFX_ARRAY_SIZE(__efx_mae_action_rule_mv_desc_set);
		mvpl = left->emms_mask_value_pairs.action;
		mvpr = right->emms_mask_value_pairs.action;
		break;
	default:
		rc = ENOTSUP;
		goto fail1;
	}

	if (field_caps == NULL) {
		rc = EAGAIN;
		goto fail2;
	}

	if (left->emms_type != right->emms_type ||
	    left->emms_prio != right->emms_prio) {
		/*
		 * Rules of different types can never map to the same class.
		 *
		 * The FW can support some set of match criteria for one
		 * priority and not support the very same set for
		 * another priority. Thus, two rules which have
		 * different priorities can never map to
		 * the same class.
		 */
		*have_same_classp = B_FALSE;
		return (0);
	}

	for (field_id = 0; (unsigned int)field_id < desc_set_nentries;
	     ++field_id) {
		const efx_mae_mv_desc_t *descp = &desc_setp[field_id];
		efx_mae_field_cap_id_t field_cap_id = descp->emmd_field_cap_id;
		const uint8_t *lmaskp = mvpl + descp->emmd_mask_offset;
		const uint8_t *rmaskp = mvpr + descp->emmd_mask_offset;
		size_t mask_size = descp->emmd_mask_size;
		const uint8_t *lvalp = mvpl + descp->emmd_value_offset;
		const uint8_t *rvalp = mvpr + descp->emmd_value_offset;
		size_t value_size = descp->emmd_value_size;

		if (mask_size == 0)
			continue; /* Skip array gap */

		if ((unsigned int)field_cap_id >= field_ncaps) {
			/*
			 * The FW has not reported capability status for this
			 * field. It's unknown whether any difference between
			 * the two masks / values affects the class. The only
			 * case when the class must be the same is when these
			 * mask-value pairs match. Otherwise, report mismatch.
			 */
			if ((memcmp(lmaskp, rmaskp, mask_size) == 0) &&
			    (memcmp(lvalp, rvalp, value_size) == 0))
				continue;
			else
				break;
		}

		if (field_caps[field_cap_id].emfc_mask_affects_class) {
			if (memcmp(lmaskp, rmaskp, mask_size) != 0) {
				have_same_class = B_FALSE;
				break;
			}
		}

		if (field_caps[field_cap_id].emfc_match_affects_class) {
			if (memcmp(lvalp, rvalp, value_size) != 0) {
				have_same_class = B_FALSE;
				break;
			}
		}
	}

	*have_same_classp = have_same_class;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn		efx_rc_t
efx_mae_outer_rule_insert(
	__in			efx_nic_t *enp,
	__in			const efx_mae_match_spec_t *spec,
	__in			efx_tunnel_protocol_t encap_type,
	__out			efx_mae_rule_id_t *or_idp)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(enp);
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload,
	    MC_CMD_MAE_OUTER_RULE_INSERT_IN_LENMAX_MCDI2,
	    MC_CMD_MAE_OUTER_RULE_INSERT_OUT_LEN);
	uint32_t encap_type_mcdi;
	efx_mae_rule_id_t or_id;
	size_t offset;
	efx_rc_t rc;

	EFX_STATIC_ASSERT(sizeof (or_idp->id) ==
	    MC_CMD_MAE_OUTER_RULE_INSERT_OUT_OR_ID_LEN);

	EFX_STATIC_ASSERT(EFX_MAE_RSRC_ID_INVALID ==
	    MC_CMD_MAE_OUTER_RULE_INSERT_OUT_OUTER_RULE_ID_NULL);

	if (encp->enc_mae_supported == B_FALSE) {
		rc = ENOTSUP;
		goto fail1;
	}

	if (spec->emms_type != EFX_MAE_RULE_OUTER) {
		rc = EINVAL;
		goto fail2;
	}

	switch (encap_type) {
	case EFX_TUNNEL_PROTOCOL_NONE:
		encap_type_mcdi = MAE_MCDI_ENCAP_TYPE_NONE;
		break;
	case EFX_TUNNEL_PROTOCOL_VXLAN:
		encap_type_mcdi = MAE_MCDI_ENCAP_TYPE_VXLAN;
		break;
	case EFX_TUNNEL_PROTOCOL_GENEVE:
		encap_type_mcdi = MAE_MCDI_ENCAP_TYPE_GENEVE;
		break;
	case EFX_TUNNEL_PROTOCOL_NVGRE:
		encap_type_mcdi = MAE_MCDI_ENCAP_TYPE_NVGRE;
		break;
	default:
		rc = ENOTSUP;
		goto fail3;
	}

	req.emr_cmd = MC_CMD_MAE_OUTER_RULE_INSERT;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_MAE_OUTER_RULE_INSERT_IN_LENMAX_MCDI2;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_MAE_OUTER_RULE_INSERT_OUT_LEN;

	MCDI_IN_SET_DWORD(req,
	    MAE_OUTER_RULE_INSERT_IN_ENCAP_TYPE, encap_type_mcdi);

	MCDI_IN_SET_DWORD(req, MAE_OUTER_RULE_INSERT_IN_PRIO, spec->emms_prio);

	/*
	 * Mask-value pairs have been stored in the byte order needed for the
	 * MCDI request and are thus safe to be copied directly to the buffer.
	 * The library cares about byte order in efx_mae_match_spec_field_set().
	 */
	EFX_STATIC_ASSERT(sizeof (spec->emms_mask_value_pairs.outer) >=
	    MAE_ENC_FIELD_PAIRS_LEN);
	offset = MC_CMD_MAE_OUTER_RULE_INSERT_IN_FIELD_MATCH_CRITERIA_OFST;
	memcpy(payload + offset, spec->emms_mask_value_pairs.outer,
	    MAE_ENC_FIELD_PAIRS_LEN);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail4;
	}

	if (req.emr_out_length_used < MC_CMD_MAE_OUTER_RULE_INSERT_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail5;
	}

	or_id.id = MCDI_OUT_DWORD(req, MAE_OUTER_RULE_INSERT_OUT_OR_ID);
	if (or_id.id == EFX_MAE_RSRC_ID_INVALID) {
		rc = ENOENT;
		goto fail6;
	}

	or_idp->id = or_id.id;

	return (0);

fail6:
	EFSYS_PROBE(fail6);
fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn		efx_rc_t
efx_mae_outer_rule_remove(
	__in			efx_nic_t *enp,
	__in			const efx_mae_rule_id_t *or_idp)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(enp);
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload,
	    MC_CMD_MAE_OUTER_RULE_REMOVE_IN_LEN(1),
	    MC_CMD_MAE_OUTER_RULE_REMOVE_OUT_LEN(1));
	efx_rc_t rc;

	if (encp->enc_mae_supported == B_FALSE) {
		rc = ENOTSUP;
		goto fail1;
	}

	req.emr_cmd = MC_CMD_MAE_OUTER_RULE_REMOVE;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_MAE_OUTER_RULE_REMOVE_IN_LEN(1);
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_MAE_OUTER_RULE_REMOVE_OUT_LEN(1);

	MCDI_IN_SET_DWORD(req, MAE_OUTER_RULE_REMOVE_IN_OR_ID, or_idp->id);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail2;
	}

	if (req.emr_out_length_used < MC_CMD_MAE_OUTER_RULE_REMOVE_OUT_LENMIN) {
		rc = EMSGSIZE;
		goto fail3;
	}

	if (MCDI_OUT_DWORD(req, MAE_OUTER_RULE_REMOVE_OUT_REMOVED_OR_ID) !=
	    or_idp->id) {
		/* Firmware failed to remove the outer rule. */
		rc = EAGAIN;
		goto fail4;
	}

	return (0);

fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn			efx_rc_t
efx_mae_match_spec_outer_rule_id_set(
	__in				efx_mae_match_spec_t *spec,
	__in				const efx_mae_rule_id_t *or_idp)
{
	uint32_t full_mask = UINT32_MAX;
	efx_rc_t rc;

	if (spec->emms_type != EFX_MAE_RULE_ACTION) {
		rc = EINVAL;
		goto fail1;
	}

	if (or_idp == NULL) {
		rc = EINVAL;
		goto fail2;
	}

	rc = efx_mae_match_spec_field_set(spec, EFX_MAE_FIELD_OUTER_RULE_ID,
	    sizeof (or_idp->id), (const uint8_t *)&or_idp->id,
	    sizeof (full_mask), (const uint8_t *)&full_mask);
	if (rc != 0)
		goto fail3;

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn			efx_rc_t
efx_mae_action_set_alloc(
	__in				efx_nic_t *enp,
	__in				const efx_mae_actions_t *spec,
	__out				efx_mae_aset_id_t *aset_idp)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(enp);
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload,
	    MC_CMD_MAE_ACTION_SET_ALLOC_IN_LEN,
	    MC_CMD_MAE_ACTION_SET_ALLOC_OUT_LEN);
	efx_mae_aset_id_t aset_id;
	efx_rc_t rc;

	if (encp->enc_mae_supported == B_FALSE) {
		rc = ENOTSUP;
		goto fail1;
	}

	req.emr_cmd = MC_CMD_MAE_ACTION_SET_ALLOC;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_MAE_ACTION_SET_ALLOC_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_MAE_ACTION_SET_ALLOC_OUT_LEN;

	/*
	 * TODO: Remove these EFX_MAE_RSRC_ID_INVALID assignments once the
	 * corresponding resource types are supported by the implementation.
	 * Use proper resource ID assignments instead.
	 */
	MCDI_IN_SET_DWORD(req,
	    MAE_ACTION_SET_ALLOC_IN_COUNTER_LIST_ID, EFX_MAE_RSRC_ID_INVALID);
	MCDI_IN_SET_DWORD(req,
	    MAE_ACTION_SET_ALLOC_IN_COUNTER_ID, EFX_MAE_RSRC_ID_INVALID);
	MCDI_IN_SET_DWORD(req,
	    MAE_ACTION_SET_ALLOC_IN_ENCAP_HEADER_ID, EFX_MAE_RSRC_ID_INVALID);

	MCDI_IN_SET_DWORD_FIELD(req, MAE_ACTION_SET_ALLOC_IN_FLAGS,
	    MAE_ACTION_SET_ALLOC_IN_VLAN_POP, spec->ema_n_vlan_tags_to_pop);

	if (spec->ema_n_vlan_tags_to_push > 0) {
		unsigned int outer_tag_idx;

		MCDI_IN_SET_DWORD_FIELD(req, MAE_ACTION_SET_ALLOC_IN_FLAGS,
		    MAE_ACTION_SET_ALLOC_IN_VLAN_PUSH,
		    spec->ema_n_vlan_tags_to_push);

		if (spec->ema_n_vlan_tags_to_push ==
		    EFX_MAE_VLAN_PUSH_MAX_NTAGS) {
			MCDI_IN_SET_WORD(req,
			    MAE_ACTION_SET_ALLOC_IN_VLAN1_PROTO_BE,
			    spec->ema_vlan_push_descs[0].emavp_tpid_be);
			MCDI_IN_SET_WORD(req,
			    MAE_ACTION_SET_ALLOC_IN_VLAN1_TCI_BE,
			    spec->ema_vlan_push_descs[0].emavp_tci_be);
		}

		outer_tag_idx = spec->ema_n_vlan_tags_to_push - 1;

		MCDI_IN_SET_WORD(req, MAE_ACTION_SET_ALLOC_IN_VLAN0_PROTO_BE,
		    spec->ema_vlan_push_descs[outer_tag_idx].emavp_tpid_be);
		MCDI_IN_SET_WORD(req, MAE_ACTION_SET_ALLOC_IN_VLAN0_TCI_BE,
		    spec->ema_vlan_push_descs[outer_tag_idx].emavp_tci_be);
	}

	if ((spec->ema_actions & (1U << EFX_MAE_ACTION_FLAG)) != 0) {
		MCDI_IN_SET_DWORD_FIELD(req, MAE_ACTION_SET_ALLOC_IN_FLAGS,
		    MAE_ACTION_SET_ALLOC_IN_FLAG, 1);
	}

	if ((spec->ema_actions & (1U << EFX_MAE_ACTION_MARK)) != 0) {
		MCDI_IN_SET_DWORD_FIELD(req, MAE_ACTION_SET_ALLOC_IN_FLAGS,
		    MAE_ACTION_SET_ALLOC_IN_MARK, 1);

		MCDI_IN_SET_DWORD(req,
		    MAE_ACTION_SET_ALLOC_IN_MARK_VALUE, spec->ema_mark_value);
	}

	MCDI_IN_SET_DWORD(req,
	    MAE_ACTION_SET_ALLOC_IN_DELIVER, spec->ema_deliver_mport.sel);

	MCDI_IN_SET_DWORD(req, MAE_ACTION_SET_ALLOC_IN_SRC_MAC_ID,
	    MC_CMD_MAE_MAC_ADDR_ALLOC_OUT_MAC_ID_NULL);
	MCDI_IN_SET_DWORD(req, MAE_ACTION_SET_ALLOC_IN_DST_MAC_ID,
	    MC_CMD_MAE_MAC_ADDR_ALLOC_OUT_MAC_ID_NULL);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail2;
	}

	if (req.emr_out_length_used < MC_CMD_MAE_ACTION_SET_ALLOC_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail3;
	}

	aset_id.id = MCDI_OUT_DWORD(req, MAE_ACTION_SET_ALLOC_OUT_AS_ID);
	if (aset_id.id == EFX_MAE_RSRC_ID_INVALID) {
		rc = ENOENT;
		goto fail4;
	}

	aset_idp->id = aset_id.id;

	return (0);

fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn			efx_rc_t
efx_mae_action_set_free(
	__in				efx_nic_t *enp,
	__in				const efx_mae_aset_id_t *aset_idp)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(enp);
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload,
	    MC_CMD_MAE_ACTION_SET_FREE_IN_LEN(1),
	    MC_CMD_MAE_ACTION_SET_FREE_OUT_LEN(1));
	efx_rc_t rc;

	if (encp->enc_mae_supported == B_FALSE) {
		rc = ENOTSUP;
		goto fail1;
	}

	req.emr_cmd = MC_CMD_MAE_ACTION_SET_FREE;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_MAE_ACTION_SET_FREE_IN_LEN(1);
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_MAE_ACTION_SET_FREE_OUT_LEN(1);

	MCDI_IN_SET_DWORD(req, MAE_ACTION_SET_FREE_IN_AS_ID, aset_idp->id);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail2;
	}

	if (req.emr_out_length_used < MC_CMD_MAE_ACTION_SET_FREE_OUT_LENMIN) {
		rc = EMSGSIZE;
		goto fail3;
	}

	if (MCDI_OUT_DWORD(req, MAE_ACTION_SET_FREE_OUT_FREED_AS_ID) !=
	    aset_idp->id) {
		/* Firmware failed to free the action set. */
		rc = EAGAIN;
		goto fail4;
	}

	return (0);

fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn			efx_rc_t
efx_mae_action_rule_insert(
	__in				efx_nic_t *enp,
	__in				const efx_mae_match_spec_t *spec,
	__in				const efx_mae_aset_list_id_t *asl_idp,
	__in				const efx_mae_aset_id_t *as_idp,
	__out				efx_mae_rule_id_t *ar_idp)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(enp);
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload,
	    MC_CMD_MAE_ACTION_RULE_INSERT_IN_LENMAX_MCDI2,
	    MC_CMD_MAE_ACTION_RULE_INSERT_OUT_LEN);
	efx_oword_t *rule_response;
	efx_mae_rule_id_t ar_id;
	size_t offset;
	efx_rc_t rc;

	EFX_STATIC_ASSERT(sizeof (ar_idp->id) ==
	    MC_CMD_MAE_ACTION_RULE_INSERT_OUT_AR_ID_LEN);

	EFX_STATIC_ASSERT(EFX_MAE_RSRC_ID_INVALID ==
	    MC_CMD_MAE_ACTION_RULE_INSERT_OUT_ACTION_RULE_ID_NULL);

	if (encp->enc_mae_supported == B_FALSE) {
		rc = ENOTSUP;
		goto fail1;
	}

	if (spec->emms_type != EFX_MAE_RULE_ACTION ||
	    (asl_idp != NULL && as_idp != NULL) ||
	    (asl_idp == NULL && as_idp == NULL)) {
		rc = EINVAL;
		goto fail2;
	}

	req.emr_cmd = MC_CMD_MAE_ACTION_RULE_INSERT;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_MAE_ACTION_RULE_INSERT_IN_LENMAX_MCDI2;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_MAE_ACTION_RULE_INSERT_OUT_LEN;

	EFX_STATIC_ASSERT(sizeof (*rule_response) <=
	    MC_CMD_MAE_ACTION_RULE_INSERT_IN_RESPONSE_LEN);
	offset = MC_CMD_MAE_ACTION_RULE_INSERT_IN_RESPONSE_OFST;
	rule_response = (efx_oword_t *)(payload + offset);
	EFX_POPULATE_OWORD_3(*rule_response,
	    MAE_ACTION_RULE_RESPONSE_ASL_ID,
	    (asl_idp != NULL) ? asl_idp->id : EFX_MAE_RSRC_ID_INVALID,
	    MAE_ACTION_RULE_RESPONSE_AS_ID,
	    (as_idp != NULL) ? as_idp->id : EFX_MAE_RSRC_ID_INVALID,
	    MAE_ACTION_RULE_RESPONSE_COUNTER_ID, EFX_MAE_RSRC_ID_INVALID);

	MCDI_IN_SET_DWORD(req, MAE_ACTION_RULE_INSERT_IN_PRIO, spec->emms_prio);

	/*
	 * Mask-value pairs have been stored in the byte order needed for the
	 * MCDI request and are thus safe to be copied directly to the buffer.
	 */
	EFX_STATIC_ASSERT(sizeof (spec->emms_mask_value_pairs.action) >=
	    MAE_FIELD_MASK_VALUE_PAIRS_LEN);
	offset = MC_CMD_MAE_ACTION_RULE_INSERT_IN_MATCH_CRITERIA_OFST;
	memcpy(payload + offset, spec->emms_mask_value_pairs.action,
	    MAE_FIELD_MASK_VALUE_PAIRS_LEN);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail3;
	}

	if (req.emr_out_length_used < MC_CMD_MAE_ACTION_RULE_INSERT_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail4;
	}

	ar_id.id = MCDI_OUT_DWORD(req, MAE_ACTION_RULE_INSERT_OUT_AR_ID);
	if (ar_id.id == EFX_MAE_RSRC_ID_INVALID) {
		rc = ENOENT;
		goto fail5;
	}

	ar_idp->id = ar_id.id;

	return (0);

fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn			efx_rc_t
efx_mae_action_rule_remove(
	__in				efx_nic_t *enp,
	__in				const efx_mae_rule_id_t *ar_idp)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(enp);
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload,
	    MC_CMD_MAE_ACTION_RULE_DELETE_IN_LEN(1),
	    MC_CMD_MAE_ACTION_RULE_DELETE_OUT_LEN(1));
	efx_rc_t rc;

	if (encp->enc_mae_supported == B_FALSE) {
		rc = ENOTSUP;
		goto fail1;
	}

	req.emr_cmd = MC_CMD_MAE_ACTION_RULE_DELETE;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_MAE_ACTION_RULE_DELETE_IN_LEN(1);
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_MAE_ACTION_RULE_DELETE_OUT_LEN(1);

	MCDI_IN_SET_DWORD(req, MAE_ACTION_RULE_DELETE_IN_AR_ID, ar_idp->id);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail2;
	}

	if (req.emr_out_length_used <
	    MC_CMD_MAE_ACTION_RULE_DELETE_OUT_LENMIN) {
		rc = EMSGSIZE;
		goto fail3;
	}

	if (MCDI_OUT_DWORD(req, MAE_ACTION_RULE_DELETE_OUT_DELETED_AR_ID) !=
	    ar_idp->id) {
		/* Firmware failed to delete the action rule. */
		rc = EAGAIN;
		goto fail4;
	}

	return (0);

fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

#endif /* EFSYS_OPT_MAE */
