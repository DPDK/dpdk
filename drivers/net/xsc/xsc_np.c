/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Yunsilicon Technology Co., Ltd.
 */

#include <rte_bitmap.h>
#include <rte_malloc.h>

#include "xsc_log.h"
#include "xsc_defs.h"
#include "xsc_np.h"
#include "xsc_cmd.h"
#include "xsc_dev.h"

#define XSC_RSS_HASH_FUNC_TOPELIZ	0x1
#define XSC_LOGIC_PORT_MASK		0x07FF

#define XSC_DEV_DEF_PCT_IDX_MIN		128
#define XSC_DEV_DEF_PCT_IDX_MAX		138

/* Each board has a PCT manager*/
static struct xsc_dev_pct_mgr xsc_pct_mgr;

enum xsc_np_type {
	XSC_NP_IPAT		= 0,
	XSC_NP_PCT_V4		= 4,
	XSC_NP_EPAT		= 19,
	XSC_NP_VFOS		= 31,
	XSC_NP_PG_QP_SET_ID	= 41,
	XSC_NP_MAX
};

enum xsc_np_opcode {
	XSC_NP_OP_ADD,
	XSC_NP_OP_DEL,
	XSC_NP_OP_GET,
	XSC_NP_OP_CLR,
	XSC_NP_OP_MOD,
	XSC_NP_OP_MAX
};

struct xsc_np_mbox_in {
	struct xsc_cmd_inbox_hdr hdr;
	rte_be16_t len;
	rte_be16_t rsvd;
	uint8_t data[];
};

struct xsc_np_mbox_out {
	struct xsc_cmd_outbox_hdr hdr;
	rte_be32_t error;
	rte_be16_t len;
	rte_be16_t rsvd;
	uint8_t data[];
};

struct xsc_np_data_tl {
	uint16_t table;
	uint16_t opmod;
	uint16_t length;
	uint16_t rsvd;
};

enum xsc_hash_tmpl {
	XSC_HASH_TMPL_IDX_IP_PORTS_IP6_PORTS = 0,
	XSC_HASH_TMPL_IDX_IP_IP6,
	XSC_HASH_TMPL_IDX_IP_PORTS_IP6,
	XSC_HASH_TMPL_IDX_IP_IP6_PORTS,
	XSC_HASH_TMPL_IDX_MAX,
};

static const int xsc_rss_hash_tmplate[XSC_HASH_TMPL_IDX_MAX] = {
	XSC_RSS_HASH_BIT_IPV4_SIP | XSC_RSS_HASH_BIT_IPV4_DIP |
	XSC_RSS_HASH_BIT_IPV6_SIP | XSC_RSS_HASH_BIT_IPV6_DIP |
	XSC_RSS_HASH_BIT_IPV4_SPORT | XSC_RSS_HASH_BIT_IPV4_DPORT |
	XSC_RSS_HASH_BIT_IPV6_SPORT | XSC_RSS_HASH_BIT_IPV6_DPORT,

	XSC_RSS_HASH_BIT_IPV4_SIP | XSC_RSS_HASH_BIT_IPV4_DIP |
	XSC_RSS_HASH_BIT_IPV6_SIP | XSC_RSS_HASH_BIT_IPV6_DIP,

	XSC_RSS_HASH_BIT_IPV4_SIP | XSC_RSS_HASH_BIT_IPV4_DIP |
	XSC_RSS_HASH_BIT_IPV6_SIP | XSC_RSS_HASH_BIT_IPV6_DIP |
	XSC_RSS_HASH_BIT_IPV4_SPORT | XSC_RSS_HASH_BIT_IPV4_DPORT,

	XSC_RSS_HASH_BIT_IPV4_SIP | XSC_RSS_HASH_BIT_IPV4_DIP |
	XSC_RSS_HASH_BIT_IPV6_SIP | XSC_RSS_HASH_BIT_IPV6_DIP |
	XSC_RSS_HASH_BIT_IPV6_SPORT | XSC_RSS_HASH_BIT_IPV6_DPORT,
};

static uint8_t
xsc_rss_hash_template_get(struct rte_eth_rss_conf *rss_conf)
{
	int rss_hf = 0;
	int i = 0;
	uint8_t idx = 0;
	uint8_t outer = 1;

	if (rss_conf->rss_hf & RTE_ETH_RSS_IP) {
		rss_hf |= XSC_RSS_HASH_BIT_IPV4_SIP;
		rss_hf |= XSC_RSS_HASH_BIT_IPV4_DIP;
		rss_hf |= XSC_RSS_HASH_BIT_IPV6_SIP;
		rss_hf |= XSC_RSS_HASH_BIT_IPV6_DIP;
	}

	if ((rss_conf->rss_hf & RTE_ETH_RSS_UDP) ||
	    (rss_conf->rss_hf & RTE_ETH_RSS_TCP)) {
		rss_hf |= XSC_RSS_HASH_BIT_IPV4_SPORT;
		rss_hf |= XSC_RSS_HASH_BIT_IPV4_DPORT;
		rss_hf |= XSC_RSS_HASH_BIT_IPV6_SPORT;
		rss_hf |= XSC_RSS_HASH_BIT_IPV6_DPORT;
	}

	if (rss_conf->rss_hf & RTE_ETH_RSS_L3_SRC_ONLY) {
		rss_hf |= XSC_RSS_HASH_BIT_IPV4_SIP;
		rss_hf |= XSC_RSS_HASH_BIT_IPV6_SIP;
		rss_hf &= ~XSC_RSS_HASH_BIT_IPV4_DIP;
		rss_hf &= ~XSC_RSS_HASH_BIT_IPV6_DIP;
	}

	if (rss_conf->rss_hf & RTE_ETH_RSS_L3_DST_ONLY) {
		rss_hf |= XSC_RSS_HASH_BIT_IPV4_DIP;
		rss_hf |= XSC_RSS_HASH_BIT_IPV6_DIP;
		rss_hf &= ~XSC_RSS_HASH_BIT_IPV4_SIP;
		rss_hf &= ~XSC_RSS_HASH_BIT_IPV6_SIP;
	}

	if (rss_conf->rss_hf & RTE_ETH_RSS_L4_SRC_ONLY) {
		rss_hf |= XSC_RSS_HASH_BIT_IPV4_SPORT;
		rss_hf |= XSC_RSS_HASH_BIT_IPV6_SPORT;
		rss_hf &= ~XSC_RSS_HASH_BIT_IPV4_DPORT;
		rss_hf &= ~XSC_RSS_HASH_BIT_IPV6_DPORT;
	}

	if (rss_conf->rss_hf & RTE_ETH_RSS_L4_DST_ONLY) {
		rss_hf |= XSC_RSS_HASH_BIT_IPV4_DPORT;
		rss_hf |= XSC_RSS_HASH_BIT_IPV6_DPORT;
		rss_hf &= ~XSC_RSS_HASH_BIT_IPV4_SPORT;
		rss_hf &= ~XSC_RSS_HASH_BIT_IPV6_SPORT;
	}

	if (rss_conf->rss_hf & RTE_ETH_RSS_LEVEL_INNERMOST)
		outer = 0;

	for (i = 0; i < XSC_HASH_TMPL_IDX_MAX; i++) {
		if (xsc_rss_hash_tmplate[i] == rss_hf) {
			idx = i;
			break;
		}
	}

	idx = (idx << 1) | outer;
	return idx;
}

static int
xsc_dev_np_exec(struct xsc_dev *xdev, void *cmd, int len, int table, int opmod)
{
	struct xsc_np_data_tl *tl;
	struct xsc_np_mbox_in *in;
	struct xsc_np_mbox_out *out;
	int in_len;
	int out_len;
	int data_len;
	int cmd_len;
	int ret;
	void *cmd_buf;

	data_len = sizeof(struct xsc_np_data_tl) + len;
	in_len = sizeof(struct xsc_np_mbox_in) + data_len;
	out_len = sizeof(struct xsc_np_mbox_out) + data_len;
	cmd_len = RTE_MAX(in_len, out_len);
	cmd_buf = malloc(cmd_len);
	if (cmd_buf == NULL) {
		rte_errno = ENOMEM;
		PMD_DRV_LOG(ERR, "Failed to alloc np cmd memory");
		return -rte_errno;
	}

	in = cmd_buf;
	memset(in, 0, cmd_len);
	in->hdr.opcode = rte_cpu_to_be_16(XSC_CMD_OP_EXEC_NP);
	in->len = rte_cpu_to_be_16(data_len);

	tl = (struct xsc_np_data_tl *)in->data;
	tl->length = len;
	tl->table = table;
	tl->opmod = opmod;
	if (cmd && len)
		memcpy(tl + 1, cmd, len);

	out = cmd_buf;
	ret = xsc_dev_mailbox_exec(xdev, in, in_len, out, out_len);

	free(cmd_buf);
	return ret;
}

int
xsc_dev_create_pct(struct xsc_dev *xdev, int repr_id,
		   uint16_t logical_in_port, uint16_t dst_info)
{
	int ret;
	struct xsc_np_pct_v4_add add;
	struct xsc_repr_port *repr = &xdev->repr_ports[repr_id];
	struct xsc_dev_pct_list *pct_list = &repr->def_pct_list;

	memset(&add, 0, sizeof(add));
	add.key.logical_in_port = logical_in_port & XSC_LOGIC_PORT_MASK;
	add.mask.logical_in_port = XSC_LOGIC_PORT_MASK;
	add.action.dst_info = dst_info;
	add.pct_idx = xsc_dev_pct_idx_alloc();
	if (add.pct_idx == XSC_DEV_PCT_IDX_INVALID)
		return -1;

	ret = xsc_dev_np_exec(xdev, &add, sizeof(add), XSC_NP_PCT_V4, XSC_NP_OP_ADD);
	if (unlikely(ret != 0)) {
		xsc_dev_pct_idx_free(add.pct_idx);
		return -1;
	}

	xsc_dev_pct_entry_insert(pct_list, add.key.logical_in_port, add.pct_idx);
	return 0;
}

int
xsc_dev_destroy_pct(struct xsc_dev *xdev, uint16_t logical_in_port, uint32_t pct_idx)
{
	struct xsc_np_pct_v4_del del;

	memset(&del, 0, sizeof(del));
	del.key.logical_in_port = logical_in_port & XSC_LOGIC_PORT_MASK;
	del.mask.logical_in_port = XSC_LOGIC_PORT_MASK;
	del.pct_idx = pct_idx;
	return xsc_dev_np_exec(xdev, &del, sizeof(del), XSC_NP_PCT_V4, XSC_NP_OP_DEL);
}

void
xsc_dev_clear_pct(struct xsc_dev *xdev, int repr_id)
{
	struct xsc_repr_port *repr;
	struct xsc_dev_pct_entry *pct_entry;
	struct xsc_dev_pct_list *pct_list;

	if (repr_id == XSC_DEV_REPR_ID_INVALID)
		return;

	repr = &xdev->repr_ports[repr_id];
	pct_list = &repr->def_pct_list;

	while ((pct_entry = xsc_dev_pct_first_get(pct_list)) != NULL) {
		xsc_dev_destroy_pct(xdev, pct_entry->logic_port, pct_entry->pct_idx);
		xsc_dev_pct_entry_remove(pct_entry);
	}
}

int
xsc_dev_create_ipat(struct xsc_dev *xdev, uint16_t logic_in_port, uint16_t dst_info)
{
	struct xsc_np_ipat add;

	memset(&add, 0, sizeof(add));
	add.key.logical_in_port = logic_in_port;
	add.action.dst_info = dst_info;
	add.action.vld = 1;
	return xsc_dev_np_exec(xdev, &add, sizeof(add), XSC_NP_IPAT, XSC_NP_OP_ADD);
}

int
xsc_dev_get_ipat_vld(struct xsc_dev *xdev, uint16_t logic_in_port)
{
	int ret;
	struct xsc_np_ipat get;

	memset(&get, 0, sizeof(get));
	get.key.logical_in_port = logic_in_port;

	ret = xsc_dev_np_exec(xdev, &get, sizeof(get), XSC_NP_IPAT, XSC_NP_OP_GET);
	if (ret != 0)
		PMD_DRV_LOG(ERR, "Get ipat vld failed, logic in port=%u", logic_in_port);

	return get.action.vld;
}

int
xsc_dev_destroy_ipat(struct xsc_dev *xdev, uint16_t logic_in_port)
{
	struct xsc_ipat_key del;

	memset(&del, 0, sizeof(del));
	del.logical_in_port = logic_in_port;
	return xsc_dev_np_exec(xdev, &del, sizeof(del), XSC_NP_IPAT, XSC_NP_OP_DEL);
}

int
xsc_dev_create_epat(struct xsc_dev *xdev, uint16_t dst_info, uint8_t dst_port,
		    uint16_t qpn_ofst, uint8_t qp_num, struct rte_eth_rss_conf *rss_conf)
{
	struct xsc_np_epat_add add;

	memset(&add, 0, sizeof(add));
	add.key.dst_info = dst_info;
	add.action.dst_port = dst_port;
	add.action.vld = 1;
	add.action.rx_qp_id_ofst = qpn_ofst;
	add.action.qp_num = qp_num - 1;
	add.action.rss_en = 1;
	add.action.rss_hash_func = XSC_RSS_HASH_FUNC_TOPELIZ;
	add.action.rss_hash_template = xsc_rss_hash_template_get(rss_conf);

	return xsc_dev_np_exec(xdev, &add, sizeof(add), XSC_NP_EPAT, XSC_NP_OP_ADD);
}

int
xsc_dev_vf_modify_epat(struct xsc_dev *xdev, uint16_t dst_info, uint16_t qpn_ofst,
		       uint8_t qp_num, struct rte_eth_rss_conf *rss_conf)
{
	struct xsc_np_epat_mod mod;

	memset(&mod, 0, sizeof(mod));
	mod.flags = XSC_EPAT_VLD_FLAG | XSC_EPAT_RX_QP_ID_OFST_FLAG |
		    XSC_EPAT_QP_NUM_FLAG | XSC_EPAT_HAS_PPH_FLAG |
		    XSC_EPAT_RSS_EN_FLAG | XSC_EPAT_RSS_HASH_TEMPLATE_FLAG |
		    XSC_EPAT_RSS_HASH_FUNC_FLAG;

	mod.key.dst_info = dst_info;
	mod.action.vld = 1;
	mod.action.rx_qp_id_ofst = qpn_ofst;
	mod.action.qp_num = qp_num - 1;
	mod.action.rss_en = 1;
	mod.action.rss_hash_func = XSC_RSS_HASH_FUNC_TOPELIZ;
	mod.action.rss_hash_template = xsc_rss_hash_template_get(rss_conf);

	return xsc_dev_np_exec(xdev, &mod, sizeof(mod), XSC_NP_EPAT, XSC_NP_OP_MOD);
}

int
xsc_dev_set_qpsetid(struct xsc_dev *xdev, uint32_t txqpn, uint16_t qp_set_id)
{
	int ret;
	struct xsc_pg_set_id add;
	uint16_t qp_id_base = xdev->hwinfo.raw_qp_id_base;

	memset(&add, 0, sizeof(add));
	add.key.qp_id = txqpn - qp_id_base;
	add.action.qp_set_id = qp_set_id;

	ret = xsc_dev_np_exec(xdev, &add, sizeof(add), XSC_NP_PG_QP_SET_ID, XSC_NP_OP_ADD);
	if (ret != 0)
		PMD_DRV_LOG(ERR, "Failed to set qp %u setid %u", txqpn, qp_set_id);

	return ret;
}

int
xsc_dev_destroy_epat(struct xsc_dev *xdev, uint16_t dst_info)
{
	struct xsc_epat_key del;

	memset(&del, 0, sizeof(del));

	del.dst_info = dst_info;
	return xsc_dev_np_exec(xdev, &del, sizeof(del), XSC_NP_EPAT, XSC_NP_OP_DEL);
}

int
xsc_dev_create_vfos_baselp(struct xsc_dev *xdev)
{
	int ret;
	struct xsc_np_vfso add;

	memset(&add, 0, sizeof(add));
	add.key.src_port = xdev->vfrep_offset;
	add.action.ofst = xdev->vfos_logical_in_port;

	ret = xsc_dev_np_exec(xdev, &add, sizeof(add), XSC_NP_VFOS, XSC_NP_OP_ADD);
	if (ret != 0)
		PMD_DRV_LOG(ERR, "Failed to set vfos, port=%u, offset=%u",
			    add.key.src_port, add.action.ofst);

	return ret;
}

void
xsc_dev_pct_uninit(void)
{
	rte_free(xsc_pct_mgr.bmp_mem);
	xsc_pct_mgr.bmp_mem = NULL;
}

int
xsc_dev_pct_init(void)
{
	int ret;
	uint8_t *bmp_mem;
	uint32_t pos, pct_sz, bmp_sz;

	if (xsc_pct_mgr.bmp_mem != NULL)
		return 0;

	pct_sz = XSC_DEV_DEF_PCT_IDX_MAX - XSC_DEV_DEF_PCT_IDX_MIN + 1;
	bmp_sz = rte_bitmap_get_memory_footprint(pct_sz);
	bmp_mem = rte_zmalloc(NULL, bmp_sz, RTE_CACHE_LINE_SIZE);
	if (bmp_mem == NULL) {
		PMD_DRV_LOG(ERR, "Failed to alloc pct bitmap memory");
		ret = -ENOMEM;
		goto pct_init_fail;
	}

	xsc_pct_mgr.bmp_mem = bmp_mem;
	xsc_pct_mgr.bmp_pct = rte_bitmap_init(pct_sz, bmp_mem, bmp_sz);
	if (xsc_pct_mgr.bmp_pct == NULL) {
		PMD_DRV_LOG(ERR, "Failed to init pct bitmap");
		ret = -EINVAL;
		goto pct_init_fail;
	}

	/* Mark all pct bitmap available */
	for (pos = 0; pos < pct_sz; pos++)
		rte_bitmap_set(xsc_pct_mgr.bmp_pct, pos);

	return 0;

pct_init_fail:
	xsc_dev_pct_uninit();
	return ret;
}

uint32_t
xsc_dev_pct_idx_alloc(void)
{
	int ret;
	uint64_t slab = 0;
	uint32_t pos = 0;

	ret = rte_bitmap_scan(xsc_pct_mgr.bmp_pct, &pos, &slab);
	if (ret != 0) {
		pos += rte_bsf64(slab);
		rte_bitmap_clear(xsc_pct_mgr.bmp_pct, pos);
		return (pos + XSC_DEV_DEF_PCT_IDX_MIN);
	}

	PMD_DRV_LOG(ERR, "Failed to alloc xsc pct idx");
	return XSC_DEV_PCT_IDX_INVALID;
}

void
xsc_dev_pct_idx_free(uint32_t pct_idx)
{
	rte_bitmap_set(xsc_pct_mgr.bmp_pct, pct_idx - XSC_DEV_DEF_PCT_IDX_MIN);
}

int
xsc_dev_pct_entry_insert(struct xsc_dev_pct_list *pct_list,
			 uint32_t logic_port, uint32_t pct_idx)
{
	struct xsc_dev_pct_entry *pct_entry;

	pct_entry = rte_zmalloc(NULL, sizeof(struct xsc_dev_pct_entry), RTE_CACHE_LINE_SIZE);
	if (pct_entry == NULL) {
		PMD_DRV_LOG(ERR, "Failed to alloc pct entry memory");
		return -ENOMEM;
	}

	pct_entry->logic_port = logic_port;
	pct_entry->pct_idx = pct_idx;
	LIST_INSERT_HEAD(pct_list, pct_entry, next);

	return 0;
}

struct xsc_dev_pct_entry *
xsc_dev_pct_first_get(struct xsc_dev_pct_list *pct_list)
{
	struct xsc_dev_pct_entry *pct_entry;

	pct_entry = LIST_FIRST(pct_list);
	return pct_entry;
}

int
xsc_dev_pct_entry_remove(struct xsc_dev_pct_entry *pct_entry)
{
	if (pct_entry == NULL)
		return -1;

	xsc_dev_pct_idx_free(pct_entry->pct_idx);
	LIST_REMOVE(pct_entry, next);
	rte_free(pct_entry);

	return 0;
}
