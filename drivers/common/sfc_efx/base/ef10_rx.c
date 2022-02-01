/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2012-2019 Solarflare Communications Inc.
 */

#include "efx.h"
#include "efx_impl.h"


#if EFSYS_OPT_RIVERHEAD || EFX_OPTS_EF10()

#if EFSYS_OPT_RX_SCALE
static	__checkReturn	efx_rc_t
efx_mcdi_rss_context_alloc(
	__in		efx_nic_t *enp,
	__in		efx_rx_scale_context_type_t type,
	__in		uint32_t num_queues,
	__in		uint32_t table_nentries,
	__out		uint32_t *rss_contextp)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(enp);
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_RSS_CONTEXT_ALLOC_V2_IN_LEN,
		MC_CMD_RSS_CONTEXT_ALLOC_OUT_LEN);
	uint32_t table_nentries_min;
	uint32_t table_nentries_max;
	uint32_t num_queues_max;
	uint32_t rss_context;
	uint32_t context_type;
	efx_rc_t rc;

	switch (type) {
	case EFX_RX_SCALE_EXCLUSIVE:
		context_type = MC_CMD_RSS_CONTEXT_ALLOC_IN_TYPE_EXCLUSIVE;
		num_queues_max = encp->enc_rx_scale_indirection_max_nqueues;
		table_nentries_min = encp->enc_rx_scale_tbl_min_nentries;
		table_nentries_max = encp->enc_rx_scale_tbl_max_nentries;
		break;
	case EFX_RX_SCALE_SHARED:
		context_type = MC_CMD_RSS_CONTEXT_ALLOC_IN_TYPE_SHARED;
		num_queues_max = encp->enc_rx_scale_indirection_max_nqueues;
		table_nentries_min = encp->enc_rx_scale_tbl_min_nentries;
		table_nentries_max = encp->enc_rx_scale_tbl_max_nentries;
		break;
	case EFX_RX_SCALE_EVEN_SPREAD:
		context_type = MC_CMD_RSS_CONTEXT_ALLOC_IN_TYPE_EVEN_SPREADING;
		num_queues_max = encp->enc_rx_scale_even_spread_max_nqueues;
		table_nentries_min = 0;
		table_nentries_max = 0;
		break;
	default:
		rc = EINVAL;
		goto fail1;
	}

	if (num_queues == 0 || num_queues > num_queues_max) {
		rc = EINVAL;
		goto fail2;
	}

	if (table_nentries < table_nentries_min ||
	    table_nentries > table_nentries_max ||
	    (table_nentries != 0 && !ISP2(table_nentries))) {
		rc = EINVAL;
		goto fail3;
	}

	req.emr_cmd = MC_CMD_RSS_CONTEXT_ALLOC;
	req.emr_in_buf = payload;
	req.emr_in_length =
	    (encp->enc_rx_scale_tbl_entry_count_is_selectable != B_FALSE) ?
	    MC_CMD_RSS_CONTEXT_ALLOC_V2_IN_LEN :
	    MC_CMD_RSS_CONTEXT_ALLOC_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_RSS_CONTEXT_ALLOC_OUT_LEN;

	MCDI_IN_SET_DWORD(req, RSS_CONTEXT_ALLOC_IN_UPSTREAM_PORT_ID,
		enp->en_vport_id);
	MCDI_IN_SET_DWORD(req, RSS_CONTEXT_ALLOC_IN_TYPE, context_type);

	/*
	 * For exclusive contexts, NUM_QUEUES is only used to validate
	 * indirection table offsets.
	 * For shared contexts, the provided context will spread traffic over
	 * NUM_QUEUES many queues.
	 * For the even spread contexts, the provided context will spread
	 * traffic over NUM_QUEUES many queues, but that will not involve
	 * the use of precious indirection table resources in the adapter.
	 */
	MCDI_IN_SET_DWORD(req, RSS_CONTEXT_ALLOC_IN_NUM_QUEUES, num_queues);

	if (encp->enc_rx_scale_tbl_entry_count_is_selectable != B_FALSE) {
		MCDI_IN_SET_DWORD(req,
		    RSS_CONTEXT_ALLOC_V2_IN_INDIRECTION_TABLE_SIZE,
		    table_nentries);
	}

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail4;
	}

	if (req.emr_out_length_used < MC_CMD_RSS_CONTEXT_ALLOC_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail5;
	}

	rss_context = MCDI_OUT_DWORD(req, RSS_CONTEXT_ALLOC_OUT_RSS_CONTEXT_ID);
	if (rss_context == EF10_RSS_CONTEXT_INVALID) {
		rc = ENOENT;
		goto fail6;
	}

	*rss_contextp = rss_context;

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
#endif /* EFSYS_OPT_RX_SCALE */

#if EFSYS_OPT_RX_SCALE
static			efx_rc_t
efx_mcdi_rss_context_free(
	__in		efx_nic_t *enp,
	__in		uint32_t rss_context)
{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_RSS_CONTEXT_FREE_IN_LEN,
		MC_CMD_RSS_CONTEXT_FREE_OUT_LEN);
	efx_rc_t rc;

	if (rss_context == EF10_RSS_CONTEXT_INVALID) {
		rc = EINVAL;
		goto fail1;
	}

	req.emr_cmd = MC_CMD_RSS_CONTEXT_FREE;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_RSS_CONTEXT_FREE_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_RSS_CONTEXT_FREE_OUT_LEN;

	MCDI_IN_SET_DWORD(req, RSS_CONTEXT_FREE_IN_RSS_CONTEXT_ID, rss_context);

	efx_mcdi_execute_quiet(enp, &req);

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
#endif /* EFSYS_OPT_RX_SCALE */

#if EFSYS_OPT_RX_SCALE
static			efx_rc_t
efx_mcdi_rss_context_set_flags(
	__in		efx_nic_t *enp,
	__in		uint32_t rss_context,
	__in		efx_rx_hash_type_t type)
{
	efx_nic_cfg_t *encp = &enp->en_nic_cfg;
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_RSS_CONTEXT_SET_FLAGS_IN_LEN,
		MC_CMD_RSS_CONTEXT_SET_FLAGS_OUT_LEN);
	efx_rc_t rc;

	EFX_STATIC_ASSERT(EFX_RX_CLASS_IPV4_TCP_LBN ==
		    MC_CMD_RSS_CONTEXT_SET_FLAGS_IN_TCP_IPV4_RSS_MODE_LBN);
	EFX_STATIC_ASSERT(EFX_RX_CLASS_IPV4_TCP_WIDTH ==
		    MC_CMD_RSS_CONTEXT_SET_FLAGS_IN_TCP_IPV4_RSS_MODE_WIDTH);
	EFX_STATIC_ASSERT(EFX_RX_CLASS_IPV4_LBN ==
		    MC_CMD_RSS_CONTEXT_SET_FLAGS_IN_OTHER_IPV4_RSS_MODE_LBN);
	EFX_STATIC_ASSERT(EFX_RX_CLASS_IPV4_WIDTH ==
		    MC_CMD_RSS_CONTEXT_SET_FLAGS_IN_OTHER_IPV4_RSS_MODE_WIDTH);
	EFX_STATIC_ASSERT(EFX_RX_CLASS_IPV6_TCP_LBN ==
		    MC_CMD_RSS_CONTEXT_SET_FLAGS_IN_TCP_IPV6_RSS_MODE_LBN);
	EFX_STATIC_ASSERT(EFX_RX_CLASS_IPV6_TCP_WIDTH ==
		    MC_CMD_RSS_CONTEXT_SET_FLAGS_IN_TCP_IPV6_RSS_MODE_WIDTH);
	EFX_STATIC_ASSERT(EFX_RX_CLASS_IPV6_LBN ==
		    MC_CMD_RSS_CONTEXT_SET_FLAGS_IN_OTHER_IPV6_RSS_MODE_LBN);
	EFX_STATIC_ASSERT(EFX_RX_CLASS_IPV6_WIDTH ==
		    MC_CMD_RSS_CONTEXT_SET_FLAGS_IN_OTHER_IPV6_RSS_MODE_WIDTH);

	if (rss_context == EF10_RSS_CONTEXT_INVALID) {
		rc = EINVAL;
		goto fail1;
	}

	req.emr_cmd = MC_CMD_RSS_CONTEXT_SET_FLAGS;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_RSS_CONTEXT_SET_FLAGS_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_RSS_CONTEXT_SET_FLAGS_OUT_LEN;

	MCDI_IN_SET_DWORD(req, RSS_CONTEXT_SET_FLAGS_IN_RSS_CONTEXT_ID,
	    rss_context);

	/*
	 * If the firmware lacks support for additional modes, RSS_MODE
	 * fields must contain zeros, otherwise the operation will fail.
	 */
	if (encp->enc_rx_scale_additional_modes_supported == B_FALSE)
		type &= EFX_RX_HASH_LEGACY_MASK;

	MCDI_IN_POPULATE_DWORD_10(req, RSS_CONTEXT_SET_FLAGS_IN_FLAGS,
	    RSS_CONTEXT_SET_FLAGS_IN_TOEPLITZ_IPV4_EN,
	    (type & EFX_RX_HASH_IPV4) ? 1 : 0,
	    RSS_CONTEXT_SET_FLAGS_IN_TOEPLITZ_TCPV4_EN,
	    (type & EFX_RX_HASH_TCPIPV4) ? 1 : 0,
	    RSS_CONTEXT_SET_FLAGS_IN_TOEPLITZ_IPV6_EN,
	    (type & EFX_RX_HASH_IPV6) ? 1 : 0,
	    RSS_CONTEXT_SET_FLAGS_IN_TOEPLITZ_TCPV6_EN,
	    (type & EFX_RX_HASH_TCPIPV6) ? 1 : 0,
	    RSS_CONTEXT_SET_FLAGS_IN_TCP_IPV4_RSS_MODE,
	    (type >> EFX_RX_CLASS_IPV4_TCP_LBN) &
	    EFX_MASK32(EFX_RX_CLASS_IPV4_TCP),
	    RSS_CONTEXT_SET_FLAGS_IN_UDP_IPV4_RSS_MODE,
	    (type >> EFX_RX_CLASS_IPV4_UDP_LBN) &
	    EFX_MASK32(EFX_RX_CLASS_IPV4_UDP),
	    RSS_CONTEXT_SET_FLAGS_IN_OTHER_IPV4_RSS_MODE,
	    (type >> EFX_RX_CLASS_IPV4_LBN) & EFX_MASK32(EFX_RX_CLASS_IPV4),
	    RSS_CONTEXT_SET_FLAGS_IN_TCP_IPV6_RSS_MODE,
	    (type >> EFX_RX_CLASS_IPV6_TCP_LBN) &
	    EFX_MASK32(EFX_RX_CLASS_IPV6_TCP),
	    RSS_CONTEXT_SET_FLAGS_IN_UDP_IPV6_RSS_MODE,
	    (type >> EFX_RX_CLASS_IPV6_UDP_LBN) &
	    EFX_MASK32(EFX_RX_CLASS_IPV6_UDP),
	    RSS_CONTEXT_SET_FLAGS_IN_OTHER_IPV6_RSS_MODE,
	    (type >> EFX_RX_CLASS_IPV6_LBN) & EFX_MASK32(EFX_RX_CLASS_IPV6));

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
#endif /* EFSYS_OPT_RX_SCALE */

#if EFSYS_OPT_RX_SCALE
static			efx_rc_t
efx_mcdi_rss_context_set_key(
	__in		efx_nic_t *enp,
	__in		uint32_t rss_context,
	__in_ecount(n)	uint8_t *key,
	__in		size_t n)
{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_RSS_CONTEXT_SET_KEY_IN_LEN,
		MC_CMD_RSS_CONTEXT_SET_KEY_OUT_LEN);
	efx_rc_t rc;

	if (rss_context == EF10_RSS_CONTEXT_INVALID) {
		rc = EINVAL;
		goto fail1;
	}

	req.emr_cmd = MC_CMD_RSS_CONTEXT_SET_KEY;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_RSS_CONTEXT_SET_KEY_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_RSS_CONTEXT_SET_KEY_OUT_LEN;

	MCDI_IN_SET_DWORD(req, RSS_CONTEXT_SET_KEY_IN_RSS_CONTEXT_ID,
	    rss_context);

	EFSYS_ASSERT3U(n, ==, MC_CMD_RSS_CONTEXT_SET_KEY_IN_TOEPLITZ_KEY_LEN);
	if (n != MC_CMD_RSS_CONTEXT_SET_KEY_IN_TOEPLITZ_KEY_LEN) {
		rc = EINVAL;
		goto fail2;
	}

	memcpy(MCDI_IN2(req, uint8_t, RSS_CONTEXT_SET_KEY_IN_TOEPLITZ_KEY),
	    key, n);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail3;
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
#endif /* EFSYS_OPT_RX_SCALE */

#if EFSYS_OPT_RX_SCALE
static				efx_rc_t
efx_mcdi_rss_context_set_table(
	__in			efx_nic_t *enp,
	__in			uint32_t rss_context,
	__in_ecount(nentries)	unsigned int *table,
	__in			size_t nentries)
{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_RSS_CONTEXT_SET_TABLE_IN_LEN,
		MC_CMD_RSS_CONTEXT_SET_TABLE_OUT_LEN);
	uint8_t *req_table;
	int i, rc;

	if (rss_context == EF10_RSS_CONTEXT_INVALID) {
		rc = EINVAL;
		goto fail1;
	}

	req.emr_cmd = MC_CMD_RSS_CONTEXT_SET_TABLE;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_RSS_CONTEXT_SET_TABLE_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_RSS_CONTEXT_SET_TABLE_OUT_LEN;

	MCDI_IN_SET_DWORD(req, RSS_CONTEXT_SET_TABLE_IN_RSS_CONTEXT_ID,
	    rss_context);

	req_table =
	    MCDI_IN2(req, uint8_t, RSS_CONTEXT_SET_TABLE_IN_INDIRECTION_TABLE);

	for (i = 0;
	    i < MC_CMD_RSS_CONTEXT_SET_TABLE_IN_INDIRECTION_TABLE_LEN;
	    i++) {
		req_table[i] = (nentries > 0) ?
		    (uint8_t)table[i % nentries] : 0;
	}

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
#endif /* EFSYS_OPT_RX_SCALE */

#if EFSYS_OPT_RX_SCALE
static	__checkReturn		efx_rc_t
efx_mcdi_rss_context_write_table(
	__in			efx_nic_t *enp,
	__in			uint32_t context,
	__in			unsigned int start_idx,
	__in_ecount(nentries)	unsigned int *table,
	__in			unsigned int nentries)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(enp);
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload,
	     MC_CMD_RSS_CONTEXT_WRITE_TABLE_IN_LENMAX_MCDI2,
	     MC_CMD_RSS_CONTEXT_WRITE_TABLE_OUT_LEN);
	unsigned int i;
	int rc;

	if (nentries >
	    MC_CMD_RSS_CONTEXT_WRITE_TABLE_IN_ENTRIES_MAXNUM_MCDI2) {
		rc = EINVAL;
		goto fail1;
	}

	if (start_idx + nentries >
	    encp->enc_rx_scale_tbl_max_nentries) {
		rc = EINVAL;
		goto fail2;
	}

	req.emr_cmd = MC_CMD_RSS_CONTEXT_WRITE_TABLE;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_RSS_CONTEXT_WRITE_TABLE_IN_LEN(nentries);
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_RSS_CONTEXT_WRITE_TABLE_OUT_LEN;

	MCDI_IN_SET_DWORD(req,
	    RSS_CONTEXT_WRITE_TABLE_IN_RSS_CONTEXT_ID, context);

	for (i = 0; i < nentries; ++i) {
		if (table[i] >= encp->enc_rx_scale_indirection_max_nqueues) {
			rc = EINVAL;
			goto fail3;
		}

		MCDI_IN_POPULATE_INDEXED_DWORD_2(req,
		    RSS_CONTEXT_WRITE_TABLE_IN_ENTRIES, i,
		    RSS_CONTEXT_WRITE_TABLE_ENTRY_INDEX, start_idx + i,
		    RSS_CONTEXT_WRITE_TABLE_ENTRY_VALUE, table[i]);
	}

	efx_mcdi_execute(enp, &req);
	if (req.emr_rc != 0) {
		rc = req.emr_rc;
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
#endif /* EFSYS_OPT_RX_SCALE */


	__checkReturn	efx_rc_t
ef10_rx_init(
	__in		efx_nic_t *enp)
{
#if EFSYS_OPT_RX_SCALE

	if (efx_mcdi_rss_context_alloc(enp, EFX_RX_SCALE_EXCLUSIVE, EFX_MAXRSS,
		EFX_RSS_TBL_SIZE, &enp->en_rss_context) == 0) {
		/*
		 * Allocated an exclusive RSS context, which allows both the
		 * indirection table and key to be modified.
		 */
		enp->en_rss_context_type = EFX_RX_SCALE_EXCLUSIVE;
		enp->en_hash_support = EFX_RX_HASH_AVAILABLE;
	} else {
		/*
		 * Failed to allocate an exclusive RSS context. Continue
		 * operation without support for RSS. The pseudo-header in
		 * received packets will not contain a Toeplitz hash value.
		 */
		enp->en_rss_context_type = EFX_RX_SCALE_UNAVAILABLE;
		enp->en_hash_support = EFX_RX_HASH_UNAVAILABLE;
	}

#endif /* EFSYS_OPT_RX_SCALE */

	return (0);
}

#if EFX_OPTS_EF10()

#if EFSYS_OPT_RX_SCATTER
	__checkReturn	efx_rc_t
ef10_rx_scatter_enable(
	__in		efx_nic_t *enp,
	__in		unsigned int buf_size)
{
	_NOTE(ARGUNUSED(enp, buf_size))
	return (0);
}
#endif	/* EFSYS_OPT_RX_SCATTER */

#endif	/* EFX_OPTS_EF10() */

#if EFSYS_OPT_RX_SCALE
	__checkReturn	efx_rc_t
ef10_rx_scale_context_alloc(
	__in		efx_nic_t *enp,
	__in		efx_rx_scale_context_type_t type,
	__in		uint32_t num_queues,
	__in		uint32_t table_nentries,
	__out		uint32_t *rss_contextp)
{
	efx_rc_t rc;

	rc = efx_mcdi_rss_context_alloc(enp, type, num_queues, table_nentries,
					rss_contextp);
	if (rc != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}
#endif /* EFSYS_OPT_RX_SCALE */

#if EFSYS_OPT_RX_SCALE
	__checkReturn	efx_rc_t
ef10_rx_scale_context_free(
	__in		efx_nic_t *enp,
	__in		uint32_t rss_context)
{
	efx_rc_t rc;

	rc = efx_mcdi_rss_context_free(enp, rss_context);
	if (rc != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}
#endif /* EFSYS_OPT_RX_SCALE */

#if EFSYS_OPT_RX_SCALE
	__checkReturn	efx_rc_t
ef10_rx_scale_mode_set(
	__in		efx_nic_t *enp,
	__in		uint32_t rss_context,
	__in		efx_rx_hash_alg_t alg,
	__in		efx_rx_hash_type_t type,
	__in		boolean_t insert)
{
	efx_nic_cfg_t *encp = &enp->en_nic_cfg;
	efx_rc_t rc;

	EFSYS_ASSERT3U(insert, ==, B_TRUE);

	if ((encp->enc_rx_scale_hash_alg_mask & (1U << alg)) == 0 ||
	    insert == B_FALSE) {
		rc = EINVAL;
		goto fail1;
	}

	if (rss_context == EFX_RSS_CONTEXT_DEFAULT) {
		if (enp->en_rss_context_type == EFX_RX_SCALE_UNAVAILABLE) {
			rc = ENOTSUP;
			goto fail2;
		}
		rss_context = enp->en_rss_context;
	}

	if ((rc = efx_mcdi_rss_context_set_flags(enp,
		    rss_context, type)) != 0)
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
#endif /* EFSYS_OPT_RX_SCALE */

#if EFSYS_OPT_RX_SCALE
	__checkReturn	efx_rc_t
ef10_rx_scale_key_set(
	__in		efx_nic_t *enp,
	__in		uint32_t rss_context,
	__in_ecount(n)	uint8_t *key,
	__in		size_t n)
{
	efx_rc_t rc;

	EFX_STATIC_ASSERT(EFX_RSS_KEY_SIZE ==
	    MC_CMD_RSS_CONTEXT_SET_KEY_IN_TOEPLITZ_KEY_LEN);

	if (rss_context == EFX_RSS_CONTEXT_DEFAULT) {
		if (enp->en_rss_context_type == EFX_RX_SCALE_UNAVAILABLE) {
			rc = ENOTSUP;
			goto fail1;
		}
		rss_context = enp->en_rss_context;
	}

	if ((rc = efx_mcdi_rss_context_set_key(enp, rss_context, key, n)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}
#endif /* EFSYS_OPT_RX_SCALE */

#if EFSYS_OPT_RX_SCALE
	__checkReturn		efx_rc_t
ef10_rx_scale_tbl_set(
	__in			efx_nic_t *enp,
	__in			uint32_t rss_context,
	__in_ecount(nentries)	unsigned int *table,
	__in			size_t nentries)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(enp);
	efx_rc_t rc;


	if (rss_context == EFX_RSS_CONTEXT_DEFAULT) {
		if (enp->en_rss_context_type == EFX_RX_SCALE_UNAVAILABLE) {
			rc = ENOTSUP;
			goto fail1;
		}
		rss_context = enp->en_rss_context;
	}

	if (encp->enc_rx_scale_tbl_entry_count_is_selectable != B_FALSE) {
		uint32_t index, remain, batch;

		batch = MC_CMD_RSS_CONTEXT_WRITE_TABLE_IN_ENTRIES_MAXNUM_MCDI2;
		index = 0;

		for (remain = nentries; remain > 0; remain -= batch) {
			if (batch > remain)
				batch = remain;

			rc = efx_mcdi_rss_context_write_table(enp, rss_context,
				    index, &table[index], batch);
			if (rc != 0)
				goto fail2;

			index += batch;
		}
	} else {
		rc = efx_mcdi_rss_context_set_table(enp, rss_context, table,
			    nentries);
		if (rc != 0)
			goto fail3;
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
#endif /* EFSYS_OPT_RX_SCALE */

#if EFX_OPTS_EF10()

/*
 * EF10 RX pseudo-header (aka Rx prefix)
 * -------------------------------------
 *
 * Receive packets are prefixed by an (optional) 14 byte pseudo-header:
 *
 *  +00: Toeplitz hash value.
 *       (32bit little-endian)
 *  +04: Outer VLAN tag. Zero if the packet did not have an outer VLAN tag.
 *       (16bit big-endian)
 *  +06: Inner VLAN tag. Zero if the packet did not have an inner VLAN tag.
 *       (16bit big-endian)
 *  +08: Packet Length. Zero if the RX datapath was in cut-through mode.
 *       (16bit little-endian)
 *  +10: MAC timestamp. Zero if timestamping is not enabled.
 *       (32bit little-endian)
 *
 * See "The RX Pseudo-header" in SF-109306-TC.
 *
 * EF10 does not support Rx prefix choice using MC_CMD_GET_RX_PREFIX_ID
 * and query its layout using MC_CMD_QUERY_RX_PREFIX_ID.
 */
static const efx_rx_prefix_layout_t ef10_default_rx_prefix_layout = {
	.erpl_id	= 0,
	.erpl_length	= 14,
	.erpl_fields	= {
		[EFX_RX_PREFIX_FIELD_RSS_HASH]			=
		    { 0,  32, B_FALSE },
		[EFX_RX_PREFIX_FIELD_VLAN_STRIP_TCI]		=
		    { 32, 16, B_TRUE },
		[EFX_RX_PREFIX_FIELD_INNER_VLAN_STRIP_TCI]	=
		    { 48, 16, B_TRUE },
		[EFX_RX_PREFIX_FIELD_LENGTH]			=
		    { 64, 16, B_FALSE },
		[EFX_RX_PREFIX_FIELD_PARTIAL_TSTAMP]		=
		    { 80, 32, B_FALSE },
	}
};

#if EFSYS_OPT_RX_PACKED_STREAM

/*
 * EF10 packed stream Rx prefix layout.
 *
 * See SF-112241-TC Full speed capture for Huntington and Medford section 4.5.
 */
static const efx_rx_prefix_layout_t ef10_packed_stream_rx_prefix_layout = {
	.erpl_id	= 0,
	.erpl_length	= 8,
	.erpl_fields	= {
#define	EF10_PS_RX_PREFIX_FIELD(_efx, _ef10) \
	EFX_RX_PREFIX_FIELD(_efx, ES_DZ_PS_RX_PREFIX_ ## _ef10, B_FALSE)

		EF10_PS_RX_PREFIX_FIELD(PARTIAL_TSTAMP, TSTAMP),
		EF10_PS_RX_PREFIX_FIELD(LENGTH, CAP_LEN),
		EF10_PS_RX_PREFIX_FIELD(ORIG_LENGTH, ORIG_LEN),

#undef	EF10_PS_RX_PREFIX_FIELD
	}
};

#endif /* EFSYS_OPT_RX_PACKED_STREAM */

#if EFSYS_OPT_RX_ES_SUPER_BUFFER

/*
 * EF10 equal stride super-buffer Rx prefix layout.
 *
 * See SF-119419-TC DPDK Firmware Driver Interface section 3.4.
 */
static const efx_rx_prefix_layout_t ef10_essb_rx_prefix_layout = {
	.erpl_id	= 0,
	.erpl_length	= ES_EZ_ESSB_RX_PREFIX_LEN,
	.erpl_fields	= {
#define	EF10_ESSB_RX_PREFIX_FIELD(_efx, _ef10) \
	EFX_RX_PREFIX_FIELD(_efx, ES_EZ_ESSB_RX_PREFIX_ ## _ef10, B_FALSE)

		EF10_ESSB_RX_PREFIX_FIELD(LENGTH, DATA_LEN),
		EF10_ESSB_RX_PREFIX_FIELD(USER_MARK, MARK),
		EF10_ESSB_RX_PREFIX_FIELD(RSS_HASH_VALID, HASH_VALID),
		EF10_ESSB_RX_PREFIX_FIELD(USER_MARK_VALID, MARK_VALID),
		EF10_ESSB_RX_PREFIX_FIELD(USER_FLAG, MATCH_FLAG),
		EF10_ESSB_RX_PREFIX_FIELD(RSS_HASH, HASH),

#undef	EF10_ESSB_RX_PREFIX_FIELD
	}
};

#endif /* EFSYS_OPT_RX_ES_SUPER_BUFFER */

	__checkReturn	efx_rc_t
ef10_rx_prefix_pktlen(
	__in		efx_nic_t *enp,
	__in		uint8_t *buffer,
	__out		uint16_t *lengthp)
{
	_NOTE(ARGUNUSED(enp))

	/*
	 * The RX pseudo-header contains the packet length, excluding the
	 * pseudo-header. If the hardware receive datapath was operating in
	 * cut-through mode then the length in the RX pseudo-header will be
	 * zero, and the packet length must be obtained from the DMA length
	 * reported in the RX event.
	 */
	*lengthp = buffer[8] | (buffer[9] << 8);
	return (0);
}

#if EFSYS_OPT_RX_SCALE
	__checkReturn	uint32_t
ef10_rx_prefix_hash(
	__in		efx_nic_t *enp,
	__in		efx_rx_hash_alg_t func,
	__in		uint8_t *buffer)
{
	_NOTE(ARGUNUSED(enp))

	switch (func) {
	case EFX_RX_HASHALG_PACKED_STREAM:
	case EFX_RX_HASHALG_TOEPLITZ:
		return (buffer[0] |
		    (buffer[1] << 8) |
		    (buffer[2] << 16) |
		    (buffer[3] << 24));

	default:
		EFSYS_ASSERT(0);
		return (0);
	}
}
#endif /* EFSYS_OPT_RX_SCALE */

#if EFSYS_OPT_RX_PACKED_STREAM
/*
 * Fake length for RXQ descriptors in packed stream mode
 * to make hardware happy
 */
#define	EFX_RXQ_PACKED_STREAM_FAKE_BUF_SIZE 32
#endif

				void
ef10_rx_qpost(
	__in			efx_rxq_t *erp,
	__in_ecount(ndescs)	efsys_dma_addr_t *addrp,
	__in			size_t size,
	__in			unsigned int ndescs,
	__in			unsigned int completed,
	__in			unsigned int added)
{
	efx_qword_t qword;
	unsigned int i;
	unsigned int offset;
	unsigned int id;

	_NOTE(ARGUNUSED(completed))

#if EFSYS_OPT_RX_PACKED_STREAM
	/*
	 * Real size of the buffer does not fit into ESF_DZ_RX_KER_BYTE_CNT
	 * and equal to 0 after applying mask. Hardware does not like it.
	 */
	if (erp->er_ev_qstate->eers_rx_packed_stream)
		size = EFX_RXQ_PACKED_STREAM_FAKE_BUF_SIZE;
#endif

	/* The client driver must not overfill the queue */
	EFSYS_ASSERT3U(added - completed + ndescs, <=,
	    EFX_RXQ_LIMIT(erp->er_mask + 1));

	id = added & (erp->er_mask);
	for (i = 0; i < ndescs; i++) {
		EFSYS_PROBE4(rx_post, unsigned int, erp->er_index,
		    unsigned int, id, efsys_dma_addr_t, addrp[i],
		    size_t, size);

		EFX_POPULATE_QWORD_3(qword,
		    ESF_DZ_RX_KER_BYTE_CNT, (uint32_t)(size),
		    ESF_DZ_RX_KER_BUF_ADDR_DW0,
		    (uint32_t)(addrp[i] & 0xffffffff),
		    ESF_DZ_RX_KER_BUF_ADDR_DW1,
		    (uint32_t)(addrp[i] >> 32));

		offset = id * sizeof (efx_qword_t);
		EFSYS_MEM_WRITEQ(erp->er_esmp, offset, &qword);

		id = (id + 1) & (erp->er_mask);
	}
}

			void
ef10_rx_qpush(
	__in	efx_rxq_t *erp,
	__in	unsigned int added,
	__inout	unsigned int *pushedp)
{
	efx_nic_t *enp = erp->er_enp;
	unsigned int pushed = *pushedp;
	uint32_t wptr;
	efx_dword_t dword;

	/* Hardware has alignment restriction for WPTR */
	wptr = EFX_P2ALIGN(unsigned int, added, EF10_RX_WPTR_ALIGN);
	if (pushed == wptr)
		return;

	*pushedp = wptr;

	/* Push the populated descriptors out */
	wptr &= erp->er_mask;

	EFX_POPULATE_DWORD_1(dword, ERF_DZ_RX_DESC_WPTR, wptr);

	/* Guarantee ordering of memory (descriptors) and PIO (doorbell) */
	EFX_DMA_SYNC_QUEUE_FOR_DEVICE(erp->er_esmp, erp->er_mask + 1,
	    EF10_RXQ_DESC_SIZE, wptr, pushed & erp->er_mask);
	EFSYS_PIO_WRITE_BARRIER();
	EFX_BAR_VI_WRITED(enp, ER_DZ_RX_DESC_UPD_REG,
	    erp->er_index, &dword, B_FALSE);
}

#if EFSYS_OPT_RX_PACKED_STREAM

			void
ef10_rx_qpush_ps_credits(
	__in		efx_rxq_t *erp)
{
	efx_nic_t *enp = erp->er_enp;
	efx_dword_t dword;
	efx_evq_rxq_state_t *rxq_state = erp->er_ev_qstate;
	uint32_t credits;

	EFSYS_ASSERT(rxq_state->eers_rx_packed_stream);

	if (rxq_state->eers_rx_packed_stream_credits == 0)
		return;

	/*
	 * It is a bug if we think that FW has utilized more
	 * credits than it is allowed to have (maximum). However,
	 * make sure that we do not credit more than maximum anyway.
	 */
	credits = MIN(rxq_state->eers_rx_packed_stream_credits,
	    EFX_RX_PACKED_STREAM_MAX_CREDITS);
	EFX_POPULATE_DWORD_3(dword,
	    ERF_DZ_RX_DESC_MAGIC_DOORBELL, 1,
	    ERF_DZ_RX_DESC_MAGIC_CMD,
	    ERE_DZ_RX_DESC_MAGIC_CMD_PS_CREDITS,
	    ERF_DZ_RX_DESC_MAGIC_DATA, credits);
	EFX_BAR_VI_WRITED(enp, ER_DZ_RX_DESC_UPD_REG,
	    erp->er_index, &dword, B_FALSE);

	rxq_state->eers_rx_packed_stream_credits = 0;
}

/*
 * In accordance with SF-112241-TC the received data has the following layout:
 *  - 8 byte pseudo-header which consist of:
 *    - 4 byte little-endian timestamp
 *    - 2 byte little-endian captured length in bytes
 *    - 2 byte little-endian original packet length in bytes
 *  - captured packet bytes
 *  - optional padding to align to 64 bytes boundary
 *  - 64 bytes scratch space for the host software
 */
	__checkReturn	uint8_t *
ef10_rx_qps_packet_info(
	__in		efx_rxq_t *erp,
	__in		uint8_t *buffer,
	__in		uint32_t buffer_length,
	__in		uint32_t current_offset,
	__out		uint16_t *lengthp,
	__out		uint32_t *next_offsetp,
	__out		uint32_t *timestamp)
{
	uint16_t buf_len;
	uint8_t *pkt_start;
	efx_qword_t *qwordp;
	efx_evq_rxq_state_t *rxq_state = erp->er_ev_qstate;

	EFSYS_ASSERT(rxq_state->eers_rx_packed_stream);

	buffer += current_offset;
	pkt_start = buffer + EFX_RX_PACKED_STREAM_RX_PREFIX_SIZE;

	qwordp = (efx_qword_t *)buffer;
	*timestamp = EFX_QWORD_FIELD(*qwordp, ES_DZ_PS_RX_PREFIX_TSTAMP);
	*lengthp   = EFX_QWORD_FIELD(*qwordp, ES_DZ_PS_RX_PREFIX_ORIG_LEN);
	buf_len    = EFX_QWORD_FIELD(*qwordp, ES_DZ_PS_RX_PREFIX_CAP_LEN);

	buf_len = EFX_P2ROUNDUP(uint16_t,
	    buf_len + EFX_RX_PACKED_STREAM_RX_PREFIX_SIZE,
	    EFX_RX_PACKED_STREAM_ALIGNMENT);
	*next_offsetp =
	    current_offset + buf_len + EFX_RX_PACKED_STREAM_ALIGNMENT;

	EFSYS_ASSERT3U(*next_offsetp, <=, buffer_length);
	EFSYS_ASSERT3U(current_offset + *lengthp, <, *next_offsetp);

	if ((*next_offsetp ^ current_offset) &
	    EFX_RX_PACKED_STREAM_MEM_PER_CREDIT)
		rxq_state->eers_rx_packed_stream_credits++;

	return (pkt_start);
}


#endif

	__checkReturn	efx_rc_t
ef10_rx_qflush(
	__in	efx_rxq_t *erp)
{
	efx_nic_t *enp = erp->er_enp;
	efx_rc_t rc;

	if ((rc = efx_mcdi_fini_rxq(enp, erp->er_index)) != 0)
		goto fail1;

	return (0);

fail1:
	/*
	 * EALREADY is not an error, but indicates that the MC has rebooted and
	 * that the RXQ has already been destroyed. Callers need to know that
	 * the RXQ flush has completed to avoid waiting until timeout for a
	 * flush done event that will not be delivered.
	 */
	if (rc != EALREADY)
		EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

		void
ef10_rx_qenable(
	__in	efx_rxq_t *erp)
{
	/* FIXME */
	_NOTE(ARGUNUSED(erp))
	/* FIXME */
}

	__checkReturn	efx_rc_t
ef10_rx_qcreate(
	__in		efx_nic_t *enp,
	__in		unsigned int index,
	__in		unsigned int label,
	__in		efx_rxq_type_t type,
	__in_opt	const efx_rxq_type_data_t *type_data,
	__in		efsys_mem_t *esmp,
	__in		size_t ndescs,
	__in		uint32_t id,
	__in		unsigned int flags,
	__in		efx_evq_t *eep,
	__in		efx_rxq_t *erp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_mcdi_init_rxq_params_t params;
	const efx_rx_prefix_layout_t *erpl;
	efx_rc_t rc;

	_NOTE(ARGUNUSED(id, erp))

	EFX_STATIC_ASSERT(EFX_EV_RX_NLABELS == (1 << ESF_DZ_RX_QLABEL_WIDTH));
	EFSYS_ASSERT3U(label, <, EFX_EV_RX_NLABELS);

	memset(&params, 0, sizeof (params));
	params.buf_size = erp->er_buf_size;

	switch (type) {
	case EFX_RXQ_TYPE_DEFAULT:
		erpl = &ef10_default_rx_prefix_layout;
		if (type_data == NULL) {
			rc = EINVAL;
			goto fail1;
		}
		erp->er_buf_size = type_data->ertd_default.ed_buf_size;
		if (flags & EFX_RXQ_FLAG_USER_MARK) {
			rc = ENOTSUP;
			goto fail2;
		}
		if (flags & EFX_RXQ_FLAG_USER_FLAG) {
			rc = ENOTSUP;
			goto fail3;
		}
		/*
		 * Ignore EFX_RXQ_FLAG_RSS_HASH since if RSS hash is calculated
		 * it is always delivered from HW in the pseudo-header.
		 */
		break;
#if EFSYS_OPT_RX_PACKED_STREAM
	case EFX_RXQ_TYPE_PACKED_STREAM:
		erpl = &ef10_packed_stream_rx_prefix_layout;
		if (type_data == NULL) {
			rc = EINVAL;
			goto fail4;
		}
		switch (type_data->ertd_packed_stream.eps_buf_size) {
		case EFX_RXQ_PACKED_STREAM_BUF_SIZE_1M:
			params.ps_buf_size = MC_CMD_INIT_RXQ_EXT_IN_PS_BUFF_1M;
			break;
		case EFX_RXQ_PACKED_STREAM_BUF_SIZE_512K:
			params.ps_buf_size = MC_CMD_INIT_RXQ_EXT_IN_PS_BUFF_512K;
			break;
		case EFX_RXQ_PACKED_STREAM_BUF_SIZE_256K:
			params.ps_buf_size = MC_CMD_INIT_RXQ_EXT_IN_PS_BUFF_256K;
			break;
		case EFX_RXQ_PACKED_STREAM_BUF_SIZE_128K:
			params.ps_buf_size = MC_CMD_INIT_RXQ_EXT_IN_PS_BUFF_128K;
			break;
		case EFX_RXQ_PACKED_STREAM_BUF_SIZE_64K:
			params.ps_buf_size = MC_CMD_INIT_RXQ_EXT_IN_PS_BUFF_64K;
			break;
		default:
			rc = ENOTSUP;
			goto fail5;
		}
		erp->er_buf_size = type_data->ertd_packed_stream.eps_buf_size;
		/* Packed stream pseudo header does not have RSS hash value */
		if (flags & EFX_RXQ_FLAG_RSS_HASH) {
			rc = ENOTSUP;
			goto fail6;
		}
		if (flags & EFX_RXQ_FLAG_USER_MARK) {
			rc = ENOTSUP;
			goto fail7;
		}
		if (flags & EFX_RXQ_FLAG_USER_FLAG) {
			rc = ENOTSUP;
			goto fail8;
		}
		break;
#endif /* EFSYS_OPT_RX_PACKED_STREAM */
#if EFSYS_OPT_RX_ES_SUPER_BUFFER
	case EFX_RXQ_TYPE_ES_SUPER_BUFFER:
		erpl = &ef10_essb_rx_prefix_layout;
		if (type_data == NULL) {
			rc = EINVAL;
			goto fail9;
		}
		params.es_bufs_per_desc =
		    type_data->ertd_es_super_buffer.eessb_bufs_per_desc;
		params.es_max_dma_len =
		    type_data->ertd_es_super_buffer.eessb_max_dma_len;
		params.es_buf_stride =
		    type_data->ertd_es_super_buffer.eessb_buf_stride;
		params.hol_block_timeout =
		    type_data->ertd_es_super_buffer.eessb_hol_block_timeout;
		/*
		 * Ignore EFX_RXQ_FLAG_RSS_HASH since if RSS hash is calculated
		 * it is always delivered from HW in the pseudo-header.
		 */
		break;
#endif /* EFSYS_OPT_RX_ES_SUPER_BUFFER */
	default:
		rc = ENOTSUP;
		goto fail10;
	}

#if EFSYS_OPT_RX_PACKED_STREAM
	if (params.ps_buf_size != 0) {
		/* Check if datapath firmware supports packed stream mode */
		if (encp->enc_rx_packed_stream_supported == B_FALSE) {
			rc = ENOTSUP;
			goto fail11;
		}
		/* Check if packed stream allows configurable buffer sizes */
		if ((params.ps_buf_size != MC_CMD_INIT_RXQ_EXT_IN_PS_BUFF_1M) &&
		    (encp->enc_rx_var_packed_stream_supported == B_FALSE)) {
			rc = ENOTSUP;
			goto fail12;
		}
	}
#else /* EFSYS_OPT_RX_PACKED_STREAM */
	EFSYS_ASSERT(params.ps_buf_size == 0);
#endif /* EFSYS_OPT_RX_PACKED_STREAM */

#if EFSYS_OPT_RX_ES_SUPER_BUFFER
	if (params.es_bufs_per_desc > 0) {
		if (encp->enc_rx_es_super_buffer_supported == B_FALSE) {
			rc = ENOTSUP;
			goto fail13;
		}
		if (!EFX_IS_P2ALIGNED(uint32_t, params.es_max_dma_len,
			    EFX_RX_ES_SUPER_BUFFER_BUF_ALIGNMENT)) {
			rc = EINVAL;
			goto fail14;
		}
		if (!EFX_IS_P2ALIGNED(uint32_t, params.es_buf_stride,
			    EFX_RX_ES_SUPER_BUFFER_BUF_ALIGNMENT)) {
			rc = EINVAL;
			goto fail15;
		}
	}
#else /* EFSYS_OPT_RX_ES_SUPER_BUFFER */
	EFSYS_ASSERT(params.es_bufs_per_desc == 0);
#endif /* EFSYS_OPT_RX_ES_SUPER_BUFFER */

	if (flags & EFX_RXQ_FLAG_INGRESS_MPORT) {
		rc = ENOTSUP;
		goto fail16;
	}

	/* Scatter can only be disabled if the firmware supports doing so */
	if (flags & EFX_RXQ_FLAG_SCATTER)
		params.disable_scatter = B_FALSE;
	else
		params.disable_scatter = encp->enc_rx_disable_scatter_supported;

	if (flags & EFX_RXQ_FLAG_INNER_CLASSES)
		params.want_inner_classes = B_TRUE;
	else
		params.want_inner_classes = B_FALSE;

	if ((rc = efx_mcdi_init_rxq(enp, ndescs, eep, label, index,
		    esmp, &params)) != 0)
		goto fail17;

	erp->er_eep = eep;
	erp->er_label = label;

	ef10_ev_rxlabel_init(eep, erp, label, type);

	erp->er_ev_qstate = &erp->er_eep->ee_rxq_state[label];

	erp->er_prefix_layout = *erpl;

	return (0);

fail17:
	EFSYS_PROBE(fail15);
fail16:
	EFSYS_PROBE(fail14);
#if EFSYS_OPT_RX_ES_SUPER_BUFFER
fail15:
	EFSYS_PROBE(fail15);
fail14:
	EFSYS_PROBE(fail14);
fail13:
	EFSYS_PROBE(fail13);
#endif /* EFSYS_OPT_RX_ES_SUPER_BUFFER */
#if EFSYS_OPT_RX_PACKED_STREAM
fail12:
	EFSYS_PROBE(fail12);
fail11:
	EFSYS_PROBE(fail11);
#endif /* EFSYS_OPT_RX_PACKED_STREAM */
fail10:
	EFSYS_PROBE(fail10);
#if EFSYS_OPT_RX_ES_SUPER_BUFFER
fail9:
	EFSYS_PROBE(fail9);
#endif /* EFSYS_OPT_RX_ES_SUPER_BUFFER */
#if EFSYS_OPT_RX_PACKED_STREAM
fail8:
	EFSYS_PROBE(fail8);
fail7:
	EFSYS_PROBE(fail7);
fail6:
	EFSYS_PROBE(fail6);
fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);
#endif /* EFSYS_OPT_RX_PACKED_STREAM */
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

		void
ef10_rx_qdestroy(
	__in	efx_rxq_t *erp)
{
	efx_evq_t *eep = erp->er_eep;
	unsigned int label = erp->er_label;

	ef10_ev_rxlabel_fini(eep, label);
}

#endif /* EFX_OPTS_EF10() */

		void
ef10_rx_fini(
	__in	efx_nic_t *enp)
{
#if EFSYS_OPT_RX_SCALE
	if (enp->en_rss_context_type != EFX_RX_SCALE_UNAVAILABLE)
		(void) efx_mcdi_rss_context_free(enp, enp->en_rss_context);
	enp->en_rss_context = 0;
	enp->en_rss_context_type = EFX_RX_SCALE_UNAVAILABLE;
#else
	_NOTE(ARGUNUSED(enp))
#endif /* EFSYS_OPT_RX_SCALE */
}

#endif /* EFSYS_OPT_RIVERHEAD || EFX_OPTS_EF10() */
