/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell International Ltd.
 */

#ifndef __RTE_EPOLL_H__
#define __RTE_EPOLL_H__

/**
 * @file
 * The rte_epoll provides interfaces functions to add delete events,
 * wait poll for an event.
 */

#include <stdint.h>

#include <rte_stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTE_INTR_EVENT_ADD            1UL
#define RTE_INTR_EVENT_DEL            2UL

typedef void (*rte_intr_event_cb_t)(int fd, void *arg);

struct rte_epoll_data {
	uint32_t event;               /**< event type */
	void *data;                   /**< User data */
	rte_intr_event_cb_t cb_fun;   /**< IN: callback fun */
	void *cb_arg;	              /**< IN: callback arg */
};

enum {
	RTE_EPOLL_INVALID = 0,
	RTE_EPOLL_VALID,
	RTE_EPOLL_EXEC,
};

/** interrupt epoll event obj, taken by epoll_event.ptr */
struct rte_epoll_event {
	RTE_ATOMIC(uint32_t) status;           /**< OUT: event status */
	int fd;                    /**< OUT: event fd */
	int epfd;       /**< OUT: epoll instance the ev associated with */
	struct rte_epoll_data epdata;
};

#define RTE_EPOLL_PER_THREAD        -1  /**< to hint using per thread epfd */

/**
 * It waits for events on the epoll instance.
 * Retries if signal received.
 *
 * @param epfd
 *   Epoll instance fd on which the caller wait for events.
 * @param events
 *   Memory area contains the events that will be available for the caller.
 * @param maxevents
 *   Up to maxevents are returned, must greater than zero.
 * @param timeout
 *   Specifying a timeout of -1 causes a block indefinitely.
 *   Specifying a timeout equal to zero cause to return immediately.
 * @return
 *   - On success, returns the number of available event.
 *   - On failure, a negative value.
 */
int
rte_epoll_wait(int epfd, struct rte_epoll_event *events,
	       int maxevents, int timeout);

/**
 * It waits for events on the epoll instance.
 * Does not retry if signal received.
 *
 * @param epfd
 *   Epoll instance fd on which the caller wait for events.
 * @param events
 *   Memory area contains the events that will be available for the caller.
 * @param maxevents
 *   Up to maxevents are returned, must greater than zero.
 * @param timeout
 *   Specifying a timeout of -1 causes a block indefinitely.
 *   Specifying a timeout equal to zero cause to return immediately.
 * @return
 *   - On success, returns the number of available event.
 *   - On failure, a negative value.
 */
int
rte_epoll_wait_interruptible(int epfd, struct rte_epoll_event *events,
	       int maxevents, int timeout);

/**
 * It performs control operations on epoll instance referred by the epfd.
 * It requests that the operation op be performed for the target fd.
 *
 * @param epfd
 *   Epoll instance fd on which the caller perform control operations.
 * @param op
 *   The operation be performed for the target fd.
 * @param fd
 *   The target fd on which the control ops perform.
 * @param event
 *   Describes the object linked to the fd.
 *   Note: The caller must take care the object deletion after CTL_DEL.
 * @return
 *   - On success, zero.
 *   - On failure, a negative value.
 */
int
rte_epoll_ctl(int epfd, int op, int fd,
	      struct rte_epoll_event *event);

#ifdef __cplusplus
}
#endif

#endif /* __RTE_EPOLL_H__ */
