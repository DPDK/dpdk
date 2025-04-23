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
	efx_mcdi_req_t req;
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

	memset(lsp, 0, sizeof (*lsp));

	if (MCDI_OUT_DWORD(req, LINK_STATE_OUT_V2_LOCAL_AN_SUPPORT) !=
	    MC_CMD_AN_NONE)
		lsp->enls_an_supported = B_TRUE;

	if (lsp->enls_an_supported != B_FALSE)
		lsp->enls_adv_cap_mask |= 1U << EFX_PHY_CAP_AN;

	efx_np_cap_hw_data_to_sw_mask(
	    MCDI_OUT2(req, const uint8_t, LINK_STATE_OUT_ADVERTISED_ABILITIES),
	    &lsp->enls_adv_cap_mask);

	return (0);

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

	__checkReturn	efx_rc_t
efx_np_attach(
	__in		efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_port_t *epp = &(enp->en_port);
	efx_np_link_state_t ls;
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

	return (0);

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
