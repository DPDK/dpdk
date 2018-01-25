/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 Mellanox Technologies, Ltd.
 */

/**
 * @file
 * Interrupts handling for failsafe driver.
 */

#include <unistd.h>

#include "failsafe_private.h"

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
	fs_rx_intr_vec_uninstall(PRIV(dev));
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

	if (intr_conf->rxq == 0)
		return 0;
	if (fs_rx_intr_vec_install(priv) < 0)
		return -rte_errno;
	dev->intr_handle = &priv->intr_handle;
	return 0;
}
