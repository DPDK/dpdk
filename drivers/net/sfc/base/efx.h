/*
 * Copyright (c) 2006-2016 Solarflare Communications Inc.
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

#ifndef	_SYS_EFX_H
#define	_SYS_EFX_H

#include "efsys.h"
#include "efx_check.h"
#include "efx_phy_ids.h"

#ifdef	__cplusplus
extern "C" {
#endif

#define	EFX_STATIC_ASSERT(_cond)		\
	((void)sizeof(char[(_cond) ? 1 : -1]))

#define	EFX_ARRAY_SIZE(_array)			\
	(sizeof(_array) / sizeof((_array)[0]))

#define	EFX_FIELD_OFFSET(_type, _field)		\
	((size_t) &(((_type *)0)->_field))

/* Return codes */

typedef __success(return == 0) int efx_rc_t;


/* Chip families */

typedef enum efx_family_e {
	EFX_FAMILY_INVALID,
	EFX_FAMILY_FALCON,	/* Obsolete and not supported */
	EFX_FAMILY_SIENA,
	EFX_FAMILY_HUNTINGTON,
	EFX_FAMILY_MEDFORD,
	EFX_FAMILY_NTYPES
} efx_family_t;

extern	__checkReturn	efx_rc_t
efx_family(
	__in		uint16_t venid,
	__in		uint16_t devid,
	__out		efx_family_t *efp);


#define	EFX_PCI_VENID_SFC			0x1924

#define	EFX_PCI_DEVID_FALCON			0x0710	/* SFC4000 */

#define	EFX_PCI_DEVID_BETHPAGE			0x0803	/* SFC9020 */
#define	EFX_PCI_DEVID_SIENA			0x0813	/* SFL9021 */
#define	EFX_PCI_DEVID_SIENA_F1_UNINIT		0x0810

#define	EFX_PCI_DEVID_HUNTINGTON_PF_UNINIT	0x0901
#define	EFX_PCI_DEVID_FARMINGDALE		0x0903	/* SFC9120 PF */
#define	EFX_PCI_DEVID_GREENPORT			0x0923	/* SFC9140 PF */

#define	EFX_PCI_DEVID_FARMINGDALE_VF		0x1903	/* SFC9120 VF */
#define	EFX_PCI_DEVID_GREENPORT_VF		0x1923	/* SFC9140 VF */

#define	EFX_PCI_DEVID_MEDFORD_PF_UNINIT		0x0913
#define	EFX_PCI_DEVID_MEDFORD			0x0A03	/* SFC9240 PF */
#define	EFX_PCI_DEVID_MEDFORD_VF		0x1A03	/* SFC9240 VF */

#define	EFX_MEM_BAR	2

/* Error codes */

enum {
	EFX_ERR_INVALID,
	EFX_ERR_SRAM_OOB,
	EFX_ERR_BUFID_DC_OOB,
	EFX_ERR_MEM_PERR,
	EFX_ERR_RBUF_OWN,
	EFX_ERR_TBUF_OWN,
	EFX_ERR_RDESQ_OWN,
	EFX_ERR_TDESQ_OWN,
	EFX_ERR_EVQ_OWN,
	EFX_ERR_EVFF_OFLO,
	EFX_ERR_ILL_ADDR,
	EFX_ERR_SRAM_PERR,
	EFX_ERR_NCODES
};

/* Calculate the IEEE 802.3 CRC32 of a MAC addr */
extern	__checkReturn		uint32_t
efx_crc32_calculate(
	__in			uint32_t crc_init,
	__in_ecount(length)	uint8_t const *input,
	__in			int length);


/* Type prototypes */

typedef struct efx_rxq_s	efx_rxq_t;

/* NIC */

typedef struct efx_nic_s	efx_nic_t;

extern	__checkReturn	efx_rc_t
efx_nic_create(
	__in		efx_family_t family,
	__in		efsys_identifier_t *esip,
	__in		efsys_bar_t *esbp,
	__in		efsys_lock_t *eslp,
	__deref_out	efx_nic_t **enpp);

extern	__checkReturn	efx_rc_t
efx_nic_probe(
	__in		efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
efx_nic_init(
	__in		efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
efx_nic_reset(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_DIAG

extern	__checkReturn	efx_rc_t
efx_nic_register_test(
	__in		efx_nic_t *enp);

#endif	/* EFSYS_OPT_DIAG */

extern		void
efx_nic_fini(
	__in		efx_nic_t *enp);

extern		void
efx_nic_unprobe(
	__in		efx_nic_t *enp);

extern		void
efx_nic_destroy(
	__in	efx_nic_t *enp);

#define	EFX_PCIE_LINK_SPEED_GEN1		1
#define	EFX_PCIE_LINK_SPEED_GEN2		2
#define	EFX_PCIE_LINK_SPEED_GEN3		3

typedef enum efx_pcie_link_performance_e {
	EFX_PCIE_LINK_PERFORMANCE_UNKNOWN_BANDWIDTH,
	EFX_PCIE_LINK_PERFORMANCE_SUBOPTIMAL_BANDWIDTH,
	EFX_PCIE_LINK_PERFORMANCE_SUBOPTIMAL_LATENCY,
	EFX_PCIE_LINK_PERFORMANCE_OPTIMAL
} efx_pcie_link_performance_t;

extern	__checkReturn	efx_rc_t
efx_nic_calculate_pcie_link_bandwidth(
	__in		uint32_t pcie_link_width,
	__in		uint32_t pcie_link_gen,
	__out		uint32_t *bandwidth_mbpsp);

extern	__checkReturn	efx_rc_t
efx_nic_check_pcie_link_speed(
	__in		efx_nic_t *enp,
	__in		uint32_t pcie_link_width,
	__in		uint32_t pcie_link_gen,
	__out		efx_pcie_link_performance_t *resultp);

#if EFSYS_OPT_MCDI

#if EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD
/* Huntington and Medford require MCDIv2 commands */
#define	WITH_MCDI_V2 1
#endif

typedef struct efx_mcdi_req_s efx_mcdi_req_t;

typedef enum efx_mcdi_exception_e {
	EFX_MCDI_EXCEPTION_MC_REBOOT,
	EFX_MCDI_EXCEPTION_MC_BADASSERT,
} efx_mcdi_exception_t;

#if EFSYS_OPT_MCDI_LOGGING
typedef enum efx_log_msg_e {
	EFX_LOG_INVALID,
	EFX_LOG_MCDI_REQUEST,
	EFX_LOG_MCDI_RESPONSE,
} efx_log_msg_t;
#endif /* EFSYS_OPT_MCDI_LOGGING */

typedef struct efx_mcdi_transport_s {
	void		*emt_context;
	efsys_mem_t	*emt_dma_mem;
	void		(*emt_execute)(void *, efx_mcdi_req_t *);
	void		(*emt_ev_cpl)(void *);
	void		(*emt_exception)(void *, efx_mcdi_exception_t);
#if EFSYS_OPT_MCDI_LOGGING
	void		(*emt_logger)(void *, efx_log_msg_t,
					void *, size_t, void *, size_t);
#endif /* EFSYS_OPT_MCDI_LOGGING */
#if EFSYS_OPT_MCDI_PROXY_AUTH
	void		(*emt_ev_proxy_response)(void *, uint32_t, efx_rc_t);
#endif /* EFSYS_OPT_MCDI_PROXY_AUTH */
} efx_mcdi_transport_t;

extern	__checkReturn	efx_rc_t
efx_mcdi_init(
	__in		efx_nic_t *enp,
	__in		const efx_mcdi_transport_t *mtp);

extern	__checkReturn	efx_rc_t
efx_mcdi_reboot(
	__in		efx_nic_t *enp);

			void
efx_mcdi_new_epoch(
	__in		efx_nic_t *enp);

extern			void
efx_mcdi_get_timeout(
	__in		efx_nic_t *enp,
	__in		efx_mcdi_req_t *emrp,
	__out		uint32_t *usec_timeoutp);

extern			void
efx_mcdi_request_start(
	__in		efx_nic_t *enp,
	__in		efx_mcdi_req_t *emrp,
	__in		boolean_t ev_cpl);

extern	__checkReturn	boolean_t
efx_mcdi_request_poll(
	__in		efx_nic_t *enp);

extern	__checkReturn	boolean_t
efx_mcdi_request_abort(
	__in		efx_nic_t *enp);

extern			void
efx_mcdi_fini(
	__in		efx_nic_t *enp);

#endif	/* EFSYS_OPT_MCDI */

/* INTR */

#define	EFX_NINTR_SIENA 1024

typedef enum efx_intr_type_e {
	EFX_INTR_INVALID = 0,
	EFX_INTR_LINE,
	EFX_INTR_MESSAGE,
	EFX_INTR_NTYPES
} efx_intr_type_t;

#define	EFX_INTR_SIZE	(sizeof (efx_oword_t))

extern	__checkReturn	efx_rc_t
efx_intr_init(
	__in		efx_nic_t *enp,
	__in		efx_intr_type_t type,
	__in		efsys_mem_t *esmp);

extern			void
efx_intr_enable(
	__in		efx_nic_t *enp);

extern			void
efx_intr_disable(
	__in		efx_nic_t *enp);

extern			void
efx_intr_disable_unlocked(
	__in		efx_nic_t *enp);

#define	EFX_INTR_NEVQS	32

extern	__checkReturn	efx_rc_t
efx_intr_trigger(
	__in		efx_nic_t *enp,
	__in		unsigned int level);

extern			void
efx_intr_status_line(
	__in		efx_nic_t *enp,
	__out		boolean_t *fatalp,
	__out		uint32_t *maskp);

extern			void
efx_intr_status_message(
	__in		efx_nic_t *enp,
	__in		unsigned int message,
	__out		boolean_t *fatalp);

extern			void
efx_intr_fatal(
	__in		efx_nic_t *enp);

extern			void
efx_intr_fini(
	__in		efx_nic_t *enp);

/* MAC */

#if EFSYS_OPT_MAC_STATS

/* START MKCONFIG GENERATED EfxHeaderMacBlock e323546097fd7c65 */
typedef enum efx_mac_stat_e {
	EFX_MAC_RX_OCTETS,
	EFX_MAC_RX_PKTS,
	EFX_MAC_RX_UNICST_PKTS,
	EFX_MAC_RX_MULTICST_PKTS,
	EFX_MAC_RX_BRDCST_PKTS,
	EFX_MAC_RX_PAUSE_PKTS,
	EFX_MAC_RX_LE_64_PKTS,
	EFX_MAC_RX_65_TO_127_PKTS,
	EFX_MAC_RX_128_TO_255_PKTS,
	EFX_MAC_RX_256_TO_511_PKTS,
	EFX_MAC_RX_512_TO_1023_PKTS,
	EFX_MAC_RX_1024_TO_15XX_PKTS,
	EFX_MAC_RX_GE_15XX_PKTS,
	EFX_MAC_RX_ERRORS,
	EFX_MAC_RX_FCS_ERRORS,
	EFX_MAC_RX_DROP_EVENTS,
	EFX_MAC_RX_FALSE_CARRIER_ERRORS,
	EFX_MAC_RX_SYMBOL_ERRORS,
	EFX_MAC_RX_ALIGN_ERRORS,
	EFX_MAC_RX_INTERNAL_ERRORS,
	EFX_MAC_RX_JABBER_PKTS,
	EFX_MAC_RX_LANE0_CHAR_ERR,
	EFX_MAC_RX_LANE1_CHAR_ERR,
	EFX_MAC_RX_LANE2_CHAR_ERR,
	EFX_MAC_RX_LANE3_CHAR_ERR,
	EFX_MAC_RX_LANE0_DISP_ERR,
	EFX_MAC_RX_LANE1_DISP_ERR,
	EFX_MAC_RX_LANE2_DISP_ERR,
	EFX_MAC_RX_LANE3_DISP_ERR,
	EFX_MAC_RX_MATCH_FAULT,
	EFX_MAC_RX_NODESC_DROP_CNT,
	EFX_MAC_TX_OCTETS,
	EFX_MAC_TX_PKTS,
	EFX_MAC_TX_UNICST_PKTS,
	EFX_MAC_TX_MULTICST_PKTS,
	EFX_MAC_TX_BRDCST_PKTS,
	EFX_MAC_TX_PAUSE_PKTS,
	EFX_MAC_TX_LE_64_PKTS,
	EFX_MAC_TX_65_TO_127_PKTS,
	EFX_MAC_TX_128_TO_255_PKTS,
	EFX_MAC_TX_256_TO_511_PKTS,
	EFX_MAC_TX_512_TO_1023_PKTS,
	EFX_MAC_TX_1024_TO_15XX_PKTS,
	EFX_MAC_TX_GE_15XX_PKTS,
	EFX_MAC_TX_ERRORS,
	EFX_MAC_TX_SGL_COL_PKTS,
	EFX_MAC_TX_MULT_COL_PKTS,
	EFX_MAC_TX_EX_COL_PKTS,
	EFX_MAC_TX_LATE_COL_PKTS,
	EFX_MAC_TX_DEF_PKTS,
	EFX_MAC_TX_EX_DEF_PKTS,
	EFX_MAC_PM_TRUNC_BB_OVERFLOW,
	EFX_MAC_PM_DISCARD_BB_OVERFLOW,
	EFX_MAC_PM_TRUNC_VFIFO_FULL,
	EFX_MAC_PM_DISCARD_VFIFO_FULL,
	EFX_MAC_PM_TRUNC_QBB,
	EFX_MAC_PM_DISCARD_QBB,
	EFX_MAC_PM_DISCARD_MAPPING,
	EFX_MAC_RXDP_Q_DISABLED_PKTS,
	EFX_MAC_RXDP_DI_DROPPED_PKTS,
	EFX_MAC_RXDP_STREAMING_PKTS,
	EFX_MAC_RXDP_HLB_FETCH,
	EFX_MAC_RXDP_HLB_WAIT,
	EFX_MAC_VADAPTER_RX_UNICAST_PACKETS,
	EFX_MAC_VADAPTER_RX_UNICAST_BYTES,
	EFX_MAC_VADAPTER_RX_MULTICAST_PACKETS,
	EFX_MAC_VADAPTER_RX_MULTICAST_BYTES,
	EFX_MAC_VADAPTER_RX_BROADCAST_PACKETS,
	EFX_MAC_VADAPTER_RX_BROADCAST_BYTES,
	EFX_MAC_VADAPTER_RX_BAD_PACKETS,
	EFX_MAC_VADAPTER_RX_BAD_BYTES,
	EFX_MAC_VADAPTER_RX_OVERFLOW,
	EFX_MAC_VADAPTER_TX_UNICAST_PACKETS,
	EFX_MAC_VADAPTER_TX_UNICAST_BYTES,
	EFX_MAC_VADAPTER_TX_MULTICAST_PACKETS,
	EFX_MAC_VADAPTER_TX_MULTICAST_BYTES,
	EFX_MAC_VADAPTER_TX_BROADCAST_PACKETS,
	EFX_MAC_VADAPTER_TX_BROADCAST_BYTES,
	EFX_MAC_VADAPTER_TX_BAD_PACKETS,
	EFX_MAC_VADAPTER_TX_BAD_BYTES,
	EFX_MAC_VADAPTER_TX_OVERFLOW,
	EFX_MAC_NSTATS
} efx_mac_stat_t;

/* END MKCONFIG GENERATED EfxHeaderMacBlock */

#endif	/* EFSYS_OPT_MAC_STATS */

typedef enum efx_link_mode_e {
	EFX_LINK_UNKNOWN = 0,
	EFX_LINK_DOWN,
	EFX_LINK_10HDX,
	EFX_LINK_10FDX,
	EFX_LINK_100HDX,
	EFX_LINK_100FDX,
	EFX_LINK_1000HDX,
	EFX_LINK_1000FDX,
	EFX_LINK_10000FDX,
	EFX_LINK_40000FDX,
	EFX_LINK_NMODES
} efx_link_mode_t;

#define	EFX_MAC_ADDR_LEN 6

#define	EFX_MAC_ADDR_IS_MULTICAST(_address) (((uint8_t *)_address)[0] & 0x01)

#define	EFX_MAC_MULTICAST_LIST_MAX	256

#define	EFX_MAC_SDU_MAX	9202

#define	EFX_MAC_PDU_ADJUSTMENT					\
	(/* EtherII */ 14					\
	    + /* VLAN */ 4					\
	    + /* CRC */ 4					\
	    + /* bug16011 */ 16)				\

#define	EFX_MAC_PDU(_sdu)					\
	P2ROUNDUP((_sdu) + EFX_MAC_PDU_ADJUSTMENT, 8)

/*
 * Due to the P2ROUNDUP in EFX_MAC_PDU(), EFX_MAC_SDU_FROM_PDU() may give
 * the SDU rounded up slightly.
 */
#define	EFX_MAC_SDU_FROM_PDU(_pdu)	((_pdu) - EFX_MAC_PDU_ADJUSTMENT)

#define	EFX_MAC_PDU_MIN	60
#define	EFX_MAC_PDU_MAX	EFX_MAC_PDU(EFX_MAC_SDU_MAX)

extern	__checkReturn	efx_rc_t
efx_mac_pdu_get(
	__in		efx_nic_t *enp,
	__out		size_t *pdu);

extern	__checkReturn	efx_rc_t
efx_mac_pdu_set(
	__in		efx_nic_t *enp,
	__in		size_t pdu);

extern	__checkReturn	efx_rc_t
efx_mac_addr_set(
	__in		efx_nic_t *enp,
	__in		uint8_t *addr);

extern	__checkReturn			efx_rc_t
efx_mac_filter_set(
	__in				efx_nic_t *enp,
	__in				boolean_t all_unicst,
	__in				boolean_t mulcst,
	__in				boolean_t all_mulcst,
	__in				boolean_t brdcst);

extern	__checkReturn	efx_rc_t
efx_mac_multicast_list_set(
	__in				efx_nic_t *enp,
	__in_ecount(6*count)		uint8_t const *addrs,
	__in				int count);

extern	__checkReturn	efx_rc_t
efx_mac_filter_default_rxq_set(
	__in		efx_nic_t *enp,
	__in		efx_rxq_t *erp,
	__in		boolean_t using_rss);

extern			void
efx_mac_filter_default_rxq_clear(
	__in		efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
efx_mac_drain(
	__in		efx_nic_t *enp,
	__in		boolean_t enabled);

extern	__checkReturn	efx_rc_t
efx_mac_up(
	__in		efx_nic_t *enp,
	__out		boolean_t *mac_upp);

#define	EFX_FCNTL_RESPOND	0x00000001
#define	EFX_FCNTL_GENERATE	0x00000002

extern	__checkReturn	efx_rc_t
efx_mac_fcntl_set(
	__in		efx_nic_t *enp,
	__in		unsigned int fcntl,
	__in		boolean_t autoneg);

extern			void
efx_mac_fcntl_get(
	__in		efx_nic_t *enp,
	__out		unsigned int *fcntl_wantedp,
	__out		unsigned int *fcntl_linkp);


#if EFSYS_OPT_MAC_STATS

#if EFSYS_OPT_NAMES

extern	__checkReturn			const char *
efx_mac_stat_name(
	__in				efx_nic_t *enp,
	__in				unsigned int id);

#endif	/* EFSYS_OPT_NAMES */

#define	EFX_MAC_STATS_MASK_BITS_PER_PAGE	(8 * sizeof (uint32_t))

#define	EFX_MAC_STATS_MASK_NPAGES	\
	(P2ROUNDUP(EFX_MAC_NSTATS, EFX_MAC_STATS_MASK_BITS_PER_PAGE) / \
	    EFX_MAC_STATS_MASK_BITS_PER_PAGE)

/*
 * Get mask of MAC statistics supported by the hardware.
 *
 * If mask_size is insufficient to return the mask, EINVAL error is
 * returned. EFX_MAC_STATS_MASK_NPAGES multiplied by size of the page
 * (which is sizeof (uint32_t)) is sufficient.
 */
extern	__checkReturn			efx_rc_t
efx_mac_stats_get_mask(
	__in				efx_nic_t *enp,
	__out_bcount(mask_size)		uint32_t *maskp,
	__in				size_t mask_size);

#define	EFX_MAC_STAT_SUPPORTED(_mask, _stat)	\
	((_mask)[(_stat) / EFX_MAC_STATS_MASK_BITS_PER_PAGE] &	\
	 (1ULL << ((_stat) & (EFX_MAC_STATS_MASK_BITS_PER_PAGE - 1))))

#define	EFX_MAC_STATS_SIZE 0x400

/*
 * Upload mac statistics supported by the hardware into the given buffer.
 *
 * The reference buffer must be at least %EFX_MAC_STATS_SIZE bytes,
 * and page aligned.
 *
 * The hardware will only DMA statistics that it understands (of course).
 * Drivers should not make any assumptions about which statistics are
 * supported, especially when the statistics are generated by firmware.
 *
 * Thus, drivers should zero this buffer before use, so that not-understood
 * statistics read back as zero.
 */
extern	__checkReturn			efx_rc_t
efx_mac_stats_upload(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp);

extern	__checkReturn			efx_rc_t
efx_mac_stats_periodic(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__in				uint16_t period_ms,
	__in				boolean_t events);

extern	__checkReturn			efx_rc_t
efx_mac_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__inout_ecount(EFX_MAC_NSTATS)	efsys_stat_t *stat,
	__inout_opt			uint32_t *generationp);

#endif	/* EFSYS_OPT_MAC_STATS */

/* MON */

typedef enum efx_mon_type_e {
	EFX_MON_INVALID = 0,
	EFX_MON_SFC90X0,
	EFX_MON_SFC91X0,
	EFX_MON_SFC92X0,
	EFX_MON_NTYPES
} efx_mon_type_t;

#if EFSYS_OPT_NAMES

extern		const char *
efx_mon_name(
	__in	efx_nic_t *enp);

#endif	/* EFSYS_OPT_NAMES */

extern	__checkReturn	efx_rc_t
efx_mon_init(
	__in		efx_nic_t *enp);

extern		void
efx_mon_fini(
	__in	efx_nic_t *enp);

/* PHY */

extern	__checkReturn	efx_rc_t
efx_phy_verify(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_PHY_LED_CONTROL

typedef enum efx_phy_led_mode_e {
	EFX_PHY_LED_DEFAULT = 0,
	EFX_PHY_LED_OFF,
	EFX_PHY_LED_ON,
	EFX_PHY_LED_FLASH,
	EFX_PHY_LED_NMODES
} efx_phy_led_mode_t;

extern	__checkReturn	efx_rc_t
efx_phy_led_set(
	__in	efx_nic_t *enp,
	__in	efx_phy_led_mode_t mode);

#endif	/* EFSYS_OPT_PHY_LED_CONTROL */

extern	__checkReturn	efx_rc_t
efx_port_init(
	__in		efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
efx_port_poll(
	__in		efx_nic_t *enp,
	__out_opt	efx_link_mode_t	*link_modep);

extern		void
efx_port_fini(
	__in	efx_nic_t *enp);

typedef enum efx_phy_cap_type_e {
	EFX_PHY_CAP_INVALID = 0,
	EFX_PHY_CAP_10HDX,
	EFX_PHY_CAP_10FDX,
	EFX_PHY_CAP_100HDX,
	EFX_PHY_CAP_100FDX,
	EFX_PHY_CAP_1000HDX,
	EFX_PHY_CAP_1000FDX,
	EFX_PHY_CAP_10000FDX,
	EFX_PHY_CAP_PAUSE,
	EFX_PHY_CAP_ASYM,
	EFX_PHY_CAP_AN,
	EFX_PHY_CAP_40000FDX,
	EFX_PHY_CAP_NTYPES
} efx_phy_cap_type_t;


#define	EFX_PHY_CAP_CURRENT	0x00000000
#define	EFX_PHY_CAP_DEFAULT	0x00000001
#define	EFX_PHY_CAP_PERM	0x00000002

extern		void
efx_phy_adv_cap_get(
	__in		efx_nic_t *enp,
	__in		uint32_t flag,
	__out		uint32_t *maskp);

extern	__checkReturn	efx_rc_t
efx_phy_adv_cap_set(
	__in		efx_nic_t *enp,
	__in		uint32_t mask);

extern			void
efx_phy_lp_cap_get(
	__in		efx_nic_t *enp,
	__out		uint32_t *maskp);

extern	__checkReturn	efx_rc_t
efx_phy_oui_get(
	__in		efx_nic_t *enp,
	__out		uint32_t *ouip);

typedef enum efx_phy_media_type_e {
	EFX_PHY_MEDIA_INVALID = 0,
	EFX_PHY_MEDIA_XAUI,
	EFX_PHY_MEDIA_CX4,
	EFX_PHY_MEDIA_KX4,
	EFX_PHY_MEDIA_XFP,
	EFX_PHY_MEDIA_SFP_PLUS,
	EFX_PHY_MEDIA_BASE_T,
	EFX_PHY_MEDIA_QSFP_PLUS,
	EFX_PHY_MEDIA_NTYPES
} efx_phy_media_type_t;

/* Get the type of medium currently used.  If the board has ports for
 * modules, a module is present, and we recognise the media type of
 * the module, then this will be the media type of the module.
 * Otherwise it will be the media type of the port.
 */
extern			void
efx_phy_media_type_get(
	__in		efx_nic_t *enp,
	__out		efx_phy_media_type_t *typep);

extern					efx_rc_t
efx_phy_module_get_info(
	__in				efx_nic_t *enp,
	__in				uint8_t dev_addr,
	__in				uint8_t offset,
	__in				uint8_t len,
	__out_bcount(len)		uint8_t *data);

#if EFSYS_OPT_PHY_STATS

/* START MKCONFIG GENERATED PhyHeaderStatsBlock 30ed56ad501f8e36 */
typedef enum efx_phy_stat_e {
	EFX_PHY_STAT_OUI,
	EFX_PHY_STAT_PMA_PMD_LINK_UP,
	EFX_PHY_STAT_PMA_PMD_RX_FAULT,
	EFX_PHY_STAT_PMA_PMD_TX_FAULT,
	EFX_PHY_STAT_PMA_PMD_REV_A,
	EFX_PHY_STAT_PMA_PMD_REV_B,
	EFX_PHY_STAT_PMA_PMD_REV_C,
	EFX_PHY_STAT_PMA_PMD_REV_D,
	EFX_PHY_STAT_PCS_LINK_UP,
	EFX_PHY_STAT_PCS_RX_FAULT,
	EFX_PHY_STAT_PCS_TX_FAULT,
	EFX_PHY_STAT_PCS_BER,
	EFX_PHY_STAT_PCS_BLOCK_ERRORS,
	EFX_PHY_STAT_PHY_XS_LINK_UP,
	EFX_PHY_STAT_PHY_XS_RX_FAULT,
	EFX_PHY_STAT_PHY_XS_TX_FAULT,
	EFX_PHY_STAT_PHY_XS_ALIGN,
	EFX_PHY_STAT_PHY_XS_SYNC_A,
	EFX_PHY_STAT_PHY_XS_SYNC_B,
	EFX_PHY_STAT_PHY_XS_SYNC_C,
	EFX_PHY_STAT_PHY_XS_SYNC_D,
	EFX_PHY_STAT_AN_LINK_UP,
	EFX_PHY_STAT_AN_MASTER,
	EFX_PHY_STAT_AN_LOCAL_RX_OK,
	EFX_PHY_STAT_AN_REMOTE_RX_OK,
	EFX_PHY_STAT_CL22EXT_LINK_UP,
	EFX_PHY_STAT_SNR_A,
	EFX_PHY_STAT_SNR_B,
	EFX_PHY_STAT_SNR_C,
	EFX_PHY_STAT_SNR_D,
	EFX_PHY_STAT_PMA_PMD_SIGNAL_A,
	EFX_PHY_STAT_PMA_PMD_SIGNAL_B,
	EFX_PHY_STAT_PMA_PMD_SIGNAL_C,
	EFX_PHY_STAT_PMA_PMD_SIGNAL_D,
	EFX_PHY_STAT_AN_COMPLETE,
	EFX_PHY_STAT_PMA_PMD_REV_MAJOR,
	EFX_PHY_STAT_PMA_PMD_REV_MINOR,
	EFX_PHY_STAT_PMA_PMD_REV_MICRO,
	EFX_PHY_STAT_PCS_FW_VERSION_0,
	EFX_PHY_STAT_PCS_FW_VERSION_1,
	EFX_PHY_STAT_PCS_FW_VERSION_2,
	EFX_PHY_STAT_PCS_FW_VERSION_3,
	EFX_PHY_STAT_PCS_FW_BUILD_YY,
	EFX_PHY_STAT_PCS_FW_BUILD_MM,
	EFX_PHY_STAT_PCS_FW_BUILD_DD,
	EFX_PHY_STAT_PCS_OP_MODE,
	EFX_PHY_NSTATS
} efx_phy_stat_t;

/* END MKCONFIG GENERATED PhyHeaderStatsBlock */

#if EFSYS_OPT_NAMES

extern					const char *
efx_phy_stat_name(
	__in				efx_nic_t *enp,
	__in				efx_phy_stat_t stat);

#endif	/* EFSYS_OPT_NAMES */

#define	EFX_PHY_STATS_SIZE 0x100

extern	__checkReturn			efx_rc_t
efx_phy_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__inout_ecount(EFX_PHY_NSTATS)	uint32_t *stat);

#endif	/* EFSYS_OPT_PHY_STATS */


#if EFSYS_OPT_BIST

typedef enum efx_bist_type_e {
	EFX_BIST_TYPE_UNKNOWN,
	EFX_BIST_TYPE_PHY_NORMAL,
	EFX_BIST_TYPE_PHY_CABLE_SHORT,
	EFX_BIST_TYPE_PHY_CABLE_LONG,
	EFX_BIST_TYPE_MC_MEM,	/* Test the MC DMEM and IMEM */
	EFX_BIST_TYPE_SAT_MEM,	/* Test the DMEM and IMEM of satellite cpus*/
	EFX_BIST_TYPE_REG,	/* Test the register memories */
	EFX_BIST_TYPE_NTYPES,
} efx_bist_type_t;

typedef enum efx_bist_result_e {
	EFX_BIST_RESULT_UNKNOWN,
	EFX_BIST_RESULT_RUNNING,
	EFX_BIST_RESULT_PASSED,
	EFX_BIST_RESULT_FAILED,
} efx_bist_result_t;

typedef enum efx_phy_cable_status_e {
	EFX_PHY_CABLE_STATUS_OK,
	EFX_PHY_CABLE_STATUS_INVALID,
	EFX_PHY_CABLE_STATUS_OPEN,
	EFX_PHY_CABLE_STATUS_INTRAPAIRSHORT,
	EFX_PHY_CABLE_STATUS_INTERPAIRSHORT,
	EFX_PHY_CABLE_STATUS_BUSY,
} efx_phy_cable_status_t;

typedef enum efx_bist_value_e {
	EFX_BIST_PHY_CABLE_LENGTH_A,
	EFX_BIST_PHY_CABLE_LENGTH_B,
	EFX_BIST_PHY_CABLE_LENGTH_C,
	EFX_BIST_PHY_CABLE_LENGTH_D,
	EFX_BIST_PHY_CABLE_STATUS_A,
	EFX_BIST_PHY_CABLE_STATUS_B,
	EFX_BIST_PHY_CABLE_STATUS_C,
	EFX_BIST_PHY_CABLE_STATUS_D,
	EFX_BIST_FAULT_CODE,
	/* Memory BIST specific values. These match to the MC_CMD_BIST_POLL
	 * response. */
	EFX_BIST_MEM_TEST,
	EFX_BIST_MEM_ADDR,
	EFX_BIST_MEM_BUS,
	EFX_BIST_MEM_EXPECT,
	EFX_BIST_MEM_ACTUAL,
	EFX_BIST_MEM_ECC,
	EFX_BIST_MEM_ECC_PARITY,
	EFX_BIST_MEM_ECC_FATAL,
	EFX_BIST_NVALUES,
} efx_bist_value_t;

extern	__checkReturn		efx_rc_t
efx_bist_enable_offline(
	__in			efx_nic_t *enp);

extern	__checkReturn		efx_rc_t
efx_bist_start(
	__in			efx_nic_t *enp,
	__in			efx_bist_type_t type);

extern	__checkReturn		efx_rc_t
efx_bist_poll(
	__in			efx_nic_t *enp,
	__in			efx_bist_type_t type,
	__out			efx_bist_result_t *resultp,
	__out_opt		uint32_t *value_maskp,
	__out_ecount_opt(count)	unsigned long *valuesp,
	__in			size_t count);

extern				void
efx_bist_stop(
	__in			efx_nic_t *enp,
	__in			efx_bist_type_t type);

#endif	/* EFSYS_OPT_BIST */

#define	EFX_FEATURE_IPV6		0x00000001
#define	EFX_FEATURE_LFSR_HASH_INSERT	0x00000002
#define	EFX_FEATURE_LINK_EVENTS		0x00000004
#define	EFX_FEATURE_PERIODIC_MAC_STATS	0x00000008
#define	EFX_FEATURE_MCDI		0x00000020
#define	EFX_FEATURE_LOOKAHEAD_SPLIT	0x00000040
#define	EFX_FEATURE_MAC_HEADER_FILTERS	0x00000080
#define	EFX_FEATURE_TURBO		0x00000100
#define	EFX_FEATURE_MCDI_DMA		0x00000200
#define	EFX_FEATURE_TX_SRC_FILTERS	0x00000400
#define	EFX_FEATURE_PIO_BUFFERS		0x00000800
#define	EFX_FEATURE_FW_ASSISTED_TSO	0x00001000
#define	EFX_FEATURE_FW_ASSISTED_TSO_V2	0x00002000
#define	EFX_FEATURE_PACKED_STREAM	0x00004000

typedef struct efx_nic_cfg_s {
	uint32_t		enc_board_type;
	uint32_t		enc_phy_type;
#if EFSYS_OPT_NAMES
	char			enc_phy_name[21];
#endif
	char			enc_phy_revision[21];
	efx_mon_type_t		enc_mon_type;
	unsigned int		enc_features;
	uint8_t			enc_mac_addr[6];
	uint8_t			enc_port;	/* PHY port number */
	uint32_t		enc_intr_vec_base;
	uint32_t		enc_intr_limit;
	uint32_t		enc_evq_limit;
	uint32_t		enc_txq_limit;
	uint32_t		enc_rxq_limit;
	uint32_t		enc_txq_max_ndescs;
	uint32_t		enc_buftbl_limit;
	uint32_t		enc_piobuf_limit;
	uint32_t		enc_piobuf_size;
	uint32_t		enc_piobuf_min_alloc_size;
	uint32_t		enc_evq_timer_quantum_ns;
	uint32_t		enc_evq_timer_max_us;
	uint32_t		enc_clk_mult;
	uint32_t		enc_rx_prefix_size;
	uint32_t		enc_rx_buf_align_start;
	uint32_t		enc_rx_buf_align_end;
#if EFSYS_OPT_PHY_FLAGS
	uint32_t		enc_phy_flags_mask;
#endif	/* EFSYS_OPT_PHY_FLAGS */
#if EFSYS_OPT_PHY_LED_CONTROL
	uint32_t		enc_led_mask;
#endif	/* EFSYS_OPT_PHY_LED_CONTROL */
#if EFSYS_OPT_PHY_STATS
	uint64_t		enc_phy_stat_mask;
#endif	/* EFSYS_OPT_PHY_STATS */
#if EFSYS_OPT_MCDI
	uint8_t			enc_mcdi_mdio_channel;
#if EFSYS_OPT_PHY_STATS
	uint32_t		enc_mcdi_phy_stat_mask;
#endif	/* EFSYS_OPT_PHY_STATS */
#endif	/* EFSYS_OPT_MCDI */
#if EFSYS_OPT_BIST
	uint32_t		enc_bist_mask;
#endif	/* EFSYS_OPT_BIST */
#if EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD
	uint32_t		enc_pf;
	uint32_t		enc_vf;
	uint32_t		enc_privilege_mask;
#endif /* EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD */
	boolean_t		enc_bug26807_workaround;
	boolean_t		enc_bug35388_workaround;
	boolean_t		enc_bug41750_workaround;
	boolean_t		enc_bug61265_workaround;
	boolean_t		enc_rx_batching_enabled;
	/* Maximum number of descriptors completed in an rx event. */
	uint32_t		enc_rx_batch_max;
	/* Number of rx descriptors the hardware requires for a push. */
	uint32_t		enc_rx_push_align;
	/*
	 * Maximum number of bytes into the packet the TCP header can start for
	 * the hardware to apply TSO packet edits.
	 */
	uint32_t		enc_tx_tso_tcp_header_offset_limit;
	boolean_t		enc_fw_assisted_tso_enabled;
	boolean_t		enc_fw_assisted_tso_v2_enabled;
	/* Number of TSO contexts on the NIC (FATSOv2) */
	uint32_t		enc_fw_assisted_tso_v2_n_contexts;
	boolean_t		enc_hw_tx_insert_vlan_enabled;
	/* Number of PFs on the NIC */
	uint32_t		enc_hw_pf_count;
	/* Datapath firmware vadapter/vport/vswitch support */
	boolean_t		enc_datapath_cap_evb;
	boolean_t		enc_rx_disable_scatter_supported;
	boolean_t		enc_allow_set_mac_with_installed_filters;
	boolean_t		enc_enhanced_set_mac_supported;
	boolean_t		enc_init_evq_v2_supported;
	boolean_t		enc_rx_packed_stream_supported;
	boolean_t		enc_rx_var_packed_stream_supported;
	boolean_t		enc_pm_and_rxdp_counters;
	boolean_t		enc_mac_stats_40g_tx_size_bins;
	/* External port identifier */
	uint8_t			enc_external_port;
	uint32_t		enc_mcdi_max_payload_length;
	/* VPD may be per-PF or global */
	boolean_t		enc_vpd_is_global;
	/* Minimum unidirectional bandwidth in Mb/s to max out all ports */
	uint32_t		enc_required_pcie_bandwidth_mbps;
	uint32_t		enc_max_pcie_link_gen;
	/* Firmware verifies integrity of NVRAM updates */
	uint32_t		enc_fw_verified_nvram_update_required;
} efx_nic_cfg_t;

#define	EFX_PCI_FUNCTION_IS_PF(_encp)	((_encp)->enc_vf == 0xffff)
#define	EFX_PCI_FUNCTION_IS_VF(_encp)	((_encp)->enc_vf != 0xffff)

#define	EFX_PCI_FUNCTION(_encp)	\
	(EFX_PCI_FUNCTION_IS_PF(_encp) ? (_encp)->enc_pf : (_encp)->enc_vf)

#define	EFX_PCI_VF_PARENT(_encp)	((_encp)->enc_pf)

extern			const efx_nic_cfg_t *
efx_nic_cfg_get(
	__in		efx_nic_t *enp);

/* Driver resource limits (minimum required/maximum usable). */
typedef struct efx_drv_limits_s {
	uint32_t	edl_min_evq_count;
	uint32_t	edl_max_evq_count;

	uint32_t	edl_min_rxq_count;
	uint32_t	edl_max_rxq_count;

	uint32_t	edl_min_txq_count;
	uint32_t	edl_max_txq_count;

	/* PIO blocks (sub-allocated from piobuf) */
	uint32_t	edl_min_pio_alloc_size;
	uint32_t	edl_max_pio_alloc_count;
} efx_drv_limits_t;

extern	__checkReturn	efx_rc_t
efx_nic_set_drv_limits(
	__inout		efx_nic_t *enp,
	__in		efx_drv_limits_t *edlp);

typedef enum efx_nic_region_e {
	EFX_REGION_VI,			/* Memory BAR UC mapping */
	EFX_REGION_PIO_WRITE_VI,	/* Memory BAR WC mapping */
} efx_nic_region_t;

extern	__checkReturn	efx_rc_t
efx_nic_get_bar_region(
	__in		efx_nic_t *enp,
	__in		efx_nic_region_t region,
	__out		uint32_t *offsetp,
	__out		size_t *sizep);

extern	__checkReturn	efx_rc_t
efx_nic_get_vi_pool(
	__in		efx_nic_t *enp,
	__out		uint32_t *evq_countp,
	__out		uint32_t *rxq_countp,
	__out		uint32_t *txq_countp);


/* NVRAM */

#if EFSYS_OPT_DIAG

typedef enum efx_pattern_type_t {
	EFX_PATTERN_BYTE_INCREMENT = 0,
	EFX_PATTERN_ALL_THE_SAME,
	EFX_PATTERN_BIT_ALTERNATE,
	EFX_PATTERN_BYTE_ALTERNATE,
	EFX_PATTERN_BYTE_CHANGING,
	EFX_PATTERN_BIT_SWEEP,
	EFX_PATTERN_NTYPES
} efx_pattern_type_t;

typedef			void
(*efx_sram_pattern_fn_t)(
	__in		size_t row,
	__in		boolean_t negate,
	__out		efx_qword_t *eqp);

extern	__checkReturn	efx_rc_t
efx_sram_test(
	__in		efx_nic_t *enp,
	__in		efx_pattern_type_t type);

#endif	/* EFSYS_OPT_DIAG */

extern	__checkReturn	efx_rc_t
efx_sram_buf_tbl_set(
	__in		efx_nic_t *enp,
	__in		uint32_t id,
	__in		efsys_mem_t *esmp,
	__in		size_t n);

extern		void
efx_sram_buf_tbl_clear(
	__in	efx_nic_t *enp,
	__in	uint32_t id,
	__in	size_t n);

#define	EFX_BUF_TBL_SIZE	0x20000

#define	EFX_BUF_SIZE		4096

/* EV */

typedef struct efx_evq_s	efx_evq_t;

#if EFSYS_OPT_QSTATS

/* START MKCONFIG GENERATED EfxHeaderEventQueueBlock 6f3843f5fe7cc843 */
typedef enum efx_ev_qstat_e {
	EV_ALL,
	EV_RX,
	EV_RX_OK,
	EV_RX_FRM_TRUNC,
	EV_RX_TOBE_DISC,
	EV_RX_PAUSE_FRM_ERR,
	EV_RX_BUF_OWNER_ID_ERR,
	EV_RX_IPV4_HDR_CHKSUM_ERR,
	EV_RX_TCP_UDP_CHKSUM_ERR,
	EV_RX_ETH_CRC_ERR,
	EV_RX_IP_FRAG_ERR,
	EV_RX_MCAST_PKT,
	EV_RX_MCAST_HASH_MATCH,
	EV_RX_TCP_IPV4,
	EV_RX_TCP_IPV6,
	EV_RX_UDP_IPV4,
	EV_RX_UDP_IPV6,
	EV_RX_OTHER_IPV4,
	EV_RX_OTHER_IPV6,
	EV_RX_NON_IP,
	EV_RX_BATCH,
	EV_TX,
	EV_TX_WQ_FF_FULL,
	EV_TX_PKT_ERR,
	EV_TX_PKT_TOO_BIG,
	EV_TX_UNEXPECTED,
	EV_GLOBAL,
	EV_GLOBAL_MNT,
	EV_DRIVER,
	EV_DRIVER_SRM_UPD_DONE,
	EV_DRIVER_TX_DESCQ_FLS_DONE,
	EV_DRIVER_RX_DESCQ_FLS_DONE,
	EV_DRIVER_RX_DESCQ_FLS_FAILED,
	EV_DRIVER_RX_DSC_ERROR,
	EV_DRIVER_TX_DSC_ERROR,
	EV_DRV_GEN,
	EV_MCDI_RESPONSE,
	EV_NQSTATS
} efx_ev_qstat_t;

/* END MKCONFIG GENERATED EfxHeaderEventQueueBlock */

#endif	/* EFSYS_OPT_QSTATS */

extern	__checkReturn	efx_rc_t
efx_ev_init(
	__in		efx_nic_t *enp);

extern		void
efx_ev_fini(
	__in		efx_nic_t *enp);

#define	EFX_EVQ_MAXNEVS		32768
#define	EFX_EVQ_MINNEVS		512

#define	EFX_EVQ_SIZE(_nevs)	((_nevs) * sizeof (efx_qword_t))
#define	EFX_EVQ_NBUFS(_nevs)	(EFX_EVQ_SIZE(_nevs) / EFX_BUF_SIZE)

#define	EFX_EVQ_FLAGS_TYPE_MASK		(0x3)
#define	EFX_EVQ_FLAGS_TYPE_AUTO		(0x0)
#define	EFX_EVQ_FLAGS_TYPE_THROUGHPUT	(0x1)
#define	EFX_EVQ_FLAGS_TYPE_LOW_LATENCY	(0x2)

#define	EFX_EVQ_FLAGS_NOTIFY_MASK	(0xC)
#define	EFX_EVQ_FLAGS_NOTIFY_INTERRUPT	(0x0)	/* Interrupting (default) */
#define	EFX_EVQ_FLAGS_NOTIFY_DISABLED	(0x4)	/* Non-interrupting */

extern	__checkReturn	efx_rc_t
efx_ev_qcreate(
	__in		efx_nic_t *enp,
	__in		unsigned int index,
	__in		efsys_mem_t *esmp,
	__in		size_t n,
	__in		uint32_t id,
	__in		uint32_t us,
	__in		uint32_t flags,
	__deref_out	efx_evq_t **eepp);

extern		void
efx_ev_qpost(
	__in		efx_evq_t *eep,
	__in		uint16_t data);

typedef __checkReturn	boolean_t
(*efx_initialized_ev_t)(
	__in_opt	void *arg);

#define	EFX_PKT_UNICAST		0x0004
#define	EFX_PKT_START		0x0008

#define	EFX_PKT_VLAN_TAGGED	0x0010
#define	EFX_CKSUM_TCPUDP	0x0020
#define	EFX_CKSUM_IPV4		0x0040
#define	EFX_PKT_CONT		0x0080

#define	EFX_CHECK_VLAN		0x0100
#define	EFX_PKT_TCP		0x0200
#define	EFX_PKT_UDP		0x0400
#define	EFX_PKT_IPV4		0x0800

#define	EFX_PKT_IPV6		0x1000
#define	EFX_PKT_PREFIX_LEN	0x2000
#define	EFX_ADDR_MISMATCH	0x4000
#define	EFX_DISCARD		0x8000

/*
 * The following flags are used only for packed stream
 * mode. The values for the flags are reused to fit into 16 bit,
 * since EFX_PKT_START and EFX_PKT_CONT are never used in
 * packed stream mode
 */
#define	EFX_PKT_PACKED_STREAM_NEW_BUFFER	EFX_PKT_START
#define	EFX_PKT_PACKED_STREAM_PARSE_INCOMPLETE	EFX_PKT_CONT


#define	EFX_EV_RX_NLABELS	32
#define	EFX_EV_TX_NLABELS	32

typedef	__checkReturn	boolean_t
(*efx_rx_ev_t)(
	__in_opt	void *arg,
	__in		uint32_t label,
	__in		uint32_t id,
	__in		uint32_t size,
	__in		uint16_t flags);

typedef	__checkReturn	boolean_t
(*efx_tx_ev_t)(
	__in_opt	void *arg,
	__in		uint32_t label,
	__in		uint32_t id);

#define	EFX_EXCEPTION_RX_RECOVERY	0x00000001
#define	EFX_EXCEPTION_RX_DSC_ERROR	0x00000002
#define	EFX_EXCEPTION_TX_DSC_ERROR	0x00000003
#define	EFX_EXCEPTION_UNKNOWN_SENSOREVT	0x00000004
#define	EFX_EXCEPTION_FWALERT_SRAM	0x00000005
#define	EFX_EXCEPTION_UNKNOWN_FWALERT	0x00000006
#define	EFX_EXCEPTION_RX_ERROR		0x00000007
#define	EFX_EXCEPTION_TX_ERROR		0x00000008
#define	EFX_EXCEPTION_EV_ERROR		0x00000009

typedef	__checkReturn	boolean_t
(*efx_exception_ev_t)(
	__in_opt	void *arg,
	__in		uint32_t label,
	__in		uint32_t data);

typedef	__checkReturn	boolean_t
(*efx_rxq_flush_done_ev_t)(
	__in_opt	void *arg,
	__in		uint32_t rxq_index);

typedef	__checkReturn	boolean_t
(*efx_rxq_flush_failed_ev_t)(
	__in_opt	void *arg,
	__in		uint32_t rxq_index);

typedef	__checkReturn	boolean_t
(*efx_txq_flush_done_ev_t)(
	__in_opt	void *arg,
	__in		uint32_t txq_index);

typedef	__checkReturn	boolean_t
(*efx_software_ev_t)(
	__in_opt	void *arg,
	__in		uint16_t magic);

typedef	__checkReturn	boolean_t
(*efx_sram_ev_t)(
	__in_opt	void *arg,
	__in		uint32_t code);

#define	EFX_SRAM_CLEAR		0
#define	EFX_SRAM_UPDATE		1
#define	EFX_SRAM_ILLEGAL_CLEAR	2

typedef	__checkReturn	boolean_t
(*efx_wake_up_ev_t)(
	__in_opt	void *arg,
	__in		uint32_t label);

typedef	__checkReturn	boolean_t
(*efx_timer_ev_t)(
	__in_opt	void *arg,
	__in		uint32_t label);

typedef __checkReturn	boolean_t
(*efx_link_change_ev_t)(
	__in_opt	void *arg,
	__in		efx_link_mode_t	link_mode);

#if EFSYS_OPT_MAC_STATS

typedef __checkReturn	boolean_t
(*efx_mac_stats_ev_t)(
	__in_opt	void *arg,
	__in		uint32_t generation
	);

#endif	/* EFSYS_OPT_MAC_STATS */

typedef struct efx_ev_callbacks_s {
	efx_initialized_ev_t		eec_initialized;
	efx_rx_ev_t			eec_rx;
	efx_tx_ev_t			eec_tx;
	efx_exception_ev_t		eec_exception;
	efx_rxq_flush_done_ev_t		eec_rxq_flush_done;
	efx_rxq_flush_failed_ev_t	eec_rxq_flush_failed;
	efx_txq_flush_done_ev_t		eec_txq_flush_done;
	efx_software_ev_t		eec_software;
	efx_sram_ev_t			eec_sram;
	efx_wake_up_ev_t		eec_wake_up;
	efx_timer_ev_t			eec_timer;
	efx_link_change_ev_t		eec_link_change;
#if EFSYS_OPT_MAC_STATS
	efx_mac_stats_ev_t		eec_mac_stats;
#endif	/* EFSYS_OPT_MAC_STATS */
} efx_ev_callbacks_t;

extern	__checkReturn	boolean_t
efx_ev_qpending(
	__in		efx_evq_t *eep,
	__in		unsigned int count);

extern			void
efx_ev_qpoll(
	__in		efx_evq_t *eep,
	__inout		unsigned int *countp,
	__in		const efx_ev_callbacks_t *eecp,
	__in_opt	void *arg);

extern	__checkReturn	efx_rc_t
efx_ev_usecs_to_ticks(
	__in		efx_nic_t *enp,
	__in		unsigned int usecs,
	__out		unsigned int *ticksp);

extern	__checkReturn	efx_rc_t
efx_ev_qmoderate(
	__in		efx_evq_t *eep,
	__in		unsigned int us);

extern	__checkReturn	efx_rc_t
efx_ev_qprime(
	__in		efx_evq_t *eep,
	__in		unsigned int count);

#if EFSYS_OPT_QSTATS

#if EFSYS_OPT_NAMES

extern		const char *
efx_ev_qstat_name(
	__in	efx_nic_t *enp,
	__in	unsigned int id);

#endif	/* EFSYS_OPT_NAMES */

extern					void
efx_ev_qstats_update(
	__in				efx_evq_t *eep,
	__inout_ecount(EV_NQSTATS)	efsys_stat_t *stat);

#endif	/* EFSYS_OPT_QSTATS */

extern		void
efx_ev_qdestroy(
	__in	efx_evq_t *eep);

/* RX */

extern	__checkReturn	efx_rc_t
efx_rx_init(
	__inout		efx_nic_t *enp);

extern		void
efx_rx_fini(
	__in		efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
efx_pseudo_hdr_pkt_length_get(
	__in		efx_rxq_t *erp,
	__in		uint8_t *buffer,
	__out		uint16_t *pkt_lengthp);

#define	EFX_RXQ_MAXNDESCS		4096
#define	EFX_RXQ_MINNDESCS		512

#define	EFX_RXQ_SIZE(_ndescs)		((_ndescs) * sizeof (efx_qword_t))
#define	EFX_RXQ_NBUFS(_ndescs)		(EFX_RXQ_SIZE(_ndescs) / EFX_BUF_SIZE)
#define	EFX_RXQ_LIMIT(_ndescs)		((_ndescs) - 16)
#define	EFX_RXQ_DC_NDESCS(_dcsize)	(8 << _dcsize)

typedef enum efx_rxq_type_e {
	EFX_RXQ_TYPE_DEFAULT,
	EFX_RXQ_TYPE_SCATTER,
	EFX_RXQ_TYPE_PACKED_STREAM_1M,
	EFX_RXQ_TYPE_PACKED_STREAM_512K,
	EFX_RXQ_TYPE_PACKED_STREAM_256K,
	EFX_RXQ_TYPE_PACKED_STREAM_128K,
	EFX_RXQ_TYPE_PACKED_STREAM_64K,
	EFX_RXQ_NTYPES
} efx_rxq_type_t;

extern	__checkReturn	efx_rc_t
efx_rx_qcreate(
	__in		efx_nic_t *enp,
	__in		unsigned int index,
	__in		unsigned int label,
	__in		efx_rxq_type_t type,
	__in		efsys_mem_t *esmp,
	__in		size_t n,
	__in		uint32_t id,
	__in		efx_evq_t *eep,
	__deref_out	efx_rxq_t **erpp);

typedef struct efx_buffer_s {
	efsys_dma_addr_t	eb_addr;
	size_t			eb_size;
	boolean_t		eb_eop;
} efx_buffer_t;

typedef struct efx_desc_s {
	efx_qword_t ed_eq;
} efx_desc_t;

extern			void
efx_rx_qpost(
	__in		efx_rxq_t *erp,
	__in_ecount(n)	efsys_dma_addr_t *addrp,
	__in		size_t size,
	__in		unsigned int n,
	__in		unsigned int completed,
	__in		unsigned int added);

extern		void
efx_rx_qpush(
	__in	efx_rxq_t *erp,
	__in	unsigned int added,
	__inout	unsigned int *pushedp);

extern	__checkReturn	efx_rc_t
efx_rx_qflush(
	__in	efx_rxq_t *erp);

extern		void
efx_rx_qenable(
	__in	efx_rxq_t *erp);

extern		void
efx_rx_qdestroy(
	__in	efx_rxq_t *erp);

/* TX */

typedef struct efx_txq_s	efx_txq_t;

#if EFSYS_OPT_QSTATS

/* START MKCONFIG GENERATED EfxHeaderTransmitQueueBlock 12dff8778598b2db */
typedef enum efx_tx_qstat_e {
	TX_POST,
	TX_POST_PIO,
	TX_NQSTATS
} efx_tx_qstat_t;

/* END MKCONFIG GENERATED EfxHeaderTransmitQueueBlock */

#endif	/* EFSYS_OPT_QSTATS */

extern	__checkReturn	efx_rc_t
efx_tx_init(
	__in		efx_nic_t *enp);

extern		void
efx_tx_fini(
	__in	efx_nic_t *enp);

#define	EFX_TXQ_MINNDESCS		512

#define	EFX_TXQ_SIZE(_ndescs)		((_ndescs) * sizeof (efx_qword_t))
#define	EFX_TXQ_NBUFS(_ndescs)		(EFX_TXQ_SIZE(_ndescs) / EFX_BUF_SIZE)
#define	EFX_TXQ_LIMIT(_ndescs)		((_ndescs) - 16)
#define	EFX_TXQ_DC_NDESCS(_dcsize)	(8 << _dcsize)

#define	EFX_TXQ_MAX_BUFS 8 /* Maximum independent of EFX_BUG35388_WORKAROUND. */

#define	EFX_TXQ_CKSUM_IPV4	0x0001
#define	EFX_TXQ_CKSUM_TCPUDP	0x0002
#define	EFX_TXQ_FATSOV2		0x0004

extern	__checkReturn	efx_rc_t
efx_tx_qcreate(
	__in		efx_nic_t *enp,
	__in		unsigned int index,
	__in		unsigned int label,
	__in		efsys_mem_t *esmp,
	__in		size_t n,
	__in		uint32_t id,
	__in		uint16_t flags,
	__in		efx_evq_t *eep,
	__deref_out	efx_txq_t **etpp,
	__out		unsigned int *addedp);

extern	__checkReturn	efx_rc_t
efx_tx_qpost(
	__in		efx_txq_t *etp,
	__in_ecount(n)	efx_buffer_t *eb,
	__in		unsigned int n,
	__in		unsigned int completed,
	__inout		unsigned int *addedp);

extern	__checkReturn	efx_rc_t
efx_tx_qpace(
	__in		efx_txq_t *etp,
	__in		unsigned int ns);

extern			void
efx_tx_qpush(
	__in		efx_txq_t *etp,
	__in		unsigned int added,
	__in		unsigned int pushed);

extern	__checkReturn	efx_rc_t
efx_tx_qflush(
	__in		efx_txq_t *etp);

extern			void
efx_tx_qenable(
	__in		efx_txq_t *etp);

extern	__checkReturn	efx_rc_t
efx_tx_qpio_enable(
	__in		efx_txq_t *etp);

extern			void
efx_tx_qpio_disable(
	__in		efx_txq_t *etp);

extern	__checkReturn	efx_rc_t
efx_tx_qpio_write(
	__in			efx_txq_t *etp,
	__in_ecount(buf_length)	uint8_t *buffer,
	__in			size_t buf_length,
	__in			size_t pio_buf_offset);

extern	__checkReturn	efx_rc_t
efx_tx_qpio_post(
	__in			efx_txq_t *etp,
	__in			size_t pkt_length,
	__in			unsigned int completed,
	__inout			unsigned int *addedp);

extern	__checkReturn	efx_rc_t
efx_tx_qdesc_post(
	__in		efx_txq_t *etp,
	__in_ecount(n)	efx_desc_t *ed,
	__in		unsigned int n,
	__in		unsigned int completed,
	__inout		unsigned int *addedp);

extern	void
efx_tx_qdesc_dma_create(
	__in	efx_txq_t *etp,
	__in	efsys_dma_addr_t addr,
	__in	size_t size,
	__in	boolean_t eop,
	__out	efx_desc_t *edp);

extern	void
efx_tx_qdesc_tso_create(
	__in	efx_txq_t *etp,
	__in	uint16_t ipv4_id,
	__in	uint32_t tcp_seq,
	__in	uint8_t  tcp_flags,
	__out	efx_desc_t *edp);

/* Number of FATSOv2 option descriptors */
#define	EFX_TX_FATSOV2_OPT_NDESCS		2

/* Maximum number of DMA segments per TSO packet (not superframe) */
#define	EFX_TX_FATSOV2_DMA_SEGS_PER_PKT_MAX	24

extern	void
efx_tx_qdesc_tso2_create(
	__in			efx_txq_t *etp,
	__in			uint16_t ipv4_id,
	__in			uint32_t tcp_seq,
	__in			uint16_t tcp_mss,
	__out_ecount(count)	efx_desc_t *edp,
	__in			int count);

extern	void
efx_tx_qdesc_vlantci_create(
	__in	efx_txq_t *etp,
	__in	uint16_t tci,
	__out	efx_desc_t *edp);

#if EFSYS_OPT_QSTATS

#if EFSYS_OPT_NAMES

extern		const char *
efx_tx_qstat_name(
	__in	efx_nic_t *etp,
	__in	unsigned int id);

#endif	/* EFSYS_OPT_NAMES */

extern					void
efx_tx_qstats_update(
	__in				efx_txq_t *etp,
	__inout_ecount(TX_NQSTATS)	efsys_stat_t *stat);

#endif	/* EFSYS_OPT_QSTATS */

extern		void
efx_tx_qdestroy(
	__in	efx_txq_t *etp);


/* FILTER */

#if EFSYS_OPT_FILTER

#define	EFX_ETHER_TYPE_IPV4 0x0800
#define	EFX_ETHER_TYPE_IPV6 0x86DD

#define	EFX_IPPROTO_TCP 6
#define	EFX_IPPROTO_UDP 17

/* Use RSS to spread across multiple queues */
#define	EFX_FILTER_FLAG_RX_RSS		0x01
/* Enable RX scatter */
#define	EFX_FILTER_FLAG_RX_SCATTER	0x02
/*
 * Override an automatic filter (priority EFX_FILTER_PRI_AUTO).
 * May only be set by the filter implementation for each type.
 * A removal request will restore the automatic filter in its place.
 */
#define	EFX_FILTER_FLAG_RX_OVER_AUTO	0x04
/* Filter is for RX */
#define	EFX_FILTER_FLAG_RX		0x08
/* Filter is for TX */
#define	EFX_FILTER_FLAG_TX		0x10

typedef unsigned int efx_filter_flags_t;

typedef enum efx_filter_match_flags_e {
	EFX_FILTER_MATCH_REM_HOST = 0x0001,	/* Match by remote IP host
						 * address */
	EFX_FILTER_MATCH_LOC_HOST = 0x0002,	/* Match by local IP host
						 * address */
	EFX_FILTER_MATCH_REM_MAC = 0x0004,	/* Match by remote MAC address */
	EFX_FILTER_MATCH_REM_PORT = 0x0008,	/* Match by remote TCP/UDP port */
	EFX_FILTER_MATCH_LOC_MAC = 0x0010,	/* Match by remote TCP/UDP port */
	EFX_FILTER_MATCH_LOC_PORT = 0x0020,	/* Match by local TCP/UDP port */
	EFX_FILTER_MATCH_ETHER_TYPE = 0x0040,	/* Match by Ether-type */
	EFX_FILTER_MATCH_INNER_VID = 0x0080,	/* Match by inner VLAN ID */
	EFX_FILTER_MATCH_OUTER_VID = 0x0100,	/* Match by outer VLAN ID */
	EFX_FILTER_MATCH_IP_PROTO = 0x0200,	/* Match by IP transport
						 * protocol */
	EFX_FILTER_MATCH_LOC_MAC_IG = 0x0400,	/* Match by local MAC address
						 * I/G bit. Used for RX default
						 * unicast and multicast/
						 * broadcast filters. */
} efx_filter_match_flags_t;

typedef enum efx_filter_priority_s {
	EFX_FILTER_PRI_HINT = 0,	/* Performance hint */
	EFX_FILTER_PRI_AUTO,		/* Automatic filter based on device
					 * address list or hardware
					 * requirements. This may only be used
					 * by the filter implementation for
					 * each NIC type. */
	EFX_FILTER_PRI_MANUAL,		/* Manually configured filter */
	EFX_FILTER_PRI_REQUIRED,	/* Required for correct behaviour of the
					 * client (e.g. SR-IOV, HyperV VMQ etc.)
					 */
} efx_filter_priority_t;

/*
 * FIXME: All these fields are assumed to be in little-endian byte order.
 * It may be better for some to be big-endian. See bug42804.
 */

typedef struct efx_filter_spec_s {
	uint32_t	efs_match_flags:12;
	uint32_t	efs_priority:2;
	uint32_t	efs_flags:6;
	uint32_t	efs_dmaq_id:12;
	uint32_t	efs_rss_context;
	uint16_t	efs_outer_vid;
	uint16_t	efs_inner_vid;
	uint8_t		efs_loc_mac[EFX_MAC_ADDR_LEN];
	uint8_t		efs_rem_mac[EFX_MAC_ADDR_LEN];
	uint16_t	efs_ether_type;
	uint8_t		efs_ip_proto;
	uint16_t	efs_loc_port;
	uint16_t	efs_rem_port;
	efx_oword_t	efs_rem_host;
	efx_oword_t	efs_loc_host;
} efx_filter_spec_t;


/* Default values for use in filter specifications */
#define	EFX_FILTER_SPEC_RSS_CONTEXT_DEFAULT	0xffffffff
#define	EFX_FILTER_SPEC_RX_DMAQ_ID_DROP		0xfff
#define	EFX_FILTER_SPEC_VID_UNSPEC		0xffff

extern	__checkReturn	efx_rc_t
efx_filter_init(
	__in		efx_nic_t *enp);

extern			void
efx_filter_fini(
	__in		efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
efx_filter_insert(
	__in		efx_nic_t *enp,
	__inout		efx_filter_spec_t *spec);

extern	__checkReturn	efx_rc_t
efx_filter_remove(
	__in		efx_nic_t *enp,
	__inout		efx_filter_spec_t *spec);

extern	__checkReturn	efx_rc_t
efx_filter_restore(
	__in		efx_nic_t *enp);

extern	__checkReturn	efx_rc_t
efx_filter_supported_filters(
	__in		efx_nic_t *enp,
	__out		uint32_t *list,
	__out		size_t *length);

extern			void
efx_filter_spec_init_rx(
	__out		efx_filter_spec_t *spec,
	__in		efx_filter_priority_t priority,
	__in		efx_filter_flags_t flags,
	__in		efx_rxq_t *erp);

extern			void
efx_filter_spec_init_tx(
	__out		efx_filter_spec_t *spec,
	__in		efx_txq_t *etp);

extern	__checkReturn	efx_rc_t
efx_filter_spec_set_ipv4_local(
	__inout		efx_filter_spec_t *spec,
	__in		uint8_t proto,
	__in		uint32_t host,
	__in		uint16_t port);

extern	__checkReturn	efx_rc_t
efx_filter_spec_set_ipv4_full(
	__inout		efx_filter_spec_t *spec,
	__in		uint8_t proto,
	__in		uint32_t lhost,
	__in		uint16_t lport,
	__in		uint32_t rhost,
	__in		uint16_t rport);

extern	__checkReturn	efx_rc_t
efx_filter_spec_set_eth_local(
	__inout		efx_filter_spec_t *spec,
	__in		uint16_t vid,
	__in		const uint8_t *addr);

extern	__checkReturn	efx_rc_t
efx_filter_spec_set_uc_def(
	__inout		efx_filter_spec_t *spec);

extern	__checkReturn	efx_rc_t
efx_filter_spec_set_mc_def(
	__inout		efx_filter_spec_t *spec);

#endif	/* EFSYS_OPT_FILTER */

/* HASH */

extern	__checkReturn		uint32_t
efx_hash_dwords(
	__in_ecount(count)	uint32_t const *input,
	__in			size_t count,
	__in			uint32_t init);

extern	__checkReturn		uint32_t
efx_hash_bytes(
	__in_ecount(length)	uint8_t const *input,
	__in			size_t length,
	__in			uint32_t init);



#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_EFX_H */
