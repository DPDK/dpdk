/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */
#include "cpfl_ethdev.h"

#include "cpfl_fxp_rule.h"
#include "cpfl_logs.h"

#define CTLQ_SEND_RETRIES 100
#define CTLQ_RECEIVE_RETRIES 100

int
cpfl_send_ctlq_msg(struct idpf_hw *hw, struct idpf_ctlq_info *cq, u16 num_q_msg,
		   struct idpf_ctlq_msg q_msg[])
{
	struct idpf_ctlq_msg **msg_ptr_list;
	u16 clean_count = 0;
	int num_cleaned = 0;
	int retries = 0;
	int ret = 0;

	msg_ptr_list = calloc(num_q_msg, sizeof(struct idpf_ctlq_msg *));
	if (!msg_ptr_list) {
		PMD_INIT_LOG(ERR, "no memory for cleaning ctlq");
		ret = -ENOMEM;
		goto err;
	}

	ret = cpfl_vport_ctlq_send(hw, cq, num_q_msg, q_msg);
	if (ret) {
		PMD_INIT_LOG(ERR, "cpfl_vport_ctlq_send() failed with error: 0x%4x", ret);
		goto send_err;
	}

	while (retries <= CTLQ_SEND_RETRIES) {
		clean_count = num_q_msg - num_cleaned;
		ret = cpfl_vport_ctlq_clean_sq(cq, &clean_count,
					       &msg_ptr_list[num_cleaned]);
		if (ret) {
			PMD_INIT_LOG(ERR, "clean ctlq failed: 0x%4x", ret);
			goto send_err;
		}

		num_cleaned += clean_count;
		retries++;
		if (num_cleaned >= num_q_msg)
			break;
		rte_delay_us_sleep(10);
	}

	if (retries > CTLQ_SEND_RETRIES) {
		PMD_INIT_LOG(ERR, "timed out while polling for completions");
		ret = -1;
		goto send_err;
	}

send_err:
	if (msg_ptr_list)
		free(msg_ptr_list);
err:
	return ret;
}

int
cpfl_receive_ctlq_msg(struct idpf_hw *hw, struct idpf_ctlq_info *cq, u16 num_q_msg,
		      struct idpf_ctlq_msg q_msg[])
{
	int retries = 0;
	struct idpf_dma_mem *dma;
	u16 i;
	uint16_t buff_cnt;
	int ret = 0;

	retries = 0;
	while (retries <= CTLQ_RECEIVE_RETRIES) {
		rte_delay_us_sleep(10);
		ret = cpfl_vport_ctlq_recv(cq, &num_q_msg, &q_msg[0]);

		if (ret && ret != CPFL_ERR_CTLQ_NO_WORK &&
		    ret != CPFL_ERR_CTLQ_ERROR) {
			PMD_INIT_LOG(ERR, "failed to recv ctrlq msg. err: 0x%4x\n", ret);
			retries++;
			continue;
		}

		if (ret == CPFL_ERR_CTLQ_NO_WORK) {
			retries++;
			continue;
		}

		if (ret == CPFL_ERR_CTLQ_EMPTY)
			break;

		/* TODO - process rx controlq message */
		for (i = 0; i < num_q_msg; i++) {
			if (q_msg[i].data_len > 0)
				dma = q_msg[i].ctx.indirect.payload;
			else
				dma = NULL;

			buff_cnt = dma ? 1 : 0;
			ret = cpfl_vport_ctlq_post_rx_buffs(hw, cq, &buff_cnt, &dma);
			if (ret)
				PMD_INIT_LOG(WARNING, "could not posted recv bufs\n");
		}
		break;
	}

	if (retries > CTLQ_RECEIVE_RETRIES) {
		PMD_INIT_LOG(ERR, "timed out while polling for receive response");
		ret = -1;
	}

	return ret;
}

static int
cpfl_mod_rule_pack(struct cpfl_rule_info *rinfo, struct idpf_dma_mem *dma,
		   struct idpf_ctlq_msg *msg)
{
	struct cpfl_mod_rule_info *minfo = &rinfo->mod;
	union cpfl_rule_cfg_pkt_record *blob = NULL;
	struct cpfl_rule_cfg_data cfg = {0};

	/* prepare rule blob */
	if (!dma->va) {
		PMD_INIT_LOG(ERR, "dma mem passed to %s is null\n", __func__);
		return -1;
	}
	blob = (union cpfl_rule_cfg_pkt_record *)dma->va;
	memset(blob, 0, sizeof(*blob));
	memset(&cfg, 0, sizeof(cfg));

	/* fill info for both query and add/update */
	cpfl_fill_rule_mod_content(minfo->mod_obj_size,
				   minfo->pin_mod_content,
				   minfo->mod_index,
				   &cfg.ext.mod_content);

	/* only fill content for add/update */
	memcpy(blob->mod_blob, minfo->mod_content,
	       minfo->mod_content_byte_len);

#define NO_HOST_NEEDED 0
	/* pack message */
	cpfl_fill_rule_cfg_data_common(cpfl_ctlq_mod_add_update_rule,
				       rinfo->cookie,
				       0, /* vsi_id not used for mod */
				       rinfo->port_num,
				       NO_HOST_NEEDED,
				       0, /* time_sel */
				       0, /* time_sel_val */
				       0, /* cache_wr_thru */
				       rinfo->resp_req,
				       (u16)sizeof(*blob),
				       (void *)dma,
				       &cfg.common);
	cpfl_prep_rule_desc(&cfg, msg);
	return 0;
}

static int
cpfl_default_rule_pack(struct cpfl_rule_info *rinfo, struct idpf_dma_mem *dma,
		       struct idpf_ctlq_msg *msg, bool add)
{
	union cpfl_rule_cfg_pkt_record *blob = NULL;
	enum cpfl_ctlq_rule_cfg_opc opc;
	struct cpfl_rule_cfg_data cfg;
	uint16_t cfg_ctrl;

	if (!dma->va) {
		PMD_INIT_LOG(ERR, "dma mem passed to %s is null\n", __func__);
		return -1;
	}
	blob = (union cpfl_rule_cfg_pkt_record *)dma->va;
	memset(blob, 0, sizeof(*blob));
	memset(msg, 0, sizeof(*msg));

	if (rinfo->type == CPFL_RULE_TYPE_SEM) {
		cfg_ctrl = CPFL_GET_MEV_SEM_RULE_CFG_CTRL(rinfo->sem.prof_id,
							  rinfo->sem.sub_prof_id,
							  rinfo->sem.pin_to_cache,
							  rinfo->sem.fixed_fetch);
		cpfl_prep_sem_rule_blob(rinfo->sem.key, rinfo->sem.key_byte_len,
					rinfo->act_bytes, rinfo->act_byte_len,
					cfg_ctrl, blob);
		opc = add ? cpfl_ctlq_sem_add_rule : cpfl_ctlq_sem_del_rule;
	} else {
		PMD_INIT_LOG(ERR, "not support %d rule.", rinfo->type);
		return -1;
	}

	cpfl_fill_rule_cfg_data_common(opc,
				       rinfo->cookie,
				       rinfo->vsi,
				       rinfo->port_num,
				       rinfo->host_id,
				       0, /* time_sel */
				       0, /* time_sel_val */
				       0, /* cache_wr_thru */
				       rinfo->resp_req,
				       sizeof(union cpfl_rule_cfg_pkt_record),
				       dma,
				       &cfg.common);
	cpfl_prep_rule_desc(&cfg, msg);
	return 0;
}

static int
cpfl_rule_pack(struct cpfl_rule_info *rinfo, struct idpf_dma_mem *dma,
	       struct idpf_ctlq_msg *msg, bool add)
{
	int ret = 0;

	if (rinfo->type == CPFL_RULE_TYPE_SEM) {
		if (cpfl_default_rule_pack(rinfo, dma, msg, add) < 0)
			ret = -1;
	} else if (rinfo->type == CPFL_RULE_TYPE_MOD) {
		if (cpfl_mod_rule_pack(rinfo, dma, msg) < 0)
			ret = -1;
	} else {
		PMD_INIT_LOG(ERR, "Invalid type of rule");
		ret = -1;
	}

	return ret;
}

int
cpfl_rule_process(struct cpfl_itf *itf,
		  struct idpf_ctlq_info *tx_cq,
		  struct idpf_ctlq_info *rx_cq,
		  struct cpfl_rule_info *rinfo,
		  int rule_num,
		  bool add)
{
	struct idpf_hw *hw = &itf->adapter->base.hw;
	int i;
	int ret = 0;

	if (rule_num == 0)
		return 0;

	for (i = 0; i < rule_num; i++) {
		ret = cpfl_rule_pack(&rinfo[i], &itf->dma[i], &itf->msg[i], add);
		if (ret) {
			PMD_INIT_LOG(ERR, "Could not pack rule");
			return ret;
		}
	}
	ret = cpfl_send_ctlq_msg(hw, tx_cq, rule_num, itf->msg);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to send control message");
		return ret;
	}
	ret = cpfl_receive_ctlq_msg(hw, rx_cq, rule_num, itf->msg);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to update rule");
		return ret;
	}

	return 0;
}
