/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#ifndef _TAP_TCMSGS_H_
#define _TAP_TCMSGS_H_

#include <tap_autoconf.h>
#include <linux/if_ether.h>
#include <linux/rtnetlink.h>
#include <linux/pkt_sched.h>
#include <linux/pkt_cls.h>
#include <linux/tc_act/tc_mirred.h>
#include <linux/tc_act/tc_gact.h>
#include <linux/tc_act/tc_skbedit.h>
#ifdef HAVE_TC_ACT_BPF
#include <linux/tc_act/tc_bpf.h>
#endif
#include <inttypes.h>

#include <rte_ether.h>
#include <tap_netlink.h>

#define MULTIQ_MAJOR_HANDLE (1 << 16)

void tc_init_msg(struct nlmsg *msg, unsigned int ifindex, uint16_t type,
		 uint16_t flags);
int qdisc_list(int nlsk_fd, unsigned int ifindex);
int qdisc_flush(int nlsk_fd, unsigned int ifindex);
int qdisc_create_ingress(int nlsk_fd, unsigned int ifindex);
int qdisc_create_multiq(int nlsk_fd, unsigned int ifindex);
int qdisc_add_ingress(int nlsk_fd, unsigned int ifindex);
int qdisc_add_multiq(int nlsk_fd, unsigned int ifindex);
int filter_list_ingress(int nlsk_fd, unsigned int ifindex);

#endif /* _TAP_TCMSGS_H_ */
