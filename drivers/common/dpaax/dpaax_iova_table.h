/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 NXP
 */

#ifndef _DPAAX_IOVA_TABLE_H_
#define _DPAAX_IOVA_TABLE_H_

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <glob.h>
#include <errno.h>
#include <arpa/inet.h>

#include <rte_eal.h>
#include <rte_branch_prediction.h>
#include <rte_memory.h>
#include <rte_malloc.h>

struct dpaax_iovat_element {
	phys_addr_t start; /**< Start address of block of physical pages */
	size_t len; /**< Difference of end-start for quick access */
	uint64_t *pages; /**< VA for each physical page in this block */
};

struct dpaax_iova_table {
	unsigned int count; /**< No. of blocks of contiguous physical pages */
	struct dpaax_iovat_element entries[0];
};

/* Pointer to the table, which is common for DPAA/DPAA2 and only a single
 * instance is required across net/crypto/event drivers. This table is
 * populated iff devices are found on the bus.
 */
extern struct dpaax_iova_table *dpaax_iova_table_p;

/* Device tree file for memory layout is named 'memory@<addr>' where the 'addr'
 * is SoC dependent, or even Uboot fixup dependent.
 */
#define MEM_NODE_PATH_GLOB "/proc/device-tree/memory[@0-9]*/reg"
/* For Virtual Machines memory node is at different path (below) */
#define MEM_NODE_PATH_GLOB_VM "/proc/device-tree/memory/reg"
/* Device file should be multiple of 16 bytes, each containing 8 byte of addr
 * and its length. Assuming max of 5 entries.
 */
#define MEM_NODE_FILE_LEN ((16 * 5) + 1)

/* Table is made up of DPAAX_MEM_SPLIT elements for each contiguous zone. This
 * helps avoid separate handling for cases where more than one size of hugepage
 * is supported.
 */
#define DPAAX_MEM_SPLIT (1<<21)
#define DPAAX_MEM_SPLIT_MASK ~(DPAAX_MEM_SPLIT - 1) /**< Floor aligned */
#define DPAAX_MEM_SPLIT_MASK_OFF (DPAAX_MEM_SPLIT - 1) /**< Offset */

/* APIs exposed */
int dpaax_iova_table_populate(void);
void dpaax_iova_table_depopulate(void);
int dpaax_iova_table_update(phys_addr_t paddr, void *vaddr, size_t length);
void dpaax_iova_table_dump(void);

static inline void *dpaax_iova_table_get_va(phys_addr_t paddr) __attribute__((hot));

static inline void *
dpaax_iova_table_get_va(phys_addr_t paddr) {
	unsigned int i = 0, index;
	void *vaddr = 0;
	phys_addr_t paddr_align = paddr & DPAAX_MEM_SPLIT_MASK;
	size_t offset = paddr & DPAAX_MEM_SPLIT_MASK_OFF;
	struct dpaax_iovat_element *entry;

	if (unlikely(dpaax_iova_table_p == NULL))
		return NULL;

	entry = dpaax_iova_table_p->entries;

	do {
		if (unlikely(i > dpaax_iova_table_p->count))
			break;

		if (paddr_align < entry[i].start) {
			/* Incorrect paddr; Not in memory range */
			return NULL;
		}

		if (paddr_align > (entry[i].start + entry[i].len)) {
			i++;
			continue;
		}

		/* paddr > entry->start && paddr <= entry->(start+len) */
		index = (paddr_align - entry[i].start)/DPAAX_MEM_SPLIT;
		vaddr = (void *)((uintptr_t)entry[i].pages[index] + offset);
		break;
	} while (1);

	return vaddr;
}

#endif /* _DPAAX_IOVA_TABLE_H_ */
