/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __CN10K_IPSEC_H__
#define __CN10K_IPSEC_H__

#include <rte_security.h>
#include <rte_security_driver.h>

#include "roc_constants.h"
#include "roc_ie_ot.h"

#include "cnxk_cryptodev.h"
#include "cnxk_cryptodev_ops.h"
#include "cnxk_ipsec.h"

/* Forward declaration */
struct cn10k_sec_session;

struct __rte_aligned(ROC_ALIGN) cn10k_ipsec_sa {
	union {
		/** Inbound SA */
		struct roc_ot_ipsec_inb_sa in_sa;
		/** Outbound SA */
		struct roc_ot_ipsec_outb_sa out_sa;
	};
};

int cn10k_ipsec_session_create(struct cnxk_cpt_vf *vf, struct cnxk_cpt_qp *qp,
			       struct rte_security_ipsec_xform *ipsec_xfrm,
			       struct rte_crypto_sym_xform *crypto_xfrm,
			       struct rte_security_session *sess);
int cn10k_sec_ipsec_session_destroy(struct cnxk_cpt_qp *qp, struct cn10k_sec_session *sess);
int cn10k_ipsec_stats_get(struct cnxk_cpt_qp *qp, struct cn10k_sec_session *sess,
			  struct rte_security_stats *stats);
int cn10k_ipsec_session_update(struct cnxk_cpt_vf *vf, struct cnxk_cpt_qp *qp,
			       struct cn10k_sec_session *sess,
			       struct rte_security_session_conf *conf);
#endif /* __CN10K_IPSEC_H__ */
