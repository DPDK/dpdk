/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 6WIND S.A.
 * Copyright 2018 Mellanox Technologies, Ltd
 */

#include <errno.h>
#include <libmnl/libmnl.h>
#include <linux/netlink.h>
#include <linux/pkt_sched.h>
#include <linux/rtnetlink.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>

#include <rte_errno.h>
#include <rte_flow.h>

#include "mlx5.h"

/* Normally found in linux/netlink.h. */
#ifndef NETLINK_CAP_ACK
#define NETLINK_CAP_ACK 10
#endif

/**
 * Send Netlink message with acknowledgment.
 *
 * @param nl
 *   Libmnl socket to use.
 * @param nlh
 *   Message to send. This function always raises the NLM_F_ACK flag before
 *   sending.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_nl_flow_nl_ack(struct mnl_socket *nl, struct nlmsghdr *nlh)
{
	alignas(struct nlmsghdr)
	uint8_t ans[mnl_nlmsg_size(sizeof(struct nlmsgerr)) +
		    nlh->nlmsg_len - sizeof(*nlh)];
	uint32_t seq = random();
	int ret;

	nlh->nlmsg_flags |= NLM_F_ACK;
	nlh->nlmsg_seq = seq;
	ret = mnl_socket_sendto(nl, nlh, nlh->nlmsg_len);
	if (ret != -1)
		ret = mnl_socket_recvfrom(nl, ans, sizeof(ans));
	if (ret != -1)
		ret = mnl_cb_run
			(ans, ret, seq, mnl_socket_get_portid(nl), NULL, NULL);
	if (!ret)
		return 0;
	rte_errno = errno;
	return -rte_errno;
}

/**
 * Initialize ingress qdisc of a given network interface.
 *
 * @param nl
 *   Libmnl socket of the @p NETLINK_ROUTE kind.
 * @param ifindex
 *   Index of network interface to initialize.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_nl_flow_init(struct mnl_socket *nl, unsigned int ifindex,
		  struct rte_flow_error *error)
{
	struct nlmsghdr *nlh;
	struct tcmsg *tcm;
	alignas(struct nlmsghdr)
	uint8_t buf[mnl_nlmsg_size(sizeof(*tcm) + 128)];

	/* Destroy existing ingress qdisc and everything attached to it. */
	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = RTM_DELQDISC;
	nlh->nlmsg_flags = NLM_F_REQUEST;
	tcm = mnl_nlmsg_put_extra_header(nlh, sizeof(*tcm));
	tcm->tcm_family = AF_UNSPEC;
	tcm->tcm_ifindex = ifindex;
	tcm->tcm_handle = TC_H_MAKE(TC_H_INGRESS, 0);
	tcm->tcm_parent = TC_H_INGRESS;
	/* Ignore errors when qdisc is already absent. */
	if (mlx5_nl_flow_nl_ack(nl, nlh) &&
	    rte_errno != EINVAL && rte_errno != ENOENT)
		return rte_flow_error_set
			(error, rte_errno, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			 NULL, "netlink: failed to remove ingress qdisc");
	/* Create fresh ingress qdisc. */
	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = RTM_NEWQDISC;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
	tcm = mnl_nlmsg_put_extra_header(nlh, sizeof(*tcm));
	tcm->tcm_family = AF_UNSPEC;
	tcm->tcm_ifindex = ifindex;
	tcm->tcm_handle = TC_H_MAKE(TC_H_INGRESS, 0);
	tcm->tcm_parent = TC_H_INGRESS;
	mnl_attr_put_strz_check(nlh, sizeof(buf), TCA_KIND, "ingress");
	if (mlx5_nl_flow_nl_ack(nl, nlh))
		return rte_flow_error_set
			(error, rte_errno, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			 NULL, "netlink: failed to create ingress qdisc");
	return 0;
}

/**
 * Create and configure a libmnl socket for Netlink flow rules.
 *
 * @return
 *   A valid libmnl socket object pointer on success, NULL otherwise and
 *   rte_errno is set.
 */
struct mnl_socket *
mlx5_nl_flow_socket_create(void)
{
	struct mnl_socket *nl = mnl_socket_open(NETLINK_ROUTE);

	if (nl) {
		mnl_socket_setsockopt(nl, NETLINK_CAP_ACK, &(int){ 1 },
				      sizeof(int));
		if (!mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID))
			return nl;
	}
	rte_errno = errno;
	if (nl)
		mnl_socket_close(nl);
	return NULL;
}

/**
 * Destroy a libmnl socket.
 */
void
mlx5_nl_flow_socket_destroy(struct mnl_socket *nl)
{
	mnl_socket_close(nl);
}
