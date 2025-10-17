/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2024 NXP
 */

#include <ethdev_pci.h>

#include "enetc_logs.h"
#include "enetc.h"

#define NTMP_RSST_ID                    3

/* Define NTMP Access Method */
#define NTMP_AM_ENTRY_ID                0
#define NTMP_AM_EXACT_KEY               1
#define NTMP_AM_SEARCH                  2
#define NTMP_AM_TERNARY_KEY             3

/* Define NTMP Header Version */
#define NTMP_HEADER_VERSION2            2

#define NTMP_REQ_HDR_NPF                BIT(15)

#define NTMP_RESP_LEN_MASK              GENMASK(19, 0)
#define NTMP_REQ_LEN_MASK               GENMASK(31, 20)

#define ENETC_NTMP_ENTRY_ID_SIZE        4

#define ENETC_RSS_TABLE_ENTRY_NUM       64
#define ENETC_RSS_CFGEU                 BIT(0)
#define ENETC_RSS_STSEU                 BIT(1)
#define ENETC_RSS_STSE_DATA_SIZE(n)     ((n) * 8)
#define ENETC_RSS_CFGE_DATA_SIZE(n)     (n)

#define NTMP_REQ_RESP_LEN(req, resp)    (((req) << 20 & NTMP_REQ_LEN_MASK) | \
					((resp) & NTMP_RESP_LEN_MASK))

static inline uint32_t
netc_cbdr_read(void *reg)
{
	return rte_read32(reg);
}

static inline void
netc_cbdr_write(void *reg, uint32_t val)
{
	rte_write32(val, reg);
}

static inline void
ntmp_fill_request_headr(union netc_cbd *cbd, dma_addr_t dma,
			int len, int table_id, int cmd,
			int access_method)
{
	dma_addr_t dma_align;

	memset(cbd, 0, sizeof(*cbd));
	dma_align = dma;
	cbd->ntmp_req_hdr.addr = dma_align;
	cbd->ntmp_req_hdr.len = len;
	cbd->ntmp_req_hdr.cmd = cmd;
	cbd->ntmp_req_hdr.access_method = access_method;
	cbd->ntmp_req_hdr.table_id = table_id;
	cbd->ntmp_req_hdr.hdr_ver = NTMP_HEADER_VERSION2;
	cbd->ntmp_req_hdr.cci = 0;
	cbd->ntmp_req_hdr.rr = 0;       /* Must be set to 0 by SW. */
	/* For NTMP version 2.0 or later version */
	cbd->ntmp_req_hdr.npf = NTMP_REQ_HDR_NPF;
}

static inline int
netc_get_free_cbd_num(struct netc_cbdr *cbdr)
{
	return (cbdr->next_to_clean - cbdr->next_to_use - 1 + cbdr->bd_num) %
		cbdr->bd_num;
}

static inline union
netc_cbd *netc_get_cbd(struct netc_cbdr *cbdr, int index)
{
	return &((union netc_cbd *)(cbdr->addr_base_align))[index];
}

static void
netc_clean_cbdr(struct netc_cbdr *cbdr)
{
	union netc_cbd *cbd;
	uint32_t i;

	i = cbdr->next_to_clean;
	while (netc_cbdr_read(cbdr->regs.cir) != i) {
		cbd = netc_get_cbd(cbdr, i);
		memset(cbd, 0, sizeof(*cbd));
		dcbf(cbd);
		i = (i + 1) % cbdr->bd_num;
	}

	cbdr->next_to_clean = i;
}

static int
netc_xmit_ntmp_cmd(struct netc_cbdr *cbdr, union netc_cbd *cbd)
{
	union netc_cbd *ring_cbd;
	uint32_t i, err = 0;
	uint16_t status;
	uint32_t timeout = cbdr->timeout;
	uint32_t delay = cbdr->delay;

	if (unlikely(!cbdr->addr_base))
		return -EFAULT;

	rte_spinlock_lock(&cbdr->ring_lock);

	if (unlikely(!netc_get_free_cbd_num(cbdr)))
		netc_clean_cbdr(cbdr);

	i = cbdr->next_to_use;
	ring_cbd = netc_get_cbd(cbdr, i);

	/* Copy command BD to the ring */
	*ring_cbd = *cbd;
	/* Update producer index of both software and hardware */
	i = (i + 1) % cbdr->bd_num;
	dcbf(ring_cbd);
	cbdr->next_to_use = i;
	netc_cbdr_write(cbdr->regs.pir, i);
	ENETC_PMD_DEBUG("Control msg sent PIR = %d, CIR = %d", netc_cbdr_read(cbdr->regs.pir),
					netc_cbdr_read(cbdr->regs.cir));
	do {
		if (netc_cbdr_read(cbdr->regs.cir) == i) {
			dccivac(ring_cbd);
			ENETC_PMD_DEBUG("got response");
			ENETC_PMD_DEBUG("Matched = %d, status = 0x%x",
					ring_cbd->ntmp_resp_hdr.num_matched,
					ring_cbd->ntmp_resp_hdr.error_rr);
			break;
		}
		rte_delay_us(delay);
	} while (timeout--);

	if (timeout <= 0)
		ENETC_PMD_ERR("no response of RSS configuration");

	ENETC_PMD_DEBUG("CIR after receive = %d, SICBDRSR = 0x%x",
					netc_cbdr_read(cbdr->regs.cir),
					netc_cbdr_read(cbdr->regs.st));
	/* Check the writeback error status */
	status = ring_cbd->ntmp_resp_hdr.error_rr & NTMP_RESP_HDR_ERR;
	if (unlikely(status)) {
		ENETC_PMD_ERR("Command BD error: 0x%04x", status);
		err = -EIO;
	}

	netc_clean_cbdr(cbdr);
	rte_spinlock_unlock(&cbdr->ring_lock);

	return err;
}

int
enetc_ntmp_rsst_query_or_update_entry(struct netc_cbdr *cbdr, uint32_t *table,
				int count, bool query)
{
	struct rsst_req_update *requ;
	struct rsst_req_query *req;
	union netc_cbd cbd;
	uint32_t len, data_size;
	dma_addr_t dma;
	int err, i;
	void *tmp;

	if (count != ENETC_RSS_TABLE_ENTRY_NUM)
		/* HW only takes in a full 64 entry table */
		return -EINVAL;

	if (query)
		data_size = ENETC_NTMP_ENTRY_ID_SIZE + ENETC_RSS_STSE_DATA_SIZE(count) +
			ENETC_RSS_CFGE_DATA_SIZE(count);
	else
		data_size = sizeof(*requ) + count * sizeof(uint8_t);

	tmp = rte_malloc(NULL, data_size, ENETC_CBDR_ALIGN);
	if (!tmp)
		return -ENOMEM;

	dma = rte_mem_virt2iova(tmp);
	req = tmp;
	/* Set the request data buffer */
	if (query) {
		len = NTMP_REQ_RESP_LEN(sizeof(*req), data_size);
		ntmp_fill_request_headr(&cbd, dma, len, NTMP_RSST_ID,
					NTMP_CMD_QUERY, NTMP_AM_ENTRY_ID);
	} else {
		requ = (struct rsst_req_update *)req;
		requ->crd.update_act = (ENETC_RSS_CFGEU | ENETC_RSS_STSEU);
		for (i = 0; i < count; i++)
			requ->groups[i] = (uint8_t)(table[i]);

		len = NTMP_REQ_RESP_LEN(data_size, 0);
		ntmp_fill_request_headr(&cbd, dma, len, NTMP_RSST_ID,
				NTMP_CMD_UPDATE, NTMP_AM_ENTRY_ID);
		dcbf(requ);
	}

	err = netc_xmit_ntmp_cmd(cbdr, &cbd);
	if (err) {
		ENETC_PMD_ERR("%s RSS table entry failed (%d)!",
				query ? "Query" : "Update", err);
		goto end;
	}

	if (query) {
		uint8_t *group = (uint8_t *)req;

		group += ENETC_NTMP_ENTRY_ID_SIZE + ENETC_RSS_STSE_DATA_SIZE(count);
		for (i = 0; i < count; i++)
			table[i] = group[i];
	}
end:
	rte_free(tmp);

	return err;
}

static int
netc_setup_cbdr(struct rte_eth_dev *dev, int cbd_num,
		struct netc_cbdr_regs *regs,
		struct netc_cbdr *cbdr)
{
	int size;

	size = cbd_num * sizeof(union netc_cbd) +
		NETC_CBDR_BASE_ADDR_ALIGN;

	cbdr->addr_base = rte_malloc(NULL, size, ENETC_CBDR_ALIGN);
	if (!cbdr->addr_base)
		return -ENOMEM;

	cbdr->dma_base = rte_mem_virt2iova(cbdr->addr_base);
	cbdr->dma_size = size;
	cbdr->bd_num = cbd_num;
	cbdr->regs = *regs;
	cbdr->dma_dev = dev;
	cbdr->timeout = ENETC_CBDR_TIMEOUT;
	cbdr->delay = ENETC_CBDR_DELAY;

	if (getenv("ENETC4_CBDR_TIMEOUT"))
		cbdr->timeout = atoi(getenv("ENETC4_CBDR_TIMEOUT"));

	if (getenv("ENETC4_CBDR_DELAY"))
		cbdr->delay = atoi(getenv("ENETC4_CBDR_DELAY"));


	ENETC_PMD_DEBUG("CBDR timeout = %u and delay = %u", cbdr->timeout,
						cbdr->delay);
	/* The base address of the Control BD Ring must be 128 bytes aligned */
	cbdr->dma_base_align =  cbdr->dma_base;
	cbdr->addr_base_align = cbdr->addr_base;

	cbdr->next_to_clean = 0;
	cbdr->next_to_use = 0;
	rte_spinlock_init(&cbdr->ring_lock);

	netc_cbdr_write(cbdr->regs.mr, ~((uint32_t)NETC_CBDRMR_EN));
	/* Step 1: Configure the base address of the Control BD Ring */
	netc_cbdr_write(cbdr->regs.bar0, lower_32_bits(cbdr->dma_base_align));
	netc_cbdr_write(cbdr->regs.bar1, upper_32_bits(cbdr->dma_base_align));

	/* Step 2: Configure the producer index register */
	netc_cbdr_write(cbdr->regs.pir, cbdr->next_to_clean);

	/* Step 3: Configure the consumer index register */
	netc_cbdr_write(cbdr->regs.cir, cbdr->next_to_use);
	/* Step4: Configure the number of BDs of the Control BD Ring */
	netc_cbdr_write(cbdr->regs.lenr, cbdr->bd_num);

	/* Step 5: Enable the Control BD Ring */
	netc_cbdr_write(cbdr->regs.mr, NETC_CBDRMR_EN);

	return 0;
}

void
enetc_free_cbdr(struct netc_cbdr *cbdr)
{
	/* Disable the Control BD Ring */
	if (cbdr->regs.mr != NULL) {
		netc_cbdr_write(cbdr->regs.mr, 0);
		rte_free(cbdr->addr_base);
		memset(cbdr, 0, sizeof(*cbdr));
	}
}

int
enetc4_setup_cbdr(struct rte_eth_dev *dev, struct enetc_hw *hw,
		  int bd_count, struct netc_cbdr *cbdr)
{
	struct netc_cbdr_regs regs;

	regs.pir = (void *)((size_t)hw->reg + ENETC4_SICBDRPIR);
	regs.cir = (void *)((size_t)hw->reg + ENETC4_SICBDRCIR);
	regs.mr = (void *)((size_t)hw->reg + ENETC4_SICBDRMR);
	regs.st = (void *)((size_t)hw->reg + ENETC4_SICBDRSR);
	regs.bar0 = (void *)((size_t)hw->reg + ENETC4_SICBDRBAR0);
	regs.bar1 = (void *)((size_t)hw->reg + ENETC4_SICBDRBAR1);
	regs.lenr = (void *)((size_t)hw->reg + ENETC4_SICBDRLENR);
	regs.sictr0 = (void *)((size_t)hw->reg + ENETC4_SICTR0);
	regs.sictr1 = (void *)((size_t)hw->reg + ENETC4_SICTR1);

	return netc_setup_cbdr(dev, bd_count, &regs, cbdr);
}
