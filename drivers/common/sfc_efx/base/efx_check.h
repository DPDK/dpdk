/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2012-2019 Solarflare Communications Inc.
 */

#ifndef _SYS_EFX_CHECK_H
#define	_SYS_EFX_CHECK_H

#include "efsys.h"

/*
 * LIBEFX_* defines may be used to put API functions into dedicated code
 * section if required by driver development framework and conventions.
 */

#ifndef LIBEFX_API
# error "LIBEFX_API must be defined"
#endif

#ifndef LIBEFX_INTERNAL
# error "LIBEFX_INTERNAL must be defined"
#endif

/*
 * Check that the efsys.h header in client code has a valid combination of
 * EFSYS_OPT_xxx options.
 *
 * NOTE: Keep checks for obsolete options here to ensure that they are removed
 * from client code (and do not reappear in merges from other branches).
 */

/* Check family options for EF10 architecture controllers. */
#define	EFX_OPTS_EF10()	\
	(EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD || EFSYS_OPT_MEDFORD2)

#ifdef EFSYS_OPT_FALCON
# error "FALCON is obsolete and is not supported."
#endif

#if EFSYS_OPT_BOOTCFG
/* Support NVRAM based boot config */
# if !EFSYS_OPT_NVRAM
#  error "BOOTCFG requires NVRAM"
# endif
#endif /* EFSYS_OPT_BOOTCFG */

#if EFSYS_OPT_CHECK_REG
/* Verify chip implements accessed registers */
# if !(EFSYS_OPT_RIVERHEAD || EFX_OPTS_EF10() || EFSYS_OPT_SIENA)
#  error "CHECK_REG requires RIVERHEAD or EF10 arch or SIENA"
# endif
#endif /* EFSYS_OPT_CHECK_REG */

#if EFSYS_OPT_DECODE_INTR_FATAL
/* Decode fatal errors */
# if !EFSYS_OPT_SIENA
#  error "INTR_FATAL requires SIENA"
# endif
#endif /* EFSYS_OPT_DECODE_INTR_FATAL */

#if EFSYS_OPT_DIAG
/* Support diagnostic hardware tests */
# if !(EFX_OPTS_EF10() || EFSYS_OPT_SIENA)
#  error "DIAG requires EF10 arch or SIENA"
# endif
#endif /* EFSYS_OPT_DIAG */

#if EFSYS_OPT_EV_PREFETCH
/* Support optimized EVQ data access */
# if !(EFX_OPTS_EF10() || EFSYS_OPT_SIENA)
#  error "EV_PREFETCH requires EF10 arch or SIENA"
# endif
#endif /* EFSYS_OPT_EV_PREFETCH */

#ifdef EFSYS_OPT_FALCON_NIC_CFG_OVERRIDE
# error "FALCON_NIC_CFG_OVERRIDE is obsolete and is not supported."
#endif

#if EFSYS_OPT_FILTER
/* Support hardware packet filters */
# if !(EFSYS_OPT_RIVERHEAD || EFX_OPTS_EF10() || EFSYS_OPT_SIENA)
#  error "FILTER requires RIVERHEAD or EF10 arch or SIENA"
# endif
#endif /* EFSYS_OPT_FILTER */

#if EFSYS_OPT_RIVERHEAD || EFX_OPTS_EF10()
# if !EFSYS_OPT_FILTER
#  error "RIVERHEAD or EF10 arch requires FILTER"
# endif
#endif /* EFX_OPTS_EF10() */

#if EFSYS_OPT_LOOPBACK
/* Support hardware loopback modes */
# if !(EFSYS_OPT_RIVERHEAD || EFX_OPTS_EF10() || EFSYS_OPT_SIENA)
#  error "LOOPBACK requires RIVERHEAD or EF10 arch or SIENA"
# endif
#endif /* EFSYS_OPT_LOOPBACK */

#ifdef EFSYS_OPT_MAC_FALCON_GMAC
# error "MAC_FALCON_GMAC is obsolete and is not supported."
#endif

#ifdef EFSYS_OPT_MAC_FALCON_XMAC
# error "MAC_FALCON_XMAC is obsolete and is not supported."
#endif

#if EFSYS_OPT_MAC_STATS
/* Support MAC statistics */
# if !(EFSYS_OPT_RIVERHEAD || EFX_OPTS_EF10() || EFSYS_OPT_SIENA)
#  error "MAC_STATS requires RIVERHEAD or EF10 arch or SIENA"
# endif
#endif /* EFSYS_OPT_MAC_STATS */

#if EFSYS_OPT_MCDI
/* Support management controller messages */
# if !(EFSYS_OPT_RIVERHEAD || EFX_OPTS_EF10() || EFSYS_OPT_SIENA)
#  error "MCDI requires RIVERHEAD or EF10 arch or SIENA"
# endif
#endif /* EFSYS_OPT_MCDI */

#if (EFSYS_OPT_RIVERHEAD || EFX_OPTS_EF10() || EFSYS_OPT_SIENA)
# if !EFSYS_OPT_MCDI
#  error "RIVERHEAD or EF10 arch or SIENA requires MCDI"
# endif
#endif

#if EFSYS_OPT_MCDI_LOGGING
/* Support MCDI logging */
# if !EFSYS_OPT_MCDI
#  error "MCDI_LOGGING requires MCDI"
# endif
#endif /* EFSYS_OPT_MCDI_LOGGING */

#if EFSYS_OPT_MCDI_PROXY_AUTH_SERVER
/* Support MCDI proxy authorization (server) */
# if !EFSYS_OPT_MCDI_PROXY_AUTH
#  error "MCDI_PROXY_AUTH_SERVER requires MCDI_PROXY_AUTH"
# endif
#endif /* EFSYS_OPT_MCDI_PROXY_AUTH_SERVER */

#if EFSYS_OPT_MCDI_PROXY_AUTH
/* Support MCDI proxy authorization (client) */
# if !EFSYS_OPT_MCDI
#  error "MCDI_PROXY_AUTH requires MCDI"
# endif
#endif /* EFSYS_OPT_MCDI_PROXY_AUTH */

#ifdef EFSYS_OPT_MON_LM87
# error "MON_LM87 is obsolete and is not supported."
#endif

#ifdef EFSYS_OPT_MON_MAX6647
# error "MON_MAX6647 is obsolete and is not supported."
#endif

#ifdef EFSYS_OPT_MON_NULL
# error "MON_NULL is obsolete and is not supported."
#endif

#ifdef EFSYS_OPT_MON_SIENA
#  error "MON_SIENA is obsolete (replaced by MON_MCDI)."
#endif

#ifdef EFSYS_OPT_MON_HUNTINGTON
#  error "MON_HUNTINGTON is obsolete (replaced by MON_MCDI)."
#endif

#if EFSYS_OPT_MON_STATS
/* Support monitor statistics (voltage/temperature) */
# if !(EFX_OPTS_EF10() || EFSYS_OPT_SIENA)
#  error "MON_STATS requires EF10 arch or SIENA"
# endif
#endif /* EFSYS_OPT_MON_STATS */

#if EFSYS_OPT_MON_MCDI
/* Support Monitor via mcdi */
# if !(EFX_OPTS_EF10() || EFSYS_OPT_SIENA)
#  error "MON_MCDI requires EF10 arch or SIENA"
# endif
#endif /* EFSYS_OPT_MON_MCDI*/

#if EFSYS_OPT_NAMES
/* Support printable names for statistics */
# if !(EFSYS_OPT_LOOPBACK || EFSYS_OPT_MAC_STATS || EFSYS_OPT_MCDI || \
	EFSYS_MON_STATS || EFSYS_OPT_PHY_STATS || EFSYS_OPT_QSTATS)
#  error "NAMES requires LOOPBACK or xxxSTATS or MCDI"
# endif
#endif /* EFSYS_OPT_NAMES */

#if EFSYS_OPT_NVRAM
/* Support non volatile configuration */
# if !(EFX_OPTS_EF10() || EFSYS_OPT_SIENA)
#  error "NVRAM requires EF10 arch or SIENA"
# endif
#endif /* EFSYS_OPT_NVRAM */

#if EFSYS_OPT_IMAGE_LAYOUT
/* Support signed image layout handling */
# if !(EFSYS_OPT_MEDFORD || EFSYS_OPT_MEDFORD2)
#  error "IMAGE_LAYOUT requires MEDFORD or MEDFORD2"
# endif
#endif /* EFSYS_OPT_IMAGE_LAYOUT */

#ifdef EFSYS_OPT_NVRAM_FALCON_BOOTROM
# error "NVRAM_FALCON_BOOTROM is obsolete and is not supported."
#endif

#ifdef EFSYS_OPT_NVRAM_SFT9001
# error "NVRAM_SFT9001 is obsolete and is not supported."
#endif

#ifdef EFSYS_OPT_NVRAM_SFX7101
# error "NVRAM_SFX7101 is obsolete and is not supported."
#endif

#ifdef EFSYS_OPT_PCIE_TUNE
# error "PCIE_TUNE is obsolete and is not supported."
#endif

#ifdef EFSYS_OPT_PHY_BIST
# error "PHY_BIST is obsolete (replaced by BIST)."
#endif

#if EFSYS_OPT_PHY_FLAGS
/* Support PHY flags */
# if !EFSYS_OPT_SIENA
#  error "PHY_FLAGS requires SIENA"
# endif
#endif /* EFSYS_OPT_PHY_FLAGS */

#if EFSYS_OPT_PHY_LED_CONTROL
/* Support for PHY LED control */
# if !(EFX_OPTS_EF10() || EFSYS_OPT_SIENA)
#  error "PHY_LED_CONTROL requires EF10 arch or SIENA"
# endif
#endif /* EFSYS_OPT_PHY_LED_CONTROL */

#ifdef EFSYS_OPT_PHY_NULL
# error "PHY_NULL is obsolete and is not supported."
#endif

#ifdef EFSYS_OPT_PHY_PM8358
# error "PHY_PM8358 is obsolete and is not supported."
#endif

#ifdef EFSYS_OPT_PHY_PROPS
# error "PHY_PROPS is obsolete and is not supported."
#endif

#ifdef EFSYS_OPT_PHY_QT2022C2
# error "PHY_QT2022C2 is obsolete and is not supported."
#endif

#ifdef EFSYS_OPT_PHY_QT2025C
# error "PHY_QT2025C is obsolete and is not supported."
#endif

#ifdef EFSYS_OPT_PHY_SFT9001
# error "PHY_SFT9001 is obsolete and is not supported."
#endif

#ifdef EFSYS_OPT_PHY_SFX7101
# error "PHY_SFX7101 is obsolete and is not supported."
#endif

#if EFSYS_OPT_PHY_STATS
/* Support PHY statistics */
# if !(EFSYS_OPT_SIENA || EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD)
#  error "PHY_STATS requires SIENA or HUNTINGTON or MEDFORD"
# endif
#endif /* EFSYS_OPT_PHY_STATS */

#ifdef EFSYS_OPT_PHY_TXC43128
# error "PHY_TXC43128 is obsolete and is not supported."
#endif

#if EFSYS_OPT_QSTATS
/* Support EVQ/RXQ/TXQ statistics */
# if !(EFX_OPTS_EF10() || EFSYS_OPT_SIENA)
#  error "QSTATS requires EF10 arch or SIENA"
# endif
#endif /* EFSYS_OPT_QSTATS */

#ifdef EFSYS_OPT_RX_HDR_SPLIT
# error "RX_HDR_SPLIT is obsolete and is not supported"
#endif

#if EFSYS_OPT_RX_SCALE
/* Support receive scaling (RSS) */
# if !(EFSYS_OPT_RIVERHEAD || EFX_OPTS_EF10() || EFSYS_OPT_SIENA)
#  error "RX_SCALE requires RIVERHEAD or EF10 arch or SIENA"
# endif
#endif /* EFSYS_OPT_RX_SCALE */

#if EFSYS_OPT_RX_SCATTER
/* Support receive scatter DMA */
# if !(EFSYS_OPT_RIVERHEAD || EFX_OPTS_EF10() || EFSYS_OPT_SIENA)
#  error "RX_SCATTER requires RIVERHEAD or EF10 arch or SIENA"
# endif
#endif /* EFSYS_OPT_RX_SCATTER */

#ifdef EFSYS_OPT_STAT_NAME
# error "STAT_NAME is obsolete (replaced by NAMES)."
#endif

#if EFSYS_OPT_VPD
/* Support PCI Vital Product Data (VPD) */
# if !(EFX_OPTS_EF10() || EFSYS_OPT_SIENA)
#  error "VPD requires EF10 arch or SIENA"
# endif
#endif /* EFSYS_OPT_VPD */

#ifdef EFSYS_OPT_WOL
# error "WOL is obsolete and is not supported"
#endif /* EFSYS_OPT_WOL */

#ifdef EFSYS_OPT_MCAST_FILTER_LIST
#  error "MCAST_FILTER_LIST is obsolete and is not supported"
#endif

#if EFSYS_OPT_BIST
/* Support BIST */
# if !(EFX_OPTS_EF10() || EFSYS_OPT_SIENA)
#  error "BIST requires EF10 arch or SIENA"
# endif
#endif /* EFSYS_OPT_BIST */

#if EFSYS_OPT_LICENSING
/* Support MCDI licensing API */
# if !EFSYS_OPT_MCDI
#  error "LICENSING requires MCDI"
# endif
# if !EFSYS_HAS_UINT64
#  error "LICENSING requires UINT64"
# endif
#endif /* EFSYS_OPT_LICENSING */

#if EFSYS_OPT_ALLOW_UNCONFIGURED_NIC
/* Support adapters with missing static config (for factory use only) */
# if !(EFSYS_OPT_RIVERHEAD || EFSYS_OPT_MEDFORD || EFSYS_OPT_MEDFORD2)
#  error "ALLOW_UNCONFIGURED_NIC requires RIVERHEAD or MEDFORD or MEDFORD2"
# endif
#endif /* EFSYS_OPT_ALLOW_UNCONFIGURED_NIC */

#if EFSYS_OPT_RX_PACKED_STREAM
/* Support packed stream mode */
# if !EFX_OPTS_EF10()
#  error "PACKED_STREAM requires EF10 arch"
# endif
#endif

#if EFSYS_OPT_RX_ES_SUPER_BUFFER
/* Support equal stride super-buffer mode */
# if !(EFSYS_OPT_MEDFORD2)
#  error "ES_SUPER_BUFFER requires MEDFORD2"
# endif
#endif

/* Support hardware assistance for tunnels */
#if EFSYS_OPT_TUNNEL
# if !(EFSYS_OPT_RIVERHEAD || EFSYS_OPT_MEDFORD || EFSYS_OPT_MEDFORD2)
#  error "TUNNEL requires RIVERHEAD or MEDFORD or MEDFORD2"
# endif
#endif /* EFSYS_OPT_TUNNEL */

#if EFSYS_OPT_FW_SUBVARIANT_AWARE
/* Advertise that the driver is firmware subvariant aware */
# if !(EFSYS_OPT_MEDFORD2)
#  error "FW_SUBVARIANT_AWARE requires MEDFORD2"
# endif
#endif

#if EFSYS_OPT_EVB
/* Support enterprise virtual bridging */
# if !(EFSYS_OPT_RIVERHEAD || EFX_OPTS_EF10())
#  error "EVB requires RIVERHEAD or EF10 arch"
# endif
#endif /* EFSYS_OPT_EVB */

#if EFSYS_OPT_PCI
# if !EFSYS_OPT_RIVERHEAD
#  error "PCI requires RIVERHEAD"
# endif
#endif /* EFSYS_OPT_PCI */

/* Support extended width event queues */
#if EFSYS_OPT_EV_EXTENDED_WIDTH
# if !EFSYS_OPT_RIVERHEAD
#  error "EV_EXTENDED_WIDTH requires RIVERHEAD"
# endif
#endif /* EFSYS_OPT_EV_EXTENDED_WIDTH */

/* Support descriptor proxy queues */
#if EFSYS_OPT_DESC_PROXY
# if !EFSYS_OPT_RIVERHEAD
#  error "DESC_PROXY requires RIVERHEAD"
# endif
# if !EFSYS_OPT_EV_EXTENDED_WIDTH
#  error "DESC_PROXY requires EV_EXTENDED_WIDTH"
# endif
#endif /* EFSYS_OPT_DESC_PROXY */

#if EFSYS_OPT_MAE
# if !EFSYS_OPT_RIVERHEAD
#  error "MAE requires RIVERHEAD"
# endif
#endif /* EFSYS_OPT_MAE */

#if EFSYS_OPT_VIRTIO
# if !EFSYS_OPT_RIVERHEAD
#  error "VIRTIO requires RIVERHEAD"
# endif
# if !EFSYS_HAS_UINT64
#  error "VIRTIO requires UINT64"
# endif
#endif /* EFSYS_OPT_VIRTIO */

#endif /* _SYS_EFX_CHECK_H */
