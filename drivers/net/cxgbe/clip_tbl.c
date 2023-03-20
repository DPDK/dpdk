/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Chelsio Communications.
 * All rights reserved.
 */

#include "base/common.h"
#include "clip_tbl.h"

/**
 * Allocate clip entry in HW with associated IPV4/IPv6 address
 */
static int clip6_get_mbox(const struct rte_eth_dev *dev, const u32 *lip)
{
	struct adapter *adap = ethdev2adap(dev);
	struct fw_clip_cmd c;
	u64 hi = ((u64)lip[1]) << 32 | lip[0];
	u64 lo = ((u64)lip[3]) << 32 | lip[2];

	memset(&c, 0, sizeof(c));
	c.op_to_write = cpu_to_be32(V_FW_CMD_OP(FW_CLIP_CMD) |
				    F_FW_CMD_REQUEST | F_FW_CMD_WRITE);
	c.alloc_to_len16 = cpu_to_be32(F_FW_CLIP_CMD_ALLOC | FW_LEN16(c));
	c.ip_hi = hi;
	c.ip_lo = lo;
	return t4_wr_mbox_meat(adap, adap->mbox, &c, sizeof(c), &c, false);
}

/**
 * Delete clip entry in HW having the associated IPV4/IPV6 address
 */
static int clip6_release_mbox(const struct rte_eth_dev *dev, const u32 *lip)
{
	struct adapter *adap = ethdev2adap(dev);
	struct fw_clip_cmd c;
	u64 hi = ((u64)lip[1]) << 32 | lip[0];
	u64 lo = ((u64)lip[3]) << 32 | lip[2];

	memset(&c, 0, sizeof(c));
	c.op_to_write = cpu_to_be32(V_FW_CMD_OP(FW_CLIP_CMD) |
				    F_FW_CMD_REQUEST | F_FW_CMD_READ);
	c.alloc_to_len16 = cpu_to_be32(F_FW_CLIP_CMD_FREE | FW_LEN16(c));
	c.ip_hi = hi;
	c.ip_lo = lo;
	return t4_wr_mbox_meat(adap, adap->mbox, &c, sizeof(c), &c, false);
}

/**
 * cxgbe_clip_release - Release associated CLIP entry
 * @ce: clip entry to release
 *
 * Releases ref count and frees up a clip entry from CLIP table
 */
void cxgbe_clip_release(struct rte_eth_dev *dev, struct clip_entry *ce)
{
	int ret;

	t4_os_lock(&ce->lock);
	if (__atomic_fetch_sub(&ce->refcnt, 1, __ATOMIC_RELAXED) - 1 == 0) {
		ret = clip6_release_mbox(dev, ce->addr);
		if (ret)
			dev_debug(adap, "CLIP FW DEL CMD failed: %d", ret);
	}
	t4_os_unlock(&ce->lock);
}

/**
 * find_or_alloc_clipe - Find/Allocate a free CLIP entry
 * @c: CLIP table
 * @lip: IPV4/IPV6 address to compare/add
 * Returns pointer to the IPV4/IPV6 entry found/created
 *
 * Finds/Allocates an CLIP entry to be used for a filter rule.
 */
static struct clip_entry *find_or_alloc_clipe(struct clip_tbl *c,
					      const u32 *lip)
{
	struct clip_entry *end, *e;
	struct clip_entry *first_free = NULL;
	unsigned int clipt_size = c->clipt_size;

	for (e = &c->cl_list[0], end = &c->cl_list[clipt_size]; e != end; ++e) {
		if (__atomic_load_n(&e->refcnt, __ATOMIC_RELAXED) == 0) {
			if (!first_free)
				first_free = e;
		} else {
			if (memcmp(lip, e->addr, sizeof(e->addr)) == 0)
				goto exists;
		}
	}

	if (first_free) {
		e = first_free;
		goto exists;
	}

	return NULL;

exists:
	return e;
}

static struct clip_entry *t4_clip_alloc(struct rte_eth_dev *dev,
					u32 *lip, u8 v6)
{
	struct adapter *adap = ethdev2adap(dev);
	struct clip_tbl *ctbl = adap->clipt;
	struct clip_entry *ce;
	int ret = 0;

	if (!ctbl)
		return NULL;

	t4_os_write_lock(&ctbl->lock);
	ce = find_or_alloc_clipe(ctbl, lip);
	if (ce) {
		t4_os_lock(&ce->lock);
		if (__atomic_load_n(&ce->refcnt, __ATOMIC_RELAXED) == 0) {
			rte_memcpy(ce->addr, lip, sizeof(ce->addr));
			if (v6) {
				ce->type = FILTER_TYPE_IPV6;
				__atomic_store_n(&ce->refcnt, 1,
						 __ATOMIC_RELAXED);
				ret = clip6_get_mbox(dev, lip);
				if (ret)
					dev_debug(adap,
						  "CLIP FW ADD CMD failed: %d",
						  ret);
			} else {
				ce->type = FILTER_TYPE_IPV4;
			}
		} else {
			__atomic_fetch_add(&ce->refcnt, 1, __ATOMIC_RELAXED);
		}
		t4_os_unlock(&ce->lock);
	}
	t4_os_write_unlock(&ctbl->lock);

	return ret ? NULL : ce;
}

/**
 * cxgbe_clip_alloc - Allocate a IPV6 CLIP entry
 * @dev: rte_eth_dev pointer
 * @lip: IPV6 address to add
 * Returns pointer to the CLIP entry created
 *
 * Allocates a IPV6 CLIP entry to be used for a filter rule.
 */
struct clip_entry *cxgbe_clip_alloc(struct rte_eth_dev *dev, u32 *lip)
{
	return t4_clip_alloc(dev, lip, FILTER_TYPE_IPV6);
}

/**
 * Initialize CLIP Table
 */
struct clip_tbl *t4_init_clip_tbl(unsigned int clipt_start,
				  unsigned int clipt_end)
{
	unsigned int clipt_size;
	struct clip_tbl *ctbl;
	unsigned int i;

	if (clipt_start >= clipt_end)
		return NULL;

	clipt_size = clipt_end - clipt_start + 1;

	ctbl = t4_os_alloc(sizeof(*ctbl) +
			   clipt_size * sizeof(struct clip_entry));
	if (!ctbl)
		return NULL;

	ctbl->clipt_start = clipt_start;
	ctbl->clipt_size = clipt_size;

	t4_os_rwlock_init(&ctbl->lock);

	for (i = 0; i < ctbl->clipt_size; i++) {
		t4_os_lock_init(&ctbl->cl_list[i].lock);
		ctbl->cl_list[i].refcnt = 0;
	}

	return ctbl;
}

/**
 * Cleanup CLIP Table
 */
void t4_cleanup_clip_tbl(struct adapter *adap)
{
	if (adap->clipt)
		t4_os_free(adap->clipt);
}
