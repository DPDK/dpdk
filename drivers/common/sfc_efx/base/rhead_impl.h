/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2018-2019 Solarflare Communications Inc.
 */

#ifndef	_SYS_RHEAD_IMPL_H
#define	_SYS_RHEAD_IMPL_H

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * Riverhead requires physically contiguous event rings (so, just one
 * DMA address is sufficient to represent it), but MCDI interface is still
 * in terms of 4k size 4k-aligned DMA buffers.
 */
#define	RHEAD_EVQ_MAXNBUFS	32

#define	RHEAD_EVQ_MAXNEVS	16384
#define	RHEAD_EVQ_MINNEVS	256

#define	RHEAD_RXQ_MAXNDESCS	16384
#define	RHEAD_RXQ_MINNDESCS	256

#define	RHEAD_TXQ_MAXNDESCS	16384
#define	RHEAD_TXQ_MINNDESCS	256

#define	RHEAD_EVQ_DESC_SIZE	(sizeof (efx_qword_t))
#define	RHEAD_RXQ_DESC_SIZE	(sizeof (efx_qword_t))
#define	RHEAD_TXQ_DESC_SIZE	(sizeof (efx_oword_t))

#if EFSYS_OPT_EV_EXTENDED_WIDTH
#define	RHEAD_EVQ_EW_DESC_SIZE	(sizeof (efx_xword_t))
#endif


/* NIC */

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_board_cfg(
	__in		efx_nic_t *enp);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_nic_probe(
	__in		efx_nic_t *enp);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_nic_set_drv_limits(
	__inout		efx_nic_t *enp,
	__in		efx_drv_limits_t *edlp);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_nic_get_vi_pool(
	__in		efx_nic_t *enp,
	__out		uint32_t *vi_countp);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_nic_get_bar_region(
	__in		efx_nic_t *enp,
	__in		efx_nic_region_t region,
	__out		uint32_t *offsetp,
	__out		size_t *sizep);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_nic_reset(
	__in		efx_nic_t *enp);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_nic_init(
	__in		efx_nic_t *enp);

LIBEFX_INTERNAL
extern	__checkReturn	boolean_t
rhead_nic_hw_unavailable(
	__in		efx_nic_t *enp);

LIBEFX_INTERNAL
extern			void
rhead_nic_set_hw_unavailable(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_DIAG

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_nic_register_test(
	__in		efx_nic_t *enp);

#endif	/* EFSYS_OPT_DIAG */

LIBEFX_INTERNAL
extern			void
rhead_nic_fini(
	__in		efx_nic_t *enp);

LIBEFX_INTERNAL
extern			void
rhead_nic_unprobe(
	__in		efx_nic_t *enp);


/* EV */

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_ev_init(
	__in		efx_nic_t *enp);

LIBEFX_INTERNAL
extern			void
rhead_ev_fini(
	__in		efx_nic_t *enp);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_ev_qcreate(
	__in		efx_nic_t *enp,
	__in		unsigned int index,
	__in		efsys_mem_t *esmp,
	__in		size_t ndescs,
	__in		uint32_t id,
	__in		uint32_t us,
	__in		uint32_t flags,
	__in		efx_evq_t *eep);

LIBEFX_INTERNAL
extern			void
rhead_ev_qdestroy(
	__in		efx_evq_t *eep);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_ev_qprime(
	__in		efx_evq_t *eep,
	__in		unsigned int count);

LIBEFX_INTERNAL
extern			void
rhead_ev_qpost(
	__in	efx_evq_t *eep,
	__in	uint16_t data);

LIBEFX_INTERNAL
extern			void
rhead_ev_qpoll(
	__in		efx_evq_t *eep,
	__inout		unsigned int *countp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_ev_qmoderate(
	__in		efx_evq_t *eep,
	__in		unsigned int us);

#if EFSYS_OPT_QSTATS

LIBEFX_INTERNAL
extern			void
rhead_ev_qstats_update(
	__in				efx_evq_t *eep,
	__inout_ecount(EV_NQSTATS)	efsys_stat_t *stat);

#endif /* EFSYS_OPT_QSTATS */


/* INTR */

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_intr_init(
	__in		efx_nic_t *enp,
	__in		efx_intr_type_t type,
	__in		efsys_mem_t *esmp);

LIBEFX_INTERNAL
extern			void
rhead_intr_enable(
	__in		efx_nic_t *enp);

LIBEFX_INTERNAL
extern			void
rhead_intr_disable(
	__in		efx_nic_t *enp);

LIBEFX_INTERNAL
extern			void
rhead_intr_disable_unlocked(
	__in		efx_nic_t *enp);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_intr_trigger(
	__in		efx_nic_t *enp,
	__in		unsigned int level);

LIBEFX_INTERNAL
extern			void
rhead_intr_status_line(
	__in		efx_nic_t *enp,
	__out		boolean_t *fatalp,
	__out		uint32_t *qmaskp);

LIBEFX_INTERNAL
extern			void
rhead_intr_status_message(
	__in		efx_nic_t *enp,
	__in		unsigned int message,
	__out		boolean_t *fatalp);

LIBEFX_INTERNAL
extern			void
rhead_intr_fatal(
	__in		efx_nic_t *enp);

LIBEFX_INTERNAL
extern			void
rhead_intr_fini(
	__in		efx_nic_t *enp);


/* RX */

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_rx_init(
	__in		efx_nic_t *enp);

LIBEFX_INTERNAL
extern			void
rhead_rx_fini(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_RX_SCATTER

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_rx_scatter_enable(
	__in		efx_nic_t *enp,
	__in		unsigned int buf_size);

#endif	/* EFSYS_OPT_RX_SCATTER */

#if EFSYS_OPT_RX_SCALE

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_rx_scale_context_alloc(
	__in		efx_nic_t *enp,
	__in		efx_rx_scale_context_type_t type,
	__in		uint32_t num_queues,
	__out		uint32_t *rss_contextp);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_rx_scale_context_free(
	__in		efx_nic_t *enp,
	__in		uint32_t rss_context);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_rx_scale_mode_set(
	__in		efx_nic_t *enp,
	__in		uint32_t rss_context,
	__in		efx_rx_hash_alg_t alg,
	__in		efx_rx_hash_type_t type,
	__in		boolean_t insert);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_rx_scale_key_set(
	__in		efx_nic_t *enp,
	__in		uint32_t rss_context,
	__in_ecount(n)	uint8_t *key,
	__in		size_t n);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_rx_scale_tbl_set(
	__in		efx_nic_t *enp,
	__in		uint32_t rss_context,
	__in_ecount(n)	unsigned int *table,
	__in		size_t n);

LIBEFX_INTERNAL
extern	__checkReturn	uint32_t
rhead_rx_prefix_hash(
	__in		efx_nic_t *enp,
	__in		efx_rx_hash_alg_t func,
	__in		uint8_t *buffer);

#endif /* EFSYS_OPT_RX_SCALE */

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_rx_prefix_pktlen(
	__in		efx_nic_t *enp,
	__in		uint8_t *buffer,
	__out		uint16_t *lengthp);

LIBEFX_INTERNAL
extern				void
rhead_rx_qpost(
	__in			efx_rxq_t *erp,
	__in_ecount(ndescs)	efsys_dma_addr_t *addrp,
	__in			size_t size,
	__in			unsigned int ndescs,
	__in			unsigned int completed,
	__in			unsigned int added);

LIBEFX_INTERNAL
extern			void
rhead_rx_qpush(
	__in		efx_rxq_t *erp,
	__in		unsigned int added,
	__inout		unsigned int *pushedp);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_rx_qflush(
	__in		efx_rxq_t *erp);

LIBEFX_INTERNAL
extern		void
rhead_rx_qenable(
	__in		efx_rxq_t *erp);

union efx_rxq_type_data_u;

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_rx_qcreate(
	__in		efx_nic_t *enp,
	__in		unsigned int index,
	__in		unsigned int label,
	__in		efx_rxq_type_t type,
	__in		const union efx_rxq_type_data_u *type_data,
	__in		efsys_mem_t *esmp,
	__in		size_t ndescs,
	__in		uint32_t id,
	__in		unsigned int flags,
	__in		efx_evq_t *eep,
	__in		efx_rxq_t *erp);

LIBEFX_INTERNAL
extern			void
rhead_rx_qdestroy(
	__in		efx_rxq_t *erp);


/* TX */

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_tx_init(
	__in		efx_nic_t *enp);

LIBEFX_INTERNAL
extern			void
rhead_tx_fini(
	__in		efx_nic_t *enp);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_tx_qcreate(
	__in		efx_nic_t *enp,
	__in		unsigned int index,
	__in		unsigned int label,
	__in		efsys_mem_t *esmp,
	__in		size_t ndescs,
	__in		uint32_t id,
	__in		uint16_t flags,
	__in		efx_evq_t *eep,
	__in		efx_txq_t *etp,
	__out		unsigned int *addedp);

LIBEFX_INTERNAL
extern		void
rhead_tx_qdestroy(
	__in		efx_txq_t *etp);

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
rhead_tx_qpost(
	__in			efx_txq_t *etp,
	__in_ecount(ndescs)	efx_buffer_t *ebp,
	__in			unsigned int ndescs,
	__in			unsigned int completed,
	__inout			unsigned int *addedp);

LIBEFX_INTERNAL
extern			void
rhead_tx_qpush(
	__in		efx_txq_t *etp,
	__in		unsigned int added,
	__in		unsigned int pushed);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_tx_qpace(
	__in		efx_txq_t *etp,
	__in		unsigned int ns);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_tx_qflush(
	__in		efx_txq_t *etp);

LIBEFX_INTERNAL
extern			void
rhead_tx_qenable(
	__in		efx_txq_t *etp);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_tx_qdesc_post(
	__in		efx_txq_t *etp,
	__in_ecount(n)	efx_desc_t *ed,
	__in		unsigned int n,
	__in		unsigned int completed,
	__inout		unsigned int *addedp);

#if EFSYS_OPT_QSTATS

LIBEFX_INTERNAL
extern			void
rhead_tx_qstats_update(
	__in				efx_txq_t *etp,
	__inout_ecount(TX_NQSTATS)	efsys_stat_t *stat);

#endif /* EFSYS_OPT_QSTATS */

#if EFSYS_OPT_TUNNEL

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_tunnel_reconfigure(
	__in		efx_nic_t *enp);

LIBEFX_INTERNAL
extern			void
rhead_tunnel_fini(
	__in		efx_nic_t *enp);

#endif /* EFSYS_OPT_TUNNEL */

#if EFSYS_OPT_PCI

/*
 * Perform discovery of function control window by looking for a
 * EF100 locator in Xilinx capabilities tables.
 */
LIBEFX_INTERNAL
extern	__checkReturn			efx_rc_t
rhead_pci_nic_membar_lookup(
	__in				efsys_pci_config_t *espcp,
	__in				const efx_pci_ops_t *epop,
	__out				efx_bar_region_t *ebrp);

#endif /* EFSYS_OPT_PCI */

LIBEFX_INTERNAL
extern	__checkReturn			efx_rc_t
rhead_nic_xilinx_cap_tbl_read_ef100_locator(
	__in				efsys_bar_t *esbp,
	__in				efsys_dma_addr_t offset,
	__out				efx_bar_region_t *ebrp);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_RHEAD_IMPL_H */
