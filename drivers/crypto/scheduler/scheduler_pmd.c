/*-
 *   BSD LICENSE
 *
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
 *     * Neither the name of Intel Corporation nor the names of its
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
#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>
#include <rte_vdev.h>
#include <rte_malloc.h>
#include <rte_cpuflags.h>
#include <rte_reorder.h>
#include <rte_cryptodev_scheduler.h>

#include "scheduler_pmd_private.h"

struct scheduler_init_params {
	struct rte_crypto_vdev_init_params def_p;
	uint32_t nb_slaves;
	uint8_t slaves[MAX_SLAVES_NUM];
};

#define RTE_CRYPTODEV_VDEV_NAME				("name")
#define RTE_CRYPTODEV_VDEV_SLAVE			("slave")
#define RTE_CRYPTODEV_VDEV_MAX_NB_QP_ARG	("max_nb_queue_pairs")
#define RTE_CRYPTODEV_VDEV_MAX_NB_SESS_ARG	("max_nb_sessions")
#define RTE_CRYPTODEV_VDEV_SOCKET_ID		("socket_id")

const char *scheduler_valid_params[] = {
	RTE_CRYPTODEV_VDEV_NAME,
	RTE_CRYPTODEV_VDEV_SLAVE,
	RTE_CRYPTODEV_VDEV_MAX_NB_QP_ARG,
	RTE_CRYPTODEV_VDEV_MAX_NB_SESS_ARG,
	RTE_CRYPTODEV_VDEV_SOCKET_ID
};

static uint16_t
scheduler_enqueue_burst(void *queue_pair, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	struct scheduler_qp_ctx *qp_ctx = queue_pair;
	uint16_t processed_ops;

	processed_ops = (*qp_ctx->schedule_enqueue)(qp_ctx, ops,
			nb_ops);

	return processed_ops;
}

static uint16_t
scheduler_dequeue_burst(void *queue_pair, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	struct scheduler_qp_ctx *qp_ctx = queue_pair;
	uint16_t processed_ops;

	processed_ops = (*qp_ctx->schedule_dequeue)(qp_ctx, ops,
			nb_ops);

	return processed_ops;
}

static int
attach_init_slaves(uint8_t scheduler_id,
		const uint8_t *slaves, const uint8_t nb_slaves)
{
	uint8_t i;

	for (i = 0; i < nb_slaves; i++) {
		struct rte_cryptodev *dev =
				rte_cryptodev_pmd_get_dev(slaves[i]);
		int status = rte_cryptodev_scheduler_slave_attach(
				scheduler_id, slaves[i]);

		if (status < 0 || !dev) {
			CS_LOG_ERR("Failed to attach slave cryptodev "
					"%u.\n", slaves[i]);
			return status;
		}

		RTE_LOG(INFO, PMD, "  Attached slave cryptodev %s\n",
				dev->data->name);
	}

	return 0;
}

static int
cryptodev_scheduler_create(const char *name,
	struct scheduler_init_params *init_params)
{
	char crypto_dev_name[RTE_CRYPTODEV_NAME_MAX_LEN] = {0};
	struct rte_cryptodev *dev;
	struct scheduler_ctx *sched_ctx;

	if (init_params->def_p.name[0] == '\0') {
		int ret = rte_cryptodev_pmd_create_dev_name(
				crypto_dev_name,
				RTE_STR(CRYPTODEV_NAME_SCHEDULER_PMD));

		if (ret < 0) {
			CS_LOG_ERR("failed to create unique name");
			return ret;
		}
	} else {
		strncpy(crypto_dev_name, init_params->def_p.name,
				RTE_CRYPTODEV_NAME_MAX_LEN - 1);
	}

	dev = rte_cryptodev_pmd_virtual_dev_init(crypto_dev_name,
			sizeof(struct scheduler_ctx),
			init_params->def_p.socket_id);
	if (dev == NULL) {
		CS_LOG_ERR("driver %s: failed to create cryptodev vdev",
			name);
		return -EFAULT;
	}

	dev->dev_type = RTE_CRYPTODEV_SCHEDULER_PMD;
	dev->dev_ops = rte_crypto_scheduler_pmd_ops;

	dev->enqueue_burst = scheduler_enqueue_burst;
	dev->dequeue_burst = scheduler_dequeue_burst;

	sched_ctx = dev->data->dev_private;
	sched_ctx->max_nb_queue_pairs =
			init_params->def_p.max_nb_queue_pairs;

	return attach_init_slaves(dev->data->dev_id, init_params->slaves,
			init_params->nb_slaves);
}

static int
cryptodev_scheduler_remove(const char *name)
{
	struct rte_cryptodev *dev;
	struct scheduler_ctx *sched_ctx;

	if (name == NULL)
		return -EINVAL;

	dev = rte_cryptodev_pmd_get_named_dev(name);
	if (dev == NULL)
		return -EINVAL;

	sched_ctx = dev->data->dev_private;

	if (sched_ctx->nb_slaves) {
		uint32_t i;

		for (i = 0; i < sched_ctx->nb_slaves; i++)
			rte_cryptodev_scheduler_slave_detach(dev->data->dev_id,
					sched_ctx->slaves[i].dev_id);
	}

	RTE_LOG(INFO, PMD, "Closing Crypto Scheduler device %s on numa "
		"socket %u\n", name, rte_socket_id());

	return 0;
}

static uint8_t
number_of_sockets(void)
{
	int sockets = 0;
	int i;
	const struct rte_memseg *ms = rte_eal_get_physmem_layout();

	for (i = 0; ((i < RTE_MAX_MEMSEG) && (ms[i].addr != NULL)); i++) {
		if (sockets < ms[i].socket_id)
			sockets = ms[i].socket_id;
	}

	/* Number of sockets = maximum socket_id + 1 */
	return ++sockets;
}

/** Parse integer from integer argument */
static int
parse_integer_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	int *i = (int *) extra_args;

	*i = atoi(value);
	if (*i < 0) {
		CS_LOG_ERR("Argument has to be positive.\n");
		return -1;
	}

	return 0;
}

/** Parse name */
static int
parse_name_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	struct rte_crypto_vdev_init_params *params = extra_args;

	if (strlen(value) >= RTE_CRYPTODEV_NAME_MAX_LEN - 1) {
		CS_LOG_ERR("Invalid name %s, should be less than "
				"%u bytes.\n", value,
				RTE_CRYPTODEV_NAME_MAX_LEN - 1);
		return -1;
	}

	strncpy(params->name, value, RTE_CRYPTODEV_NAME_MAX_LEN);

	return 0;
}

/** Parse slave */
static int
parse_slave_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	struct scheduler_init_params *param = extra_args;
	struct rte_cryptodev *dev =
			rte_cryptodev_pmd_get_named_dev(value);

	if (!dev) {
		RTE_LOG(ERR, PMD, "Invalid slave name %s.\n", value);
		return -1;
	}

	if (param->nb_slaves >= MAX_SLAVES_NUM - 1) {
		CS_LOG_ERR("Too many slaves.\n");
		return -1;
	}

	param->slaves[param->nb_slaves] = dev->data->dev_id;
	param->nb_slaves++;

	return 0;
}

static int
scheduler_parse_init_params(struct scheduler_init_params *params,
		const char *input_args)
{
	struct rte_kvargs *kvlist = NULL;
	int ret = 0;

	if (params == NULL)
		return -EINVAL;

	if (input_args) {
		kvlist = rte_kvargs_parse(input_args,
				scheduler_valid_params);
		if (kvlist == NULL)
			return -1;

		ret = rte_kvargs_process(kvlist,
				RTE_CRYPTODEV_VDEV_MAX_NB_QP_ARG,
				&parse_integer_arg,
				&params->def_p.max_nb_queue_pairs);
		if (ret < 0)
			goto free_kvlist;

		ret = rte_kvargs_process(kvlist,
				RTE_CRYPTODEV_VDEV_MAX_NB_SESS_ARG,
				&parse_integer_arg,
				&params->def_p.max_nb_sessions);
		if (ret < 0)
			goto free_kvlist;

		ret = rte_kvargs_process(kvlist, RTE_CRYPTODEV_VDEV_SOCKET_ID,
				&parse_integer_arg,
				&params->def_p.socket_id);
		if (ret < 0)
			goto free_kvlist;

		ret = rte_kvargs_process(kvlist, RTE_CRYPTODEV_VDEV_NAME,
				&parse_name_arg,
				&params->def_p);
		if (ret < 0)
			goto free_kvlist;

		ret = rte_kvargs_process(kvlist, RTE_CRYPTODEV_VDEV_SLAVE,
				&parse_slave_arg, params);
		if (ret < 0)
			goto free_kvlist;

		if (params->def_p.socket_id >= number_of_sockets()) {
			CDEV_LOG_ERR("Invalid socket id specified to create "
				"the virtual crypto device on");
			goto free_kvlist;
		}
	}

free_kvlist:
	rte_kvargs_free(kvlist);
	return ret;
}

static int
cryptodev_scheduler_probe(const char *name, const char *input_args)
{
	struct scheduler_init_params init_params = {
		.def_p = {
			RTE_CRYPTODEV_VDEV_DEFAULT_MAX_NB_QUEUE_PAIRS,
			RTE_CRYPTODEV_VDEV_DEFAULT_MAX_NB_SESSIONS,
			rte_socket_id(),
			""
		},
		.nb_slaves = 0,
		.slaves = {0}
	};

	scheduler_parse_init_params(&init_params, input_args);

	RTE_LOG(INFO, PMD, "Initialising %s on NUMA node %d\n", name,
			init_params.def_p.socket_id);
	RTE_LOG(INFO, PMD, "  Max number of queue pairs = %d\n",
			init_params.def_p.max_nb_queue_pairs);
	RTE_LOG(INFO, PMD, "  Max number of sessions = %d\n",
			init_params.def_p.max_nb_sessions);
	if (init_params.def_p.name[0] != '\0')
		RTE_LOG(INFO, PMD, "  User defined name = %s\n",
			init_params.def_p.name);

	return cryptodev_scheduler_create(name, &init_params);
}

static struct rte_vdev_driver cryptodev_scheduler_pmd_drv = {
	.probe = cryptodev_scheduler_probe,
	.remove = cryptodev_scheduler_remove
};

RTE_PMD_REGISTER_VDEV(CRYPTODEV_NAME_SCHEDULER_PMD,
	cryptodev_scheduler_pmd_drv);
RTE_PMD_REGISTER_PARAM_STRING(CRYPTODEV_NAME_SCHEDULER_PMD,
	"max_nb_queue_pairs=<int> "
	"max_nb_sessions=<int> "
	"socket_id=<int> "
	"slave=<name>");
