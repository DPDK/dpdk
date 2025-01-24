/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#ifndef _CPFL_FXP_RULE_H_
#define _CPFL_FXP_RULE_H_

#include "cpfl_rules.h"

#define CPFL_MAX_KEY_LEN 128
#define CPFL_MAX_RULE_ACTIONS 32

struct cpfl_sem_rule_info {
	uint16_t prof_id;
	uint8_t sub_prof_id;
	uint8_t key[CPFL_MAX_KEY_LEN];
	uint8_t key_byte_len;
	uint8_t pin_to_cache;
	uint8_t fixed_fetch;
};

#define CPFL_MAX_MOD_CONTENT_LEN 256
struct cpfl_mod_rule_info {
	uint8_t mod_content[CPFL_MAX_MOD_CONTENT_LEN];
	uint8_t mod_content_byte_len;
	uint32_t mod_index;
	uint8_t pin_mod_content;
	uint8_t mod_obj_size;
};

enum cpfl_rule_type {
	CPFL_RULE_TYPE_NONE,
	CPFL_RULE_TYPE_SEM,
	CPFL_RULE_TYPE_MOD
};

struct cpfl_rule_info {
	enum cpfl_rule_type type;
	uint64_t cookie;
	uint8_t host_id;
	uint8_t port_num;
	uint8_t resp_req;
	/* TODO: change this to be dynamically allocated/reallocated */
	uint8_t act_bytes[CPFL_MAX_RULE_ACTIONS * sizeof(union cpfl_action_set)];
	uint8_t act_byte_len;
	/* vsi is used for lem and lpm rules */
	uint16_t vsi;
	uint8_t clear_mirror_1st_state;
	/* mod related fields */
	union {
		struct cpfl_mod_rule_info mod;
		struct cpfl_sem_rule_info sem;
	};
};

extern struct cpfl_vport_ext *vport;

int cpfl_rule_process(struct cpfl_itf *itf,
		      struct idpf_ctlq_info *tx_cq,
		      struct idpf_ctlq_info *rx_cq,
		      struct cpfl_rule_info *rinfo,
		      int rule_num,
		      bool add);
int cpfl_send_ctlq_msg(struct idpf_hw *hw, struct idpf_ctlq_info *cq, u16 num_q_msg,
		       struct idpf_ctlq_msg q_msg[]);
int cpfl_receive_ctlq_msg(struct idpf_hw *hw, struct idpf_ctlq_info *cq, u16 num_q_msg,
			  struct idpf_ctlq_msg q_msg[]);
#endif /*CPFL_FXP_RULE_H*/
