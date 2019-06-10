/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2018-2019 Solarflare Communications Inc.
 * All rights reserved.
 */

#include "efx.h"
#include "efx_impl.h"

#if EFSYS_OPT_EVB

#if EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD || EFSYS_OPT_MEDFORD2

	__checkReturn	efx_rc_t
ef10_evb_init(
	__in		efx_nic_t *enp)
{
	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON ||
		enp->en_family == EFX_FAMILY_MEDFORD ||
		enp->en_family == EFX_FAMILY_MEDFORD2);

	return (0);
}

	void
ef10_evb_fini(
	__in		efx_nic_t *enp)
{
	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON ||
		enp->en_family == EFX_FAMILY_MEDFORD ||
		enp->en_family == EFX_FAMILY_MEDFORD2);
}

	__checkReturn	efx_rc_t
efx_mcdi_vswitch_alloc(
	__in		efx_nic_t *enp,
	__in		efx_vport_id_t vport_id,
	__in		efx_vswitch_type_t vswitch_type)
{
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_VSWITCH_ALLOC_IN_LEN,
		MC_CMD_VSWITCH_ALLOC_OUT_LEN);
	efx_mcdi_req_t req;
	efx_rc_t rc;
	uint8_t ntags;

	/* Ensure EFX and MCDI use same values for vswitch types */
	EFX_STATIC_ASSERT(EFX_VSWITCH_TYPE_VLAN ==
		MC_CMD_VSWITCH_ALLOC_IN_VSWITCH_TYPE_VLAN);
	EFX_STATIC_ASSERT(EFX_VSWITCH_TYPE_VEB ==
		MC_CMD_VSWITCH_ALLOC_IN_VSWITCH_TYPE_VEB);
	EFX_STATIC_ASSERT(EFX_VSWITCH_TYPE_MUX ==
		MC_CMD_VSWITCH_ALLOC_IN_VSWITCH_TYPE_MUX);

	/* First try with maximum number of VLAN tags FW supports */
	ntags = 2;
retry:
	req.emr_cmd = MC_CMD_VSWITCH_ALLOC;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_VSWITCH_ALLOC_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_VSWITCH_ALLOC_OUT_LEN;

	MCDI_IN_SET_DWORD(req, VSWITCH_ALLOC_IN_UPSTREAM_PORT_ID, vport_id);
	MCDI_IN_SET_DWORD(req, VSWITCH_ALLOC_IN_TYPE, vswitch_type);
	MCDI_IN_SET_DWORD(req, VSWITCH_ALLOC_IN_NUM_VLAN_TAGS, ntags);
	MCDI_IN_POPULATE_DWORD_1(req, VSWITCH_ALLOC_IN_FLAGS,
		VSWITCH_ALLOC_IN_FLAG_AUTO_PORT, 0);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		/*
		 * efx_rc_t error codes in libefx are translated from MCDI
		 * error codes in efx_mcdi_request_errcode. As this conversion
		 * is not a 1:1, here we check the specific MCDI error code.
		 */
		if (req.emr_err_code == MC_CMD_ERR_VLAN_LIMIT) {
			/* Too many VLAN tags, retry with fewer */
			EFSYS_PROBE(vlan_limit);
			ntags--;
			if (ntags > 0) {
				/*
				 * Zero the buffer before reusing it
				 * for another request
				 */
				memset(payload, 0, sizeof (payload));
				goto retry;
			}
			goto fail1;
		}
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn	efx_rc_t
efx_mcdi_vswitch_free(
	__in		efx_nic_t *enp)
{
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_VSWITCH_FREE_IN_LEN,
		MC_CMD_VSWITCH_FREE_OUT_LEN);
	efx_mcdi_req_t req;
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_VSWITCH_FREE;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_VSWITCH_FREE_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_VSWITCH_FREE_OUT_LEN;

	MCDI_IN_SET_DWORD(req, VSWITCH_FREE_IN_UPSTREAM_PORT_ID,
		EVB_PORT_ID_ASSIGNED);
	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn	efx_rc_t
efx_mcdi_vport_alloc(
	__in		efx_nic_t *enp,
	__in		efx_vport_type_t vport_type,
	__in		uint16_t vid,
	__in		boolean_t vlan_restrict,
	__out		efx_vport_id_t *vport_idp)
{
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_VPORT_ALLOC_IN_LEN,
		MC_CMD_VPORT_ALLOC_OUT_LEN);
	efx_mcdi_req_t req;
	efx_rc_t rc;

	/* Ensure EFX and MCDI use same values for vport types */
	EFX_STATIC_ASSERT(EFX_VPORT_TYPE_NORMAL ==
		MC_CMD_VPORT_ALLOC_IN_VPORT_TYPE_NORMAL);
	EFX_STATIC_ASSERT(EFX_VPORT_TYPE_EXPANSION ==
		MC_CMD_VPORT_ALLOC_IN_VPORT_TYPE_EXPANSION);
	EFX_STATIC_ASSERT(EFX_VPORT_TYPE_TEST ==
		MC_CMD_VPORT_ALLOC_IN_VPORT_TYPE_TEST);

	req.emr_cmd = MC_CMD_VPORT_ALLOC;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_VPORT_ALLOC_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_VPORT_ALLOC_OUT_LEN;

	MCDI_IN_SET_DWORD(req, VPORT_ALLOC_IN_UPSTREAM_PORT_ID,
		EVB_PORT_ID_ASSIGNED);
	MCDI_IN_SET_DWORD(req, VPORT_ALLOC_IN_TYPE, vport_type);
	MCDI_IN_SET_DWORD(req, VPORT_ALLOC_IN_NUM_VLAN_TAGS,
		(vid != EFX_FILTER_VID_UNSPEC));

	MCDI_IN_POPULATE_DWORD_2(req, VPORT_ALLOC_IN_FLAGS,
		 VPORT_ALLOC_IN_FLAG_AUTO_PORT, 0,
		 VPORT_ALLOC_IN_FLAG_VLAN_RESTRICT, vlan_restrict);

	if (vid != EFX_FILTER_VID_UNSPEC)
		MCDI_IN_POPULATE_DWORD_1(req, VPORT_ALLOC_IN_VLAN_TAGS,
			 VPORT_ALLOC_IN_VLAN_TAG_0, vid);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_VPORT_ALLOC_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	*vport_idp = *MCDI_OUT2(req, uint32_t, VPORT_ALLOC_OUT_VPORT_ID);
	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn	efx_rc_t
efx_mcdi_vport_free(
	__in		efx_nic_t *enp,
	__in		efx_vport_id_t vport_id)
{
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_VPORT_FREE_IN_LEN,
		MC_CMD_VPORT_FREE_OUT_LEN);
	efx_mcdi_req_t req;
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_VPORT_FREE;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_VPORT_FREE_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_VPORT_FREE_OUT_LEN;

	MCDI_IN_SET_DWORD(req, VPORT_FREE_IN_VPORT_ID, vport_id);
	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn	efx_rc_t
efx_mcdi_vport_mac_addr_add(
	__in		efx_nic_t *enp,
	__in		efx_vport_id_t vport_id,
	__in_ecount(6)	uint8_t *addrp)
{
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_VPORT_ADD_MAC_ADDRESS_IN_LEN,
		MC_CMD_VPORT_ADD_MAC_ADDRESS_OUT_LEN);
	efx_mcdi_req_t req;
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_VPORT_ADD_MAC_ADDRESS;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_VPORT_ADD_MAC_ADDRESS_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_VPORT_ADD_MAC_ADDRESS_OUT_LEN;

	MCDI_IN_SET_DWORD(req, VPORT_ADD_MAC_ADDRESS_IN_VPORT_ID, vport_id);
	EFX_MAC_ADDR_COPY(MCDI_IN2(req, uint8_t,
				VPORT_ADD_MAC_ADDRESS_IN_MACADDR), addrp);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn	efx_rc_t
efx_mcdi_vport_mac_addr_del(
	__in		efx_nic_t *enp,
	__in		efx_vport_id_t vport_id,
	__in_ecount(6)	uint8_t *addrp)
{
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_VPORT_DEL_MAC_ADDRESS_IN_LEN,
		MC_CMD_VPORT_DEL_MAC_ADDRESS_OUT_LEN);
	efx_mcdi_req_t req;
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_VPORT_DEL_MAC_ADDRESS;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_VPORT_DEL_MAC_ADDRESS_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_VPORT_DEL_MAC_ADDRESS_OUT_LEN;

	MCDI_IN_SET_DWORD(req, VPORT_DEL_MAC_ADDRESS_IN_VPORT_ID, vport_id);
	EFX_MAC_ADDR_COPY(MCDI_IN2(req, uint8_t,
				VPORT_DEL_MAC_ADDRESS_IN_MACADDR), addrp);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn	efx_rc_t
efx_mcdi_port_assign(
	__in		efx_nic_t *enp,
	__in		efx_vport_id_t vport_id,
	__in		uint32_t vf_index)
{
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_EVB_PORT_ASSIGN_IN_LEN,
		MC_CMD_EVB_PORT_ASSIGN_OUT_LEN);
	efx_mcdi_req_t req;
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_EVB_PORT_ASSIGN;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_EVB_PORT_ASSIGN_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_EVB_PORT_ASSIGN_OUT_LEN;

	MCDI_IN_SET_DWORD(req, EVB_PORT_ASSIGN_IN_PORT_ID, vport_id);
	MCDI_IN_POPULATE_DWORD_2(req, EVB_PORT_ASSIGN_IN_FUNCTION,
		EVB_PORT_ASSIGN_IN_PF, encp->enc_pf,
		EVB_PORT_ASSIGN_IN_VF, vf_index);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return 0;

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

#endif /* EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD || EFSYS_OPT_MEDFORD2 */
#endif /* EFSYS_OPT_EVB */
