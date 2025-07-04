/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _NITROX_CSR_H_
#define _NITROX_CSR_H_

#include <rte_common.h>
#include <rte_io.h>

#define CSR_DELAY	30
#define NITROX_CSR_ADDR(bar_addr, offset) (bar_addr + (offset))

/* NPS packet registers */
#define NPS_PKT_IN_INSTR_CTLX(_i)	(0x10060UL + ((_i) * 0x40000UL))
#define NPS_PKT_IN_INSTR_BADDRX(_i)	(0x10068UL + ((_i) * 0x40000UL))
#define NPS_PKT_IN_INSTR_RSIZEX(_i)	(0x10070UL + ((_i) * 0x40000UL))
#define NPS_PKT_IN_DONE_CNTSX(_i)	(0x10080UL + ((_i) * 0x40000UL))
#define NPS_PKT_IN_INSTR_BAOFF_DBELLX(_i)	(0x10078UL + ((_i) * 0x40000UL))
#define NPS_PKT_IN_INT_LEVELSX(_i)		(0x10088UL + ((_i) * 0x40000UL))
#define NPS_PKT_SLC_CTLX(_i)		(0x10000UL + ((_i) * 0x40000UL))
#define NPS_PKT_SLC_CNTSX(_i)		(0x10008UL + ((_i) * 0x40000UL))
#define NPS_PKT_SLC_INT_LEVELSX(_i)	(0x10010UL + ((_i) * 0x40000UL))

/* AQM Virtual Function Registers */
#define AQMQ_QSZX(_i)			(0x20008UL + ((_i) * 0x40000UL))

/* ZQM virtual function registers */
#define ZQMQ_DRBLX(_i)			(0x30000UL + ((_i) * 0x40000UL))
#define ZQMQ_QSZX(_i)			(0x30008UL + ((_i) * 0x40000UL))
#define ZQMQ_BADRX(_i)			(0x30010UL + ((_i) * 0x40000UL))
#define ZQMQ_NXT_CMDX(_i)		(0x30018UL + ((_i) * 0x40000UL))
#define ZQMQ_CMD_CNTX(_i)		(0x30020UL + ((_i) * 0x40000UL))
#define ZQMQ_CMP_THRX(_i)		(0x30028UL + ((_i) * 0x40000UL))
#define ZQMQ_CMP_CNTX(_i)		(0x30030UL + ((_i) * 0x40000UL))
#define ZQMQ_TIMER_LDX(_i)		(0x30038UL + ((_i) * 0x40000UL))
#define ZQMQ_ENX(_i)			(0x30048UL + ((_i) * 0x40000UL))
#define ZQMQ_ACTIVITY_STATX(_i)		(0x30050UL + ((_i) * 0x40000UL))

static inline uint64_t
nitrox_read_csr(uint8_t *bar_addr, uint64_t offset)
{
	return rte_read64(bar_addr + offset);
}

static inline void
nitrox_write_csr(uint8_t *bar_addr, uint64_t offset, uint64_t value)
{
	rte_write64(value, (bar_addr + offset));
}

#endif /* _NITROX_CSR_H_ */
