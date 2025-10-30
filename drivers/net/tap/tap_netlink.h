/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#ifndef _TAP_NETLINK_H_
#define _TAP_NETLINK_H_

#include <inttypes.h>
#include <linux/rtnetlink.h>
#include <linux/netlink.h>

#include <rte_ether.h>
#include <rte_log.h>

#define NLMSG_BUF 512

struct tap_nlmsg {
	struct nlmsghdr nh;
	struct tcmsg t;
	char buf[NLMSG_BUF];
	struct nested_tail *nested_tails;
};

#define NLMSG_TAIL(msg) (void *)((char *)(msg) + NLMSG_ALIGN((msg)->nh.nlmsg_len))

int tap_nl_init(uint32_t nl_groups);
int tap_nl_final(int nlsk_fd);
int tap_nl_send(int nlsk_fd, struct nlmsghdr *nh);
int tap_nl_recv(int nlsk_fd, int (*callback)(struct nlmsghdr *, void *),
		void *arg);
int tap_nl_recv_ack(int nlsk_fd);
void tap_nlattr_add(struct tap_nlmsg *msg, unsigned short type,
		    unsigned int data_len, const void *data);
void tap_nlattr_add8(struct tap_nlmsg *msg, unsigned short type, uint8_t data);
void tap_nlattr_add16(struct tap_nlmsg *msg, unsigned short type, uint16_t data);
void tap_nlattr_add32(struct tap_nlmsg *msg, unsigned short type, uint32_t data);
int tap_nlattr_nested_start(struct tap_nlmsg *msg, uint16_t type);
void tap_nlattr_nested_finish(struct tap_nlmsg *msg);

/* Link management functions using netlink */
int tap_nl_get_flags(int nlsk_fd, unsigned int ifindex, unsigned int *flags);
int tap_nl_set_flags(int nlsk_fd, unsigned int ifindex, unsigned int flags, int set);
int tap_nl_set_mtu(int nlsk_fd, unsigned int ifindex, unsigned int mtu);
int tap_nl_set_mac(int nlsk_fd, unsigned int ifindex, const struct rte_ether_addr *mac);
int tap_nl_get_mac(int nlsk_fd, unsigned int ifindex, struct rte_ether_addr *mac);

#endif /* _TAP_NETLINK_H_ */
