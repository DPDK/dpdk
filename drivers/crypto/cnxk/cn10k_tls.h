/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#ifndef __CN10K_TLS_H__
#define __CN10K_TLS_H__

#include <rte_crypto_sym.h>
#include <rte_security.h>

#include "roc_ie_ot_tls.h"

#include "cnxk_cryptodev.h"
#include "cnxk_cryptodev_ops.h"

/* Forward declaration */
struct cn10k_sec_session;

struct __rte_aligned(ROC_ALIGN) cn10k_tls_record {
	union {
		/** Read SA */
		struct roc_ie_ot_tls_read_sa read_sa;
		/** Write SA */
		struct roc_ie_ot_tls_write_sa write_sa;
	};
};

int cn10k_tls_record_session_update(struct cnxk_cpt_vf *vf, struct cnxk_cpt_qp *qp,
				    struct cn10k_sec_session *sess,
				    struct rte_security_session_conf *conf);

int cn10k_tls_record_session_create(struct cnxk_cpt_vf *vf, struct cnxk_cpt_qp *qp,
				    struct rte_security_tls_record_xform *tls_xfrm,
				    struct rte_crypto_sym_xform *crypto_xfrm,
				    struct rte_security_session *sess);

int cn10k_sec_tls_session_destroy(struct cnxk_cpt_qp *qp, struct cn10k_sec_session *sess);

#endif /* __CN10K_TLS_H__ */
