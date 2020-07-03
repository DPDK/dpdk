/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 NXP.
 * Copyright(c) 2017 Intel Corporation.
 * Copyright (c) 2020 Samsung Electronics Co., Ltd All Rights Reserved
 */

#include <rte_malloc.h>
#include <rte_dev.h>
#include "rte_compat.h"
#include "rte_security.h"
#include "rte_security_driver.h"

/* Macro to check for invalid pointers */
#define RTE_PTR_OR_ERR_RET(ptr, retval) do {	\
	if ((ptr) == NULL)			\
		return retval;			\
} while (0)

/* Macro to check for invalid pointers chains */
#define RTE_PTR_CHAIN3_OR_ERR_RET(p1, p2, p3, retval, last_retval) do {	\
	RTE_PTR_OR_ERR_RET(p1, retval);					\
	RTE_PTR_OR_ERR_RET(p1->p2, retval);				\
	RTE_PTR_OR_ERR_RET(p1->p2->p3, last_retval);			\
} while (0)

struct rte_security_session *
rte_security_session_create(struct rte_security_ctx *instance,
			    struct rte_security_session_conf *conf,
			    struct rte_mempool *mp)
{
	struct rte_security_session *sess = NULL;

	RTE_PTR_CHAIN3_OR_ERR_RET(instance, ops, session_create, NULL, NULL);
	RTE_PTR_OR_ERR_RET(conf, NULL);
	RTE_PTR_OR_ERR_RET(mp, NULL);

	if (rte_mempool_get(mp, (void **)&sess))
		return NULL;

	if (instance->ops->session_create(instance->device, conf, sess, mp)) {
		rte_mempool_put(mp, (void *)sess);
		return NULL;
	}
	instance->sess_cnt++;

	return sess;
}

int
rte_security_session_update(struct rte_security_ctx *instance,
			    struct rte_security_session *sess,
			    struct rte_security_session_conf *conf)
{
	RTE_PTR_CHAIN3_OR_ERR_RET(instance, ops, session_update, -EINVAL,
			-ENOTSUP);
	RTE_PTR_OR_ERR_RET(sess, -EINVAL);
	RTE_PTR_OR_ERR_RET(conf, -EINVAL);

	return instance->ops->session_update(instance->device, sess, conf);
}

unsigned int
rte_security_session_get_size(struct rte_security_ctx *instance)
{
	RTE_PTR_CHAIN3_OR_ERR_RET(instance, ops, session_get_size, 0, 0);

	return instance->ops->session_get_size(instance->device);
}

int
rte_security_session_stats_get(struct rte_security_ctx *instance,
			       struct rte_security_session *sess,
			       struct rte_security_stats *stats)
{
	RTE_PTR_CHAIN3_OR_ERR_RET(instance, ops, session_stats_get, -EINVAL,
			-ENOTSUP);
	/* Parameter sess can be NULL in case of getting global statistics. */
	RTE_PTR_OR_ERR_RET(stats, -EINVAL);

	return instance->ops->session_stats_get(instance->device, sess, stats);
}

int
rte_security_session_destroy(struct rte_security_ctx *instance,
			     struct rte_security_session *sess)
{
	int ret;

	RTE_PTR_CHAIN3_OR_ERR_RET(instance, ops, session_destroy, -EINVAL,
			-ENOTSUP);
	RTE_PTR_OR_ERR_RET(sess, -EINVAL);

	ret = instance->ops->session_destroy(instance->device, sess);
	if (ret != 0)
		return ret;

	rte_mempool_put(rte_mempool_from_obj(sess), (void *)sess);

	if (instance->sess_cnt)
		instance->sess_cnt--;

	return 0;
}

int
rte_security_set_pkt_metadata(struct rte_security_ctx *instance,
			      struct rte_security_session *sess,
			      struct rte_mbuf *m, void *params)
{
#ifdef RTE_DEBUG
	RTE_PTR_OR_ERR_RET(sess, -EINVAL);
	RTE_PTR_OR_ERR_RET(instance, -EINVAL);
	RTE_PTR_OR_ERR_RET(instance->ops, -EINVAL);
#endif
	RTE_FUNC_PTR_OR_ERR_RET(*instance->ops->set_pkt_metadata, -ENOTSUP);
	return instance->ops->set_pkt_metadata(instance->device,
					       sess, m, params);
}

void *
rte_security_get_userdata(struct rte_security_ctx *instance, uint64_t md)
{
	void *userdata = NULL;

#ifdef RTE_DEBUG
	RTE_PTR_OR_ERR_RET(instance, NULL);
	RTE_PTR_OR_ERR_RET(instance->ops, NULL);
#endif
	RTE_FUNC_PTR_OR_ERR_RET(*instance->ops->get_userdata, NULL);
	if (instance->ops->get_userdata(instance->device, md, &userdata))
		return NULL;

	return userdata;
}

const struct rte_security_capability *
rte_security_capabilities_get(struct rte_security_ctx *instance)
{
	RTE_PTR_CHAIN3_OR_ERR_RET(instance, ops, capabilities_get, NULL, NULL);

	return instance->ops->capabilities_get(instance->device);
}

const struct rte_security_capability *
rte_security_capability_get(struct rte_security_ctx *instance,
			    struct rte_security_capability_idx *idx)
{
	const struct rte_security_capability *capabilities;
	const struct rte_security_capability *capability;
	uint16_t i = 0;

	RTE_PTR_CHAIN3_OR_ERR_RET(instance, ops, capabilities_get, NULL, NULL);
	RTE_PTR_OR_ERR_RET(idx, NULL);

	capabilities = instance->ops->capabilities_get(instance->device);

	if (capabilities == NULL)
		return NULL;

	while ((capability = &capabilities[i++])->action
			!= RTE_SECURITY_ACTION_TYPE_NONE) {
		if (capability->action == idx->action &&
				capability->protocol == idx->protocol) {
			if (idx->protocol == RTE_SECURITY_PROTOCOL_IPSEC) {
				if (capability->ipsec.proto ==
						idx->ipsec.proto &&
					capability->ipsec.mode ==
							idx->ipsec.mode &&
					capability->ipsec.direction ==
							idx->ipsec.direction)
					return capability;
			} else if (idx->protocol == RTE_SECURITY_PROTOCOL_PDCP) {
				if (capability->pdcp.domain ==
							idx->pdcp.domain)
					return capability;
			} else if (idx->protocol ==
						RTE_SECURITY_PROTOCOL_DOCSIS) {
				if (capability->docsis.direction ==
							idx->docsis.direction)
					return capability;
			}
		}
	}

	return NULL;
}
