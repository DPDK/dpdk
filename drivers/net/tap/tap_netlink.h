/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 6WIND S.A.
 *   Copyright 2017 Mellanox.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of 6WIND S.A. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

int nl_init(uint32_t nl_groups);
int nl_final(int nlsk_fd);
int nl_send(int nlsk_fd, struct nlmsghdr *nh);
int nl_recv(int nlsk_fd, int (*callback)(struct nlmsghdr *, void *), void *arg);
int nl_recv_ack(int nlsk_fd);
void nlattr_add(struct nlmsghdr *nh, unsigned short type,
		unsigned int data_len, const void *data);
void nlattr_add8(struct nlmsghdr *nh, unsigned short type, uint8_t data);
void nlattr_add16(struct nlmsghdr *nh, unsigned short type, uint16_t data);
void nlattr_add32(struct nlmsghdr *nh, unsigned short type, uint32_t data);
int nlattr_nested_start(struct nlmsg *msg, uint16_t type);
void nlattr_nested_finish(struct nlmsg *msg);

#endif /* _TAP_NETLINK_H_ */
