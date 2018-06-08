/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Chelsio Communications.
 * All rights reserved.
 */

#include "common.h"
#include "t4_regs.h"
#include "cxgbe_filter.h"

/**
 * Validate if the requested filter specification can be set by checking
 * if the requested features have been enabled
 */
int validate_filter(struct adapter *adapter, struct ch_filter_specification *fs)
{
	u32 fconf;

	/*
	 * Check for unconfigured fields being used.
	 */
	fconf = adapter->params.tp.vlan_pri_map;

#define S(_field) \
	(fs->val._field || fs->mask._field)
#define U(_mask, _field) \
	(!(fconf & (_mask)) && S(_field))

	if (U(F_ETHERTYPE, ethtype) || U(F_PROTOCOL, proto))
		return -EOPNOTSUPP;

#undef S
#undef U
	return 0;
}

/**
 * Check if entry already filled.
 */
bool is_filter_set(struct tid_info *t, int fidx, int family)
{
	bool result = FALSE;
	int i, max;

	/* IPv6 requires four slots and IPv4 requires only 1 slot.
	 * Ensure, there's enough slots available.
	 */
	max = family == FILTER_TYPE_IPV6 ? fidx + 3 : fidx;

	t4_os_lock(&t->ftid_lock);
	for (i = fidx; i <= max; i++) {
		if (rte_bitmap_get(t->ftid_bmap, i)) {
			result = TRUE;
			break;
		}
	}
	t4_os_unlock(&t->ftid_lock);
	return result;
}

/**
 * Allocate a available free entry
 */
int cxgbe_alloc_ftid(struct adapter *adap, unsigned int family)
{
	struct tid_info *t = &adap->tids;
	int pos;
	int size = t->nftids;

	t4_os_lock(&t->ftid_lock);
	if (family == FILTER_TYPE_IPV6)
		pos = cxgbe_bitmap_find_free_region(t->ftid_bmap, size, 4);
	else
		pos = cxgbe_find_first_zero_bit(t->ftid_bmap, size);
	t4_os_unlock(&t->ftid_lock);

	return pos < size ? pos : -1;
}
