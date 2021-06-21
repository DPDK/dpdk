/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <pthread.h>

#include "roc_api.h"
#include "roc_priv.h"

#define CGX_CMRX_INT		       0x40
#define CGX_CMRX_INT_OVERFLW	       BIT_ULL(1)
/*
 * CN10K stores number of lmacs in 4 bit filed
 * in contraty to CN9K which uses only 3 bits.
 *
 * In theory masks should differ yet on CN9K
 * bits beyond specified range contain zeros.
 *
 * Hence common longer mask may be used.
 */
#define CGX_CMRX_RX_LMACS	0x128
#define CGX_CMRX_RX_LMACS_LMACS GENMASK_ULL(3, 0)
#define CGX_CMRX_SCRATCH0	0x1050
#define CGX_CMRX_SCRATCH1	0x1058

static uint64_t
roc_bphy_cgx_read(struct roc_bphy_cgx *roc_cgx, uint64_t lmac, uint64_t offset)
{
	int shift = roc_model_is_cn10k() ? 20 : 18;
	uint64_t base = (uint64_t)roc_cgx->bar0_va;

	return plt_read64(base + (lmac << shift) + offset);
}

static void
roc_bphy_cgx_write(struct roc_bphy_cgx *roc_cgx, uint64_t lmac, uint64_t offset,
		   uint64_t value)
{
	int shift = roc_model_is_cn10k() ? 20 : 18;
	uint64_t base = (uint64_t)roc_cgx->bar0_va;

	plt_write64(value, base + (lmac << shift) + offset);
}

static void
roc_bphy_cgx_ack(struct roc_bphy_cgx *roc_cgx, unsigned int lmac,
		 uint64_t *scr0)
{
	uint64_t val;

	/* clear interrupt */
	val = roc_bphy_cgx_read(roc_cgx, lmac, CGX_CMRX_INT);
	val |= FIELD_PREP(CGX_CMRX_INT_OVERFLW, 1);
	roc_bphy_cgx_write(roc_cgx, lmac, CGX_CMRX_INT, val);

	/* ack fw response */
	*scr0 &= ~SCR0_ETH_EVT_STS_S_ACK;
	roc_bphy_cgx_write(roc_cgx, lmac, CGX_CMRX_SCRATCH0, *scr0);
}

static int
roc_bphy_cgx_wait_for_ownership(struct roc_bphy_cgx *roc_cgx, unsigned int lmac,
				uint64_t *scr0)
{
	int tries = 5000;
	uint64_t scr1;

	do {
		*scr0 = roc_bphy_cgx_read(roc_cgx, lmac, CGX_CMRX_SCRATCH0);
		scr1 = roc_bphy_cgx_read(roc_cgx, lmac, CGX_CMRX_SCRATCH1);

		if (FIELD_GET(SCR1_OWN_STATUS, scr1) == ETH_OWN_NON_SECURE_SW &&
		    FIELD_GET(SCR0_ETH_EVT_STS_S_ACK, *scr0) == 0)
			break;

		/* clear async events if any */
		if (FIELD_GET(SCR0_ETH_EVT_STS_S_EVT_TYPE, *scr0) ==
		    ETH_EVT_ASYNC &&
		    FIELD_GET(SCR0_ETH_EVT_STS_S_ACK, *scr0))
			roc_bphy_cgx_ack(roc_cgx, lmac, scr0);

		plt_delay_ms(1);
	} while (--tries);

	return tries ? 0 : -ETIMEDOUT;
}

static int
roc_bphy_cgx_wait_for_ack(struct roc_bphy_cgx *roc_cgx, unsigned int lmac,
			  uint64_t *scr0)
{
	int tries = 5000;
	uint64_t scr1;

	do {
		*scr0 = roc_bphy_cgx_read(roc_cgx, lmac, CGX_CMRX_SCRATCH0);
		scr1 = roc_bphy_cgx_read(roc_cgx, lmac, CGX_CMRX_SCRATCH1);

		if (FIELD_GET(SCR1_OWN_STATUS, scr1) == ETH_OWN_NON_SECURE_SW &&
		    FIELD_GET(SCR0_ETH_EVT_STS_S_ACK, *scr0))
			break;

		plt_delay_ms(1);
	} while (--tries);

	return tries ? 0 : -ETIMEDOUT;
}

static int __rte_unused
roc_bphy_cgx_intf_req(struct roc_bphy_cgx *roc_cgx, unsigned int lmac,
		      uint64_t scr1, uint64_t *scr0)
{
	uint8_t cmd_id = FIELD_GET(SCR1_ETH_CMD_ID, scr1);
	int ret;

	pthread_mutex_lock(&roc_cgx->lock);

	/* wait for ownership */
	ret = roc_bphy_cgx_wait_for_ownership(roc_cgx, lmac, scr0);
	if (ret) {
		plt_err("timed out waiting for ownership");
		goto out;
	}

	/* write command */
	scr1 |= FIELD_PREP(SCR1_OWN_STATUS, ETH_OWN_FIRMWARE);
	roc_bphy_cgx_write(roc_cgx, lmac, CGX_CMRX_SCRATCH1, scr1);

	/* wait for command ack */
	ret = roc_bphy_cgx_wait_for_ack(roc_cgx, lmac, scr0);
	if (ret) {
		plt_err("timed out waiting for response");
		goto out;
	}

	if (cmd_id == ETH_CMD_INTF_SHUTDOWN)
		goto out;

	if (FIELD_GET(SCR0_ETH_EVT_STS_S_EVT_TYPE, *scr0) != ETH_EVT_CMD_RESP) {
		plt_err("received async event instead of cmd resp event");
		ret = -EIO;
		goto out;
	}

	if (FIELD_GET(SCR0_ETH_EVT_STS_S_ID, *scr0) != cmd_id) {
		plt_err("received resp for cmd %d expected for cmd %d",
			(int)FIELD_GET(SCR0_ETH_EVT_STS_S_ID, *scr0), cmd_id);
		ret = -EIO;
		goto out;
	}

	if (FIELD_GET(SCR0_ETH_EVT_STS_S_STAT, *scr0) != ETH_STAT_SUCCESS) {
		plt_err("cmd %d failed on cgx%u lmac%u with errcode %d", cmd_id,
			roc_cgx->id, lmac,
			(int)FIELD_GET(SCR0_ETH_LNK_STS_S_ERR_TYPE, *scr0));
		ret = -EIO;
	}

out:
	roc_bphy_cgx_ack(roc_cgx, lmac, scr0);

	pthread_mutex_unlock(&roc_cgx->lock);

	return ret;
}

static unsigned int
roc_bphy_cgx_dev_id(struct roc_bphy_cgx *roc_cgx)
{
	uint64_t cgx_id = roc_model_is_cn10k() ? GENMASK_ULL(26, 24) :
						 GENMASK_ULL(25, 24);

	return FIELD_GET(cgx_id, roc_cgx->bar0_pa);
}

int
roc_bphy_cgx_dev_init(struct roc_bphy_cgx *roc_cgx)
{
	uint64_t val;
	int ret;

	if (!roc_cgx || !roc_cgx->bar0_va || !roc_cgx->bar0_pa)
		return -EINVAL;

	ret = pthread_mutex_init(&roc_cgx->lock, NULL);
	if (ret)
		return ret;

	val = roc_bphy_cgx_read(roc_cgx, 0, CGX_CMRX_RX_LMACS);
	val = FIELD_GET(CGX_CMRX_RX_LMACS_LMACS, val);
	if (roc_model_is_cn9k())
		val = GENMASK_ULL(val - 1, 0);
	roc_cgx->lmac_bmap = val;
	roc_cgx->id = roc_bphy_cgx_dev_id(roc_cgx);

	return 0;
}

int
roc_bphy_cgx_dev_fini(struct roc_bphy_cgx *roc_cgx)
{
	if (!roc_cgx)
		return -EINVAL;

	pthread_mutex_destroy(&roc_cgx->lock);

	return 0;
}
