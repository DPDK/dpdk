/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_COMPAT_NETMAP_H_

#include <poll.h>
#include <linux/ioctl.h>
#include <net/if.h>

#include <rte_ethdev.h>
#include <rte_mempool.h>

#include "netmap.h"
#include "netmap_user.h"

/**
 * One can overwrite Netmap macros here as needed
 */

struct rte_netmap_conf {
	int32_t  socket_id;
	uint32_t max_rings; /* number of rings(queues) per netmap_if(port) */
	uint32_t max_slots; /* number of slots(descriptors) per netmap ring. */
	uint16_t max_bufsz; /* size of each netmap buffer. */
};

struct rte_netmap_port_conf {
	struct rte_eth_conf   *eth_conf;
	struct rte_mempool    *pool;
	int32_t socket_id;
	uint16_t nr_tx_rings;
	uint16_t nr_rx_rings;
	uint32_t nr_tx_slots;
	uint32_t nr_rx_slots;
	uint16_t tx_burst;
	uint16_t rx_burst;
};

int rte_netmap_init(const struct rte_netmap_conf *conf);
int rte_netmap_init_port(uint16_t portid,
	const struct rte_netmap_port_conf *conf);

int rte_netmap_close(int fd);
int rte_netmap_ioctl(int fd, uint32_t op, void *param);
int rte_netmap_open(const char *pathname, int flags);
int rte_netmap_poll(struct pollfd *fds, nfds_t nfds, int timeout);
void *rte_netmap_mmap(void *addr, size_t length, int prot, int flags, int fd,
	                  off_t offset);

#endif /* _RTE_COMPAT_NETMAP_H_ */
