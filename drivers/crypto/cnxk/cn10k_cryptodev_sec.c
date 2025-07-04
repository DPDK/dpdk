/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include <rte_security.h>

#include "cn10k_cryptodev_ops.h"
#include "cn10k_cryptodev_sec.h"
#include "cnxk_cryptodev_ops.h"

static int
cn10k_sec_session_create(void *dev, struct rte_security_session_conf *conf,
			 struct rte_security_session *sess)
{
	struct rte_cryptodev *crypto_dev = dev;
	struct cnxk_cpt_vf *vf;
	struct cnxk_cpt_qp *qp;

	if (conf->action_type != RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL)
		return -EINVAL;

	qp = crypto_dev->data->queue_pairs[0];
	if (qp == NULL) {
		plt_err("Setup cryptodev queue pair before creating security session");
		return -EPERM;
	}

	vf = crypto_dev->data->dev_private;

	if (conf->protocol == RTE_SECURITY_PROTOCOL_IPSEC) {
		((struct cn10k_sec_session *)sess)->userdata = conf->userdata;
		return cn10k_ipsec_session_create(vf, qp, &conf->ipsec, conf->crypto_xform, sess);
	}

	if (conf->protocol == RTE_SECURITY_PROTOCOL_TLS_RECORD)
		return cn10k_tls_record_session_create(vf, qp, &conf->tls_record,
						       conf->crypto_xform, sess);

	return -ENOTSUP;
}

static int
cn10k_sec_session_destroy(void *dev, struct rte_security_session *sec_sess)
{
	struct cn10k_sec_session *cn10k_sec_sess;
	struct rte_cryptodev *crypto_dev = dev;
	struct cnxk_cpt_qp *qp;

	if (unlikely(sec_sess == NULL))
		return -EINVAL;

	qp = crypto_dev->data->queue_pairs[0];
	if (unlikely(qp == NULL))
		return -ENOTSUP;

	cn10k_sec_sess = (struct cn10k_sec_session *)sec_sess;

	if (cn10k_sec_sess->proto == RTE_SECURITY_PROTOCOL_IPSEC)
		return cn10k_sec_ipsec_session_destroy(qp, cn10k_sec_sess);

	if (cn10k_sec_sess->proto == RTE_SECURITY_PROTOCOL_TLS_RECORD)
		return cn10k_sec_tls_session_destroy(qp, cn10k_sec_sess);

	return -EINVAL;
}

static unsigned int
cn10k_sec_session_get_size(void *dev __rte_unused)
{
	return sizeof(struct cn10k_sec_session) - sizeof(struct rte_security_session);
}

static int
cn10k_sec_session_stats_get(void *dev, struct rte_security_session *sec_sess,
			    struct rte_security_stats *stats)
{
	struct cn10k_sec_session *cn10k_sec_sess;
	struct rte_cryptodev *crypto_dev = dev;
	struct cnxk_cpt_qp *qp;

	if (unlikely(sec_sess == NULL))
		return -EINVAL;

	qp = crypto_dev->data->queue_pairs[0];
	if (unlikely(qp == NULL))
		return -ENOTSUP;

	cn10k_sec_sess = (struct cn10k_sec_session *)sec_sess;

	if (cn10k_sec_sess->proto == RTE_SECURITY_PROTOCOL_IPSEC)
		return cn10k_ipsec_stats_get(qp, cn10k_sec_sess, stats);

	return -ENOTSUP;
}

static int
cn10k_sec_session_update(void *dev, struct rte_security_session *sec_sess,
			 struct rte_security_session_conf *conf)
{
	struct cn10k_sec_session *cn10k_sec_sess;
	struct rte_cryptodev *crypto_dev = dev;
	struct cnxk_cpt_qp *qp;
	struct cnxk_cpt_vf *vf;

	if (sec_sess == NULL)
		return -EINVAL;

	qp = crypto_dev->data->queue_pairs[0];
	if (qp == NULL)
		return -EINVAL;

	vf = crypto_dev->data->dev_private;

	cn10k_sec_sess = (struct cn10k_sec_session *)sec_sess;

	if (cn10k_sec_sess->proto == RTE_SECURITY_PROTOCOL_IPSEC)
		return cn10k_ipsec_session_update(vf, qp, cn10k_sec_sess, conf);

	if (conf->protocol == RTE_SECURITY_PROTOCOL_TLS_RECORD)
		return cn10k_tls_record_session_update(vf, qp, cn10k_sec_sess, conf);

	return -ENOTSUP;
}

/* Update platform specific security ops */
void
cn10k_sec_ops_override(void)
{
	/* Update platform specific ops */
	cnxk_sec_ops.session_create = cn10k_sec_session_create;
	cnxk_sec_ops.session_destroy = cn10k_sec_session_destroy;
	cnxk_sec_ops.session_get_size = cn10k_sec_session_get_size;
	cnxk_sec_ops.session_stats_get = cn10k_sec_session_stats_get;
	cnxk_sec_ops.session_update = cn10k_sec_session_update;
	cnxk_sec_ops.inb_pkt_rx_inject = cn10k_cryptodev_sec_inb_rx_inject;
	cnxk_sec_ops.rx_inject_configure = cn10k_cryptodev_sec_rx_inject_configure;
}
