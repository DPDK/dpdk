/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include <fcntl.h>
#include <unistd.h>

#include <cnxk_flow.h>
#include <cnxk_rep.h>
#include <cnxk_rep_msg.h>

#define DEFAULT_DUMP_FILE_NAME "/tmp/fdump"
#define MAX_BUFFER_SIZE	       1500

const struct cnxk_rte_flow_action_info action_info[] = {
	[RTE_FLOW_ACTION_TYPE_MARK] = {sizeof(struct rte_flow_action_mark)},
	[RTE_FLOW_ACTION_TYPE_VF] = {sizeof(struct rte_flow_action_vf)},
	[RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT] = {sizeof(struct rte_flow_action_port_id)},
	[RTE_FLOW_ACTION_TYPE_PORT_ID] = {sizeof(struct rte_flow_action_port_id)},
	[RTE_FLOW_ACTION_TYPE_QUEUE] = {sizeof(struct rte_flow_action_queue)},
	[RTE_FLOW_ACTION_TYPE_RSS] = {sizeof(struct rte_flow_action_rss)},
	[RTE_FLOW_ACTION_TYPE_SECURITY] = {sizeof(struct rte_flow_action_security)},
	[RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID] = {sizeof(struct rte_flow_action_of_set_vlan_vid)},
	[RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN] = {sizeof(struct rte_flow_action_of_push_vlan)},
	[RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP] = {sizeof(struct rte_flow_action_of_set_vlan_pcp)},
	[RTE_FLOW_ACTION_TYPE_METER] = {sizeof(struct rte_flow_action_meter)},
	[RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP] = {sizeof(struct rte_flow_action_vxlan_encap)},
	[RTE_FLOW_ACTION_TYPE_COUNT] = {sizeof(struct rte_flow_action_count)},
};

static void
cnxk_flow_params_count(const struct rte_flow_item pattern[], const struct rte_flow_action actions[],
		       uint16_t *n_pattern, uint16_t *n_action)
{
	int i = 0;

	for (; pattern->type != RTE_FLOW_ITEM_TYPE_END; pattern++)
		i++;

	*n_pattern = ++i;
	plt_rep_dbg("Total patterns is %d", *n_pattern);

	i = 0;
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++)
		i++;
	*n_action = ++i;
	plt_rep_dbg("Total actions is %d", *n_action);
}

static void
populate_attr_data(void *buffer, uint32_t *length, const struct rte_flow_attr *attr)
{
	uint32_t sz = sizeof(struct rte_flow_attr);
	uint32_t len;

	cnxk_rep_msg_populate_type(buffer, length, CNXK_TYPE_ATTR, sz);

	len = *length;
	/* Populate the attribute data */
	rte_memcpy(RTE_PTR_ADD(buffer, len), attr, sz);
	len += sz;

	*length = len;
}

static uint16_t
prepare_pattern_data(const struct rte_flow_item *pattern, uint16_t nb_pattern,
		     uint64_t *pattern_data)
{
	cnxk_pattern_hdr_t hdr;
	uint16_t len = 0;
	int i = 0;

	for (i = 0; i < nb_pattern; i++) {
		/* Populate the pattern type hdr */
		memset(&hdr, 0, sizeof(cnxk_pattern_hdr_t));
		hdr.type = pattern->type;
		if (pattern->spec) {
			hdr.spec_sz = term[pattern->type].item_size;
			hdr.last_sz = 0;
			hdr.mask_sz = term[pattern->type].item_size;
		}

		rte_memcpy(RTE_PTR_ADD(pattern_data, len), &hdr, sizeof(cnxk_pattern_hdr_t));
		len += sizeof(cnxk_pattern_hdr_t);

		/* Copy pattern spec data */
		if (pattern->spec) {
			rte_memcpy(RTE_PTR_ADD(pattern_data, len), pattern->spec,
				   term[pattern->type].item_size);
			len += term[pattern->type].item_size;
		}

		/* Copy pattern last data */
		if (pattern->last) {
			rte_memcpy(RTE_PTR_ADD(pattern_data, len), pattern->last,
				   term[pattern->type].item_size);
			len += term[pattern->type].item_size;
		}

		/* Copy pattern mask data */
		if (pattern->mask) {
			rte_memcpy(RTE_PTR_ADD(pattern_data, len), pattern->mask,
				   term[pattern->type].item_size);
			len += term[pattern->type].item_size;
		}
		pattern++;
	}

	return len;
}

static void
populate_pattern_data(void *buffer, uint32_t *length, const struct rte_flow_item *pattern,
		      uint16_t nb_pattern)
{
	uint64_t pattern_data[BUFSIZ];
	uint32_t len;
	uint32_t sz;

	memset(pattern_data, 0, BUFSIZ * sizeof(uint64_t));
	/* Prepare pattern_data */
	sz = prepare_pattern_data(pattern, nb_pattern, pattern_data);

	cnxk_rep_msg_populate_type(buffer, length, CNXK_TYPE_PATTERN, sz);

	len = *length;
	/* Populate the pattern data */
	rte_memcpy(RTE_PTR_ADD(buffer, len), pattern_data, sz);
	len += sz;

	*length = len;
}

static uint16_t
populate_rss_action_conf(const struct rte_flow_action_rss *conf, void *rss_action_conf)
{
	int len, sz;

	len = sizeof(struct rte_flow_action_rss) - sizeof(conf->key) - sizeof(conf->queue);

	if (rss_action_conf)
		rte_memcpy(rss_action_conf, conf, len);

	if (conf->key) {
		sz = conf->key_len;
		if (rss_action_conf)
			rte_memcpy(RTE_PTR_ADD(rss_action_conf, len), conf->key, sz);
		len += sz;
	}

	if (conf->queue) {
		sz = conf->queue_num * sizeof(conf->queue);
		if (rss_action_conf)
			rte_memcpy(RTE_PTR_ADD(rss_action_conf, len), conf->queue, sz);
		len += sz;
	}

	return len;
}

static uint16_t
populate_vxlan_encap_action_conf(const struct rte_flow_action_vxlan_encap *vxlan_conf,
				 void *vxlan_encap_action_data)
{
	const struct rte_flow_item *pattern;
	uint64_t nb_patterns = 0;
	uint16_t len, sz;

	pattern = vxlan_conf->definition;
	for (; pattern->type != RTE_FLOW_ITEM_TYPE_END; pattern++)
		nb_patterns++;

	/* +1 for RTE_FLOW_ITEM_TYPE_END */
	nb_patterns++;

	len = sizeof(uint64_t);
	rte_memcpy(vxlan_encap_action_data, &nb_patterns, len);
	pattern = vxlan_conf->definition;
	/* Prepare pattern_data */
	sz = prepare_pattern_data(pattern, nb_patterns, RTE_PTR_ADD(vxlan_encap_action_data, len));

	len += sz;
	if (len > BUFSIZ) {
		plt_err("Incomplete item definition loaded, len %d", len);
		return 0;
	}

	return len;
}

static uint16_t
prepare_action_data(const struct rte_flow_action *action, uint16_t nb_action, uint64_t *action_data)
{
	void *action_conf_data = NULL;
	cnxk_action_hdr_t hdr;
	uint16_t len = 0, sz = 0;
	int i = 0;

	for (i = 0; i < nb_action; i++) {
		if (action->conf) {
			switch (action->type) {
			case RTE_FLOW_ACTION_TYPE_RSS:
				sz = populate_rss_action_conf(action->conf, NULL);
				action_conf_data = plt_zmalloc(sz, 0);
				if (populate_rss_action_conf(action->conf, action_conf_data) !=
				    sz) {
					plt_err("Populating RSS action config failed");
					return 0;
				}
				break;
			case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
				action_conf_data = plt_zmalloc(BUFSIZ, 0);
				sz = populate_vxlan_encap_action_conf(action->conf,
								      action_conf_data);
				if (!sz) {
					plt_err("Populating vxlan action config failed");
					return 0;
				}
				break;
			default:
				sz = action_info[action->type].conf_size;
				action_conf_data = plt_zmalloc(sz, 0);
				rte_memcpy(action_conf_data, action->conf, sz);
				break;
			};
		}

		/* Populate the action type hdr */
		memset(&hdr, 0, sizeof(cnxk_action_hdr_t));
		hdr.type = action->type;
		hdr.conf_sz = sz;

		rte_memcpy(RTE_PTR_ADD(action_data, len), &hdr, sizeof(cnxk_action_hdr_t));
		len += sizeof(cnxk_action_hdr_t);

		/* Copy action conf data */
		if (action_conf_data) {
			rte_memcpy(RTE_PTR_ADD(action_data, len), action_conf_data, sz);
			len += sz;
			plt_free(action_conf_data);
			action_conf_data = NULL;
		}

		action++;
	}

	return len;
}

static void
populate_action_data(void *buffer, uint32_t *length, const struct rte_flow_action *action,
		     uint16_t nb_action)
{
	uint64_t action_data[BUFSIZ];
	uint32_t len;
	uint32_t sz;

	memset(action_data, 0, BUFSIZ * sizeof(uint64_t));
	/* Prepare action_data */
	sz = prepare_action_data(action, nb_action, action_data);

	cnxk_rep_msg_populate_type(buffer, length, CNXK_TYPE_ACTION, sz);

	len = *length;
	/* Populate the action data */
	rte_memcpy(RTE_PTR_ADD(buffer, len), action_data, sz);
	len += sz;

	*length = len;
}

static int
process_flow_destroy(struct cnxk_rep_dev *rep_dev, void *flow, cnxk_rep_msg_ack_data_t *adata)
{
	cnxk_rep_msg_flow_destroy_meta_t msg_fd_meta;
	uint32_t len = 0, rc;
	void *buffer;
	size_t size;

	/* If representor not representing any active VF, return 0 */
	if (!rep_dev->is_vf_active)
		return 0;

	size = MAX_BUFFER_SIZE;
	buffer = plt_zmalloc(size, 0);
	if (!buffer) {
		plt_err("Failed to allocate mem");
		rc = -ENOMEM;
		goto fail;
	}

	cnxk_rep_msg_populate_header(buffer, &len);

	msg_fd_meta.portid = rep_dev->rep_id;
	msg_fd_meta.flow = (uint64_t)flow;
	plt_rep_dbg("Flow Destroy: flow 0x%" PRIu64 ", portid %d", msg_fd_meta.flow,
		    msg_fd_meta.portid);
	cnxk_rep_msg_populate_command_meta(buffer, &len, &msg_fd_meta,
					   sizeof(cnxk_rep_msg_flow_destroy_meta_t),
					   CNXK_REP_MSG_FLOW_DESTROY);
	cnxk_rep_msg_populate_msg_end(buffer, &len);

	rc = cnxk_rep_msg_send_process(rep_dev, buffer, len, adata);
	if (rc) {
		plt_err("Failed to process the message, err %d", rc);
		goto fail;
	}

	return 0;
fail:
	return rc;
}

static int
copy_flow_dump_file(FILE *target)
{
	FILE *source = NULL;
	int pos;
	char ch;

	source = fopen(DEFAULT_DUMP_FILE_NAME, "r");
	if (source == NULL) {
		plt_err("Failed to read default dump file: %s, err %d", DEFAULT_DUMP_FILE_NAME,
			errno);
		return errno;
	}

	fseek(source, 0L, SEEK_END);
	pos = ftell(source);
	fseek(source, 0L, SEEK_SET);
	while (pos--) {
		ch = fgetc(source);
		fputc(ch, target);
	}

	fclose(source);

	/* Remove the default file after reading */
	remove(DEFAULT_DUMP_FILE_NAME);

	return 0;
}

static int
process_flow_dump(struct cnxk_rep_dev *rep_dev, struct rte_flow *flow, FILE *file,
		  cnxk_rep_msg_ack_data_t *adata)
{
	cnxk_rep_msg_flow_dump_meta_t msg_fp_meta;
	uint32_t len = 0, rc;
	void *buffer;
	size_t size;

	size = MAX_BUFFER_SIZE;
	buffer = plt_zmalloc(size, 0);
	if (!buffer) {
		plt_err("Failed to allocate mem");
		rc = -ENOMEM;
		goto fail;
	}

	cnxk_rep_msg_populate_header(buffer, &len);

	msg_fp_meta.portid = rep_dev->rep_id;
	msg_fp_meta.flow = (uint64_t)flow;
	msg_fp_meta.is_stdout = (file == stdout) ? 1 : 0;

	plt_rep_dbg("Flow Dump: flow 0x%" PRIu64 ", portid %d stdout %d", msg_fp_meta.flow,
		    msg_fp_meta.portid, msg_fp_meta.is_stdout);
	cnxk_rep_msg_populate_command_meta(buffer, &len, &msg_fp_meta,
					   sizeof(cnxk_rep_msg_flow_dump_meta_t),
					   CNXK_REP_MSG_FLOW_DUMP);
	cnxk_rep_msg_populate_msg_end(buffer, &len);

	rc = cnxk_rep_msg_send_process(rep_dev, buffer, len, adata);
	if (rc) {
		plt_err("Failed to process the message, err %d", rc);
		goto fail;
	}

	/* Copy contents from default file to user file */
	if (file != stdout)
		copy_flow_dump_file(file);

	return 0;
fail:
	return rc;
}

static int
process_flow_flush(struct cnxk_rep_dev *rep_dev, cnxk_rep_msg_ack_data_t *adata)
{
	cnxk_rep_msg_flow_flush_meta_t msg_ff_meta;
	uint32_t len = 0, rc;
	void *buffer;
	size_t size;

	size = MAX_BUFFER_SIZE;
	buffer = plt_zmalloc(size, 0);
	if (!buffer) {
		plt_err("Failed to allocate mem");
		rc = -ENOMEM;
		goto fail;
	}

	cnxk_rep_msg_populate_header(buffer, &len);

	msg_ff_meta.portid = rep_dev->rep_id;
	plt_rep_dbg("Flow Flush: portid %d", msg_ff_meta.portid);
	cnxk_rep_msg_populate_command_meta(buffer, &len, &msg_ff_meta,
					   sizeof(cnxk_rep_msg_flow_flush_meta_t),
					   CNXK_REP_MSG_FLOW_FLUSH);
	cnxk_rep_msg_populate_msg_end(buffer, &len);

	rc = cnxk_rep_msg_send_process(rep_dev, buffer, len, adata);
	if (rc) {
		plt_err("Failed to process the message, err %d", rc);
		goto fail;
	}

	return 0;
fail:
	return rc;
}

static int
process_flow_query(struct cnxk_rep_dev *rep_dev, struct rte_flow *flow,
		   const struct rte_flow_action *action, void *data, cnxk_rep_msg_ack_data_t *adata)
{
	cnxk_rep_msg_flow_query_meta_t *msg_fq_meta;
	struct rte_flow_query_count *query = data;
	uint32_t len = 0, rc, sz, total_sz;
	uint64_t action_data[BUFSIZ];
	void *buffer;
	size_t size;

	size = MAX_BUFFER_SIZE;
	buffer = plt_zmalloc(size, 0);
	if (!buffer) {
		plt_err("Failed to allocate mem");
		rc = -ENOMEM;
		goto fail;
	}

	cnxk_rep_msg_populate_header(buffer, &len);

	memset(action_data, 0, BUFSIZ * sizeof(uint64_t));
	sz = prepare_action_data(action, 1, action_data);
	total_sz = sz + sizeof(cnxk_rep_msg_flow_query_meta_t);

	msg_fq_meta = plt_zmalloc(total_sz, 0);
	if (!msg_fq_meta) {
		plt_err("Failed to allocate memory");
		rc = -ENOMEM;
		goto fail;
	}

	msg_fq_meta->portid = rep_dev->rep_id;
	msg_fq_meta->reset = query->reset;
	;
	msg_fq_meta->flow = (uint64_t)flow;
	/* Populate the action data */
	rte_memcpy(msg_fq_meta->action_data, action_data, sz);
	msg_fq_meta->action_data_sz = sz;

	plt_rep_dbg("Flow query: flow 0x%" PRIu64 ", portid %d, action type %d total sz %d "
		    "action sz %d", msg_fq_meta->flow, msg_fq_meta->portid, action->type, total_sz,
		    sz);
	cnxk_rep_msg_populate_command_meta(buffer, &len, msg_fq_meta, total_sz,
					   CNXK_REP_MSG_FLOW_QUERY);
	cnxk_rep_msg_populate_msg_end(buffer, &len);

	rc = cnxk_rep_msg_send_process(rep_dev, buffer, len, adata);
	if (rc) {
		plt_err("Failed to process the message, err %d", rc);
		goto free;
	}

	rte_free(msg_fq_meta);

	return 0;

free:
	rte_free(msg_fq_meta);
fail:
	return rc;
}

static int
process_flow_rule(struct cnxk_rep_dev *rep_dev, const struct rte_flow_attr *attr,
		  const struct rte_flow_item pattern[], const struct rte_flow_action actions[],
		  cnxk_rep_msg_ack_data_t *adata, cnxk_rep_msg_t msg)
{
	cnxk_rep_msg_flow_create_meta_t msg_fc_meta;
	uint16_t n_pattern, n_action;
	uint32_t len = 0, rc = 0;
	void *buffer;
	size_t size;

	size = MAX_BUFFER_SIZE;
	buffer = plt_zmalloc(size, 0);
	if (!buffer) {
		plt_err("Failed to allocate mem");
		rc = -ENOMEM;
		goto fail;
	}

	/* Get no of actions and patterns */
	cnxk_flow_params_count(pattern, actions, &n_pattern, &n_action);

	/* Adding the header */
	cnxk_rep_msg_populate_header(buffer, &len);

	/* Representor port identified as rep_xport queue */
	msg_fc_meta.portid = rep_dev->rep_id;
	msg_fc_meta.nb_pattern = n_pattern;
	msg_fc_meta.nb_action = n_action;

	cnxk_rep_msg_populate_command_meta(buffer, &len, &msg_fc_meta,
					   sizeof(cnxk_rep_msg_flow_create_meta_t), msg);

	/* Populate flow create parameters data */
	populate_attr_data(buffer, &len, attr);
	populate_pattern_data(buffer, &len, pattern, n_pattern);
	populate_action_data(buffer, &len, actions, n_action);

	cnxk_rep_msg_populate_msg_end(buffer, &len);

	rc = cnxk_rep_msg_send_process(rep_dev, buffer, len, adata);
	if (rc) {
		plt_err("Failed to process the message, err %d", rc);
		goto fail;
	}

	return 0;
fail:
	return rc;
}

static struct rte_flow *
cnxk_rep_flow_create_native(struct rte_eth_dev *eth_dev, const struct rte_flow_attr *attr,
			    const struct rte_flow_item pattern[],
			    const struct rte_flow_action actions[], struct rte_flow_error *error)
{
	struct cnxk_rep_dev *rep_dev = cnxk_rep_pmd_priv(eth_dev);
	struct roc_npc_flow *flow;
	uint16_t new_entry;
	int rc;

	flow = cnxk_flow_create_common(eth_dev, attr, pattern, actions, error, true);
	if (!flow) {
		plt_err("Fail to create flow");
		goto fail;
	}

	/* Shifting the rules with higher priority than exception path rules */
	new_entry = (uint16_t)flow->mcam_id;
	rc = cnxk_eswitch_flow_rule_shift(rep_dev->hw_func, &new_entry);
	if (rc) {
		plt_err("Failed to shift the flow rule entry, err %d", rc);
		goto fail;
	}

	flow->mcam_id = new_entry;

	return (struct rte_flow *)flow;
fail:
	return NULL;
}

static struct rte_flow *
cnxk_rep_flow_create(struct rte_eth_dev *eth_dev, const struct rte_flow_attr *attr,
		     const struct rte_flow_item pattern[], const struct rte_flow_action actions[],
		     struct rte_flow_error *error)
{
	struct cnxk_rep_dev *rep_dev = cnxk_rep_pmd_priv(eth_dev);
	struct rte_flow *flow = NULL;
	cnxk_rep_msg_ack_data_t adata;
	int rc = 0;

	/* If representor not representing any active VF, return 0 */
	if (!rep_dev->is_vf_active) {
		rte_flow_error_set(error, -EAGAIN, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Represented VF not active yet");
		return 0;
	}

	if (rep_dev->native_repte)
		return cnxk_rep_flow_create_native(eth_dev, attr, pattern, actions, error);

	rc = process_flow_rule(rep_dev, attr, pattern, actions, &adata, CNXK_REP_MSG_FLOW_CREATE);
	if (!rc || adata.u.sval < 0) {
		if (adata.u.sval < 0) {
			rc = (int)adata.u.sval;
			rte_flow_error_set(error, adata.u.sval, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					   NULL, "Failed to validate flow");
			goto fail;
		}

		flow = adata.u.data;
		if (!flow) {
			rte_flow_error_set(error, adata.u.sval, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					   NULL, "Failed to create flow");
			goto fail;
		}
	} else {
		rte_flow_error_set(error, rc, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Failed to create flow");
		goto fail;
	}
	plt_rep_dbg("Flow %p created successfully", adata.u.data);

	return flow;
fail:
	return NULL;
}

static int
cnxk_rep_flow_validate(struct rte_eth_dev *eth_dev, const struct rte_flow_attr *attr,
		       const struct rte_flow_item pattern[], const struct rte_flow_action actions[],
		       struct rte_flow_error *error)
{
	struct cnxk_rep_dev *rep_dev = cnxk_rep_pmd_priv(eth_dev);
	cnxk_rep_msg_ack_data_t adata;
	int rc = 0;

	/* If representor not representing any active VF, return 0 */
	if (!rep_dev->is_vf_active) {
		rte_flow_error_set(error, -EAGAIN, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Represented VF not active yet");
		return 0;
	}

	if (rep_dev->native_repte)
		return cnxk_flow_validate_common(eth_dev, attr, pattern, actions, error, true);

	rc = process_flow_rule(rep_dev, attr, pattern, actions, &adata, CNXK_REP_MSG_FLOW_VALIDATE);
	if (!rc || adata.u.sval < 0) {
		if (adata.u.sval < 0) {
			rc = (int)adata.u.sval;
			rte_flow_error_set(error, adata.u.sval, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					   NULL, "Failed to validate flow");
			goto fail;
		}
	} else {
		rte_flow_error_set(error, rc, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Failed to validate flow");
	}

	plt_rep_dbg("Flow %p validated successfully", adata.u.data);

fail:
	return rc;
}

static int
cnxk_rep_flow_destroy(struct rte_eth_dev *eth_dev, struct rte_flow *flow,
		      struct rte_flow_error *error)
{
	struct cnxk_rep_dev *rep_dev = cnxk_rep_pmd_priv(eth_dev);
	cnxk_rep_msg_ack_data_t adata;
	int rc;

	/* If representor not representing any active VF, return 0 */
	if (!rep_dev->is_vf_active) {
		rte_flow_error_set(error, -EAGAIN, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Represented VF not active yet");
		return 0;
	}

	if (rep_dev->native_repte)
		return cnxk_flow_destroy_common(eth_dev, (struct roc_npc_flow *)flow, error, true);

	rc = process_flow_destroy(rep_dev, flow, &adata);
	if (rc || adata.u.sval < 0) {
		if (adata.u.sval < 0)
			rc = adata.u.sval;

		rte_flow_error_set(error, rc, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Failed to destroy flow");
		goto fail;
	}

	return 0;
fail:
	return rc;
}

static int
cnxk_rep_flow_query(struct rte_eth_dev *eth_dev, struct rte_flow *flow,
		    const struct rte_flow_action *action, void *data, struct rte_flow_error *error)
{
	struct cnxk_rep_dev *rep_dev = cnxk_rep_pmd_priv(eth_dev);
	cnxk_rep_msg_ack_data_t adata;
	int rc;

	/* If representor not representing any active VF, return 0 */
	if (!rep_dev->is_vf_active) {
		rte_flow_error_set(error, -EAGAIN, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Represented VF not active yet");
		return 0;
	}

	if (action->type != RTE_FLOW_ACTION_TYPE_COUNT) {
		rc = -ENOTSUP;
		rte_flow_error_set(error, rc, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Only COUNT is supported in query");
		goto fail;
	}

	if (rep_dev->native_repte)
		return cnxk_flow_query_common(eth_dev, flow, action, data, error, true);

	rc = process_flow_query(rep_dev, flow, action, data, &adata);
	if (rc || adata.u.sval < 0) {
		if (adata.u.sval < 0)
			rc = adata.u.sval;

		rte_flow_error_set(error, rc, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Failed to query the flow");
		goto fail;
	}

	rte_memcpy(data, adata.u.data, adata.size);

	return 0;
fail:
	return rc;
}

static int
cnxk_rep_flow_flush(struct rte_eth_dev *eth_dev, struct rte_flow_error *error)
{
	struct cnxk_rep_dev *rep_dev = cnxk_rep_pmd_priv(eth_dev);
	cnxk_rep_msg_ack_data_t adata;
	int rc;

	/* If representor not representing any active VF, return 0 */
	if (!rep_dev->is_vf_active) {
		rte_flow_error_set(error, -EAGAIN, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Represented VF not active yet");
		return 0;
	}

	if (rep_dev->native_repte)
		return cnxk_flow_flush_common(eth_dev, error, true);

	rc = process_flow_flush(rep_dev, &adata);
	if (rc || adata.u.sval < 0) {
		if (adata.u.sval < 0)
			rc = adata.u.sval;

		rte_flow_error_set(error, rc, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Failed to destroy flow");
		goto fail;
	}

	return 0;
fail:
	return rc;
}

static int
cnxk_rep_flow_dev_dump(struct rte_eth_dev *eth_dev, struct rte_flow *flow, FILE *file,
		       struct rte_flow_error *error)
{
	struct cnxk_rep_dev *rep_dev = cnxk_rep_pmd_priv(eth_dev);
	cnxk_rep_msg_ack_data_t adata;
	int rc;

	/* If representor not representing any active VF, return 0 */
	if (!rep_dev->is_vf_active) {
		rte_flow_error_set(error, -EAGAIN, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Represented VF not active yet");
		return 0;
	}

	if (rep_dev->native_repte)
		return cnxk_flow_dev_dump_common(eth_dev, flow, file, error, true);

	rc = process_flow_dump(rep_dev, flow, file, &adata);
	if (rc || adata.u.sval < 0) {
		if (adata.u.sval < 0)
			rc = adata.u.sval;

		rte_flow_error_set(error, rc, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Failed to destroy flow");
		goto fail;
	}

	return 0;
fail:
	return rc;
}

static int
cnxk_rep_flow_isolate(struct rte_eth_dev *eth_dev __rte_unused, int enable __rte_unused,
		      struct rte_flow_error *error)
{
	/* If we support, we need to un-install the default mcam
	 * entry for this port.
	 */

	rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			   "Flow isolation not supported");

	return -rte_errno;
}

struct rte_flow_ops cnxk_rep_flow_ops = {
	.validate = cnxk_rep_flow_validate,
	.create = cnxk_rep_flow_create,
	.destroy = cnxk_rep_flow_destroy,
	.query = cnxk_rep_flow_query,
	.flush = cnxk_rep_flow_flush,
	.isolate = cnxk_rep_flow_isolate,
	.dev_dump = cnxk_rep_flow_dev_dump,
};
