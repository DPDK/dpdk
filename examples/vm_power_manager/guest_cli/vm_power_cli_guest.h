/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef VM_POWER_CLI_H_
#define VM_POWER_CLI_H_

#ifdef __cplusplus
extern "C" {
#endif

struct rte_power_channel_packet *get_policy(void);

int set_policy_mac(int port, int idx);

int set_policy_defaults(struct rte_power_channel_packet *pkt);

void run_cli(__rte_unused void *arg);

#ifdef __cplusplus
}
#endif

#endif /* VM_POWER_CLI_H_ */
