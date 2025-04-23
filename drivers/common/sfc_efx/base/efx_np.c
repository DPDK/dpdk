/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2025 Advanced Micro Devices, Inc.
 */
#include "efx.h"
#include "efx_impl.h"

			boolean_t
efx_np_supported(
	__in		efx_nic_t *enp)
{
	return (enp->en_family >= EFX_FAMILY_MEDFORD4) ? B_TRUE : B_FALSE;
}

static				void
efx_np_assign_legacy_props(
	__in			efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);

	memset(encp->enc_phy_revision, 0, sizeof (encp->enc_phy_revision));
	encp->enc_phy_type = 0;

#if EFSYS_OPT_NAMES
	memset(encp->enc_phy_name, 0, sizeof (encp->enc_phy_name));
#endif /* EFSYS_OPT_NAMES */

#if EFSYS_OPT_PHY_STATS
	encp->enc_mcdi_phy_stat_mask = 0;
#endif /* EFSYS_OPT_PHY_STATS */

#if EFSYS_OPT_PHY_FLAGS
	encp->enc_phy_flags_mask = 0;
#endif /* EFSYS_OPT_PHY_FLAGS */

#if EFSYS_OPT_BIST
	encp->enc_bist_mask = 0;
#endif /* EFSYS_OPT_BIST */

	encp->enc_mcdi_mdio_channel = 0;
	encp->enc_port = 0;
}

static	__checkReturn		efx_rc_t
efx_np_get_assigned_handle(
	__in			efx_nic_t *enp,
	__out			efx_np_handle_t *nphp)
{
	EFX_MCDI_DECLARE_BUF(payload,
	    MC_CMD_GET_ASSIGNED_PORT_HANDLE_IN_LEN,
	    MC_CMD_GET_ASSIGNED_PORT_HANDLE_OUT_LEN);
	efx_mcdi_req_t req;
	efx_rc_t rc;

	req.emr_out_length = MC_CMD_GET_ASSIGNED_PORT_HANDLE_OUT_LEN;
	req.emr_in_length = MC_CMD_GET_ASSIGNED_PORT_HANDLE_IN_LEN;
	req.emr_cmd = MC_CMD_GET_ASSIGNED_PORT_HANDLE;
	req.emr_out_buf = payload;
	req.emr_in_buf = payload;

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_GET_ASSIGNED_PORT_HANDLE_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	*nphp = MCDI_OUT_DWORD(req, GET_ASSIGNED_PORT_HANDLE_OUT_PORT_HANDLE);
	return (0);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

struct efx_np_cap_map {
	uint16_t	encm_hw;
	uint16_t	encm_sw;
};

static const struct efx_np_cap_map efx_np_cap_map_tech[] = {
	/* 1G */
	{ MC_CMD_ETH_TECH_1000BASEKX, EFX_PHY_CAP_1000FDX },
	{ MC_CMD_ETH_TECH_1000BASEX, EFX_PHY_CAP_1000FDX },

	/* 10G */
	{ MC_CMD_ETH_TECH_10GBASE_KR, EFX_PHY_CAP_10000FDX },
	{ MC_CMD_ETH_TECH_10GBASE_CR, EFX_PHY_CAP_10000FDX },
	{ MC_CMD_ETH_TECH_10GBASE_SR, EFX_PHY_CAP_10000FDX },
	{ MC_CMD_ETH_TECH_10GBASE_LR, EFX_PHY_CAP_10000FDX },
	{ MC_CMD_ETH_TECH_10GBASE_LRM, EFX_PHY_CAP_10000FDX },
	{ MC_CMD_ETH_TECH_10GBASE_ER, EFX_PHY_CAP_10000FDX },

	/* 25GBASE */
	{ MC_CMD_ETH_TECH_25GBASE_CR, EFX_PHY_CAP_25000FDX },
	{ MC_CMD_ETH_TECH_25GBASE_KR, EFX_PHY_CAP_25000FDX },
	{ MC_CMD_ETH_TECH_25GBASE_SR, EFX_PHY_CAP_25000FDX },
	{ MC_CMD_ETH_TECH_25GBASE_LR_ER, EFX_PHY_CAP_25000FDX },

	/* 40G */
	{ MC_CMD_ETH_TECH_40GBASE_KR4, EFX_PHY_CAP_40000FDX },
	{ MC_CMD_ETH_TECH_40GBASE_CR4, EFX_PHY_CAP_40000FDX },
	{ MC_CMD_ETH_TECH_40GBASE_SR4, EFX_PHY_CAP_40000FDX },
	{ MC_CMD_ETH_TECH_40GBASE_LR4, EFX_PHY_CAP_40000FDX },

	/* 50G */
	{ MC_CMD_ETH_TECH_50GBASE_CR2, EFX_PHY_CAP_50000FDX },
	{ MC_CMD_ETH_TECH_50GBASE_KR2, EFX_PHY_CAP_50000FDX },
	{ MC_CMD_ETH_TECH_50GBASE_SR2, EFX_PHY_CAP_50000FDX },
	{ MC_CMD_ETH_TECH_50GBASE_KR, EFX_PHY_CAP_50000FDX },
	{ MC_CMD_ETH_TECH_50GBASE_SR, EFX_PHY_CAP_50000FDX },
	{ MC_CMD_ETH_TECH_50GBASE_CR, EFX_PHY_CAP_50000FDX },
	{ MC_CMD_ETH_TECH_50GBASE_LR_ER_FR, EFX_PHY_CAP_50000FDX },
	{ MC_CMD_ETH_TECH_50GBASE_DR, EFX_PHY_CAP_50000FDX },

	/* 100G */
	{ MC_CMD_ETH_TECH_100GBASE_KR4, EFX_PHY_CAP_100000FDX },
	{ MC_CMD_ETH_TECH_100GBASE_SR4, EFX_PHY_CAP_100000FDX },
	{ MC_CMD_ETH_TECH_100GBASE_CR4, EFX_PHY_CAP_100000FDX },
	{ MC_CMD_ETH_TECH_100GBASE_LR4_ER4, EFX_PHY_CAP_100000FDX },
	{ MC_CMD_ETH_TECH_100GBASE_KR2, EFX_PHY_CAP_100000FDX },
	{ MC_CMD_ETH_TECH_100GBASE_SR2, EFX_PHY_CAP_100000FDX },
	{ MC_CMD_ETH_TECH_100GBASE_CR2, EFX_PHY_CAP_100000FDX },
	{ MC_CMD_ETH_TECH_100GBASE_LR2_ER2_FR2, EFX_PHY_CAP_100000FDX },
	{ MC_CMD_ETH_TECH_100GBASE_DR2, EFX_PHY_CAP_100000FDX },
	{ MC_CMD_ETH_TECH_100GBASE_KR, EFX_PHY_CAP_100000FDX },
	{ MC_CMD_ETH_TECH_100GBASE_SR, EFX_PHY_CAP_100000FDX },
	{ MC_CMD_ETH_TECH_100GBASE_LR_ER_FR, EFX_PHY_CAP_100000FDX },
	{ MC_CMD_ETH_TECH_100GBASE_CR, EFX_PHY_CAP_100000FDX },
	{ MC_CMD_ETH_TECH_100GBASE_DR, EFX_PHY_CAP_100000FDX },
	{ MC_CMD_ETH_TECH_100GBASE_CR10, EFX_PHY_CAP_100000FDX },

	/* 200G */
	{ MC_CMD_ETH_TECH_200GBASE_KR4, EFX_PHY_CAP_200000FDX },
	{ MC_CMD_ETH_TECH_200GBASE_SR4, EFX_PHY_CAP_200000FDX },
	{ MC_CMD_ETH_TECH_200GBASE_LR4_ER4_FR4, EFX_PHY_CAP_200000FDX },
	{ MC_CMD_ETH_TECH_200GBASE_DR4, EFX_PHY_CAP_200000FDX },
	{ MC_CMD_ETH_TECH_200GBASE_CR4, EFX_PHY_CAP_200000FDX },
	{ MC_CMD_ETH_TECH_200GBASE_KR2, EFX_PHY_CAP_200000FDX },
	{ MC_CMD_ETH_TECH_200GBASE_SR2, EFX_PHY_CAP_200000FDX },
	{ MC_CMD_ETH_TECH_200GBASE_LR2_ER2_FR2, EFX_PHY_CAP_200000FDX },
	{ MC_CMD_ETH_TECH_200GBASE_DR2, EFX_PHY_CAP_200000FDX },
	{ MC_CMD_ETH_TECH_200GBASE_CR2, EFX_PHY_CAP_200000FDX },
};

static const struct efx_np_cap_map efx_np_cap_map_fec[] = {
	{ MC_CMD_FEC_BASER, EFX_PHY_CAP_BASER_FEC },
	{ MC_CMD_FEC_RS, EFX_PHY_CAP_RS_FEC },
	{ MC_CMD_FEC_IEEE_RS_INT, EFX_PHY_CAP_IEEE_RS_INT_FEC },
	{ MC_CMD_FEC_ETCS_RS_LL, EFX_PHY_CAP_ETCS_RS_LL_FEC },
};

static const struct efx_np_cap_map efx_np_cap_map_fec_req[] = {
	{ MC_CMD_FEC_BASER, EFX_PHY_CAP_BASER_FEC_REQUESTED },
	{ MC_CMD_FEC_RS, EFX_PHY_CAP_RS_FEC_REQUESTED },
	{ MC_CMD_FEC_IEEE_RS_INT, EFX_PHY_CAP_IEEE_RS_INT_FEC_REQUESTED },
	{ MC_CMD_FEC_ETCS_RS_LL, EFX_PHY_CAP_ETCS_RS_LL_FEC_REQUESTED },
};

static const struct efx_np_cap_map efx_np_cap_map_pause[] = {
	{ MC_CMD_PAUSE_MODE_AN_ASYM_DIR, EFX_PHY_CAP_ASYM },
	{ MC_CMD_PAUSE_MODE_AN_PAUSE, EFX_PHY_CAP_PAUSE },
};

#define	CAP_BYTE(_map)	((_map)->encm_hw / CHAR_BIT)

#define	CAP_VLD(_map, _data_nbytes)	(CAP_BYTE(_map) < (_data_nbytes))

#define	CAP_FLAG(_map)	(1U << ((_map)->encm_hw % CHAR_BIT))

#define	CAP_SUP(_map, _data)						\
	(((_data)[CAP_BYTE(_map)] & CAP_FLAG(_map)) == CAP_FLAG(_map))

#define	FOREACH_SUP_CAP(_map, _map_nentries, _data, _data_nbytes)	\
	for (unsigned int _i = 0; _i < (_map_nentries); ++_i, ++(_map))	\
		if (CAP_VLD(_map, _data_nbytes) && CAP_SUP(_map, _data))

static					void
efx_np_cap_mask_hw_to_sw(
	__in_ecount(hw_sw_map_nentries)	const struct efx_np_cap_map *hw_sw_map,
	__in				unsigned int hw_sw_map_nentries,
	__in_bcount(hw_cap_data_nbytes)	const uint8_t *hw_cap_data,
	__in				size_t hw_cap_data_nbytes,
	__out				uint32_t *sw_cap_maskp)
{
	FOREACH_SUP_CAP(hw_sw_map, hw_sw_map_nentries,
	    hw_cap_data, hw_cap_data_nbytes) {
		*sw_cap_maskp |= 1U << hw_sw_map->encm_sw;
	}
}

/*
 * Convert the given fraction of raw HW netport capability data (identified by
 * the given section name of the MCDI response) to the EFX mask representation,
 * in accordance with the specified collection of HW-to-SW capability mappings.
 */
#define	EFX_NP_CAP_MASK_HW_TO_SW(					\
	    _hw_sw_cap_map, _hw_cap_section, _hw_cap_data, _sw_maskp)	\
	efx_np_cap_mask_hw_to_sw((_hw_sw_cap_map),			\
	    EFX_ARRAY_SIZE(_hw_sw_cap_map),				\
	    MCDI_STRUCT_MEMBER((_hw_cap_data), const uint8_t,		\
		    MC_CMD_##_hw_cap_section),				\
	    MC_CMD_##_hw_cap_section##_LEN, (_sw_maskp))

static				void
efx_np_cap_hw_data_to_sw_mask(
	__in			const uint8_t *hw_data,
	__out			uint32_t *sw_maskp)
{
	EFX_NP_CAP_MASK_HW_TO_SW(efx_np_cap_map_tech, ETH_AN_FIELDS_TECH_MASK,
	    hw_data, sw_maskp);

	EFX_NP_CAP_MASK_HW_TO_SW(efx_np_cap_map_fec, ETH_AN_FIELDS_FEC_MASK,
	    hw_data, sw_maskp);

	EFX_NP_CAP_MASK_HW_TO_SW(efx_np_cap_map_fec_req, ETH_AN_FIELDS_FEC_REQ,
	    hw_data, sw_maskp);

	EFX_NP_CAP_MASK_HW_TO_SW(efx_np_cap_map_pause, ETH_AN_FIELDS_PAUSE_MASK,
	    hw_data, sw_maskp);
}

static	__checkReturn		efx_rc_t
efx_np_get_fixed_port_props(
	__in			efx_nic_t *enp,
	__in			efx_np_handle_t nph,
	__out_opt		uint8_t *sup_cap_rawp,
	__out_opt		uint32_t *sup_cap_maskp,
	__out_opt		efx_qword_t *loopback_cap_maskp)
{
	EFX_MCDI_DECLARE_BUF(payload,
	    MC_CMD_GET_FIXED_PORT_PROPERTIES_IN_LEN,
	    MC_CMD_GET_FIXED_PORT_PROPERTIES_OUT_V2_LEN);
	const uint8_t *cap_data;
	efx_mcdi_req_t req;
	efx_rc_t rc;

	req.emr_out_length = MC_CMD_GET_FIXED_PORT_PROPERTIES_OUT_V2_LEN;
	req.emr_in_length = MC_CMD_GET_FIXED_PORT_PROPERTIES_IN_LEN;
	req.emr_cmd = MC_CMD_GET_FIXED_PORT_PROPERTIES;
	req.emr_out_buf = payload;
	req.emr_in_buf = payload;

	MCDI_IN_SET_DWORD(req, GET_FIXED_PORT_PROPERTIES_IN_PORT_HANDLE, nph);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used <
	    MC_CMD_GET_FIXED_PORT_PROPERTIES_OUT_V2_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	cap_data = MCDI_OUT2(req, const uint8_t,
		    GET_FIXED_PORT_PROPERTIES_OUT_ABILITIES);

	if (sup_cap_maskp != NULL)
		efx_np_cap_hw_data_to_sw_mask(cap_data, sup_cap_maskp);

	if (sup_cap_rawp != NULL) {
		memcpy(sup_cap_rawp,
		    MCDI_OUT2(req, const uint8_t,
			    GET_FIXED_PORT_PROPERTIES_OUT_ABILITIES),
		    MC_CMD_GET_FIXED_PORT_PROPERTIES_OUT_ABILITIES_LEN);
	}

	if (loopback_cap_maskp != NULL) {
		*loopback_cap_maskp = *MCDI_OUT2(req, efx_qword_t,
		    GET_FIXED_PORT_PROPERTIES_OUT_V2_LOOPBACK_MODES_MASK_V2);
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn	efx_rc_t
efx_np_link_state(
	__in		efx_nic_t *enp,
	__in		efx_np_handle_t nph,
	__out		efx_np_link_state_t *lsp)
{
	EFX_MCDI_DECLARE_BUF(payload,
	    MC_CMD_LINK_STATE_IN_LEN,
	    MC_CMD_LINK_STATE_OUT_V3_LEN);
	uint32_t status_flags;
	efx_mcdi_req_t req;
	uint32_t v3_flags;
	efx_rc_t rc;

	req.emr_out_length = MC_CMD_LINK_STATE_OUT_V3_LEN;
	req.emr_in_length = MC_CMD_LINK_STATE_IN_LEN;
	req.emr_cmd = MC_CMD_LINK_STATE;
	req.emr_out_buf = payload;
	req.emr_in_buf = payload;

	MCDI_IN_SET_DWORD(req, LINK_STATE_IN_PORT_HANDLE, nph);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_LINK_STATE_OUT_V3_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	status_flags = MCDI_OUT_DWORD(req, LINK_STATE_OUT_STATUS_FLAGS_LO);
	v3_flags = MCDI_OUT_DWORD(req, LINK_STATE_OUT_V3_FLAGS);
	memset(lsp, 0, sizeof (*lsp));

	if (status_flags & (1U << MC_CMD_LINK_STATUS_FLAGS_AN_ABLE) &&
	    MCDI_OUT_DWORD(req, LINK_STATE_OUT_V2_LOCAL_AN_SUPPORT) !=
	    MC_CMD_AN_NONE)
		lsp->enls_an_supported = B_TRUE;

	if (v3_flags & (1U << MC_CMD_LINK_STATE_OUT_V3_FULL_DUPLEX_LBN))
		lsp->enls_fd = B_TRUE;

	if (status_flags & (1U << MC_CMD_LINK_STATUS_FLAGS_LINK_UP))
		lsp->enls_up = B_TRUE;

	lsp->enls_speed = MCDI_OUT_DWORD(req, LINK_STATE_OUT_V3_LINK_SPEED);
	lsp->enls_fec = MCDI_OUT_BYTE(req, LINK_STATE_OUT_FEC_MODE);

#if EFSYS_OPT_LOOPBACK
	switch (MCDI_OUT_BYTE(req, LINK_STATE_OUT_LOOPBACK)) {
	case MC_CMD_LOOPBACK_V2_NONE:
		lsp->enls_loopback = EFX_LOOPBACK_OFF;
		break;
	case MC_CMD_LOOPBACK_V2_AUTO:
		lsp->enls_loopback = EFX_LOOPBACK_DATA;
		break;
	case MC_CMD_LOOPBACK_V2_POST_PCS:
		lsp->enls_loopback = EFX_LOOPBACK_PCS;
		break;
	default:
		rc = EINVAL;
		goto fail3;
	}
#else /* ! EFSYS_OPT_LOOPBACK */
	_NOTE(ARGUNUSED(lbp))
#endif /* EFSYS_OPT_LOOPBACK */

	if (lsp->enls_an_supported != B_FALSE)
		lsp->enls_adv_cap_mask |= 1U << EFX_PHY_CAP_AN;

	efx_np_cap_hw_data_to_sw_mask(
	    MCDI_OUT2(req, const uint8_t, LINK_STATE_OUT_ADVERTISED_ABILITIES),
	    &lsp->enls_adv_cap_mask);

	if (lsp->enls_an_supported != B_FALSE)
		lsp->enls_lp_cap_mask |= 1U << EFX_PHY_CAP_AN;

	efx_np_cap_hw_data_to_sw_mask(
	    MCDI_OUT2(req, const uint8_t,
		    LINK_STATE_OUT_LINK_PARTNER_ABILITIES),
	    &lsp->enls_lp_cap_mask);

	return (0);

#if EFSYS_OPT_LOOPBACK
fail3:
	EFSYS_PROBE(fail3);
#endif /* EFSYS_OPT_LOOPBACK */

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

#if EFSYS_OPT_LOOPBACK
static	__checkReturn		efx_rc_t
efx_np_sw_link_mode_to_cap(
	__in			efx_link_mode_t link_mode,
	__out			uint16_t *capp)
{
	switch (link_mode) {
	case EFX_LINK_1000FDX:
		*capp = EFX_PHY_CAP_1000FDX;
		break;
	case EFX_LINK_10000FDX:
		*capp = EFX_PHY_CAP_10000FDX;
		break;
	case EFX_LINK_40000FDX:
		*capp = EFX_PHY_CAP_40000FDX;
		break;
	case EFX_LINK_25000FDX:
		*capp = EFX_PHY_CAP_25000FDX;
		break;
	case EFX_LINK_50000FDX:
		*capp = EFX_PHY_CAP_50000FDX;
		break;
	case EFX_LINK_100000FDX:
		*capp = EFX_PHY_CAP_100000FDX;
		break;
	case EFX_LINK_200000FDX:
		*capp = EFX_PHY_CAP_200000FDX;
		break;
	default:
		return (EINVAL);
	}

	return (0);
}

static					void
efx_np_cap_enum_sw_to_hw(
	__in_ecount(hw_sw_map_nentries)	const struct efx_np_cap_map *hw_sw_map,
	__in				unsigned int hw_sw_map_nentries,
	__in_bcount(hw_cap_data_nbytes)	const uint8_t *hw_cap_data,
	__in				size_t hw_cap_data_nbytes,
	__in				uint16_t enum_sw,
	__out				boolean_t *supportedp,
	__out_opt			uint16_t *enum_hwp)
{
	FOREACH_SUP_CAP(hw_sw_map, hw_sw_map_nentries,
	    hw_cap_data, hw_cap_data_nbytes) {
		if (hw_sw_map->encm_sw != enum_sw)
			continue;

		if (enum_hwp != NULL)
			*enum_hwp = hw_sw_map->encm_hw;

		*supportedp = B_TRUE;
		return;
	}

	*supportedp = B_FALSE;
}

/*
 * Convert the given EFX PHY capability enum value to the HW counterpart,
 * provided that the capability is supported by the HW, where the latter
 * is detected from the given fraction of raw HW netport capability data.
 *
 * As the mapping of a capability from EFX to HW can be one to many, use
 * the first supported HW capability bit, in accordance with the HW data.
 */
#define	EFX_NP_CAP_ENUM_SW_TO_HW(						\
	    _hw_sw_cap_map, _hw_cap_section, _hw_cap_data,		\
	    _enum_sw, _supportedp, _enum_hwp)				\
	efx_np_cap_enum_sw_to_hw((_hw_sw_cap_map),			\
	    EFX_ARRAY_SIZE(_hw_sw_cap_map),				\
	    MCDI_STRUCT_MEMBER((_hw_cap_data), const uint8_t,		\
		    MC_CMD_##_hw_cap_section),				\
	    MC_CMD_##_hw_cap_section##_LEN, (_enum_sw),			\
	    (_supportedp), (_enum_hwp))

static				void
efx_np_assign_loopback_props(
	__in			efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_port_t *epp = &(enp->en_port);
	efx_qword_t lbm_off;
	efx_qword_t lbm_sup;
	unsigned int i;

	EFX_ZERO_QWORD(lbm_off);
	EFX_SET_QWORD_BIT(lbm_off, EFX_LOOPBACK_OFF);

	/*
	 * Netport MCDI capable NICs support loopback modes which are
	 * generalisations of the existing modes that specify roughly
	 * where in the processing chain the loopback occurs, without
	 * the need to refer to the specific technology. Provide some
	 * to users under the guise of older technology-specific ones.
	 */
	EFX_ZERO_QWORD(lbm_sup);
	EFX_SET_QWORD_BIT(lbm_sup, EFX_LOOPBACK_OFF);

	if (EFX_TEST_QWORD_BIT(epp->ep_np_loopback_cap_mask,
			    MC_CMD_LOOPBACK_V2_AUTO))
		EFX_SET_QWORD_BIT(lbm_sup, EFX_LOOPBACK_DATA);

	if (EFX_TEST_QWORD_BIT(epp->ep_np_loopback_cap_mask,
			    MC_CMD_LOOPBACK_V2_POST_PCS))
		EFX_SET_QWORD_BIT(lbm_sup, EFX_LOOPBACK_PCS);

	for (i = 0; i < EFX_ARRAY_SIZE(encp->enc_loopback_types); ++i) {
		boolean_t supported = B_FALSE;
		uint16_t cap_enum_sw;
		efx_rc_t rc;

		rc = efx_np_sw_link_mode_to_cap(i, &cap_enum_sw);
		if (rc != 0) {
			/* No support for this link mode => no loopbacks. */
			encp->enc_loopback_types[i] = lbm_off;
			continue;
		}

		EFX_NP_CAP_ENUM_SW_TO_HW(efx_np_cap_map_tech,
		    ETH_AN_FIELDS_TECH_MASK, epp->ep_np_cap_data_raw,
		    cap_enum_sw, &supported, NULL);

		if (supported != B_FALSE)
			encp->enc_loopback_types[i] = lbm_sup;
		else
			encp->enc_loopback_types[i] = lbm_off;
	}
}
#endif /* EFSYS_OPT_LOOPBACK */

#if EFSYS_OPT_MAC_STATS
/* HW statistic IDs, as per MC_CMD_MAC_STATISTICS_DESCRIPTOR format. */
#define	EFX_NP_HW_STAT_ID(_src, _idx)					\
	(((MC_CMD_STAT_ID_##_src) << MC_CMD_STAT_ID_SOURCE_ID_LBN) |	\
	    ((uint32_t)(MC_CMD_STAT_ID_##_idx) <<			\
		    MC_CMD_STAT_ID_MAC_STAT_ID_LBN))

/*
 * Mapping between EFX statistic IDs (array indices) and their HW counterparts.
 *
 * This is used in conjunction with HW statistic descriptors bearing DMA field
 * IDs (offsets into the DMA buffer) to provide a SW lookup table for readings.
 *
 * From the HW perspective, statistics come from MAC and PHY, hence two macros.
 */
static const efx_np_stat_t efx_np_mac_stat_map[] = {
#define	EFX_NP_STAT_MAC(_hw, _sw)				\
	[EFX_MAC_##_sw] = {					\
		.ens_hw_id = EFX_NP_HW_STAT_ID(MAC, _hw),	\
		.ens_valid = B_TRUE,				\
	}

	EFX_NP_STAT_MAC(TX_PKTS, TX_PKTS),
	EFX_NP_STAT_MAC(TX_PAUSE_PKTS, TX_PAUSE_PKTS),
	EFX_NP_STAT_MAC(TX_UNICAST_PKTS, TX_UNICST_PKTS),
	EFX_NP_STAT_MAC(TX_MULTICAST_PKTS, TX_MULTICST_PKTS),
	EFX_NP_STAT_MAC(TX_BROADCAST_PKTS, TX_BRDCST_PKTS),
	EFX_NP_STAT_MAC(TX_BYTES, TX_OCTETS),
	EFX_NP_STAT_MAC(TX_64_PKTS, TX_LE_64_PKTS),
	EFX_NP_STAT_MAC(TX_65_TO_127_PKTS, TX_65_TO_127_PKTS),
	EFX_NP_STAT_MAC(TX_128_TO_255_PKTS, TX_128_TO_255_PKTS),
	EFX_NP_STAT_MAC(TX_256_TO_511_PKTS, TX_256_TO_511_PKTS),
	EFX_NP_STAT_MAC(TX_512_TO_1023_PKTS, TX_512_TO_1023_PKTS),
	EFX_NP_STAT_MAC(TX_1024_TO_15XX_PKTS, TX_1024_TO_15XX_PKTS),
	EFX_NP_STAT_MAC(TX_15XX_TO_JUMBO_PKTS, TX_GE_15XX_PKTS),
	EFX_NP_STAT_MAC(RX_PKTS, RX_PKTS),
	EFX_NP_STAT_MAC(RX_PAUSE_PKTS, RX_PAUSE_PKTS),
	EFX_NP_STAT_MAC(RX_UNICAST_PKTS, RX_UNICST_PKTS),
	EFX_NP_STAT_MAC(RX_MULTICAST_PKTS, RX_MULTICST_PKTS),
	EFX_NP_STAT_MAC(RX_BROADCAST_PKTS, RX_BRDCST_PKTS),
	EFX_NP_STAT_MAC(RX_BYTES, RX_OCTETS),
	EFX_NP_STAT_MAC(RX_64_PKTS, RX_LE_64_PKTS),
	EFX_NP_STAT_MAC(RX_65_TO_127_PKTS, RX_65_TO_127_PKTS),
	EFX_NP_STAT_MAC(RX_128_TO_255_PKTS, RX_128_TO_255_PKTS),
	EFX_NP_STAT_MAC(RX_256_TO_511_PKTS, RX_256_TO_511_PKTS),
	EFX_NP_STAT_MAC(RX_512_TO_1023_PKTS, RX_512_TO_1023_PKTS),
	EFX_NP_STAT_MAC(RX_1024_TO_15XX_PKTS, RX_1024_TO_15XX_PKTS),
	EFX_NP_STAT_MAC(RX_15XX_TO_JUMBO_PKTS, RX_GE_15XX_PKTS),
	EFX_NP_STAT_MAC(RX_BAD_FCS_PKTS, RX_FCS_ERRORS),
	EFX_NP_STAT_MAC(RX_SYMBOL_ERROR_PKTS, RX_SYMBOL_ERRORS),
	EFX_NP_STAT_MAC(RX_ALIGN_ERROR_PKTS, RX_ALIGN_ERRORS),
	EFX_NP_STAT_MAC(RX_INTERNAL_ERROR_PKTS, RX_INTERNAL_ERRORS),
	EFX_NP_STAT_MAC(RX_JABBER_PKTS, RX_JABBER_PKTS),
	EFX_NP_STAT_MAC(RX_NODESC_DROPS, RX_NODESC_DROP_CNT),
	EFX_NP_STAT_MAC(RX_MATCH_FAULT, RX_MATCH_FAULT),

#undef EFX_NP_STAT_MAC

#define	EFX_NP_STAT_PHY(_hw, _sw)				\
	[EFX_MAC_##_sw] = {					\
		.ens_hw_id = EFX_NP_HW_STAT_ID(PHY, _hw),	\
		.ens_valid = B_TRUE,				\
	}

	EFX_NP_STAT_PHY(FEC_UNCORRECTED_ERRORS, FEC_UNCORRECTED_ERRORS),
	EFX_NP_STAT_PHY(FEC_CORRECTED_ERRORS, FEC_CORRECTED_ERRORS),
	EFX_NP_STAT_PHY(FEC_CORRECTED_SYMBOLS_LANE0,
	    FEC_CORRECTED_SYMBOLS_LANE0),
	EFX_NP_STAT_PHY(FEC_CORRECTED_SYMBOLS_LANE1,
	    FEC_CORRECTED_SYMBOLS_LANE1),
	EFX_NP_STAT_PHY(FEC_CORRECTED_SYMBOLS_LANE2,
	    FEC_CORRECTED_SYMBOLS_LANE2),
	EFX_NP_STAT_PHY(FEC_CORRECTED_SYMBOLS_LANE3,
	    FEC_CORRECTED_SYMBOLS_LANE3),

#undef EFX_NP_STAT_PHY
};
#undef EFX_NP_HW_STAT_ID

/* See efx_np_stats_describe() below. */
static					void
efx_np_stat_describe(
	__in				uint8_t *hw_entry_buf,
	__in				unsigned int lut_nentries,
	__out_ecount_opt(lut_nentries)	efx_np_stat_t *lut)
{
	const efx_np_stat_t *map;
	efx_mac_stat_t sw_id;
	uint32_t hw_id;

	hw_id = MCDI_STRUCT_DWORD(hw_entry_buf, MC_CMD_STAT_DESC_STAT_ID);

	for (sw_id = 0; sw_id < EFX_ARRAY_SIZE(efx_np_mac_stat_map); ++sw_id) {
		map = &efx_np_mac_stat_map[sw_id];

		if (map->ens_valid != B_FALSE && map->ens_hw_id == hw_id)
			goto found;
	}

	/* The statistic is unknown to EFX. */
	return;

found:
	if (sw_id >= lut_nentries) {
		/*
		 * Static mapping size and the size of lookup
		 * table are out-of-sync. Should never happen.
		 */
		return;
	}

	lut[sw_id].ens_dma_fld =
	    MCDI_STRUCT_WORD(hw_entry_buf, MC_CMD_STAT_DESC_STAT_INDEX);
	lut[sw_id].ens_valid = B_TRUE;
	lut[sw_id].ens_hw_id = hw_id;
}

/*
 * Get a fraction of statistic descriptors from the running FW, starting from
 * the given HW ID offset, and fill DMA buffer field IDs in the corresponding
 * entries of the software lookup table that will be used to get the readings.
 */
static	__checkReturn			efx_rc_t
efx_np_stats_describe(
	__in				efx_nic_t *enp,
	__in				efx_np_handle_t nph,
	__in				uint32_t req_ofst,
	__in				unsigned int lut_nentries,
	__out_ecount_opt(lut_nentries)	efx_np_stat_t *lut,
	__out_opt			uint32_t *nprocessedp,
	__out_opt			uint32_t *nstats_maxp)
{
	EFX_MCDI_DECLARE_BUF(payload,
	    MC_CMD_MAC_STATISTICS_DESCRIPTOR_IN_LEN,
	    MC_CMD_MAC_STATISTICS_DESCRIPTOR_OUT_LENMAX_MCDI2);
	efx_port_t *epp = &(enp->en_port);
	uint32_t nprocessed;
	efx_mcdi_req_t req;
	uint8_t *entries;
	uint32_t stride;
	unsigned int i;
	size_t out_sz;
	efx_rc_t rc;

	req.emr_out_length = MC_CMD_MAC_STATISTICS_DESCRIPTOR_OUT_LENMAX_MCDI2;
	req.emr_in_length = MC_CMD_MAC_STATISTICS_DESCRIPTOR_IN_LEN;
	req.emr_cmd = MC_CMD_MAC_STATISTICS_DESCRIPTOR;
	req.emr_out_buf = payload;
	req.emr_in_buf = payload;

	MCDI_IN_SET_DWORD(req, MAC_STATISTICS_DESCRIPTOR_IN_PORT_HANDLE, nph);
	MCDI_IN_SET_DWORD(req, MAC_STATISTICS_DESCRIPTOR_IN_OFFSET, req_ofst);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	out_sz = req.emr_out_length_used;
	if (out_sz < MC_CMD_MAC_STATISTICS_DESCRIPTOR_OUT_LENMIN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	if (nstats_maxp != NULL) {
		*nstats_maxp = MCDI_OUT_DWORD(req,
		    MAC_STATISTICS_DESCRIPTOR_OUT_DMA_BUFFER_SIZE) /
		    sizeof (efx_qword_t);
	}

	if (lut_nentries == 0 || lut == NULL || nprocessedp == NULL)
		return (0);

	stride = MCDI_OUT_DWORD(req, MAC_STATISTICS_DESCRIPTOR_OUT_ENTRY_SIZE);
	nprocessed = MC_CMD_MAC_STATISTICS_DESCRIPTOR_OUT_ENTRIES_NUM(out_sz);
	if (nprocessed == 0) {
		rc = EMSGSIZE;
		goto fail3;
	}

	entries = MCDI_OUT2(req, uint8_t,
	    MAC_STATISTICS_DESCRIPTOR_OUT_ENTRIES);

	for (i = 0; i < nprocessed; ++i)
		efx_np_stat_describe(entries + i * stride, lut_nentries, lut);

	*nprocessedp = nprocessed;
	return (0);

fail3:
	EFSYS_PROBE(fail3);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

static	__checkReturn	efx_rc_t
efx_np_stats_assign(
	__in		efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_port_t *epp = &(enp->en_port);
	unsigned int lut_nentries;
	uint32_t nprocessed = 0;
	efx_mac_stat_t sw_id;
	efx_rc_t rc;

	memset(epp->ep_np_mac_stat_lut, 0, sizeof (epp->ep_np_mac_stat_lut));
	lut_nentries = EFX_ARRAY_SIZE(epp->ep_np_mac_stat_lut);

	/*
	 * First get encp->enc_mac_stats_nstats from the firmware.
	 *
	 * Do not limit encp->enc_mac_stats_nstats by the size of
	 * epp->ep_np_mac_stat_lut, because the former is used to
	 * allocate DMA buffer by client drivers, which must have
	 * its size match expectations of the running MC firmware.
	 */
	rc = efx_np_stats_describe(enp, epp->ep_np_handle, 0, 0, NULL,
			    NULL, &encp->enc_mac_stats_nstats);
	if (rc != 0)
		goto fail1;

	/* Then process the actual descriptor data. */
	while (nprocessed < encp->enc_mac_stats_nstats) {
		uint32_t batch;

		rc = efx_np_stats_describe(enp, epp->ep_np_handle, nprocessed,
		    lut_nentries, epp->ep_np_mac_stat_lut, &batch, NULL);
		if (rc != 0)
			goto fail2;

		nprocessed += batch;

		if (batch == 0) {
			/* Failed to supply all descriptors. */
			rc = EMSGSIZE;
			goto fail3;
		}
	}

	sw_id = EFX_MAC_FEC_UNCORRECTED_ERRORS;
	if (epp->ep_np_mac_stat_lut[sw_id].ens_valid == B_FALSE)
		encp->enc_fec_counters = B_FALSE;
	else
		encp->enc_fec_counters = B_TRUE;

	encp->enc_hlb_counters = B_FALSE;
	return (0);

fail3:
	EFSYS_PROBE(fail3);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}
#endif /* EFSYS_OPT_MAC_STATS */

	__checkReturn	efx_rc_t
efx_np_attach(
	__in		efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_port_t *epp = &(enp->en_port);
	efx_np_link_state_t ls;
	efx_np_mac_state_t ms;
	efx_rc_t rc;

	if (efx_np_supported(enp) == B_FALSE)
		return (0);

	/*
	 * Some EFX properties are mostly leftover from Siena era
	 * and we prefer to initialise those to harmless defaults.
	 */
	efx_np_assign_legacy_props(enp);

#if EFSYS_OPT_PHY_LED_CONTROL
	encp->enc_led_mask = 1U << EFX_PHY_LED_DEFAULT;
#endif /* EFSYS_OPT_PHY_LED_CONTROL */

	epp->ep_fixed_port_type = EFX_PHY_MEDIA_INVALID;
	epp->ep_module_type = EFX_PHY_MEDIA_INVALID;

	rc = efx_np_get_assigned_handle(enp, &epp->ep_np_handle);
	if (rc != 0)
		goto fail1;

	/*
	 * FIXME: This may need revisiting for VFs, which
	 * don't necessarily have access to these details.
	 */
	rc = efx_np_get_fixed_port_props(enp, epp->ep_np_handle,
		    epp->ep_np_cap_data_raw, &epp->ep_phy_cap_mask,
		    &epp->ep_np_loopback_cap_mask);
	if (rc != 0)
		goto fail2;

	rc = efx_np_link_state(enp, epp->ep_np_handle, &ls);
	if (rc != 0)
		goto fail3;

	if (ls.enls_an_supported != B_FALSE)
		epp->ep_phy_cap_mask |= 1U << EFX_PHY_CAP_AN;

	epp->ep_adv_cap_mask = ls.enls_adv_cap_mask;

#if EFSYS_OPT_LOOPBACK
	efx_np_assign_loopback_props(enp);
#endif /* EFSYS_OPT_LOOPBACK */

#if EFSYS_OPT_MAC_STATS
	rc = efx_np_stats_assign(enp);
	if (rc != 0)
		goto fail4;
#endif /* EFSYS_OPT_MAC_STATS */

	rc = efx_np_mac_state(enp, epp->ep_np_handle, &ms);
	if (rc != 0)
		goto fail5;

	epp->ep_mac_pdu = ms.enms_pdu;
	return (0);

fail5:
	EFSYS_PROBE(fail5);

#if EFSYS_OPT_MAC_STATS
fail4:
	EFSYS_PROBE(fail4);
#endif /* EFSYS_OPT_MAC_STATS */

fail3:
	EFSYS_PROBE(fail3);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

		void
efx_np_detach(
	__in	efx_nic_t *enp)
{
	if (efx_np_supported(enp) == B_FALSE)
		return;
}

	__checkReturn	efx_rc_t
efx_np_mac_state(
	__in		efx_nic_t *enp,
	__in		efx_np_handle_t nph,
	__out		efx_np_mac_state_t *msp)
{
	EFX_MCDI_DECLARE_BUF(payload,
	    MC_CMD_MAC_STATE_IN_LEN,
	    MC_CMD_MAC_STATE_OUT_LEN);
	efx_mcdi_req_t req;
	efx_rc_t rc;

	req.emr_out_length = MC_CMD_MAC_STATE_OUT_LEN;
	req.emr_in_length = MC_CMD_MAC_STATE_IN_LEN;
	req.emr_cmd = MC_CMD_MAC_STATE;
	req.emr_out_buf = payload;
	req.emr_in_buf = payload;

	MCDI_IN_SET_DWORD(req, MAC_STATE_IN_PORT_HANDLE, nph);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_MAC_STATE_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	memset(msp, 0, sizeof (*msp));

	if (MCDI_OUT_DWORD(req, MAC_STATE_OUT_MAC_FAULT_FLAGS) == 0)
		msp->enms_up = B_TRUE;

	msp->enms_pdu = MCDI_OUT_DWORD(req, MAC_STATE_OUT_MAX_FRAME_LEN);
	msp->enms_fcntl = MCDI_OUT_DWORD(req, MAC_STATE_OUT_FCNTL);
	return (0);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

static					void
efx_np_cap_mask_sw_to_hw(
	__in_ecount(hw_sw_map_nentries)	const struct efx_np_cap_map *hw_sw_map,
	__in				unsigned int hw_sw_map_nentries,
	__in_bcount(hw_cap_data_nbytes)	const uint8_t *hw_cap_data,
	__in				size_t hw_cap_data_nbytes,
	__in				uint32_t mask_sw,
	__out				uint8_t *mask_hwp)
{
	FOREACH_SUP_CAP(hw_sw_map, hw_sw_map_nentries,
	    hw_cap_data, hw_cap_data_nbytes) {
		uint32_t flag_sw = 1U << hw_sw_map->encm_sw;

		if ((mask_sw & flag_sw) != flag_sw)
			continue;

		mask_hwp[CAP_BYTE(hw_sw_map)] |= CAP_FLAG(hw_sw_map);
		mask_sw &= ~(flag_sw);
	}
}

/*
 * Convert the given EFX PHY capability mask to the HW representation.
 *
 * The mapping of a capability from EFX to HW can be one to many. Use
 * the given fraction of raw HW netport capability data to choose the
 * first supported HW capability bit encountered for a particular EFX
 * one and proceed with handling the next EFX bit, if any, afterwards.
 *
 * Do not check the input mask for leftover bits (unknown to EFX), as
 * inputs should have been validated by efx_phy_adv_cap_set() already.
 */
#define	EFX_NP_CAP_MASK_SW_TO_HW(					\
	    _hw_sw_cap_map, _hw_cap_section, _hw_cap_data,		\
	    _mask_sw, _mask_hwp)					\
	efx_np_cap_mask_sw_to_hw((_hw_sw_cap_map),			\
	    EFX_ARRAY_SIZE(_hw_sw_cap_map),				\
	    MCDI_STRUCT_MEMBER((_hw_cap_data), const uint8_t,		\
		    MC_CMD_##_hw_cap_section),				\
	    MC_CMD_##_hw_cap_section##_LEN,				\
	    (_mask_sw), (_mask_hwp))

static					void
efx_np_cap_sw_mask_to_hw_enum(
	__in_ecount(hw_sw_map_nentries)	const struct efx_np_cap_map *hw_sw_map,
	__in				unsigned int hw_sw_map_nentries,
	__in_bcount(hw_cap_data_nbytes)	const uint8_t *hw_cap_data,
	__in				size_t hw_cap_data_nbytes,
	__in				uint32_t mask_sw,
	__out				boolean_t *supportedp,
	__out_opt			uint16_t *enum_hwp)
{
	unsigned int sw_nflags_req = 0;
	unsigned int sw_nflags_sup = 0;
	uint32_t sw_check_mask = 0;
	unsigned int i;

	for (i = 0; i < hw_sw_map_nentries; ++i) {
		uint32_t flag_sw = 1U << hw_sw_map->encm_sw;
		unsigned int byte_idx = CAP_BYTE(hw_sw_map);
		uint8_t flag_hw = CAP_FLAG(hw_sw_map);

		if (byte_idx >= hw_cap_data_nbytes) {
			++(hw_sw_map);
			continue;
		}

		if ((mask_sw & flag_sw) == flag_sw) {
			if ((sw_check_mask & flag_sw) == 0)
				++(sw_nflags_req);

			sw_check_mask |= flag_sw;

			if ((hw_cap_data[byte_idx] & flag_hw) == flag_hw) {
				mask_sw &= ~(flag_sw);

				if (enum_hwp != NULL)
					*enum_hwp = hw_sw_map->encm_hw;
			}
		}

		++(hw_sw_map);
	}

	if (sw_check_mask != 0 && (mask_sw & sw_check_mask) == sw_check_mask) {
		/* Failed to select the enum by at least one capability bit. */
		*supportedp = B_FALSE;
		return;
	}

	*supportedp = B_TRUE;
}

/*
 * Convert (conceivably) the only EFX capability bit of the given mask to
 * the HW enum value, provided that the capability is supported by the HW,
 * where the latter follows from the given fraction of HW capability data.
 */
#define	EFX_NP_CAP_SW_MASK_TO_HW_ENUM(					\
	    _hw_sw_cap_map, _hw_cap_section, _hw_cap_data,		\
	    _mask_sw, _supportedp, _enum_hwp)				\
	efx_np_cap_sw_mask_to_hw_enum((_hw_sw_cap_map),			\
	    EFX_ARRAY_SIZE(_hw_sw_cap_map),				\
	    MCDI_STRUCT_MEMBER((_hw_cap_data), const uint8_t,		\
		    MC_CMD_##_hw_cap_section),				\
	    MC_CMD_##_hw_cap_section##_LEN, (_mask_sw),			\
	    (_supportedp), (_enum_hwp))

	__checkReturn	efx_rc_t
efx_np_link_ctrl(
	__in		efx_nic_t *enp,
	__in		efx_np_handle_t nph,
	__in		const uint8_t *cap_data_raw,
	__in		efx_link_mode_t loopback_link_mode,
	__in		efx_loopback_type_t loopback_mode,
	__in		uint32_t cap_mask_sw,
	__in		boolean_t fcntl_an)
{
	uint32_t flags = 1U << MC_CMD_LINK_FLAGS_IGNORE_MODULE_SEQ;
	uint8_t loopback = MC_CMD_LOOPBACK_V2_NONE;
	uint16_t link_tech = MC_CMD_ETH_TECH_NONE;
	EFX_MCDI_DECLARE_BUF(payload,
	    MC_CMD_LINK_CTRL_IN_LEN,
	    MC_CMD_LINK_CTRL_OUT_LEN);
	uint8_t *cap_mask_hw_pausep;
	uint8_t *cap_mask_hw_techp;
	uint16_t cap_enum_hw;
	boolean_t supported;
	efx_mcdi_req_t req;
	boolean_t phy_an;
	efx_rc_t rc;
	uint8_t fec;

	req.emr_out_length = MC_CMD_LINK_CTRL_OUT_LEN;
	req.emr_in_length = MC_CMD_LINK_CTRL_IN_LEN;
	req.emr_cmd = MC_CMD_LINK_CTRL;
	req.emr_out_buf = payload;
	req.emr_in_buf = payload;

	MCDI_IN_SET_DWORD(req, LINK_CTRL_IN_PORT_HANDLE, nph);

	cap_mask_hw_pausep = MCDI_IN2(req, uint8_t,
	    LINK_CTRL_IN_ADVERTISED_PAUSE_ABILITIES_MASK);

	cap_mask_hw_techp = MCDI_IN2(req, uint8_t,
	    LINK_CTRL_IN_ADVERTISED_TECH_ABILITIES_MASK);

	if (loopback_mode != EFX_LOOPBACK_OFF) {
#if EFSYS_OPT_LOOPBACK
		uint16_t cap_enum_sw;

		switch (loopback_mode) {
		case EFX_LOOPBACK_DATA:
			loopback = MC_CMD_LOOPBACK_V2_AUTO;
			break;
		case EFX_LOOPBACK_PCS:
			loopback = MC_CMD_LOOPBACK_V2_POST_PCS;
			break;
		default:
			rc = ENOTSUP;
			goto fail1;
		}

		rc = efx_np_sw_link_mode_to_cap(loopback_link_mode,
					    &cap_enum_sw);
		if (rc != 0)
			goto fail2;

		EFX_NP_CAP_ENUM_SW_TO_HW(efx_np_cap_map_tech,
		    ETH_AN_FIELDS_TECH_MASK, cap_data_raw, cap_enum_sw,
		    &supported, &link_tech);

		if (supported == B_FALSE) {
			rc = ENOTSUP;
			goto fail3;
		}
#else /* ! EFSYS_OPT_LOOPBACK */
		rc = ENOTSUP;
		goto fail1;
#endif /* EFSYS_OPT_LOOPBACK */
	} else if (cap_mask_sw & (1U << EFX_PHY_CAP_AN)) {
		EFX_NP_CAP_MASK_SW_TO_HW(efx_np_cap_map_tech,
		    ETH_AN_FIELDS_TECH_MASK, cap_data_raw, cap_mask_sw,
		    cap_mask_hw_techp);

		if (fcntl_an != B_FALSE) {
			EFX_NP_CAP_MASK_SW_TO_HW(efx_np_cap_map_pause,
			    ETH_AN_FIELDS_PAUSE_MASK, cap_data_raw, cap_mask_sw,
			    cap_mask_hw_pausep);
		}

		flags |= 1U << MC_CMD_LINK_FLAGS_AUTONEG_EN;
		link_tech = MC_CMD_ETH_TECH_AUTO;
	} else {
		EFX_NP_CAP_SW_MASK_TO_HW_ENUM(efx_np_cap_map_tech,
		    ETH_AN_FIELDS_TECH_MASK, cap_data_raw, cap_mask_sw,
		    &supported, &link_tech);

		if (supported == B_FALSE) {
			rc = ENOTSUP;
			goto fail4;
		}
	}

	/* The software mask may have no requested FEC bits. Default is NONE. */
	cap_enum_hw = MC_CMD_FEC_NONE;

	/*
	 * Compared to older EF10 interface, in netport MCDI, FEC mode is a
	 * single enum choice. For compatibility, do not enforce only single
	 * requested FEC bit in the original mask.
	 *
	 * No requested FEC bits in the original mask gives supported=TRUE.
	 */
	EFX_NP_CAP_SW_MASK_TO_HW_ENUM(efx_np_cap_map_fec_req,
	    ETH_AN_FIELDS_FEC_REQ, cap_data_raw, cap_mask_sw,
	    &supported, &cap_enum_hw);

	if ((cap_mask_sw & EFX_PHY_CAP_FEC_MASK) != 0 && supported == B_FALSE) {
		rc = ENOTSUP;
		goto fail5;
	}

	EFSYS_ASSERT(cap_enum_hw <= UINT8_MAX);
	fec = cap_enum_hw;

	MCDI_IN_SET_WORD(req, LINK_CTRL_IN_LINK_TECHNOLOGY, link_tech);
	MCDI_IN_SET_DWORD(req, LINK_CTRL_IN_CONTROL_FLAGS, flags);
	MCDI_IN_SET_BYTE(req, LINK_CTRL_IN_LOOPBACK, loopback);
	MCDI_IN_SET_BYTE(req, LINK_CTRL_IN_FEC_MODE, fec);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail6;
	}

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

	__checkReturn	efx_rc_t
efx_np_mac_ctrl(
	__in		efx_nic_t *enp,
	__in		efx_np_handle_t nph,
	__in		const efx_np_mac_ctrl_t *mc)
{
	EFX_MCDI_DECLARE_BUF(payload,
	    MC_CMD_MAC_CTRL_IN_LEN,
	    MC_CMD_MAC_CTRL_OUT_LEN);
	efx_mcdi_req_t req;
	uint32_t flags = 0;
	uint32_t cfg = 0;
	uint32_t fcntl;
	efx_rc_t rc;

	req.emr_out_length = MC_CMD_MAC_CTRL_OUT_LEN;
	req.emr_in_length = MC_CMD_MAC_CTRL_IN_LEN;
	req.emr_cmd = MC_CMD_MAC_CTRL;
	req.emr_out_buf = payload;
	req.emr_in_buf = payload;

	MCDI_IN_SET_DWORD(req, MAC_CTRL_IN_PORT_HANDLE, nph);

	MCDI_IN_SET_DWORD(req, MAC_CTRL_IN_MAX_FRAME_LEN, mc->enmc_pdu);
	cfg |= 1U << MC_CMD_MAC_CONFIG_OPTIONS_CFG_MAX_FRAME_LEN;

	if (mc->enmc_set_pdu_only != B_FALSE)
		goto skip_full_reconfigure;

	cfg |= 1U << MC_CMD_MAC_CONFIG_OPTIONS_CFG_INCLUDE_FCS;
	if (mc->enmc_include_fcs != B_FALSE)
		flags |= 1U << MC_CMD_MAC_FLAGS_FLAG_INCLUDE_FCS;

	MCDI_IN_SET_DWORD(req, MAC_CTRL_IN_FLAGS, flags);

	if (mc->enmc_fcntl_autoneg != B_FALSE) {
		fcntl = MC_CMD_FCNTL_AUTO;
	} else {
		switch (mc->enmc_fcntl) {
		case 0:
			fcntl = MC_CMD_FCNTL_OFF;
			break;
		case EFX_FCNTL_RESPOND:
			fcntl = MC_CMD_FCNTL_RESPOND;
			break;
		case EFX_FCNTL_GENERATE:
			fcntl = MC_CMD_FCNTL_GENERATE;
			break;
		case EFX_FCNTL_RESPOND | EFX_FCNTL_GENERATE:
			fcntl = MC_CMD_FCNTL_BIDIR;
			break;
		default:
			rc = EINVAL;
			goto fail1;
		}
	}

	cfg |= 1U << MC_CMD_MAC_CONFIG_OPTIONS_CFG_FCNTL;
	MCDI_IN_SET_DWORD(req, MAC_CTRL_IN_FCNTL, fcntl);

skip_full_reconfigure:
	MCDI_IN_SET_DWORD(req, MAC_CTRL_IN_V2_CONTROL_FLAGS, cfg);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail2;
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

#if EFSYS_OPT_MAC_STATS
	__checkReturn	efx_rc_t
efx_np_mac_stats(
	__in		efx_nic_t *enp,
	__in		efx_np_handle_t nph,
	__in		efx_stats_action_t action,
	__in_opt	const efsys_mem_t *esmp,
	__in		uint16_t period_ms)
{
	EFX_MCDI_DECLARE_BUF(payload,
	    MC_CMD_GET_NETPORT_STATISTICS_IN_LEN,
	    MC_CMD_GET_NETPORT_STATISTICS_OUT_LENMIN);
	boolean_t enable = (action == EFX_STATS_ENABLE_NOEVENTS);
	boolean_t events = (action == EFX_STATS_ENABLE_EVENTS);
	boolean_t disable = (action == EFX_STATS_DISABLE);
	boolean_t upload = (action == EFX_STATS_UPLOAD);
	boolean_t clear = (action == EFX_STATS_CLEAR);
	efx_mcdi_req_t req;
	efx_rc_t rc;

	req.emr_out_length = MC_CMD_GET_NETPORT_STATISTICS_OUT_LENMIN;
	req.emr_in_length = MC_CMD_GET_NETPORT_STATISTICS_IN_LEN;
	req.emr_cmd = MC_CMD_GET_NETPORT_STATISTICS;
	req.emr_out_buf = payload;
	req.emr_in_buf = payload;

	MCDI_IN_SET_DWORD(req, GET_NETPORT_STATISTICS_IN_PORT_HANDLE, nph);

	MCDI_IN_POPULATE_DWORD_6(req, GET_NETPORT_STATISTICS_IN_CMD,
	    GET_NETPORT_STATISTICS_IN_DMA, upload,
	    GET_NETPORT_STATISTICS_IN_CLEAR, clear,
	    GET_NETPORT_STATISTICS_IN_PERIODIC_CHANGE,
		    enable | events | disable,
	    GET_NETPORT_STATISTICS_IN_PERIODIC_ENABLE, enable | events,
	    GET_NETPORT_STATISTICS_IN_PERIODIC_NOEVENT, !events,
	    GET_NETPORT_STATISTICS_IN_PERIOD_MS,
		    (enable | events) ? period_ms : 0);

	if (enable || events || upload) {
		const efx_nic_cfg_t *encp = &enp->en_nic_cfg;
		uint32_t sz;

		/* Periodic stats or stats upload require a DMA buffer */
		if (esmp == NULL) {
			rc = EINVAL;
			goto fail1;
		}

		sz = encp->enc_mac_stats_nstats * sizeof (efx_qword_t);

		if (EFSYS_MEM_SIZE(esmp) < sz) {
			/* DMA buffer too small */
			rc = ENOSPC;
			goto fail2;
		}

		MCDI_IN_SET_DWORD(req, GET_NETPORT_STATISTICS_IN_DMA_ADDR_LO,
		    EFSYS_MEM_ADDR(esmp) & 0xffffffff);
		MCDI_IN_SET_DWORD(req, GET_NETPORT_STATISTICS_IN_DMA_ADDR_HI,
		    EFSYS_MEM_ADDR(esmp) >> 32);
		MCDI_IN_SET_DWORD(req, GET_NETPORT_STATISTICS_IN_DMA_LEN, sz);
	}

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		/* EF10: Expect ENOENT if no DMA queues are initialised */
		if ((req.emr_rc != ENOENT) ||
		    (enp->en_rx_qcount + enp->en_tx_qcount != 0)) {
			rc = req.emr_rc;
			goto fail3;
		}
	}

	return (0);

fail3:
	EFSYS_PROBE(fail3);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}
#endif /* EFSYS_OPT_MAC_STATS */
