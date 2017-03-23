/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 6WIND S.A.
 *   Copyright 2017 Mellanox.
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
 *     * Neither the name of 6WIND S.A. nor the names of its
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

#include <sys/queue.h>

#include <rte_malloc.h>
#include <rte_eth_tap.h>
#include <tap_flow.h>

struct rte_flow {
	LIST_ENTRY(rte_flow) next; /* Pointer to the next rte_flow structure */
};

static int
tap_flow_validate(struct rte_eth_dev *dev,
		  const struct rte_flow_attr *attr,
		  const struct rte_flow_item items[],
		  const struct rte_flow_action actions[],
		  struct rte_flow_error *error);

static struct rte_flow *
tap_flow_create(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item items[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error);

static int
tap_flow_destroy(struct rte_eth_dev *dev,
		 struct rte_flow *flow,
		 struct rte_flow_error *error);

static const struct rte_flow_ops tap_flow_ops = {
	.validate = tap_flow_validate,
	.create = tap_flow_create,
	.destroy = tap_flow_destroy,
	.flush = tap_flow_flush,
};

/**
 * Validate a flow.
 *
 * @see rte_flow_validate()
 * @see rte_flow_ops
 */
static int
tap_flow_validate(struct rte_eth_dev *dev __rte_unused,
		  const struct rte_flow_attr *attr __rte_unused,
		  const struct rte_flow_item items[] __rte_unused,
		  const struct rte_flow_action actions[] __rte_unused,
		  struct rte_flow_error *error)
{
	return -rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, "not implemented yet");
}

/**
 * Create a flow.
 *
 * @see rte_flow_create()
 * @see rte_flow_ops
 */
static struct rte_flow *
tap_flow_create(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item items[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct rte_flow *flow = NULL;

	if (tap_flow_validate(dev, attr, items, actions, error))
		return NULL;
	flow = rte_malloc(__func__, sizeof(struct rte_flow), 0);
	if (!flow) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "cannot allocate memory for rte_flow");
		return NULL;
	}
	LIST_INSERT_HEAD(&pmd->flows, flow, next);
	return flow;
}

/**
 * Destroy a flow.
 *
 * @see rte_flow_destroy()
 * @see rte_flow_ops
 */
static int
tap_flow_destroy(struct rte_eth_dev *dev __rte_unused,
		 struct rte_flow *flow,
		 struct rte_flow_error *error __rte_unused)
{
	LIST_REMOVE(flow, next);
	rte_free(flow);
	return 0;
}

/**
 * Destroy all flows.
 *
 * @see rte_flow_flush()
 * @see rte_flow_ops
 */
int
tap_flow_flush(struct rte_eth_dev *dev, struct rte_flow_error *error)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct rte_flow *flow;

	while (!LIST_EMPTY(&pmd->flows)) {
		flow = LIST_FIRST(&pmd->flows);
		if (tap_flow_destroy(dev, flow, error) < 0)
			return -1;
	}
	return 0;
}

/**
 * Manage filter operations.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param filter_type
 *   Filter type.
 * @param filter_op
 *   Operation to perform.
 * @param arg
 *   Pointer to operation-specific structure.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
int
tap_dev_filter_ctrl(struct rte_eth_dev *dev,
		    enum rte_filter_type filter_type,
		    enum rte_filter_op filter_op,
		    void *arg)
{
	switch (filter_type) {
	case RTE_ETH_FILTER_GENERIC:
		if (filter_op != RTE_ETH_FILTER_GET)
			return -EINVAL;
		*(const void **)arg = &tap_flow_ops;
		return 0;
	default:
		RTE_LOG(ERR, PMD, "%p: filter type (%d) not supported",
			(void *)dev, filter_type);
	}
	return -EINVAL;
}

