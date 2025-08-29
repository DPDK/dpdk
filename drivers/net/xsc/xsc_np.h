/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Yunsilicon Technology Co., Ltd.
 */

#ifndef _XSC_NP_H_
#define _XSC_NP_H_

#include <rte_byteorder.h>
#include <rte_ethdev.h>

struct xsc_dev;

struct __rte_packed_begin xsc_ipat_key {
	uint16_t logical_in_port:11;
	uint16_t rsv:5;
} __rte_packed_end;

struct __rte_packed_begin xsc_ipat_action {
	uint64_t rsv0;
	uint64_t rsv1:9;
	uint64_t dst_info:11;
	uint64_t rsv2:34;
	uint64_t vld:1;
	uint64_t rsv:1;
} __rte_packed_end;

struct xsc_np_ipat {
	struct xsc_ipat_key key;
	struct xsc_ipat_action action;
};

struct __rte_packed_begin xsc_epat_key {
	uint16_t dst_info:11;
	uint16_t rsv:5;
} __rte_packed_end;

struct __rte_packed_begin xsc_epat_action {
	uint8_t rsv0[3];
	uint8_t mac_addr[6];
	uint64_t mac_filter_en:1;
	uint64_t rsv1:43;
	uint64_t dst_port:4;
	uint64_t rss_hash_func:2;
	uint64_t rss_hash_template:5;
	uint64_t rss_en:1;
	uint64_t qp_num:8;
	uint16_t rx_qp_id_ofst:12;
	uint16_t rsv3:4;
	uint8_t rsv4:7;
	uint8_t vld:1;
} __rte_packed_end;

struct xsc_np_epat_add {
	struct xsc_epat_key key;
	struct xsc_epat_action action;
};

struct xsc_np_epat_mod {
	uint64_t flags;
	struct xsc_epat_key key;
	struct xsc_epat_action action;
};

struct __rte_packed_begin xsc_pct_v4_key {
	uint16_t rsv0[20];
	uint32_t rsv1:13;
	uint32_t logical_in_port:11;
	uint32_t rsv2:8;
} __rte_packed_end;

struct __rte_packed_begin xsc_pct_action {
	uint64_t rsv0:29;
	uint64_t dst_info:11;
	uint64_t rsv1:8;
} __rte_packed_end;

struct xsc_np_pct_v4_add {
	struct xsc_pct_v4_key key;
	struct xsc_pct_v4_key mask;
	struct xsc_pct_action action;
	uint32_t pct_idx;
};

struct xsc_np_pct_v4_del {
	struct xsc_pct_v4_key key;
	struct xsc_pct_v4_key mask;
	uint32_t pct_idx;
};

struct __rte_packed_begin xsc_pg_qp_set_id_key {
	uint16_t qp_id:13;
	uint16_t rsv:3;
} __rte_packed_end;

struct __rte_packed_begin xsc_pg_qp_set_id_action {
	uint16_t qp_set_id:9;
	uint16_t rsv:7;
} __rte_packed_end;

struct xsc_pg_set_id {
	struct xsc_pg_qp_set_id_key key;
	struct xsc_pg_qp_set_id_action action;
};

struct __rte_packed_begin xsc_vfos_key {
	uint16_t src_port:11;
	uint16_t rsv:5;
} __rte_packed_end;

struct __rte_packed_begin xsc_vfos_start_ofst_action {
	uint16_t ofst:11;
	uint16_t rsv:5;
} __rte_packed_end;

struct xsc_np_vfso {
	struct xsc_vfos_key key;
	struct xsc_vfos_start_ofst_action action;
};

struct xsc_dev_pct_mgr {
	uint8_t *bmp_mem;
	struct rte_bitmap *bmp_pct;
};

struct xsc_dev_pct_entry {
	LIST_ENTRY(xsc_dev_pct_entry) next;
	uint32_t logic_port;
	uint32_t pct_idx;
};

LIST_HEAD(xsc_dev_pct_list, xsc_dev_pct_entry);

int xsc_dev_create_pct(struct xsc_dev *xdev, int repr_id,
		       uint16_t logical_in_port, uint16_t dst_info);
int xsc_dev_destroy_pct(struct xsc_dev *xdev, uint16_t logical_in_port, uint32_t pct_idx);
void xsc_dev_clear_pct(struct xsc_dev *xdev, int repr_id);
int xsc_dev_create_ipat(struct xsc_dev *xdev, uint16_t logic_in_port, uint16_t dst_info);
int xsc_dev_get_ipat_vld(struct xsc_dev *xdev, uint16_t logic_in_port);
int xsc_dev_destroy_ipat(struct xsc_dev *xdev, uint16_t logic_in_port);
int xsc_dev_create_epat(struct xsc_dev *xdev, uint16_t dst_info, uint8_t dst_port,
			uint16_t qpn_ofst, uint8_t qp_num, struct rte_eth_rss_conf *rss_conf,
			uint8_t mac_filter_en, uint8_t *mac);
int xsc_dev_vf_modify_epat(struct xsc_dev *xdev, uint16_t dst_info, uint16_t qpn_ofst,
			   uint8_t qp_num, struct rte_eth_rss_conf *rss_conf,
			   uint8_t mac_filter_en, uint8_t *mac);
int xsc_dev_modify_epat_mac_filter(struct xsc_dev *xdev, uint16_t dst_info,
				   uint8_t mac_filter_en);
int xsc_dev_destroy_epat(struct xsc_dev *xdev, uint16_t dst_info);
int xsc_dev_set_qpsetid(struct xsc_dev *xdev, uint32_t txqpn, uint16_t qp_set_id);
int xsc_dev_create_vfos_baselp(struct xsc_dev *xdev);
void xsc_dev_pct_uninit(void);
int xsc_dev_pct_init(void);
uint32_t xsc_dev_pct_idx_alloc(void);
void xsc_dev_pct_idx_free(uint32_t pct_idx);
int xsc_dev_pct_entry_insert(struct xsc_dev_pct_list *pct_list,
			     uint32_t logic_port, uint32_t pct_idx);
struct xsc_dev_pct_entry *xsc_dev_pct_first_get(struct xsc_dev_pct_list *pct_list);
int xsc_dev_pct_entry_remove(struct xsc_dev_pct_entry *pct_entry);

#endif /* _XSC_NP_H_ */
