/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include <string.h>

#include <rte_common.h>
#include <rte_malloc.h>

#include "ipsec_mb_private.h"

#define IMB_MP_REQ_VER_STR "1.1.0"

/** Configure device */
int
ipsec_mb_config(__rte_unused struct rte_cryptodev *dev,
		    __rte_unused struct rte_cryptodev_config *config)
{
	return 0;
}

/** Start device */
int
ipsec_mb_start(__rte_unused struct rte_cryptodev *dev)
{
	return 0;
}

/** Stop device */
void
ipsec_mb_stop(__rte_unused struct rte_cryptodev *dev)
{
}

/** Close device */
int
ipsec_mb_close(__rte_unused struct rte_cryptodev *dev)
{
	return 0;
}

/** Get device statistics */
void
ipsec_mb_stats_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_stats *stats)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct ipsec_mb_qp *qp = dev->data->queue_pairs[qp_id];
		if (qp == NULL) {
			IPSEC_MB_LOG(DEBUG, "Uninitialised qp %d", qp_id);
			continue;
		}

		stats->enqueued_count += qp->stats.enqueued_count;
		stats->dequeued_count += qp->stats.dequeued_count;

		stats->enqueue_err_count += qp->stats.enqueue_err_count;
		stats->dequeue_err_count += qp->stats.dequeue_err_count;
	}
}

/** Reset device statistics */
void
ipsec_mb_stats_reset(struct rte_cryptodev *dev)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct ipsec_mb_qp *qp = dev->data->queue_pairs[qp_id];

		memset(&qp->stats, 0, sizeof(qp->stats));
	}
}

/** Get device info */
void
ipsec_mb_info_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_info *dev_info)
{
	struct ipsec_mb_dev_private *internals = dev->data->dev_private;
	struct ipsec_mb_internals *pmd_info =
		&ipsec_mb_pmds[internals->pmd_type];

	if (dev_info != NULL) {
		dev_info->driver_id = dev->driver_id;
		dev_info->feature_flags = dev->feature_flags;
		dev_info->capabilities = pmd_info->caps;
		dev_info->max_nb_queue_pairs = internals->max_nb_queue_pairs;
		/* No limit of number of sessions */
		dev_info->sym.max_nb_sessions = 0;
	}
}

/** Release queue pair */
int
ipsec_mb_qp_release(struct rte_cryptodev *dev, uint16_t qp_id)
{
	struct ipsec_mb_qp *qp = dev->data->queue_pairs[qp_id];
	struct rte_ring *r = NULL;

	if (qp != NULL && rte_eal_process_type() == RTE_PROC_PRIMARY) {
		r = rte_ring_lookup(qp->name);
		rte_ring_free(r);

#if IMB_VERSION(1, 1, 0) > IMB_VERSION_NUM
		if (qp->mb_mgr)
			free_mb_mgr(qp->mb_mgr);
#else
		if (qp->mb_mgr_mz) {
			rte_memzone_free(qp->mb_mgr_mz);
			qp->mb_mgr = NULL;
		}
#endif
		rte_free(qp);
		dev->data->queue_pairs[qp_id] = NULL;
	}
	return 0;
}

/** Set a unique name for the queue pair */
int
ipsec_mb_qp_set_unique_name(struct rte_cryptodev *dev,
					   struct ipsec_mb_qp *qp)
{
	uint32_t n =
	    snprintf(qp->name, sizeof(qp->name), "ipsec_mb_pmd_%u_qp_%u",
		     dev->data->dev_id, qp->id);

	if (n >= sizeof(qp->name))
		return -1;

	return 0;
}

/** Create a ring to place processed operations on */
static struct rte_ring
*ipsec_mb_qp_create_processed_ops_ring(
	struct ipsec_mb_qp *qp, unsigned int ring_size, int socket_id)
{
	struct rte_ring *r;
	char ring_name[RTE_CRYPTODEV_NAME_MAX_LEN];

	unsigned int n = rte_strlcpy(ring_name, qp->name, sizeof(ring_name));

	if (n >= sizeof(ring_name))
		return NULL;

	r = rte_ring_lookup(ring_name);
	if (r) {
		if (rte_ring_get_size(r) >= ring_size) {
			IPSEC_MB_LOG(
			    INFO, "Reusing existing ring %s for processed ops",
			    ring_name);
			return r;
		}
		IPSEC_MB_LOG(
		    ERR, "Unable to reuse existing ring %s for processed ops",
		    ring_name);
		return NULL;
	}

	return rte_ring_create(ring_name, ring_size, socket_id,
			       RING_F_SP_ENQ | RING_F_SC_DEQ);
}

#if IMB_VERSION(1, 1, 0) <= IMB_VERSION_NUM
static IMB_MGR *
ipsec_mb_alloc_mgr_from_memzone(const struct rte_memzone **mb_mgr_mz,
		const char *mb_mgr_mz_name)
{
	IMB_MGR *mb_mgr;

	if (rte_eal_process_type() ==  RTE_PROC_PRIMARY) {
		*mb_mgr_mz = rte_memzone_lookup(mb_mgr_mz_name);
		if (*mb_mgr_mz == NULL) {
			*mb_mgr_mz = rte_memzone_reserve(mb_mgr_mz_name,
			imb_get_mb_mgr_size(),
			rte_socket_id(), 0);
		}
		if (*mb_mgr_mz == NULL) {
			IPSEC_MB_LOG(DEBUG, "Error allocating memzone for %s",
					mb_mgr_mz_name);
			return NULL;
		}
		mb_mgr = imb_set_pointers_mb_mgr((*mb_mgr_mz)->addr, 0, 1);
		init_mb_mgr_auto(mb_mgr, NULL);
	} else {
		*mb_mgr_mz = rte_memzone_lookup(mb_mgr_mz_name);
		if (*mb_mgr_mz == NULL) {
			IPSEC_MB_LOG(ERR,
				"Secondary can't find %s mz, did primary create it?",
				mb_mgr_mz_name);
			return NULL;
		}
		mb_mgr = imb_set_pointers_mb_mgr((*mb_mgr_mz)->addr, 0, 0);
	}
	return mb_mgr;
}
#endif

/** Setup a queue pair */
int
ipsec_mb_qp_setup(struct rte_cryptodev *dev, uint16_t qp_id,
				const struct rte_cryptodev_qp_conf *qp_conf,
				int socket_id)
{
	struct ipsec_mb_qp *qp = NULL;
	struct ipsec_mb_dev_private *internals = dev->data->dev_private;
	struct ipsec_mb_internals *pmd_data =
		&ipsec_mb_pmds[internals->pmd_type];
	uint32_t qp_size;
	int ret;

	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
#if IMB_VERSION(1, 1, 0) > IMB_VERSION_NUM
		IPSEC_MB_LOG(ERR, "The intel-ipsec-mb version (%s) does not support multiprocess,"
				"the minimum version required for this feature is %s.",
				IMB_VERSION_STR, IMB_MP_REQ_VER_STR);
		return -EINVAL;
#endif
		qp = dev->data->queue_pairs[qp_id];
		if (qp == NULL) {
			IPSEC_MB_LOG(ERR, "Primary process hasn't configured device qp.");
			return -EINVAL;
		}
	} else {
		/* Free memory prior to re-allocation if needed. */
		if (dev->data->queue_pairs[qp_id] != NULL)
			ipsec_mb_qp_release(dev, qp_id);

		qp_size = sizeof(*qp) + pmd_data->qp_priv_size;
		/* Allocate the queue pair data structure. */
		qp = rte_zmalloc_socket("IPSEC PMD Queue Pair", qp_size,
					RTE_CACHE_LINE_SIZE, socket_id);
		if (qp == NULL)
			return -ENOMEM;
	}

#if IMB_VERSION(1, 1, 0) > IMB_VERSION_NUM
	qp->mb_mgr = alloc_init_mb_mgr();
#else
	char mz_name[IPSEC_MB_MAX_MZ_NAME];
	snprintf(mz_name, sizeof(mz_name), "IMB_MGR_DEV_%d_QP_%d",
			dev->data->dev_id, qp_id);
	qp->mb_mgr = ipsec_mb_alloc_mgr_from_memzone(&(qp->mb_mgr_mz),
			mz_name);
#endif
	if (qp->mb_mgr == NULL) {
		ret = -ENOMEM;
		goto qp_setup_cleanup;
	}

	if (rte_eal_process_type() == RTE_PROC_SECONDARY)
		return 0;

	qp->id = qp_id;
	dev->data->queue_pairs[qp_id] = qp;
	if (ipsec_mb_qp_set_unique_name(dev, qp)) {
		ret = -EINVAL;
		goto qp_setup_cleanup;
	}

	qp->pmd_type = internals->pmd_type;
	qp->sess_mp = qp_conf->mp_session;
	qp->sess_mp_priv = qp_conf->mp_session_private;

	qp->ingress_queue = ipsec_mb_qp_create_processed_ops_ring(qp,
		qp_conf->nb_descriptors, socket_id);
	if (qp->ingress_queue == NULL) {
		ret = -EINVAL;
		goto qp_setup_cleanup;
	}

	memset(&qp->stats, 0, sizeof(qp->stats));

	if (pmd_data->queue_pair_configure) {
		ret = pmd_data->queue_pair_configure(qp);
		if (ret < 0)
			goto qp_setup_cleanup;
	}

	return 0;

qp_setup_cleanup:
#if IMB_VERSION(1, 1, 0) > IMB_VERSION_NUM
	if (qp->mb_mgr)
		free_mb_mgr(qp->mb_mgr);
#else
	if (rte_eal_process_type() == RTE_PROC_SECONDARY)
		return ret;
	if (qp->mb_mgr_mz)
		rte_memzone_free(qp->mb_mgr_mz);
#endif
	rte_free(qp);
	return ret;
}

/** Return the size of the specific pmd session structure */
unsigned
ipsec_mb_sym_session_get_size(struct rte_cryptodev *dev)
{
	struct ipsec_mb_dev_private *internals = dev->data->dev_private;
	struct ipsec_mb_internals *pmd_data =
		&ipsec_mb_pmds[internals->pmd_type];

	return pmd_data->session_priv_size;
}

/** Configure pmd specific multi-buffer session from a crypto xform chain */
int
ipsec_mb_sym_session_configure(
	struct rte_cryptodev *dev, struct rte_crypto_sym_xform *xform,
	struct rte_cryptodev_sym_session *sess, struct rte_mempool *mempool)
{
	void *sess_private_data;
	struct ipsec_mb_dev_private *internals = dev->data->dev_private;
	struct ipsec_mb_internals *pmd_data =
		&ipsec_mb_pmds[internals->pmd_type];
	IMB_MGR *mb_mgr = alloc_init_mb_mgr();
	int ret = 0;

	if (!mb_mgr)
		return -ENOMEM;

	if (unlikely(sess == NULL)) {
		IPSEC_MB_LOG(ERR, "invalid session struct");
		free_mb_mgr(mb_mgr);
		return -EINVAL;
	}

	if (rte_mempool_get(mempool, &sess_private_data)) {
		IPSEC_MB_LOG(ERR, "Couldn't get object from session mempool");
		free_mb_mgr(mb_mgr);
		return -ENOMEM;
	}

	ret = (*pmd_data->session_configure)(mb_mgr, sess_private_data, xform);
	if (ret != 0) {
		IPSEC_MB_LOG(ERR, "failed configure session parameters");

		/* Return session to mempool */
		rte_mempool_put(mempool, sess_private_data);
		free_mb_mgr(mb_mgr);
		return ret;
	}

	set_sym_session_private_data(sess, dev->driver_id, sess_private_data);

	free_mb_mgr(mb_mgr);
	return 0;
}

/** Clear the session memory */
void
ipsec_mb_sym_session_clear(struct rte_cryptodev *dev,
			       struct rte_cryptodev_sym_session *sess)
{
	uint8_t index = dev->driver_id;
	void *sess_priv = get_sym_session_private_data(sess, index);

	/* Zero out the whole structure */
	if (sess_priv) {
		memset(sess_priv, 0, ipsec_mb_sym_session_get_size(dev));
		struct rte_mempool *sess_mp = rte_mempool_from_obj(sess_priv);

		set_sym_session_private_data(sess, index, NULL);
		rte_mempool_put(sess_mp, sess_priv);
	}
}
