/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 Mellanox Technologies, Ltd
 */

/**
 * @file
 * Interrupts handling for failsafe driver.
 */

#if defined(LINUX)
#include <sys/epoll.h>
#endif
#include <unistd.h>

#include <rte_alarm.h>
#include <rte_config.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_interrupts.h>
#include <rte_io.h>
#include <rte_service_component.h>

#include "failsafe_private.h"

#define NUM_RX_PROXIES (FAILSAFE_MAX_ETHPORTS * RTE_MAX_RXTX_INTR_VEC_ID)


/**
 * Open an epoll file descriptor.
 *
 * @param flags
 *   Flags for defining epoll behavior.
 * @return
 *   0 on success, negative errno value otherwise.
 */
static int
fs_epoll_create1(int flags)
{
#if defined(LINUX)
	return epoll_create1(flags);
#elif defined(BSD)
	RTE_SET_USED(flags);
	return -ENOTSUP;
#endif
}

/**
 * Install failsafe Rx event proxy service.
 * The Rx event proxy is the service that listens to Rx events from the
 * subdevices and triggers failsafe Rx events accordingly.
 *
 * @param priv
 *   Pointer to failsafe private structure.
 * @return
 *   0 on success, negative errno value otherwise.
 */
static int
fs_rx_event_proxy_routine(void *data)
{
	struct fs_priv *priv;
	struct rxq *rxq;
	struct rte_epoll_event *events;
	uint64_t u64;
	int i, n;
	int rc = 0;

	u64 = 1;
	priv = data;
	events = priv->rxp.evec;
	n = rte_epoll_wait(priv->rxp.efd, events, NUM_RX_PROXIES, -1);
	for (i = 0; i < n; i++) {
		rxq = events[i].epdata.data;
		if (rxq->enable_events && rxq->event_fd != -1) {
			if (write(rxq->event_fd, &u64, sizeof(u64)) !=
			    sizeof(u64)) {
				ERROR("Failed to proxy Rx event to socket %d",
				       rxq->event_fd);
				rc = -EIO;
			}
		}
	}
	return rc;
}

/**
 * Uninstall failsafe Rx event proxy service.
 *
 * @param priv
 *   Pointer to failsafe private structure.
 */
static void
fs_rx_event_proxy_service_uninstall(struct fs_priv *priv)
{
	/* Unregister the event service. */
	switch (priv->rxp.sstate) {
	case SS_RUNNING:
		rte_service_map_lcore_set(priv->rxp.sid, priv->rxp.scid, 0);
		/* fall through */
	case SS_READY:
		rte_service_runstate_set(priv->rxp.sid, 0);
		rte_service_set_stats_enable(priv->rxp.sid, 0);
		rte_service_component_runstate_set(priv->rxp.sid, 0);
		/* fall through */
	case SS_REGISTERED:
		rte_service_component_unregister(priv->rxp.sid);
		/* fall through */
	default:
		break;
	}
}

/**
 * Install the failsafe Rx event proxy service.
 *
 * @param priv
 *   Pointer to failsafe private structure.
 * @return
 *   0 on success, negative errno value otherwise.
 */
static int
fs_rx_event_proxy_service_install(struct fs_priv *priv)
{
	struct rte_service_spec service;
	int32_t num_service_cores;
	int ret = 0;

	num_service_cores = rte_service_lcore_count();
	if (num_service_cores <= 0) {
		ERROR("Failed to install Rx interrupts, "
		      "no service core found");
		return -ENOTSUP;
	}
	/* prepare service info */
	memset(&service, 0, sizeof(struct rte_service_spec));
	snprintf(service.name, sizeof(service.name), "%s_Rx_service",
		 priv->dev->data->name);
	service.socket_id = priv->dev->data->numa_node;
	service.callback = fs_rx_event_proxy_routine;
	service.callback_userdata = priv;

	if (priv->rxp.sstate == SS_NO_SERVICE) {
		uint32_t service_core_list[num_service_cores];

		/* get a service core to work with */
		ret = rte_service_lcore_list(service_core_list,
					     num_service_cores);
		if (ret <= 0) {
			ERROR("Failed to install Rx interrupts, "
			      "service core list empty or corrupted");
			return -ENOTSUP;
		}
		priv->rxp.scid = service_core_list[0];
		ret = rte_service_lcore_add(priv->rxp.scid);
		if (ret && ret != -EALREADY) {
			ERROR("Failed adding service core");
			return ret;
		}
		/* service core may be in "stopped" state, start it */
		ret = rte_service_lcore_start(priv->rxp.scid);
		if (ret && (ret != -EALREADY)) {
			ERROR("Failed to install Rx interrupts, "
			      "service core not started");
			return ret;
		}
		/* register our service */
		int32_t ret = rte_service_component_register(&service,
							     &priv->rxp.sid);
		if (ret) {
			ERROR("service register() failed");
			return -ENOEXEC;
		}
		priv->rxp.sstate = SS_REGISTERED;
		/* run the service */
		ret = rte_service_component_runstate_set(priv->rxp.sid, 1);
		if (ret < 0) {
			ERROR("Failed Setting component runstate\n");
			return ret;
		}
		ret = rte_service_set_stats_enable(priv->rxp.sid, 1);
		if (ret < 0) {
			ERROR("Failed enabling stats\n");
			return ret;
		}
		ret = rte_service_runstate_set(priv->rxp.sid, 1);
		if (ret < 0) {
			ERROR("Failed to run service\n");
			return ret;
		}
		priv->rxp.sstate = SS_READY;
		/* map the service with the service core */
		ret = rte_service_map_lcore_set(priv->rxp.sid,
						priv->rxp.scid, 1);
		if (ret) {
			ERROR("Failed to install Rx interrupts, "
			      "could not map service core");
			return ret;
		}
		priv->rxp.sstate = SS_RUNNING;
	}
	return 0;
}

/**
 * Install failsafe Rx event proxy subsystem.
 * This is the way the failsafe PMD generates Rx events on behalf of its
 * subdevices.
 *
 * @param priv
 *   Pointer to failsafe private structure.
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
fs_rx_event_proxy_install(struct fs_priv *priv)
{
	int rc = 0;

	/*
	 * Create the epoll fd and event vector for the proxy service to
	 * wait on for Rx events generated by the subdevices.
	 */
	priv->rxp.efd = fs_epoll_create1(0);
	if (priv->rxp.efd < 0) {
		rte_errno = errno;
		ERROR("Failed to create epoll,"
		      " Rx interrupts will not be supported");
		return -rte_errno;
	}
	priv->rxp.evec = calloc(NUM_RX_PROXIES, sizeof(*priv->rxp.evec));
	if (priv->rxp.evec == NULL) {
		ERROR("Failed to allocate memory for event vectors,"
		      " Rx interrupts will not be supported");
		rc = -ENOMEM;
		goto error;
	}
	rc = fs_rx_event_proxy_service_install(priv);
	if (rc < 0)
		goto error;
	return 0;
error:
	if (priv->rxp.efd >= 0) {
		close(priv->rxp.efd);
		priv->rxp.efd = -1;
	}
	if (priv->rxp.evec != NULL) {
		free(priv->rxp.evec);
		priv->rxp.evec = NULL;
	}
	rte_errno = -rc;
	return rc;
}

/**
 * RX Interrupt control per subdevice.
 *
 * @param sdev
 *   Pointer to sub-device structure.
 * @param op
 *   The operation be performed for the vector.
 *   Operation type of {RTE_INTR_EVENT_ADD, RTE_INTR_EVENT_DEL}.
 * @return
 *   - On success, zero.
 *   - On failure, a negative value.
 */
static int
failsafe_eth_rx_intr_ctl_subdevice(struct sub_device *sdev, int op)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev *fsdev;
	int epfd;
	uint16_t pid;
	uint16_t qid;
	struct rxq *fsrxq;
	int rc;
	int ret = 0;

	if (sdev == NULL || (ETH(sdev) == NULL) ||
	    sdev->fs_dev == NULL || (PRIV(sdev->fs_dev) == NULL)) {
		ERROR("Called with invalid arguments");
		return -EINVAL;
	}
	dev = ETH(sdev);
	fsdev = sdev->fs_dev;
	epfd = PRIV(sdev->fs_dev)->rxp.efd;
	pid = PORT_ID(sdev);

	if (epfd <= 0) {
		if (op == RTE_INTR_EVENT_ADD) {
			ERROR("Proxy events are not initialized");
			return -EBADF;
		} else {
			return 0;
		}
	}
	if (dev->data->nb_rx_queues > fsdev->data->nb_rx_queues) {
		ERROR("subdevice has too many queues,"
		      " Interrupts will not be enabled");
			return -E2BIG;
	}
	for (qid = 0; qid < dev->data->nb_rx_queues; qid++) {
		fsrxq = fsdev->data->rx_queues[qid];
		rc = rte_eth_dev_rx_intr_ctl_q(pid, qid, epfd,
					       op, (void *)fsrxq);
		if (rc) {
			ERROR("rte_eth_dev_rx_intr_ctl_q failed for "
			      "port %d  queue %d, epfd %d, error %d",
			      pid, qid, epfd, rc);
			ret = rc;
		}
	}
	return ret;
}

/**
 * Install Rx interrupts subsystem for a subdevice.
 * This is a support for dynamically adding subdevices.
 *
 * @param sdev
 *   Pointer to subdevice structure.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
int failsafe_rx_intr_install_subdevice(struct sub_device *sdev)
{
	int rc;
	int qid;
	struct rte_eth_dev *fsdev;
	struct rxq **rxq;
	const struct rte_intr_conf *const intr_conf =
				&ETH(sdev)->data->dev_conf.intr_conf;

	fsdev = sdev->fs_dev;
	rxq = (struct rxq **)fsdev->data->rx_queues;
	if (intr_conf->rxq == 0)
		return 0;
	rc = failsafe_eth_rx_intr_ctl_subdevice(sdev, RTE_INTR_EVENT_ADD);
	if (rc)
		return rc;
	/* enable interrupts on already-enabled queues */
	for (qid = 0; qid < ETH(sdev)->data->nb_rx_queues; qid++) {
		if (rxq[qid]->enable_events) {
			int ret = rte_eth_dev_rx_intr_enable(PORT_ID(sdev),
							     qid);
			if (ret && (ret != -ENOTSUP)) {
				ERROR("Failed to enable interrupts on "
				      "port %d queue %d", PORT_ID(sdev), qid);
				rc = ret;
			}
		}
	}
	return rc;
}

/**
 * Uninstall Rx interrupts subsystem for a subdevice.
 * This is a support for dynamically removing subdevices.
 *
 * @param sdev
 *   Pointer to subdevice structure.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
void failsafe_rx_intr_uninstall_subdevice(struct sub_device *sdev)
{
	int qid;
	struct rte_eth_dev *fsdev;
	struct rxq *fsrxq;

	fsdev = sdev->fs_dev;
	for (qid = 0; qid < ETH(sdev)->data->nb_rx_queues; qid++) {
		if (qid < fsdev->data->nb_rx_queues) {
			fsrxq = fsdev->data->rx_queues[qid];
			if (fsrxq != NULL && fsrxq->enable_events)
				rte_eth_dev_rx_intr_disable(PORT_ID(sdev),
							    qid);
		}
	}
	failsafe_eth_rx_intr_ctl_subdevice(sdev, RTE_INTR_EVENT_DEL);
}

/**
 * Uninstall failsafe Rx event proxy.
 *
 * @param priv
 *   Pointer to failsafe private structure.
 */
static void
fs_rx_event_proxy_uninstall(struct fs_priv *priv)
{
	fs_rx_event_proxy_service_uninstall(priv);
	if (priv->rxp.evec != NULL) {
		free(priv->rxp.evec);
		priv->rxp.evec = NULL;
	}
	if (priv->rxp.efd > 0) {
		close(priv->rxp.efd);
		priv->rxp.efd = -1;
	}
}

/**
 * Uninstall failsafe interrupt vector.
 *
 * @param priv
 *   Pointer to failsafe private structure.
 */
static void
fs_rx_intr_vec_uninstall(struct fs_priv *priv)
{
	struct rte_intr_handle *intr_handle;

	intr_handle = &priv->intr_handle;
	if (intr_handle->intr_vec != NULL) {
		free(intr_handle->intr_vec);
		intr_handle->intr_vec = NULL;
	}
	intr_handle->nb_efd = 0;
}

/**
 * Installs failsafe interrupt vector to be registered with EAL later on.
 *
 * @param priv
 *   Pointer to failsafe private structure.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
fs_rx_intr_vec_install(struct fs_priv *priv)
{
	unsigned int i;
	unsigned int rxqs_n;
	unsigned int n;
	unsigned int count;
	struct rte_intr_handle *intr_handle;

	rxqs_n = priv->dev->data->nb_rx_queues;
	n = RTE_MIN(rxqs_n, (uint32_t)RTE_MAX_RXTX_INTR_VEC_ID);
	count = 0;
	intr_handle = &priv->intr_handle;
	RTE_ASSERT(intr_handle->intr_vec == NULL);
	/* Allocate the interrupt vector of the failsafe Rx proxy interrupts */
	intr_handle->intr_vec = malloc(n * sizeof(intr_handle->intr_vec[0]));
	if (intr_handle->intr_vec == NULL) {
		fs_rx_intr_vec_uninstall(priv);
		rte_errno = ENOMEM;
		ERROR("Failed to allocate memory for interrupt vector,"
		      " Rx interrupts will not be supported");
		return -rte_errno;
	}
	for (i = 0; i < n; i++) {
		struct rxq *rxq = priv->dev->data->rx_queues[i];

		/* Skip queues that cannot request interrupts. */
		if (rxq == NULL || rxq->event_fd < 0) {
			/* Use invalid intr_vec[] index to disable entry. */
			intr_handle->intr_vec[i] =
				RTE_INTR_VEC_RXTX_OFFSET +
				RTE_MAX_RXTX_INTR_VEC_ID;
			continue;
		}
		if (count >= RTE_MAX_RXTX_INTR_VEC_ID) {
			rte_errno = E2BIG;
			ERROR("Too many Rx queues for interrupt vector size"
			      " (%d), Rx interrupts cannot be enabled",
			      RTE_MAX_RXTX_INTR_VEC_ID);
			fs_rx_intr_vec_uninstall(priv);
			return -rte_errno;
		}
		intr_handle->intr_vec[i] = RTE_INTR_VEC_RXTX_OFFSET + count;
		intr_handle->efds[count] = rxq->event_fd;
		count++;
	}
	if (count == 0) {
		fs_rx_intr_vec_uninstall(priv);
	} else {
		intr_handle->nb_efd = count;
		intr_handle->efd_counter_size = sizeof(uint64_t);
	}
	return 0;
}


/**
 * Uninstall failsafe Rx interrupts subsystem.
 *
 * @param priv
 *   Pointer to private structure.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
void
failsafe_rx_intr_uninstall(struct rte_eth_dev *dev)
{
	struct fs_priv *priv;
	struct rte_intr_handle *intr_handle;

	priv = PRIV(dev);
	intr_handle = &priv->intr_handle;
	rte_intr_free_epoll_fd(intr_handle);
	fs_rx_event_proxy_uninstall(priv);
	fs_rx_intr_vec_uninstall(priv);
	dev->intr_handle = NULL;
}

/**
 * Install failsafe Rx interrupts subsystem.
 *
 * @param priv
 *   Pointer to private structure.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
int
failsafe_rx_intr_install(struct rte_eth_dev *dev)
{
	struct fs_priv *priv = PRIV(dev);
	const struct rte_intr_conf *const intr_conf =
			&priv->dev->data->dev_conf.intr_conf;

	if (intr_conf->rxq == 0 || dev->intr_handle != NULL)
		return 0;
	if (fs_rx_intr_vec_install(priv) < 0)
		return -rte_errno;
	if (fs_rx_event_proxy_install(priv) < 0) {
		fs_rx_intr_vec_uninstall(priv);
		return -rte_errno;
	}
	dev->intr_handle = &priv->intr_handle;
	return 0;
}
