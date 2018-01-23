/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 NXP.
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of NXP nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <rte_malloc.h>
#include <rte_dev.h>
#include "rte_compat.h"
#include "rte_security.h"
#include "rte_security_driver.h"

struct rte_security_session *
__rte_experimental rte_security_session_create(struct rte_security_ctx *instance,
			    struct rte_security_session_conf *conf,
			    struct rte_mempool *mp)
{
	struct rte_security_session *sess = NULL;

	if (conf == NULL)
		return NULL;

	RTE_FUNC_PTR_OR_ERR_RET(*instance->ops->session_create, NULL);

	if (rte_mempool_get(mp, (void **)&sess))
		return NULL;

	if (instance->ops->session_create(instance->device, conf, sess, mp)) {
		rte_mempool_put(mp, (void *)sess);
		return NULL;
	}
	instance->sess_cnt++;

	return sess;
}

int __rte_experimental
rte_security_session_update(struct rte_security_ctx *instance,
			    struct rte_security_session *sess,
			    struct rte_security_session_conf *conf)
{
	RTE_FUNC_PTR_OR_ERR_RET(*instance->ops->session_update, -ENOTSUP);
	return instance->ops->session_update(instance->device, sess, conf);
}

unsigned int __rte_experimental
rte_security_session_get_size(struct rte_security_ctx *instance)
{
	RTE_FUNC_PTR_OR_ERR_RET(*instance->ops->session_get_size, 0);
	return instance->ops->session_get_size(instance->device);
}

int __rte_experimental
rte_security_session_stats_get(struct rte_security_ctx *instance,
			       struct rte_security_session *sess,
			       struct rte_security_stats *stats)
{
	RTE_FUNC_PTR_OR_ERR_RET(*instance->ops->session_stats_get, -ENOTSUP);
	return instance->ops->session_stats_get(instance->device, sess, stats);
}

int __rte_experimental
rte_security_session_destroy(struct rte_security_ctx *instance,
			     struct rte_security_session *sess)
{
	int ret;
	struct rte_mempool *mp = rte_mempool_from_obj(sess);

	RTE_FUNC_PTR_OR_ERR_RET(*instance->ops->session_destroy, -ENOTSUP);

	if (instance->sess_cnt)
		instance->sess_cnt--;

	ret = instance->ops->session_destroy(instance->device, sess);
	if (!ret)
		rte_mempool_put(mp, (void *)sess);

	return ret;
}

int __rte_experimental
rte_security_set_pkt_metadata(struct rte_security_ctx *instance,
			      struct rte_security_session *sess,
			      struct rte_mbuf *m, void *params)
{
	RTE_FUNC_PTR_OR_ERR_RET(*instance->ops->set_pkt_metadata, -ENOTSUP);
	return instance->ops->set_pkt_metadata(instance->device,
					       sess, m, params);
}

void * __rte_experimental
rte_security_get_userdata(struct rte_security_ctx *instance, uint64_t md)
{
	void *userdata = NULL;

	RTE_FUNC_PTR_OR_ERR_RET(*instance->ops->get_userdata, NULL);
	if (instance->ops->get_userdata(instance->device, md, &userdata))
		return NULL;

	return userdata;
}

const struct rte_security_capability * __rte_experimental
rte_security_capabilities_get(struct rte_security_ctx *instance)
{
	RTE_FUNC_PTR_OR_ERR_RET(*instance->ops->capabilities_get, NULL);
	return instance->ops->capabilities_get(instance->device);
}

const struct rte_security_capability * __rte_experimental
rte_security_capability_get(struct rte_security_ctx *instance,
			    struct rte_security_capability_idx *idx)
{
	const struct rte_security_capability *capabilities;
	const struct rte_security_capability *capability;
	uint16_t i = 0;

	RTE_FUNC_PTR_OR_ERR_RET(*instance->ops->capabilities_get, NULL);
	capabilities = instance->ops->capabilities_get(instance->device);

	if (capabilities == NULL)
		return NULL;

	while ((capability = &capabilities[i++])->action
			!= RTE_SECURITY_ACTION_TYPE_NONE) {
		if (capability->action  == idx->action &&
				capability->protocol == idx->protocol) {
			if (idx->protocol == RTE_SECURITY_PROTOCOL_IPSEC) {
				if (capability->ipsec.proto ==
						idx->ipsec.proto &&
					capability->ipsec.mode ==
							idx->ipsec.mode &&
					capability->ipsec.direction ==
							idx->ipsec.direction)
					return capability;
			}
		}
	}

	return NULL;
}
