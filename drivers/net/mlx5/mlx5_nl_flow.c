/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 6WIND S.A.
 * Copyright 2018 Mellanox Technologies, Ltd
 */

#include <errno.h>
#include <libmnl/libmnl.h>
#include <linux/if_ether.h>
#include <linux/netlink.h>
#include <linux/pkt_cls.h>
#include <linux/pkt_sched.h>
#include <linux/rtnetlink.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>

#include <rte_byteorder.h>
#include <rte_errno.h>
#include <rte_flow.h>

#include "mlx5.h"

/* Normally found in linux/netlink.h. */
#ifndef NETLINK_CAP_ACK
#define NETLINK_CAP_ACK 10
#endif

/* Normally found in linux/pkt_sched.h. */
#ifndef TC_H_MIN_INGRESS
#define TC_H_MIN_INGRESS 0xfff2u
#endif

/* Normally found in linux/pkt_cls.h. */
#ifndef TCA_CLS_FLAGS_SKIP_SW
#define TCA_CLS_FLAGS_SKIP_SW (1 << 1)
#endif
#ifndef HAVE_TCA_FLOWER_ACT
#define TCA_FLOWER_ACT 3
#endif
#ifndef HAVE_TCA_FLOWER_FLAGS
#define TCA_FLOWER_FLAGS 22
#endif

/** Parser state definitions for mlx5_nl_flow_trans[]. */
enum mlx5_nl_flow_trans {
	INVALID,
	BACK,
	ATTR,
	PATTERN,
	ITEM_VOID,
	ACTIONS,
	ACTION_VOID,
	END,
};

#define TRANS(...) (const enum mlx5_nl_flow_trans []){ __VA_ARGS__, INVALID, }

#define PATTERN_COMMON \
	ITEM_VOID, ACTIONS
#define ACTIONS_COMMON \
	ACTION_VOID, END

/** Parser state transitions used by mlx5_nl_flow_transpose(). */
static const enum mlx5_nl_flow_trans *const mlx5_nl_flow_trans[] = {
	[INVALID] = NULL,
	[BACK] = NULL,
	[ATTR] = TRANS(PATTERN),
	[PATTERN] = TRANS(PATTERN_COMMON),
	[ITEM_VOID] = TRANS(BACK),
	[ACTIONS] = TRANS(ACTIONS_COMMON),
	[ACTION_VOID] = TRANS(BACK),
	[END] = NULL,
};

/**
 * Transpose flow rule description to rtnetlink message.
 *
 * This function transposes a flow rule description to a traffic control
 * (TC) filter creation message ready to be sent over Netlink.
 *
 * Target interface is specified as the first entry of the @p ptoi table.
 * Subsequent entries enable this function to resolve other DPDK port IDs
 * found in the flow rule.
 *
 * @param[out] buf
 *   Output message buffer. May be NULL when @p size is 0.
 * @param size
 *   Size of @p buf. Message may be truncated if not large enough.
 * @param[in] ptoi
 *   DPDK port ID to network interface index translation table. This table
 *   is terminated by an entry with a zero ifindex value.
 * @param[in] attr
 *   Flow rule attributes.
 * @param[in] pattern
 *   Pattern specification.
 * @param[in] actions
 *   Associated actions.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   A positive value representing the exact size of the message in bytes
 *   regardless of the @p size parameter on success, a negative errno value
 *   otherwise and rte_errno is set.
 */
int
mlx5_nl_flow_transpose(void *buf,
		       size_t size,
		       const struct mlx5_nl_flow_ptoi *ptoi,
		       const struct rte_flow_attr *attr,
		       const struct rte_flow_item *pattern,
		       const struct rte_flow_action *actions,
		       struct rte_flow_error *error)
{
	alignas(struct nlmsghdr)
	uint8_t buf_tmp[mnl_nlmsg_size(sizeof(struct tcmsg) + 1024)];
	const struct rte_flow_item *item;
	const struct rte_flow_action *action;
	unsigned int n;
	struct nlattr *na_flower;
	struct nlattr *na_flower_act;
	const enum mlx5_nl_flow_trans *trans;
	const enum mlx5_nl_flow_trans *back;

	if (!size)
		goto error_nobufs;
init:
	item = pattern;
	action = actions;
	n = 0;
	na_flower = NULL;
	na_flower_act = NULL;
	trans = TRANS(ATTR);
	back = trans;
trans:
	switch (trans[n++]) {
		struct nlmsghdr *nlh;
		struct tcmsg *tcm;

	case INVALID:
		if (item->type)
			return rte_flow_error_set
				(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM,
				 item, "unsupported pattern item combination");
		else if (action->type)
			return rte_flow_error_set
				(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION,
				 action, "unsupported action combination");
		return rte_flow_error_set
			(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			 "flow rule lacks some kind of fate action");
	case BACK:
		trans = back;
		n = 0;
		goto trans;
	case ATTR:
		/*
		 * Supported attributes: no groups, some priorities and
		 * ingress only. Don't care about transfer as it is the
		 * caller's problem.
		 */
		if (attr->group)
			return rte_flow_error_set
				(error, ENOTSUP,
				 RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
				 attr, "groups are not supported");
		if (attr->priority > 0xfffe)
			return rte_flow_error_set
				(error, ENOTSUP,
				 RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
				 attr, "lowest priority level is 0xfffe");
		if (!attr->ingress)
			return rte_flow_error_set
				(error, ENOTSUP,
				 RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
				 attr, "only ingress is supported");
		if (attr->egress)
			return rte_flow_error_set
				(error, ENOTSUP,
				 RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
				 attr, "egress is not supported");
		if (size < mnl_nlmsg_size(sizeof(*tcm)))
			goto error_nobufs;
		nlh = mnl_nlmsg_put_header(buf);
		nlh->nlmsg_type = 0;
		nlh->nlmsg_flags = 0;
		nlh->nlmsg_seq = 0;
		tcm = mnl_nlmsg_put_extra_header(nlh, sizeof(*tcm));
		tcm->tcm_family = AF_UNSPEC;
		tcm->tcm_ifindex = ptoi[0].ifindex;
		/*
		 * Let kernel pick a handle by default. A predictable handle
		 * can be set by the caller on the resulting buffer through
		 * mlx5_nl_flow_brand().
		 */
		tcm->tcm_handle = 0;
		tcm->tcm_parent = TC_H_MAKE(TC_H_INGRESS, TC_H_MIN_INGRESS);
		/*
		 * Priority cannot be zero to prevent the kernel from
		 * picking one automatically.
		 */
		tcm->tcm_info = TC_H_MAKE((attr->priority + 1) << 16,
					  RTE_BE16(ETH_P_ALL));
		break;
	case PATTERN:
		if (!mnl_attr_put_strz_check(buf, size, TCA_KIND, "flower"))
			goto error_nobufs;
		na_flower = mnl_attr_nest_start_check(buf, size, TCA_OPTIONS);
		if (!na_flower)
			goto error_nobufs;
		if (!mnl_attr_put_u32_check(buf, size, TCA_FLOWER_FLAGS,
					    TCA_CLS_FLAGS_SKIP_SW))
			goto error_nobufs;
		break;
	case ITEM_VOID:
		if (item->type != RTE_FLOW_ITEM_TYPE_VOID)
			goto trans;
		++item;
		break;
	case ACTIONS:
		if (item->type != RTE_FLOW_ITEM_TYPE_END)
			goto trans;
		assert(na_flower);
		assert(!na_flower_act);
		na_flower_act =
			mnl_attr_nest_start_check(buf, size, TCA_FLOWER_ACT);
		if (!na_flower_act)
			goto error_nobufs;
		break;
	case ACTION_VOID:
		if (action->type != RTE_FLOW_ACTION_TYPE_VOID)
			goto trans;
		++action;
		break;
	case END:
		if (item->type != RTE_FLOW_ITEM_TYPE_END ||
		    action->type != RTE_FLOW_ACTION_TYPE_END)
			goto trans;
		if (na_flower_act)
			mnl_attr_nest_end(buf, na_flower_act);
		if (na_flower)
			mnl_attr_nest_end(buf, na_flower);
		nlh = buf;
		return nlh->nlmsg_len;
	}
	back = trans;
	trans = mlx5_nl_flow_trans[trans[n - 1]];
	n = 0;
	goto trans;
error_nobufs:
	if (buf != buf_tmp) {
		buf = buf_tmp;
		size = sizeof(buf_tmp);
		goto init;
	}
	return rte_flow_error_set
		(error, ENOBUFS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
		 "generated TC message is too large");
}

/**
 * Brand rtnetlink buffer with unique handle.
 *
 * This handle should be unique for a given network interface to avoid
 * collisions.
 *
 * @param buf
 *   Flow rule buffer previously initialized by mlx5_nl_flow_transpose().
 * @param handle
 *   Unique 32-bit handle to use.
 */
void
mlx5_nl_flow_brand(void *buf, uint32_t handle)
{
	struct tcmsg *tcm = mnl_nlmsg_get_payload(buf);

	tcm->tcm_handle = handle;
}

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
 * Create a Netlink flow rule.
 *
 * @param nl
 *   Libmnl socket to use.
 * @param buf
 *   Flow rule buffer previously initialized by mlx5_nl_flow_transpose().
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_nl_flow_create(struct mnl_socket *nl, void *buf,
		    struct rte_flow_error *error)
{
	struct nlmsghdr *nlh = buf;

	nlh->nlmsg_type = RTM_NEWTFILTER;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
	if (!mlx5_nl_flow_nl_ack(nl, nlh))
		return 0;
	return rte_flow_error_set
		(error, rte_errno, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
		 "netlink: failed to create TC flow rule");
}

/**
 * Destroy a Netlink flow rule.
 *
 * @param nl
 *   Libmnl socket to use.
 * @param buf
 *   Flow rule buffer previously initialized by mlx5_nl_flow_transpose().
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_nl_flow_destroy(struct mnl_socket *nl, void *buf,
		     struct rte_flow_error *error)
{
	struct nlmsghdr *nlh = buf;

	nlh->nlmsg_type = RTM_DELTFILTER;
	nlh->nlmsg_flags = NLM_F_REQUEST;
	if (!mlx5_nl_flow_nl_ack(nl, nlh))
		return 0;
	return rte_flow_error_set
		(error, errno, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
		 "netlink: failed to destroy TC flow rule");
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
