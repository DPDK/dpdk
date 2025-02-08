/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#ifndef _ZSDA_QP_COMMON_H_
#define _ZSDA_QP_COMMON_H_

#include <rte_bus_pci.h>
#include <rte_io.h>
#include <rte_cycles.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>

#include "bus_pci_driver.h"
#include "zsda_logs.h"

#define ZSDA_MAX_DEV				RTE_PMD_ZSDA_MAX_PCI_DEVICES

#define ZSDA_SUCCESS			0
#define ZSDA_FAILED				(-1)

enum zsda_service_type {
	ZSDA_SERVICE_INVALID,
};
#define ZSDA_MAX_SERVICES (0)

#define ZSDA_CSR_READ32(addr)	      rte_read32((addr))
#define ZSDA_CSR_WRITE32(addr, value) rte_write32((value), (addr))
#define ZSDA_CSR_READ8(addr)	      rte_read8((addr))
#define ZSDA_CSR_WRITE8(addr, value)  rte_write8_relaxed((value), (addr))

struct __rte_packed_begin zsda_admin_req {
	uint16_t msg_type;
	uint8_t data[26];
} __rte_packed_end;

struct __rte_packed_begin zsda_admin_req_qcfg {
	uint16_t msg_type;
	uint8_t qid;
	uint8_t data[25];
} __rte_packed_end;

struct __rte_packed_begin qinfo {
	uint16_t q_type;
	uint16_t wq_tail;
	uint16_t wq_head;
	uint16_t cq_tail;
	uint16_t cq_head;
	uint16_t cycle;
} __rte_packed_end;

struct __rte_packed_begin zsda_admin_resp_qcfg {
	uint16_t msg_type;
	struct qinfo qcfg;
	uint8_t data[14];
} __rte_packed_end;

#endif /* _ZSDA_QP_COMMON_H_ */
