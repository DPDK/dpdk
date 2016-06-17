/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium networks Ltd. 2016.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Cavium networks nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _THUNDERX_NICVF_HW_DEFS_H
#define _THUNDERX_NICVF_HW_DEFS_H

#include <stdint.h>
#include <stdbool.h>

/* Virtual function register offsets */

#define NIC_VF_CFG                      (0x000020)
#define NIC_VF_PF_MAILBOX_0_1           (0x000130)
#define NIC_VF_INT                      (0x000200)
#define NIC_VF_INT_W1S                  (0x000220)
#define NIC_VF_ENA_W1C                  (0x000240)
#define NIC_VF_ENA_W1S                  (0x000260)

#define NIC_VNIC_RSS_CFG                (0x0020E0)
#define NIC_VNIC_RSS_KEY_0_4            (0x002200)
#define NIC_VNIC_TX_STAT_0_4            (0x004000)
#define NIC_VNIC_RX_STAT_0_13           (0x004100)
#define NIC_VNIC_RQ_GEN_CFG             (0x010010)

#define NIC_QSET_CQ_0_7_CFG             (0x010400)
#define NIC_QSET_CQ_0_7_CFG2            (0x010408)
#define NIC_QSET_CQ_0_7_THRESH          (0x010410)
#define NIC_QSET_CQ_0_7_BASE            (0x010420)
#define NIC_QSET_CQ_0_7_HEAD            (0x010428)
#define NIC_QSET_CQ_0_7_TAIL            (0x010430)
#define NIC_QSET_CQ_0_7_DOOR            (0x010438)
#define NIC_QSET_CQ_0_7_STATUS          (0x010440)
#define NIC_QSET_CQ_0_7_STATUS2         (0x010448)
#define NIC_QSET_CQ_0_7_DEBUG           (0x010450)

#define NIC_QSET_RQ_0_7_CFG             (0x010600)
#define NIC_QSET_RQ_0_7_STATUS0         (0x010700)
#define NIC_QSET_RQ_0_7_STATUS1         (0x010708)

#define NIC_QSET_SQ_0_7_CFG             (0x010800)
#define NIC_QSET_SQ_0_7_THRESH          (0x010810)
#define NIC_QSET_SQ_0_7_BASE            (0x010820)
#define NIC_QSET_SQ_0_7_HEAD            (0x010828)
#define NIC_QSET_SQ_0_7_TAIL            (0x010830)
#define NIC_QSET_SQ_0_7_DOOR            (0x010838)
#define NIC_QSET_SQ_0_7_STATUS          (0x010840)
#define NIC_QSET_SQ_0_7_DEBUG           (0x010848)
#define NIC_QSET_SQ_0_7_STATUS0         (0x010900)
#define NIC_QSET_SQ_0_7_STATUS1         (0x010908)

#define NIC_QSET_RBDR_0_1_CFG           (0x010C00)
#define NIC_QSET_RBDR_0_1_THRESH        (0x010C10)
#define NIC_QSET_RBDR_0_1_BASE          (0x010C20)
#define NIC_QSET_RBDR_0_1_HEAD          (0x010C28)
#define NIC_QSET_RBDR_0_1_TAIL          (0x010C30)
#define NIC_QSET_RBDR_0_1_DOOR          (0x010C38)
#define NIC_QSET_RBDR_0_1_STATUS0       (0x010C40)
#define NIC_QSET_RBDR_0_1_STATUS1       (0x010C48)
#define NIC_QSET_RBDR_0_1_PRFCH_STATUS  (0x010C50)

/* vNIC HW Constants */

#define NIC_Q_NUM_SHIFT                 18

#define MAX_QUEUE_SET                   128
#define MAX_RCV_QUEUES_PER_QS           8
#define MAX_RCV_BUF_DESC_RINGS_PER_QS   2
#define MAX_SND_QUEUES_PER_QS           8
#define MAX_CMP_QUEUES_PER_QS           8

#define NICVF_INTR_CQ_SHIFT             0
#define NICVF_INTR_SQ_SHIFT             8
#define NICVF_INTR_RBDR_SHIFT           16
#define NICVF_INTR_PKT_DROP_SHIFT       20
#define NICVF_INTR_TCP_TIMER_SHIFT      21
#define NICVF_INTR_MBOX_SHIFT           22
#define NICVF_INTR_QS_ERR_SHIFT         23

#define NICVF_INTR_CQ_MASK              (0xFF << NICVF_INTR_CQ_SHIFT)
#define NICVF_INTR_SQ_MASK              (0xFF << NICVF_INTR_SQ_SHIFT)
#define NICVF_INTR_RBDR_MASK            (0x03 << NICVF_INTR_RBDR_SHIFT)
#define NICVF_INTR_PKT_DROP_MASK        (1 << NICVF_INTR_PKT_DROP_SHIFT)
#define NICVF_INTR_TCP_TIMER_MASK       (1 << NICVF_INTR_TCP_TIMER_SHIFT)
#define NICVF_INTR_MBOX_MASK            (1 << NICVF_INTR_MBOX_SHIFT)
#define NICVF_INTR_QS_ERR_MASK          (1 << NICVF_INTR_QS_ERR_SHIFT)
#define NICVF_INTR_ALL_MASK             (0x7FFFFF)

#define NICVF_CQ_WR_FULL                (1ULL << 26)
#define NICVF_CQ_WR_DISABLE             (1ULL << 25)
#define NICVF_CQ_WR_FAULT               (1ULL << 24)
#define NICVF_CQ_ERR_MASK               (NICVF_CQ_WR_FULL |\
					 NICVF_CQ_WR_DISABLE |\
					 NICVF_CQ_WR_FAULT)
#define NICVF_CQ_CQE_COUNT_MASK         (0xFFFF)

#define NICVF_SQ_ERR_STOPPED            (1ULL << 21)
#define NICVF_SQ_ERR_SEND               (1ULL << 20)
#define NICVF_SQ_ERR_DPE                (1ULL << 19)
#define NICVF_SQ_ERR_MASK               (NICVF_SQ_ERR_STOPPED |\
					 NICVF_SQ_ERR_SEND |\
					 NICVF_SQ_ERR_DPE)
#define NICVF_SQ_STATUS_STOPPED_BIT     (21)

#define NICVF_RBDR_FIFO_STATE_SHIFT     (62)
#define NICVF_RBDR_FIFO_STATE_MASK      (3ULL << NICVF_RBDR_FIFO_STATE_SHIFT)
#define NICVF_RBDR_COUNT_MASK           (0x7FFFF)

/* Queue reset */
#define NICVF_CQ_RESET                  (1ULL << 41)
#define NICVF_SQ_RESET                  (1ULL << 17)
#define NICVF_RBDR_RESET                (1ULL << 43)

/* RSS constants */
#define NIC_MAX_RSS_HASH_BITS           (8)
#define NIC_MAX_RSS_IDR_TBL_SIZE        (1 << NIC_MAX_RSS_HASH_BITS)
#define RSS_HASH_KEY_SIZE               (5) /* 320 bit key */
#define RSS_HASH_KEY_BYTE_SIZE          (40) /* 320 bit key */

#define RSS_L2_EXTENDED_HASH_ENA        (1 << 0)
#define RSS_IP_ENA                      (1 << 1)
#define RSS_TCP_ENA                     (1 << 2)
#define RSS_TCP_SYN_ENA                 (1 << 3)
#define RSS_UDP_ENA                     (1 << 4)
#define RSS_L4_EXTENDED_ENA             (1 << 5)
#define RSS_L3_BI_DIRECTION_ENA         (1 << 7)
#define RSS_L4_BI_DIRECTION_ENA         (1 << 8)
#define RSS_TUN_VXLAN_ENA               (1 << 9)
#define RSS_TUN_GENEVE_ENA              (1 << 10)
#define RSS_TUN_NVGRE_ENA               (1 << 11)

#define RBDR_QUEUE_SZ_8K                (8 * 1024)
#define RBDR_QUEUE_SZ_16K               (16 * 1024)
#define RBDR_QUEUE_SZ_32K               (32 * 1024)
#define RBDR_QUEUE_SZ_64K               (64 * 1024)
#define RBDR_QUEUE_SZ_128K              (128 * 1024)
#define RBDR_QUEUE_SZ_256K              (256 * 1024)
#define RBDR_QUEUE_SZ_512K              (512 * 1024)

#define RBDR_SIZE_SHIFT                 (13) /* 8k */

#define SND_QUEUE_SZ_1K                 (1 * 1024)
#define SND_QUEUE_SZ_2K                 (2 * 1024)
#define SND_QUEUE_SZ_4K                 (4 * 1024)
#define SND_QUEUE_SZ_8K                 (8 * 1024)
#define SND_QUEUE_SZ_16K                (16 * 1024)
#define SND_QUEUE_SZ_32K                (32 * 1024)
#define SND_QUEUE_SZ_64K                (64 * 1024)

#define SND_QSIZE_SHIFT                 (10) /* 1k */

#define CMP_QUEUE_SZ_1K                 (1 * 1024)
#define CMP_QUEUE_SZ_2K                 (2 * 1024)
#define CMP_QUEUE_SZ_4K                 (4 * 1024)
#define CMP_QUEUE_SZ_8K                 (8 * 1024)
#define CMP_QUEUE_SZ_16K                (16 * 1024)
#define CMP_QUEUE_SZ_32K                (32 * 1024)
#define CMP_QUEUE_SZ_64K                (64 * 1024)

#define CMP_QSIZE_SHIFT                 (10) /* 1k */

#define NICVF_QSIZE_MIN_VAL             (0)
#define NICVF_QSIZE_MAX_VAL             (6)

/* Min/Max packet size */
#define NIC_HW_MIN_FRS                  (64)
#define NIC_HW_MAX_FRS                  (9200) /* 9216 max pkt including FCS */
#define NIC_HW_MAX_SEGS                 (12)

/* Descriptor alignments */
#define NICVF_RBDR_BASE_ALIGN_BYTES     (128) /* 7 bits */
#define NICVF_CQ_BASE_ALIGN_BYTES       (512) /* 9 bits */
#define NICVF_SQ_BASE_ALIGN_BYTES       (128) /* 7 bits */

#define NICVF_CQE_RBPTR_WORD            (6)
#define NICVF_CQE_RX2_RBPTR_WORD        (7)

#define NICVF_STATIC_ASSERT(s) _Static_assert(s, #s)

typedef uint64_t nicvf_phys_addr_t;

#ifndef __BYTE_ORDER__
#error __BYTE_ORDER__ not defined
#endif

/* vNIC HW Enumerations */

enum nic_send_ld_type_e {
	NIC_SEND_LD_TYPE_E_LDD,
	NIC_SEND_LD_TYPE_E_LDT,
	NIC_SEND_LD_TYPE_E_LDWB,
	NIC_SEND_LD_TYPE_E_ENUM_LAST,
};

enum ether_type_algorithm {
	ETYPE_ALG_NONE,
	ETYPE_ALG_SKIP,
	ETYPE_ALG_ENDPARSE,
	ETYPE_ALG_VLAN,
	ETYPE_ALG_VLAN_STRIP,
};

enum layer3_type {
	L3TYPE_NONE,
	L3TYPE_GRH,
	L3TYPE_IPV4 = 0x4,
	L3TYPE_IPV4_OPTIONS = 0x5,
	L3TYPE_IPV6 = 0x6,
	L3TYPE_IPV6_OPTIONS = 0x7,
	L3TYPE_ET_STOP = 0xD,
	L3TYPE_OTHER = 0xE,
};

#define NICVF_L3TYPE_OPTIONS_MASK	((uint8_t)1)
#define NICVF_L3TYPE_IPVX_MASK		((uint8_t)0x06)

enum layer4_type {
	L4TYPE_NONE,
	L4TYPE_IPSEC_ESP,
	L4TYPE_IPFRAG,
	L4TYPE_IPCOMP,
	L4TYPE_TCP,
	L4TYPE_UDP,
	L4TYPE_SCTP,
	L4TYPE_GRE,
	L4TYPE_ROCE_BTH,
	L4TYPE_OTHER = 0xE,
};

/* CPI and RSSI configuration */
enum cpi_algorithm_type {
	CPI_ALG_NONE,
	CPI_ALG_VLAN,
	CPI_ALG_VLAN16,
	CPI_ALG_DIFF,
};

enum rss_algorithm_type {
	RSS_ALG_NONE,
	RSS_ALG_PORT,
	RSS_ALG_IP,
	RSS_ALG_TCP_IP,
	RSS_ALG_UDP_IP,
	RSS_ALG_SCTP_IP,
	RSS_ALG_GRE_IP,
	RSS_ALG_ROCE,
};

enum rss_hash_cfg {
	RSS_HASH_L2ETC,
	RSS_HASH_IP,
	RSS_HASH_TCP,
	RSS_HASH_TCP_SYN_DIS,
	RSS_HASH_UDP,
	RSS_HASH_L4ETC,
	RSS_HASH_ROCE,
	RSS_L3_BIDI,
	RSS_L4_BIDI,
};

/* Completion queue entry types */
enum cqe_type {
	CQE_TYPE_INVALID,
	CQE_TYPE_RX = 0x2,
	CQE_TYPE_RX_SPLIT = 0x3,
	CQE_TYPE_RX_TCP = 0x4,
	CQE_TYPE_SEND = 0x8,
	CQE_TYPE_SEND_PTP = 0x9,
};

enum cqe_rx_tcp_status {
	CQE_RX_STATUS_VALID_TCP_CNXT,
	CQE_RX_STATUS_INVALID_TCP_CNXT = 0x0F,
};

enum cqe_send_status {
	CQE_SEND_STATUS_GOOD,
	CQE_SEND_STATUS_DESC_FAULT = 0x01,
	CQE_SEND_STATUS_HDR_CONS_ERR = 0x11,
	CQE_SEND_STATUS_SUBDESC_ERR = 0x12,
	CQE_SEND_STATUS_IMM_SIZE_OFLOW = 0x80,
	CQE_SEND_STATUS_CRC_SEQ_ERR = 0x81,
	CQE_SEND_STATUS_DATA_SEQ_ERR = 0x82,
	CQE_SEND_STATUS_MEM_SEQ_ERR = 0x83,
	CQE_SEND_STATUS_LOCK_VIOL = 0x84,
	CQE_SEND_STATUS_LOCK_UFLOW = 0x85,
	CQE_SEND_STATUS_DATA_FAULT = 0x86,
	CQE_SEND_STATUS_TSTMP_CONFLICT = 0x87,
	CQE_SEND_STATUS_TSTMP_TIMEOUT = 0x88,
	CQE_SEND_STATUS_MEM_FAULT = 0x89,
	CQE_SEND_STATUS_CSUM_OVERLAP = 0x8A,
	CQE_SEND_STATUS_CSUM_OVERFLOW = 0x8B,
};

enum cqe_rx_tcp_end_reason {
	CQE_RX_TCP_END_FIN_FLAG_DET,
	CQE_RX_TCP_END_INVALID_FLAG,
	CQE_RX_TCP_END_TIMEOUT,
	CQE_RX_TCP_END_OUT_OF_SEQ,
	CQE_RX_TCP_END_PKT_ERR,
	CQE_RX_TCP_END_QS_DISABLED = 0x0F,
};

/* Packet protocol level error enumeration */
enum cqe_rx_err_level {
	CQE_RX_ERRLVL_RE,
	CQE_RX_ERRLVL_L2,
	CQE_RX_ERRLVL_L3,
	CQE_RX_ERRLVL_L4,
};

/* Packet protocol level error type enumeration */
enum cqe_rx_err_opcode {
	CQE_RX_ERR_RE_NONE,
	CQE_RX_ERR_RE_PARTIAL,
	CQE_RX_ERR_RE_JABBER,
	CQE_RX_ERR_RE_FCS = 0x7,
	CQE_RX_ERR_RE_TERMINATE = 0x9,
	CQE_RX_ERR_RE_RX_CTL = 0xb,
	CQE_RX_ERR_PREL2_ERR = 0x1f,
	CQE_RX_ERR_L2_FRAGMENT = 0x20,
	CQE_RX_ERR_L2_OVERRUN = 0x21,
	CQE_RX_ERR_L2_PFCS = 0x22,
	CQE_RX_ERR_L2_PUNY = 0x23,
	CQE_RX_ERR_L2_MAL = 0x24,
	CQE_RX_ERR_L2_OVERSIZE = 0x25,
	CQE_RX_ERR_L2_UNDERSIZE = 0x26,
	CQE_RX_ERR_L2_LENMISM = 0x27,
	CQE_RX_ERR_L2_PCLP = 0x28,
	CQE_RX_ERR_IP_NOT = 0x41,
	CQE_RX_ERR_IP_CHK = 0x42,
	CQE_RX_ERR_IP_MAL = 0x43,
	CQE_RX_ERR_IP_MALD = 0x44,
	CQE_RX_ERR_IP_HOP = 0x45,
	CQE_RX_ERR_L3_ICRC = 0x46,
	CQE_RX_ERR_L3_PCLP = 0x47,
	CQE_RX_ERR_L4_MAL = 0x61,
	CQE_RX_ERR_L4_CHK = 0x62,
	CQE_RX_ERR_UDP_LEN = 0x63,
	CQE_RX_ERR_L4_PORT = 0x64,
	CQE_RX_ERR_TCP_FLAG = 0x65,
	CQE_RX_ERR_TCP_OFFSET = 0x66,
	CQE_RX_ERR_L4_PCLP = 0x67,
	CQE_RX_ERR_RBDR_TRUNC = 0x70,
};

enum send_l4_csum_type {
	SEND_L4_CSUM_DISABLE,
	SEND_L4_CSUM_UDP,
	SEND_L4_CSUM_TCP,
};

enum send_crc_alg {
	SEND_CRCALG_CRC32,
	SEND_CRCALG_CRC32C,
	SEND_CRCALG_ICRC,
};

enum send_load_type {
	SEND_LD_TYPE_LDD,
	SEND_LD_TYPE_LDT,
	SEND_LD_TYPE_LDWB,
};

enum send_mem_alg_type {
	SEND_MEMALG_SET,
	SEND_MEMALG_ADD = 0x08,
	SEND_MEMALG_SUB = 0x09,
	SEND_MEMALG_ADDLEN = 0x0A,
	SEND_MEMALG_SUBLEN = 0x0B,
};

enum send_mem_dsz_type {
	SEND_MEMDSZ_B64,
	SEND_MEMDSZ_B32,
	SEND_MEMDSZ_B8 = 0x03,
};

enum sq_subdesc_type {
	SQ_DESC_TYPE_INVALID,
	SQ_DESC_TYPE_HEADER,
	SQ_DESC_TYPE_CRC,
	SQ_DESC_TYPE_IMMEDIATE,
	SQ_DESC_TYPE_GATHER,
	SQ_DESC_TYPE_MEMORY,
};

enum l3_type_t {
	L3_NONE,
	L3_IPV4		= 0x04,
	L3_IPV4_OPT	= 0x05,
	L3_IPV6		= 0x06,
	L3_IPV6_OPT	= 0x07,
	L3_ET_STOP	= 0x0D,
	L3_OTHER	= 0x0E
};

enum l4_type_t {
	L4_NONE,
	L4_IPSEC_ESP	= 0x01,
	L4_IPFRAG	= 0x02,
	L4_IPCOMP	= 0x03,
	L4_TCP		= 0x04,
	L4_UDP_PASS1	= 0x05,
	L4_GRE		= 0x07,
	L4_UDP_PASS2	= 0x08,
	L4_UDP_GENEVE	= 0x09,
	L4_UDP_VXLAN	= 0x0A,
	L4_NVGRE	= 0x0C,
	L4_OTHER	= 0x0E
};

enum vlan_strip {
	NO_STRIP,
	STRIP_FIRST_VLAN,
	STRIP_SECOND_VLAN,
	STRIP_RESERV,
};

enum rbdr_state {
	RBDR_FIFO_STATE_INACTIVE,
	RBDR_FIFO_STATE_ACTIVE,
	RBDR_FIFO_STATE_RESET,
	RBDR_FIFO_STATE_FAIL,
};

enum rq_cache_allocation {
	RQ_CACHE_ALLOC_OFF,
	RQ_CACHE_ALLOC_ALL,
	RQ_CACHE_ALLOC_FIRST,
	RQ_CACHE_ALLOC_TWO,
};

enum cq_rx_errlvl_e {
	CQ_ERRLVL_MAC,
	CQ_ERRLVL_L2,
	CQ_ERRLVL_L3,
	CQ_ERRLVL_L4,
};

enum cq_rx_errop_e {
	CQ_RX_ERROP_RE_NONE,
	CQ_RX_ERROP_RE_PARTIAL = 0x1,
	CQ_RX_ERROP_RE_JABBER = 0x2,
	CQ_RX_ERROP_RE_FCS = 0x7,
	CQ_RX_ERROP_RE_TERMINATE = 0x9,
	CQ_RX_ERROP_RE_RX_CTL = 0xb,
	CQ_RX_ERROP_PREL2_ERR = 0x1f,
	CQ_RX_ERROP_L2_FRAGMENT = 0x20,
	CQ_RX_ERROP_L2_OVERRUN = 0x21,
	CQ_RX_ERROP_L2_PFCS = 0x22,
	CQ_RX_ERROP_L2_PUNY = 0x23,
	CQ_RX_ERROP_L2_MAL = 0x24,
	CQ_RX_ERROP_L2_OVERSIZE = 0x25,
	CQ_RX_ERROP_L2_UNDERSIZE = 0x26,
	CQ_RX_ERROP_L2_LENMISM = 0x27,
	CQ_RX_ERROP_L2_PCLP = 0x28,
	CQ_RX_ERROP_IP_NOT = 0x41,
	CQ_RX_ERROP_IP_CSUM_ERR = 0x42,
	CQ_RX_ERROP_IP_MAL = 0x43,
	CQ_RX_ERROP_IP_MALD = 0x44,
	CQ_RX_ERROP_IP_HOP = 0x45,
	CQ_RX_ERROP_L3_ICRC = 0x46,
	CQ_RX_ERROP_L3_PCLP = 0x47,
	CQ_RX_ERROP_L4_MAL = 0x61,
	CQ_RX_ERROP_L4_CHK = 0x62,
	CQ_RX_ERROP_UDP_LEN = 0x63,
	CQ_RX_ERROP_L4_PORT = 0x64,
	CQ_RX_ERROP_TCP_FLAG = 0x65,
	CQ_RX_ERROP_TCP_OFFSET = 0x66,
	CQ_RX_ERROP_L4_PCLP = 0x67,
	CQ_RX_ERROP_RBDR_TRUNC = 0x70,
};

enum cq_tx_errop_e {
	CQ_TX_ERROP_GOOD,
	CQ_TX_ERROP_DESC_FAULT = 0x10,
	CQ_TX_ERROP_HDR_CONS_ERR = 0x11,
	CQ_TX_ERROP_SUBDC_ERR = 0x12,
	CQ_TX_ERROP_IMM_SIZE_OFLOW = 0x80,
	CQ_TX_ERROP_DATA_SEQUENCE_ERR = 0x81,
	CQ_TX_ERROP_MEM_SEQUENCE_ERR = 0x82,
	CQ_TX_ERROP_LOCK_VIOL = 0x83,
	CQ_TX_ERROP_DATA_FAULT = 0x84,
	CQ_TX_ERROP_TSTMP_CONFLICT = 0x85,
	CQ_TX_ERROP_TSTMP_TIMEOUT = 0x86,
	CQ_TX_ERROP_MEM_FAULT = 0x87,
	CQ_TX_ERROP_CK_OVERLAP = 0x88,
	CQ_TX_ERROP_CK_OFLOW = 0x89,
	CQ_TX_ERROP_ENUM_LAST = 0x8a,
};

enum rq_sq_stats_reg_offset {
	RQ_SQ_STATS_OCTS,
	RQ_SQ_STATS_PKTS,
};

enum nic_stat_vnic_rx_e {
	RX_OCTS,
	RX_UCAST,
	RX_BCAST,
	RX_MCAST,
	RX_RED,
	RX_RED_OCTS,
	RX_ORUN,
	RX_ORUN_OCTS,
	RX_FCS,
	RX_L2ERR,
	RX_DRP_BCAST,
	RX_DRP_MCAST,
	RX_DRP_L3BCAST,
	RX_DRP_L3MCAST,
};

enum nic_stat_vnic_tx_e {
	TX_OCTS,
	TX_UCAST,
	TX_BCAST,
	TX_MCAST,
	TX_DROP,
};

#endif /* _THUNDERX_NICVF_HW_DEFS_H */
