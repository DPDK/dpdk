/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */
#ifndef __CN20K_RX_H__
#define __CN20K_RX_H__

#include "cn20k_rxtx.h"
#include <rte_ethdev.h>
#include <rte_security_driver.h>
#include <rte_vect.h>

#define NSEC_PER_SEC 1000000000L

#define NIX_RX_OFFLOAD_NONE	     (0)
#define NIX_RX_OFFLOAD_RSS_F	     BIT(0)
#define NIX_RX_OFFLOAD_PTYPE_F	     BIT(1)
#define NIX_RX_OFFLOAD_CHECKSUM_F    BIT(2)
#define NIX_RX_OFFLOAD_MARK_UPDATE_F BIT(3)
#define NIX_RX_OFFLOAD_TSTAMP_F	     BIT(4)
#define NIX_RX_OFFLOAD_VLAN_STRIP_F  BIT(5)
#define NIX_RX_OFFLOAD_SECURITY_F    BIT(6)
#define NIX_RX_OFFLOAD_MAX	     (NIX_RX_OFFLOAD_SECURITY_F << 1)

/* Flags to control cqe_to_mbuf conversion function.
 * Defining it from backwards to denote its been
 * not used as offload flags to pick function
 */
#define NIX_RX_REAS_F	   BIT(12)
#define NIX_RX_VWQE_F	   BIT(13)
#define NIX_RX_MULTI_SEG_F BIT(14)

#define NIX_RX_SEC_REASSEMBLY_F (NIX_RX_REAS_F | NIX_RX_OFFLOAD_SECURITY_F)

#define RSS_F	  NIX_RX_OFFLOAD_RSS_F
#define PTYPE_F	  NIX_RX_OFFLOAD_PTYPE_F
#define CKSUM_F	  NIX_RX_OFFLOAD_CHECKSUM_F
#define MARK_F	  NIX_RX_OFFLOAD_MARK_UPDATE_F
#define TS_F	  NIX_RX_OFFLOAD_TSTAMP_F
#define RX_VLAN_F NIX_RX_OFFLOAD_VLAN_STRIP_F
#define R_SEC_F	  NIX_RX_OFFLOAD_SECURITY_F

/* [R_SEC_F] [RX_VLAN_F] [TS] [MARK] [CKSUM] [PTYPE] [RSS] */
#define NIX_RX_FASTPATH_MODES_0_15                                                                 \
	R(no_offload, NIX_RX_OFFLOAD_NONE)                                                         \
	R(rss, RSS_F)                                                                              \
	R(ptype, PTYPE_F)                                                                          \
	R(ptype_rss, PTYPE_F | RSS_F)                                                              \
	R(cksum, CKSUM_F)                                                                          \
	R(cksum_rss, CKSUM_F | RSS_F)                                                              \
	R(cksum_ptype, CKSUM_F | PTYPE_F)                                                          \
	R(cksum_ptype_rss, CKSUM_F | PTYPE_F | RSS_F)                                              \
	R(mark, MARK_F)                                                                            \
	R(mark_rss, MARK_F | RSS_F)                                                                \
	R(mark_ptype, MARK_F | PTYPE_F)                                                            \
	R(mark_ptype_rss, MARK_F | PTYPE_F | RSS_F)                                                \
	R(mark_cksum, MARK_F | CKSUM_F)                                                            \
	R(mark_cksum_rss, MARK_F | CKSUM_F | RSS_F)                                                \
	R(mark_cksum_ptype, MARK_F | CKSUM_F | PTYPE_F)                                            \
	R(mark_cksum_ptype_rss, MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES_16_31                                                                \
	R(ts, TS_F)                                                                                \
	R(ts_rss, TS_F | RSS_F)                                                                    \
	R(ts_ptype, TS_F | PTYPE_F)                                                                \
	R(ts_ptype_rss, TS_F | PTYPE_F | RSS_F)                                                    \
	R(ts_cksum, TS_F | CKSUM_F)                                                                \
	R(ts_cksum_rss, TS_F | CKSUM_F | RSS_F)                                                    \
	R(ts_cksum_ptype, TS_F | CKSUM_F | PTYPE_F)                                                \
	R(ts_cksum_ptype_rss, TS_F | CKSUM_F | PTYPE_F | RSS_F)                                    \
	R(ts_mark, TS_F | MARK_F)                                                                  \
	R(ts_mark_rss, TS_F | MARK_F | RSS_F)                                                      \
	R(ts_mark_ptype, TS_F | MARK_F | PTYPE_F)                                                  \
	R(ts_mark_ptype_rss, TS_F | MARK_F | PTYPE_F | RSS_F)                                      \
	R(ts_mark_cksum, TS_F | MARK_F | CKSUM_F)                                                  \
	R(ts_mark_cksum_rss, TS_F | MARK_F | CKSUM_F | RSS_F)                                      \
	R(ts_mark_cksum_ptype, TS_F | MARK_F | CKSUM_F | PTYPE_F)                                  \
	R(ts_mark_cksum_ptype_rss, TS_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES_32_47                                                                \
	R(vlan, RX_VLAN_F)                                                                         \
	R(vlan_rss, RX_VLAN_F | RSS_F)                                                             \
	R(vlan_ptype, RX_VLAN_F | PTYPE_F)                                                         \
	R(vlan_ptype_rss, RX_VLAN_F | PTYPE_F | RSS_F)                                             \
	R(vlan_cksum, RX_VLAN_F | CKSUM_F)                                                         \
	R(vlan_cksum_rss, RX_VLAN_F | CKSUM_F | RSS_F)                                             \
	R(vlan_cksum_ptype, RX_VLAN_F | CKSUM_F | PTYPE_F)                                         \
	R(vlan_cksum_ptype_rss, RX_VLAN_F | CKSUM_F | PTYPE_F | RSS_F)                             \
	R(vlan_mark, RX_VLAN_F | MARK_F)                                                           \
	R(vlan_mark_rss, RX_VLAN_F | MARK_F | RSS_F)                                               \
	R(vlan_mark_ptype, RX_VLAN_F | MARK_F | PTYPE_F)                                           \
	R(vlan_mark_ptype_rss, RX_VLAN_F | MARK_F | PTYPE_F | RSS_F)                               \
	R(vlan_mark_cksum, RX_VLAN_F | MARK_F | CKSUM_F)                                           \
	R(vlan_mark_cksum_rss, RX_VLAN_F | MARK_F | CKSUM_F | RSS_F)                               \
	R(vlan_mark_cksum_ptype, RX_VLAN_F | MARK_F | CKSUM_F | PTYPE_F)                           \
	R(vlan_mark_cksum_ptype_rss, RX_VLAN_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES_48_63                                                                \
	R(vlan_ts, RX_VLAN_F | TS_F)                                                               \
	R(vlan_ts_rss, RX_VLAN_F | TS_F | RSS_F)                                                   \
	R(vlan_ts_ptype, RX_VLAN_F | TS_F | PTYPE_F)                                               \
	R(vlan_ts_ptype_rss, RX_VLAN_F | TS_F | PTYPE_F | RSS_F)                                   \
	R(vlan_ts_cksum, RX_VLAN_F | TS_F | CKSUM_F)                                               \
	R(vlan_ts_cksum_rss, RX_VLAN_F | TS_F | CKSUM_F | RSS_F)                                   \
	R(vlan_ts_cksum_ptype, RX_VLAN_F | TS_F | CKSUM_F | PTYPE_F)                               \
	R(vlan_ts_cksum_ptype_rss, RX_VLAN_F | TS_F | CKSUM_F | PTYPE_F | RSS_F)                   \
	R(vlan_ts_mark, RX_VLAN_F | TS_F | MARK_F)                                                 \
	R(vlan_ts_mark_rss, RX_VLAN_F | TS_F | MARK_F | RSS_F)                                     \
	R(vlan_ts_mark_ptype, RX_VLAN_F | TS_F | MARK_F | PTYPE_F)                                 \
	R(vlan_ts_mark_ptype_rss, RX_VLAN_F | TS_F | MARK_F | PTYPE_F | RSS_F)                     \
	R(vlan_ts_mark_cksum, RX_VLAN_F | TS_F | MARK_F | CKSUM_F)                                 \
	R(vlan_ts_mark_cksum_rss, RX_VLAN_F | TS_F | MARK_F | CKSUM_F | RSS_F)                     \
	R(vlan_ts_mark_cksum_ptype, RX_VLAN_F | TS_F | MARK_F | CKSUM_F | PTYPE_F)                 \
	R(vlan_ts_mark_cksum_ptype_rss, RX_VLAN_F | TS_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES_64_79                                                                \
	R(sec, R_SEC_F)                                                                            \
	R(sec_rss, R_SEC_F | RSS_F)                                                                \
	R(sec_ptype, R_SEC_F | PTYPE_F)                                                            \
	R(sec_ptype_rss, R_SEC_F | PTYPE_F | RSS_F)                                                \
	R(sec_cksum, R_SEC_F | CKSUM_F)                                                            \
	R(sec_cksum_rss, R_SEC_F | CKSUM_F | RSS_F)                                                \
	R(sec_cksum_ptype, R_SEC_F | CKSUM_F | PTYPE_F)                                            \
	R(sec_cksum_ptype_rss, R_SEC_F | CKSUM_F | PTYPE_F | RSS_F)                                \
	R(sec_mark, R_SEC_F | MARK_F)                                                              \
	R(sec_mark_rss, R_SEC_F | MARK_F | RSS_F)                                                  \
	R(sec_mark_ptype, R_SEC_F | MARK_F | PTYPE_F)                                              \
	R(sec_mark_ptype_rss, R_SEC_F | MARK_F | PTYPE_F | RSS_F)                                  \
	R(sec_mark_cksum, R_SEC_F | MARK_F | CKSUM_F)                                              \
	R(sec_mark_cksum_rss, R_SEC_F | MARK_F | CKSUM_F | RSS_F)                                  \
	R(sec_mark_cksum_ptype, R_SEC_F | MARK_F | CKSUM_F | PTYPE_F)                              \
	R(sec_mark_cksum_ptype_rss, R_SEC_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES_80_95                                                                \
	R(sec_ts, R_SEC_F | TS_F)                                                                  \
	R(sec_ts_rss, R_SEC_F | TS_F | RSS_F)                                                      \
	R(sec_ts_ptype, R_SEC_F | TS_F | PTYPE_F)                                                  \
	R(sec_ts_ptype_rss, R_SEC_F | TS_F | PTYPE_F | RSS_F)                                      \
	R(sec_ts_cksum, R_SEC_F | TS_F | CKSUM_F)                                                  \
	R(sec_ts_cksum_rss, R_SEC_F | TS_F | CKSUM_F | RSS_F)                                      \
	R(sec_ts_cksum_ptype, R_SEC_F | TS_F | CKSUM_F | PTYPE_F)                                  \
	R(sec_ts_cksum_ptype_rss, R_SEC_F | TS_F | CKSUM_F | PTYPE_F | RSS_F)                      \
	R(sec_ts_mark, R_SEC_F | TS_F | MARK_F)                                                    \
	R(sec_ts_mark_rss, R_SEC_F | TS_F | MARK_F | RSS_F)                                        \
	R(sec_ts_mark_ptype, R_SEC_F | TS_F | MARK_F | PTYPE_F)                                    \
	R(sec_ts_mark_ptype_rss, R_SEC_F | TS_F | MARK_F | PTYPE_F | RSS_F)                        \
	R(sec_ts_mark_cksum, R_SEC_F | TS_F | MARK_F | CKSUM_F)                                    \
	R(sec_ts_mark_cksum_rss, R_SEC_F | TS_F | MARK_F | CKSUM_F | RSS_F)                        \
	R(sec_ts_mark_cksum_ptype, R_SEC_F | TS_F | MARK_F | CKSUM_F | PTYPE_F)                    \
	R(sec_ts_mark_cksum_ptype_rss, R_SEC_F | TS_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES_96_111                                                               \
	R(sec_vlan, R_SEC_F | RX_VLAN_F)                                                           \
	R(sec_vlan_rss, R_SEC_F | RX_VLAN_F | RSS_F)                                               \
	R(sec_vlan_ptype, R_SEC_F | RX_VLAN_F | PTYPE_F)                                           \
	R(sec_vlan_ptype_rss, R_SEC_F | RX_VLAN_F | PTYPE_F | RSS_F)                               \
	R(sec_vlan_cksum, R_SEC_F | RX_VLAN_F | CKSUM_F)                                           \
	R(sec_vlan_cksum_rss, R_SEC_F | RX_VLAN_F | CKSUM_F | RSS_F)                               \
	R(sec_vlan_cksum_ptype, R_SEC_F | RX_VLAN_F | CKSUM_F | PTYPE_F)                           \
	R(sec_vlan_cksum_ptype_rss, R_SEC_F | RX_VLAN_F | CKSUM_F | PTYPE_F | RSS_F)               \
	R(sec_vlan_mark, R_SEC_F | RX_VLAN_F | MARK_F)                                             \
	R(sec_vlan_mark_rss, R_SEC_F | RX_VLAN_F | MARK_F | RSS_F)                                 \
	R(sec_vlan_mark_ptype, R_SEC_F | RX_VLAN_F | MARK_F | PTYPE_F)                             \
	R(sec_vlan_mark_ptype_rss, R_SEC_F | RX_VLAN_F | MARK_F | PTYPE_F | RSS_F)                 \
	R(sec_vlan_mark_cksum, R_SEC_F | RX_VLAN_F | MARK_F | CKSUM_F)                             \
	R(sec_vlan_mark_cksum_rss, R_SEC_F | RX_VLAN_F | MARK_F | CKSUM_F | RSS_F)                 \
	R(sec_vlan_mark_cksum_ptype, R_SEC_F | RX_VLAN_F | MARK_F | CKSUM_F | PTYPE_F)             \
	R(sec_vlan_mark_cksum_ptype_rss, R_SEC_F | RX_VLAN_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES_112_127                                                              \
	R(sec_vlan_ts, R_SEC_F | RX_VLAN_F | TS_F)                                                 \
	R(sec_vlan_ts_rss, R_SEC_F | RX_VLAN_F | TS_F | RSS_F)                                     \
	R(sec_vlan_ts_ptype, R_SEC_F | RX_VLAN_F | TS_F | PTYPE_F)                                 \
	R(sec_vlan_ts_ptype_rss, R_SEC_F | RX_VLAN_F | TS_F | PTYPE_F | RSS_F)                     \
	R(sec_vlan_ts_cksum, R_SEC_F | RX_VLAN_F | TS_F | CKSUM_F)                                 \
	R(sec_vlan_ts_cksum_rss, R_SEC_F | RX_VLAN_F | TS_F | CKSUM_F | RSS_F)                     \
	R(sec_vlan_ts_cksum_ptype, R_SEC_F | RX_VLAN_F | TS_F | CKSUM_F | PTYPE_F)                 \
	R(sec_vlan_ts_cksum_ptype_rss, R_SEC_F | RX_VLAN_F | TS_F | CKSUM_F | PTYPE_F | RSS_F)     \
	R(sec_vlan_ts_mark, R_SEC_F | RX_VLAN_F | TS_F | MARK_F)                                   \
	R(sec_vlan_ts_mark_rss, R_SEC_F | RX_VLAN_F | TS_F | MARK_F | RSS_F)                       \
	R(sec_vlan_ts_mark_ptype, R_SEC_F | RX_VLAN_F | TS_F | MARK_F | PTYPE_F)                   \
	R(sec_vlan_ts_mark_ptype_rss, R_SEC_F | RX_VLAN_F | TS_F | MARK_F | PTYPE_F | RSS_F)       \
	R(sec_vlan_ts_mark_cksum, R_SEC_F | RX_VLAN_F | TS_F | MARK_F | CKSUM_F)                   \
	R(sec_vlan_ts_mark_cksum_rss, R_SEC_F | RX_VLAN_F | TS_F | MARK_F | CKSUM_F | RSS_F)       \
	R(sec_vlan_ts_mark_cksum_ptype, R_SEC_F | RX_VLAN_F | TS_F | MARK_F | CKSUM_F | PTYPE_F)   \
	R(sec_vlan_ts_mark_cksum_ptype_rss,                                                        \
	  R_SEC_F | RX_VLAN_F | TS_F | MARK_F | CKSUM_F | PTYPE_F | RSS_F)

#define NIX_RX_FASTPATH_MODES                                                                      \
	NIX_RX_FASTPATH_MODES_0_15                                                                 \
	NIX_RX_FASTPATH_MODES_16_31                                                                \
	NIX_RX_FASTPATH_MODES_32_47                                                                \
	NIX_RX_FASTPATH_MODES_48_63                                                                \
	NIX_RX_FASTPATH_MODES_64_79                                                                \
	NIX_RX_FASTPATH_MODES_80_95                                                                \
	NIX_RX_FASTPATH_MODES_96_111                                                               \
	NIX_RX_FASTPATH_MODES_112_127

#define R(name, flags)                                                                             \
	uint16_t __rte_noinline __rte_hot cn20k_nix_recv_pkts_##name(                              \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);                         \
	uint16_t __rte_noinline __rte_hot cn20k_nix_recv_pkts_mseg_##name(                         \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);                         \
	uint16_t __rte_noinline __rte_hot cn20k_nix_recv_pkts_vec_##name(                          \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);                         \
	uint16_t __rte_noinline __rte_hot cn20k_nix_recv_pkts_vec_mseg_##name(                     \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);                         \
	uint16_t __rte_noinline __rte_hot cn20k_nix_recv_pkts_reas_##name(                         \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);                         \
	uint16_t __rte_noinline __rte_hot cn20k_nix_recv_pkts_reas_mseg_##name(                    \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);                         \
	uint16_t __rte_noinline __rte_hot cn20k_nix_recv_pkts_reas_vec_##name(                     \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);                         \
	uint16_t __rte_noinline __rte_hot cn20k_nix_recv_pkts_reas_vec_mseg_##name(                \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts);

NIX_RX_FASTPATH_MODES
#undef R

#define NIX_RX_RECV(fn, flags)                                                                     \
	uint16_t __rte_noinline __rte_hot fn(void *rx_queue, struct rte_mbuf **rx_pkts,            \
					     uint16_t pkts)                                        \
	{                                                                                          \
		RTE_SET_USED(rx_queue);                                                            \
		RTE_SET_USED(rx_pkts);                                                             \
		RTE_SET_USED(pkts);                                                                \
		return 0;                                                                          \
	}

#define NIX_RX_RECV_MSEG(fn, flags) NIX_RX_RECV(fn, flags | NIX_RX_MULTI_SEG_F)

#define NIX_RX_RECV_VEC(fn, flags)                                                                 \
	uint16_t __rte_noinline __rte_hot fn(void *rx_queue, struct rte_mbuf **rx_pkts,            \
					     uint16_t pkts)                                        \
	{                                                                                          \
		RTE_SET_USED(rx_queue);                                                            \
		RTE_SET_USED(rx_pkts);                                                             \
		RTE_SET_USED(pkts);                                                                \
		return 0;                                                                          \
	}

#define NIX_RX_RECV_VEC_MSEG(fn, flags) NIX_RX_RECV_VEC(fn, flags | NIX_RX_MULTI_SEG_F)

uint16_t __rte_noinline __rte_hot cn20k_nix_recv_pkts_all_offload(void *rx_queue,
								  struct rte_mbuf **rx_pkts,
								  uint16_t pkts);

uint16_t __rte_noinline __rte_hot cn20k_nix_recv_pkts_vec_all_offload(void *rx_queue,
								      struct rte_mbuf **rx_pkts,
								      uint16_t pkts);

uint16_t __rte_noinline __rte_hot cn20k_nix_recv_pkts_all_offload_tst(void *rx_queue,
								      struct rte_mbuf **rx_pkts,
								      uint16_t pkts);

uint16_t __rte_noinline __rte_hot cn20k_nix_recv_pkts_vec_all_offload_tst(void *rx_queue,
									  struct rte_mbuf **rx_pkts,
									  uint16_t pkts);

#endif /* __CN20K_RX_H__ */
