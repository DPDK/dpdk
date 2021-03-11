/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Xilinx, Inc.
 */

#include "efx.h"
#include "efx_impl.h"

#if EFSYS_OPT_RIVERHEAD && EFSYS_OPT_TUNNEL

/* Match by Ether-type */
#define	EFX_VNIC_ENCAP_RULE_MATCH_ETHER_TYPE \
	(1u << MC_CMD_VNIC_ENCAP_RULE_ADD_IN_MATCH_ETHER_TYPE_LBN)
/* Match by outer VLAN ID */
#define	EFX_VNIC_ENCAP_RULE_MATCH_OUTER_VID \
	(1u << MC_CMD_VNIC_ENCAP_RULE_ADD_IN_MATCH_OUTER_VLAN_LBN)
/* Match by local IP host address */
#define	EFX_VNIC_ENCAP_RULE_MATCH_LOC_HOST \
	(1u << MC_CMD_VNIC_ENCAP_RULE_ADD_IN_MATCH_DST_IP_LBN)
/* Match by IP transport protocol */
#define	EFX_VNIC_ENCAP_RULE_MATCH_IP_PROTO \
	(1u << MC_CMD_VNIC_ENCAP_RULE_ADD_IN_MATCH_IP_PROTO_LBN)
/* Match by local TCP/UDP port */
#define	EFX_VNIC_ENCAP_RULE_MATCH_LOC_PORT \
	(1u << MC_CMD_VNIC_ENCAP_RULE_ADD_IN_MATCH_DST_PORT_LBN)

/*
 * Helper structure to pass parameters to MCDI function to add a VNIC
 * encapsulation rule.
 */
typedef struct efx_vnic_encap_rule_spec_s {
	uint32_t		evers_mport_selector; /* Host-endian */
	uint32_t		evers_match_flags; /* Host-endian */
	uint16_t		evers_ether_type; /* Host-endian */
	uint16_t		evers_outer_vid; /* Host-endian */
	efx_oword_t		evers_loc_host; /* Big-endian */
	uint8_t			evers_ip_proto;
	uint16_t		evers_loc_port; /* Host-endian */
	efx_tunnel_protocol_t	evers_encap_type;
} efx_vnic_encap_rule_spec_t;

static				uint32_t
efx_tunnel_protocol2mae_encap_type(
	__in		efx_tunnel_protocol_t proto,
	__out		uint32_t *typep)
{
	efx_rc_t rc;

	switch (proto) {
	case EFX_TUNNEL_PROTOCOL_NONE:
		*typep = MAE_MCDI_ENCAP_TYPE_NONE;
		break;
	case EFX_TUNNEL_PROTOCOL_VXLAN:
		*typep = MAE_MCDI_ENCAP_TYPE_VXLAN;
		break;
	case EFX_TUNNEL_PROTOCOL_GENEVE:
		*typep = MAE_MCDI_ENCAP_TYPE_GENEVE;
		break;
	case EFX_TUNNEL_PROTOCOL_NVGRE:
		*typep = MAE_MCDI_ENCAP_TYPE_NVGRE;
		break;
	default:
		rc = EINVAL;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

static	__checkReturn		efx_rc_t
efx_mcdi_vnic_encap_rule_add(
	__in			efx_nic_t *enp,
	__in			const efx_vnic_encap_rule_spec_t *spec,
	__out			efx_vnic_encap_rule_handle_t *handle)

{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload,
		MC_CMD_VNIC_ENCAP_RULE_ADD_IN_LEN,
		MC_CMD_VNIC_ENCAP_RULE_ADD_OUT_LEN);
	uint32_t encap_type;
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_VNIC_ENCAP_RULE_ADD;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_VNIC_ENCAP_RULE_ADD_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_VNIC_ENCAP_RULE_ADD_OUT_LEN;

	MCDI_IN_SET_DWORD(req, VNIC_ENCAP_RULE_ADD_IN_MPORT_SELECTOR,
	    spec->evers_mport_selector);
	MCDI_IN_SET_DWORD(req, VNIC_ENCAP_RULE_ADD_IN_MATCH_FLAGS,
	    spec->evers_match_flags);

	MCDI_IN_SET_WORD_NATIVE(req, VNIC_ENCAP_RULE_ADD_IN_ETHER_TYPE,
	    __CPU_TO_BE_16(spec->evers_ether_type));
	MCDI_IN_SET_WORD_NATIVE(req, VNIC_ENCAP_RULE_ADD_IN_OUTER_VLAN_WORD,
	    __CPU_TO_BE_16(spec->evers_outer_vid));

	/*
	 * Address is already in network order as well as the MCDI field,
	 * so plain copy is used.
	 */
	EFX_STATIC_ASSERT(sizeof (spec->evers_loc_host) ==
	    MC_CMD_VNIC_ENCAP_RULE_ADD_IN_DST_IP_LEN);
	memcpy(MCDI_IN2(req, uint8_t, VNIC_ENCAP_RULE_ADD_IN_DST_IP),
	    &spec->evers_loc_host.eo_byte[0],
	    MC_CMD_VNIC_ENCAP_RULE_ADD_IN_DST_IP_LEN);

	MCDI_IN_SET_BYTE(req, VNIC_ENCAP_RULE_ADD_IN_IP_PROTO,
	    spec->evers_ip_proto);
	MCDI_IN_SET_WORD_NATIVE(req, VNIC_ENCAP_RULE_ADD_IN_DST_PORT,
	    __CPU_TO_BE_16(spec->evers_loc_port));

	rc = efx_tunnel_protocol2mae_encap_type(spec->evers_encap_type,
	    &encap_type);
	if (rc != 0)
		goto fail1;

	MCDI_IN_SET_DWORD(req, VNIC_ENCAP_RULE_ADD_IN_ENCAP_TYPE, encap_type);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail2;
	}

	if (req.emr_out_length_used != MC_CMD_VNIC_ENCAP_RULE_ADD_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail3;
	}

	if (handle != NULL)
		*handle = MCDI_OUT_DWORD(req, VNIC_ENCAP_RULE_ADD_OUT_HANDLE);

	return (0);

fail3:
	EFSYS_PROBE(fail3);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

static	__checkReturn		efx_rc_t
efx_mcdi_vnic_encap_rule_remove(
	__in			efx_nic_t *enp,
	__in			efx_vnic_encap_rule_handle_t handle)

{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload,
	    MC_CMD_VNIC_ENCAP_RULE_REMOVE_IN_LEN,
	    MC_CMD_VNIC_ENCAP_RULE_REMOVE_OUT_LEN);
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_VNIC_ENCAP_RULE_REMOVE;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_VNIC_ENCAP_RULE_REMOVE_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_VNIC_ENCAP_RULE_REMOVE_OUT_LEN;

	MCDI_IN_SET_DWORD(req, VNIC_ENCAP_RULE_REMOVE_IN_HANDLE, handle);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used != MC_CMD_VNIC_ENCAP_RULE_REMOVE_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

static			void
rhead_vnic_encap_rule_spec_init(
	__in		const efx_tunnel_udp_entry_t *etuep,
	__out		efx_vnic_encap_rule_spec_t *spec)
{
	memset(spec, 0, sizeof (*spec));

	spec->evers_mport_selector = MAE_MPORT_SELECTOR_ASSIGNED;
	spec->evers_match_flags = EFX_VNIC_ENCAP_RULE_MATCH_IP_PROTO |
	    EFX_VNIC_ENCAP_RULE_MATCH_LOC_PORT;
	spec->evers_ip_proto = EFX_IPPROTO_UDP;
	spec->evers_loc_port = etuep->etue_port;
	spec->evers_encap_type = etuep->etue_protocol;
}

static	__checkReturn	efx_rc_t
rhead_udp_port_tunnel_add(
	__in		efx_nic_t *enp,
	__inout		efx_tunnel_udp_entry_t *etuep)
{
	efx_vnic_encap_rule_spec_t spec;

	rhead_vnic_encap_rule_spec_init(etuep, &spec);
	return (efx_mcdi_vnic_encap_rule_add(enp, &spec, &etuep->etue_handle));
}

static	__checkReturn	efx_rc_t
rhead_udp_port_tunnel_remove(
	__in		efx_nic_t *enp,
	__in		efx_tunnel_udp_entry_t *etuep)
{
	return (efx_mcdi_vnic_encap_rule_remove(enp, etuep->etue_handle));
}

	__checkReturn	efx_rc_t
rhead_tunnel_reconfigure(
	__in		efx_nic_t *enp)
{
	efx_tunnel_cfg_t *etcp = &enp->en_tunnel_cfg;
	efx_rc_t rc;
	efsys_lock_state_t state;
	efx_tunnel_cfg_t etc;
	efx_tunnel_cfg_t added;
	unsigned int i;
	unsigned int j;

	memset(&added, 0, sizeof(added));

	/*
	 * Make a local copy of UDP tunnel table to release the lock
	 * when executing MCDIs.
	 */
	EFSYS_LOCK(enp->en_eslp, state);
	memcpy(&etc, etcp, sizeof (etc));
	EFSYS_UNLOCK(enp->en_eslp, state);

	for (i = 0; i < etc.etc_udp_entries_num; i++) {
		efx_tunnel_udp_entry_t *etc_entry = &etc.etc_udp_entries[i];

		if (etc_entry->etue_busy == B_FALSE)
			continue;

		switch (etc_entry->etue_state) {
		case EFX_TUNNEL_UDP_ENTRY_APPLIED:
			break;
		case EFX_TUNNEL_UDP_ENTRY_ADDED:
			rc = rhead_udp_port_tunnel_add(enp, etc_entry);
			if (rc != 0)
				goto fail1;
			added.etc_udp_entries[added.etc_udp_entries_num] =
			    *etc_entry;
			added.etc_udp_entries_num++;
			break;
		case EFX_TUNNEL_UDP_ENTRY_REMOVED:
			rc = rhead_udp_port_tunnel_remove(enp, etc_entry);
			if (rc != 0)
				goto fail2;
			break;
		default:
			EFSYS_ASSERT(0);
			break;
		}
	}

	EFSYS_LOCK(enp->en_eslp, state);

	/*
	 * Adding or removing non-busy entries does not change the
	 * order of busy entries. Therefore one linear search iteration
	 * suffices.
	 */
	for (i = 0, j = 0; i < etcp->etc_udp_entries_num; i++) {
		efx_tunnel_udp_entry_t *cur_entry = &etcp->etc_udp_entries[i];
		efx_tunnel_udp_entry_t *added_entry = &added.etc_udp_entries[j];

		if (cur_entry->etue_state == EFX_TUNNEL_UDP_ENTRY_ADDED &&
		    cur_entry->etue_port == added_entry->etue_port) {
			cur_entry->etue_handle = added_entry->etue_handle;
			j++;
		}
	}

	EFSYS_UNLOCK(enp->en_eslp, state);

	return (0);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	while (i-- > 0) {
		if (etc.etc_udp_entries[i].etue_busy == B_FALSE)
			continue;

		switch (etc.etc_udp_entries[i].etue_state) {
		case EFX_TUNNEL_UDP_ENTRY_APPLIED:
			break;
		case EFX_TUNNEL_UDP_ENTRY_ADDED:
			(void) rhead_udp_port_tunnel_remove(enp,
					&etc.etc_udp_entries[i]);
			break;
		case EFX_TUNNEL_UDP_ENTRY_REMOVED:
			(void) rhead_udp_port_tunnel_add(enp,
					&etc.etc_udp_entries[i]);
			break;
		default:
			EFSYS_ASSERT(0);
			break;
		}
	}

	return (rc);
}

			void
rhead_tunnel_fini(
	__in		efx_nic_t *enp)
{
	(void) efx_tunnel_config_clear(enp);
	(void) efx_tunnel_reconfigure(enp);
}

#endif	/* EFSYS_OPT_RIVERHEAD && EFSYS_OPT_TUNNEL */
