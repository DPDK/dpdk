/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell.
 */

#include <cryptodev_pmd.h>
#include <rte_cryptodev.h>

#include "roc_cpt.h"
#include "roc_idev.h"

#include "cn20k_cryptodev.h"
#include "cn20k_cryptodev_ops.h"
#include "cnxk_cryptodev.h"
#include "cnxk_cryptodev_ops.h"
#include "cnxk_se.h"

#include "rte_pmd_cnxk_crypto.h"

static int
cn20k_cpt_crypto_adapter_ev_mdata_set(struct rte_cryptodev *dev __rte_unused, void *sess,
				      enum rte_crypto_op_type op_type,
				      enum rte_crypto_op_sess_type sess_type, void *mdata)
{
	(void)dev;
	(void)sess;
	(void)op_type;
	(void)sess_type;
	(void)mdata;

	return 0;
}

static uint16_t
cn20k_cpt_enqueue_burst(void *qptr, struct rte_crypto_op **ops, uint16_t nb_ops)
{
	(void)qptr;
	(void)ops;
	(void)nb_ops;

	return 0;
}

static uint16_t
cn20k_cpt_dequeue_burst(void *qptr, struct rte_crypto_op **ops, uint16_t nb_ops)
{
	(void)qptr;
	(void)ops;
	(void)nb_ops;

	return 0;
}

void
cn20k_cpt_set_enqdeq_fns(struct rte_cryptodev *dev)
{
	dev->enqueue_burst = cn20k_cpt_enqueue_burst;
	dev->dequeue_burst = cn20k_cpt_dequeue_burst;

	rte_mb();
}

static void
cn20k_cpt_dev_info_get(struct rte_cryptodev *dev, struct rte_cryptodev_info *info)
{
	if (info != NULL) {
		cnxk_cpt_dev_info_get(dev, info);
		info->driver_id = cn20k_cryptodev_driver_id;
	}
}

static int
cn20k_sym_get_raw_dp_ctx_size(struct rte_cryptodev *dev __rte_unused)
{
	return 0;
}

static int
cn20k_sym_configure_raw_dp_ctx(struct rte_cryptodev *dev, uint16_t qp_id,
			       struct rte_crypto_raw_dp_ctx *raw_dp_ctx,
			       enum rte_crypto_op_sess_type sess_type,
			       union rte_cryptodev_session_ctx session_ctx, uint8_t is_update)
{
	(void)dev;
	(void)qp_id;
	(void)raw_dp_ctx;
	(void)sess_type;
	(void)session_ctx;
	(void)is_update;
	return 0;
}

struct rte_cryptodev_ops cn20k_cpt_ops = {
	/* Device control ops */
	.dev_configure = cnxk_cpt_dev_config,
	.dev_start = cnxk_cpt_dev_start,
	.dev_stop = cnxk_cpt_dev_stop,
	.dev_close = cnxk_cpt_dev_close,
	.dev_infos_get = cn20k_cpt_dev_info_get,

	.stats_get = NULL,
	.stats_reset = NULL,
	.queue_pair_setup = cnxk_cpt_queue_pair_setup,
	.queue_pair_release = cnxk_cpt_queue_pair_release,
	.queue_pair_reset = cnxk_cpt_queue_pair_reset,

	/* Symmetric crypto ops */
	.sym_session_get_size = cnxk_cpt_sym_session_get_size,
	.sym_session_configure = cnxk_cpt_sym_session_configure,
	.sym_session_clear = cnxk_cpt_sym_session_clear,

	/* Asymmetric crypto ops */
	.asym_session_get_size = cnxk_ae_session_size_get,
	.asym_session_configure = cnxk_ae_session_cfg,
	.asym_session_clear = cnxk_ae_session_clear,

	/* Event crypto ops */
	.session_ev_mdata_set = cn20k_cpt_crypto_adapter_ev_mdata_set,
	.queue_pair_event_error_query = cnxk_cpt_queue_pair_event_error_query,

	/* Raw data-path API related operations */
	.sym_get_raw_dp_ctx_size = cn20k_sym_get_raw_dp_ctx_size,
	.sym_configure_raw_dp_ctx = cn20k_sym_configure_raw_dp_ctx,
};
