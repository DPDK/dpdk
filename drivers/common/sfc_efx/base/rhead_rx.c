/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2018-2019 Solarflare Communications Inc.
 */

#include "efx.h"
#include "efx_impl.h"


#if EFSYS_OPT_RIVERHEAD

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
		RHEAD_RX_PREFIX_FIELD(INGRESS_VPORT, B_FALSE),
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
	__out		uint32_t *rss_contextp)
{
	efx_rc_t rc;

	rc = ef10_rx_scale_context_alloc(enp, type, num_queues, rss_contextp);
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

	__checkReturn	efx_rc_t
rhead_rx_scale_tbl_set(
	__in		efx_nic_t *enp,
	__in		uint32_t rss_context,
	__in_ecount(n)	unsigned int *table,
	__in		size_t n)
{
	efx_rc_t rc;

	rc = ef10_rx_scale_tbl_set(enp, rss_context, table, n);
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

	/*
	 * Ignore EFX_RXQ_FLAG_INNER_CLASSES since in accordance with
	 * EF100 host interface both inner and outer classes are provided
	 * by HW if applicable.
	 */

	if ((rc = efx_mcdi_init_rxq(enp, ndescs, eep, label, index,
		    esmp, &params)) != 0)
		goto fail3;

	erp->er_eep = eep;
	erp->er_label = label;
	erp->er_buf_size = params.buf_size;
	erp->er_prefix_layout = rhead_default_rx_prefix_layout;

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
rhead_rx_qdestroy(
	__in	efx_rxq_t *erp)
{
	_NOTE(ARGUNUSED(erp))
	/* Nothing to do here */
}

#endif /* EFSYS_OPT_RIVERHEAD */
