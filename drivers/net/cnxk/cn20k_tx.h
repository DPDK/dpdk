/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */
#ifndef __CN20K_TX_H__
#define __CN20K_TX_H__

#include "cn20k_rxtx.h"
#include <rte_eventdev.h>
#include <rte_vect.h>

#define NIX_TX_OFFLOAD_NONE	      (0)
#define NIX_TX_OFFLOAD_L3_L4_CSUM_F   BIT(0)
#define NIX_TX_OFFLOAD_OL3_OL4_CSUM_F BIT(1)
#define NIX_TX_OFFLOAD_VLAN_QINQ_F    BIT(2)
#define NIX_TX_OFFLOAD_MBUF_NOFF_F    BIT(3)
#define NIX_TX_OFFLOAD_TSO_F	      BIT(4)
#define NIX_TX_OFFLOAD_TSTAMP_F	      BIT(5)
#define NIX_TX_OFFLOAD_SECURITY_F     BIT(6)
#define NIX_TX_OFFLOAD_MAX	      (NIX_TX_OFFLOAD_SECURITY_F << 1)

/* Flags to control xmit_prepare function.
 * Defining it from backwards to denote its been
 * not used as offload flags to pick function
 */
#define NIX_TX_VWQE_F	   BIT(14)
#define NIX_TX_MULTI_SEG_F BIT(15)

#define NIX_TX_NEED_SEND_HDR_W1                                                                    \
	(NIX_TX_OFFLOAD_L3_L4_CSUM_F | NIX_TX_OFFLOAD_OL3_OL4_CSUM_F |                             \
	 NIX_TX_OFFLOAD_VLAN_QINQ_F | NIX_TX_OFFLOAD_TSO_F)

#define NIX_TX_NEED_EXT_HDR                                                                        \
	(NIX_TX_OFFLOAD_VLAN_QINQ_F | NIX_TX_OFFLOAD_TSTAMP_F | NIX_TX_OFFLOAD_TSO_F)

#define L3L4CSUM_F   NIX_TX_OFFLOAD_L3_L4_CSUM_F
#define OL3OL4CSUM_F NIX_TX_OFFLOAD_OL3_OL4_CSUM_F
#define VLAN_F	     NIX_TX_OFFLOAD_VLAN_QINQ_F
#define NOFF_F	     NIX_TX_OFFLOAD_MBUF_NOFF_F
#define TSO_F	     NIX_TX_OFFLOAD_TSO_F
#define TSP_F	     NIX_TX_OFFLOAD_TSTAMP_F
#define T_SEC_F	     NIX_TX_OFFLOAD_SECURITY_F

/* [T_SEC_F] [TSP] [TSO] [NOFF] [VLAN] [OL3OL4CSUM] [L3L4CSUM] */
#define NIX_TX_FASTPATH_MODES_0_15                                                                 \
	T(no_offload, 6, NIX_TX_OFFLOAD_NONE)                                                      \
	T(l3l4csum, 6, L3L4CSUM_F)                                                                 \
	T(ol3ol4csum, 6, OL3OL4CSUM_F)                                                             \
	T(ol3ol4csum_l3l4csum, 6, OL3OL4CSUM_F | L3L4CSUM_F)                                       \
	T(vlan, 6, VLAN_F)                                                                         \
	T(vlan_l3l4csum, 6, VLAN_F | L3L4CSUM_F)                                                   \
	T(vlan_ol3ol4csum, 6, VLAN_F | OL3OL4CSUM_F)                                               \
	T(vlan_ol3ol4csum_l3l4csum, 6, VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)                         \
	T(noff, 6, NOFF_F)                                                                         \
	T(noff_l3l4csum, 6, NOFF_F | L3L4CSUM_F)                                                   \
	T(noff_ol3ol4csum, 6, NOFF_F | OL3OL4CSUM_F)                                               \
	T(noff_ol3ol4csum_l3l4csum, 6, NOFF_F | OL3OL4CSUM_F | L3L4CSUM_F)                         \
	T(noff_vlan, 6, NOFF_F | VLAN_F)                                                           \
	T(noff_vlan_l3l4csum, 6, NOFF_F | VLAN_F | L3L4CSUM_F)                                     \
	T(noff_vlan_ol3ol4csum, 6, NOFF_F | VLAN_F | OL3OL4CSUM_F)                                 \
	T(noff_vlan_ol3ol4csum_l3l4csum, 6, NOFF_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)

#define NIX_TX_FASTPATH_MODES_16_31                                                                \
	T(tso, 6, TSO_F)                                                                           \
	T(tso_l3l4csum, 6, TSO_F | L3L4CSUM_F)                                                     \
	T(tso_ol3ol4csum, 6, TSO_F | OL3OL4CSUM_F)                                                 \
	T(tso_ol3ol4csum_l3l4csum, 6, TSO_F | OL3OL4CSUM_F | L3L4CSUM_F)                           \
	T(tso_vlan, 6, TSO_F | VLAN_F)                                                             \
	T(tso_vlan_l3l4csum, 6, TSO_F | VLAN_F | L3L4CSUM_F)                                       \
	T(tso_vlan_ol3ol4csum, 6, TSO_F | VLAN_F | OL3OL4CSUM_F)                                   \
	T(tso_vlan_ol3ol4csum_l3l4csum, 6, TSO_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)             \
	T(tso_noff, 6, TSO_F | NOFF_F)                                                             \
	T(tso_noff_l3l4csum, 6, TSO_F | NOFF_F | L3L4CSUM_F)                                       \
	T(tso_noff_ol3ol4csum, 6, TSO_F | NOFF_F | OL3OL4CSUM_F)                                   \
	T(tso_noff_ol3ol4csum_l3l4csum, 6, TSO_F | NOFF_F | OL3OL4CSUM_F | L3L4CSUM_F)             \
	T(tso_noff_vlan, 6, TSO_F | NOFF_F | VLAN_F)                                               \
	T(tso_noff_vlan_l3l4csum, 6, TSO_F | NOFF_F | VLAN_F | L3L4CSUM_F)                         \
	T(tso_noff_vlan_ol3ol4csum, 6, TSO_F | NOFF_F | VLAN_F | OL3OL4CSUM_F)                     \
	T(tso_noff_vlan_ol3ol4csum_l3l4csum, 6, TSO_F | NOFF_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)

#define NIX_TX_FASTPATH_MODES_32_47                                                                \
	T(ts, 8, TSP_F)                                                                            \
	T(ts_l3l4csum, 8, TSP_F | L3L4CSUM_F)                                                      \
	T(ts_ol3ol4csum, 8, TSP_F | OL3OL4CSUM_F)                                                  \
	T(ts_ol3ol4csum_l3l4csum, 8, TSP_F | OL3OL4CSUM_F | L3L4CSUM_F)                            \
	T(ts_vlan, 8, TSP_F | VLAN_F)                                                              \
	T(ts_vlan_l3l4csum, 8, TSP_F | VLAN_F | L3L4CSUM_F)                                        \
	T(ts_vlan_ol3ol4csum, 8, TSP_F | VLAN_F | OL3OL4CSUM_F)                                    \
	T(ts_vlan_ol3ol4csum_l3l4csum, 8, TSP_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)              \
	T(ts_noff, 8, TSP_F | NOFF_F)                                                              \
	T(ts_noff_l3l4csum, 8, TSP_F | NOFF_F | L3L4CSUM_F)                                        \
	T(ts_noff_ol3ol4csum, 8, TSP_F | NOFF_F | OL3OL4CSUM_F)                                    \
	T(ts_noff_ol3ol4csum_l3l4csum, 8, TSP_F | NOFF_F | OL3OL4CSUM_F | L3L4CSUM_F)              \
	T(ts_noff_vlan, 8, TSP_F | NOFF_F | VLAN_F)                                                \
	T(ts_noff_vlan_l3l4csum, 8, TSP_F | NOFF_F | VLAN_F | L3L4CSUM_F)                          \
	T(ts_noff_vlan_ol3ol4csum, 8, TSP_F | NOFF_F | VLAN_F | OL3OL4CSUM_F)                      \
	T(ts_noff_vlan_ol3ol4csum_l3l4csum, 8, TSP_F | NOFF_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)

#define NIX_TX_FASTPATH_MODES_48_63                                                                \
	T(ts_tso, 8, TSP_F | TSO_F)                                                                \
	T(ts_tso_l3l4csum, 8, TSP_F | TSO_F | L3L4CSUM_F)                                          \
	T(ts_tso_ol3ol4csum, 8, TSP_F | TSO_F | OL3OL4CSUM_F)                                      \
	T(ts_tso_ol3ol4csum_l3l4csum, 8, TSP_F | TSO_F | OL3OL4CSUM_F | L3L4CSUM_F)                \
	T(ts_tso_vlan, 8, TSP_F | TSO_F | VLAN_F)                                                  \
	T(ts_tso_vlan_l3l4csum, 8, TSP_F | TSO_F | VLAN_F | L3L4CSUM_F)                            \
	T(ts_tso_vlan_ol3ol4csum, 8, TSP_F | TSO_F | VLAN_F | OL3OL4CSUM_F)                        \
	T(ts_tso_vlan_ol3ol4csum_l3l4csum, 8, TSP_F | TSO_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)  \
	T(ts_tso_noff, 8, TSP_F | TSO_F | NOFF_F)                                                  \
	T(ts_tso_noff_l3l4csum, 8, TSP_F | TSO_F | NOFF_F | L3L4CSUM_F)                            \
	T(ts_tso_noff_ol3ol4csum, 8, TSP_F | TSO_F | NOFF_F | OL3OL4CSUM_F)                        \
	T(ts_tso_noff_ol3ol4csum_l3l4csum, 8, TSP_F | TSO_F | NOFF_F | OL3OL4CSUM_F | L3L4CSUM_F)  \
	T(ts_tso_noff_vlan, 8, TSP_F | TSO_F | NOFF_F | VLAN_F)                                    \
	T(ts_tso_noff_vlan_l3l4csum, 8, TSP_F | TSO_F | NOFF_F | VLAN_F | L3L4CSUM_F)              \
	T(ts_tso_noff_vlan_ol3ol4csum, 8, TSP_F | TSO_F | NOFF_F | VLAN_F | OL3OL4CSUM_F)          \
	T(ts_tso_noff_vlan_ol3ol4csum_l3l4csum, 8,                                                 \
	  TSP_F | TSO_F | NOFF_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)

#define NIX_TX_FASTPATH_MODES_64_79                                                                \
	T(sec, 6, T_SEC_F)                                                                         \
	T(sec_l3l4csum, 6, T_SEC_F | L3L4CSUM_F)                                                   \
	T(sec_ol3ol4csum, 6, T_SEC_F | OL3OL4CSUM_F)                                               \
	T(sec_ol3ol4csum_l3l4csum, 6, T_SEC_F | OL3OL4CSUM_F | L3L4CSUM_F)                         \
	T(sec_vlan, 6, T_SEC_F | VLAN_F)                                                           \
	T(sec_vlan_l3l4csum, 6, T_SEC_F | VLAN_F | L3L4CSUM_F)                                     \
	T(sec_vlan_ol3ol4csum, 6, T_SEC_F | VLAN_F | OL3OL4CSUM_F)                                 \
	T(sec_vlan_ol3ol4csum_l3l4csum, 6, T_SEC_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)           \
	T(sec_noff, 6, T_SEC_F | NOFF_F)                                                           \
	T(sec_noff_l3l4csum, 6, T_SEC_F | NOFF_F | L3L4CSUM_F)                                     \
	T(sec_noff_ol3ol4csum, 6, T_SEC_F | NOFF_F | OL3OL4CSUM_F)                                 \
	T(sec_noff_ol3ol4csum_l3l4csum, 6, T_SEC_F | NOFF_F | OL3OL4CSUM_F | L3L4CSUM_F)           \
	T(sec_noff_vlan, 6, T_SEC_F | NOFF_F | VLAN_F)                                             \
	T(sec_noff_vlan_l3l4csum, 6, T_SEC_F | NOFF_F | VLAN_F | L3L4CSUM_F)                       \
	T(sec_noff_vlan_ol3ol4csum, 6, T_SEC_F | NOFF_F | VLAN_F | OL3OL4CSUM_F)                   \
	T(sec_noff_vlan_ol3ol4csum_l3l4csum, 6,                                                    \
	  T_SEC_F | NOFF_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)

#define NIX_TX_FASTPATH_MODES_80_95                                                                \
	T(sec_tso, 6, T_SEC_F | TSO_F)                                                             \
	T(sec_tso_l3l4csum, 6, T_SEC_F | TSO_F | L3L4CSUM_F)                                       \
	T(sec_tso_ol3ol4csum, 6, T_SEC_F | TSO_F | OL3OL4CSUM_F)                                   \
	T(sec_tso_ol3ol4csum_l3l4csum, 6, T_SEC_F | TSO_F | OL3OL4CSUM_F | L3L4CSUM_F)             \
	T(sec_tso_vlan, 6, T_SEC_F | TSO_F | VLAN_F)                                               \
	T(sec_tso_vlan_l3l4csum, 6, T_SEC_F | TSO_F | VLAN_F | L3L4CSUM_F)                         \
	T(sec_tso_vlan_ol3ol4csum, 6, T_SEC_F | TSO_F | VLAN_F | OL3OL4CSUM_F)                     \
	T(sec_tso_vlan_ol3ol4csum_l3l4csum, 6,                                                     \
	  T_SEC_F | TSO_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)                                    \
	T(sec_tso_noff, 6, T_SEC_F | TSO_F | NOFF_F)                                               \
	T(sec_tso_noff_l3l4csum, 6, T_SEC_F | TSO_F | NOFF_F | L3L4CSUM_F)                         \
	T(sec_tso_noff_ol3ol4csum, 6, T_SEC_F | TSO_F | NOFF_F | OL3OL4CSUM_F)                     \
	T(sec_tso_noff_ol3ol4csum_l3l4csum, 6,                                                     \
	  T_SEC_F | TSO_F | NOFF_F | OL3OL4CSUM_F | L3L4CSUM_F)                                    \
	T(sec_tso_noff_vlan, 6, T_SEC_F | TSO_F | NOFF_F | VLAN_F)                                 \
	T(sec_tso_noff_vlan_l3l4csum, 6, T_SEC_F | TSO_F | NOFF_F | VLAN_F | L3L4CSUM_F)           \
	T(sec_tso_noff_vlan_ol3ol4csum, 6, T_SEC_F | TSO_F | NOFF_F | VLAN_F | OL3OL4CSUM_F)       \
	T(sec_tso_noff_vlan_ol3ol4csum_l3l4csum, 6,                                                \
	  T_SEC_F | TSO_F | NOFF_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)

#define NIX_TX_FASTPATH_MODES_96_111                                                               \
	T(sec_ts, 8, T_SEC_F | TSP_F)                                                              \
	T(sec_ts_l3l4csum, 8, T_SEC_F | TSP_F | L3L4CSUM_F)                                        \
	T(sec_ts_ol3ol4csum, 8, T_SEC_F | TSP_F | OL3OL4CSUM_F)                                    \
	T(sec_ts_ol3ol4csum_l3l4csum, 8, T_SEC_F | TSP_F | OL3OL4CSUM_F | L3L4CSUM_F)              \
	T(sec_ts_vlan, 8, T_SEC_F | TSP_F | VLAN_F)                                                \
	T(sec_ts_vlan_l3l4csum, 8, T_SEC_F | TSP_F | VLAN_F | L3L4CSUM_F)                          \
	T(sec_ts_vlan_ol3ol4csum, 8, T_SEC_F | TSP_F | VLAN_F | OL3OL4CSUM_F)                      \
	T(sec_ts_vlan_ol3ol4csum_l3l4csum, 8,                                                      \
	  T_SEC_F | TSP_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)                                    \
	T(sec_ts_noff, 8, T_SEC_F | TSP_F | NOFF_F)                                                \
	T(sec_ts_noff_l3l4csum, 8, T_SEC_F | TSP_F | NOFF_F | L3L4CSUM_F)                          \
	T(sec_ts_noff_ol3ol4csum, 8, T_SEC_F | TSP_F | NOFF_F | OL3OL4CSUM_F)                      \
	T(sec_ts_noff_ol3ol4csum_l3l4csum, 8,                                                      \
	  T_SEC_F | TSP_F | NOFF_F | OL3OL4CSUM_F | L3L4CSUM_F)                                    \
	T(sec_ts_noff_vlan, 8, T_SEC_F | TSP_F | NOFF_F | VLAN_F)                                  \
	T(sec_ts_noff_vlan_l3l4csum, 8, T_SEC_F | TSP_F | NOFF_F | VLAN_F | L3L4CSUM_F)            \
	T(sec_ts_noff_vlan_ol3ol4csum, 8, T_SEC_F | TSP_F | NOFF_F | VLAN_F | OL3OL4CSUM_F)        \
	T(sec_ts_noff_vlan_ol3ol4csum_l3l4csum, 8,                                                 \
	  T_SEC_F | TSP_F | NOFF_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)

#define NIX_TX_FASTPATH_MODES_112_127                                                              \
	T(sec_ts_tso, 8, T_SEC_F | TSP_F | TSO_F)                                                  \
	T(sec_ts_tso_l3l4csum, 8, T_SEC_F | TSP_F | TSO_F | L3L4CSUM_F)                            \
	T(sec_ts_tso_ol3ol4csum, 8, T_SEC_F | TSP_F | TSO_F | OL3OL4CSUM_F)                        \
	T(sec_ts_tso_ol3ol4csum_l3l4csum, 8, T_SEC_F | TSP_F | TSO_F | OL3OL4CSUM_F | L3L4CSUM_F)  \
	T(sec_ts_tso_vlan, 8, T_SEC_F | TSP_F | TSO_F | VLAN_F)                                    \
	T(sec_ts_tso_vlan_l3l4csum, 8, T_SEC_F | TSP_F | TSO_F | VLAN_F | L3L4CSUM_F)              \
	T(sec_ts_tso_vlan_ol3ol4csum, 8, T_SEC_F | TSP_F | TSO_F | VLAN_F | OL3OL4CSUM_F)          \
	T(sec_ts_tso_vlan_ol3ol4csum_l3l4csum, 8,                                                  \
	  T_SEC_F | TSP_F | TSO_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)                            \
	T(sec_ts_tso_noff, 8, T_SEC_F | TSP_F | TSO_F | NOFF_F)                                    \
	T(sec_ts_tso_noff_l3l4csum, 8, T_SEC_F | TSP_F | TSO_F | NOFF_F | L3L4CSUM_F)              \
	T(sec_ts_tso_noff_ol3ol4csum, 8, T_SEC_F | TSP_F | TSO_F | NOFF_F | OL3OL4CSUM_F)          \
	T(sec_ts_tso_noff_ol3ol4csum_l3l4csum, 8,                                                  \
	  T_SEC_F | TSP_F | TSO_F | NOFF_F | OL3OL4CSUM_F | L3L4CSUM_F)                            \
	T(sec_ts_tso_noff_vlan, 8, T_SEC_F | TSP_F | TSO_F | NOFF_F | VLAN_F)                      \
	T(sec_ts_tso_noff_vlan_l3l4csum, 8,                                                        \
	  T_SEC_F | TSP_F | TSO_F | NOFF_F | VLAN_F | L3L4CSUM_F)                                  \
	T(sec_ts_tso_noff_vlan_ol3ol4csum, 8,                                                      \
	  T_SEC_F | TSP_F | TSO_F | NOFF_F | VLAN_F | OL3OL4CSUM_F)                                \
	T(sec_ts_tso_noff_vlan_ol3ol4csum_l3l4csum, 8,                                             \
	  T_SEC_F | TSP_F | TSO_F | NOFF_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)

#define NIX_TX_FASTPATH_MODES                                                                      \
	NIX_TX_FASTPATH_MODES_0_15                                                                 \
	NIX_TX_FASTPATH_MODES_16_31                                                                \
	NIX_TX_FASTPATH_MODES_32_47                                                                \
	NIX_TX_FASTPATH_MODES_48_63                                                                \
	NIX_TX_FASTPATH_MODES_64_79                                                                \
	NIX_TX_FASTPATH_MODES_80_95                                                                \
	NIX_TX_FASTPATH_MODES_96_111                                                               \
	NIX_TX_FASTPATH_MODES_112_127

#define T(name, sz, flags)                                                                         \
	uint16_t __rte_noinline __rte_hot cn20k_nix_xmit_pkts_##name(                              \
		void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t pkts);                         \
	uint16_t __rte_noinline __rte_hot cn20k_nix_xmit_pkts_mseg_##name(                         \
		void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t pkts);                         \
	uint16_t __rte_noinline __rte_hot cn20k_nix_xmit_pkts_vec_##name(                          \
		void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t pkts);                         \
	uint16_t __rte_noinline __rte_hot cn20k_nix_xmit_pkts_vec_mseg_##name(                     \
		void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t pkts);

NIX_TX_FASTPATH_MODES
#undef T

#define NIX_TX_XMIT(fn, sz, flags)                                                                 \
	uint16_t __rte_noinline __rte_hot fn(void *tx_queue, struct rte_mbuf **tx_pkts,            \
					     uint16_t pkts)                                        \
	{                                                                                          \
		RTE_SET_USED(tx_queue);                                                            \
		RTE_SET_USED(tx_pkts);                                                             \
		RTE_SET_USED(pkts);                                                                \
		return 0;                                                                          \
	}

#define NIX_TX_XMIT_MSEG(fn, sz, flags)                                                            \
	uint16_t __rte_noinline __rte_hot fn(void *tx_queue, struct rte_mbuf **tx_pkts,            \
					     uint16_t pkts)                                        \
	{                                                                                          \
		RTE_SET_USED(tx_queue);                                                            \
		RTE_SET_USED(tx_pkts);                                                             \
		RTE_SET_USED(pkts);                                                                \
		return 0;                                                                          \
	}

#define NIX_TX_XMIT_VEC(fn, sz, flags)                                                             \
	uint16_t __rte_noinline __rte_hot fn(void *tx_queue, struct rte_mbuf **tx_pkts,            \
					     uint16_t pkts)                                        \
	{                                                                                          \
		RTE_SET_USED(tx_queue);                                                            \
		RTE_SET_USED(tx_pkts);                                                             \
		RTE_SET_USED(pkts);                                                                \
		return 0;                                                                          \
	}

#define NIX_TX_XMIT_VEC_MSEG(fn, sz, flags)                                                        \
	uint16_t __rte_noinline __rte_hot fn(void *tx_queue, struct rte_mbuf **tx_pkts,            \
					     uint16_t pkts)                                        \
	{                                                                                          \
		RTE_SET_USED(tx_queue);                                                            \
		RTE_SET_USED(tx_pkts);                                                             \
		RTE_SET_USED(pkts);                                                                \
		return 0;                                                                          \
	}

uint16_t __rte_noinline __rte_hot cn20k_nix_xmit_pkts_all_offload(void *tx_queue,
								  struct rte_mbuf **tx_pkts,
								  uint16_t pkts);

uint16_t __rte_noinline __rte_hot cn20k_nix_xmit_pkts_vec_all_offload(void *tx_queue,
								      struct rte_mbuf **tx_pkts,
								      uint16_t pkts);

#endif /* __CN20K_TX_H__ */
