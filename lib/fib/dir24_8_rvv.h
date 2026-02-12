/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2025 Institute of Software Chinese Academy of Sciences (ISCAS).
 */

#ifndef _DIR248_RVV_H_
#define _DIR248_RVV_H_

void
rte_dir24_8_vec_lookup_bulk_1b(void *p, const uint32_t *ips,
	uint64_t *next_hops, const unsigned int n);

void
rte_dir24_8_vec_lookup_bulk_2b(void *p, const uint32_t *ips,
	uint64_t *next_hops, const unsigned int n);

void
rte_dir24_8_vec_lookup_bulk_4b(void *p, const uint32_t *ips,
	uint64_t *next_hops, const unsigned int n);

void
rte_dir24_8_vec_lookup_bulk_8b(void *p, const uint32_t *ips,
	uint64_t *next_hops, const unsigned int n);

#endif /* _DIR248_RVV_H_ */
