/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell.
 */

#ifndef _CN20K_CRYPTODEV_OPS_H_
#define _CN20K_CRYPTODEV_OPS_H_

#include <cryptodev_pmd.h>
#include <rte_compat.h>
#include <rte_cryptodev.h>
#include <rte_eventdev.h>

#if defined(__aarch64__)
#include "roc_io.h"
#else
#include "roc_io_generic.h"
#endif

#include "cnxk_cryptodev.h"
#include "roc_npa.h"

#define CN20K_PKTS_PER_STEORL	  32
#define CN20K_LMTLINES_PER_STEORL 16

extern struct rte_cryptodev_ops cn20k_cpt_ops;

void cn20k_cpt_set_enqdeq_fns(struct rte_cryptodev *dev);

__rte_internal
uint16_t __rte_hot cn20k_cryptodev_sec_inb_rx_inject(void *dev, struct rte_mbuf **pkts,
						     struct rte_security_session **sess,
						     uint16_t nb_pkts);

__rte_internal
int cn20k_cryptodev_sec_rx_inject_configure(void *device, uint16_t port_id, bool enable);

__rte_internal
uint16_t __rte_hot cn20k_cpt_crypto_adapter_enqueue(void *ws, struct rte_event ev[],
						    uint16_t nb_events);

static __rte_always_inline void __rte_hot
cn20k_cpt_lmtst_dual_submit(uint64_t *io_addr, const uint16_t lmt_id, int *i)
{
	uint64_t lmt_arg;

	/* Check if the total number of instructions is odd or even. */
	const int flag_odd = *i & 0x1;

	/* Reduce i by 1 when odd number of instructions.*/
	*i -= flag_odd;

	if (*i > CN20K_PKTS_PER_STEORL) {
		lmt_arg = ROC_CN20K_DUAL_CPT_LMT_ARG | (CN20K_LMTLINES_PER_STEORL - 1) << 12 |
			  (uint64_t)lmt_id;
		roc_lmt_submit_steorl(lmt_arg, *io_addr);
		lmt_arg = ROC_CN20K_DUAL_CPT_LMT_ARG |
			  (*i / 2 - CN20K_LMTLINES_PER_STEORL - 1) << 12 |
			  (uint64_t)(lmt_id + CN20K_LMTLINES_PER_STEORL);
		roc_lmt_submit_steorl(lmt_arg, *io_addr);
		if (flag_odd) {
			*io_addr = (*io_addr & ~(uint64_t)(0x7 << 4)) |
				   (ROC_CN20K_CPT_INST_DW_M1 << 4);
			lmt_arg = (uint64_t)(lmt_id + *i / 2);
			roc_lmt_submit_steorl(lmt_arg, *io_addr);
			*io_addr = (*io_addr & ~(uint64_t)(0x7 << 4)) |
				   (ROC_CN20K_TWO_CPT_INST_DW_M1 << 4);
			*i += 1;
		}
	} else {
		if (*i != 0) {
			lmt_arg =
				ROC_CN20K_DUAL_CPT_LMT_ARG | (*i / 2 - 1) << 12 | (uint64_t)lmt_id;
			roc_lmt_submit_steorl(lmt_arg, *io_addr);
		}

		if (flag_odd) {
			*io_addr = (*io_addr & ~(uint64_t)(0x7 << 4)) |
				   (ROC_CN20K_CPT_INST_DW_M1 << 4);
			lmt_arg = (uint64_t)(lmt_id + *i / 2);
			roc_lmt_submit_steorl(lmt_arg, *io_addr);
			*io_addr = (*io_addr & ~(uint64_t)(0x7 << 4)) |
				   (ROC_CN20K_TWO_CPT_INST_DW_M1 << 4);
			*i += 1;
		}
	}

	rte_io_wmb();
}

#endif /* _CN20K_CRYPTODEV_OPS_H_ */
