/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2007-2019 Solarflare Communications Inc.
 */

#include "efx.h"
#include "efx_impl.h"

/*
 * State diagram of the UDP tunnel table entries
 * (efx_tunnel_udp_entry_state_t and busy flag):
 *
 *                             +---------+
 *                    +--------| APPLIED |<-------+
 *                    |        +---------+        |
 *                    |                           |
 *                    |                efx_tunnel_reconfigure (end)
 *   efx_tunnel_config_udp_remove                 |
 *                    |                    +------------+
 *                    v                    | BUSY ADDED |
 *               +---------+               +------------+
 *               | REMOVED |                      ^
 *               +---------+                      |
 *                    |               efx_tunnel_reconfigure (begin)
 *  efx_tunnel_reconfigure (begin)                |
 *                    |                           |
 *                    v                     +-----------+
 *            +--------------+              |   ADDED   |<---------+
 *            | BUSY REMOVED |              +-----------+          |
 *            +--------------+                    |                |
 *                    |              efx_tunnel_config_udp_remove  |
 *  efx_tunnel_reconfigure (end)                  |                |
 *                    |                           |                |
 *                    |        +---------+        |                |
 *                    |        |+-------+|        |                |
 *                    +------->|| empty ||<-------+                |
 *                             |+-------+|                         |
 *                             +---------+        efx_tunnel_config_udp_add
 *                                  |                              |
 *                                  +------------------------------+
 *
 * Note that there is no BUSY APPLIED state since removing an applied entry
 * should not be blocked by ongoing reconfiguration in another thread -
 * reconfiguration will remove only busy entries.
 */

#if EFSYS_OPT_TUNNEL

#if EFSYS_OPT_MEDFORD || EFSYS_OPT_MEDFORD2
static	__checkReturn	boolean_t
ef10_udp_encap_supported(
	__in		efx_nic_t *enp);

static	__checkReturn	efx_rc_t
ef10_tunnel_reconfigure(
	__in		efx_nic_t *enp);

static			void
ef10_tunnel_fini(
	__in		efx_nic_t *enp);
#endif /* EFSYS_OPT_MEDFORD || EFSYS_OPT_MEDFORD2 */

#if EFSYS_OPT_SIENA || EFSYS_OPT_HUNTINGTON
static const efx_tunnel_ops_t	__efx_tunnel_dummy_ops = {
	NULL,	/* eto_reconfigure */
	NULL,	/* eto_fini */
};
#endif /* EFSYS_OPT_SIENA || EFSYS_OPT_HUNTINGTON */

#if EFSYS_OPT_MEDFORD || EFSYS_OPT_MEDFORD2
static const efx_tunnel_ops_t	__efx_tunnel_ef10_ops = {
	ef10_tunnel_reconfigure,	/* eto_reconfigure */
	ef10_tunnel_fini,		/* eto_fini */
};
#endif /* EFSYS_OPT_MEDFORD || EFSYS_OPT_MEDFORD2 */

#if EFSYS_OPT_RIVERHEAD
static const efx_tunnel_ops_t	__efx_tunnel_rhead_ops = {
	rhead_tunnel_reconfigure,	/* eto_reconfigure */
	rhead_tunnel_fini,		/* eto_fini */
};
#endif /* EFSYS_OPT_RIVERHEAD */

/* Indicates that an entry is to be set */
static	__checkReturn		boolean_t
ef10_entry_staged(
	__in			efx_tunnel_udp_entry_t *entry)
{
	switch (entry->etue_state) {
	case EFX_TUNNEL_UDP_ENTRY_ADDED:
		return (entry->etue_busy);
	case EFX_TUNNEL_UDP_ENTRY_REMOVED:
		return (!entry->etue_busy);
	case EFX_TUNNEL_UDP_ENTRY_APPLIED:
		return (B_TRUE);
	default:
		EFSYS_ASSERT(0);
		return (B_FALSE);
	}
}

static	__checkReturn		efx_rc_t
efx_mcdi_set_tunnel_encap_udp_ports(
	__in			efx_nic_t *enp,
	__in			efx_tunnel_cfg_t *etcp,
	__in			boolean_t unloading,
	__out			boolean_t *resetting)
{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload,
		MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_IN_LENMAX,
		MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_OUT_LEN);
	efx_word_t flags;
	efx_rc_t rc;
	unsigned int i;
	unsigned int entries_num;
	unsigned int entry;

	entries_num = 0;
	if (etcp != NULL) {
		for (i = 0; i < etcp->etc_udp_entries_num; i++) {
			if (ef10_entry_staged(&etcp->etc_udp_entries[i]) !=
			    B_FALSE) {
				entries_num++;
			}
		}
	}

	req.emr_cmd = MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS;
	req.emr_in_buf = payload;
	req.emr_in_length =
	    MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_IN_LEN(entries_num);
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_OUT_LEN;

	EFX_POPULATE_WORD_1(flags,
	    MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_IN_UNLOADING,
	    (unloading == B_TRUE) ? 1 : 0);
	MCDI_IN_SET_WORD(req, SET_TUNNEL_ENCAP_UDP_PORTS_IN_FLAGS,
	    EFX_WORD_FIELD(flags, EFX_WORD_0));

	MCDI_IN_SET_WORD(req, SET_TUNNEL_ENCAP_UDP_PORTS_IN_NUM_ENTRIES,
	    entries_num);

	for (i = 0, entry = 0; entry < entries_num; ++entry, ++i) {
		uint16_t mcdi_udp_protocol;

		while (ef10_entry_staged(&etcp->etc_udp_entries[i]) == B_FALSE)
			i++;

		switch (etcp->etc_udp_entries[i].etue_protocol) {
		case EFX_TUNNEL_PROTOCOL_VXLAN:
			mcdi_udp_protocol = TUNNEL_ENCAP_UDP_PORT_ENTRY_VXLAN;
			break;
		case EFX_TUNNEL_PROTOCOL_GENEVE:
			mcdi_udp_protocol = TUNNEL_ENCAP_UDP_PORT_ENTRY_GENEVE;
			break;
		default:
			rc = EINVAL;
			goto fail1;
		}

		/*
		 * UDP port is MCDI native little-endian in the request
		 * and EFX_POPULATE_DWORD cares about conversion from
		 * host/CPU byte order to little-endian.
		 */
		EFX_STATIC_ASSERT(sizeof (efx_dword_t) ==
		    TUNNEL_ENCAP_UDP_PORT_ENTRY_LEN);
		EFX_POPULATE_DWORD_2(
		    MCDI_IN2(req, efx_dword_t,
			SET_TUNNEL_ENCAP_UDP_PORTS_IN_ENTRIES)[entry],
		    TUNNEL_ENCAP_UDP_PORT_ENTRY_UDP_PORT,
		    etcp->etc_udp_entries[i].etue_port,
		    TUNNEL_ENCAP_UDP_PORT_ENTRY_PROTOCOL,
		    mcdi_udp_protocol);
	}

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail2;
	}

	if (req.emr_out_length_used !=
	    MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail3;
	}

	*resetting = MCDI_OUT_WORD_FIELD(req,
	    SET_TUNNEL_ENCAP_UDP_PORTS_OUT_FLAGS,
	    SET_TUNNEL_ENCAP_UDP_PORTS_OUT_RESETTING);

	return (0);

fail3:
	EFSYS_PROBE(fail3);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_tunnel_init(
	__in		efx_nic_t *enp)
{
	efx_tunnel_cfg_t *etcp = &enp->en_tunnel_cfg;
	const efx_tunnel_ops_t *etop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_TUNNEL));

	EFX_STATIC_ASSERT(EFX_TUNNEL_MAXNENTRIES ==
	    MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_IN_ENTRIES_MAXNUM);

	switch (enp->en_family) {
#if EFSYS_OPT_SIENA
	case EFX_FAMILY_SIENA:
		etop = &__efx_tunnel_dummy_ops;
		break;
#endif /* EFSYS_OPT_SIENA */

#if EFSYS_OPT_HUNTINGTON
	case EFX_FAMILY_HUNTINGTON:
		etop = &__efx_tunnel_dummy_ops;
		break;
#endif /* EFSYS_OPT_HUNTINGTON */

#if EFSYS_OPT_MEDFORD
	case EFX_FAMILY_MEDFORD:
		etop = &__efx_tunnel_ef10_ops;
		break;
#endif /* EFSYS_OPT_MEDFORD */

#if EFSYS_OPT_MEDFORD2
	case EFX_FAMILY_MEDFORD2:
		etop = &__efx_tunnel_ef10_ops;
		break;
#endif /* EFSYS_OPT_MEDFORD2 */

#if EFSYS_OPT_RIVERHEAD
	case EFX_FAMILY_RIVERHEAD:
		etop = &__efx_tunnel_rhead_ops;
		break;
#endif /* EFSYS_OPT_RIVERHEAD */

	default:
		EFSYS_ASSERT(0);
		rc = ENOTSUP;
		goto fail1;
	}

	memset(etcp->etc_udp_entries, 0, sizeof (etcp->etc_udp_entries));
	etcp->etc_udp_entries_num = 0;

	enp->en_etop = etop;
	enp->en_mod_flags |= EFX_MOD_TUNNEL;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	enp->en_etop = NULL;
	enp->en_mod_flags &= ~EFX_MOD_TUNNEL;

	return (rc);
}

			void
efx_tunnel_fini(
	__in		efx_nic_t *enp)
{
	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_TUNNEL);

	if (enp->en_etop->eto_fini != NULL)
		enp->en_etop->eto_fini(enp);

	enp->en_etop = NULL;
	enp->en_mod_flags &= ~EFX_MOD_TUNNEL;
}

static	__checkReturn	efx_rc_t
efx_tunnel_config_find_udp_tunnel_entry(
	__in		efx_tunnel_cfg_t *etcp,
	__in		uint16_t port,
	__out		unsigned int *entryp)
{
	unsigned int i;

	for (i = 0; i < etcp->etc_udp_entries_num; ++i) {
		efx_tunnel_udp_entry_t *p = &etcp->etc_udp_entries[i];

		if (p->etue_port == port &&
		    p->etue_state != EFX_TUNNEL_UDP_ENTRY_REMOVED) {
			*entryp = i;
			return (0);
		}
	}

	return (ENOENT);
}

	__checkReturn	efx_rc_t
efx_tunnel_config_udp_add(
	__in		efx_nic_t *enp,
	__in		uint16_t port /* host/cpu-endian */,
	__in		efx_tunnel_protocol_t protocol)
{
	const efx_nic_cfg_t *encp = &enp->en_nic_cfg;
	efx_tunnel_cfg_t *etcp = &enp->en_tunnel_cfg;
	efsys_lock_state_t state;
	efx_rc_t rc;
	unsigned int entry;

	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_TUNNEL);

	if (protocol >= EFX_TUNNEL_NPROTOS) {
		rc = EINVAL;
		goto fail1;
	}

	if ((encp->enc_tunnel_encapsulations_supported &
	    (1u << protocol)) == 0) {
		rc = ENOTSUP;
		goto fail2;
	}

	EFSYS_LOCK(enp->en_eslp, state);

	rc = efx_tunnel_config_find_udp_tunnel_entry(etcp, port, &entry);
	if (rc == 0) {
		rc = EEXIST;
		goto fail3;
	}

	if (etcp->etc_udp_entries_num ==
	    encp->enc_tunnel_config_udp_entries_max) {
		rc = ENOSPC;
		goto fail4;
	}

	etcp->etc_udp_entries[etcp->etc_udp_entries_num].etue_port = port;
	etcp->etc_udp_entries[etcp->etc_udp_entries_num].etue_protocol =
	    protocol;
	etcp->etc_udp_entries[etcp->etc_udp_entries_num].etue_state =
	    EFX_TUNNEL_UDP_ENTRY_ADDED;

	etcp->etc_udp_entries_num++;

	EFSYS_UNLOCK(enp->en_eslp, state);

	return (0);

fail4:
	EFSYS_PROBE(fail4);

fail3:
	EFSYS_PROBE(fail3);
	EFSYS_UNLOCK(enp->en_eslp, state);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

/*
 * Returns the index of the entry after the deleted one,
 * or one past the last entry.
 */
static			unsigned int
efx_tunnel_config_udp_do_remove(
	__in		efx_tunnel_cfg_t *etcp,
	__in		unsigned int entry)
{
	EFSYS_ASSERT3U(etcp->etc_udp_entries_num, >, 0);
	etcp->etc_udp_entries_num--;

	if (entry < etcp->etc_udp_entries_num) {
		memmove(&etcp->etc_udp_entries[entry],
		    &etcp->etc_udp_entries[entry + 1],
		    (etcp->etc_udp_entries_num - entry) *
		    sizeof (etcp->etc_udp_entries[0]));
	}

	memset(&etcp->etc_udp_entries[etcp->etc_udp_entries_num], 0,
	    sizeof (etcp->etc_udp_entries[0]));

	return (entry);
}

/*
 * Returns the index of the entry after the specified one,
 * or one past the last entry. The index is correct whether
 * the specified entry was removed or not.
 */
static			unsigned int
efx_tunnel_config_udp_remove_prepare(
	__in		efx_tunnel_cfg_t *etcp,
	__in		unsigned int entry)
{
	unsigned int next = entry + 1;

	switch (etcp->etc_udp_entries[entry].etue_state) {
	case EFX_TUNNEL_UDP_ENTRY_ADDED:
		next = efx_tunnel_config_udp_do_remove(etcp, entry);
		break;
	case EFX_TUNNEL_UDP_ENTRY_REMOVED:
		break;
	case EFX_TUNNEL_UDP_ENTRY_APPLIED:
		etcp->etc_udp_entries[entry].etue_state =
		    EFX_TUNNEL_UDP_ENTRY_REMOVED;
		break;
	default:
		EFSYS_ASSERT(0);
		break;
	}

	return (next);
}

	__checkReturn	efx_rc_t
efx_tunnel_config_udp_remove(
	__in		efx_nic_t *enp,
	__in		uint16_t port /* host/cpu-endian */,
	__in		efx_tunnel_protocol_t protocol)
{
	efx_tunnel_cfg_t *etcp = &enp->en_tunnel_cfg;
	efsys_lock_state_t state;
	unsigned int entry;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_TUNNEL);

	EFSYS_LOCK(enp->en_eslp, state);

	rc = efx_tunnel_config_find_udp_tunnel_entry(etcp, port, &entry);
	if (rc != 0)
		goto fail1;

	if (etcp->etc_udp_entries[entry].etue_busy != B_FALSE) {
		rc = EBUSY;
		goto fail2;
	}

	if (etcp->etc_udp_entries[entry].etue_protocol != protocol) {
		rc = EINVAL;
		goto fail3;
	}

	(void) efx_tunnel_config_udp_remove_prepare(etcp, entry);

	EFSYS_UNLOCK(enp->en_eslp, state);

	return (0);

fail3:
	EFSYS_PROBE(fail3);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	EFSYS_UNLOCK(enp->en_eslp, state);

	return (rc);
}

static			boolean_t
efx_tunnel_table_all_available(
	__in			efx_tunnel_cfg_t *etcp)
{
	unsigned int i;

	for (i = 0; i < etcp->etc_udp_entries_num; i++) {
		if (etcp->etc_udp_entries[i].etue_busy != B_FALSE)
			return (B_FALSE);
	}

	return (B_TRUE);
}

	__checkReturn	efx_rc_t
efx_tunnel_config_clear(
	__in			efx_nic_t *enp)
{
	efx_tunnel_cfg_t *etcp = &enp->en_tunnel_cfg;
	efsys_lock_state_t state;
	unsigned int i;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_TUNNEL);

	EFSYS_LOCK(enp->en_eslp, state);

	if (efx_tunnel_table_all_available(etcp) == B_FALSE) {
		rc = EBUSY;
		goto fail1;
	}

	i = 0;
	while (i < etcp->etc_udp_entries_num)
		i = efx_tunnel_config_udp_remove_prepare(etcp, i);

	EFSYS_UNLOCK(enp->en_eslp, state);

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	EFSYS_UNLOCK(enp->en_eslp, state);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_tunnel_reconfigure(
	__in		efx_nic_t *enp)
{
	const efx_tunnel_ops_t *etop = enp->en_etop;
	efx_tunnel_cfg_t *etcp = &enp->en_tunnel_cfg;
	efx_tunnel_udp_entry_t *entry;
	boolean_t locked = B_FALSE;
	efsys_lock_state_t state;
	boolean_t resetting;
	unsigned int i;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_TUNNEL);

	if (etop->eto_reconfigure == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}

	EFSYS_LOCK(enp->en_eslp, state);
	locked = B_TRUE;

	if (efx_tunnel_table_all_available(etcp) == B_FALSE) {
		rc = EBUSY;
		goto fail2;
	}

	for (i = 0; i < etcp->etc_udp_entries_num; i++) {
		entry = &etcp->etc_udp_entries[i];
		if (entry->etue_state != EFX_TUNNEL_UDP_ENTRY_APPLIED)
			entry->etue_busy = B_TRUE;
	}

	EFSYS_UNLOCK(enp->en_eslp, state);
	locked = B_FALSE;

	rc = enp->en_etop->eto_reconfigure(enp);
	if (rc != 0 && rc != EAGAIN)
		goto fail3;

	resetting = (rc == EAGAIN) ? B_TRUE : B_FALSE;

	EFSYS_LOCK(enp->en_eslp, state);
	locked = B_TRUE;

	/*
	 * Delete entries marked for removal since they are no longer
	 * needed after successful NIC-specific reconfiguration.
	 * Added entries become applied because they are installed in
	 * the hardware.
	 */

	i = 0;
	while (i < etcp->etc_udp_entries_num) {
		unsigned int next = i + 1;

		entry = &etcp->etc_udp_entries[i];
		if (entry->etue_busy != B_FALSE) {
			entry->etue_busy = B_FALSE;

			switch (entry->etue_state) {
			case EFX_TUNNEL_UDP_ENTRY_APPLIED:
				break;
			case EFX_TUNNEL_UDP_ENTRY_ADDED:
				entry->etue_state =
				    EFX_TUNNEL_UDP_ENTRY_APPLIED;
				break;
			case EFX_TUNNEL_UDP_ENTRY_REMOVED:
				next = efx_tunnel_config_udp_do_remove(etcp, i);
				break;
			default:
				EFSYS_ASSERT(0);
				break;
			}
		}

		i = next;
	}

	EFSYS_UNLOCK(enp->en_eslp, state);
	locked = B_FALSE;

	return ((resetting == B_FALSE) ? 0 : EAGAIN);

fail3:
	EFSYS_PROBE(fail3);

	EFSYS_ASSERT(locked == B_FALSE);
	EFSYS_LOCK(enp->en_eslp, state);

	for (i = 0; i < etcp->etc_udp_entries_num; i++)
		etcp->etc_udp_entries[i].etue_busy = B_FALSE;

	EFSYS_UNLOCK(enp->en_eslp, state);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	if (locked)
		EFSYS_UNLOCK(enp->en_eslp, state);

	return (rc);
}

#if EFSYS_OPT_MEDFORD || EFSYS_OPT_MEDFORD2
static	__checkReturn		boolean_t
ef10_udp_encap_supported(
	__in		efx_nic_t *enp)
{
	const efx_nic_cfg_t *encp = &enp->en_nic_cfg;
	uint32_t udp_tunnels_mask = 0;

	udp_tunnels_mask |= (1u << EFX_TUNNEL_PROTOCOL_VXLAN);
	udp_tunnels_mask |= (1u << EFX_TUNNEL_PROTOCOL_GENEVE);

	return ((encp->enc_tunnel_encapsulations_supported &
	    udp_tunnels_mask) == 0 ? B_FALSE : B_TRUE);
}

static	__checkReturn	efx_rc_t
ef10_tunnel_reconfigure(
	__in		efx_nic_t *enp)
{
	efx_tunnel_cfg_t *etcp = &enp->en_tunnel_cfg;
	efx_rc_t rc;
	boolean_t resetting = B_FALSE;
	efsys_lock_state_t state;
	efx_tunnel_cfg_t etc;

	EFSYS_LOCK(enp->en_eslp, state);
	memcpy(&etc, etcp, sizeof (etc));
	EFSYS_UNLOCK(enp->en_eslp, state);

	if (ef10_udp_encap_supported(enp) == B_FALSE) {
		/*
		 * It is OK to apply empty UDP tunnel ports when UDP
		 * tunnel encapsulations are not supported - just nothing
		 * should be done.
		 */
		if (etc.etc_udp_entries_num == 0)
			return (0);
		rc = ENOTSUP;
		goto fail1;
	} else {
		/*
		 * All PCI functions can see a reset upon the
		 * MCDI request completion
		 */
		rc = efx_mcdi_set_tunnel_encap_udp_ports(enp, &etc, B_FALSE,
		    &resetting);
		if (rc != 0) {
			/*
			 * Do not fail if the access is denied when no
			 * tunnel encap UDP ports are configured.
			 */
			if (rc != EACCES || etc.etc_udp_entries_num != 0)
				goto fail2;
		}

		/*
		 * Although the caller should be able to handle MC reboot,
		 * it might come in handy to report the impending reboot
		 * by returning EAGAIN
		 */
		return ((resetting) ? EAGAIN : 0);
	}
fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

static			void
ef10_tunnel_fini(
	__in		efx_nic_t *enp)
{
	boolean_t resetting;

	if (ef10_udp_encap_supported(enp) != B_FALSE) {
		/*
		 * The UNLOADING flag allows the MC to suppress the datapath
		 * reset if it was set on the last call to
		 * MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS by all functions
		 */
		(void) efx_mcdi_set_tunnel_encap_udp_ports(enp, NULL, B_TRUE,
		    &resetting);
	}
}
#endif /* EFSYS_OPT_MEDFORD || EFSYS_OPT_MEDFORD2 */

#endif /* EFSYS_OPT_TUNNEL */
