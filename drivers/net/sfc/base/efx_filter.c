/*
 * Copyright (c) 2007-2016 Solarflare Communications Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of the FreeBSD Project.
 */

#include "efx.h"
#include "efx_impl.h"


#if EFSYS_OPT_FILTER

	__checkReturn	efx_rc_t
efx_filter_insert(
	__in		efx_nic_t *enp,
	__inout		efx_filter_spec_t *spec)
{
	const efx_filter_ops_t *efop = enp->en_efop;

	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_FILTER);
	EFSYS_ASSERT3P(spec, !=, NULL);
	EFSYS_ASSERT3U(spec->efs_flags, &, EFX_FILTER_FLAG_RX);

	return (efop->efo_add(enp, spec, B_FALSE));
}

	__checkReturn	efx_rc_t
efx_filter_remove(
	__in		efx_nic_t *enp,
	__inout		efx_filter_spec_t *spec)
{
	const efx_filter_ops_t *efop = enp->en_efop;

	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_FILTER);
	EFSYS_ASSERT3P(spec, !=, NULL);
	EFSYS_ASSERT3U(spec->efs_flags, &, EFX_FILTER_FLAG_RX);

	return (efop->efo_delete(enp, spec));
}

	__checkReturn	efx_rc_t
efx_filter_restore(
	__in		efx_nic_t *enp)
{
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_FILTER);

	if ((rc = enp->en_efop->efo_restore(enp)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_filter_init(
	__in		efx_nic_t *enp)
{
	const efx_filter_ops_t *efop;
	efx_rc_t rc;

	/* Check that efx_filter_spec_t is 64 bytes. */
	EFX_STATIC_ASSERT(sizeof (efx_filter_spec_t) == 64);

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_FILTER));

	switch (enp->en_family) {

	default:
		EFSYS_ASSERT(0);
		rc = ENOTSUP;
		goto fail1;
	}

	if ((rc = efop->efo_init(enp)) != 0)
		goto fail2;

	enp->en_efop = efop;
	enp->en_mod_flags |= EFX_MOD_FILTER;
	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	enp->en_efop = NULL;
	enp->en_mod_flags &= ~EFX_MOD_FILTER;
	return (rc);
}

			void
efx_filter_fini(
	__in		efx_nic_t *enp)
{
	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_FILTER);

	enp->en_efop->efo_fini(enp);

	enp->en_efop = NULL;
	enp->en_mod_flags &= ~EFX_MOD_FILTER;
}

	__checkReturn	efx_rc_t
efx_filter_supported_filters(
	__in		efx_nic_t *enp,
	__out		uint32_t *list,
	__out		size_t *length)
{
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_FILTER);
	EFSYS_ASSERT(enp->en_efop->efo_supported_filters != NULL);

	if ((rc = enp->en_efop->efo_supported_filters(enp, list, length)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_filter_reconfigure(
	__in				efx_nic_t *enp,
	__in_ecount(6)			uint8_t const *mac_addr,
	__in				boolean_t all_unicst,
	__in				boolean_t mulcst,
	__in				boolean_t all_mulcst,
	__in				boolean_t brdcst,
	__in_ecount(6*count)		uint8_t const *addrs,
	__in				uint32_t count)
{
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_FILTER);

	if (enp->en_efop->efo_reconfigure != NULL) {
		if ((rc = enp->en_efop->efo_reconfigure(enp, mac_addr,
							all_unicst, mulcst,
							all_mulcst, brdcst,
							addrs, count)) != 0)
			goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

		void
efx_filter_spec_init_rx(
	__out		efx_filter_spec_t *spec,
	__in		efx_filter_priority_t priority,
	__in		efx_filter_flags_t flags,
	__in		efx_rxq_t *erp)
{
	EFSYS_ASSERT3P(spec, !=, NULL);
	EFSYS_ASSERT3P(erp, !=, NULL);
	EFSYS_ASSERT((flags & ~(EFX_FILTER_FLAG_RX_RSS |
				EFX_FILTER_FLAG_RX_SCATTER)) == 0);

	memset(spec, 0, sizeof (*spec));
	spec->efs_priority = priority;
	spec->efs_flags = EFX_FILTER_FLAG_RX | flags;
	spec->efs_rss_context = EFX_FILTER_SPEC_RSS_CONTEXT_DEFAULT;
	spec->efs_dmaq_id = (uint16_t)erp->er_index;
}

		void
efx_filter_spec_init_tx(
	__out		efx_filter_spec_t *spec,
	__in		efx_txq_t *etp)
{
	EFSYS_ASSERT3P(spec, !=, NULL);
	EFSYS_ASSERT3P(etp, !=, NULL);

	memset(spec, 0, sizeof (*spec));
	spec->efs_priority = EFX_FILTER_PRI_REQUIRED;
	spec->efs_flags = EFX_FILTER_FLAG_TX;
	spec->efs_dmaq_id = (uint16_t)etp->et_index;
}


/*
 *  Specify IPv4 host, transport protocol and port in a filter specification
 */
__checkReturn		efx_rc_t
efx_filter_spec_set_ipv4_local(
	__inout		efx_filter_spec_t *spec,
	__in		uint8_t proto,
	__in		uint32_t host,
	__in		uint16_t port)
{
	EFSYS_ASSERT3P(spec, !=, NULL);

	spec->efs_match_flags |=
		EFX_FILTER_MATCH_ETHER_TYPE | EFX_FILTER_MATCH_IP_PROTO |
		EFX_FILTER_MATCH_LOC_HOST | EFX_FILTER_MATCH_LOC_PORT;
	spec->efs_ether_type = EFX_ETHER_TYPE_IPV4;
	spec->efs_ip_proto = proto;
	spec->efs_loc_host.eo_u32[0] = host;
	spec->efs_loc_port = port;
	return (0);
}

/*
 * Specify IPv4 hosts, transport protocol and ports in a filter specification
 */
__checkReturn		efx_rc_t
efx_filter_spec_set_ipv4_full(
	__inout		efx_filter_spec_t *spec,
	__in		uint8_t proto,
	__in		uint32_t lhost,
	__in		uint16_t lport,
	__in		uint32_t rhost,
	__in		uint16_t rport)
{
	EFSYS_ASSERT3P(spec, !=, NULL);

	spec->efs_match_flags |=
		EFX_FILTER_MATCH_ETHER_TYPE | EFX_FILTER_MATCH_IP_PROTO |
		EFX_FILTER_MATCH_LOC_HOST | EFX_FILTER_MATCH_LOC_PORT |
		EFX_FILTER_MATCH_REM_HOST | EFX_FILTER_MATCH_REM_PORT;
	spec->efs_ether_type = EFX_ETHER_TYPE_IPV4;
	spec->efs_ip_proto = proto;
	spec->efs_loc_host.eo_u32[0] = lhost;
	spec->efs_loc_port = lport;
	spec->efs_rem_host.eo_u32[0] = rhost;
	spec->efs_rem_port = rport;
	return (0);
}

/*
 * Specify local Ethernet address and/or VID in filter specification
 */
__checkReturn		efx_rc_t
efx_filter_spec_set_eth_local(
	__inout		efx_filter_spec_t *spec,
	__in		uint16_t vid,
	__in		const uint8_t *addr)
{
	EFSYS_ASSERT3P(spec, !=, NULL);
	EFSYS_ASSERT3P(addr, !=, NULL);

	if (vid == EFX_FILTER_SPEC_VID_UNSPEC && addr == NULL)
		return (EINVAL);

	if (vid != EFX_FILTER_SPEC_VID_UNSPEC) {
		spec->efs_match_flags |= EFX_FILTER_MATCH_OUTER_VID;
		spec->efs_outer_vid = vid;
	}
	if (addr != NULL) {
		spec->efs_match_flags |= EFX_FILTER_MATCH_LOC_MAC;
		memcpy(spec->efs_loc_mac, addr, EFX_MAC_ADDR_LEN);
	}
	return (0);
}

/*
 * Specify matching otherwise-unmatched unicast in a filter specification
 */
__checkReturn		efx_rc_t
efx_filter_spec_set_uc_def(
	__inout		efx_filter_spec_t *spec)
{
	EFSYS_ASSERT3P(spec, !=, NULL);

	spec->efs_match_flags |= EFX_FILTER_MATCH_LOC_MAC_IG;
	return (0);
}

/*
 * Specify matching otherwise-unmatched multicast in a filter specification
 */
__checkReturn		efx_rc_t
efx_filter_spec_set_mc_def(
	__inout		efx_filter_spec_t *spec)
{
	EFSYS_ASSERT3P(spec, !=, NULL);

	spec->efs_match_flags |= EFX_FILTER_MATCH_LOC_MAC_IG;
	spec->efs_loc_mac[0] = 1;
	return (0);
}



#endif /* EFSYS_OPT_FILTER */
