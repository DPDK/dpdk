/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2007-2019 Solarflare Communications Inc.
 */

#ifndef	_SYS_EFX_IMPL_H
#define	_SYS_EFX_IMPL_H

#include "efx.h"
#include "efx_regs.h"
#include "efx_regs_ef10.h"
#include "efx_regs_ef100.h"
#if EFSYS_OPT_MCDI
#include "efx_mcdi.h"
#endif	/* EFSYS_OPT_MCDI */

/* FIXME: Add definition for driver generated software events */
#ifndef	ESE_DZ_EV_CODE_DRV_GEN_EV
#define	ESE_DZ_EV_CODE_DRV_GEN_EV FSE_AZ_EV_CODE_DRV_GEN_EV
#endif


#if EFSYS_OPT_SIENA
#include "siena_impl.h"
#endif	/* EFSYS_OPT_SIENA */

#if EFSYS_OPT_HUNTINGTON
#include "hunt_impl.h"
#endif	/* EFSYS_OPT_HUNTINGTON */

#if EFSYS_OPT_MEDFORD
#include "medford_impl.h"
#endif	/* EFSYS_OPT_MEDFORD */

#if EFSYS_OPT_MEDFORD2
#include "medford2_impl.h"
#endif	/* EFSYS_OPT_MEDFORD2 */

#if EFSYS_OPT_RIVERHEAD || EFX_OPTS_EF10()
#include "ef10_impl.h"
#endif	/* EFSYS_OPT_RIVERHEAD || EFX_OPTS_EF10() */

#if EFSYS_OPT_RIVERHEAD
#include "rhead_impl.h"
#endif	/* EFSYS_OPT_RIVERHEAD */

#ifdef	__cplusplus
extern "C" {
#endif

#define	EFX_MOD_MCDI		0x00000001
#define	EFX_MOD_PROBE		0x00000002
#define	EFX_MOD_NVRAM		0x00000004
#define	EFX_MOD_VPD		0x00000008
#define	EFX_MOD_NIC		0x00000010
#define	EFX_MOD_INTR		0x00000020
#define	EFX_MOD_EV		0x00000040
#define	EFX_MOD_RX		0x00000080
#define	EFX_MOD_TX		0x00000100
#define	EFX_MOD_PORT		0x00000200
#define	EFX_MOD_MON		0x00000400
#define	EFX_MOD_FILTER		0x00001000
#define	EFX_MOD_LIC		0x00002000
#define	EFX_MOD_TUNNEL		0x00004000
#define	EFX_MOD_EVB		0x00008000
#define	EFX_MOD_PROXY		0x00010000
#define	EFX_MOD_VIRTIO		0x00020000

#define	EFX_RESET_PHY		0x00000001
#define	EFX_RESET_RXQ_ERR	0x00000002
#define	EFX_RESET_TXQ_ERR	0x00000004
#define	EFX_RESET_HW_UNAVAIL	0x00000008

typedef enum efx_mac_type_e {
	EFX_MAC_INVALID = 0,
	EFX_MAC_SIENA,
	EFX_MAC_HUNTINGTON,
	EFX_MAC_MEDFORD,
	EFX_MAC_MEDFORD2,
	EFX_MAC_RIVERHEAD,
	EFX_MAC_NTYPES
} efx_mac_type_t;

typedef struct efx_ev_ops_s {
	efx_rc_t	(*eevo_init)(efx_nic_t *);
	void		(*eevo_fini)(efx_nic_t *);
	efx_rc_t	(*eevo_qcreate)(efx_nic_t *, unsigned int,
					  efsys_mem_t *, size_t, uint32_t,
					  uint32_t, uint32_t, uint32_t,
					  efx_evq_t *);
	void		(*eevo_qdestroy)(efx_evq_t *);
	efx_rc_t	(*eevo_qprime)(efx_evq_t *, unsigned int);
	void		(*eevo_qpost)(efx_evq_t *, uint16_t);
	void		(*eevo_qpoll)(efx_evq_t *, unsigned int *,
					const efx_ev_callbacks_t *, void *);
	efx_rc_t	(*eevo_qmoderate)(efx_evq_t *, unsigned int);
#if EFSYS_OPT_QSTATS
	void		(*eevo_qstats_update)(efx_evq_t *, efsys_stat_t *);
#endif
} efx_ev_ops_t;

typedef struct efx_tx_ops_s {
	efx_rc_t	(*etxo_init)(efx_nic_t *);
	void		(*etxo_fini)(efx_nic_t *);
	efx_rc_t	(*etxo_qcreate)(efx_nic_t *,
					unsigned int, unsigned int,
					efsys_mem_t *, size_t,
					uint32_t, uint16_t,
					efx_evq_t *, efx_txq_t *,
					unsigned int *);
	void		(*etxo_qdestroy)(efx_txq_t *);
	efx_rc_t	(*etxo_qpost)(efx_txq_t *, efx_buffer_t *,
				      unsigned int, unsigned int,
				      unsigned int *);
	void		(*etxo_qpush)(efx_txq_t *, unsigned int, unsigned int);
	efx_rc_t	(*etxo_qpace)(efx_txq_t *, unsigned int);
	efx_rc_t	(*etxo_qflush)(efx_txq_t *);
	void		(*etxo_qenable)(efx_txq_t *);
	efx_rc_t	(*etxo_qpio_enable)(efx_txq_t *);
	void		(*etxo_qpio_disable)(efx_txq_t *);
	efx_rc_t	(*etxo_qpio_write)(efx_txq_t *, uint8_t *, size_t,
					   size_t);
	efx_rc_t	(*etxo_qpio_post)(efx_txq_t *, size_t, unsigned int,
					   unsigned int *);
	efx_rc_t	(*etxo_qdesc_post)(efx_txq_t *, efx_desc_t *,
				      unsigned int, unsigned int,
				      unsigned int *);
	void		(*etxo_qdesc_dma_create)(efx_txq_t *, efsys_dma_addr_t,
						size_t, boolean_t,
						efx_desc_t *);
	void		(*etxo_qdesc_tso_create)(efx_txq_t *, uint16_t,
						uint32_t, uint8_t,
						efx_desc_t *);
	void		(*etxo_qdesc_tso2_create)(efx_txq_t *, uint16_t,
						uint16_t, uint32_t, uint16_t,
						efx_desc_t *, int);
	void		(*etxo_qdesc_vlantci_create)(efx_txq_t *, uint16_t,
						efx_desc_t *);
	void		(*etxo_qdesc_checksum_create)(efx_txq_t *, uint16_t,
						efx_desc_t *);
#if EFSYS_OPT_QSTATS
	void		(*etxo_qstats_update)(efx_txq_t *,
					      efsys_stat_t *);
#endif
} efx_tx_ops_t;

typedef union efx_rxq_type_data_u {
	struct {
		size_t		ed_buf_size;
	} ertd_default;
#if EFSYS_OPT_RX_PACKED_STREAM
	struct {
		uint32_t	eps_buf_size;
	} ertd_packed_stream;
#endif
#if EFSYS_OPT_RX_ES_SUPER_BUFFER
	struct {
		uint32_t	eessb_bufs_per_desc;
		uint32_t	eessb_max_dma_len;
		uint32_t	eessb_buf_stride;
		uint32_t	eessb_hol_block_timeout;
	} ertd_es_super_buffer;
#endif
} efx_rxq_type_data_t;

typedef struct efx_rx_ops_s {
	efx_rc_t	(*erxo_init)(efx_nic_t *);
	void		(*erxo_fini)(efx_nic_t *);
#if EFSYS_OPT_RX_SCATTER
	efx_rc_t	(*erxo_scatter_enable)(efx_nic_t *, unsigned int);
#endif
#if EFSYS_OPT_RX_SCALE
	efx_rc_t	(*erxo_scale_context_alloc)(efx_nic_t *,
						    efx_rx_scale_context_type_t,
						    uint32_t, uint32_t,
						    uint32_t *);
	efx_rc_t	(*erxo_scale_context_free)(efx_nic_t *, uint32_t);
	efx_rc_t	(*erxo_scale_mode_set)(efx_nic_t *, uint32_t,
					       efx_rx_hash_alg_t,
					       efx_rx_hash_type_t, boolean_t);
	efx_rc_t	(*erxo_scale_key_set)(efx_nic_t *, uint32_t,
					      uint8_t *, size_t);
	efx_rc_t	(*erxo_scale_tbl_set)(efx_nic_t *, uint32_t,
					      unsigned int *, size_t);
	uint32_t	(*erxo_prefix_hash)(efx_nic_t *, efx_rx_hash_alg_t,
					    uint8_t *);
#endif /* EFSYS_OPT_RX_SCALE */
	efx_rc_t	(*erxo_prefix_pktlen)(efx_nic_t *, uint8_t *,
					      uint16_t *);
	void		(*erxo_qpost)(efx_rxq_t *, efsys_dma_addr_t *, size_t,
				      unsigned int, unsigned int,
				      unsigned int);
	void		(*erxo_qpush)(efx_rxq_t *, unsigned int, unsigned int *);
#if EFSYS_OPT_RX_PACKED_STREAM
	void		(*erxo_qpush_ps_credits)(efx_rxq_t *);
	uint8_t *	(*erxo_qps_packet_info)(efx_rxq_t *, uint8_t *,
						uint32_t, uint32_t,
						uint16_t *, uint32_t *, uint32_t *);
#endif
	efx_rc_t	(*erxo_qflush)(efx_rxq_t *);
	void		(*erxo_qenable)(efx_rxq_t *);
	efx_rc_t	(*erxo_qcreate)(efx_nic_t *enp, unsigned int,
					unsigned int, efx_rxq_type_t,
					const efx_rxq_type_data_t *,
					efsys_mem_t *, size_t, uint32_t,
					unsigned int,
					efx_evq_t *, efx_rxq_t *);
	void		(*erxo_qdestroy)(efx_rxq_t *);
} efx_rx_ops_t;

typedef struct efx_mac_ops_s {
	efx_rc_t	(*emo_poll)(efx_nic_t *, efx_link_mode_t *);
	efx_rc_t	(*emo_up)(efx_nic_t *, boolean_t *);
	efx_rc_t	(*emo_addr_set)(efx_nic_t *);
	efx_rc_t	(*emo_pdu_set)(efx_nic_t *);
	efx_rc_t	(*emo_pdu_get)(efx_nic_t *, size_t *);
	efx_rc_t	(*emo_reconfigure)(efx_nic_t *);
	efx_rc_t	(*emo_multicast_list_set)(efx_nic_t *);
	efx_rc_t	(*emo_filter_default_rxq_set)(efx_nic_t *,
						      efx_rxq_t *, boolean_t);
	void		(*emo_filter_default_rxq_clear)(efx_nic_t *);
#if EFSYS_OPT_LOOPBACK
	efx_rc_t	(*emo_loopback_set)(efx_nic_t *, efx_link_mode_t,
					    efx_loopback_type_t);
#endif	/* EFSYS_OPT_LOOPBACK */
#if EFSYS_OPT_MAC_STATS
	efx_rc_t	(*emo_stats_get_mask)(efx_nic_t *, uint32_t *, size_t);
	efx_rc_t	(*emo_stats_clear)(efx_nic_t *);
	efx_rc_t	(*emo_stats_upload)(efx_nic_t *, efsys_mem_t *);
	efx_rc_t	(*emo_stats_periodic)(efx_nic_t *, efsys_mem_t *,
					      uint16_t, boolean_t);
	efx_rc_t	(*emo_stats_update)(efx_nic_t *, efsys_mem_t *,
					    efsys_stat_t *, uint32_t *);
#endif	/* EFSYS_OPT_MAC_STATS */
} efx_mac_ops_t;

typedef struct efx_phy_ops_s {
	efx_rc_t	(*epo_power)(efx_nic_t *, boolean_t); /* optional */
	efx_rc_t	(*epo_reset)(efx_nic_t *);
	efx_rc_t	(*epo_reconfigure)(efx_nic_t *);
	efx_rc_t	(*epo_verify)(efx_nic_t *);
	efx_rc_t	(*epo_oui_get)(efx_nic_t *, uint32_t *);
	efx_rc_t	(*epo_link_state_get)(efx_nic_t *, efx_phy_link_state_t *);
#if EFSYS_OPT_PHY_STATS
	efx_rc_t	(*epo_stats_update)(efx_nic_t *, efsys_mem_t *,
					    uint32_t *);
#endif	/* EFSYS_OPT_PHY_STATS */
#if EFSYS_OPT_BIST
	efx_rc_t	(*epo_bist_enable_offline)(efx_nic_t *);
	efx_rc_t	(*epo_bist_start)(efx_nic_t *, efx_bist_type_t);
	efx_rc_t	(*epo_bist_poll)(efx_nic_t *, efx_bist_type_t,
					 efx_bist_result_t *, uint32_t *,
					 unsigned long *, size_t);
	void		(*epo_bist_stop)(efx_nic_t *, efx_bist_type_t);
#endif	/* EFSYS_OPT_BIST */
} efx_phy_ops_t;

#if EFSYS_OPT_FILTER

/*
 * Policy for replacing existing filter when inserting a new one.
 * Note that all policies allow for storing the new lower priority
 * filters as overridden by existing higher priority ones. It is needed
 * to restore the lower priority filters on higher priority ones removal.
 */
typedef enum efx_filter_replacement_policy_e {
	/* Cannot replace existing filter */
	EFX_FILTER_REPLACEMENT_NEVER,
	/* Higher priority filters can replace lower priotiry ones */
	EFX_FILTER_REPLACEMENT_HIGHER_PRIORITY,
	/*
	 * Higher priority filters can replace lower priority ones and
	 * equal priority filters can replace each other.
	 */
	EFX_FILTER_REPLACEMENT_HIGHER_OR_EQUAL_PRIORITY,
} efx_filter_replacement_policy_t;

typedef struct efx_filter_ops_s {
	efx_rc_t	(*efo_init)(efx_nic_t *);
	void		(*efo_fini)(efx_nic_t *);
	efx_rc_t	(*efo_restore)(efx_nic_t *);
	efx_rc_t	(*efo_add)(efx_nic_t *, efx_filter_spec_t *,
				   efx_filter_replacement_policy_t policy);
	efx_rc_t	(*efo_delete)(efx_nic_t *, efx_filter_spec_t *);
	efx_rc_t	(*efo_supported_filters)(efx_nic_t *, uint32_t *,
				   size_t, size_t *);
	efx_rc_t	(*efo_reconfigure)(efx_nic_t *, uint8_t const *, boolean_t,
				   boolean_t, boolean_t, boolean_t,
				   uint8_t const *, uint32_t);
	efx_rc_t	(*efo_get_count)(efx_nic_t *, uint32_t *);
} efx_filter_ops_t;

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
efx_filter_reconfigure(
	__in				efx_nic_t *enp,
	__in_ecount(6)			uint8_t const *mac_addr,
	__in				boolean_t all_unicst,
	__in				boolean_t mulcst,
	__in				boolean_t all_mulcst,
	__in				boolean_t brdcst,
	__in_ecount(6*count)		uint8_t const *addrs,
	__in				uint32_t count);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
efx_filter_get_count(
	__in	efx_nic_t *enp,
	__out	uint32_t *countp);

#endif /* EFSYS_OPT_FILTER */

#if EFSYS_OPT_TUNNEL
typedef struct efx_tunnel_ops_s {
	efx_rc_t	(*eto_reconfigure)(efx_nic_t *);
	void		(*eto_fini)(efx_nic_t *);
} efx_tunnel_ops_t;
#endif /* EFSYS_OPT_TUNNEL */

#if EFSYS_OPT_VIRTIO
typedef struct efx_virtio_ops_s {
	efx_rc_t	(*evo_virtio_qstart)(efx_virtio_vq_t *,
				efx_virtio_vq_cfg_t *,
				efx_virtio_vq_dyncfg_t *);
	efx_rc_t	(*evo_virtio_qstop)(efx_virtio_vq_t *,
				efx_virtio_vq_dyncfg_t *);
	efx_rc_t	(*evo_get_doorbell_offset)(efx_virtio_vq_t *,
				uint32_t *);
	efx_rc_t	(*evo_get_features)(efx_nic_t *,
				efx_virtio_device_type_t, uint64_t *);
	efx_rc_t	(*evo_verify_features)(efx_nic_t *,
				efx_virtio_device_type_t, uint64_t);
} efx_virtio_ops_t;
#endif /* EFSYS_OPT_VIRTIO */

typedef struct efx_port_s {
	efx_mac_type_t		ep_mac_type;
	uint32_t		ep_phy_type;
	uint8_t			ep_port;
	uint32_t		ep_mac_pdu;
	uint8_t			ep_mac_addr[6];
	efx_link_mode_t		ep_link_mode;
	boolean_t		ep_all_unicst;
	boolean_t		ep_all_unicst_inserted;
	boolean_t		ep_mulcst;
	boolean_t		ep_all_mulcst;
	boolean_t		ep_all_mulcst_inserted;
	boolean_t		ep_brdcst;
	unsigned int		ep_fcntl;
	boolean_t		ep_fcntl_autoneg;
	efx_oword_t		ep_multicst_hash[2];
	uint8_t			ep_mulcst_addr_list[EFX_MAC_ADDR_LEN *
						    EFX_MAC_MULTICAST_LIST_MAX];
	uint32_t		ep_mulcst_addr_count;
#if EFSYS_OPT_LOOPBACK
	efx_loopback_type_t	ep_loopback_type;
	efx_link_mode_t		ep_loopback_link_mode;
#endif	/* EFSYS_OPT_LOOPBACK */
#if EFSYS_OPT_PHY_FLAGS
	uint32_t		ep_phy_flags;
#endif	/* EFSYS_OPT_PHY_FLAGS */
#if EFSYS_OPT_PHY_LED_CONTROL
	efx_phy_led_mode_t	ep_phy_led_mode;
#endif	/* EFSYS_OPT_PHY_LED_CONTROL */
	efx_phy_media_type_t	ep_fixed_port_type;
	efx_phy_media_type_t	ep_module_type;
	uint32_t		ep_adv_cap_mask;
	uint32_t		ep_lp_cap_mask;
	uint32_t		ep_default_adv_cap_mask;
	uint32_t		ep_phy_cap_mask;
	boolean_t		ep_mac_drain;
	boolean_t		ep_include_fcs;
	boolean_t		ep_vlan_strip;
#if EFSYS_OPT_BIST
	efx_bist_type_t		ep_current_bist;
#endif
	const efx_mac_ops_t	*ep_emop;
	const efx_phy_ops_t	*ep_epop;
} efx_port_t;

typedef struct efx_mon_ops_s {
#if EFSYS_OPT_MON_STATS
	efx_rc_t	(*emo_stats_update)(efx_nic_t *, efsys_mem_t *,
					    efx_mon_stat_value_t *);
	efx_rc_t	(*emo_limits_update)(efx_nic_t *,
					     efx_mon_stat_limits_t *);
#endif	/* EFSYS_OPT_MON_STATS */
} efx_mon_ops_t;

typedef struct efx_mon_s {
	efx_mon_type_t		em_type;
	const efx_mon_ops_t	*em_emop;
} efx_mon_t;

typedef struct efx_intr_ops_s {
	efx_rc_t	(*eio_init)(efx_nic_t *, efx_intr_type_t, efsys_mem_t *);
	void		(*eio_enable)(efx_nic_t *);
	void		(*eio_disable)(efx_nic_t *);
	void		(*eio_disable_unlocked)(efx_nic_t *);
	efx_rc_t	(*eio_trigger)(efx_nic_t *, unsigned int);
	void		(*eio_status_line)(efx_nic_t *, boolean_t *, uint32_t *);
	void		(*eio_status_message)(efx_nic_t *, unsigned int,
				 boolean_t *);
	void		(*eio_fatal)(efx_nic_t *);
	void		(*eio_fini)(efx_nic_t *);
} efx_intr_ops_t;

typedef struct efx_intr_s {
	const efx_intr_ops_t	*ei_eiop;
	efsys_mem_t		*ei_esmp;
	efx_intr_type_t		ei_type;
	unsigned int		ei_level;
} efx_intr_t;

typedef struct efx_nic_ops_s {
	efx_rc_t	(*eno_probe)(efx_nic_t *);
	efx_rc_t	(*eno_board_cfg)(efx_nic_t *);
	efx_rc_t	(*eno_set_drv_limits)(efx_nic_t *, efx_drv_limits_t*);
	efx_rc_t	(*eno_reset)(efx_nic_t *);
	efx_rc_t	(*eno_init)(efx_nic_t *);
	efx_rc_t	(*eno_get_vi_pool)(efx_nic_t *, uint32_t *);
	efx_rc_t	(*eno_get_bar_region)(efx_nic_t *, efx_nic_region_t,
					uint32_t *, size_t *);
	boolean_t	(*eno_hw_unavailable)(efx_nic_t *);
	void		(*eno_set_hw_unavailable)(efx_nic_t *);
#if EFSYS_OPT_DIAG
	efx_rc_t	(*eno_register_test)(efx_nic_t *);
#endif	/* EFSYS_OPT_DIAG */
	void		(*eno_fini)(efx_nic_t *);
	void		(*eno_unprobe)(efx_nic_t *);
} efx_nic_ops_t;

#ifndef EFX_TXQ_LIMIT_TARGET
#define	EFX_TXQ_LIMIT_TARGET 259
#endif
#ifndef EFX_RXQ_LIMIT_TARGET
#define	EFX_RXQ_LIMIT_TARGET 512
#endif

typedef struct efx_nic_dma_region_s {
	efsys_dma_addr_t	endr_nic_base;
	efsys_dma_addr_t	endr_trgt_base;
	unsigned int		endr_window_log2;
	unsigned int		endr_align_log2;
	boolean_t		endr_inuse;
} efx_nic_dma_region_t;

typedef struct efx_nic_dma_region_info_s {
	unsigned int		endri_count;
	efx_nic_dma_region_t	*endri_regions;
} efx_nic_dma_region_info_t;

typedef struct efx_nic_dma_s {
	union {
		/* No configuration in the case flat mapping type */
		efx_nic_dma_region_info_t	endu_region_info;
	} end_u;
} efx_nic_dma_t;

#if EFSYS_OPT_FILTER

#if EFSYS_OPT_SIENA

typedef struct siena_filter_spec_s {
	uint8_t		sfs_type;
	uint32_t	sfs_flags;
	uint32_t	sfs_dmaq_id;
	uint32_t	sfs_dword[3];
} siena_filter_spec_t;

typedef enum siena_filter_type_e {
	EFX_SIENA_FILTER_RX_TCP_FULL,	/* TCP/IPv4 {dIP,dTCP,sIP,sTCP} */
	EFX_SIENA_FILTER_RX_TCP_WILD,	/* TCP/IPv4 {dIP,dTCP,  -,   -} */
	EFX_SIENA_FILTER_RX_UDP_FULL,	/* UDP/IPv4 {dIP,dUDP,sIP,sUDP} */
	EFX_SIENA_FILTER_RX_UDP_WILD,	/* UDP/IPv4 {dIP,dUDP,  -,   -} */
	EFX_SIENA_FILTER_RX_MAC_FULL,	/* Ethernet {dMAC,VLAN} */
	EFX_SIENA_FILTER_RX_MAC_WILD,	/* Ethernet {dMAC,   -} */

	EFX_SIENA_FILTER_TX_TCP_FULL,	/* TCP/IPv4 {dIP,dTCP,sIP,sTCP} */
	EFX_SIENA_FILTER_TX_TCP_WILD,	/* TCP/IPv4 {  -,   -,sIP,sTCP} */
	EFX_SIENA_FILTER_TX_UDP_FULL,	/* UDP/IPv4 {dIP,dTCP,sIP,sTCP} */
	EFX_SIENA_FILTER_TX_UDP_WILD,	/* UDP/IPv4 {  -,   -,sIP,sUDP} */
	EFX_SIENA_FILTER_TX_MAC_FULL,	/* Ethernet {sMAC,VLAN} */
	EFX_SIENA_FILTER_TX_MAC_WILD,	/* Ethernet {sMAC,   -} */

	EFX_SIENA_FILTER_NTYPES
} siena_filter_type_t;

typedef enum siena_filter_tbl_id_e {
	EFX_SIENA_FILTER_TBL_RX_IP = 0,
	EFX_SIENA_FILTER_TBL_RX_MAC,
	EFX_SIENA_FILTER_TBL_TX_IP,
	EFX_SIENA_FILTER_TBL_TX_MAC,
	EFX_SIENA_FILTER_NTBLS
} siena_filter_tbl_id_t;

typedef struct siena_filter_tbl_s {
	int			sft_size;	/* number of entries */
	int			sft_used;	/* active count */
	uint32_t		*sft_bitmap;	/* active bitmap */
	siena_filter_spec_t	*sft_spec;	/* array of saved specs */
} siena_filter_tbl_t;

typedef struct siena_filter_s {
	siena_filter_tbl_t	sf_tbl[EFX_SIENA_FILTER_NTBLS];
	unsigned int		sf_depth[EFX_SIENA_FILTER_NTYPES];
} siena_filter_t;

#endif	/* EFSYS_OPT_SIENA */

typedef struct efx_filter_s {
#if EFSYS_OPT_SIENA
	siena_filter_t		*ef_siena_filter;
#endif /* EFSYS_OPT_SIENA */
#if EFSYS_OPT_RIVERHEAD || EFX_OPTS_EF10()
	ef10_filter_table_t	*ef_ef10_filter_table;
#endif /* EFSYS_OPT_RIVERHEAD || EFX_OPTS_EF10() */
} efx_filter_t;

#if EFSYS_OPT_SIENA

LIBEFX_INTERNAL
extern			void
siena_filter_tbl_clear(
	__in		efx_nic_t *enp,
	__in		siena_filter_tbl_id_t tbl);

#endif	/* EFSYS_OPT_SIENA */

#endif	/* EFSYS_OPT_FILTER */

#if EFSYS_OPT_MCDI

#define	EFX_TUNNEL_MAXNENTRIES	(16)

#if EFSYS_OPT_TUNNEL

/* State of a UDP tunnel table entry */
typedef enum efx_tunnel_udp_entry_state_e {
	EFX_TUNNEL_UDP_ENTRY_ADDED, /* Tunnel addition is requested */
	EFX_TUNNEL_UDP_ENTRY_REMOVED, /* Tunnel removal is requested */
	EFX_TUNNEL_UDP_ENTRY_APPLIED, /* Tunnel is applied by HW */
} efx_tunnel_udp_entry_state_t;

#if EFSYS_OPT_RIVERHEAD
typedef uint32_t	efx_vnic_encap_rule_handle_t;
#endif /* EFSYS_OPT_RIVERHEAD */

typedef struct efx_tunnel_udp_entry_s {
	uint16_t			etue_port; /* host/cpu-endian */
	uint16_t			etue_protocol;
	boolean_t			etue_busy;
	efx_tunnel_udp_entry_state_t	etue_state;
#if EFSYS_OPT_RIVERHEAD
	efx_vnic_encap_rule_handle_t	etue_handle;
#endif /* EFSYS_OPT_RIVERHEAD */
} efx_tunnel_udp_entry_t;

typedef struct efx_tunnel_cfg_s {
	efx_tunnel_udp_entry_t	etc_udp_entries[EFX_TUNNEL_MAXNENTRIES];
	unsigned int		etc_udp_entries_num;
} efx_tunnel_cfg_t;

#endif /* EFSYS_OPT_TUNNEL */

typedef struct efx_mcdi_ops_s {
	efx_rc_t	(*emco_init)(efx_nic_t *, const efx_mcdi_transport_t *);
	void		(*emco_send_request)(efx_nic_t *, void *, size_t,
					void *, size_t);
	efx_rc_t	(*emco_poll_reboot)(efx_nic_t *);
	boolean_t	(*emco_poll_response)(efx_nic_t *);
	void		(*emco_read_response)(efx_nic_t *, void *, size_t, size_t);
	void		(*emco_fini)(efx_nic_t *);
	efx_rc_t	(*emco_feature_supported)(efx_nic_t *,
					    efx_mcdi_feature_id_t, boolean_t *);
	void		(*emco_get_timeout)(efx_nic_t *, efx_mcdi_req_t *,
					    uint32_t *);
} efx_mcdi_ops_t;

typedef struct efx_mcdi_s {
	const efx_mcdi_ops_t		*em_emcop;
	const efx_mcdi_transport_t	*em_emtp;
	efx_mcdi_iface_t		em_emip;
} efx_mcdi_t;

#endif /* EFSYS_OPT_MCDI */

#if EFSYS_OPT_NVRAM

/* Invalid partition ID for en_nvram_partn_locked field of efx_nc_t */
#define	EFX_NVRAM_PARTN_INVALID		(0xffffffffu)

typedef struct efx_nvram_ops_s {
#if EFSYS_OPT_DIAG
	efx_rc_t	(*envo_test)(efx_nic_t *);
#endif	/* EFSYS_OPT_DIAG */
	efx_rc_t	(*envo_type_to_partn)(efx_nic_t *, efx_nvram_type_t,
					    uint32_t *);
	efx_rc_t	(*envo_partn_info)(efx_nic_t *, uint32_t,
					    efx_nvram_info_t *);
	efx_rc_t	(*envo_partn_rw_start)(efx_nic_t *, uint32_t, size_t *);
	efx_rc_t	(*envo_partn_read)(efx_nic_t *, uint32_t,
					    unsigned int, caddr_t, size_t);
	efx_rc_t	(*envo_partn_read_backup)(efx_nic_t *, uint32_t,
					    unsigned int, caddr_t, size_t);
	efx_rc_t	(*envo_partn_erase)(efx_nic_t *, uint32_t,
					    unsigned int, size_t);
	efx_rc_t	(*envo_partn_write)(efx_nic_t *, uint32_t,
					    unsigned int, caddr_t, size_t);
	efx_rc_t	(*envo_partn_rw_finish)(efx_nic_t *, uint32_t,
					    uint32_t *);
	efx_rc_t	(*envo_partn_get_version)(efx_nic_t *, uint32_t,
					    uint32_t *, uint16_t *);
	efx_rc_t	(*envo_partn_set_version)(efx_nic_t *, uint32_t,
					    uint16_t *);
	efx_rc_t	(*envo_buffer_validate)(uint32_t,
					    caddr_t, size_t);
} efx_nvram_ops_t;
#endif /* EFSYS_OPT_NVRAM */

#if EFSYS_OPT_VPD
typedef struct efx_vpd_ops_s {
	efx_rc_t	(*evpdo_init)(efx_nic_t *);
	efx_rc_t	(*evpdo_size)(efx_nic_t *, size_t *);
	efx_rc_t	(*evpdo_read)(efx_nic_t *, caddr_t, size_t);
	efx_rc_t	(*evpdo_verify)(efx_nic_t *, caddr_t, size_t);
	efx_rc_t	(*evpdo_reinit)(efx_nic_t *, caddr_t, size_t);
	efx_rc_t	(*evpdo_get)(efx_nic_t *, caddr_t, size_t,
					efx_vpd_value_t *);
	efx_rc_t	(*evpdo_set)(efx_nic_t *, caddr_t, size_t,
					efx_vpd_value_t *);
	efx_rc_t	(*evpdo_next)(efx_nic_t *, caddr_t, size_t,
					efx_vpd_value_t *, unsigned int *);
	efx_rc_t	(*evpdo_write)(efx_nic_t *, caddr_t, size_t);
	void		(*evpdo_fini)(efx_nic_t *);
} efx_vpd_ops_t;
#endif	/* EFSYS_OPT_VPD */

#if EFSYS_OPT_VPD || EFSYS_OPT_NVRAM

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_mcdi_nvram_partitions(
	__in			efx_nic_t *enp,
	__out_bcount(size)	caddr_t data,
	__in			size_t size,
	__out			unsigned int *npartnp);

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_mcdi_nvram_metadata(
	__in			efx_nic_t *enp,
	__in			uint32_t partn,
	__out			uint32_t *subtypep,
	__out_ecount(4)		uint16_t version[4],
	__out_bcount_opt(size)	char *descp,
	__in			size_t size);

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_mcdi_nvram_info(
	__in			efx_nic_t *enp,
	__in			uint32_t partn,
	__out			efx_nvram_info_t *eni);

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_mcdi_nvram_update_start(
	__in			efx_nic_t *enp,
	__in			uint32_t partn);

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_mcdi_nvram_read(
	__in			efx_nic_t *enp,
	__in			uint32_t partn,
	__in			uint32_t offset,
	__out_bcount(size)	caddr_t data,
	__in			size_t size,
	__in			uint32_t mode);

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_mcdi_nvram_erase(
	__in			efx_nic_t *enp,
	__in			uint32_t partn,
	__in			uint32_t offset,
	__in			size_t size);

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_mcdi_nvram_write(
	__in			efx_nic_t *enp,
	__in			uint32_t partn,
	__in			uint32_t offset,
	__in_bcount(size)	caddr_t data,
	__in			size_t size);

#define	EFX_NVRAM_UPDATE_FLAGS_BACKGROUND	0x00000001
#define	EFX_NVRAM_UPDATE_FLAGS_POLL		0x00000002

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_mcdi_nvram_update_finish(
	__in			efx_nic_t *enp,
	__in			uint32_t partn,
	__in			boolean_t reboot,
	__in			uint32_t flags,
	__out_opt		uint32_t *verify_resultp);

#if EFSYS_OPT_DIAG

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_mcdi_nvram_test(
	__in			efx_nic_t *enp,
	__in			uint32_t partn);

#endif	/* EFSYS_OPT_DIAG */

#endif /* EFSYS_OPT_VPD || EFSYS_OPT_NVRAM */

#if EFSYS_OPT_LICENSING

typedef struct efx_lic_ops_s {
	efx_rc_t	(*elo_update_licenses)(efx_nic_t *);
	efx_rc_t	(*elo_get_key_stats)(efx_nic_t *, efx_key_stats_t *);
	efx_rc_t	(*elo_app_state)(efx_nic_t *, uint64_t, boolean_t *);
	efx_rc_t	(*elo_get_id)(efx_nic_t *, size_t, uint32_t *,
				      size_t *, uint8_t *);
	efx_rc_t	(*elo_find_start)
				(efx_nic_t *, caddr_t, size_t, uint32_t *);
	efx_rc_t	(*elo_find_end)(efx_nic_t *, caddr_t, size_t,
				uint32_t, uint32_t *);
	boolean_t	(*elo_find_key)(efx_nic_t *, caddr_t, size_t,
				uint32_t, uint32_t *, uint32_t *);
	boolean_t	(*elo_validate_key)(efx_nic_t *,
				caddr_t, uint32_t);
	efx_rc_t	(*elo_read_key)(efx_nic_t *,
				caddr_t, size_t, uint32_t, uint32_t,
				caddr_t, size_t, uint32_t *);
	efx_rc_t	(*elo_write_key)(efx_nic_t *,
				caddr_t, size_t, uint32_t,
				caddr_t, uint32_t, uint32_t *);
	efx_rc_t	(*elo_delete_key)(efx_nic_t *,
				caddr_t, size_t, uint32_t,
				uint32_t, uint32_t, uint32_t *);
	efx_rc_t	(*elo_create_partition)(efx_nic_t *,
				caddr_t, size_t);
	efx_rc_t	(*elo_finish_partition)(efx_nic_t *,
				caddr_t, size_t);
} efx_lic_ops_t;

#endif

#if EFSYS_OPT_EVB

struct efx_vswitch_s {
	efx_nic_t		*ev_enp;
	efx_vswitch_id_t	ev_vswitch_id;
	uint32_t		ev_num_vports;
	/*
	 * Vport configuration array: index 0 to store PF configuration
	 * and next ev_num_vports-1 entries hold VFs configuration.
	 */
	efx_vport_config_t	*ev_evcp;
};

typedef struct efx_evb_ops_s {
	efx_rc_t	(*eeo_init)(efx_nic_t *);
	void		(*eeo_fini)(efx_nic_t *);
	efx_rc_t	(*eeo_vswitch_alloc)(efx_nic_t *, efx_vswitch_id_t *);
	efx_rc_t	(*eeo_vswitch_free)(efx_nic_t *, efx_vswitch_id_t);
	efx_rc_t	(*eeo_vport_alloc)(efx_nic_t *, efx_vswitch_id_t,
						efx_vport_type_t, uint16_t,
						boolean_t, efx_vport_id_t *);
	efx_rc_t	(*eeo_vport_free)(efx_nic_t *, efx_vswitch_id_t,
						efx_vport_id_t);
	efx_rc_t	(*eeo_vport_mac_addr_add)(efx_nic_t *, efx_vswitch_id_t,
						efx_vport_id_t, uint8_t *);
	efx_rc_t	(*eeo_vport_mac_addr_del)(efx_nic_t *, efx_vswitch_id_t,
						efx_vport_id_t, uint8_t *);
	efx_rc_t	(*eeo_vadaptor_alloc)(efx_nic_t *, efx_vswitch_id_t,
						efx_vport_id_t);
	efx_rc_t	(*eeo_vadaptor_free)(efx_nic_t *, efx_vswitch_id_t,
						efx_vport_id_t);
	efx_rc_t	(*eeo_vport_assign)(efx_nic_t *, efx_vswitch_id_t,
						efx_vport_id_t, uint32_t);
	efx_rc_t	(*eeo_vport_reconfigure)(efx_nic_t *, efx_vswitch_id_t,
							efx_vport_id_t,
							uint16_t *, uint8_t *,
							boolean_t *);
	efx_rc_t	(*eeo_vport_stats)(efx_nic_t *, efx_vswitch_id_t,
						efx_vport_id_t, efsys_mem_t *);
} efx_evb_ops_t;

LIBEFX_INTERNAL
extern __checkReturn	boolean_t
efx_is_zero_eth_addr(
	__in_bcount(EFX_MAC_ADDR_LEN)	const uint8_t *addrp);

#endif /* EFSYS_OPT_EVB */

#if EFSYS_OPT_MCDI_PROXY_AUTH_SERVER

#define	EFX_PROXY_CONFIGURE_MAGIC	0xAB2015EF


typedef struct efx_proxy_ops_s {
	efx_rc_t	(*epo_init)(efx_nic_t *);
	void		(*epo_fini)(efx_nic_t *);
	efx_rc_t	(*epo_mc_config)(efx_nic_t *, efsys_mem_t *,
					efsys_mem_t *, efsys_mem_t *,
					uint32_t, uint32_t *, size_t);
	efx_rc_t	(*epo_disable)(efx_nic_t *);
	efx_rc_t	(*epo_privilege_modify)(efx_nic_t *, uint32_t, uint32_t,
					uint32_t, uint32_t, uint32_t);
	efx_rc_t	(*epo_set_privilege_mask)(efx_nic_t *, uint32_t,
					uint32_t, uint32_t);
	efx_rc_t	(*epo_complete_request)(efx_nic_t *, uint32_t,
					uint32_t, uint32_t);
	efx_rc_t	(*epo_exec_cmd)(efx_nic_t *, efx_proxy_cmd_params_t *);
	efx_rc_t	(*epo_get_privilege_mask)(efx_nic_t *, uint32_t,
					uint32_t, uint32_t *);
} efx_proxy_ops_t;

#endif /* EFSYS_OPT_MCDI_PROXY_AUTH_SERVER */

#if EFSYS_OPT_MAE

typedef struct efx_mae_field_cap_s {
	uint32_t			emfc_support;
	boolean_t			emfc_mask_affects_class;
	boolean_t			emfc_match_affects_class;
} efx_mae_field_cap_t;

typedef struct efx_mae_s {
	uint32_t			em_max_n_action_prios;
	/*
	 * The number of MAE field IDs recognised by the FW implementation.
	 * Any field ID greater than or equal to this value is unsupported.
	 */
	uint32_t			em_max_nfields;
	/** Action rule match field capabilities. */
	efx_mae_field_cap_t		*em_action_rule_field_caps;
	size_t				em_action_rule_field_caps_size;
	uint32_t			em_max_n_outer_prios;
	uint32_t			em_encap_types_supported;
	/** Outer rule match field capabilities. */
	efx_mae_field_cap_t		*em_outer_rule_field_caps;
	size_t				em_outer_rule_field_caps_size;
	uint32_t			em_max_n_action_counters;
	uint32_t			em_max_n_conntrack_counters;
} efx_mae_t;

#endif /* EFSYS_OPT_MAE */

#define	EFX_DRV_VER_MAX		20

typedef struct efx_drv_cfg_s {
	uint32_t		edc_min_vi_count;
	uint32_t		edc_max_vi_count;

	uint32_t		edc_max_piobuf_count;
	uint32_t		edc_pio_alloc_size;
} efx_drv_cfg_t;

struct efx_nic_s {
	uint32_t		en_magic;
	efx_family_t		en_family;
	uint32_t		en_features;
	efsys_identifier_t	*en_esip;
	efsys_lock_t		*en_eslp;
	efsys_bar_t		*en_esbp;
	unsigned int		en_mod_flags;
	unsigned int		en_reset_flags;
	efx_nic_cfg_t		en_nic_cfg;
	efx_drv_cfg_t		en_drv_cfg;
	efx_port_t		en_port;
	efx_mon_t		en_mon;
	efx_intr_t		en_intr;
	uint32_t		en_ev_qcount;
	uint32_t		en_rx_qcount;
	uint32_t		en_tx_qcount;
	const efx_nic_ops_t	*en_enop;
	const efx_ev_ops_t	*en_eevop;
	const efx_tx_ops_t	*en_etxop;
	const efx_rx_ops_t	*en_erxop;
	efx_fw_variant_t	efv;
	char			en_drv_version[EFX_DRV_VER_MAX];
	efx_nic_dma_t		en_dma;
#if EFSYS_OPT_FILTER
	efx_filter_t		en_filter;
	const efx_filter_ops_t	*en_efop;
#endif	/* EFSYS_OPT_FILTER */
#if EFSYS_OPT_TUNNEL
	efx_tunnel_cfg_t	en_tunnel_cfg;
	const efx_tunnel_ops_t	*en_etop;
#endif /* EFSYS_OPT_TUNNEL */
#if EFSYS_OPT_MCDI
	efx_mcdi_t		en_mcdi;
#endif	/* EFSYS_OPT_MCDI */
#if EFSYS_OPT_NVRAM
	uint32_t		en_nvram_partn_locked;
	const efx_nvram_ops_t	*en_envop;
#endif	/* EFSYS_OPT_NVRAM */
#if EFSYS_OPT_VPD
	const efx_vpd_ops_t	*en_evpdop;
#endif	/* EFSYS_OPT_VPD */
#if EFSYS_OPT_VIRTIO
	const efx_virtio_ops_t	*en_evop;
#endif	/* EFSYS_OPT_VPD */
#if EFSYS_OPT_RX_SCALE
	efx_rx_hash_support_t		en_hash_support;
	efx_rx_scale_context_type_t	en_rss_context_type;
	uint32_t			en_rss_context;
#endif	/* EFSYS_OPT_RX_SCALE */
	uint32_t		en_vport_id;
#if EFSYS_OPT_LICENSING
	const efx_lic_ops_t	*en_elop;
	boolean_t		en_licensing_supported;
#endif
	union {
#if EFSYS_OPT_SIENA
		struct {
#if EFSYS_OPT_NVRAM || EFSYS_OPT_VPD
			unsigned int		enu_partn_mask;
#endif	/* EFSYS_OPT_NVRAM || EFSYS_OPT_VPD */
#if EFSYS_OPT_VPD
			caddr_t			enu_svpd;
			size_t			enu_svpd_length;
#endif	/* EFSYS_OPT_VPD */
			int			enu_unused;
		} siena;
#endif	/* EFSYS_OPT_SIENA */
		int	enu_unused;
	} en_u;
#if EFSYS_OPT_RIVERHEAD || EFX_OPTS_EF10()
	union en_arch {
		struct {
			int			ena_vi_base;
			int			ena_vi_count;
			int			ena_vi_shift;
			uint32_t		ena_fcw_base;
#if EFSYS_OPT_VPD
			caddr_t			ena_svpd;
			size_t			ena_svpd_length;
#endif	/* EFSYS_OPT_VPD */
			efx_piobuf_handle_t	ena_piobuf_handle[EF10_MAX_PIOBUF_NBUFS];
			uint32_t		ena_piobuf_count;
			uint32_t		ena_pio_alloc_map[EF10_MAX_PIOBUF_NBUFS];
			uint32_t		ena_pio_write_vi_base;
			/* Memory BAR mapping regions */
			uint32_t		ena_uc_mem_map_offset;
			size_t			ena_uc_mem_map_size;
			uint32_t		ena_wc_mem_map_offset;
			size_t			ena_wc_mem_map_size;
		} ef10;
	} en_arch;
#endif	/* EFSYS_OPT_RIVERHEAD || EFX_OPTS_EF10() */
#if EFSYS_OPT_EVB
	const efx_evb_ops_t	*en_eeop;
	struct efx_vswitch_s    *en_vswitchp;
#endif	/* EFSYS_OPT_EVB */
#if EFSYS_OPT_MCDI_PROXY_AUTH_SERVER
	const efx_proxy_ops_t	*en_epop;
#endif	/* EFSYS_OPT_MCDI_PROXY_AUTH_SERVER */
#if EFSYS_OPT_MAE
	efx_mae_t		*en_maep;
#endif	/* EFSYS_OPT_MAE */
};

#define	EFX_FAMILY_IS_EF10(_enp) \
	((_enp)->en_family == EFX_FAMILY_MEDFORD2 || \
	 (_enp)->en_family == EFX_FAMILY_MEDFORD || \
	 (_enp)->en_family == EFX_FAMILY_HUNTINGTON)

#define	EFX_FAMILY_IS_EF100(_enp) \
	((_enp)->en_family == EFX_FAMILY_RIVERHEAD)


#define	EFX_NIC_MAGIC	0x02121996

typedef	boolean_t (*efx_ev_handler_t)(efx_evq_t *, efx_qword_t *,
    const efx_ev_callbacks_t *, void *);

#if EFSYS_OPT_EV_EXTENDED_WIDTH
typedef	boolean_t (*efx_ev_ew_handler_t)(efx_evq_t *, efx_xword_t *,
    const efx_ev_callbacks_t *, void *);
#endif /* EFSYS_OPT_EV_EXTENDED_WIDTH */

typedef struct efx_evq_rxq_state_s {
	unsigned int			eers_rx_read_ptr;
	unsigned int			eers_rx_mask;
#if EFSYS_OPT_RX_PACKED_STREAM || EFSYS_OPT_RX_ES_SUPER_BUFFER
	unsigned int			eers_rx_stream_npackets;
	boolean_t			eers_rx_packed_stream;
#endif
#if EFSYS_OPT_RX_PACKED_STREAM
	unsigned int			eers_rx_packed_stream_credits;
#endif
} efx_evq_rxq_state_t;

struct efx_evq_s {
	uint32_t			ee_magic;
	uint32_t			ee_flags;
	efx_nic_t			*ee_enp;
	unsigned int			ee_index;
	unsigned int			ee_mask;
	efsys_mem_t			*ee_esmp;
#if EFSYS_OPT_QSTATS
	uint32_t			ee_stat[EV_NQSTATS];
#endif	/* EFSYS_OPT_QSTATS */

	efx_ev_handler_t		ee_rx;
	efx_ev_handler_t		ee_tx;
	efx_ev_handler_t		ee_driver;
	efx_ev_handler_t		ee_global;
	efx_ev_handler_t		ee_drv_gen;
#if EFSYS_OPT_MCDI
	efx_ev_handler_t		ee_mcdi;
#endif	/* EFSYS_OPT_MCDI */

#if EFSYS_OPT_DESC_PROXY
	efx_ev_ew_handler_t		ee_ew_txq_desc;
	efx_ev_ew_handler_t		ee_ew_virtq_desc;
#endif /* EFSYS_OPT_DESC_PROXY */

	efx_evq_rxq_state_t		ee_rxq_state[EFX_EV_RX_NLABELS];
};

#define	EFX_EVQ_MAGIC	0x08081997

#define	EFX_EVQ_SIENA_TIMER_QUANTUM_NS	6144 /* 768 cycles */

#if EFSYS_OPT_QSTATS
#define	EFX_EV_QSTAT_INCR(_eep, _stat)					\
	do {								\
		(_eep)->ee_stat[_stat]++;				\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)
#else
#define	EFX_EV_QSTAT_INCR(_eep, _stat)
#endif

struct efx_rxq_s {
	uint32_t			er_magic;
	efx_nic_t			*er_enp;
	efx_evq_t			*er_eep;
	unsigned int			er_index;
	unsigned int			er_label;
	unsigned int			er_mask;
	size_t				er_buf_size;
	efsys_mem_t			*er_esmp;
	efx_evq_rxq_state_t		*er_ev_qstate;
	efx_rx_prefix_layout_t		er_prefix_layout;
};

#define	EFX_RXQ_MAGIC	0x15022005

struct efx_txq_s {
	uint32_t			et_magic;
	efx_nic_t			*et_enp;
	unsigned int			et_index;
	unsigned int			et_mask;
	efsys_mem_t			*et_esmp;
#if EFSYS_OPT_HUNTINGTON
	uint32_t			et_pio_bufnum;
	uint32_t			et_pio_blknum;
	uint32_t			et_pio_write_offset;
	uint32_t			et_pio_offset;
	size_t				et_pio_size;
#endif
#if EFSYS_OPT_QSTATS
	uint32_t			et_stat[TX_NQSTATS];
#endif	/* EFSYS_OPT_QSTATS */
};

#define	EFX_TXQ_MAGIC	0x05092005

#define	EFX_MAC_ADDR_COPY(_dst, _src)					\
	do {								\
		(_dst)[0] = (_src)[0];					\
		(_dst)[1] = (_src)[1];					\
		(_dst)[2] = (_src)[2];					\
		(_dst)[3] = (_src)[3];					\
		(_dst)[4] = (_src)[4];					\
		(_dst)[5] = (_src)[5];					\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFX_MAC_BROADCAST_ADDR_SET(_dst)				\
	do {								\
		uint16_t *_d = (uint16_t *)(_dst);			\
		_d[0] = 0xffff;						\
		_d[1] = 0xffff;						\
		_d[2] = 0xffff;						\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#if EFSYS_OPT_CHECK_REG
#define	EFX_CHECK_REG(_enp, _reg)					\
	do {								\
		const char *name = #_reg;				\
		char min = name[4];					\
		char max = name[5];					\
		char rev;						\
									\
		switch ((_enp)->en_family) {				\
		case EFX_FAMILY_SIENA:					\
			rev = 'C';					\
			break;						\
									\
		case EFX_FAMILY_HUNTINGTON:				\
			rev = 'D';					\
			break;						\
									\
		case EFX_FAMILY_MEDFORD:				\
			rev = 'E';					\
			break;						\
									\
		case EFX_FAMILY_MEDFORD2:				\
			rev = 'F';					\
			break;						\
									\
		case EFX_FAMILY_RIVERHEAD:				\
			rev = 'G';					\
			break;						\
									\
		default:						\
			rev = '?';					\
			break;						\
		}							\
									\
		EFSYS_ASSERT3S(rev, >=, min);				\
		EFSYS_ASSERT3S(rev, <=, max);				\
									\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)
#else
#define	EFX_CHECK_REG(_enp, _reg) do {					\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)
#endif

#define	EFX_BAR_READD(_enp, _reg, _edp, _lock)				\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_BAR_READD((_enp)->en_esbp, _reg ## _OFST,		\
		    (_edp), (_lock));					\
		EFSYS_PROBE3(efx_bar_readd, const char *, #_reg,	\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_edp)->ed_u32[0]);			\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFX_BAR_WRITED(_enp, _reg, _edp, _lock)				\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_PROBE3(efx_bar_writed, const char *, #_reg,	\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_edp)->ed_u32[0]);			\
		EFSYS_BAR_WRITED((_enp)->en_esbp, _reg ## _OFST,	\
		    (_edp), (_lock));					\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFX_BAR_READQ(_enp, _reg, _eqp)					\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_BAR_READQ((_enp)->en_esbp, _reg ## _OFST,		\
		    (_eqp));						\
		EFSYS_PROBE4(efx_bar_readq, const char *, #_reg,	\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_eqp)->eq_u32[1],			\
		    uint32_t, (_eqp)->eq_u32[0]);			\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFX_BAR_WRITEQ(_enp, _reg, _eqp)				\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_PROBE4(efx_bar_writeq, const char *, #_reg,	\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_eqp)->eq_u32[1],			\
		    uint32_t, (_eqp)->eq_u32[0]);			\
		EFSYS_BAR_WRITEQ((_enp)->en_esbp, _reg ## _OFST,	\
		    (_eqp));						\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFX_BAR_READO(_enp, _reg, _eop)					\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_BAR_READO((_enp)->en_esbp, _reg ## _OFST,		\
		    (_eop), B_TRUE);					\
		EFSYS_PROBE6(efx_bar_reado, const char *, #_reg,	\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_eop)->eo_u32[3],			\
		    uint32_t, (_eop)->eo_u32[2],			\
		    uint32_t, (_eop)->eo_u32[1],			\
		    uint32_t, (_eop)->eo_u32[0]);			\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFX_BAR_WRITEO(_enp, _reg, _eop)				\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_PROBE6(efx_bar_writeo, const char *, #_reg,	\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_eop)->eo_u32[3],			\
		    uint32_t, (_eop)->eo_u32[2],			\
		    uint32_t, (_eop)->eo_u32[1],			\
		    uint32_t, (_eop)->eo_u32[0]);			\
		EFSYS_BAR_WRITEO((_enp)->en_esbp, _reg ## _OFST,	\
		    (_eop), B_TRUE);					\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

/*
 * Accessors for memory BAR non-VI tables.
 *
 * Code used on EF10 *must* use EFX_BAR_VI_*() macros for per-VI registers,
 * to ensure the correct runtime VI window size is used on Medford2.
 *
 * Code used on EF100 *must* use EFX_BAR_FCW_* macros for function control
 * window registers, to ensure the correct starting offset is used.
 *
 * Siena-only code may continue using EFX_BAR_TBL_*() macros for VI registers.
 */

#define	EFX_BAR_TBL_READD(_enp, _reg, _index, _edp, _lock)		\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_BAR_READD((_enp)->en_esbp,			\
		    (_reg ## _OFST + ((_index) * _reg ## _STEP)),	\
		    (_edp), (_lock));					\
		EFSYS_PROBE4(efx_bar_tbl_readd, const char *, #_reg,	\
		    uint32_t, (_index),					\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_edp)->ed_u32[0]);			\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFX_BAR_TBL_WRITED(_enp, _reg, _index, _edp, _lock)		\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_PROBE4(efx_bar_tbl_writed, const char *, #_reg,	\
		    uint32_t, (_index),					\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_edp)->ed_u32[0]);			\
		EFSYS_BAR_WRITED((_enp)->en_esbp,			\
		    (_reg ## _OFST + ((_index) * _reg ## _STEP)),	\
		    (_edp), (_lock));					\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFX_BAR_TBL_WRITED3(_enp, _reg, _index, _edp, _lock)		\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_PROBE4(efx_bar_tbl_writed, const char *, #_reg,	\
		    uint32_t, (_index),					\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_edp)->ed_u32[0]);			\
		EFSYS_BAR_WRITED((_enp)->en_esbp,			\
		    (_reg ## _OFST +					\
		    (3 * sizeof (efx_dword_t)) +			\
		    ((_index) * _reg ## _STEP)),			\
		    (_edp), (_lock));					\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFX_BAR_TBL_READQ(_enp, _reg, _index, _eqp)			\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_BAR_READQ((_enp)->en_esbp,			\
		    (_reg ## _OFST + ((_index) * _reg ## _STEP)),	\
		    (_eqp));						\
		EFSYS_PROBE5(efx_bar_tbl_readq, const char *, #_reg,	\
		    uint32_t, (_index),					\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_eqp)->eq_u32[1],			\
		    uint32_t, (_eqp)->eq_u32[0]);			\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFX_BAR_TBL_WRITEQ(_enp, _reg, _index, _eqp)			\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_PROBE5(efx_bar_tbl_writeq, const char *, #_reg,	\
		    uint32_t, (_index),					\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_eqp)->eq_u32[1],			\
		    uint32_t, (_eqp)->eq_u32[0]);			\
		EFSYS_BAR_WRITEQ((_enp)->en_esbp,			\
		    (_reg ## _OFST + ((_index) * _reg ## _STEP)),	\
		    (_eqp));						\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFX_BAR_TBL_READO(_enp, _reg, _index, _eop, _lock)		\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_BAR_READO((_enp)->en_esbp,			\
		    (_reg ## _OFST + ((_index) * _reg ## _STEP)),	\
		    (_eop), (_lock));					\
		EFSYS_PROBE7(efx_bar_tbl_reado, const char *, #_reg,	\
		    uint32_t, (_index),					\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_eop)->eo_u32[3],			\
		    uint32_t, (_eop)->eo_u32[2],			\
		    uint32_t, (_eop)->eo_u32[1],			\
		    uint32_t, (_eop)->eo_u32[0]);			\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFX_BAR_TBL_WRITEO(_enp, _reg, _index, _eop, _lock)		\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_PROBE7(efx_bar_tbl_writeo, const char *, #_reg,	\
		    uint32_t, (_index),					\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_eop)->eo_u32[3],			\
		    uint32_t, (_eop)->eo_u32[2],			\
		    uint32_t, (_eop)->eo_u32[1],			\
		    uint32_t, (_eop)->eo_u32[0]);			\
		EFSYS_BAR_WRITEO((_enp)->en_esbp,			\
		    (_reg ## _OFST + ((_index) * _reg ## _STEP)),	\
		    (_eop), (_lock));					\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

/*
 * Accessors for memory BAR function control window registers.
 *
 * The function control window is located at an offset which can be
 * non-zero in case of Riverhead.
 */

#if EFSYS_OPT_RIVERHEAD

#define	EFX_BAR_FCW_READD(_enp, _reg, _edp)				\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_BAR_READD((_enp)->en_esbp, _reg ## _OFST +	\
		    (_enp)->en_arch.ef10.ena_fcw_base,			\
		    (_edp), B_FALSE);					\
		EFSYS_PROBE3(efx_bar_fcw_readd, const char *, #_reg,	\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_edp)->ed_u32[0]);			\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFX_BAR_FCW_WRITED(_enp, _reg, _edp)				\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_PROBE3(efx_bar_fcw_writed, const char *, #_reg,	\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_edp)->ed_u32[0]);			\
		EFSYS_BAR_WRITED((_enp)->en_esbp, _reg ## _OFST +	\
		    (_enp)->en_arch.ef10.ena_fcw_base,			\
		    (_edp), B_FALSE);					\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#endif	/* EFSYS_OPT_RIVERHEAD */

/*
 * Accessors for memory BAR per-VI registers.
 *
 * The VI window size is 8KB for Medford and all earlier controllers.
 * For Medford2, the VI window size can be 8KB, 16KB or 64KB.
 */

#define	EFX_BAR_VI_READD(_enp, _reg, _index, _edp, _lock)		\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_BAR_READD((_enp)->en_esbp,			\
		    ((_reg ## _OFST) +					\
		    ((_index) << (_enp)->en_nic_cfg.enc_vi_window_shift)), \
		    (_edp), (_lock));					\
		EFSYS_PROBE4(efx_bar_vi_readd, const char *, #_reg,	\
		    uint32_t, (_index),					\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_edp)->ed_u32[0]);			\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFX_BAR_VI_WRITED(_enp, _reg, _index, _edp, _lock)		\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_PROBE4(efx_bar_vi_writed, const char *, #_reg,	\
		    uint32_t, (_index),					\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_edp)->ed_u32[0]);			\
		EFSYS_BAR_WRITED((_enp)->en_esbp,			\
		    ((_reg ## _OFST) +					\
		    ((_index) << (_enp)->en_nic_cfg.enc_vi_window_shift)), \
		    (_edp), (_lock));					\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFX_BAR_VI_WRITED2(_enp, _reg, _index, _edp, _lock)		\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_PROBE4(efx_bar_vi_writed, const char *, #_reg,	\
		    uint32_t, (_index),					\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_edp)->ed_u32[0]);			\
		EFSYS_BAR_WRITED((_enp)->en_esbp,			\
		    ((_reg ## _OFST) +					\
		    (2 * sizeof (efx_dword_t)) +			\
		    ((_index) << (_enp)->en_nic_cfg.enc_vi_window_shift)), \
		    (_edp), (_lock));					\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

/*
 * Allow drivers to perform optimised 128-bit VI doorbell writes.
 * The DMA descriptor pointers (RX_DESC_UPD and TX_DESC_UPD) are
 * special-cased in the BIU on the Falcon/Siena and EF10 architectures to avoid
 * the need for locking in the host, and are the only ones known to be safe to
 * use 128-bites write with.
 */
#define	EFX_BAR_VI_DOORBELL_WRITEO(_enp, _reg, _index, _eop)		\
	do {								\
		EFX_CHECK_REG((_enp), (_reg));				\
		EFSYS_PROBE7(efx_bar_vi_doorbell_writeo,		\
		    const char *, #_reg,				\
		    uint32_t, (_index),					\
		    uint32_t, _reg ## _OFST,				\
		    uint32_t, (_eop)->eo_u32[3],			\
		    uint32_t, (_eop)->eo_u32[2],			\
		    uint32_t, (_eop)->eo_u32[1],			\
		    uint32_t, (_eop)->eo_u32[0]);			\
		EFSYS_BAR_DOORBELL_WRITEO((_enp)->en_esbp,		\
		    (_reg ## _OFST +					\
		    ((_index) << (_enp)->en_nic_cfg.enc_vi_window_shift)), \
		    (_eop));						\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFX_DMA_SYNC_QUEUE_FOR_DEVICE(_esmp, _entries, _desc_size,	\
				      _wptr, _owptr)			\
	do {								\
		unsigned int _new = (_wptr);				\
		unsigned int _old = (_owptr);				\
									\
		if ((_new) >= (_old))					\
			EFSYS_DMA_SYNC_FOR_DEVICE((_esmp),		\
			    (_old) * (_desc_size),			\
			    ((_new) - (_old)) * (_desc_size));		\
		else							\
			/*						\
			 * It is cheaper to sync entire map than sync	\
			 * two parts especially when offset/size are	\
			 * ignored and entire map is synced in any case.\
			 */						\
			EFSYS_DMA_SYNC_FOR_DEVICE((_esmp),		\
			    0,						\
			    (_entries) * (_desc_size));			\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
efx_mac_select(
	__in		efx_nic_t *enp);

LIBEFX_INTERNAL
extern	void
efx_mac_multicast_hash_compute(
	__in_ecount(6*count)		uint8_t const *addrs,
	__in				int count,
	__out				efx_oword_t *hash_low,
	__out				efx_oword_t *hash_high);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
efx_phy_probe(
	__in		efx_nic_t *enp);

LIBEFX_INTERNAL
extern			void
efx_phy_unprobe(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_VPD

/* VPD utility functions */

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_vpd_hunk_length(
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__out			size_t *lengthp);

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_vpd_hunk_verify(
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__out_opt		boolean_t *cksummedp);

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_vpd_hunk_reinit(
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__in			boolean_t wantpid);

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_vpd_hunk_get(
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__in			efx_vpd_tag_t tag,
	__in			efx_vpd_keyword_t keyword,
	__out			unsigned int *payloadp,
	__out			uint8_t *paylenp);

LIBEFX_INTERNAL
extern	__checkReturn			efx_rc_t
efx_vpd_hunk_next(
	__in_bcount(size)		caddr_t data,
	__in				size_t size,
	__out				efx_vpd_tag_t *tagp,
	__out				efx_vpd_keyword_t *keyword,
	__out_opt			unsigned int *payloadp,
	__out_opt			uint8_t *paylenp,
	__inout				unsigned int *contp);

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_vpd_hunk_set(
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__in			efx_vpd_value_t *evvp);

#endif	/* EFSYS_OPT_VPD */

#if EFSYS_OPT_MCDI

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_mcdi_set_workaround(
	__in			efx_nic_t *enp,
	__in			uint32_t type,
	__in			boolean_t enabled,
	__out_opt		uint32_t *flagsp);

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_mcdi_get_workarounds(
	__in			efx_nic_t *enp,
	__out_opt		uint32_t *implementedp,
	__out_opt		uint32_t *enabledp);

#if EFSYS_OPT_RIVERHEAD || EFX_OPTS_EF10()

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_mcdi_intf_from_pcie(
	__in			uint32_t pcie_intf,
	__out			efx_pcie_interface_t *efx_intf);

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_mcdi_intf_to_pcie(
	__in			efx_pcie_interface_t efx_intf,
	__out			uint32_t *pcie_intf);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
efx_mcdi_init_evq(
	__in		efx_nic_t *enp,
	__in		unsigned int instance,
	__in		efsys_mem_t *esmp,
	__in		size_t nevs,
	__in		uint32_t irq,
	__in		uint32_t target_evq,
	__in		uint32_t us,
	__in		uint32_t flags,
	__in		boolean_t low_latency);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
efx_mcdi_fini_evq(
	__in		efx_nic_t *enp,
	__in		uint32_t instance);

typedef struct efx_mcdi_init_rxq_params_s {
	boolean_t	disable_scatter;
	boolean_t	want_inner_classes;
	uint32_t	buf_size;
	uint32_t	ps_buf_size;
	uint32_t	es_bufs_per_desc;
	uint32_t	es_max_dma_len;
	uint32_t	es_buf_stride;
	uint32_t	hol_block_timeout;
	uint32_t	prefix_id;
} efx_mcdi_init_rxq_params_t;

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
efx_mcdi_init_rxq(
	__in		efx_nic_t *enp,
	__in		uint32_t ndescs,
	__in		efx_evq_t *eep,
	__in		uint32_t label,
	__in		uint32_t instance,
	__in		efsys_mem_t *esmp,
	__in		const efx_mcdi_init_rxq_params_t *params);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
efx_mcdi_fini_rxq(
	__in		efx_nic_t *enp,
	__in		uint32_t instance);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
efx_mcdi_init_txq(
	__in		efx_nic_t *enp,
	__in		uint32_t ndescs,
	__in		uint32_t target_evq,
	__in		uint32_t label,
	__in		uint32_t instance,
	__in		uint16_t flags,
	__in		efsys_mem_t *esmp);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
efx_mcdi_fini_txq(
	__in		efx_nic_t *enp,
	__in		uint32_t instance);

#endif	/* EFSYS_OPT_RIVERHEAD || EFX_OPTS_EF10() */

#endif /* EFSYS_OPT_MCDI */

#if EFSYS_OPT_MAC_STATS

/*
 * Closed range of stats (i.e. the first and the last are included).
 * The last must be greater or equal (if the range is one item only) to
 * the first.
 */
struct efx_mac_stats_range {
	efx_mac_stat_t		first;
	efx_mac_stat_t		last;
};

typedef enum efx_stats_action_e {
	EFX_STATS_CLEAR,
	EFX_STATS_UPLOAD,
	EFX_STATS_ENABLE_NOEVENTS,
	EFX_STATS_ENABLE_EVENTS,
	EFX_STATS_DISABLE,
} efx_stats_action_t;

LIBEFX_INTERNAL
extern					efx_rc_t
efx_mac_stats_mask_add_ranges(
	__inout_bcount(mask_size)	uint32_t *maskp,
	__in				size_t mask_size,
	__in_ecount(rng_count)		const struct efx_mac_stats_range *rngp,
	__in				unsigned int rng_count);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
efx_mcdi_mac_stats(
	__in		efx_nic_t *enp,
	__in		uint32_t vport_id,
	__in_opt	efsys_mem_t *esmp,
	__in		efx_stats_action_t action,
	__in		uint16_t period_ms);

#endif	/* EFSYS_OPT_MAC_STATS */

#if EFSYS_OPT_PCI

/*
 * Find the next extended capability in a PCI device's config space
 * with specified capability id.
 * Passing 0 offset makes the function search from the start.
 * If search succeeds, found capability is in modified offset.
 *
 * Returns ENOENT if a capability is not found.
 */
LIBEFX_INTERNAL
extern	__checkReturn			efx_rc_t
efx_pci_config_find_next_ext_cap(
	__in				efsys_pci_config_t *espcp,
	__in				const efx_pci_ops_t *epop,
	__in				uint16_t cap_id,
	__inout				size_t *offsetp);

/*
 * Get the next extended capability in a PCI device's config space.
 * Passing 0 offset makes the function get the first capability.
 * If search succeeds, the capability is in modified offset.
 *
 * Returns ENOENT if there is no next capability.
 */
LIBEFX_INTERNAL
extern	__checkReturn			efx_rc_t
efx_pci_config_next_ext_cap(
	__in				efsys_pci_config_t *espcp,
	__in				const efx_pci_ops_t *epop,
	__inout				size_t *offsetp);

/*
 * Find the next Xilinx capabilities table location by searching
 * PCI extended capabilities.
 *
 * Returns ENOENT if a table location is not found.
 */
LIBEFX_INTERNAL
extern	__checkReturn			efx_rc_t
efx_pci_find_next_xilinx_cap_table(
	__in				efsys_pci_config_t *espcp,
	__in				const efx_pci_ops_t *epop,
	__inout				size_t *pci_cap_offsetp,
	__out				unsigned int *xilinx_tbl_barp,
	__out				efsys_dma_addr_t *xilinx_tbl_offsetp);

/*
 * Read a Xilinx extended PCI capability that gives the location
 * of a Xilinx capabilities table.
 *
 * Returns ENOENT if the extended PCI capability does not contain
 * Xilinx capabilities table locator.
 */
LIBEFX_INTERNAL
extern	__checkReturn			efx_rc_t
efx_pci_read_ext_cap_xilinx_table(
	__in				efsys_pci_config_t *espcp,
	__in				const efx_pci_ops_t *epop,
	__in				size_t cap_offset,
	__out				unsigned int *barp,
	__out				efsys_dma_addr_t *offsetp);

/*
 * Find a capability with specified format_id in a Xilinx capabilities table.
 * Searching is started from provided offset, taking skip_first into account.
 * If search succeeds, found capability is in modified offset.
 *
 * Returns ENOENT if an entry with specified format id is not found.
 */
LIBEFX_INTERNAL
extern	__checkReturn			efx_rc_t
efx_pci_xilinx_cap_tbl_find(
	__in				efsys_bar_t *esbp,
	__in				uint32_t format_id,
	__in				boolean_t skip_first,
	__inout				efsys_dma_addr_t *entry_offsetp);

#endif /* EFSYS_OPT_PCI */

#if EFSYS_OPT_MAE

struct efx_mae_match_spec_s {
	efx_mae_rule_type_t		emms_type;
	uint32_t			emms_prio;
	union emms_mask_value_pairs {
		uint8_t			action[
					    MAE_FIELD_MASK_VALUE_PAIRS_V2_LEN];
		uint8_t			outer[MAE_ENC_FIELD_PAIRS_LEN];
	} emms_mask_value_pairs;
	uint8_t				emms_outer_rule_recirc_id;
	boolean_t			emms_outer_rule_do_ct;
};

typedef enum efx_mae_action_e {
	/* These actions are strictly ordered. */
	EFX_MAE_ACTION_DECAP,
	EFX_MAE_ACTION_VLAN_POP,
	EFX_MAE_ACTION_SET_DST_MAC,
	EFX_MAE_ACTION_SET_SRC_MAC,
	EFX_MAE_ACTION_DECR_IP_TTL,
	EFX_MAE_ACTION_NAT,
	EFX_MAE_ACTION_VLAN_PUSH,
	EFX_MAE_ACTION_COUNT,
	EFX_MAE_ACTION_ENCAP,

	/*
	 * These actions are not strictly ordered and can
	 * be passed by a client in any order (before DELIVER).
	 * However, these enumerants must be kept compactly
	 * in the end of the enumeration (before DELIVER).
	 */
	EFX_MAE_ACTION_FLAG,
	EFX_MAE_ACTION_MARK,

	/* DELIVER is always the last action. */
	EFX_MAE_ACTION_DELIVER,

	EFX_MAE_NACTIONS
} efx_mae_action_t;

/* MAE VLAN_POP action can handle 1 or 2 tags. */
#define	EFX_MAE_VLAN_POP_MAX_NTAGS	(2)

/* MAE VLAN_PUSH action can handle 1 or 2 tags. */
#define	EFX_MAE_VLAN_PUSH_MAX_NTAGS	(2)

typedef struct efx_mae_action_vlan_push_s {
	uint16_t			emavp_tpid_be;
	uint16_t			emavp_tci_be;
} efx_mae_action_vlan_push_t;

/*
 * Helper efx_mae_action_set_clear_fw_rsrc_ids() is responsible
 * to initialise every field in this structure to INVALID value.
 */
typedef struct efx_mae_actions_rsrc_s {
	efx_mae_mac_id_t		emar_dst_mac_id;
	efx_mae_mac_id_t		emar_src_mac_id;
	efx_mae_eh_id_t			emar_eh_id;
	efx_counter_t			emar_counter_id;
} efx_mae_actions_rsrc_t;

struct efx_mae_actions_s {
	/* Bitmap of actions in spec, indexed by action type */
	uint32_t			ema_actions;

	unsigned int			ema_n_vlan_tags_to_pop;
	unsigned int			ema_n_vlan_tags_to_push;
	efx_mae_action_vlan_push_t	ema_vlan_push_descs[
	    EFX_MAE_VLAN_PUSH_MAX_NTAGS];
	unsigned int			ema_n_count_actions;
	uint32_t			ema_mark_value;
	efx_mport_sel_t			ema_deliver_mport;

	/*
	 * Always keep this at the end of the struct since
	 * efx_mae_action_set_specs_equal() relies on that
	 * to make sure that resource IDs are not compared.
	 */
	efx_mae_actions_rsrc_t		ema_rsrc;

	/*
	 * A copy of encp->enc_mae_aset_v2_supported.
	 * It is set by efx_mae_action_set_spec_init().
	 * This value is ignored on spec comparisons.
	 */
	boolean_t			ema_v2_is_supported;
};

#endif /* EFSYS_OPT_MAE */

#if EFSYS_OPT_VIRTIO

#define	EFX_VQ_MAGIC	0x026011950

typedef enum efx_virtio_vq_state_e {
	EFX_VIRTIO_VQ_STATE_UNKNOWN = 0,
	EFX_VIRTIO_VQ_STATE_INITIALIZED,
	EFX_VIRTIO_VQ_STATE_STARTED,
	EFX_VIRTIO_VQ_NSTATES
} efx_virtio_vq_state_t;

struct efx_virtio_vq_s {
	uint32_t		evv_magic;
	efx_nic_t		*evv_enp;
	efx_virtio_vq_state_t	evv_state;
	uint32_t		evv_vi_index;
	efx_virtio_vq_type_t	evv_type;
	uint16_t		evv_target_vf;
};

#endif /* EFSYS_OPT_VIRTIO */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_EFX_IMPL_H */
