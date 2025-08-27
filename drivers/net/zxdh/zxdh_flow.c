/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 ZTE Corporation
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>

#include <rte_debug.h>
#include <rte_ether.h>
#include <ethdev_driver.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_tailq.h>
#include <rte_flow.h>
#include <rte_bitmap.h>

#include "zxdh_ethdev.h"
#include "zxdh_logs.h"
#include "zxdh_flow.h"
#include "zxdh_tables.h"
#include "zxdh_ethdev_ops.h"
#include "zxdh_np.h"
#include "zxdh_msg.h"

#define ZXDH_IPV6_FRAG_HEADER    44
#define ZXDH_TENANT_ARRAY_NUM    3
#define ZXDH_VLAN_TCI_MASK       0xFFFF
#define ZXDH_VLAN_PRI_MASK       0xE000
#define ZXDH_VLAN_CFI_MASK       0x1000
#define ZXDH_VLAN_VID_MASK       0x0FFF
#define MAX_STRING_LEN           8192
#define FLOW_INGRESS             0
#define FLOW_EGRESS              1
#define MAX_ENCAP1_NUM           (256)
#define INVALID_HANDLEIDX        0xffff
#define ACTION_VXLAN_ENCAP_ITEMS_NUM (6)
static struct dh_engine_list flow_engine_list = TAILQ_HEAD_INITIALIZER(flow_engine_list);
static struct count_res flow_count_ref[MAX_FLOW_COUNT_NUM];
static rte_spinlock_t fd_hw_res_lock = RTE_SPINLOCK_INITIALIZER;
static uint8_t fd_hwres_bitmap[ZXDH_MAX_FLOW_NUM] = {0};

#define MKDUMPSTR(buf, buf_size, cur_len, ...) \
do { \
	if ((cur_len) >= (buf_size)) \
		break; \
	(cur_len) += snprintf((buf) + (cur_len), (buf_size) - (cur_len), __VA_ARGS__); \
} while (0)

static inline void
print_ether_addr(const char *what, const struct rte_ether_addr *eth_addr,
		 char print_buf[], uint32_t buf_size, uint32_t *cur_len)
{
	char buf[RTE_ETHER_ADDR_FMT_SIZE];

	rte_ether_format_addr(buf, RTE_ETHER_ADDR_FMT_SIZE, eth_addr);
	MKDUMPSTR(print_buf, buf_size, *cur_len, "%s%s", what, buf);
}

static inline void
zxdh_fd_flow_free_dtbentry(ZXDH_DTB_USER_ENTRY_T *dtb_entry)
{
	rte_free(dtb_entry->p_entry_data);
	dtb_entry->p_entry_data = NULL;
	dtb_entry->sdt_no = 0;
}

static void
data_bitwise(void *data, int bytecnt)
{
	int i;
	uint32_t *temp = (uint32_t *)data;
	int remain = bytecnt % 4;
	for (i = 0; i < (bytecnt >> 2); i++)	{
		*(temp) = ~*(temp);
		temp++;
	}

	if (remain) {
		for (i = 0; i < remain; i++) {
			uint8_t *tmp = (uint8_t *)temp;
			*(uint8_t *)tmp = ~*(uint8_t *)tmp;
			tmp++;
		}
	}
}

static void
zxdh_adjust_flow_op_rsp_memory_layout(void *old_data,
		size_t old_size, void *new_data)
{
	rte_memcpy(new_data, old_data, sizeof(struct zxdh_flow));
	memset((char *)new_data + sizeof(struct zxdh_flow), 0, 4);
	rte_memcpy((char *)new_data + sizeof(struct zxdh_flow) + 4,
		(char *)old_data + sizeof(struct zxdh_flow),
		old_size - sizeof(struct zxdh_flow));
}

void zxdh_flow_global_init(void)
{
	int i;
	for (i = 0; i < MAX_FLOW_COUNT_NUM; i++) {
		rte_spinlock_init(&flow_count_ref[i].count_lock);
		flow_count_ref[i].count_ref = 0;
	}
}

static void
__entry_dump(char *print_buf, uint32_t buf_size,
		uint32_t *cur_len, struct fd_flow_key *key)
{
	print_ether_addr("\nL2\t  dst=", &key->mac_dst, print_buf, buf_size, cur_len);
	print_ether_addr(" - src=", &key->mac_src, print_buf, buf_size, cur_len);
	MKDUMPSTR(print_buf, buf_size, *cur_len, " -eth type=0x%04x", key->ether_type);
	MKDUMPSTR(print_buf, buf_size, *cur_len,
		" -vlan_pri=0x%02x -vlan_vlanid=0x%04x  -vlan_tci=0x%04x ",
		key->cvlan_pri, key->cvlan_vlanid, key->vlan_tci);
	MKDUMPSTR(print_buf, buf_size, *cur_len,
		" -vni=0x%02x 0x%02x 0x%02x\n", key->vni[0], key->vni[1], key->vni[2]);
	MKDUMPSTR(print_buf, buf_size, *cur_len,
		"L3\t  dstip=0x%08x 0x%08x 0x%08x 0x%08x("IPv6_BYTES_FMT")\n",
		*(uint32_t *)key->dst_ip, *((uint32_t *)key->dst_ip + 1),
		*((uint32_t *)key->dst_ip + 2),
		*((uint32_t *)key->dst_ip + 3),
		IPv6_BYTES(key->dst_ip));
	MKDUMPSTR(print_buf, buf_size, *cur_len,
		"\t  srcip=0x%08x 0x%08x 0x%08x 0x%08x("IPv6_BYTES_FMT")\n",
		*((uint32_t *)key->src_ip), *((uint32_t *)key->src_ip + 1),
		*((uint32_t *)key->src_ip + 2),
		*((uint32_t *)key->src_ip + 3),
		IPv6_BYTES(key->src_ip));
	MKDUMPSTR(print_buf, buf_size, *cur_len,
		"  \t  tos=0x%02x -nw-proto=0x%02x -frag-flag %u\n",
		key->tos, key->nw_proto, key->frag_flag);
	MKDUMPSTR(print_buf, buf_size, *cur_len,
		"L4\t  dstport=0x%04x -srcport=0x%04x", key->tp_dst, key->tp_src);
}

static void
__result_dump(char *print_buf, uint32_t buf_size,
		uint32_t *cur_len, struct fd_flow_result *res)
{
	MKDUMPSTR(print_buf, buf_size, *cur_len, " -hit_flag = 0x%04x", res->hit_flag);
	MKDUMPSTR(print_buf, buf_size, *cur_len, " -action_idx = 0x%02x", res->action_idx);
	MKDUMPSTR(print_buf, buf_size, *cur_len, " -qid = 0x%04x", res->qid);
	MKDUMPSTR(print_buf, buf_size, *cur_len, " -mark_id = 0x%08x", res->mark_fd_id);
	MKDUMPSTR(print_buf, buf_size, *cur_len, " -count_id = 0x%02x", res->countid);
}

static void offlow_key_dump(struct fd_flow_key *key, struct fd_flow_key *key_mask, FILE *file)
{
	char print_buf[MAX_STRING_LEN];
	uint32_t buf_size = MAX_STRING_LEN;
	uint32_t cur_len = 0;

	MKDUMPSTR(print_buf, buf_size, cur_len, "offload key:\n\t");
	__entry_dump(print_buf, buf_size, &cur_len, key);

	MKDUMPSTR(print_buf, buf_size, cur_len, "\noffload key_mask:\n\t");
	__entry_dump(print_buf, buf_size, &cur_len, key_mask);

	PMD_DRV_LOG(INFO, "%s", print_buf);
	MKDUMPSTR(print_buf, buf_size, cur_len, "\n");
	if (file)
		fputs(print_buf, file);
}

static void offlow_result_dump(struct fd_flow_result *res, FILE *file)
{
	char print_buf[MAX_STRING_LEN];
	uint32_t buf_size = MAX_STRING_LEN;
	uint32_t cur_len = 0;

	MKDUMPSTR(print_buf, buf_size, cur_len, "offload result:\n");
	__result_dump(print_buf, buf_size, &cur_len, res);
	PMD_DRV_LOG(INFO, "%s", print_buf);
	PMD_DRV_LOG(INFO, "memdump : ===result ===");
	MKDUMPSTR(print_buf, buf_size, cur_len, "\n");
	if (file)
		fputs(print_buf, file);
}

static int
set_flow_enable(struct rte_eth_dev *dev, uint8_t dir,
		bool enable, struct rte_flow_error *error)
{
	struct zxdh_hw *priv = dev->data->dev_private;
	struct zxdh_port_attr_table port_attr = {0};
	int ret = 0;

	if (priv->is_pf) {
		ret = zxdh_get_port_attr(priv, priv->vport.vport, &port_attr);
		if (ret)
			return -rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					"get port attr failed.");
		port_attr.fd_enable = enable;

		ret = zxdh_set_port_attr(priv, priv->vport.vport, &port_attr);
		if (ret)
			return -rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					"set port attr fd_enable failed.");
	} else {
		struct zxdh_msg_info msg_info = {0};
		struct zxdh_port_attr_set_msg *attr_msg = &msg_info.data.port_attr_msg;

		attr_msg->mode  = ZXDH_PORT_FD_EN_OFF_FLAG;
		attr_msg->value = enable;
		zxdh_msg_head_build(priv, ZXDH_PORT_ATTRS_SET, &msg_info);
		ret = zxdh_vf_send_msg_to_pf(dev, &msg_info, sizeof(msg_info), NULL, 0);
		if (ret) {
			PMD_DRV_LOG(ERR, "port %d flow enable failed", priv->port_id);
			return -rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
						"flow enable failed.");
		}
	}
	if (dir == FLOW_INGRESS)
		priv->i_flow_en = !!enable;
	else
		priv->e_flow_en = !!enable;

	return ret;
}

static int
set_vxlan_enable(struct rte_eth_dev *dev, bool enable, struct rte_flow_error *error)
{
	struct zxdh_hw *priv = dev->data->dev_private;
	struct zxdh_port_attr_table port_attr = {0};
	int ret = 0;

	if (priv->vxlan_flow_en == !!enable)
		return 0;
	if (priv->is_pf) {
		ret = zxdh_get_port_attr(priv, priv->vport.vport, &port_attr);
		if (ret)
			return -rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					"get port attr failed.");
		port_attr.fd_vxlan_offload_en = enable;

		ret = zxdh_set_port_attr(priv, priv->vport.vport, &port_attr);
		if (ret)
			return -rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					"set port attr fd_enable failed.");
	} else {
		struct zxdh_msg_info msg_info = {0};
		struct zxdh_port_attr_set_msg *attr_msg = &msg_info.data.port_attr_msg;

		attr_msg->mode  = ZXDH_PORT_VXLAN_OFFLOAD_EN_OFF;
		attr_msg->value = enable;

		zxdh_msg_head_build(priv, ZXDH_PORT_ATTRS_SET, &msg_info);
		ret = zxdh_vf_send_msg_to_pf(dev, &msg_info, sizeof(msg_info), NULL, 0);
		if (ret) {
			PMD_DRV_LOG(ERR, "port %d vxlan flow enable failed", priv->port_id);
			return -rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
						"vxlan offload enable failed.");
		}
	}

	priv->vxlan_flow_en = !!enable;
	return ret;
}

void zxdh_register_flow_engine(struct dh_flow_engine *engine)
{
	TAILQ_INSERT_TAIL(&flow_engine_list, engine, node);
}

static void zxdh_flow_free(struct zxdh_flow *dh_flow)
{
	if (dh_flow)
		rte_mempool_put(zxdh_shared_data->flow_mp, dh_flow);
}

static struct dh_flow_engine *zxdh_get_flow_engine(struct rte_eth_dev *dev __rte_unused)
{
	struct dh_flow_engine *engine = NULL;
	void *temp;

	RTE_TAILQ_FOREACH_SAFE(engine, &flow_engine_list, node, temp) {
		if (engine->type  == FLOW_TYPE_FD_TCAM)
			break;
	}
	return engine;
}

static int
zxdh_flow_validate(struct rte_eth_dev *dev,
			 const struct rte_flow_attr *attr,
			 const struct rte_flow_item  *pattern,
			 const struct rte_flow_action *actions,
			 struct rte_flow_error *error)
{
	struct dh_flow_engine *flow_engine = NULL;

	if (!pattern) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM_NUM,
				   NULL, "NULL pattern.");
		return -rte_errno;
	}

	if (!actions) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION_NUM,
				   NULL, "NULL action.");
		return -rte_errno;
	}

	if (!attr) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR,
				   NULL, "NULL attribute.");
		return -rte_errno;
	}
	flow_engine = zxdh_get_flow_engine(dev);
	if (flow_engine == NULL || flow_engine->parse_pattern_action == NULL) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, "cannot find valid flow engine.");
		return -rte_errno;
	}
	if (flow_engine->parse_pattern_action(dev, attr, pattern, actions, error, NULL) != 0)
		return -rte_errno;
	return 0;
}

static struct zxdh_flow *flow_exist_check(struct rte_eth_dev *dev, struct zxdh_flow *dh_flow)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct rte_flow *entry;
	struct zxdh_flow *entry_flow;

	TAILQ_FOREACH(entry, &hw->dh_flow_list, next) {
		entry_flow = (struct zxdh_flow *)entry->driver_flow;
		if ((memcmp(&entry_flow->flowentry.fd_flow.key, &dh_flow->flowentry.fd_flow.key,
				 sizeof(struct fd_flow_key)) == 0)  &&
			(memcmp(&entry_flow->flowentry.fd_flow.key_mask,
				&dh_flow->flowentry.fd_flow.key_mask,
				 sizeof(struct fd_flow_key)) == 0)) {
			return entry_flow;
		}
	}
	return NULL;
}

static struct rte_flow *
zxdh_flow_create(struct rte_eth_dev *dev,
		 const struct rte_flow_attr *attr,
		 const struct rte_flow_item pattern[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct rte_flow *flow = NULL;
	struct zxdh_flow *dh_flow = NULL;
	int ret = 0;
	struct dh_flow_engine *flow_engine = NULL;

	flow_engine = zxdh_get_flow_engine(dev);

	if (flow_engine == NULL ||
		flow_engine->parse_pattern_action == NULL ||
		flow_engine->apply == NULL) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, "cannot find valid flow engine.");
		return NULL;
	}

	flow = rte_zmalloc("rte_flow", sizeof(struct rte_flow), 0);
	if (!flow) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL, "flow malloc failed");
		return NULL;
	}
	ret = rte_mempool_get(zxdh_shared_data->flow_mp, (void **)&dh_flow);
	if (ret) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					"Failed to allocate memory from flowmp");
		goto free_flow;
	}
	memset(dh_flow, 0, sizeof(struct zxdh_flow));
	if (flow_engine->parse_pattern_action(dev, attr, pattern, actions, error, dh_flow) != 0) {
		PMD_DRV_LOG(ERR, "parse_pattern_action failed zxdh_created failed");
		goto free_flow;
	}

	if (flow_exist_check(dev, dh_flow) != NULL) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					"duplicate entry: will not add");
		goto free_flow;
	}

	ret = flow_engine->apply(dev, dh_flow, error, hw->vport.vport, hw->pcie_id);
	if (ret) {
		PMD_DRV_LOG(ERR, "flow creation failed: failed to apply");
		goto free_flow;
	}

	if (hw->i_flow_en == 0) {
		ret = set_flow_enable(dev, FLOW_INGRESS, 1, error);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "set flow enable failed");
			goto free_flow;
		}
	}

	flow->driver_flow = dh_flow;
	flow->port_id = dev->data->port_id;
	flow->type = ZXDH_FLOW_GROUP_TCAM;
	TAILQ_INSERT_TAIL(&hw->dh_flow_list, flow, next);

	return flow;
free_flow:
	zxdh_flow_free(dh_flow);
	rte_free(flow);
	return NULL;
}

static int
zxdh_flow_destroy(struct rte_eth_dev *dev,
		  struct rte_flow *flow,
		  struct rte_flow_error *error)
{
	struct zxdh_hw *priv = dev->data->dev_private;
	struct zxdh_flow *dh_flow = NULL;
	int ret = 0;
	struct dh_flow_engine *flow_engine = NULL;

	flow_engine = zxdh_get_flow_engine(dev);
	if (flow_engine == NULL ||
		flow_engine->destroy == NULL) {
		rte_flow_error_set(error, EINVAL,
				 RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				 NULL, "cannot find valid flow engine.");
		return -rte_errno;
	}
	if (flow->driver_flow)
		dh_flow = (struct zxdh_flow *)flow->driver_flow;

	if (dh_flow == NULL) {
		PMD_DRV_LOG(ERR, "invalid flow");
		rte_flow_error_set(error, EINVAL,
				 RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				 NULL, "invalid flow");
		return -1;
	}
	ret = flow_engine->destroy(dev, dh_flow, error, priv->vport.vport, priv->pcie_id);
	if (ret) {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to destroy flow.");
		return -rte_errno;
	}
	TAILQ_REMOVE(&priv->dh_flow_list, flow, next);
	zxdh_flow_free(dh_flow);
	rte_free(flow);

	if (TAILQ_EMPTY(&priv->dh_flow_list)) {
		ret = set_flow_enable(dev, FLOW_INGRESS, 0, error);
		if (ret) {
			PMD_DRV_LOG(ERR, "clear flow enable failed");
			return -rte_errno;
		}
	}
	return ret;
}


static int
zxdh_flow_query(struct rte_eth_dev *dev,
		struct rte_flow *flow,
		const struct rte_flow_action *actions,
		void *data, struct rte_flow_error *error)
{
	struct zxdh_flow *dh_flow = NULL;
	struct dh_flow_engine *flow_engine = NULL;
	int ret = 0;

	flow_engine = zxdh_get_flow_engine(dev);

	if (flow_engine == NULL ||
		flow_engine->query_count == NULL) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, "cannot find valid flow engine.");
		return -rte_errno;
	}

	if (flow->driver_flow) {
		dh_flow = (struct zxdh_flow *)flow->driver_flow;
		if (dh_flow == NULL) {
			PMD_DRV_LOG(ERR, "flow does not exist");
			return -1;
		}
	}

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			ret = flow_engine->query_count(dev, dh_flow,
						 (struct rte_flow_query_count *)data, error);
			break;
		default:
			ret = rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION,
					actions,
					"action does not support QUERY");
			goto out;
		}
	}
out:
	if (ret)
		PMD_DRV_LOG(ERR, "flow query failed");
	return ret;
}

static int zxdh_flow_flush(struct rte_eth_dev *dev, struct rte_flow_error *error)
{
	struct rte_flow *flow;
	struct zxdh_flow *dh_flow = NULL;
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_dtb_shared_data *dtb_data = &hw->dev_sd->dtb_sd;
	struct dh_flow_engine *flow_engine = NULL;
	struct zxdh_msg_info msg_info = {0};
	uint8_t zxdh_msg_reply_info[ZXDH_ST_SZ_BYTES(msg_reply_info)] = {0};
	int ret = 0;

	flow_engine = zxdh_get_flow_engine(dev);
	if (flow_engine == NULL) {
		PMD_DRV_LOG(ERR, "get flow engine failed");
		return -1;
	}
	ret = set_flow_enable(dev, FLOW_INGRESS, 0, error);
	if (ret) {
		PMD_DRV_LOG(ERR, "clear flow enable failed");
		return ret;
	}

	ret = set_vxlan_enable(dev, 0, error);
	if (ret)
		PMD_DRV_LOG(ERR, "clear vxlan enable failed");
	hw->vxlan_fd_num = 0;

	if (hw->is_pf) {
		ret = zxdh_np_dtb_acl_offline_delete(hw->dev_id, dtb_data->queueid,
					ZXDH_SDT_FD_TABLE, hw->vport.vport,
					ZXDH_FLOW_STATS_INGRESS_BASE, 1);
		if (ret)
			PMD_DRV_LOG(ERR, "%s flush failed. code:%d", dev->data->name, ret);
	} else {
		zxdh_msg_head_build(hw, ZXDH_FLOW_HW_FLUSH, &msg_info);
		ret = zxdh_vf_send_msg_to_pf(dev, &msg_info, sizeof(struct zxdh_msg_info),
			(void *)zxdh_msg_reply_info, ZXDH_ST_SZ_BYTES(msg_reply_info));
		if (ret) {
			PMD_DRV_LOG(ERR, "port %d flow op %d flush failed ret %d",
				hw->port_id, ZXDH_FLOW_HW_FLUSH, ret);
			return -1;
		}
	}

	/* Remove all flows */
	while ((flow = TAILQ_FIRST(&hw->dh_flow_list))) {
		TAILQ_REMOVE(&hw->dh_flow_list, flow, next);
		if (flow->driver_flow)
			dh_flow = (struct zxdh_flow *)flow->driver_flow;
		if (dh_flow == NULL) {
			PMD_DRV_LOG(ERR, "Invalid flow Failed to destroy flow.");
			ret = rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_HANDLE,
					NULL,
					"Invalid flow, flush failed");
			return ret;
		}

		zxdh_flow_free(dh_flow);
		rte_free(flow);
	}
	return ret;
}

static void
handle_res_dump(struct rte_eth_dev *dev)
{
	struct zxdh_hw *priv =  dev->data->dev_private;
	uint16_t hwres_base = priv->vport.pfid << 10;
	uint16_t hwres_cnt = ZXDH_MAX_FLOW_NUM >> 1;
	uint16_t i;

	PMD_DRV_LOG(DEBUG, "hwres_base %d", hwres_base);
	rte_spinlock_lock(&fd_hw_res_lock);
	for (i = 0; i < hwres_cnt; i++) {
		if (fd_hwres_bitmap[hwres_base + i] == 1)
			PMD_DRV_LOG(DEBUG, "used idx %d", i + hwres_base);
	}
	rte_spinlock_unlock(&fd_hw_res_lock);
}

static int
zxdh_flow_dev_dump(struct rte_eth_dev *dev,
			struct rte_flow *flow,
			FILE *file,
			struct rte_flow_error *error __rte_unused)
{
	struct zxdh_hw *hw =  dev->data->dev_private;
	struct rte_flow *entry;
	struct zxdh_flow *entry_flow;
	uint32_t dtb_qid = 0;
	uint32_t entry_num = 0;
	uint16_t ret = 0;
	ZXDH_DTB_ACL_ENTRY_INFO_T *fd_entry = NULL;
	uint8_t *key = NULL;
	uint8_t *key_mask = NULL;
	uint8_t *result = NULL;

	if (flow) {
		entry_flow = flow_exist_check(dev, (struct zxdh_flow *)flow->driver_flow);
		if (entry_flow) {
			PMD_DRV_LOG(DEBUG, "handle idx %d:", entry_flow->flowentry.hw_idx);
			offlow_key_dump(&entry_flow->flowentry.fd_flow.key,
				&entry_flow->flowentry.fd_flow.key_mask, file);
			offlow_result_dump(&entry_flow->flowentry.fd_flow.result, file);
		}
	} else {
		if (hw->is_pf) {
			dtb_qid = hw->dev_sd->dtb_sd.queueid;
			fd_entry = calloc(1, sizeof(ZXDH_DTB_ACL_ENTRY_INFO_T) * ZXDH_MAX_FLOW_NUM);
			key = calloc(1, sizeof(struct fd_flow_key) * ZXDH_MAX_FLOW_NUM);
			key_mask = calloc(1, sizeof(struct fd_flow_key) * ZXDH_MAX_FLOW_NUM);
			result = calloc(1, sizeof(struct fd_flow_result) * ZXDH_MAX_FLOW_NUM);
			if (!fd_entry || !key || !key_mask || !result) {
				PMD_DRV_LOG(ERR, "fd_entry malloc failed!");
				goto end;
			}

			for (int i = 0; i < ZXDH_MAX_FLOW_NUM; i++) {
				fd_entry[i].key_data = key + i * sizeof(struct fd_flow_key);
				fd_entry[i].key_mask = key_mask + i * sizeof(struct fd_flow_key);
				fd_entry[i].p_as_rslt = result + i * sizeof(struct fd_flow_result);
			}
			ret = zxdh_np_dtb_acl_table_dump_by_vport(hw->dev_id, dtb_qid,
						ZXDH_SDT_FD_TABLE, hw->vport.vport, &entry_num,
						(uint8_t *)fd_entry);
			if (ret) {
				PMD_DRV_LOG(ERR, "dpp_dtb_acl_table_dump_by_vport failed!");
				goto end;
			}
			for (uint32_t i = 0; i < entry_num; i++) {
				offlow_key_dump((struct fd_flow_key *)fd_entry[i].key_data,
					(struct fd_flow_key *)fd_entry[i].key_mask, file);
				offlow_result_dump((struct fd_flow_result *)fd_entry[i].p_as_rslt,
						file);
			}
			free(result);
			free(key_mask);
			free(key);
			free(fd_entry);
		} else {
			entry = calloc(1, sizeof(struct rte_flow));
			entry_flow = calloc(1, sizeof(struct zxdh_flow));
			TAILQ_FOREACH(entry, &hw->dh_flow_list, next) {
				entry_flow = (struct zxdh_flow *)entry->driver_flow;
				offlow_key_dump(&entry_flow->flowentry.fd_flow.key,
						&entry_flow->flowentry.fd_flow.key_mask, file);
				offlow_result_dump(&entry_flow->flowentry.fd_flow.result, file);
			}
			free(entry_flow);
			free(entry);
		}
	}
	handle_res_dump(dev);

	return 0;
end:
	free(result);
	free(key_mask);
	free(key);
	free(fd_entry);
	return -1;
}

static int32_t
get_available_handle(struct zxdh_hw *hw, uint16_t vport)
{
	int ret = 0;
	uint32_t handle_idx = 0;

	ret = zxdh_np_dtb_acl_index_request(hw->dev_id, ZXDH_SDT_FD_TABLE, vport, &handle_idx);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory for hw!");
		return INVALID_HANDLEIDX;
	}
	return handle_idx;
}

static int free_handle(struct zxdh_hw *hw, uint16_t handle_idx, uint16_t vport)
{
	int ret = zxdh_np_dtb_acl_index_release(hw->dev_id, ZXDH_SDT_FD_TABLE, vport, handle_idx);

	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to free handle_idx %d for hw!", handle_idx);
		return -1;
	}
	return 0;
}

static uint16_t
zxdh_encap0_to_dtbentry(struct zxdh_hw *hw __rte_unused,
			struct zxdh_flow *dh_flow,
			ZXDH_DTB_USER_ENTRY_T *dtb_entry)
{
	ZXDH_DTB_ERAM_ENTRY_INFO_T *dtb_eram_entry;
	dtb_eram_entry = rte_zmalloc(NULL, sizeof(ZXDH_DTB_ERAM_ENTRY_INFO_T), 0);

	if (dtb_eram_entry == NULL)
		return INVALID_HANDLEIDX;

	dtb_eram_entry->index = dh_flow->flowentry.fd_flow.result.encap0_index * 2;
	dtb_eram_entry->p_data = (uint32_t *)&dh_flow->encap0;

	dtb_entry->sdt_no = ZXDH_SDT_TUNNEL_ENCAP0_TABLE;
	dtb_entry->p_entry_data = dtb_eram_entry;
	return 0;
}

static uint16_t
zxdh_encap0_ip_to_dtbentry(struct zxdh_hw *hw __rte_unused,
			struct zxdh_flow *dh_flow,
			ZXDH_DTB_USER_ENTRY_T *dtb_entry)
{
	ZXDH_DTB_ERAM_ENTRY_INFO_T *dtb_eram_entry;
	dtb_eram_entry = rte_zmalloc(NULL, sizeof(ZXDH_DTB_ERAM_ENTRY_INFO_T), 0);

	if (dtb_eram_entry == NULL)
		return INVALID_HANDLEIDX;

	dtb_eram_entry->index = dh_flow->flowentry.fd_flow.result.encap0_index * 2 + 1;
	dtb_eram_entry->p_data = (uint32_t *)&dh_flow->encap0.dip;
	dtb_entry->sdt_no = ZXDH_SDT_TUNNEL_ENCAP0_TABLE;
	dtb_entry->p_entry_data = dtb_eram_entry;
	return 0;
}

static uint16_t zxdh_encap1_to_dtbentry(struct zxdh_hw *hw __rte_unused,
							 struct zxdh_flow *dh_flow,
							 ZXDH_DTB_USER_ENTRY_T *dtb_entry)
{
	ZXDH_DTB_ERAM_ENTRY_INFO_T *dtb_eram_entry;
	dtb_eram_entry = rte_zmalloc(NULL, sizeof(ZXDH_DTB_ERAM_ENTRY_INFO_T), 0);

	if (dtb_eram_entry == NULL)
		return INVALID_HANDLEIDX;

	if (dh_flow->encap0.ethtype == 0)
		dtb_eram_entry->index = dh_flow->flowentry.fd_flow.result.encap1_index * 4;
	else
		dtb_eram_entry->index = dh_flow->flowentry.fd_flow.result.encap1_index * 4 + 1;

	dtb_eram_entry->p_data = (uint32_t *)&dh_flow->encap1;

	dtb_entry->sdt_no = ZXDH_SDT_TUNNEL_ENCAP1_TABLE;
	dtb_entry->p_entry_data = dtb_eram_entry;
	return 0;
}

static uint16_t
zxdh_encap1_ip_to_dtbentry(struct zxdh_hw *hw __rte_unused,
			struct zxdh_flow *dh_flow,
			ZXDH_DTB_USER_ENTRY_T *dtb_entry)
{
	ZXDH_DTB_ERAM_ENTRY_INFO_T *dtb_eram_entry;
	dtb_eram_entry = rte_zmalloc(NULL, sizeof(ZXDH_DTB_ERAM_ENTRY_INFO_T), 0);

	if (dtb_eram_entry == NULL)
		return INVALID_HANDLEIDX;
	if (dh_flow->encap0.ethtype == 0)
		dtb_eram_entry->index = dh_flow->flowentry.fd_flow.result.encap1_index * 4 + 2;
	else
		dtb_eram_entry->index = dh_flow->flowentry.fd_flow.result.encap1_index * 4 + 3;
	dtb_eram_entry->p_data = (uint32_t *)&dh_flow->encap1.sip;
	dtb_entry->sdt_no = ZXDH_SDT_TUNNEL_ENCAP1_TABLE;
	dtb_entry->p_entry_data = dtb_eram_entry;
	return 0;
}

static int zxdh_hw_encap_insert(struct rte_eth_dev *dev,
					struct zxdh_flow *dh_flow,
					struct rte_flow_error *error)
{
	uint32_t ret;
	struct zxdh_hw *hw = dev->data->dev_private;
	uint32_t dtb_qid = hw->dev_sd->dtb_sd.queueid;
	ZXDH_DTB_USER_ENTRY_T dtb_entry = {0};

	zxdh_encap0_to_dtbentry(hw, dh_flow, &dtb_entry);
	ret = zxdh_np_dtb_table_entry_write(hw->dev_id, dtb_qid, 1, &dtb_entry);
	zxdh_fd_flow_free_dtbentry(&dtb_entry);
	if (ret) {
		rte_flow_error_set(error, EINVAL,
							RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
							"write to hw failed");
		return -1;
	}

	zxdh_encap0_ip_to_dtbentry(hw, dh_flow, &dtb_entry);
	ret = zxdh_np_dtb_table_entry_write(hw->dev_id, dtb_qid, 1, &dtb_entry);
	zxdh_fd_flow_free_dtbentry(&dtb_entry);
	if (ret) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"write to hw failed");
		return -1;
	}

	zxdh_encap1_to_dtbentry(hw, dh_flow, &dtb_entry);
	ret = zxdh_np_dtb_table_entry_write(hw->dev_id, dtb_qid, 1, &dtb_entry);
	zxdh_fd_flow_free_dtbentry(&dtb_entry);
	if (ret) {
		rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					"write to hw failed");
		return -1;
	}

	zxdh_encap1_ip_to_dtbentry(hw, dh_flow, &dtb_entry);
	ret = zxdh_np_dtb_table_entry_write(hw->dev_id, dtb_qid, 1, &dtb_entry);
	zxdh_fd_flow_free_dtbentry(&dtb_entry);
	if (ret) {
		rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					"write to hw failed");
		return -1;
	}
	return 0;
}

static uint16_t
zxdh_fd_flow_to_dtbentry(struct zxdh_hw *hw __rte_unused,
		struct zxdh_flow_info *fdflow,
		ZXDH_DTB_USER_ENTRY_T *dtb_entry)
{
	ZXDH_DTB_ACL_ENTRY_INFO_T *dtb_acl_entry;
	uint16_t handle_idx = 0;
	dtb_acl_entry = rte_zmalloc("fdflow_dtbentry", sizeof(ZXDH_DTB_ACL_ENTRY_INFO_T), 0);

	if (dtb_acl_entry == NULL)
		return INVALID_HANDLEIDX;

	dtb_acl_entry->key_data = (uint8_t *)&fdflow->fd_flow.key;
	dtb_acl_entry->key_mask = (uint8_t *)&fdflow->fd_flow.key_mask;
	dtb_acl_entry->p_as_rslt = (uint8_t *)&fdflow->fd_flow.result;

	handle_idx = fdflow->hw_idx;

	if (handle_idx >= ZXDH_MAX_FLOW_NUM) {
		rte_free(dtb_acl_entry);
		return INVALID_HANDLEIDX;
	}
	dtb_acl_entry->handle = handle_idx;
	dtb_entry->sdt_no = ZXDH_SDT_FD_TABLE;
	dtb_entry->p_entry_data = dtb_acl_entry;
	return handle_idx;
}

static int zxdh_hw_flow_insert(struct rte_eth_dev *dev,
								struct zxdh_flow *dh_flow,
								struct rte_flow_error *error,
								uint16_t vport)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	uint32_t dtb_qid = hw->dev_sd->dtb_sd.queueid;
	ZXDH_DTB_USER_ENTRY_T dtb_entry = {0};
	uint32_t ret;
	uint16_t handle_idx;

	struct zxdh_flow_info *flow = &dh_flow->flowentry;
	handle_idx = zxdh_fd_flow_to_dtbentry(hw, flow, &dtb_entry);
	if (handle_idx == INVALID_HANDLEIDX) {
		rte_flow_error_set(error, EINVAL,
						 RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
						 "Failed to allocate memory for hw");
		return -1;
	}
	ret = zxdh_np_dtb_table_entry_write(hw->dev_id, dtb_qid, 1, &dtb_entry);
	zxdh_fd_flow_free_dtbentry(&dtb_entry);
	if (ret) {
		ret = free_handle(hw, handle_idx, vport);
		if (ret) {
			rte_flow_error_set(error, EINVAL,
						 RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
						 "release handle_idx to hw failed");
			return -1;
		}
		rte_flow_error_set(error, EINVAL,
						 RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
						 "write to hw failed");
		return -1;
	}
	dh_flow->flowentry.hw_idx = handle_idx;
	return 0;
}

static int
hw_count_query(struct zxdh_hw *hw, uint32_t countid, bool clear,
		struct flow_stats *fstats, struct rte_flow_error *error)
{
	uint32_t stats_id = 0;
	int ret = 0;
	stats_id = countid;
	if (stats_id >= ZXDH_MAX_FLOW_NUM) {
		PMD_DRV_LOG(DEBUG, "query count id %d invalid", stats_id);
		ret = rte_flow_error_set(error, ENODEV,
				 RTE_FLOW_ERROR_TYPE_HANDLE,
				 NULL,
				 "query count id invalid");
		return -rte_errno;
	}
	PMD_DRV_LOG(DEBUG, "query count id %d,clear %d ", stats_id, clear);
	if (!clear)
		ret = zxdh_np_dtb_stats_get(hw->dev_id, hw->dev_sd->dtb_sd.queueid, 1,
					stats_id + ZXDH_FLOW_STATS_INGRESS_BASE,
					(uint32_t *)fstats);
	else
		ret = zxdh_np_stat_ppu_cnt_get_ex(hw->dev_id, 1,
					stats_id + ZXDH_FLOW_STATS_INGRESS_BASE,
					1, (uint32_t *)fstats);
	if (ret)
		rte_flow_error_set(error, EINVAL,
				 RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					 "fail to get flow stats");
	return ret;
}

static int
count_deref(struct zxdh_hw *hw, uint32_t countid,
		struct rte_flow_error *error)
{
	int ret = 0;
	struct count_res *count_res = &flow_count_ref[countid];
	struct flow_stats fstats = {0};

	rte_spinlock_lock(&count_res->count_lock);

	if (count_res->count_ref >= 1) {
		count_res->count_ref--;
	} else {
		rte_spinlock_unlock(&count_res->count_lock);
		return rte_flow_error_set(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					 NULL,
					 "count deref underflow");
	}
	if (count_res->count_ref == 0)
		ret = hw_count_query(hw, countid, 1, &fstats, error);

	rte_spinlock_unlock(&count_res->count_lock);
	return ret;
}

static int
count_ref(struct zxdh_hw *hw, uint32_t countid, struct rte_flow_error *error)
{
	int ret = 0;
	struct count_res *count_res = &flow_count_ref[countid];
	struct flow_stats fstats = {0};

	rte_spinlock_lock(&count_res->count_lock);
	if (count_res->count_ref < 255) {
		count_res->count_ref++;
	} else {
		rte_spinlock_unlock(&count_res->count_lock);
		return rte_flow_error_set(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					 NULL,
					 "count ref overflow");
	}

	if (count_res->count_ref == 1)
		ret = hw_count_query(hw, countid, 1, &fstats, error);

	rte_spinlock_unlock(&count_res->count_lock);
	return ret;
}

int
pf_fd_hw_apply(struct rte_eth_dev *dev, struct zxdh_flow *dh_flow,
		struct rte_flow_error *error, uint16_t vport, uint16_t pcieid)
{
	int ret = 0;
	struct zxdh_hw *hw = dev->data->dev_private;
	uint8_t vf_index = 0;
	uint8_t action_bits = dh_flow->flowentry.fd_flow.result.action_idx;
	uint32_t countid  = MAX_FLOW_COUNT_NUM;
	uint32_t handle_idx = 0;
	union zxdh_virport_num port = {0};

	port.vport = vport;
	handle_idx = get_available_handle(hw, vport);
	if (handle_idx >= ZXDH_MAX_FLOW_NUM) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_HANDLE, NULL, "Failed to allocate memory for hw");
		return -1;
	}
	dh_flow->flowentry.hw_idx = handle_idx;
	if ((action_bits & (1 << FD_ACTION_COUNT_BIT)) != 0) {
		countid = handle_idx;
		dh_flow->flowentry.fd_flow.result.countid = countid;
	}

	if ((action_bits & (1 << FD_ACTION_VXLAN_ENCAP)) != 0) {
		dh_flow->flowentry.fd_flow.result.encap0_index = handle_idx;
		if (!port.vf_flag) {
			dh_flow->flowentry.fd_flow.result.encap1_index =
				hw->hash_search_index * MAX_ENCAP1_NUM;
		} else {
			vf_index = VF_IDX(pcieid);
			if (vf_index < (ZXDH_MAX_VF - 1)) {
				dh_flow->flowentry.fd_flow.result.encap1_index =
					hw->hash_search_index * MAX_ENCAP1_NUM + vf_index + 1;
			} else {
				rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
						"encap1 vf_index is too big");
				return -1;
			}
		}
		PMD_DRV_LOG(DEBUG, "encap_index (%d)(%d)",
				dh_flow->flowentry.fd_flow.result.encap0_index,
				dh_flow->flowentry.fd_flow.result.encap1_index);
		if (zxdh_hw_encap_insert(dev, dh_flow, error) != 0)
			return -1;
	}
	ret = zxdh_hw_flow_insert(dev, dh_flow, error, vport);
	if (!ret && countid < MAX_FLOW_COUNT_NUM)
		ret = count_ref(hw, countid, error);

	if (!ret) {
		if (!port.vf_flag) {
			if (((action_bits & (1 << FD_ACTION_VXLAN_ENCAP)) != 0) ||
				((action_bits & (1 << FD_ACTION_VXLAN_DECAP)) != 0)) {
				hw->vxlan_fd_num++;
				if (hw->vxlan_fd_num == 1)
					set_vxlan_enable(dev, 1, error);
			}
		}
	}

	return ret;
}

static int
zxdh_hw_flow_del(struct rte_eth_dev *dev,
							struct zxdh_flow *dh_flow,
							struct rte_flow_error *error,
							uint16_t vport)
{
	struct zxdh_flow_info *flow = &dh_flow->flowentry;
	ZXDH_DTB_USER_ENTRY_T dtb_entry = {0};
	struct zxdh_hw *hw = dev->data->dev_private;
	uint32_t dtb_qid = hw->dev_sd->dtb_sd.queueid;
	uint32_t ret;
	uint16_t handle_idx;

	handle_idx = zxdh_fd_flow_to_dtbentry(hw, flow, &dtb_entry);
	if (handle_idx >= ZXDH_MAX_FLOW_NUM) {
		rte_flow_error_set(error, EINVAL,
						 RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
						 "Failed to allocate memory for hw");
		return -1;
	}
	ret = zxdh_np_dtb_table_entry_delete(hw->dev_id, dtb_qid, 1, &dtb_entry);
	zxdh_fd_flow_free_dtbentry(&dtb_entry);
	if (ret) {
		rte_flow_error_set(error, EINVAL,
						 RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
						 "delete to hw failed");
		return -1;
	}
	ret = free_handle(hw, handle_idx, vport);
	if (ret) {
		rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
						"release handle_idx to hw failed");
		return -1;
	}
	PMD_DRV_LOG(DEBUG, "release handle_idx to hw success! %d", handle_idx);
	return ret;
}

int
pf_fd_hw_destroy(struct rte_eth_dev *dev, struct zxdh_flow *dh_flow,
		struct rte_flow_error *error, uint16_t vport,
		uint16_t pcieid __rte_unused)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	union zxdh_virport_num port = {0};
	int ret = 0;

	port.vport = vport;
	ret = zxdh_hw_flow_del(dev, dh_flow, error, vport);
	PMD_DRV_LOG(DEBUG, "destroy handle id %d", dh_flow->flowentry.hw_idx);
	if (!ret) {
		uint8_t action_bits = dh_flow->flowentry.fd_flow.result.action_idx;
		uint32_t countid;
		countid = dh_flow->flowentry.hw_idx;
		if ((action_bits & (1 << FD_ACTION_COUNT_BIT)) != 0)
			ret = count_deref(hw, countid, error);
		if (!port.vf_flag) {
			if (((action_bits & (1 << FD_ACTION_VXLAN_ENCAP)) != 0) ||
				((action_bits & (1 << FD_ACTION_VXLAN_DECAP)) != 0)) {
				hw->vxlan_fd_num--;
				if (hw->vxlan_fd_num == 0)
					set_vxlan_enable(dev, 0, error);
			}
		}
	}
	return ret;
}

static int
zxdh_hw_flow_query(struct rte_eth_dev *dev, struct zxdh_flow *dh_flow,
		struct rte_flow_error *error)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	int ret = 0;
	struct zxdh_flow_info *flow = &dh_flow->flowentry;
	ZXDH_DTB_USER_ENTRY_T dtb_entry;
	uint16_t handle_idx;

	handle_idx = zxdh_fd_flow_to_dtbentry(hw, flow, &dtb_entry);
	if (handle_idx >= ZXDH_MAX_FLOW_NUM) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Failed to build hw entry for query");
		ret = -1;
		goto free_res;
	}
	ret = zxdh_np_dtb_table_entry_get(hw->dev_id, hw->dev_sd->dtb_sd.queueid, &dtb_entry, 0);
	if (ret != 0) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Failed query  entry from hw ");
		goto free_res;
	}

free_res:
	zxdh_fd_flow_free_dtbentry(&dtb_entry);
	return ret;
}

int
pf_fd_hw_query_count(struct rte_eth_dev *dev,
			struct zxdh_flow *flow,
			struct rte_flow_query_count *count,
			struct rte_flow_error *error)
{
	struct zxdh_hw *hw =  dev->data->dev_private;
	struct flow_stats  fstats = {0};
	int ret = 0;
	uint32_t countid;

	memset(&flow->flowentry.fd_flow.result, 0, sizeof(struct fd_flow_result));
	ret = zxdh_hw_flow_query(dev, flow, error);
	if (ret) {
		ret = rte_flow_error_set(error, ENODEV,
				 RTE_FLOW_ERROR_TYPE_HANDLE,
				 NULL,
				 "query failed");
		return -rte_errno;
	}
	countid = flow->flowentry.hw_idx;
	if (countid >= ZXDH_MAX_FLOW_NUM) {
		ret = rte_flow_error_set(error, ENODEV,
				 RTE_FLOW_ERROR_TYPE_HANDLE,
				 NULL,
				 "query count id invalid");
		return -rte_errno;
	}
	ret = hw_count_query(hw, countid, 0, &fstats, error);
	if (ret) {
		rte_flow_error_set(error, EINVAL,
				 RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					 "fail to get flow stats");
			return ret;
	}
	count->bytes = (uint64_t)(rte_le_to_cpu_32(fstats.hit_bytes_hi)) << 32 |
					rte_le_to_cpu_32(fstats.hit_bytes_lo);
	count->hits = (uint64_t)(rte_le_to_cpu_32(fstats.hit_pkts_hi)) << 32 |
					rte_le_to_cpu_32(fstats.hit_pkts_lo);
	return ret;
}

static int
fd_flow_parse_attr(struct rte_eth_dev *dev __rte_unused,
		const struct rte_flow_attr *attr,
		struct rte_flow_error *error,
		struct zxdh_flow *dh_flow)
{
	/* Not supported */
	if (attr->priority) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
				   attr, "Not support priority.");
		return -rte_errno;
	}

	/* Not supported */
	if (attr->group >= MAX_GROUP) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
				   attr, "Not support group.");
		return -rte_errno;
	}

	if (dh_flow) {
		dh_flow->group = attr->group;
		dh_flow->direct = (attr->ingress == 1) ? 0 : 1;
		dh_flow->pri = attr->priority;
	}

	return 0;
}

static int fd_flow_parse_pattern(struct rte_eth_dev *dev, const struct rte_flow_item *items,
			 struct rte_flow_error *error, struct zxdh_flow *dh_flow)
{
	struct zxdh_hw *priv = dev->data->dev_private;
	struct zxdh_flow_info *flow = NULL;
	const struct rte_flow_item *item;
	const struct rte_flow_item_eth *eth_spec, *eth_mask;
	const struct rte_flow_item_vlan *vlan_spec, *vlan_mask;
	const struct rte_flow_item_ipv4 *ipv4_spec, *ipv4_mask;
	const struct rte_flow_item_ipv6 *ipv6_spec = NULL, *ipv6_mask = NULL;
	const struct rte_flow_item_tcp *tcp_spec, *tcp_mask;
	const struct rte_flow_item_udp *udp_spec, *udp_mask;
	const struct rte_flow_item_sctp *sctp_spec, *sctp_mask;
	const struct rte_flow_item_vxlan *vxlan_spec, *vxlan_mask;
	struct fd_flow_key *key, *key_mask;

	if (dh_flow) {
		flow = &dh_flow->flowentry;
	} else {
		flow = rte_zmalloc("dh_flow", sizeof(*flow), 0);
		if (flow == NULL) {
			rte_flow_error_set(error, EINVAL,
						 RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
						 "Failed to allocate memory ");
			return -rte_errno;
		}
	}

	key = &flow->fd_flow.key;
	key_mask = &flow->fd_flow.key_mask;
	key->vfid = rte_cpu_to_be_16(priv->vfid);
	key_mask->vfid  = 0xffff;
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		item = items;
		if (items->last) {
			rte_flow_error_set(error, EINVAL,
					 RTE_FLOW_ERROR_TYPE_ITEM,
					 items,
					 "Not support range");
			return -rte_errno;
		}

		switch (item->type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			eth_spec = item->spec;
			eth_mask = item->mask;
			if (eth_spec && eth_mask) {
				key->mac_dst = eth_spec->dst;
				key->mac_src  = eth_spec->src;
				key_mask->mac_dst  = eth_mask->dst;
				key_mask->mac_src  = eth_mask->src;

				if (eth_mask->type == 0xffff) {
					key->ether_type = eth_spec->type;
					key_mask->ether_type = eth_mask->type;
				}
			}
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			vlan_spec = item->spec;
			vlan_mask = item->mask;
			if (vlan_spec && vlan_mask) {
				key->vlan_tci  = vlan_spec->tci;
				key_mask->vlan_tci = vlan_mask->tci;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			ipv4_spec = item->spec;
			ipv4_mask = item->mask;

			if (ipv4_spec && ipv4_mask) {
				/* Check IPv4 mask and update input set */
				if (ipv4_mask->hdr.version_ihl ||
					ipv4_mask->hdr.total_length ||
					ipv4_mask->hdr.packet_id ||
					ipv4_mask->hdr.hdr_checksum ||
					ipv4_mask->hdr.time_to_live) {
					rte_flow_error_set(error, EINVAL,
							 RTE_FLOW_ERROR_TYPE_ITEM,
							 item,
							 "Invalid IPv4 mask.");
					return -rte_errno;
				}
					/* Get the filter info */
				key->nw_proto =
						ipv4_spec->hdr.next_proto_id;
				key->tos =
						ipv4_spec->hdr.type_of_service;
				key_mask->nw_proto =
						ipv4_mask->hdr.next_proto_id;
				key_mask->tos =
						ipv4_mask->hdr.type_of_service;
				key->frag_flag = (ipv4_spec->hdr.fragment_offset != 0) ? 1 : 0;
				key_mask->frag_flag = (ipv4_mask->hdr.fragment_offset != 0) ? 1 : 0;
				rte_memcpy((uint32_t *)key->src_ip + 3,
							 &ipv4_spec->hdr.src_addr, 4);
				rte_memcpy((uint32_t *)key->dst_ip + 3,
							 &ipv4_spec->hdr.dst_addr, 4);
				rte_memcpy((uint32_t *)key_mask->src_ip + 3,
							 &ipv4_mask->hdr.src_addr, 4);
				rte_memcpy((uint32_t *)key_mask->dst_ip + 3,
							 &ipv4_mask->hdr.dst_addr, 4);
			}
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			ipv6_spec = item->spec;
			ipv6_mask = item->mask;

			if (ipv6_spec && ipv6_mask) {
				/* Check IPv6 mask and update input set */
				if (ipv6_mask->hdr.payload_len ||
					 ipv6_mask->hdr.hop_limits == UINT8_MAX) {
					rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM,
						item,
						"Invalid IPv6 mask");
					return -rte_errno;
				}
				key->tc =
					(uint8_t)((ipv6_spec->hdr.vtc_flow &
								RTE_IPV6_HDR_TC_MASK) >>
								RTE_IPV6_HDR_TC_SHIFT);
				key_mask->tc =
					(uint8_t)((ipv6_mask->hdr.vtc_flow &
								RTE_IPV6_HDR_TC_MASK) >>
								RTE_IPV6_HDR_TC_SHIFT);

				key->nw_proto = ipv6_spec->hdr.proto;
				key_mask->nw_proto = ipv6_mask->hdr.proto;

				rte_memcpy(key->src_ip,
							 &ipv6_spec->hdr.src_addr, 16);
				rte_memcpy(key->dst_ip,
							 &ipv6_spec->hdr.dst_addr, 16);
				rte_memcpy(key_mask->src_ip,
							 &ipv6_mask->hdr.src_addr, 16);
				rte_memcpy(key_mask->dst_ip,
							 &ipv6_mask->hdr.dst_addr, 16);
			}
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			tcp_spec = item->spec;
			tcp_mask = item->mask;

			if (tcp_spec && tcp_mask) {
				/* Check TCP mask and update input set */
				if (tcp_mask->hdr.sent_seq ||
					tcp_mask->hdr.recv_ack ||
					tcp_mask->hdr.data_off ||
					tcp_mask->hdr.tcp_flags ||
					tcp_mask->hdr.rx_win ||
					tcp_mask->hdr.cksum ||
					tcp_mask->hdr.tcp_urp ||
					(tcp_mask->hdr.src_port &&
					tcp_mask->hdr.src_port != UINT16_MAX) ||
					(tcp_mask->hdr.dst_port &&
					tcp_mask->hdr.dst_port != UINT16_MAX)) {
					rte_flow_error_set(error, EINVAL,
								 RTE_FLOW_ERROR_TYPE_ITEM,
								 item,
								 "Invalid TCP mask");
					return -rte_errno;
				}

				key->tp_src = tcp_spec->hdr.src_port;
				key_mask->tp_src = tcp_mask->hdr.src_port;

				key->tp_dst = tcp_spec->hdr.dst_port;
				key_mask->tp_dst = tcp_mask->hdr.dst_port;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			udp_spec = item->spec;
			udp_mask = item->mask;

			if (udp_spec && udp_mask) {
				/* Check UDP mask and update input set*/
				if (udp_mask->hdr.dgram_len ||
					udp_mask->hdr.dgram_cksum ||
					(udp_mask->hdr.src_port &&
					udp_mask->hdr.src_port != UINT16_MAX) ||
					(udp_mask->hdr.dst_port &&
					udp_mask->hdr.dst_port != UINT16_MAX)) {
					rte_flow_error_set(error, EINVAL,
									 RTE_FLOW_ERROR_TYPE_ITEM,
									 item,
									 "Invalid UDP mask");
					return -rte_errno;
				}

				key->tp_src = udp_spec->hdr.src_port;
				key_mask->tp_src = udp_mask->hdr.src_port;

				key->tp_dst = udp_spec->hdr.dst_port;
				key_mask->tp_dst = udp_mask->hdr.dst_port;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_SCTP:
			sctp_spec = item->spec;
			sctp_mask = item->mask;

			if (!(sctp_spec && sctp_mask))
				break;

			/* Check SCTP mask and update input set */
			if (sctp_mask->hdr.cksum) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid sctp mask");
				return -rte_errno;
			}

			/* Mask for SCTP src/dst ports not supported */
			if (sctp_mask->hdr.src_port &&
				sctp_mask->hdr.src_port != UINT16_MAX)
				return -rte_errno;
			if (sctp_mask->hdr.dst_port &&
				sctp_mask->hdr.dst_port != UINT16_MAX)
				return -rte_errno;

			key->tp_src = sctp_spec->hdr.src_port;
			key_mask->tp_src = sctp_mask->hdr.src_port;
			key->tp_dst = sctp_spec->hdr.dst_port;
			key_mask->tp_dst = sctp_mask->hdr.dst_port;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
		{
			vxlan_spec = item->spec;
			vxlan_mask = item->mask;
			static const struct rte_flow_item_vxlan flow_item_vxlan_mask = {
				.vni = {0xff, 0xff, 0xff},
			};
			if (!(vxlan_spec && vxlan_mask))
				break;
			if (memcmp(vxlan_mask, &flow_item_vxlan_mask,
				sizeof(struct rte_flow_item_vxlan))) {
				rte_flow_error_set(error, EINVAL,
								 RTE_FLOW_ERROR_TYPE_ITEM,
								 item,
								 "Invalid vxlan mask");
					return -rte_errno;
			}
			rte_memcpy(key->vni, vxlan_spec->vni, 3);
			rte_memcpy(key_mask->vni, vxlan_mask->vni, 3);
			break;
		}
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		default:
				return rte_flow_error_set(error, ENOTSUP,
							RTE_FLOW_ERROR_TYPE_ITEM,
							NULL, "item not supported");
		}
	}

	data_bitwise(key_mask, sizeof(*key_mask));
	return 0;
}

static inline int
validate_action_rss(struct rte_eth_dev *dev,
			 const struct rte_flow_action *action,
			 struct rte_flow_error *error)
{
	const struct rte_flow_action_rss *rss = action->conf;

	if (rss->func != RTE_ETH_HASH_FUNCTION_DEFAULT &&
		rss->func != RTE_ETH_HASH_FUNCTION_TOEPLITZ) {
		rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF,
				&rss->func,
				"RSS hash function not supported");
		return -rte_errno;
		}

	if (rss->level > 1) {
		rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF,
				&rss->level,
				"tunnel RSS is not supported");
		return -rte_errno;
	}

	/* allow RSS key_len 0 in case of NULL (default) RSS key. */
	if (rss->key_len == 0 && rss->key != NULL) {
		rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF,
				&rss->key_len,
				"RSS hash key length 0");
		return -rte_errno;
	}

	if (rss->key_len > 0 && rss->key_len < ZXDH_RSS_HASH_KEY_LEN) {
		rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF,
				&rss->key_len,
				"RSS hash key too small, value is 40U");
		return -rte_errno;
	}

	if (rss->key_len > ZXDH_RSS_HASH_KEY_LEN) {
		rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF,
				&rss->key_len,
				"RSS hash key too large, value is 40U");
		return -rte_errno;
	}

	if (!rss->queue_num) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF,
				&dev->data->nb_rx_queues, "No queues configured");
		return -rte_errno;
	}

	return 0;
}

static int
fd_flow_parse_vxlan_encap(struct rte_eth_dev *dev __rte_unused,
		const struct rte_flow_item *item,
		struct zxdh_flow *dh_flow)
{
	const struct rte_flow_item *items;
	const struct rte_flow_item_eth *item_eth;
	const struct rte_flow_item_vlan *item_vlan;
	const struct rte_flow_item_ipv4 *item_ipv4;
	const struct rte_flow_item_ipv6 *item_ipv6;
	const struct rte_flow_item_udp *item_udp;
	const struct rte_flow_item_vxlan *item_vxlan;
	uint32_t i = 0;
	rte_be32_t addr;

	for (i = 0; i < ACTION_VXLAN_ENCAP_ITEMS_NUM; i++) {
		items = &item[i];
		switch (items->type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			item_eth = items->spec;
			rte_memcpy(&dh_flow->encap0.dst_mac1, item_eth->dst.addr_bytes, 2);
			rte_memcpy(&dh_flow->encap1.src_mac1, item_eth->src.addr_bytes, 2);
			rte_memcpy(&dh_flow->encap0.dst_mac2, &item_eth->dst.addr_bytes[2], 4);
			rte_memcpy(&dh_flow->encap1.src_mac2, &item_eth->src.addr_bytes[2], 4);
			dh_flow->encap0.dst_mac1 = rte_bswap16(dh_flow->encap0.dst_mac1);
			dh_flow->encap1.src_mac1 = rte_bswap16(dh_flow->encap1.src_mac1);
			dh_flow->encap0.dst_mac2 = rte_bswap32(dh_flow->encap0.dst_mac2);
			dh_flow->encap1.src_mac2 = rte_bswap32(dh_flow->encap1.src_mac2);
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			item_vlan = items->spec;
			dh_flow->encap1.vlan_tci = item_vlan->hdr.vlan_tci;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			item_ipv4 = items->spec;
			dh_flow->encap0.ethtype = 0;
			dh_flow->encap0.tos = item_ipv4->hdr.type_of_service;
			dh_flow->encap0.ttl = item_ipv4->hdr.time_to_live;
			addr = rte_bswap32(item_ipv4->hdr.src_addr);
			rte_memcpy((uint32_t *)dh_flow->encap1.sip.ip_addr + 3, &addr, 4);
			addr = rte_bswap32(item_ipv4->hdr.dst_addr);
			rte_memcpy((uint32_t *)dh_flow->encap0.dip.ip_addr + 3, &addr, 4);
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			item_ipv6 = items->spec;
			dh_flow->encap0.ethtype = 1;
			dh_flow->encap0.tos =
					(item_ipv6->hdr.vtc_flow & RTE_IPV6_HDR_TC_MASK) >>
						RTE_IPV6_HDR_TC_SHIFT;
			dh_flow->encap0.ttl = item_ipv6->hdr.hop_limits;
			rte_memcpy(dh_flow->encap1.sip.ip_addr, &item_ipv6->hdr.src_addr, 16);
			dh_flow->encap1.sip.ip_addr[0] =
				rte_bswap32(dh_flow->encap1.sip.ip_addr[0]);
			dh_flow->encap1.sip.ip_addr[1] =
				rte_bswap32(dh_flow->encap1.sip.ip_addr[1]);
			dh_flow->encap1.sip.ip_addr[2] =
				rte_bswap32(dh_flow->encap1.sip.ip_addr[2]);
			dh_flow->encap1.sip.ip_addr[3] =
				rte_bswap32(dh_flow->encap1.sip.ip_addr[3]);
			rte_memcpy(dh_flow->encap0.dip.ip_addr, &item_ipv6->hdr.dst_addr, 16);
			dh_flow->encap0.dip.ip_addr[0] =
					rte_bswap32(dh_flow->encap0.dip.ip_addr[0]);
			dh_flow->encap0.dip.ip_addr[1] =
					rte_bswap32(dh_flow->encap0.dip.ip_addr[1]);
			dh_flow->encap0.dip.ip_addr[2] =
					rte_bswap32(dh_flow->encap0.dip.ip_addr[2]);
			dh_flow->encap0.dip.ip_addr[3] =
					rte_bswap32(dh_flow->encap0.dip.ip_addr[3]);
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			item_udp = items->spec;
			dh_flow->encap0.tp_dst = item_udp->hdr.dst_port;
			dh_flow->encap0.tp_dst = rte_bswap16(dh_flow->encap0.tp_dst);
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			item_vxlan = items->spec;
			dh_flow->encap0.vni = item_vxlan->vni[0] * 65536 +
					item_vxlan->vni[1] * 256 + item_vxlan->vni[2];
			break;
		case RTE_FLOW_ITEM_TYPE_VOID:
			break;
		default:
			break;
		}
	}
	dh_flow->encap0.hit_flag = 1;
	dh_flow->encap1.hit_flag = 1;

	return 0;
}

static int
fd_flow_parse_action(struct rte_eth_dev *dev, const struct rte_flow_action *actions,
			 struct rte_flow_error *error, struct zxdh_flow *dh_flow)
{
	struct zxdh_flow_info *flow = NULL;
	struct fd_flow_result *result = NULL;
	const struct rte_flow_item *enc_item = NULL;
	uint8_t action_bitmap = 0;
	uint32_t dest_num = 0;
	uint32_t mark_num = 0;
	uint32_t counter_num = 0;
	int ret;

	rte_errno = 0;
	if (dh_flow) {
		flow = &dh_flow->flowentry;
	} else {
		flow = rte_zmalloc("dh_flow", sizeof(*flow), 0);
		if (flow == NULL) {
			rte_flow_error_set(error, EINVAL,
					 RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					 "Failed to allocate memory ");
			return -rte_errno;
		}
	}
	result = &flow->fd_flow.result;
	action_bitmap = result->action_idx;

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_RSS:
		{
			dest_num++;
			if (action_bitmap & (1 << FD_ACTION_RSS_BIT)) {
				rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION, actions,
						"rss action does no support.");
				goto free_flow;
			}
			ret = validate_action_rss(dev, actions, error);
			if (ret)
				goto free_flow;
			action_bitmap |= (1 << FD_ACTION_RSS_BIT);
			break;
		}
		case RTE_FLOW_ACTION_TYPE_MARK:
		{
			mark_num++;
			if (action_bitmap & (1 << FD_ACTION_MARK_BIT)) {
				rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION, actions,
						"multi mark action no support.");
				goto free_flow;
			}
			const struct rte_flow_action_mark *act_mark = actions->conf;
			result->mark_fd_id = rte_cpu_to_le_32(act_mark->id);
			action_bitmap |= (1 << FD_ACTION_MARK_BIT);
			break;
		}
		case RTE_FLOW_ACTION_TYPE_COUNT:
		{
			counter_num++;
			if (action_bitmap & (1 << FD_ACTION_COUNT_BIT)) {
				rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION, actions,
						"multi count action no support.");
				goto free_flow;
			}
			const struct rte_flow_action_count *act_count = actions->conf;
			if (act_count->id > MAX_FLOW_COUNT_NUM) {
				rte_flow_error_set(error, EINVAL,
							RTE_FLOW_ERROR_TYPE_ACTION, actions,
							"count action id no support.");
				goto free_flow;
			};
			result->countid = act_count->id;
			action_bitmap |= (1 << FD_ACTION_COUNT_BIT);
			break;
		}
		case RTE_FLOW_ACTION_TYPE_QUEUE:
		{
			dest_num++;
			if (action_bitmap & (1 << FD_ACTION_QUEUE_BIT)) {
				rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION, actions,
						"multi queue action no support.");
				goto free_flow;
			}
			const struct rte_flow_action_queue *act_q;
			act_q = actions->conf;
			if (act_q->index >= dev->data->nb_rx_queues) {
				rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION, actions,
						"Invalid queue ID");
				goto free_flow;
			}
			ret = zxdh_hw_qid_to_logic_qid(dev, act_q->index << 1);
			if (ret < 0) {
				rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION, actions,
						"Invalid phy queue ID .");
				goto free_flow;
			}
			result->qid = rte_cpu_to_le_16(ret);
			action_bitmap |= (1 << FD_ACTION_QUEUE_BIT);

			PMD_DRV_LOG(DEBUG, "QID RET 0x%x", result->qid);
			break;
		}
		case RTE_FLOW_ACTION_TYPE_DROP:
		{
			dest_num++;
			if (action_bitmap & (1 << FD_ACTION_DROP_BIT)) {
				rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION, actions,
						"multi drop action no support.");
				goto free_flow;
			}
			action_bitmap |= (1 << FD_ACTION_DROP_BIT);
			break;
		}
		case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:
		{
			dest_num++;
			if (action_bitmap & (1 << FD_ACTION_VXLAN_DECAP)) {
				rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION, actions,
						"multi drop action no support.");
				goto free_flow;
			}
			action_bitmap |= (1 << FD_ACTION_VXLAN_DECAP);
			break;
		}
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
			enc_item = ((const struct rte_flow_action_vxlan_encap *)
				   actions->conf)->definition;
			if (dh_flow != NULL)
				fd_flow_parse_vxlan_encap(dev, enc_item, dh_flow);
			dest_num++;
			if (action_bitmap & (1 << FD_ACTION_VXLAN_ENCAP)) {
				rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION, actions,
						"multi drop action no support.");
				goto free_flow;
			}
			action_bitmap |= (1 << FD_ACTION_VXLAN_ENCAP);
			break;
		default:
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, actions,
				"Invalid action.");
			goto free_flow;
		}
	}

	if (dest_num >= 2) {
		rte_flow_error_set(error, EINVAL,
			   RTE_FLOW_ERROR_TYPE_ACTION, actions,
			   "Unsupported action combination");
		return -rte_errno;
	}

	if (mark_num >= 2) {
		rte_flow_error_set(error, EINVAL,
			   RTE_FLOW_ERROR_TYPE_ACTION, actions,
			   "Too many mark actions");
		return -rte_errno;
	}

	if (counter_num >= 2) {
		rte_flow_error_set(error, EINVAL,
			   RTE_FLOW_ERROR_TYPE_ACTION, actions,
			   "Too many count actions");
		return -rte_errno;
	}

	if (dest_num + mark_num + counter_num == 0) {
		rte_flow_error_set(error, EINVAL,
			   RTE_FLOW_ERROR_TYPE_ACTION, actions,
			   "Empty action, packet forwarding as default");
		return -rte_errno;
	}

	result->action_idx = action_bitmap;
	return 0;

free_flow:
	if (!dh_flow)
		rte_free(flow);
	return -rte_errno;
}

static int
fd_parse_pattern_action(struct rte_eth_dev *dev,
			const struct rte_flow_attr *attr,
			const struct rte_flow_item pattern[],
			const struct rte_flow_action *actions,
			struct rte_flow_error *error, struct zxdh_flow *dh_flow)
{
	int ret = 0;
	ret = fd_flow_parse_attr(dev, attr, error, dh_flow);
	if (ret < 0)
		return -rte_errno;
	ret = fd_flow_parse_pattern(dev, pattern, error, dh_flow);
	if (ret < 0)
		return -rte_errno;

	ret = fd_flow_parse_action(dev, actions, error, dh_flow);
	if (ret < 0)
		return -rte_errno;
	return 0;
}

struct dh_flow_engine pf_fd_engine = {
	.apply = pf_fd_hw_apply,
	.destroy = pf_fd_hw_destroy,
	.query_count = pf_fd_hw_query_count,
	.parse_pattern_action = fd_parse_pattern_action,
	.type = FLOW_TYPE_FD_TCAM,
};


static int
vf_flow_msg_process(enum zxdh_msg_type msg_type, struct rte_eth_dev *dev,
		struct zxdh_flow *dh_flow, struct rte_flow_error *error,
		struct rte_flow_query_count *count)
{
	int ret = 0;
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_msg_info msg_info = {0};
	struct zxdh_flow_op_msg *flow_msg = &msg_info.data.flow_msg;

	uint8_t zxdh_msg_reply_info[ZXDH_ST_SZ_BYTES(msg_reply_info)] = {0};
	void *reply_body_addr = ZXDH_ADDR_OF(msg_reply_info, zxdh_msg_reply_info, reply_body);
	void *flow_rsp_addr = ZXDH_ADDR_OF(msg_reply_body, reply_body_addr, flow_rsp);
	uint8_t flow_op_rsp[sizeof(struct zxdh_flow_op_rsp)] = {0};
	uint16_t len = sizeof(struct zxdh_flow_op_rsp) - 4;
	struct zxdh_flow_op_rsp *flow_rsp = (struct zxdh_flow_op_rsp *)flow_op_rsp;

	dh_flow->hash_search_index = hw->hash_search_index;
	rte_memcpy(&flow_msg->dh_flow, dh_flow, sizeof(struct zxdh_flow));

	zxdh_msg_head_build(hw, msg_type, &msg_info);
	ret = zxdh_vf_send_msg_to_pf(dev, &msg_info, sizeof(struct zxdh_msg_info),
			(void *)zxdh_msg_reply_info, ZXDH_ST_SZ_BYTES(msg_reply_info));
	zxdh_adjust_flow_op_rsp_memory_layout(flow_rsp_addr, len, flow_op_rsp);
	if (ret) {
		PMD_DRV_LOG(ERR, "port %d flow op %d failed ret %d", hw->port_id, msg_type, ret);
		if (ret == -2) {
			PMD_DRV_LOG(ERR, "port %d  flow %d failed: cause %s",
				 hw->port_id, msg_type, flow_rsp->error.reason);
			rte_flow_error_set(error, EBUSY,
					 RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					 flow_rsp->error.reason);
		} else {
			rte_flow_error_set(error, EBUSY,
					 RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					 "msg channel error");
		}
		return ret;
	}

	if (msg_type == ZXDH_FLOW_HW_ADD)
		dh_flow->flowentry.hw_idx = flow_rsp->dh_flow.flowentry.hw_idx;
	if (count)
		rte_memcpy((void *)count, &flow_rsp->count, sizeof(flow_rsp->count));

	return ret;
}

static int
vf_fd_apply(struct rte_eth_dev *dev, struct zxdh_flow *dh_flow,
		struct rte_flow_error *error, uint16_t vport __rte_unused,
		uint16_t pcieid __rte_unused)
{
	int ret = 0;
	struct zxdh_hw *hw = dev->data->dev_private;
	ret =  vf_flow_msg_process(ZXDH_FLOW_HW_ADD, dev, dh_flow, error, NULL);
	if (!ret) {
		uint8_t action_bits = dh_flow->flowentry.fd_flow.result.action_idx;
		if (((action_bits & (1 << FD_ACTION_VXLAN_ENCAP)) != 0) ||
				((action_bits & (1 << FD_ACTION_VXLAN_DECAP)) != 0)) {
			hw->vxlan_fd_num++;
			if (hw->vxlan_fd_num == 1) {
				set_vxlan_enable(dev, 1, error);
				PMD_DRV_LOG(DEBUG, "vf set_vxlan_enable");
			}
		}
	}
	return ret;
}

static int
vf_fd_destroy(struct rte_eth_dev *dev, struct zxdh_flow *dh_flow,
		struct rte_flow_error *error, uint16_t vport __rte_unused,
		uint16_t pcieid __rte_unused)
{
	int ret = 0;
	struct zxdh_hw *hw = dev->data->dev_private;
	ret = vf_flow_msg_process(ZXDH_FLOW_HW_DEL, dev, dh_flow, error, NULL);
	if (!ret) {
		uint8_t action_bits = dh_flow->flowentry.fd_flow.result.action_idx;
		if (((action_bits & (1 << FD_ACTION_VXLAN_ENCAP)) != 0) ||
				((action_bits & (1 << FD_ACTION_VXLAN_DECAP)) != 0)) {
			hw->vxlan_fd_num--;
			if (hw->vxlan_fd_num == 0) {
				set_vxlan_enable(dev, 0, error);
				PMD_DRV_LOG(DEBUG, "vf set_vxlan_disable");
			}
		}
	}
	return ret;
}

static int
vf_fd_query_count(struct rte_eth_dev *dev,
		struct zxdh_flow *dh_flow,
		struct rte_flow_query_count *count,
		struct rte_flow_error *error)
{
	int ret = 0;
	ret = vf_flow_msg_process(ZXDH_FLOW_HW_GET, dev, dh_flow, error, count);
	return ret;
}


static struct dh_flow_engine vf_fd_engine = {
	.apply = vf_fd_apply,
	.destroy = vf_fd_destroy,
	.parse_pattern_action = fd_parse_pattern_action,
	.query_count = vf_fd_query_count,
	.type = FLOW_TYPE_FD_TCAM,
};

void zxdh_flow_init(struct rte_eth_dev *dev)
{
	struct zxdh_hw *priv =  dev->data->dev_private;
	if (priv->is_pf)
		zxdh_register_flow_engine(&pf_fd_engine);
	else
		zxdh_register_flow_engine(&vf_fd_engine);
	TAILQ_INIT(&priv->dh_flow_list);
}

const struct rte_flow_ops zxdh_flow_ops = {
	.validate = zxdh_flow_validate,
	.create = zxdh_flow_create,
	.destroy = zxdh_flow_destroy,
	.flush = zxdh_flow_flush,
	.query = zxdh_flow_query,
	.dev_dump = zxdh_flow_dev_dump,
};

int
zxdh_flow_ops_get(struct rte_eth_dev *dev __rte_unused,
		const struct rte_flow_ops **ops)
{
	*ops = &zxdh_flow_ops;

	return 0;
}

void
zxdh_flow_release(struct rte_eth_dev *dev)
{
	struct rte_flow_error error = {0};
	const struct rte_flow_ops *flow_ops = NULL;

	if (dev->dev_ops && dev->dev_ops->flow_ops_get)
		dev->dev_ops->flow_ops_get(dev, &flow_ops);
	if (flow_ops && flow_ops->flush)
		flow_ops->flush(dev, &error);
}
