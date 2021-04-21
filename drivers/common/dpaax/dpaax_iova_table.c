/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 NXP
 */

#include <rte_memory.h>

#include "dpaax_iova_table.h"
#include "dpaax_logs.h"

/* Global table reference */
struct dpaax_iova_table *dpaax_iova_table_p;

static int dpaax_handle_memevents(void);

/* A structure representing the device-tree node available in /proc/device-tree.
 */
struct reg_node {
	phys_addr_t addr;
	size_t len;
};

/* A ntohll equivalent routine
 * XXX: This is only applicable for 64 bit environment.
 */
static void
rotate_8(unsigned char *arr)
{
	uint32_t temp;
	uint32_t *first_half;
	uint32_t *second_half;

	first_half = (uint32_t *)(arr);
	second_half = (uint32_t *)(arr + 4);

	temp = *first_half;
	*first_half = *second_half;
	*second_half = temp;

	*first_half = ntohl(*first_half);
	*second_half = ntohl(*second_half);
}

/* read_memory_nodes
 * Memory layout for DPAAx platforms (LS1043, LS1046, LS1088, LS2088, LX2160)
 * are populated by Uboot and available in device tree:
 * /proc/device-tree/memory@<address>/reg <= register.
 * Entries are of the form:
 *  (<8 byte start addr><8 byte length>)(..more similar blocks of start,len>)..
 *
 * @param count
 *    OUT populate number of entries found in memory node
 * @return
 *    Pointer to array of reg_node elements, count size
 */
static struct reg_node *
read_memory_node(unsigned int *count)
{
	int fd, ret, i;
	unsigned int j;
	glob_t result = {0};
	struct stat statbuf = {0};
	char file_data[MEM_NODE_FILE_LEN];
	struct reg_node *nodes = NULL;

	*count = 0;

	ret = glob(MEM_NODE_PATH_GLOB, 0, NULL, &result);
	if (ret != 0)
		ret = glob(MEM_NODE_PATH_GLOB_VM, 0, NULL, &result);

	if (ret != 0) {
		DPAAX_DEBUG("Unable to glob device-tree memory node (err: %d)",
			ret);
		goto out;
	}

	if (result.gl_pathc != 1) {
		/* Either more than one memory@<addr> node found, or none.
		 * In either case, cannot work ahead.
		 */
		DPAAX_DEBUG("Found (%zu) entries in device-tree. Not supported!",
			    result.gl_pathc);
		goto out;
	}

	DPAAX_DEBUG("Opening and parsing device-tree node: (%s)",
		    result.gl_pathv[0]);
	fd = open(result.gl_pathv[0], O_RDONLY);
	if (fd < 0) {
		DPAAX_DEBUG("Unable to open the device-tree node: (%s)(fd=%d)",
			    MEM_NODE_PATH_GLOB, fd);
		goto cleanup;
	}

	/* Stat to get the file size */
	ret = fstat(fd, &statbuf);
	if (ret != 0) {
		DPAAX_DEBUG("Unable to get device-tree memory node size.");
		goto cleanup;
	}

	DPAAX_DEBUG("Size of device-tree mem node: %" PRIu64, statbuf.st_size);
	if (statbuf.st_size > MEM_NODE_FILE_LEN) {
		DPAAX_DEBUG("More memory nodes available than assumed.");
		DPAAX_DEBUG("System may not work properly!");
	}

	ret = read(fd, file_data, statbuf.st_size > MEM_NODE_FILE_LEN ?
				  MEM_NODE_FILE_LEN : statbuf.st_size);
	if (ret <= 0) {
		DPAAX_DEBUG("Unable to read device-tree memory node: (%d)",
			    ret);
		goto cleanup;
	}

	/* The reg node should be multiple of 16 bytes, 8 bytes each for addr
	 * and len.
	 */
	*count = (statbuf.st_size / 16);
	if ((*count) <= 0 || (statbuf.st_size % 16 != 0)) {
		DPAAX_DEBUG("Invalid memory node values or count. (size=%" PRIu64 ")",
			    statbuf.st_size);
		goto cleanup;
	}

	/* each entry is of 16 bytes, and size/16 is total count of entries */
	nodes = malloc(sizeof(struct reg_node) * (*count));
	if (!nodes) {
		DPAAX_DEBUG("Failure in allocating working memory.");
		goto cleanup;
	}
	memset(nodes, 0, sizeof(struct reg_node) * (*count));

	for (i = 0, j = 0; i < (statbuf.st_size) && j < (*count); i += 16, j++) {
		memcpy(&nodes[j], file_data + i, 16);
		/* Rotate (ntohl) each 8 byte entry */
		rotate_8((unsigned char *)(&(nodes[j].addr)));
		rotate_8((unsigned char *)(&(nodes[j].len)));
	}

	DPAAX_DEBUG("Device-tree memory node data:");
	do {
		DPAAX_DEBUG("    %08" PRIx64 " %08zu",
			    nodes[j].addr, nodes[j].len);
	} while (--j);

cleanup:
	close(fd);
	globfree(&result);
out:
	return nodes;
}

int
dpaax_iova_table_populate(void)
{
	int ret;
	unsigned int i, node_count;
	size_t tot_memory_size, total_table_size;
	struct reg_node *nodes;
	struct dpaax_iovat_element *entry;

	/* dpaax_iova_table_p is a singleton - only one instance should be
	 * created.
	 */
	if (dpaax_iova_table_p) {
		DPAAX_DEBUG("Multiple allocation attempt for IOVA Table (%p)",
			    dpaax_iova_table_p);
		/* This can be an error case as well - some path not cleaning
		 * up table - but, for now, it is assumed that if IOVA Table
		 * pointer is valid, table is allocated.
		 */
		return 0;
	}

	nodes = read_memory_node(&node_count);
	if (nodes == NULL) {
		DPAAX_WARN("PA->VA translation not available;");
		DPAAX_WARN("Expect performance impact.");
		return -1;
	}

	tot_memory_size = 0;
	for (i = 0; i < node_count; i++)
		tot_memory_size += nodes[i].len;

	DPAAX_DEBUG("Total available PA memory size: %zu", tot_memory_size);

	/* Total table size = meta data + tot_memory_size/8 */
	total_table_size = sizeof(struct dpaax_iova_table) +
			   (sizeof(struct dpaax_iovat_element) * node_count) +
			   ((tot_memory_size / DPAAX_MEM_SPLIT) * sizeof(uint64_t));

	/* TODO: This memory doesn't need to shared but needs to be always
	 * pinned to RAM (no swap out) - using hugepage rather than malloc
	 */
	dpaax_iova_table_p = rte_zmalloc(NULL, total_table_size, 0);
	if (dpaax_iova_table_p == NULL) {
		DPAAX_WARN("Unable to allocate memory for PA->VA Table;");
		DPAAX_WARN("PA->VA translation not available;");
		DPAAX_WARN("Expect performance impact.");
		free(nodes);
		return -1;
	}

	/* Initialize table */
	dpaax_iova_table_p->count = node_count;
	entry = dpaax_iova_table_p->entries;

	DPAAX_DEBUG("IOVA Table entries: (entry start = %p)", (void *)entry);
	DPAAX_DEBUG("\t(entry),(start),(len),(next)");

	for (i = 0; i < node_count; i++) {
		/* dpaax_iova_table_p
		 * |   dpaax_iova_table_p->entries
		 * |      |
		 * |      |
		 * V      V
		 * +------+------+-------+---+----------+---------+---
		 * |iova_ |entry | entry |   | pages    | pages   |
		 * |table | 1    |  2    |...| entry 1  | entry2  |
		 * +-----'+.-----+-------+---+;---------+;--------+---
		 *         \      \          /          /
		 *          `~~~~~~|~~~~~>pages        /
		 *                  \                 /
		 *                   `~~~~~~~~~~~>pages
		 */
		entry[i].start = nodes[i].addr;
		entry[i].len = nodes[i].len;
		if (i > 0)
			entry[i].pages = entry[i-1].pages +
				((entry[i-1].len/DPAAX_MEM_SPLIT));
		else
			entry[i].pages = (uint64_t *)((unsigned char *)entry +
					 (sizeof(struct dpaax_iovat_element) *
					 node_count));

		DPAAX_DEBUG("\t(%u),(%8"PRIx64"),(%8zu),(%8p)",
			    i, entry[i].start, entry[i].len, entry[i].pages);
	}

	/* Release memory associated with nodes array - not required now */
	free(nodes);

	DPAAX_DEBUG("Adding mem-event handler");
	ret = dpaax_handle_memevents();
	if (ret) {
		DPAAX_ERR("Unable to add mem-event handler");
		DPAAX_WARN("Cases with non-buffer pool mem won't work!");
	}

	return 0;
}

void
dpaax_iova_table_depopulate(void)
{
	if (dpaax_iova_table_p == NULL)
		return;

	rte_free(dpaax_iova_table_p->entries);
	dpaax_iova_table_p = NULL;

	DPAAX_DEBUG("IOVA Table cleanedup");
}

int
dpaax_iova_table_update(phys_addr_t paddr, void *vaddr, size_t length)
{
	int found = 0;
	unsigned int i;
	size_t req_length = length, e_offset;
	struct dpaax_iovat_element *entry;
	uintptr_t align_vaddr;
	phys_addr_t align_paddr;

	if (unlikely(dpaax_iova_table_p == NULL))
		return -1;

	align_paddr = paddr & DPAAX_MEM_SPLIT_MASK;
	align_vaddr = ((uintptr_t)vaddr & DPAAX_MEM_SPLIT_MASK);

	/* Check if paddr is available in table */
	entry = dpaax_iova_table_p->entries;
	for (i = 0; i < dpaax_iova_table_p->count; i++) {
		if (align_paddr < entry[i].start) {
			/* Address lower than start, but not found in previous
			 * iteration shouldn't exist.
			 */
			DPAAX_ERR("Add: Incorrect entry for PA->VA Table"
				  "(%"PRIu64")", paddr);
			DPAAX_ERR("Add: Lowest address: %"PRIu64"",
				  entry[i].start);
			return -1;
		}

		if (align_paddr > (entry[i].start + entry[i].len))
			continue;

		/* align_paddr >= start && align_paddr < (start + len) */
		found = 1;

		do {
			e_offset = ((align_paddr - entry[i].start) / DPAAX_MEM_SPLIT);
			/* TODO: Whatif something already exists at this
			 * location - is that an error? For now, ignoring the
			 * case.
			 */
			entry[i].pages[e_offset] = align_vaddr;
#ifdef RTE_COMMON_DPAAX_DEBUG
			DPAAX_DEBUG("Added: vaddr=%zu for Phy:%"PRIu64" at %zu"
				    " remaining len %zu", align_vaddr,
				    align_paddr, e_offset, req_length);
#endif
			/* Incoming request can be larger than the
			 * DPAAX_MEM_SPLIT size - in which case, multiple
			 * entries in entry->pages[] are filled up.
			 */
			if (req_length <= DPAAX_MEM_SPLIT)
				break;
			align_paddr += DPAAX_MEM_SPLIT;
			align_vaddr += DPAAX_MEM_SPLIT;
			req_length -= DPAAX_MEM_SPLIT;
		} while (1);

		break;
	}

	if (!found) {
		/* There might be case where the incoming physical address is
		 * beyond the address discovered in the memory node of
		 * device-tree. Specially if some malloc'd area is used by EAL
		 * and the memevent handlers passes that across. But, this is
		 * not necessarily an error.
		 */
		DPAAX_DEBUG("Add: Unable to find slot for vaddr:(%p),"
			    " phy(%"PRIu64")",
			    vaddr, paddr);
		return -1;
	}
#ifdef RTE_COMMON_DPAAX_DEBUG
	DPAAX_DEBUG("Add: Found slot at (%"PRIu64")[(%zu)] for vaddr:(%p),"
		    " phy(%"PRIu64"), len(%zu)", entry[i].start, e_offset,
		    vaddr, paddr, length);
#endif
	return 0;
}

/* dpaax_iova_table_dump
 * Dump the table, with its entries, on screen. Only works in Debug Mode
 * Not for weak hearted - the tables can get quite large
 */
void
dpaax_iova_table_dump(void)
{
	unsigned int i, j;
	struct dpaax_iovat_element *entry;

	/* In case DEBUG is not enabled, some 'if' conditions might misbehave
	 * as they have nothing else in them  except a DPAAX_DEBUG() which if
	 * tuned out would leave 'if' naked.
	 */
	if (rte_log_get_global_level() < RTE_LOG_DEBUG) {
		DPAAX_ERR("Set log level to Debug for PA->Table dump!");
		return;
	}

	DPAAX_DEBUG(" === Start of PA->VA Translation Table ===");
	if (dpaax_iova_table_p == NULL) {
		DPAAX_DEBUG("\tNULL");
		return;
	}

	entry = dpaax_iova_table_p->entries;
	for (i = 0; i < dpaax_iova_table_p->count; i++) {
		DPAAX_DEBUG("\t(%16i),(%16"PRIu64"),(%16zu),(%16p)",
			    i, entry[i].start, entry[i].len, entry[i].pages);
		DPAAX_DEBUG("\t\t          (PA),          (VA)");
		for (j = 0; j < (entry->len/DPAAX_MEM_SPLIT); j++) {
			if (entry[i].pages[j] == 0)
				continue;
			DPAAX_DEBUG("\t\t(%16"PRIx64"),(%16"PRIx64")",
				    (entry[i].start + (j * sizeof(uint64_t))),
				    entry[i].pages[j]);
		}
	}
	DPAAX_DEBUG(" === End of PA->VA Translation Table ===");
}

static void
dpaax_memevent_cb(enum rte_mem_event type, const void *addr, size_t len,
		  void *arg __rte_unused)
{
	struct rte_memseg_list *msl;
	struct rte_memseg *ms;
	size_t cur_len = 0, map_len = 0;
	phys_addr_t phys_addr;
	void *virt_addr;
	int ret;

	DPAAX_DEBUG("Called with addr=%p, len=%zu", addr, len);

	msl = rte_mem_virt2memseg_list(addr);

	while (cur_len < len) {
		const void *va = RTE_PTR_ADD(addr, cur_len);

		ms = rte_mem_virt2memseg(va, msl);
		phys_addr = rte_mem_virt2phy(ms->addr);
		virt_addr = ms->addr;
		map_len = ms->len;
#ifdef RTE_COMMON_DPAAX_DEBUG
		DPAAX_DEBUG("Request for %s, va=%p, virt_addr=%p,"
			    "iova=%"PRIu64", map_len=%zu",
			    type == RTE_MEM_EVENT_ALLOC ?
			    "alloc" : "dealloc",
			    va, virt_addr, phys_addr, map_len);
#endif
		if (type == RTE_MEM_EVENT_ALLOC)
			ret = dpaax_iova_table_update(phys_addr, virt_addr,
						      map_len);
		else
			/* In case of mem_events for MEM_EVENT_FREE, complete
			 * hugepage is released and its PA entry is set to 0.
			 */
			ret = dpaax_iova_table_update(phys_addr, 0, map_len);

		if (ret != 0) {
			DPAAX_DEBUG("PA-Table entry update failed. "
				    "Map=%d, addr=%p, len=%zu, err:(%d)",
				    type, va, map_len, ret);
			return;
		}

		cur_len += map_len;
	}
}

static int
dpaax_memevent_walk_memsegs(const struct rte_memseg_list *msl __rte_unused,
			    const struct rte_memseg *ms, size_t len,
			    void *arg __rte_unused)
{
	DPAAX_DEBUG("Walking for %p (pa=%"PRIu64") and len %zu",
		    ms->addr, ms->iova, len);
	dpaax_iova_table_update(rte_mem_virt2phy(ms->addr), ms->addr, len);
	return 0;
}

static int
dpaax_handle_memevents(void)
{
	/* First, walk through all memsegs and pin them, before installing
	 * handler. This assures that all memseg which have already been
	 * identified/allocated by EAL, are already part of PA->VA Table. This
	 * is especially for cases where application allocates memory before
	 * the EAL or this is an externally allocated memory passed to EAL.
	 */
	rte_memseg_contig_walk_thread_unsafe(dpaax_memevent_walk_memsegs, NULL);

	return rte_mem_event_callback_register("dpaax_memevents_cb",
					       dpaax_memevent_cb, NULL);
}

RTE_LOG_REGISTER(dpaax_logger, pmd.common.dpaax, ERR);
