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

	maep->em_max_n_action_prios =
	    MCDI_OUT_DWORD(req, MAE_GET_CAPS_OUT_ACTION_PRIOS);

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

	mcdi_field_ncaps = MCDI_OUT_DWORD(req, MAE_GET_OR_CAPS_OUT_COUNT);

	if (req.emr_out_length_used <
	    MC_CMD_MAE_GET_AR_CAPS_OUT_LEN(mcdi_field_ncaps)) {
		rc = EMSGSIZE;
		goto fail3;
	}

	if (mcdi_field_ncaps > field_ncaps) {
		rc = EMSGSIZE;
		goto fail4;
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

	ar_fcaps_size = maep->em_max_nfields * sizeof (*ar_fcaps);
	EFSYS_KMEM_ALLOC(enp->en_esip, ar_fcaps_size, ar_fcaps);
	if (ar_fcaps == NULL) {
		rc = ENOMEM;
		goto fail4;
	}

	maep->em_action_rule_field_caps_size = ar_fcaps_size;
	maep->em_action_rule_field_caps = ar_fcaps;

	rc = efx_mae_get_action_rule_caps(enp, maep->em_max_nfields, ar_fcaps);
	if (rc != 0)
		goto fail5;

	return (0);

fail5:
	EFSYS_PROBE(fail5);
	EFSYS_KMEM_FREE(enp->en_esip, ar_fcaps_size, ar_fcaps);
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

	emlp->eml_max_n_action_prios = maep->em_max_n_action_prios;

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
 */
typedef struct efx_mae_mv_desc_s {
	efx_mae_field_cap_id_t		emmd_field_cap_id;

	size_t				emmd_value_size;
	size_t				emmd_value_offset;
	size_t				emmd_mask_size;
	size_t				emmd_mask_offset;

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
		_endianness						\
	}

	EFX_MAE_MV_DESC(INGRESS_MPORT_SELECTOR, EFX_MAE_FIELD_LE),
	EFX_MAE_MV_DESC(ETHER_TYPE_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(ETH_SADDR_BE, EFX_MAE_FIELD_BE),
	EFX_MAE_MV_DESC(ETH_DADDR_BE, EFX_MAE_FIELD_BE),

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
	mportp->sel = dword.ed_u32[0];

	return (0);

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
	uint8_t *mvp;
	efx_rc_t rc;

	if (field_id >= EFX_MAE_FIELD_NIDS) {
		rc = EINVAL;
		goto fail1;
	}

	switch (spec->emms_type) {
	case EFX_MAE_RULE_ACTION:
		descp = &__efx_mae_action_rule_mv_desc_set[field_id];
		mvp = spec->emms_mask_value_pairs.action;
		break;
	default:
		rc = ENOTSUP;
		goto fail2;
	}

	if (value_size != descp->emmd_value_size) {
		rc = EINVAL;
		goto fail3;
	}

	if (mask_size != descp->emmd_mask_size) {
		rc = EINVAL;
		goto fail4;
	}

	if (descp->emmd_endianness == EFX_MAE_FIELD_BE) {
		/*
		 * The mask/value are in network (big endian) order.
		 * The MCDI request field is also big endian.
		 */
		memcpy(mvp + descp->emmd_value_offset, value, value_size);
		memcpy(mvp + descp->emmd_mask_offset, mask, mask_size);
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

#define	EFX_MASK_BIT_IS_SET(_mask, _mask_page_nbits, _bit)		\
	    ((_mask)[(_bit) / (_mask_page_nbits)] &			\
		    (1ULL << ((_bit) & ((_mask_page_nbits) - 1))))

static inline				boolean_t
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

static inline				boolean_t
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

static inline				boolean_t
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

	for (field_id = 0; field_id < desc_set_nentries; ++field_id) {
		const efx_mae_mv_desc_t *descp = &desc_setp[field_id];
		efx_mae_field_cap_id_t field_cap_id = descp->emmd_field_cap_id;
		const uint8_t *m_buf = mvp + descp->emmd_mask_offset;
		size_t m_size = descp->emmd_mask_size;

		if (m_size == 0)
			continue; /* Skip array gap */

		if (field_cap_id >= field_ncaps)
			break;

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
	[EFX_MAE_ACTION_DELIVER] = {
		.emad_add = efx_mae_action_set_add_deliver
	}
};

static const uint32_t efx_mae_action_ordered_map =
	(1U << EFX_MAE_ACTION_DELIVER);

static const uint32_t efx_mae_action_repeat_map = 0;

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
		uint32_t later_actions_mask =
			efx_mae_action_ordered_map &
			~(action_mask | (action_mask - 1));

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

	for (field_id = 0; field_id < desc_set_nentries; ++field_id) {
		const efx_mae_mv_desc_t *descp = &desc_setp[field_id];
		efx_mae_field_cap_id_t field_cap_id = descp->emmd_field_cap_id;

		if (descp->emmd_mask_size == 0)
			continue; /* Skip array gap */

		if (field_cap_id >= field_ncaps)
			break;

		if (field_caps[field_cap_id].emfc_mask_affects_class) {
			const uint8_t *lmaskp = mvpl + descp->emmd_mask_offset;
			const uint8_t *rmaskp = mvpr + descp->emmd_mask_offset;
			size_t mask_size = descp->emmd_mask_size;

			if (memcmp(lmaskp, rmaskp, mask_size) != 0) {
				have_same_class = B_FALSE;
				break;
			}
		}

		if (field_caps[field_cap_id].emfc_match_affects_class) {
			const uint8_t *lvalp = mvpl + descp->emmd_value_offset;
			const uint8_t *rvalp = mvpr + descp->emmd_value_offset;
			size_t value_size = descp->emmd_value_size;

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

#endif /* EFSYS_OPT_MAE */
