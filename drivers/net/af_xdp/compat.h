/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation.
 */

#include <bpf/xsk.h>
#include <linux/version.h>
#include <poll.h>

#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE && \
	defined(RTE_LIBRTE_AF_XDP_PMD_SHARED_UMEM)
#define ETH_AF_XDP_SHARED_UMEM 1
#endif

#ifdef ETH_AF_XDP_SHARED_UMEM
static __rte_always_inline int
create_shared_socket(struct xsk_socket **xsk_ptr,
			  const char *ifname,
			  __u32 queue_id, struct xsk_umem *umem,
			  struct xsk_ring_cons *rx,
			  struct xsk_ring_prod *tx,
			  struct xsk_ring_prod *fill,
			  struct xsk_ring_cons *comp,
			  const struct xsk_socket_config *config)
{
	return xsk_socket__create_shared(xsk_ptr, ifname, queue_id, umem, rx,
						tx, fill, comp, config);
}
#else
static __rte_always_inline int
create_shared_socket(struct xsk_socket **xsk_ptr __rte_unused,
			  const char *ifname __rte_unused,
			  __u32 queue_id __rte_unused,
			  struct xsk_umem *umem __rte_unused,
			  struct xsk_ring_cons *rx __rte_unused,
			  struct xsk_ring_prod *tx __rte_unused,
			  struct xsk_ring_prod *fill __rte_unused,
			  struct xsk_ring_cons *comp __rte_unused,
			  const struct xsk_socket_config *config __rte_unused)
{
	return -1;
}
#endif

#ifdef XDP_USE_NEED_WAKEUP
static void
rx_syscall_handler(struct xsk_ring_prod *q, uint32_t busy_budget,
		   struct pollfd *fds, struct xsk_socket *xsk)
{
	/* we can assume a kernel >= 5.11 is in use if busy polling is enabled
	 * and thus we can safely use the recvfrom() syscall which is only
	 * supported for AF_XDP sockets in kernels >= 5.11.
	 */
	if (busy_budget) {
		(void)recvfrom(xsk_socket__fd(xsk), NULL, 0,
			MSG_DONTWAIT, NULL, NULL);
		return;
	}

	if (xsk_ring_prod__needs_wakeup(q))
		(void)poll(fds, 1, 1000);
}
static int
tx_syscall_needed(struct xsk_ring_prod *q)
{
	return xsk_ring_prod__needs_wakeup(q);
}
#else
static void
rx_syscall_handler(struct xsk_ring_prod *q __rte_unused, uint32_t busy_budget,
		   struct pollfd *fds __rte_unused, struct xsk_socket *xsk)
{
	if (busy_budget)
		(void)recvfrom(xsk_socket__fd(xsk), NULL, 0,
			MSG_DONTWAIT, NULL, NULL);
}
static int
tx_syscall_needed(struct xsk_ring_prod *q __rte_unused)
{
	return 1;
}
#endif
