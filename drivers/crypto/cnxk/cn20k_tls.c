/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell.
 */

#include <rte_crypto_sym.h>
#include <rte_cryptodev.h>
#include <rte_security.h>

#include <cryptodev_pmd.h>

#include "roc_cpt.h"
#include "roc_se.h"

#include "cn20k_cryptodev_sec.h"
#include "cn20k_tls.h"
#include "cnxk_cryptodev.h"
#include "cnxk_cryptodev_ops.h"
#include "cnxk_security.h"

int
cn20k_tls_record_session_update(struct cnxk_cpt_vf *vf, struct cnxk_cpt_qp *qp,
				struct cn20k_sec_session *sess,
				struct rte_security_session_conf *conf)
{
	RTE_SET_USED(vf);
	RTE_SET_USED(qp);
	RTE_SET_USED(sess);
	RTE_SET_USED(conf);

	return 0;
}

int
cn20k_tls_record_session_create(struct cnxk_cpt_vf *vf, struct cnxk_cpt_qp *qp,
				struct rte_security_tls_record_xform *tls_xfrm,
				struct rte_crypto_sym_xform *crypto_xfrm,
				struct rte_security_session *sess)
{
	RTE_SET_USED(vf);
	RTE_SET_USED(qp);
	RTE_SET_USED(tls_xfrm);
	RTE_SET_USED(crypto_xfrm);
	RTE_SET_USED(sess);

	return 0;
}

int
cn20k_sec_tls_session_destroy(struct cnxk_cpt_qp *qp, struct cn20k_sec_session *sess)
{

	RTE_SET_USED(qp);
	RTE_SET_USED(sess);

	return 0;
}
