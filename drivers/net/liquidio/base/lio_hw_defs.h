/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef _LIO_HW_DEFS_H_
#define _LIO_HW_DEFS_H_

#include <rte_io.h>

#ifndef PCI_VENDOR_ID_CAVIUM
#define PCI_VENDOR_ID_CAVIUM	0x177D
#endif

#define LIO_CN23XX_VF_VID	0x9712

/* CN23xx subsystem device ids */
#define PCI_SUBSYS_DEV_ID_CN2350_210		0x0004
#define PCI_SUBSYS_DEV_ID_CN2360_210		0x0005
#define PCI_SUBSYS_DEV_ID_CN2360_225		0x0006
#define PCI_SUBSYS_DEV_ID_CN2350_225		0x0007
#define PCI_SUBSYS_DEV_ID_CN2350_210SVPN3	0x0008
#define PCI_SUBSYS_DEV_ID_CN2360_210SVPN3	0x0009
#define PCI_SUBSYS_DEV_ID_CN2350_210SVPT	0x000a
#define PCI_SUBSYS_DEV_ID_CN2360_210SVPT	0x000b

/* --------------------------CONFIG VALUES------------------------ */

/* CN23xx IQ configuration macros */
#define CN23XX_MAX_RINGS_PER_PF			64
#define CN23XX_MAX_RINGS_PER_VF			8

#define CN23XX_MAX_INPUT_QUEUES			CN23XX_MAX_RINGS_PER_PF
#define CN23XX_MAX_IQ_DESCRIPTORS		512
#define CN23XX_MIN_IQ_DESCRIPTORS		128

#define CN23XX_MAX_OUTPUT_QUEUES		CN23XX_MAX_RINGS_PER_PF
#define CN23XX_MAX_OQ_DESCRIPTORS		512
#define CN23XX_MIN_OQ_DESCRIPTORS		128
#define CN23XX_OQ_BUF_SIZE			1536

#define CN23XX_OQ_REFIL_THRESHOLD		16

#define CN23XX_DEFAULT_NUM_PORTS		1

#define CN23XX_CFG_IO_QUEUES			CN23XX_MAX_RINGS_PER_PF

/* common OCTEON configuration macros */
#define OCTEON_64BYTE_INSTR			64
#define OCTEON_OQ_INFOPTR_MODE			1

/* Max IOQs per LIO Link */
#define LIO_MAX_IOQS_PER_IF			64

/* Wait time in milliseconds for FLR */
#define LIO_PCI_FLR_WAIT			100

enum lio_card_type {
	LIO_23XX /* 23xx */
};

#define LIO_23XX_NAME "23xx"

#define LIO_DEV_RUNNING		0xc

#define LIO_OQ_REFILL_THRESHOLD_CFG(cfg)				\
		((cfg)->default_config->oq.refill_threshold)
#define LIO_NUM_DEF_TX_DESCS_CFG(cfg)					\
		((cfg)->default_config->num_def_tx_descs)

#define LIO_IQ_INSTR_TYPE(cfg)		((cfg)->default_config->iq.instr_type)

/* The following config values are fixed and should not be modified. */

/* Maximum number of Instruction queues */
#define LIO_MAX_INSTR_QUEUES(lio_dev)		CN23XX_MAX_RINGS_PER_VF

#define LIO_MAX_POSSIBLE_INSTR_QUEUES		CN23XX_MAX_INPUT_QUEUES
#define LIO_MAX_POSSIBLE_OUTPUT_QUEUES		CN23XX_MAX_OUTPUT_QUEUES

#define LIO_DEVICE_NAME_LEN		32
#define LIO_BASE_MAJOR_VERSION		1
#define LIO_BASE_MINOR_VERSION		5
#define LIO_BASE_MICRO_VERSION		1

#define LIO_FW_VERSION_LENGTH		32

#define LIO_Q_RECONF_MIN_VERSION	"1.7.0"
#define LIO_VF_TRUST_MIN_VERSION	"1.7.1"

/** Tag types used by Octeon cores in its work. */
enum octeon_tag_type {
	OCTEON_ORDERED_TAG	= 0,
	OCTEON_ATOMIC_TAG	= 1,
};

/* pre-defined host->NIC tag values */
#define LIO_CONTROL	(0x11111110)
#define LIO_DATA(i)	(0x11111111 + (i))

/* used for NIC operations */
#define LIO_OPCODE	1

/* Subcodes are used by host driver/apps to identify the sub-operation
 * for the core. They only need to by unique for a given subsystem.
 */
#define LIO_OPCODE_SUBCODE(op, sub)		\
		((((op) & 0x0f) << 8) | ((sub) & 0x7f))

/** LIO_OPCODE subcodes */
/* This subcode is sent by core PCI driver to indicate cores are ready. */
#define LIO_OPCODE_NW_DATA		0x02 /* network packet data */
#define LIO_OPCODE_CMD			0x03
#define LIO_OPCODE_INFO			0x04
#define LIO_OPCODE_PORT_STATS		0x05
#define LIO_OPCODE_IF_CFG		0x09

#define LIO_MIN_RX_BUF_SIZE		64
#define LIO_MAX_RX_PKTLEN		(64 * 1024)

/* NIC Command types */
#define LIO_CMD_CHANGE_MTU		0x1
#define LIO_CMD_CHANGE_DEVFLAGS		0x3
#define LIO_CMD_RX_CTL			0x4
#define LIO_CMD_CLEAR_STATS		0x6
#define LIO_CMD_SET_RSS			0xD
#define LIO_CMD_TNL_RX_CSUM_CTL		0x10
#define LIO_CMD_TNL_TX_CSUM_CTL		0x11
#define LIO_CMD_ADD_VLAN_FILTER		0x17
#define LIO_CMD_DEL_VLAN_FILTER		0x18
#define LIO_CMD_VXLAN_PORT_CONFIG	0x19
#define LIO_CMD_QUEUE_COUNT_CTL		0x1f

#define LIO_CMD_VXLAN_PORT_ADD		0x0
#define LIO_CMD_VXLAN_PORT_DEL		0x1
#define LIO_CMD_RXCSUM_ENABLE		0x0
#define LIO_CMD_TXCSUM_ENABLE		0x0

/* RX(packets coming from wire) Checksum verification flags */
/* TCP/UDP csum */
#define LIO_L4_CSUM_VERIFIED		0x1
#define LIO_IP_CSUM_VERIFIED		0x2

/* RSS */
#define LIO_RSS_PARAM_DISABLE_RSS		0x10
#define LIO_RSS_PARAM_HASH_KEY_UNCHANGED	0x08
#define LIO_RSS_PARAM_ITABLE_UNCHANGED		0x04
#define LIO_RSS_PARAM_HASH_INFO_UNCHANGED	0x02

#define LIO_RSS_HASH_IPV4			0x100
#define LIO_RSS_HASH_TCP_IPV4			0x200
#define LIO_RSS_HASH_IPV6			0x400
#define LIO_RSS_HASH_TCP_IPV6			0x1000
#define LIO_RSS_HASH_IPV6_EX			0x800
#define LIO_RSS_HASH_TCP_IPV6_EX		0x2000

#define LIO_RSS_OFFLOAD_ALL (		\
		LIO_RSS_HASH_IPV4 |	\
		LIO_RSS_HASH_TCP_IPV4 |	\
		LIO_RSS_HASH_IPV6 |	\
		LIO_RSS_HASH_TCP_IPV6 |	\
		LIO_RSS_HASH_IPV6_EX |	\
		LIO_RSS_HASH_TCP_IPV6_EX)

#define LIO_RSS_MAX_TABLE_SZ		128
#define LIO_RSS_MAX_KEY_SZ		40
#define LIO_RSS_PARAM_SIZE		16

/* Interface flags communicated between host driver and core app. */
enum lio_ifflags {
	LIO_IFFLAG_PROMISC	= 0x01,
	LIO_IFFLAG_ALLMULTI	= 0x02,
	LIO_IFFLAG_UNICAST	= 0x10
};

/* Routines for reading and writing CSRs */
#ifdef RTE_LIBRTE_LIO_DEBUG_REGS
#define lio_write_csr(lio_dev, reg_off, value)				\
	do {								\
		typeof(lio_dev) _dev = lio_dev;				\
		typeof(reg_off) _reg_off = reg_off;			\
		typeof(value) _value = value;				\
		PMD_REGS_LOG(_dev,					\
			     "Write32: Reg: 0x%08lx Val: 0x%08lx\n",	\
			     (unsigned long)_reg_off,			\
			     (unsigned long)_value);			\
		rte_write32(_value, _dev->hw_addr + _reg_off);		\
	} while (0)

#define lio_write_csr64(lio_dev, reg_off, val64)			\
	do {								\
		typeof(lio_dev) _dev = lio_dev;				\
		typeof(reg_off) _reg_off = reg_off;			\
		typeof(val64) _val64 = val64;				\
		PMD_REGS_LOG(						\
		    _dev,						\
		    "Write64: Reg: 0x%08lx Val: 0x%016llx\n",		\
		    (unsigned long)_reg_off,				\
		    (unsigned long long)_val64);			\
		rte_write64(_val64, _dev->hw_addr + _reg_off);		\
	} while (0)

#define lio_read_csr(lio_dev, reg_off)					\
	({								\
		typeof(lio_dev) _dev = lio_dev;				\
		typeof(reg_off) _reg_off = reg_off;			\
		uint32_t val = rte_read32(_dev->hw_addr + _reg_off);	\
		PMD_REGS_LOG(_dev,					\
			     "Read32: Reg: 0x%08lx Val: 0x%08lx\n",	\
			     (unsigned long)_reg_off,			\
			     (unsigned long)val);			\
		val;							\
	})

#define lio_read_csr64(lio_dev, reg_off)				\
	({								\
		typeof(lio_dev) _dev = lio_dev;				\
		typeof(reg_off) _reg_off = reg_off;			\
		uint64_t val64 = rte_read64(_dev->hw_addr + _reg_off);	\
		PMD_REGS_LOG(						\
		    _dev,						\
		    "Read64: Reg: 0x%08lx Val: 0x%016llx\n",		\
		    (unsigned long)_reg_off,				\
		    (unsigned long long)val64);				\
		val64;							\
	})
#else
#define lio_write_csr(lio_dev, reg_off, value)				\
	rte_write32(value, (lio_dev)->hw_addr + (reg_off))

#define lio_write_csr64(lio_dev, reg_off, val64)			\
	rte_write64(val64, (lio_dev)->hw_addr + (reg_off))

#define lio_read_csr(lio_dev, reg_off)					\
	rte_read32((lio_dev)->hw_addr + (reg_off))

#define lio_read_csr64(lio_dev, reg_off)				\
	rte_read64((lio_dev)->hw_addr + (reg_off))
#endif
#endif /* _LIO_HW_DEFS_H_ */
