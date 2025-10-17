/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2024 NXP
 */

#ifndef ENETC_NTMP_H
#define ENETC_NTMP_H

#include "compat.h"
#include <linux/types.h>

#define BITS_PER_LONG   (__SIZEOF_LONG__ * 8)
#define BITS_PER_LONG_LONG  (__SIZEOF_LONG_LONG__ * 8)
#define GENMASK(h, l)       (((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))

/* define NTMP Operation Commands */
#define NTMP_CMD_DELETE			BIT(0)
#define NTMP_CMD_UPDATE			BIT(1)
#define NTMP_CMD_QUERY			BIT(2)

#define NETC_CBDR_TIMEOUT		1000 /* us */
#define NETC_CBDR_BD_NUM		256
#define NETC_CBDR_BASE_ADDR_ALIGN	128
#define NETC_CBD_DATA_ADDR_ALIGN	16
#define NETC_CBDRMR_EN			BIT(31)

#define NTMP_RESP_HDR_ERR		GENMASK(11, 0)

struct common_req_data {
	uint16_t update_act;
	uint8_t dbg_opt;
	uint8_t query_act:4;
	uint8_t tbl_ver:4;
};

/* RSS Table Request and Response Data Buffer Format */
struct rsst_req_query {
	struct common_req_data crd;
	uint32_t entry_id;
};

/* struct for update operation */
struct rsst_req_update {
	struct common_req_data crd;
	uint32_t entry_id;
	uint8_t groups[];
};

/* The format of conctrol buffer descriptor */
union netc_cbd {
	struct {
		uint64_t addr;
		uint32_t len;
		uint8_t cmd;
		uint8_t resv1:4;
		uint8_t access_method:4;
		uint8_t table_id;
		uint8_t hdr_ver:6;
		uint8_t cci:1;
		uint8_t rr:1;
		uint32_t resv2[3];
		uint32_t npf;
	} ntmp_req_hdr;	/* NTMP Request Message Header Format */

	struct {
		uint32_t resv1[3];
		uint16_t num_matched;
		uint16_t error_rr; /* bit0~11: error, bit12~14: reserved, bit15: rr */
		uint32_t resv3[4];
	} ntmp_resp_hdr; /* NTMP Response Message Header Format */
};

struct netc_cbdr_regs {
	void *pir;
	void *cir;
	void *mr;
	void *st;

	void *bar0;
	void *bar1;
	void *lenr;

	/* station interface current time register */
	void *sictr0;
	void *sictr1;
};

struct netc_cbdr {
	struct netc_cbdr_regs regs;

	int bd_num;
	int next_to_use;
	int next_to_clean;

	int dma_size;
	void *addr_base;
	void *addr_base_align;
	dma_addr_t dma_base;
	dma_addr_t dma_base_align;
	struct rte_eth_dev *dma_dev;

	rte_spinlock_t ring_lock; /* Avoid race condition */

	/* bitmap of used words of SGCL table */
	unsigned long *sgclt_used_words;
	uint32_t sgclt_words_num;
	uint32_t timeout;
	uint32_t delay;
};

#endif /* ENETC_NTMP_H */
