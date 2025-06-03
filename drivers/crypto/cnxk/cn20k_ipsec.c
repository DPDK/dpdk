/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell.
 */

#include <cryptodev_pmd.h>
#include <rte_esp.h>
#include <rte_ip.h>
#include <rte_malloc.h>
#include <rte_security.h>
#include <rte_security_driver.h>
#include <rte_udp.h>

#include "cn20k_cryptodev_ops.h"
#include "cn20k_cryptodev_sec.h"
#include "cn20k_ipsec.h"
#include "cnxk_cryptodev.h"
#include "cnxk_cryptodev_ops.h"
#include "cnxk_ipsec.h"
#include "cnxk_security.h"

#include "roc_api.h"

int
cn20k_ipsec_session_create(struct cnxk_cpt_vf *vf, struct cnxk_cpt_qp *qp,
			   struct rte_security_ipsec_xform *ipsec_xfrm,
			   struct rte_crypto_sym_xform *crypto_xfrm,
			   struct rte_security_session *sess)
{
	RTE_SET_USED(vf);
	RTE_SET_USED(qp);
	RTE_SET_USED(ipsec_xfrm);
	RTE_SET_USED(crypto_xfrm);
	RTE_SET_USED(sess);

	return 0;
}

int
cn20k_sec_ipsec_session_destroy(struct cnxk_cpt_qp *qp, struct cn20k_sec_session *sess)
{
	RTE_SET_USED(qp);
	RTE_SET_USED(sess);

	return 0;
}

int
cn20k_ipsec_stats_get(struct cnxk_cpt_qp *qp, struct cn20k_sec_session *sess,
		      struct rte_security_stats *stats)
{
	RTE_SET_USED(qp);
	RTE_SET_USED(sess);
	RTE_SET_USED(stats);

	return 0;
}

int
cn20k_ipsec_session_update(struct cnxk_cpt_vf *vf, struct cnxk_cpt_qp *qp,
			   struct cn20k_sec_session *sess, struct rte_security_session_conf *conf)
{
	RTE_SET_USED(vf);
	RTE_SET_USED(qp);
	RTE_SET_USED(sess);
	RTE_SET_USED(conf);

	return 0;
}
