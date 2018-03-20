/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#ifndef _TAP_NETLINK_H_
#define _TAP_NETLINK_H_

#include <ctype.h>
#include <inttypes.h>
#include <linux/rtnetlink.h>
#include <linux/netlink.h>
#include <stdio.h>

#include <rte_log.h>

#define NLMSG_BUF 512

struct nlmsg {
	struct nlmsghdr nh;
	struct tcmsg t;
	char buf[NLMSG_BUF];
	struct nested_tail *nested_tails;
};

#define NLMSG_TAIL(nlh) (void *)((char *)(nlh) + NLMSG_ALIGN((nlh)->nlmsg_len))

int tap_nl_init(uint32_t nl_groups);
int tap_nl_final(int nlsk_fd);
int tap_nl_send(int nlsk_fd, struct nlmsghdr *nh);
int tap_nl_recv(int nlsk_fd, int (*callback)(struct nlmsghdr *, void *),
		void *arg);
int tap_nl_recv_ack(int nlsk_fd);
void tap_nlattr_add(struct nlmsghdr *nh, unsigned short type,
		    unsigned int data_len, const void *data);
void tap_nlattr_add8(struct nlmsghdr *nh, unsigned short type, uint8_t data);
void tap_nlattr_add16(struct nlmsghdr *nh, unsigned short type, uint16_t data);
void tap_nlattr_add32(struct nlmsghdr *nh, unsigned short type, uint32_t data);
int tap_nlattr_nested_start(struct nlmsg *msg, uint16_t type);
void tap_nlattr_nested_finish(struct nlmsg *msg);

#endif /* _TAP_NETLINK_H_ */
