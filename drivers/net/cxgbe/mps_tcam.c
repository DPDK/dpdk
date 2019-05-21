/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Chelsio Communications.
 * All rights reserved.
 */

#include "mps_tcam.h"

static inline bool
match_entry(struct mps_tcam_entry *entry, const u8 *eth_addr, const u8 *mask)
{
	if (!memcmp(eth_addr, entry->eth_addr, RTE_ETHER_ADDR_LEN) &&
	    !memcmp(mask, entry->mask, RTE_ETHER_ADDR_LEN))
		return true;
	return false;
}

static int cxgbe_update_free_idx(struct mpstcam_table *t)
{
	struct mps_tcam_entry *entry = t->entry;
	u16 i, next = t->free_idx + 1;

	if (entry[t->free_idx].state == MPS_ENTRY_UNUSED)
		/* You are already pointing to a free entry !! */
		return 0;

	/* loop, till we don't rollback to same index where we started */
	for (i = next; i != t->free_idx; i++) {
		if (i == t->size)
			/* rollback and search free entry from start */
			i = 0;

		if (entry[i].state == MPS_ENTRY_UNUSED) {
			t->free_idx = i;
			return 0;
		}
	}

	return -1;	/* table is full */
}

static struct mps_tcam_entry *
cxgbe_mpstcam_lookup(struct mpstcam_table *t, const u8 *eth_addr,
		     const u8 *mask)
{
	struct mps_tcam_entry *entry = t->entry;
	int i;

	if (!entry)
		return NULL;

	for (i = 0; i < t->size; i++) {
		if (entry[i].state == MPS_ENTRY_UNUSED)
			continue;	/* entry is not being used */
		if (match_entry(&entry[i], eth_addr, mask))
			return &entry[i];
	}

	return NULL;
}

int cxgbe_mpstcam_alloc(struct port_info *pi, const u8 *eth_addr,
			const u8 *mask)
{
	struct adapter *adap = pi->adapter;
	struct mpstcam_table *mpstcam = adap->mpstcam;
	struct mps_tcam_entry *entry;
	int ret;

	if (!adap->mpstcam) {
		dev_err(adap, "mpstcam table is not available\n");
		return -EOPNOTSUPP;
	}

	/* If entry already present, return it. */
	t4_os_write_lock(&mpstcam->lock);
	entry = cxgbe_mpstcam_lookup(adap->mpstcam, eth_addr, mask);
	if (entry) {
		rte_atomic32_add(&entry->refcnt, 1);
		t4_os_write_unlock(&mpstcam->lock);
		return entry->idx;
	}

	if (mpstcam->full) {
		t4_os_write_unlock(&mpstcam->lock);
		dev_err(adap, "mps-tcam table is full\n");
		return -ENOMEM;
	}

	ret = t4_alloc_raw_mac_filt(adap, pi->viid, eth_addr, mask,
				    mpstcam->free_idx, 0, pi->port_id, false);
	if (ret <= 0) {
		t4_os_write_unlock(&mpstcam->lock);
		return ret;
	}

	/* Fill in the new values */
	entry = &mpstcam->entry[ret];
	memcpy(entry->eth_addr, eth_addr, RTE_ETHER_ADDR_LEN);
	memcpy(entry->mask, mask, RTE_ETHER_ADDR_LEN);
	rte_atomic32_set(&entry->refcnt, 1);
	entry->state = MPS_ENTRY_USED;

	if (cxgbe_update_free_idx(mpstcam))
		mpstcam->full = true;

	t4_os_write_unlock(&mpstcam->lock);
	return ret;
}

int cxgbe_mpstcam_modify(struct port_info *pi, int idx, const u8 *addr)
{
	struct adapter *adap = pi->adapter;
	struct mpstcam_table *mpstcam = adap->mpstcam;
	struct mps_tcam_entry *entry;

	if (!mpstcam)
		return -EOPNOTSUPP;
	t4_os_write_lock(&mpstcam->lock);
	if (idx != -1 && idx >= mpstcam->size) {
		t4_os_write_unlock(&mpstcam->lock);
		return -EINVAL;
	}
	if (idx >= 0) {
		entry = &mpstcam->entry[idx];
		/* user wants to modify an existing entry.
		 * verify if entry exists
		 */
		if (entry->state != MPS_ENTRY_USED) {
			t4_os_write_unlock(&mpstcam->lock);
			return -EINVAL;
		}
	}

	idx = t4_change_mac(adap, adap->mbox, pi->viid, idx, addr, true, true);
	if (idx < 0) {
		t4_os_write_unlock(&mpstcam->lock);
		return idx;
	}

	/* idx can now be different from what user provided */
	entry = &mpstcam->entry[idx];
	memcpy(entry->eth_addr, addr, RTE_ETHER_ADDR_LEN);
	/* NOTE: we have considered the case that idx returned by t4_change_mac
	 * will be different from the user provided value only if user
	 * provided value is -1
	 */
	if (entry->state == MPS_ENTRY_UNUSED) {
		rte_atomic32_set(&entry->refcnt, 1);
		entry->state = MPS_ENTRY_USED;
	}

	if (cxgbe_update_free_idx(mpstcam))
		mpstcam->full = true;

	t4_os_write_unlock(&mpstcam->lock);
	return idx;
}

/**
 * hold appropriate locks while calling this.
 */
static inline void reset_mpstcam_entry(struct mps_tcam_entry *entry)
{
	memset(entry->eth_addr, 0, RTE_ETHER_ADDR_LEN);
	memset(entry->mask, 0, RTE_ETHER_ADDR_LEN);
	rte_atomic32_clear(&entry->refcnt);
	entry->state = MPS_ENTRY_UNUSED;
}

/**
 * ret < 0: fatal error
 * ret = 0: entry removed in h/w
 * ret > 0: updated refcount.
 */
int cxgbe_mpstcam_remove(struct port_info *pi, u16 idx)
{
	struct adapter *adap = pi->adapter;
	struct mpstcam_table *t = adap->mpstcam;
	struct mps_tcam_entry *entry;
	int ret;

	if (!t)
		return -EOPNOTSUPP;
	t4_os_write_lock(&t->lock);
	entry = &t->entry[idx];
	if (entry->state == MPS_ENTRY_UNUSED) {
		t4_os_write_unlock(&t->lock);
		return -EINVAL;
	}

	if (rte_atomic32_read(&entry->refcnt) == 1)
		ret = t4_free_raw_mac_filt(adap, pi->viid, entry->eth_addr,
					   entry->mask, idx, 1, pi->port_id,
					   false);
	else
		ret = rte_atomic32_sub_return(&entry->refcnt, 1);

	if (ret == 0) {
		reset_mpstcam_entry(entry);
		t->full = false;	/* We have atleast 1 free entry */
		cxgbe_update_free_idx(t);
	}

	t4_os_write_unlock(&t->lock);
	return ret;
}

struct mpstcam_table *t4_init_mpstcam(struct adapter *adap)
{
	struct mpstcam_table *t;
	int i;
	u16 size = adap->params.arch.mps_tcam_size;

	t =  t4_os_alloc(sizeof(*t) + size * sizeof(struct mps_tcam_entry));
	if (!t)
		return NULL;

	t4_os_rwlock_init(&t->lock);
	t->full = false;
	t->size = size;

	for (i = 0; i < size; i++) {
		reset_mpstcam_entry(&t->entry[i]);
		t->entry[i].mpstcam = t;
		t->entry[i].idx = i;
	}

	/* first entry is used by chip. this is overwritten only
	 * in t4_cleanup_mpstcam()
	 */
	t->entry[0].state = MPS_ENTRY_USED;
	t->free_idx = 1;

	return t;
}

void t4_cleanup_mpstcam(struct adapter *adap)
{
	if (adap->mpstcam)
		t4_os_free(adap->mpstcam);
}
