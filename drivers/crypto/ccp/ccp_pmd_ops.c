/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 */

#include <string.h>

#include <rte_common.h>
#include <rte_cryptodev_pmd.h>
#include <rte_malloc.h>

#include "ccp_pmd_private.h"
#include "ccp_dev.h"
#include "ccp_crypto.h"

static const struct rte_cryptodev_capabilities ccp_pmd_capabilities[] = {
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static int
ccp_pmd_config(struct rte_cryptodev *dev __rte_unused,
	       struct rte_cryptodev_config *config __rte_unused)
{
	return 0;
}

static int
ccp_pmd_start(struct rte_cryptodev *dev)
{
	return ccp_dev_start(dev);
}

static void
ccp_pmd_stop(struct rte_cryptodev *dev __rte_unused)
{

}

static int
ccp_pmd_close(struct rte_cryptodev *dev __rte_unused)
{
	return 0;
}

static void
ccp_pmd_info_get(struct rte_cryptodev *dev,
		 struct rte_cryptodev_info *dev_info)
{
	struct ccp_private *internals = dev->data->dev_private;

	if (dev_info != NULL) {
		dev_info->driver_id = dev->driver_id;
		dev_info->feature_flags = dev->feature_flags;
		dev_info->capabilities = ccp_pmd_capabilities;
		dev_info->max_nb_queue_pairs = internals->max_nb_qpairs;
		dev_info->sym.max_nb_sessions = internals->max_nb_sessions;
	}
}

static unsigned
ccp_pmd_session_get_size(struct rte_cryptodev *dev __rte_unused)
{
	return sizeof(struct ccp_session);
}

static int
ccp_pmd_session_configure(struct rte_cryptodev *dev,
			  struct rte_crypto_sym_xform *xform,
			  struct rte_cryptodev_sym_session *sess,
			  struct rte_mempool *mempool)
{
	int ret;
	void *sess_private_data;

	if (unlikely(sess == NULL || xform == NULL)) {
		CCP_LOG_ERR("Invalid session struct or xform");
		return -ENOMEM;
	}

	if (rte_mempool_get(mempool, &sess_private_data)) {
		CCP_LOG_ERR("Couldn't get object from session mempool");
		return -ENOMEM;
	}
	ret = ccp_set_session_parameters(sess_private_data, xform);
	if (ret != 0) {
		CCP_LOG_ERR("failed configure session parameters");

		/* Return session to mempool */
		rte_mempool_put(mempool, sess_private_data);
		return ret;
	}
	set_session_private_data(sess, dev->driver_id,
				 sess_private_data);

	return 0;
}

static void
ccp_pmd_session_clear(struct rte_cryptodev *dev,
		      struct rte_cryptodev_sym_session *sess)
{
	uint8_t index = dev->driver_id;
	void *sess_priv = get_session_private_data(sess, index);

	if (sess_priv) {
		struct rte_mempool *sess_mp = rte_mempool_from_obj(sess_priv);

		rte_mempool_put(sess_mp, sess_priv);
		memset(sess_priv, 0, sizeof(struct ccp_session));
		set_session_private_data(sess, index, NULL);
	}
}

struct rte_cryptodev_ops ccp_ops = {
		.dev_configure		= ccp_pmd_config,
		.dev_start		= ccp_pmd_start,
		.dev_stop		= ccp_pmd_stop,
		.dev_close		= ccp_pmd_close,

		.stats_get		= NULL,
		.stats_reset		= NULL,

		.dev_infos_get		= ccp_pmd_info_get,

		.queue_pair_setup	= NULL,
		.queue_pair_release	= NULL,
		.queue_pair_start	= NULL,
		.queue_pair_stop	= NULL,
		.queue_pair_count	= NULL,

		.session_get_size	= ccp_pmd_session_get_size,
		.session_configure	= ccp_pmd_session_configure,
		.session_clear		= ccp_pmd_session_clear,
};

struct rte_cryptodev_ops *ccp_pmd_ops = &ccp_ops;
