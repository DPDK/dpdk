/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2018-2019 Solarflare Communications Inc.
 */

#include "efx.h"
#include "efx_impl.h"


#if EFSYS_OPT_RIVERHEAD

/*
 * Maximum number of Rx prefixes supported by Rx prefix choice to be
 * returned from firmware.
 */
#define	RHEAD_RX_PREFIX_IDS_MAX		16

/*
 * Default Rx prefix layout on Riverhead if FW does not support Rx
 * prefix choice using MC_CMD_GET_RX_PREFIX_ID and query its layout
 * using MC_CMD_QUERY_RX_PREFIX_ID.
 *
 * See SF-119689-TC Riverhead Host Interface section 6.4.
 */
static const efx_rx_prefix_layout_t rhead_default_rx_prefix_layout = {
	.erpl_id	= 0,
	.erpl_length	= ESE_GZ_RX_PKT_PREFIX_LEN,
	.erpl_fields	= {
#define	RHEAD_RX_PREFIX_FIELD(_name, _big_endian) \
	EFX_RX_PREFIX_FIELD(_name, ESF_GZ_RX_PREFIX_ ## _name, _big_endian)

		RHEAD_RX_PREFIX_FIELD(LENGTH, B_FALSE),
		RHEAD_RX_PREFIX_FIELD(RSS_HASH_VALID, B_FALSE),
		RHEAD_RX_PREFIX_FIELD(USER_FLAG, B_FALSE),
		RHEAD_RX_PREFIX_FIELD(CLASS, B_FALSE),
		RHEAD_RX_PREFIX_FIELD(PARTIAL_TSTAMP, B_FALSE),
		RHEAD_RX_PREFIX_FIELD(RSS_HASH, B_FALSE),
		RHEAD_RX_PREFIX_FIELD(USER_MARK, B_FALSE),
		RHEAD_RX_PREFIX_FIELD(INGRESS_MPORT, B_FALSE),
		RHEAD_RX_PREFIX_FIELD(CSUM_FRAME, B_TRUE),
		RHEAD_RX_PREFIX_FIELD(VLAN_STRIP_TCI, B_TRUE),

#undef	RHEAD_RX_PREFIX_FIELD
	}
};

	__checkReturn	efx_rc_t
rhead_rx_init(
	__in		efx_nic_t *enp)
{
	efx_rc_t rc;

	rc = ef10_rx_init(enp);
	if (rc != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

		void
rhead_rx_fini(
	__in	efx_nic_t *enp)
{
	ef10_rx_fini(enp);
}

#if EFSYS_OPT_RX_SCATTER
	__checkReturn	efx_rc_t
rhead_rx_scatter_enable(
	__in		efx_nic_t *enp,
	__in		unsigned int buf_size)
{
	_NOTE(ARGUNUSED(enp, buf_size))
	/* Nothing to do here */
	return (0);
}
#endif	/* EFSYS_OPT_RX_SCATTER */

#if EFSYS_OPT_RX_SCALE

	__checkReturn	efx_rc_t
rhead_rx_scale_context_alloc(
	__in		efx_nic_t *enp,
	__in		efx_rx_scale_context_type_t type,
	__in		uint32_t num_queues,
	__in		uint32_t table_nentries,
	__out		uint32_t *rss_contextp)
{
	efx_rc_t rc;

	rc = ef10_rx_scale_context_alloc(enp, type, num_queues, table_nentries,
		    rss_contextp);
	if (rc != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn	efx_rc_t
rhead_rx_scale_context_free(
	__in		efx_nic_t *enp,
	__in		uint32_t rss_context)
{
	efx_rc_t rc;

	rc = ef10_rx_scale_context_free(enp, rss_context);
	if (rc != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn	efx_rc_t
rhead_rx_scale_mode_set(
	__in		efx_nic_t *enp,
	__in		uint32_t rss_context,
	__in		efx_rx_hash_alg_t alg,
	__in		efx_rx_hash_type_t type,
	__in		boolean_t insert)
{
	efx_rc_t rc;

	rc = ef10_rx_scale_mode_set(enp, rss_context, alg, type, insert);
	if (rc != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn	efx_rc_t
rhead_rx_scale_key_set(
	__in		efx_nic_t *enp,
	__in		uint32_t rss_context,
	__in_ecount(n)	uint8_t *key,
	__in		size_t n)
{
	efx_rc_t rc;

	rc = ef10_rx_scale_key_set(enp, rss_context, key, n);
	if (rc != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn		efx_rc_t
rhead_rx_scale_tbl_set(
	__in			efx_nic_t *enp,
	__in			uint32_t rss_context,
	__in_ecount(nentries)	unsigned int *table,
	__in			size_t nentries)
{
	efx_rc_t rc;

	rc = ef10_rx_scale_tbl_set(enp, rss_context, table, nentries);
	if (rc != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn	uint32_t
rhead_rx_prefix_hash(
	__in		efx_nic_t *enp,
	__in		efx_rx_hash_alg_t func,
	__in		uint8_t *buffer)
{
	_NOTE(ARGUNUSED(enp, func, buffer))

	/* FIXME implement the method for Riverhead */

	return (ENOTSUP);
}

#endif /* EFSYS_OPT_RX_SCALE */

	__checkReturn	efx_rc_t
rhead_rx_prefix_pktlen(
	__in		efx_nic_t *enp,
	__in		uint8_t *buffer,
	__out		uint16_t *lengthp)
{
	_NOTE(ARGUNUSED(enp, buffer, lengthp))

	/* FIXME implement the method for Riverhead */

	return (ENOTSUP);
}

				void
rhead_rx_qpost(
	__in			efx_rxq_t *erp,
	__in_ecount(ndescs)	efsys_dma_addr_t *addrp,
	__in			size_t size,
	__in			unsigned int ndescs,
	__in			unsigned int completed,
	__in			unsigned int added)
{
	_NOTE(ARGUNUSED(erp, addrp, size, ndescs, completed, added))

	/* FIXME implement the method for Riverhead */

	EFSYS_ASSERT(B_FALSE);
}

			void
rhead_rx_qpush(
	__in	efx_rxq_t *erp,
	__in	unsigned int added,
	__inout	unsigned int *pushedp)
{
	_NOTE(ARGUNUSED(erp, added, pushedp))

	/* FIXME implement the method for Riverhead */

	EFSYS_ASSERT(B_FALSE);
}

	__checkReturn	efx_rc_t
rhead_rx_qflush(
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
rhead_rx_qenable(
	__in	efx_rxq_t *erp)
{
	_NOTE(ARGUNUSED(erp))
}

static	__checkReturn	efx_rc_t
efx_mcdi_get_rx_prefix_ids(
	__in					efx_nic_t *enp,
	__in					uint32_t mcdi_fields_mask,
	__in					unsigned int max_ids,
	__out					unsigned int *nids,
	__out_ecount_part(max_ids, *nids)	uint32_t *idsp)
{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_GET_RX_PREFIX_ID_IN_LEN,
		MC_CMD_GET_RX_PREFIX_ID_OUT_LENMAX);
	efx_rc_t rc;
	uint32_t num;

	req.emr_cmd = MC_CMD_GET_RX_PREFIX_ID;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_GET_RX_PREFIX_ID_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_GET_RX_PREFIX_ID_OUT_LENMAX;

	MCDI_IN_SET_DWORD(req, GET_RX_PREFIX_ID_IN_FIELDS, mcdi_fields_mask);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_GET_RX_PREFIX_ID_OUT_LENMIN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	num = MCDI_OUT_DWORD(req, GET_RX_PREFIX_ID_OUT_NUM_RX_PREFIX_IDS);

	if (req.emr_out_length_used != MC_CMD_GET_RX_PREFIX_ID_OUT_LEN(num)) {
		rc = EMSGSIZE;
		goto fail3;
	}

	*nids = MIN(num, max_ids);

	EFX_STATIC_ASSERT(sizeof (idsp[0]) ==
	    MC_CMD_GET_RX_PREFIX_ID_OUT_RX_PREFIX_ID_LEN);
	memcpy(idsp,
	    MCDI_OUT2(req, uint32_t, GET_RX_PREFIX_ID_OUT_RX_PREFIX_ID),
	    *nids * sizeof (idsp[0]));

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

static	__checkReturn	efx_rx_prefix_field_t
efx_mcdi_rx_prefix_field_map(unsigned int mcdi_idx)
{
	static const efx_rx_prefix_field_t efx_mcdi_to_rx_prefix_field[] = {
#define	EFX_MCDI_TO_RX_PREFIX_FIELD(_field) \
	[RX_PREFIX_FIELD_INFO_ ## _field] = EFX_RX_PREFIX_FIELD_ ## _field

		EFX_MCDI_TO_RX_PREFIX_FIELD(LENGTH),
		EFX_MCDI_TO_RX_PREFIX_FIELD(RSS_HASH_VALID),
		EFX_MCDI_TO_RX_PREFIX_FIELD(USER_FLAG),
		EFX_MCDI_TO_RX_PREFIX_FIELD(CLASS),
		EFX_MCDI_TO_RX_PREFIX_FIELD(PARTIAL_TSTAMP),
		EFX_MCDI_TO_RX_PREFIX_FIELD(RSS_HASH),
		EFX_MCDI_TO_RX_PREFIX_FIELD(USER_MARK),
		EFX_MCDI_TO_RX_PREFIX_FIELD(INGRESS_VPORT),
		EFX_MCDI_TO_RX_PREFIX_FIELD(CSUM_FRAME),
		EFX_MCDI_TO_RX_PREFIX_FIELD(VLAN_STRIP_TCI),

#undef	EFX_MCDI_TO_RX_PREFIX_FIELD
	};

	if (mcdi_idx >= EFX_ARRAY_SIZE(efx_mcdi_to_rx_prefix_field))
		return (EFX_RX_PREFIX_NFIELDS);

	return (efx_mcdi_to_rx_prefix_field[mcdi_idx]);
}

static	__checkReturn	int
efx_rx_prefix_field_map_to_mcdi(
	__in		efx_rx_prefix_field_t field)
{
	static const int efx_rx_prefix_field_to_mcdi[] = {
		[EFX_RX_PREFIX_FIELD_LENGTH] =
			EFX_LOW_BIT(MC_CMD_GET_RX_PREFIX_ID_IN_LENGTH),
		[EFX_RX_PREFIX_FIELD_ORIG_LENGTH] = -1,
		[EFX_RX_PREFIX_FIELD_CLASS] =
			EFX_LOW_BIT(MC_CMD_GET_RX_PREFIX_ID_IN_CLASS),
		[EFX_RX_PREFIX_FIELD_RSS_HASH] =
			EFX_LOW_BIT(MC_CMD_GET_RX_PREFIX_ID_IN_RSS_HASH),
		[EFX_RX_PREFIX_FIELD_RSS_HASH_VALID] =
			EFX_LOW_BIT(MC_CMD_GET_RX_PREFIX_ID_IN_RSS_HASH_VALID),
		[EFX_RX_PREFIX_FIELD_PARTIAL_TSTAMP] =
			EFX_LOW_BIT(MC_CMD_GET_RX_PREFIX_ID_IN_PARTIAL_TSTAMP),
		[EFX_RX_PREFIX_FIELD_VLAN_STRIP_TCI] =
			EFX_LOW_BIT(MC_CMD_GET_RX_PREFIX_ID_IN_VLAN_STRIP_TCI),
		[EFX_RX_PREFIX_FIELD_INNER_VLAN_STRIP_TCI] = -1,
		[EFX_RX_PREFIX_FIELD_USER_FLAG] =
			EFX_LOW_BIT(MC_CMD_GET_RX_PREFIX_ID_IN_USER_FLAG),
		[EFX_RX_PREFIX_FIELD_USER_MARK] =
			EFX_LOW_BIT(MC_CMD_GET_RX_PREFIX_ID_IN_USER_MARK),
		[EFX_RX_PREFIX_FIELD_USER_MARK_VALID] = -1,
		[EFX_RX_PREFIX_FIELD_CSUM_FRAME] =
			EFX_LOW_BIT(MC_CMD_GET_RX_PREFIX_ID_IN_CSUM_FRAME),
		[EFX_RX_PREFIX_FIELD_INGRESS_VPORT] =
			EFX_LOW_BIT(MC_CMD_GET_RX_PREFIX_ID_IN_INGRESS_VPORT),
	};

	if (field >= EFX_ARRAY_SIZE(efx_rx_prefix_field_to_mcdi))
		return (-1);

	return (efx_rx_prefix_field_to_mcdi[field]);
}

static	__checkReturn	efx_rc_t
efx_rx_prefix_fields_mask_to_mcdi(
	__in		uint32_t fields_mask,
	__out		uint32_t *mcdi_fields_maskp)
{
	uint32_t mcdi_fields_mask = 0;
	unsigned int i;

	for (i = 0; i < EFX_RX_PREFIX_NFIELDS; ++i) {
		if (fields_mask & (1U << i)) {
			int mcdi_field = efx_rx_prefix_field_map_to_mcdi(i);

			if (mcdi_field < 0)
				return (EINVAL);

			mcdi_fields_mask |= (1U << mcdi_field);
		}
	}

	*mcdi_fields_maskp = mcdi_fields_mask;
	return (0);
}

static	__checkReturn	efx_rc_t
efx_mcdi_query_rx_prefix_id(
	__in		efx_nic_t *enp,
	__in		uint32_t prefix_id,
	__out		efx_rx_prefix_layout_t *erplp)
{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload, MC_CMD_QUERY_RX_PREFIX_ID_IN_LEN,
		MC_CMD_QUERY_RX_PREFIX_ID_OUT_LENMAX);
	efx_rc_t rc;
	size_t response_len;
	const efx_dword_t *resp;
	const efx_dword_t *finfo;
	unsigned int num_fields;
	unsigned int mcdi_field;
	efx_rx_prefix_field_t field;
	unsigned int i;

	req.emr_cmd = MC_CMD_QUERY_RX_PREFIX_ID;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_QUERY_RX_PREFIX_ID_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_QUERY_RX_PREFIX_ID_OUT_LENMAX;

	MCDI_IN_SET_DWORD(req, QUERY_RX_PREFIX_ID_IN_RX_PREFIX_ID, prefix_id);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_QUERY_RX_PREFIX_ID_OUT_LENMIN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	if (MCDI_OUT_BYTE(req, QUERY_RX_PREFIX_ID_OUT_RESPONSE_TYPE) !=
	    MC_CMD_QUERY_RX_PREFIX_ID_OUT_RESPONSE_TYPE_FIXED) {
		rc = ENOTSUP;
		goto fail3;
	}

	EFX_STATIC_ASSERT(MC_CMD_QUERY_RX_PREFIX_ID_OUT_LENMIN >=
	    MC_CMD_QUERY_RX_PREFIX_ID_OUT_RESPONSE_OFST);
	response_len = req.emr_out_length_used -
	    MC_CMD_QUERY_RX_PREFIX_ID_OUT_RESPONSE_OFST;

	if (response_len < RX_PREFIX_FIXED_RESPONSE_LENMIN) {
		rc = EMSGSIZE;
		goto fail4;
	}

	resp = MCDI_OUT2(req, efx_dword_t, QUERY_RX_PREFIX_ID_OUT_RESPONSE);

	memset(erplp, 0, sizeof (*erplp));
	erplp->erpl_id = prefix_id;
	erplp->erpl_length =
	    EFX_DWORD_FIELD(*resp, RX_PREFIX_FIXED_RESPONSE_PREFIX_LENGTH_BYTES);
	num_fields =
	    EFX_DWORD_FIELD(*resp, RX_PREFIX_FIXED_RESPONSE_FIELD_COUNT);

	if (response_len < RX_PREFIX_FIXED_RESPONSE_LEN(num_fields)) {
		rc = EMSGSIZE;
		goto fail5;
	}

	finfo = (const efx_dword_t *)((const uint8_t *)resp +
	     RX_PREFIX_FIXED_RESPONSE_FIELDS_OFST);

	for (i = 0; i < num_fields; ++i, ++finfo) {
		mcdi_field = EFX_DWORD_FIELD(*finfo, RX_PREFIX_FIELD_INFO_TYPE);

		field = efx_mcdi_rx_prefix_field_map(mcdi_field);
		if (field >= EFX_RX_PREFIX_NFIELDS)
			continue;

		erplp->erpl_fields[field].erpfi_offset_bits =
		    EFX_DWORD_FIELD(*finfo, RX_PREFIX_FIELD_INFO_OFFSET_BITS);
		erplp->erpl_fields[field].erpfi_width_bits =
		    EFX_DWORD_FIELD(*finfo, RX_PREFIX_FIELD_INFO_WIDTH_BITS);
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

static	__checkReturn	efx_rc_t
rhead_rx_choose_prefix_id(
	__in		efx_nic_t *enp,
	__in		uint32_t fields_mask,
	__out		efx_rx_prefix_layout_t *erplp)
{
	efx_rx_prefix_layout_t erpl;
	uint32_t prefix_ids[RHEAD_RX_PREFIX_IDS_MAX];
	uint32_t mcdi_fields_mask;
	unsigned int num = 0;
	unsigned int i;
	efx_rc_t rc;

	rc = efx_rx_prefix_fields_mask_to_mcdi(fields_mask, &mcdi_fields_mask);
	if (rc != 0)
		goto fail1;

	memset(erplp, 0, sizeof (*erplp));

	rc = efx_mcdi_get_rx_prefix_ids(enp, mcdi_fields_mask,
	    EFX_ARRAY_SIZE(prefix_ids), &num, prefix_ids);
	if (rc == ENOTSUP) {
		/* Not supported MCDI, use default prefix ID */
		*erplp = rhead_default_rx_prefix_layout;
		goto done;
	}
	if (rc != 0)
		goto fail2;

	if (num == 0) {
		rc = ENOTSUP;
		goto fail3;
	}

	for (i = 0; i < num; ++i) {
		rc = efx_mcdi_query_rx_prefix_id(enp, prefix_ids[i], &erpl);
		if (rc != 0)
			goto fail4;

		/* Choose the smallest prefix which meets our requirements */
		if (i == 0 || erpl.erpl_length < erplp->erpl_length)
			*erplp = erpl;
	}

done:
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

	__checkReturn	efx_rc_t
rhead_rx_qcreate(
	__in		efx_nic_t *enp,
	__in		unsigned int index,
	__in		unsigned int label,
	__in		efx_rxq_type_t type,
	__in		const efx_rxq_type_data_t *type_data,
	__in		efsys_mem_t *esmp,
	__in		size_t ndescs,
	__in		uint32_t id,
	__in		unsigned int flags,
	__in		efx_evq_t *eep,
	__in		efx_rxq_t *erp)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(enp);
	efx_mcdi_init_rxq_params_t params;
	efx_rx_prefix_layout_t erpl;
	uint32_t fields_mask = 0;
	efx_rc_t rc;

	_NOTE(ARGUNUSED(id))

	EFX_STATIC_ASSERT(EFX_EV_RX_NLABELS <=
	    (1 << ESF_GZ_EV_RXPKTS_Q_LABEL_WIDTH));
	EFSYS_ASSERT3U(label, <, EFX_EV_RX_NLABELS);

	memset(&params, 0, sizeof (params));

	switch (type) {
	case EFX_RXQ_TYPE_DEFAULT:
		if (type_data == NULL) {
			rc = EINVAL;
			goto fail1;
		}
		params.buf_size = type_data->ertd_default.ed_buf_size;
		break;
	default:
		rc = ENOTSUP;
		goto fail2;
	}

	/* Scatter can only be disabled if the firmware supports doing so */
	if (flags & EFX_RXQ_FLAG_SCATTER)
		params.disable_scatter = B_FALSE;
	else
		params.disable_scatter = encp->enc_rx_disable_scatter_supported;

	if (flags & EFX_RXQ_FLAG_RSS_HASH) {
		fields_mask |= 1U << EFX_RX_PREFIX_FIELD_RSS_HASH;
		fields_mask |= 1U << EFX_RX_PREFIX_FIELD_RSS_HASH_VALID;
	}

	if (flags & EFX_RXQ_FLAG_INGRESS_MPORT)
		fields_mask |= 1U << EFX_RX_PREFIX_FIELD_INGRESS_MPORT;

	if (flags & EFX_RXQ_FLAG_USER_MARK)
		fields_mask |= 1U << EFX_RX_PREFIX_FIELD_USER_MARK;

	if (flags & EFX_RXQ_FLAG_USER_FLAG)
		fields_mask |= 1U << EFX_RX_PREFIX_FIELD_USER_FLAG;

	/*
	 * LENGTH is required in EF100 host interface, as receive events
	 * do not include the packet length.
	 */
	fields_mask |= 1U << EFX_RX_PREFIX_FIELD_LENGTH;
	if ((rc = rhead_rx_choose_prefix_id(enp, fields_mask, &erpl)) != 0)
		goto fail3;

	params.prefix_id = erpl.erpl_id;

	/*
	 * Ignore EFX_RXQ_FLAG_INNER_CLASSES since in accordance with
	 * EF100 host interface both inner and outer classes are provided
	 * by HW if applicable.
	 */

	if ((rc = efx_mcdi_init_rxq(enp, ndescs, eep, label, index,
		    esmp, &params)) != 0)
		goto fail4;

	erp->er_eep = eep;
	erp->er_label = label;
	erp->er_buf_size = params.buf_size;
	erp->er_prefix_layout = erpl;

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

		void
rhead_rx_qdestroy(
	__in	efx_rxq_t *erp)
{
	_NOTE(ARGUNUSED(erp))
	/* Nothing to do here */
}

#endif /* EFSYS_OPT_RIVERHEAD */
