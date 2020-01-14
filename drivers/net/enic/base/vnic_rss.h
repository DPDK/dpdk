/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#ifndef _VNIC_RSS_H_
#define _VNIC_RSS_H_

/* RSS key array */
union vnic_rss_key {
	struct {
		uint8_t b[10];
		uint8_t b_pad[6];
	} key[4];
	uint64_t raw[8];
};

/* RSS cpu array */
union vnic_rss_cpu {
	struct {
		uint8_t b[4];
		uint8_t b_pad[4];
	} cpu[32];
	uint64_t raw[32];
};

#endif /* _VNIC_RSS_H_ */
