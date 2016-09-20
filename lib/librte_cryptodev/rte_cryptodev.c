/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015-2016 Intel Corporation. All rights reserved.
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

#include <sys/types.h>
#include <sys/queue.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <netinet/in.h>

#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_dev.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_errno.h>
#include <rte_spinlock.h>
#include <rte_string_fns.h>

#include "rte_crypto.h"
#include "rte_cryptodev.h"
#include "rte_cryptodev_pmd.h"

struct rte_cryptodev rte_crypto_devices[RTE_CRYPTO_MAX_DEVS];

struct rte_cryptodev *rte_cryptodevs = &rte_crypto_devices[0];

static struct rte_cryptodev_global cryptodev_globals = {
		.devs			= &rte_crypto_devices[0],
		.data			= { NULL },
		.nb_devs		= 0,
		.max_devs		= RTE_CRYPTO_MAX_DEVS
};

struct rte_cryptodev_global *rte_cryptodev_globals = &cryptodev_globals;

/* spinlock for crypto device callbacks */
static rte_spinlock_t rte_cryptodev_cb_lock = RTE_SPINLOCK_INITIALIZER;


/**
 * The user application callback description.
 *
 * It contains callback address to be registered by user application,
 * the pointer to the parameters for callback, and the event type.
 */
struct rte_cryptodev_callback {
	TAILQ_ENTRY(rte_cryptodev_callback) next; /**< Callbacks list */
	rte_cryptodev_cb_fn cb_fn;		/**< Callback address */
	void *cb_arg;				/**< Parameter for callback */
	enum rte_cryptodev_event_type event;	/**< Interrupt event type */
	uint32_t active;			/**< Callback is executing */
};

#define RTE_CRYPTODEV_VDEV_MAX_NB_QP_ARG		("max_nb_queue_pairs")
#define RTE_CRYPTODEV_VDEV_MAX_NB_SESS_ARG		("max_nb_sessions")
#define RTE_CRYPTODEV_VDEV_SOCKET_ID			("socket_id")

static const char *cryptodev_vdev_valid_params[] = {
	RTE_CRYPTODEV_VDEV_MAX_NB_QP_ARG,
	RTE_CRYPTODEV_VDEV_MAX_NB_SESS_ARG,
	RTE_CRYPTODEV_VDEV_SOCKET_ID
};

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
		CDEV_LOG_ERR("Argument has to be positive.");
		return -1;
	}

	return 0;
}

int
rte_cryptodev_parse_vdev_init_params(struct rte_crypto_vdev_init_params *params,
		const char *input_args)
{
	struct rte_kvargs *kvlist = NULL;
	int ret = 0;

	if (params == NULL)
		return -EINVAL;

	if (input_args) {
		kvlist = rte_kvargs_parse(input_args,
				cryptodev_vdev_valid_params);
		if (kvlist == NULL)
			return -1;

		ret = rte_kvargs_process(kvlist,
					RTE_CRYPTODEV_VDEV_MAX_NB_QP_ARG,
					&parse_integer_arg,
					&params->max_nb_queue_pairs);
		if (ret < 0)
			goto free_kvlist;

		ret = rte_kvargs_process(kvlist,
					RTE_CRYPTODEV_VDEV_MAX_NB_SESS_ARG,
					&parse_integer_arg,
					&params->max_nb_sessions);
		if (ret < 0)
			goto free_kvlist;

		ret = rte_kvargs_process(kvlist, RTE_CRYPTODEV_VDEV_SOCKET_ID,
					&parse_integer_arg,
					&params->socket_id);
		if (ret < 0)
			goto free_kvlist;

		if (params->socket_id >= number_of_sockets()) {
			CDEV_LOG_ERR("Invalid socket id specified to create "
				"the virtual crypto device on");
			goto free_kvlist;
		}
	}

free_kvlist:
	rte_kvargs_free(kvlist);
	return ret;
}

const char *
rte_cryptodev_get_feature_name(uint64_t flag)
{
	switch (flag) {
	case RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO:
		return "SYMMETRIC_CRYPTO";
	case RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO:
		return "ASYMMETRIC_CRYPTO";
	case RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING:
		return "SYM_OPERATION_CHAINING";
	case RTE_CRYPTODEV_FF_CPU_SSE:
		return "CPU_SSE";
	case RTE_CRYPTODEV_FF_CPU_AVX:
		return "CPU_AVX";
	case RTE_CRYPTODEV_FF_CPU_AVX2:
		return "CPU_AVX2";
	case RTE_CRYPTODEV_FF_CPU_AESNI:
		return "CPU_AESNI";
	case RTE_CRYPTODEV_FF_HW_ACCELERATED:
		return "HW_ACCELERATED";

	default:
		return NULL;
	}
}


int
rte_cryptodev_create_vdev(const char *name, const char *args)
{
	return rte_eal_vdev_init(name, args);
}

int
rte_cryptodev_get_dev_id(const char *name) {
	unsigned i;

	if (name == NULL)
		return -1;

	for (i = 0; i < rte_cryptodev_globals->max_devs; i++)
		if ((strcmp(rte_cryptodev_globals->devs[i].data->name, name)
				== 0) &&
				(rte_cryptodev_globals->devs[i].attached ==
						RTE_CRYPTODEV_ATTACHED))
			return i;

	return -1;
}

uint8_t
rte_cryptodev_count(void)
{
	return rte_cryptodev_globals->nb_devs;
}

uint8_t
rte_cryptodev_count_devtype(enum rte_cryptodev_type type)
{
	uint8_t i, dev_count = 0;

	for (i = 0; i < rte_cryptodev_globals->max_devs; i++)
		if (rte_cryptodev_globals->devs[i].dev_type == type &&
			rte_cryptodev_globals->devs[i].attached ==
					RTE_CRYPTODEV_ATTACHED)
			dev_count++;

	return dev_count;
}

int
rte_cryptodev_socket_id(uint8_t dev_id)
{
	struct rte_cryptodev *dev;

	if (!rte_cryptodev_pmd_is_valid_dev(dev_id))
		return -1;

	dev = rte_cryptodev_pmd_get_dev(dev_id);

	return dev->data->socket_id;
}

static inline int
rte_cryptodev_data_alloc(uint8_t dev_id, struct rte_cryptodev_data **data,
		int socket_id)
{
	char mz_name[RTE_CRYPTODEV_NAME_MAX_LEN];
	const struct rte_memzone *mz;
	int n;

	/* generate memzone name */
	n = snprintf(mz_name, sizeof(mz_name), "rte_cryptodev_data_%u", dev_id);
	if (n >= (int)sizeof(mz_name))
		return -EINVAL;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		mz = rte_memzone_reserve(mz_name,
				sizeof(struct rte_cryptodev_data),
				socket_id, 0);
	} else
		mz = rte_memzone_lookup(mz_name);

	if (mz == NULL)
		return -ENOMEM;

	*data = mz->addr;
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		memset(*data, 0, sizeof(struct rte_cryptodev_data));

	return 0;
}

static uint8_t
rte_cryptodev_find_free_device_index(void)
{
	uint8_t dev_id;

	for (dev_id = 0; dev_id < RTE_CRYPTO_MAX_DEVS; dev_id++) {
		if (rte_crypto_devices[dev_id].attached ==
				RTE_CRYPTODEV_DETACHED)
			return dev_id;
	}
	return RTE_CRYPTO_MAX_DEVS;
}

struct rte_cryptodev *
rte_cryptodev_pmd_allocate(const char *name, int socket_id)
{
	struct rte_cryptodev *cryptodev;
	uint8_t dev_id;

	if (rte_cryptodev_pmd_get_named_dev(name) != NULL) {
		CDEV_LOG_ERR("Crypto device with name %s already "
				"allocated!", name);
		return NULL;
	}

	dev_id = rte_cryptodev_find_free_device_index();
	if (dev_id == RTE_CRYPTO_MAX_DEVS) {
		CDEV_LOG_ERR("Reached maximum number of crypto devices");
		return NULL;
	}

	cryptodev = rte_cryptodev_pmd_get_dev(dev_id);

	if (cryptodev->data == NULL) {
		struct rte_cryptodev_data *cryptodev_data =
				cryptodev_globals.data[dev_id];

		int retval = rte_cryptodev_data_alloc(dev_id, &cryptodev_data,
				socket_id);

		if (retval < 0 || cryptodev_data == NULL)
			return NULL;

		cryptodev->data = cryptodev_data;

		snprintf(cryptodev->data->name, RTE_CRYPTODEV_NAME_MAX_LEN,
				"%s", name);

		cryptodev->data->dev_id = dev_id;
		cryptodev->data->socket_id = socket_id;
		cryptodev->data->dev_started = 0;

		cryptodev->attached = RTE_CRYPTODEV_ATTACHED;

		cryptodev_globals.nb_devs++;
	}

	return cryptodev;
}

int
rte_cryptodev_pmd_release_device(struct rte_cryptodev *cryptodev)
{
	int ret;

	if (cryptodev == NULL)
		return -EINVAL;

	ret = rte_cryptodev_close(cryptodev->data->dev_id);
	if (ret < 0)
		return ret;

	cryptodev->attached = RTE_CRYPTODEV_DETACHED;
	cryptodev_globals.nb_devs--;
	return 0;
}

struct rte_cryptodev *
rte_cryptodev_pmd_virtual_dev_init(const char *name, size_t dev_private_size,
		int socket_id)
{
	struct rte_cryptodev *cryptodev;

	/* allocate device structure */
	cryptodev = rte_cryptodev_pmd_allocate(name, socket_id);
	if (cryptodev == NULL)
		return NULL;

	/* allocate private device structure */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		cryptodev->data->dev_private =
				rte_zmalloc_socket("cryptodev device private",
						dev_private_size,
						RTE_CACHE_LINE_SIZE,
						socket_id);

		if (cryptodev->data->dev_private == NULL)
			rte_panic("Cannot allocate memzone for private device"
					" data");
	}

	/* initialise user call-back tail queue */
	TAILQ_INIT(&(cryptodev->link_intr_cbs));

	return cryptodev;
}

int
rte_cryptodev_pci_probe(struct rte_pci_driver *pci_drv,
			struct rte_pci_device *pci_dev)
{
	struct rte_cryptodev_driver *cryptodrv;
	struct rte_cryptodev *cryptodev;

	char cryptodev_name[RTE_CRYPTODEV_NAME_MAX_LEN];

	int retval;

	cryptodrv = (struct rte_cryptodev_driver *)pci_drv;
	if (cryptodrv == NULL)
		return -ENODEV;

	rte_eal_pci_device_name(&pci_dev->addr, cryptodev_name,
			sizeof(cryptodev_name));

	cryptodev = rte_cryptodev_pmd_allocate(cryptodev_name, rte_socket_id());
	if (cryptodev == NULL)
		return -ENOMEM;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		cryptodev->data->dev_private =
				rte_zmalloc_socket(
						"cryptodev private structure",
						cryptodrv->dev_private_size,
						RTE_CACHE_LINE_SIZE,
						rte_socket_id());

		if (cryptodev->data->dev_private == NULL)
			rte_panic("Cannot allocate memzone for private "
					"device data");
	}

	cryptodev->pci_dev = pci_dev;
	cryptodev->driver = cryptodrv;

	/* init user callbacks */
	TAILQ_INIT(&(cryptodev->link_intr_cbs));

	/* Invoke PMD device initialization function */
	retval = (*cryptodrv->cryptodev_init)(cryptodrv, cryptodev);
	if (retval == 0)
		return 0;

	CDEV_LOG_ERR("driver %s: crypto_dev_init(vendor_id=0x%x device_id=0x%x)"
			" failed", pci_drv->driver.name,
			(unsigned) pci_dev->id.vendor_id,
			(unsigned) pci_dev->id.device_id);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_free(cryptodev->data->dev_private);

	cryptodev->attached = RTE_CRYPTODEV_DETACHED;
	cryptodev_globals.nb_devs--;

	return -ENXIO;
}

int
rte_cryptodev_pci_remove(struct rte_pci_device *pci_dev)
{
	const struct rte_cryptodev_driver *cryptodrv;
	struct rte_cryptodev *cryptodev;
	char cryptodev_name[RTE_CRYPTODEV_NAME_MAX_LEN];
	int ret;

	if (pci_dev == NULL)
		return -EINVAL;

	rte_eal_pci_device_name(&pci_dev->addr, cryptodev_name,
			sizeof(cryptodev_name));

	cryptodev = rte_cryptodev_pmd_get_named_dev(cryptodev_name);
	if (cryptodev == NULL)
		return -ENODEV;

	cryptodrv = (const struct rte_cryptodev_driver *)pci_dev->driver;
	if (cryptodrv == NULL)
		return -ENODEV;

	/* Invoke PMD device uninit function */
	if (*cryptodrv->cryptodev_uninit) {
		ret = (*cryptodrv->cryptodev_uninit)(cryptodrv, cryptodev);
		if (ret)
			return ret;
	}

	/* free crypto device */
	rte_cryptodev_pmd_release_device(cryptodev);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_free(cryptodev->data->dev_private);

	cryptodev->pci_dev = NULL;
	cryptodev->driver = NULL;
	cryptodev->data = NULL;

	return 0;
}

uint16_t
rte_cryptodev_queue_pair_count(uint8_t dev_id)
{
	struct rte_cryptodev *dev;

	dev = &rte_crypto_devices[dev_id];
	return dev->data->nb_queue_pairs;
}

static int
rte_cryptodev_queue_pairs_config(struct rte_cryptodev *dev, uint16_t nb_qpairs,
		int socket_id)
{
	struct rte_cryptodev_info dev_info;
	void **qp;
	unsigned i;

	if ((dev == NULL) || (nb_qpairs < 1)) {
		CDEV_LOG_ERR("invalid param: dev %p, nb_queues %u",
							dev, nb_qpairs);
		return -EINVAL;
	}

	CDEV_LOG_DEBUG("Setup %d queues pairs on device %u",
			nb_qpairs, dev->data->dev_id);

	memset(&dev_info, 0, sizeof(struct rte_cryptodev_info));

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_infos_get, -ENOTSUP);
	(*dev->dev_ops->dev_infos_get)(dev, &dev_info);

	if (nb_qpairs > (dev_info.max_nb_queue_pairs)) {
		CDEV_LOG_ERR("Invalid num queue_pairs (%u) for dev %u",
				nb_qpairs, dev->data->dev_id);
	    return -EINVAL;
	}

	if (dev->data->queue_pairs == NULL) { /* first time configuration */
		dev->data->queue_pairs = rte_zmalloc_socket(
				"cryptodev->queue_pairs",
				sizeof(dev->data->queue_pairs[0]) * nb_qpairs,
				RTE_CACHE_LINE_SIZE, socket_id);

		if (dev->data->queue_pairs == NULL) {
			dev->data->nb_queue_pairs = 0;
			CDEV_LOG_ERR("failed to get memory for qp meta data, "
							"nb_queues %u",
							nb_qpairs);
			return -(ENOMEM);
		}
	} else { /* re-configure */
		int ret;
		uint16_t old_nb_queues = dev->data->nb_queue_pairs;

		qp = dev->data->queue_pairs;

		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->queue_pair_release,
				-ENOTSUP);

		for (i = nb_qpairs; i < old_nb_queues; i++) {
			ret = (*dev->dev_ops->queue_pair_release)(dev, i);
			if (ret < 0)
				return ret;
		}

		qp = rte_realloc(qp, sizeof(qp[0]) * nb_qpairs,
				RTE_CACHE_LINE_SIZE);
		if (qp == NULL) {
			CDEV_LOG_ERR("failed to realloc qp meta data,"
						" nb_queues %u", nb_qpairs);
			return -(ENOMEM);
		}

		if (nb_qpairs > old_nb_queues) {
			uint16_t new_qs = nb_qpairs - old_nb_queues;

			memset(qp + old_nb_queues, 0,
				sizeof(qp[0]) * new_qs);
		}

		dev->data->queue_pairs = qp;

	}
	dev->data->nb_queue_pairs = nb_qpairs;
	return 0;
}

int
rte_cryptodev_queue_pair_start(uint8_t dev_id, uint16_t queue_pair_id)
{
	struct rte_cryptodev *dev;

	if (!rte_cryptodev_pmd_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%" PRIu8, dev_id);
		return -EINVAL;
	}

	dev = &rte_crypto_devices[dev_id];
	if (queue_pair_id >= dev->data->nb_queue_pairs) {
		CDEV_LOG_ERR("Invalid queue_pair_id=%d", queue_pair_id);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->queue_pair_start, -ENOTSUP);

	return dev->dev_ops->queue_pair_start(dev, queue_pair_id);

}

int
rte_cryptodev_queue_pair_stop(uint8_t dev_id, uint16_t queue_pair_id)
{
	struct rte_cryptodev *dev;

	if (!rte_cryptodev_pmd_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%" PRIu8, dev_id);
		return -EINVAL;
	}

	dev = &rte_crypto_devices[dev_id];
	if (queue_pair_id >= dev->data->nb_queue_pairs) {
		CDEV_LOG_ERR("Invalid queue_pair_id=%d", queue_pair_id);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->queue_pair_stop, -ENOTSUP);

	return dev->dev_ops->queue_pair_stop(dev, queue_pair_id);

}

static int
rte_cryptodev_sym_session_pool_create(struct rte_cryptodev *dev,
		unsigned nb_objs, unsigned obj_cache_size, int socket_id);

int
rte_cryptodev_configure(uint8_t dev_id, struct rte_cryptodev_config *config)
{
	struct rte_cryptodev *dev;
	int diag;

	if (!rte_cryptodev_pmd_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%" PRIu8, dev_id);
		return -EINVAL;
	}

	dev = &rte_crypto_devices[dev_id];

	if (dev->data->dev_started) {
		CDEV_LOG_ERR(
		    "device %d must be stopped to allow configuration", dev_id);
		return -EBUSY;
	}

	/* Setup new number of queue pairs and reconfigure device. */
	diag = rte_cryptodev_queue_pairs_config(dev, config->nb_queue_pairs,
			config->socket_id);
	if (diag != 0) {
		CDEV_LOG_ERR("dev%d rte_crypto_dev_queue_pairs_config = %d",
				dev_id, diag);
		return diag;
	}

	/* Setup Session mempool for device */
	return rte_cryptodev_sym_session_pool_create(dev,
			config->session_mp.nb_objs,
			config->session_mp.cache_size,
			config->socket_id);
}


int
rte_cryptodev_start(uint8_t dev_id)
{
	struct rte_cryptodev *dev;
	int diag;

	CDEV_LOG_DEBUG("Start dev_id=%" PRIu8, dev_id);

	if (!rte_cryptodev_pmd_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%" PRIu8, dev_id);
		return -EINVAL;
	}

	dev = &rte_crypto_devices[dev_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_start, -ENOTSUP);

	if (dev->data->dev_started != 0) {
		CDEV_LOG_ERR("Device with dev_id=%" PRIu8 " already started",
			dev_id);
		return 0;
	}

	diag = (*dev->dev_ops->dev_start)(dev);
	if (diag == 0)
		dev->data->dev_started = 1;
	else
		return diag;

	return 0;
}

void
rte_cryptodev_stop(uint8_t dev_id)
{
	struct rte_cryptodev *dev;

	if (!rte_cryptodev_pmd_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%" PRIu8, dev_id);
		return;
	}

	dev = &rte_crypto_devices[dev_id];

	RTE_FUNC_PTR_OR_RET(*dev->dev_ops->dev_stop);

	if (dev->data->dev_started == 0) {
		CDEV_LOG_ERR("Device with dev_id=%" PRIu8 " already stopped",
			dev_id);
		return;
	}

	dev->data->dev_started = 0;
	(*dev->dev_ops->dev_stop)(dev);
}

int
rte_cryptodev_close(uint8_t dev_id)
{
	struct rte_cryptodev *dev;
	int retval;

	if (!rte_cryptodev_pmd_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%" PRIu8, dev_id);
		return -1;
	}

	dev = &rte_crypto_devices[dev_id];

	/* Device must be stopped before it can be closed */
	if (dev->data->dev_started == 1) {
		CDEV_LOG_ERR("Device %u must be stopped before closing",
				dev_id);
		return -EBUSY;
	}

	/* We can't close the device if there are outstanding sessions in use */
	if (dev->data->session_pool != NULL) {
		if (!rte_mempool_full(dev->data->session_pool)) {
			CDEV_LOG_ERR("dev_id=%u close failed, session mempool "
					"has sessions still in use, free "
					"all sessions before calling close",
					(unsigned)dev_id);
			return -EBUSY;
		}
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_close, -ENOTSUP);
	retval = (*dev->dev_ops->dev_close)(dev);

	if (retval < 0)
		return retval;

	return 0;
}

int
rte_cryptodev_queue_pair_setup(uint8_t dev_id, uint16_t queue_pair_id,
		const struct rte_cryptodev_qp_conf *qp_conf, int socket_id)
{
	struct rte_cryptodev *dev;

	if (!rte_cryptodev_pmd_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%" PRIu8, dev_id);
		return -EINVAL;
	}

	dev = &rte_crypto_devices[dev_id];
	if (queue_pair_id >= dev->data->nb_queue_pairs) {
		CDEV_LOG_ERR("Invalid queue_pair_id=%d", queue_pair_id);
		return -EINVAL;
	}

	if (dev->data->dev_started) {
		CDEV_LOG_ERR(
		    "device %d must be stopped to allow configuration", dev_id);
		return -EBUSY;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->queue_pair_setup, -ENOTSUP);

	return (*dev->dev_ops->queue_pair_setup)(dev, queue_pair_id, qp_conf,
			socket_id);
}


int
rte_cryptodev_stats_get(uint8_t dev_id, struct rte_cryptodev_stats *stats)
{
	struct rte_cryptodev *dev;

	if (!rte_cryptodev_pmd_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%d", dev_id);
		return -ENODEV;
	}

	if (stats == NULL) {
		CDEV_LOG_ERR("Invalid stats ptr");
		return -EINVAL;
	}

	dev = &rte_crypto_devices[dev_id];
	memset(stats, 0, sizeof(*stats));

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->stats_get, -ENOTSUP);
	(*dev->dev_ops->stats_get)(dev, stats);
	return 0;
}

void
rte_cryptodev_stats_reset(uint8_t dev_id)
{
	struct rte_cryptodev *dev;

	if (!rte_cryptodev_pmd_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%" PRIu8, dev_id);
		return;
	}

	dev = &rte_crypto_devices[dev_id];

	RTE_FUNC_PTR_OR_RET(*dev->dev_ops->stats_reset);
	(*dev->dev_ops->stats_reset)(dev);
}


void
rte_cryptodev_info_get(uint8_t dev_id, struct rte_cryptodev_info *dev_info)
{
	struct rte_cryptodev *dev;

	if (dev_id >= cryptodev_globals.nb_devs) {
		CDEV_LOG_ERR("Invalid dev_id=%d", dev_id);
		return;
	}

	dev = &rte_crypto_devices[dev_id];

	memset(dev_info, 0, sizeof(struct rte_cryptodev_info));

	RTE_FUNC_PTR_OR_RET(*dev->dev_ops->dev_infos_get);
	(*dev->dev_ops->dev_infos_get)(dev, dev_info);

	dev_info->pci_dev = dev->pci_dev;
	if (dev->driver)
		dev_info->driver_name = dev->driver->pci_drv.driver.name;
}


int
rte_cryptodev_callback_register(uint8_t dev_id,
			enum rte_cryptodev_event_type event,
			rte_cryptodev_cb_fn cb_fn, void *cb_arg)
{
	struct rte_cryptodev *dev;
	struct rte_cryptodev_callback *user_cb;

	if (!cb_fn)
		return -EINVAL;

	if (!rte_cryptodev_pmd_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%" PRIu8, dev_id);
		return -EINVAL;
	}

	dev = &rte_crypto_devices[dev_id];
	rte_spinlock_lock(&rte_cryptodev_cb_lock);

	TAILQ_FOREACH(user_cb, &(dev->link_intr_cbs), next) {
		if (user_cb->cb_fn == cb_fn &&
			user_cb->cb_arg == cb_arg &&
			user_cb->event == event) {
			break;
		}
	}

	/* create a new callback. */
	if (user_cb == NULL) {
		user_cb = rte_zmalloc("INTR_USER_CALLBACK",
				sizeof(struct rte_cryptodev_callback), 0);
		if (user_cb != NULL) {
			user_cb->cb_fn = cb_fn;
			user_cb->cb_arg = cb_arg;
			user_cb->event = event;
			TAILQ_INSERT_TAIL(&(dev->link_intr_cbs), user_cb, next);
		}
	}

	rte_spinlock_unlock(&rte_cryptodev_cb_lock);
	return (user_cb == NULL) ? -ENOMEM : 0;
}

int
rte_cryptodev_callback_unregister(uint8_t dev_id,
			enum rte_cryptodev_event_type event,
			rte_cryptodev_cb_fn cb_fn, void *cb_arg)
{
	int ret;
	struct rte_cryptodev *dev;
	struct rte_cryptodev_callback *cb, *next;

	if (!cb_fn)
		return -EINVAL;

	if (!rte_cryptodev_pmd_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%" PRIu8, dev_id);
		return -EINVAL;
	}

	dev = &rte_crypto_devices[dev_id];
	rte_spinlock_lock(&rte_cryptodev_cb_lock);

	ret = 0;
	for (cb = TAILQ_FIRST(&dev->link_intr_cbs); cb != NULL; cb = next) {

		next = TAILQ_NEXT(cb, next);

		if (cb->cb_fn != cb_fn || cb->event != event ||
				(cb->cb_arg != (void *)-1 &&
				cb->cb_arg != cb_arg))
			continue;

		/*
		 * if this callback is not executing right now,
		 * then remove it.
		 */
		if (cb->active == 0) {
			TAILQ_REMOVE(&(dev->link_intr_cbs), cb, next);
			rte_free(cb);
		} else {
			ret = -EAGAIN;
		}
	}

	rte_spinlock_unlock(&rte_cryptodev_cb_lock);
	return ret;
}

void
rte_cryptodev_pmd_callback_process(struct rte_cryptodev *dev,
	enum rte_cryptodev_event_type event)
{
	struct rte_cryptodev_callback *cb_lst;
	struct rte_cryptodev_callback dev_cb;

	rte_spinlock_lock(&rte_cryptodev_cb_lock);
	TAILQ_FOREACH(cb_lst, &(dev->link_intr_cbs), next) {
		if (cb_lst->cb_fn == NULL || cb_lst->event != event)
			continue;
		dev_cb = *cb_lst;
		cb_lst->active = 1;
		rte_spinlock_unlock(&rte_cryptodev_cb_lock);
		dev_cb.cb_fn(dev->data->dev_id, dev_cb.event,
						dev_cb.cb_arg);
		rte_spinlock_lock(&rte_cryptodev_cb_lock);
		cb_lst->active = 0;
	}
	rte_spinlock_unlock(&rte_cryptodev_cb_lock);
}


static void
rte_cryptodev_sym_session_init(struct rte_mempool *mp,
		void *opaque_arg,
		void *_sess,
		__rte_unused unsigned i)
{
	struct rte_cryptodev_sym_session *sess = _sess;
	struct rte_cryptodev *dev = opaque_arg;

	memset(sess, 0, mp->elt_size);

	sess->dev_id = dev->data->dev_id;
	sess->dev_type = dev->dev_type;
	sess->mp = mp;

	if (dev->dev_ops->session_initialize)
		(*dev->dev_ops->session_initialize)(mp, sess);
}

static int
rte_cryptodev_sym_session_pool_create(struct rte_cryptodev *dev,
		unsigned nb_objs, unsigned obj_cache_size, int socket_id)
{
	char mp_name[RTE_CRYPTODEV_NAME_MAX_LEN];
	unsigned priv_sess_size;

	unsigned n = snprintf(mp_name, sizeof(mp_name), "cdev_%d_sess_mp",
			dev->data->dev_id);
	if (n > sizeof(mp_name)) {
		CDEV_LOG_ERR("Unable to create unique name for session mempool");
		return -ENOMEM;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->session_get_size, -ENOTSUP);
	priv_sess_size = (*dev->dev_ops->session_get_size)(dev);
	if (priv_sess_size == 0) {
		CDEV_LOG_ERR("%s returned and invalid private session size ",
						dev->data->name);
		return -ENOMEM;
	}

	unsigned elt_size = sizeof(struct rte_cryptodev_sym_session) +
			priv_sess_size;

	dev->data->session_pool = rte_mempool_lookup(mp_name);
	if (dev->data->session_pool != NULL) {
		if ((dev->data->session_pool->elt_size != elt_size) ||
				(dev->data->session_pool->cache_size <
				obj_cache_size) ||
				(dev->data->session_pool->size < nb_objs)) {

			CDEV_LOG_ERR("%s mempool already exists with different"
					" initialization parameters", mp_name);
			dev->data->session_pool = NULL;
			return -ENOMEM;
		}
	} else {
		dev->data->session_pool = rte_mempool_create(
				mp_name, /* mempool name */
				nb_objs, /* number of elements*/
				elt_size, /* element size*/
				obj_cache_size, /* Cache size*/
				0, /* private data size */
				NULL, /* obj initialization constructor */
				NULL, /* obj initialization constructor arg */
				rte_cryptodev_sym_session_init,
				/**< obj constructor*/
				dev, /* obj constructor arg */
				socket_id, /* socket id */
				0); /* flags */

		if (dev->data->session_pool == NULL) {
			CDEV_LOG_ERR("%s mempool allocation failed", mp_name);
			return -ENOMEM;
		}
	}

	CDEV_LOG_DEBUG("%s mempool created!", mp_name);
	return 0;
}

struct rte_cryptodev_sym_session *
rte_cryptodev_sym_session_create(uint8_t dev_id,
		struct rte_crypto_sym_xform *xform)
{
	struct rte_cryptodev *dev;
	struct rte_cryptodev_sym_session *sess;
	void *_sess;

	if (!rte_cryptodev_pmd_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%d", dev_id);
		return NULL;
	}

	dev = &rte_crypto_devices[dev_id];

	/* Allocate a session structure from the session pool */
	if (rte_mempool_get(dev->data->session_pool, &_sess)) {
		CDEV_LOG_ERR("Couldn't get object from session mempool");
		return NULL;
	}

	sess = (struct rte_cryptodev_sym_session *)_sess;

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->session_configure, NULL);
	if (dev->dev_ops->session_configure(dev, xform, sess->_private) ==
			NULL) {
		CDEV_LOG_ERR("dev_id %d failed to configure session details",
				dev_id);

		/* Return session to mempool */
		rte_mempool_put(sess->mp, _sess);
		return NULL;
	}

	return sess;
}

struct rte_cryptodev_sym_session *
rte_cryptodev_sym_session_free(uint8_t dev_id,
		struct rte_cryptodev_sym_session *sess)
{
	struct rte_cryptodev *dev;

	if (!rte_cryptodev_pmd_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%d", dev_id);
		return sess;
	}

	dev = &rte_crypto_devices[dev_id];

	/* Check the session belongs to this device type */
	if (sess->dev_type != dev->dev_type)
		return sess;

	/* Let device implementation clear session material */
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->session_clear, sess);
	dev->dev_ops->session_clear(dev, (void *)sess->_private);

	/* Return session to mempool */
	rte_mempool_put(sess->mp, (void *)sess);

	return NULL;
}

/** Initialise rte_crypto_op mempool element */
static void
rte_crypto_op_init(struct rte_mempool *mempool,
		void *opaque_arg,
		void *_op_data,
		__rte_unused unsigned i)
{
	struct rte_crypto_op *op = _op_data;
	enum rte_crypto_op_type type = *(enum rte_crypto_op_type *)opaque_arg;

	memset(_op_data, 0, mempool->elt_size);

	__rte_crypto_op_reset(op, type);

	op->phys_addr = rte_mem_virt2phy(_op_data);
	op->mempool = mempool;
}


struct rte_mempool *
rte_crypto_op_pool_create(const char *name, enum rte_crypto_op_type type,
		unsigned nb_elts, unsigned cache_size, uint16_t priv_size,
		int socket_id)
{
	struct rte_crypto_op_pool_private *priv;

	unsigned elt_size = sizeof(struct rte_crypto_op) +
			sizeof(struct rte_crypto_sym_op) +
			priv_size;

	/* lookup mempool in case already allocated */
	struct rte_mempool *mp = rte_mempool_lookup(name);

	if (mp != NULL) {
		priv = (struct rte_crypto_op_pool_private *)
				rte_mempool_get_priv(mp);

		if (mp->elt_size != elt_size ||
				mp->cache_size < cache_size ||
				mp->size < nb_elts ||
				priv->priv_size <  priv_size) {
			mp = NULL;
			CDEV_LOG_ERR("Mempool %s already exists but with "
					"incompatible parameters", name);
			return NULL;
		}
		return mp;
	}

	mp = rte_mempool_create(
			name,
			nb_elts,
			elt_size,
			cache_size,
			sizeof(struct rte_crypto_op_pool_private),
			NULL,
			NULL,
			rte_crypto_op_init,
			&type,
			socket_id,
			0);

	if (mp == NULL) {
		CDEV_LOG_ERR("Failed to create mempool %s", name);
		return NULL;
	}

	priv = (struct rte_crypto_op_pool_private *)
			rte_mempool_get_priv(mp);

	priv->priv_size = priv_size;
	priv->type = type;

	return mp;
}
