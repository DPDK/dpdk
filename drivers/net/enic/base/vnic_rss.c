/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#include "enic_compat.h"
#include "vnic_rss.h"

void vnic_set_rss_key(union vnic_rss_key *rss_key, u8 *key)
{
	u32 i;
	u32 *p;
	u16 *q;

	for (i = 0; i < 4; ++i) {
		p = (u32 *)(key + (10 * i));
		iowrite32(*p++, &rss_key->key[i].b[0]);
		iowrite32(*p++, &rss_key->key[i].b[4]);
		q = (u16 *)p;
		iowrite32(*q, &rss_key->key[i].b[8]);
	}
}

