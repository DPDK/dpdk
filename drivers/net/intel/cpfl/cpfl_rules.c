/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2023 Intel Corporation
 */

#include <base/idpf_controlq.h>
#include <stdint.h>
#include "cpfl_rules.h"

 /**
  * cpfl_prep_rule_desc_common_ctx - get bit common context for descriptor
  */
static inline uint64_t
cpfl_prep_rule_desc_common_ctx(struct cpfl_rule_cfg_data_common *cmn_cfg)
{
	uint64_t context = 0;

	switch (cmn_cfg->opc) {
	case cpfl_ctlq_mod_query_rule:
	case cpfl_ctlq_mod_add_update_rule:
		/* fallthrough */
	case cpfl_ctlq_sem_query_rule_hash_addr:
	case cpfl_ctlq_sem_query_del_rule_hash_addr:
	case cpfl_ctlq_sem_add_rule:
	case cpfl_ctlq_sem_del_rule:
	case cpfl_ctlq_sem_query_rule:
	case cpfl_ctlq_sem_update_rule:
		context |= SHIFT_VAL64(cmn_cfg->time_sel,
				       MEV_RULE_TIME_SEL);
		context |= SHIFT_VAL64(cmn_cfg->time_sel_val,
				       MEV_RULE_TIME_SEL_VAL);
		context |= SHIFT_VAL64(cmn_cfg->host_id,
				       MEV_RULE_HOST_ID);
		context |= SHIFT_VAL64(cmn_cfg->port_num,
				       MEV_RULE_PORT_NUM);
		context |= SHIFT_VAL64(cmn_cfg->resp_req,
				       MEV_RULE_RESP_REQ);
		context |= SHIFT_VAL64(cmn_cfg->cache_wr_thru,
				       MEV_RULE_CACHE_WR_THRU);
		break;
	default:
		break;
	}

	return context;
}

/**
 * cpfl_prep_rule_desc_ctx - get bit context for descriptor
 */
static inline uint64_t
cpfl_prep_rule_desc_ctx(struct cpfl_rule_cfg_data *cfg_data)
{
	uint64_t context = 0;

	context |= cpfl_prep_rule_desc_common_ctx(&cfg_data->common);

	switch (cfg_data->common.opc) {
	case cpfl_ctlq_mod_query_rule:
	case cpfl_ctlq_mod_add_update_rule:
		context |= SHIFT_VAL64(cfg_data->ext.mod_content.obj_size,
				       MEV_RULE_MOD_OBJ_SIZE);
		context |= SHIFT_VAL64(cfg_data->ext.mod_content.pin_content,
				       MEV_RULE_PIN_MOD_CONTENT);
		context |= SHIFT_VAL64(cfg_data->ext.mod_content.index,
				       MEV_RULE_MOD_INDEX);
		break;
	case cpfl_ctlq_sem_query_rule_hash_addr:
	case cpfl_ctlq_sem_query_del_rule_hash_addr:
		context |= SHIFT_VAL64(cfg_data->ext.query_del_addr.obj_id,
				       MEV_RULE_OBJ_ID);
		context |= SHIFT_VAL64(cfg_data->ext.query_del_addr.obj_addr,
				       MEV_RULE_OBJ_ADDR);
		break;
	default:
		break;
	}

	return context;
}

/**
 * cpfl_prep_rule_desc - build descriptor data from rule config data
 *
 * note: call this function before sending rule to HW via fast path
 */
void
cpfl_prep_rule_desc(struct cpfl_rule_cfg_data *cfg_data,
		    struct idpf_ctlq_msg *ctlq_msg)
{
	uint64_t context;
	uint64_t *ctlq_ctx = (uint64_t *)&ctlq_msg->ctx.indirect.context[0];

	context = cpfl_prep_rule_desc_ctx(cfg_data);
	*ctlq_ctx = CPU_TO_LE64(context);
	memcpy(&ctlq_msg->cookie, &cfg_data->common.cookie, sizeof(uint64_t));
	ctlq_msg->opcode = (uint16_t)cfg_data->common.opc;
	ctlq_msg->data_len = cfg_data->common.buf_len;
	ctlq_msg->status = 0;
	ctlq_msg->ctx.indirect.payload = cfg_data->common.payload;
}

/**
 * cpfl_prep_sem_rule_blob - build SEM rule blob data from rule entry info
 * note: call this function before sending rule to HW via fast path
 */
void
cpfl_prep_sem_rule_blob(const uint8_t *key,
			uint8_t key_byte_len,
			const uint8_t *act_bytes,
			uint8_t act_byte_len,
			uint16_t cfg_ctrl,
			union cpfl_rule_cfg_pkt_record *rule_blob)
{
	uint32_t *act_dst = (uint32_t *)&rule_blob->sem_rule.actions;
	const uint32_t *act_src = (const uint32_t *)act_bytes;
	uint32_t i;

	idpf_memset(rule_blob, 0, sizeof(*rule_blob), IDPF_DMA_MEM);
	memcpy(rule_blob->sem_rule.key, key, key_byte_len);

	for (i = 0; i < act_byte_len / sizeof(uint32_t); i++)
		*act_dst++ = CPU_TO_LE32(*act_src++);

	rule_blob->sem_rule.cfg_ctrl[0] = cfg_ctrl & 0xFF;
	rule_blob->sem_rule.cfg_ctrl[1] = (cfg_ctrl >> 8) & 0xFF;
}
