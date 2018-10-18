/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 Mellanox Technologies, Ltd
 */

/**
 * @file
 * Interrupts handling for tap driver.
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <rte_eth_tap.h>
#include <rte_errno.h>
#include <rte_interrupts.h>


/**
 * Unregister Rx interrupts free the queue interrupt vector.
 *
 * @param dev
 *   Pointer to the tap rte_eth_dev structure.
 */
static void
tap_rx_intr_vec_uninstall(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct rte_intr_handle *intr_handle = &pmd->intr_handle;

	rte_intr_free_epoll_fd(intr_handle);
	free(intr_handle->intr_vec);
	intr_handle->intr_vec = NULL;
	intr_handle->nb_efd = 0;
}

/**
 * Allocate Rx queue interrupt vector and register Rx interrupts.
 *
 * @param dev
 *   Pointer to the tap rte_eth_dev device structure.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
tap_rx_intr_vec_install(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct pmd_process_private *process_private = dev->process_private;
	unsigned int rxqs_n = pmd->dev->data->nb_rx_queues;
	struct rte_intr_handle *intr_handle = &pmd->intr_handle;
	unsigned int n = RTE_MIN(rxqs_n, (uint32_t)RTE_MAX_RXTX_INTR_VEC_ID);
	unsigned int i;
	unsigned int count = 0;

	if (!dev->data->dev_conf.intr_conf.rxq)
		return 0;
	intr_handle->intr_vec = malloc(sizeof(intr_handle->intr_vec[rxqs_n]));
	if (intr_handle->intr_vec == NULL) {
		rte_errno = ENOMEM;
		TAP_LOG(ERR,
			"failed to allocate memory for interrupt vector,"
			" Rx interrupts will not be supported");
		return -rte_errno;
	}
	for (i = 0; i < n; i++) {
		struct rx_queue *rxq = pmd->dev->data->rx_queues[i];

		/* Skip queues that cannot request interrupts. */
		if (!rxq || process_private->rxq_fds[i] <= 0) {
			/* Use invalid intr_vec[] index to disable entry. */
			intr_handle->intr_vec[i] =
				RTE_INTR_VEC_RXTX_OFFSET +
				RTE_MAX_RXTX_INTR_VEC_ID;
			continue;
		}
		intr_handle->intr_vec[i] = RTE_INTR_VEC_RXTX_OFFSET + count;
		intr_handle->efds[count] = process_private->rxq_fds[i];
		count++;
	}
	if (!count)
		tap_rx_intr_vec_uninstall(dev);
	else
		intr_handle->nb_efd = count;
	return 0;
}

/**
 * Register or unregister the Rx interrupts.
 *
 * @param dev
 *   Pointer to the tap rte_eth_dev device structure.
 * @param set
 *   should the operation be register or unregister the interrupts.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
int
tap_rx_intr_vec_set(struct rte_eth_dev *dev, int set)
{
	tap_rx_intr_vec_uninstall(dev);
	if (set)
		return tap_rx_intr_vec_install(dev);
	return 0;
}
