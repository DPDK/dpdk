/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

/*
 * nfp_rtsym.c
 * Interface for accessing run-time symbol table
 */

#include "nfp_rtsym.h"

#include <rte_byteorder.h>

#include "nfp_logs.h"
#include "nfp_mip.h"
#include "nfp_target.h"
#include "nfp6000/nfp6000.h"

enum nfp_rtsym_type {
	NFP_RTSYM_TYPE_NONE,
	NFP_RTSYM_TYPE_OBJECT,
	NFP_RTSYM_TYPE_FUNCTION,
	NFP_RTSYM_TYPE_ABS,
};

#define NFP_RTSYM_TARGET_NONE           0
#define NFP_RTSYM_TARGET_LMEM           -1
#define NFP_RTSYM_TARGET_EMU_CACHE      -7

/* These need to match the linker */
#define SYM_TGT_LMEM            0
#define SYM_TGT_EMU_CACHE       0x17

struct nfp_rtsym_entry {
	uint8_t type;
	uint8_t target;
	uint8_t island;
	uint8_t addr_hi;
	uint32_t addr_lo;
	uint16_t name;
	uint8_t menum;
	uint8_t size_hi;
	uint32_t size_lo;
};

/*
 * Structure describing a run-time NFP symbol.
 *
 * The memory target of the symbol is generally the CPP target number and can be
 * used directly by the nfp_cpp API calls.  However, in some cases (i.e., for
 * local memory or control store) the target is encoded using a negative number.
 *
 * When the target type can not be used to fully describe the location of a
 * symbol the domain field is used to further specify the location (i.e., the
 * specific ME or island number).
 *
 * For ME target resources, 'domain' is an MEID.
 * For Island target resources, 'domain' is an island ID, with the one exception
 * of "sram" symbols for backward compatibility, which are viewed as global.
 */
struct nfp_rtsym {
	const char *name;  /**< Symbol name */
	uint64_t addr;     /**< Address in the domain/target's address space */
	uint64_t size;     /**< Size (in bytes) of the symbol */
	enum nfp_rtsym_type type; /**< NFP_RTSYM_TYPE_* of the symbol */
	int target;        /**< CPP target identifier, or NFP_RTSYM_TARGET_* */
	int domain;        /**< CPP target domain */
};

struct nfp_rtsym_table {
	struct nfp_cpp *cpp;
	int num;
	char *strtab;
	struct nfp_rtsym symtab[];
};

static int
nfp_meid(uint8_t island_id,
		uint8_t menum)
{
	return (island_id & 0x3F) == island_id && menum < 12 ?
		(island_id << 4) | (menum + 4) : -1;
}

static void
nfp_rtsym_sw_entry_init(struct nfp_rtsym_table *cache,
		uint32_t strtab_size,
		struct nfp_rtsym *sw,
		struct nfp_rtsym_entry *fw)
{
	sw->type = fw->type;
	sw->name = cache->strtab + rte_le_to_cpu_16(fw->name) % strtab_size;
	sw->addr = ((uint64_t)fw->addr_hi << 32) |
			rte_le_to_cpu_32(fw->addr_lo);
	sw->size = ((uint64_t)fw->size_hi << 32) |
			rte_le_to_cpu_32(fw->size_lo);

	switch (fw->target) {
	case SYM_TGT_LMEM:
		sw->target = NFP_RTSYM_TARGET_LMEM;
		break;
	case SYM_TGT_EMU_CACHE:
		sw->target = NFP_RTSYM_TARGET_EMU_CACHE;
		break;
	default:
		sw->target = fw->target;
		break;
	}

	if (fw->menum != 0xff)
		sw->domain = nfp_meid(fw->island, fw->menum);
	else if (fw->island != 0xff)
		sw->domain = fw->island;
	else
		sw->domain = -1;
}

static struct nfp_rtsym_table *
nfp_rtsym_table_read_real(struct nfp_cpp *cpp,
		const struct nfp_mip *mip)
{
	int n;
	int err;
	uint32_t size;
	uint32_t strtab_addr;
	uint32_t symtab_addr;
	uint32_t strtab_size;
	uint32_t symtab_size;
	struct nfp_rtsym_table *cache;
	struct nfp_rtsym_entry *rtsymtab;
	const uint32_t dram =
		NFP_CPP_ID(NFP_CPP_TARGET_MU, NFP_CPP_ACTION_RW, 0) |
		NFP_ISL_EMEM0;

	if (mip == NULL)
		return NULL;

	nfp_mip_strtab(mip, &strtab_addr, &strtab_size);
	nfp_mip_symtab(mip, &symtab_addr, &symtab_size);

	if (symtab_size == 0 || strtab_size == 0 || symtab_size % sizeof(*rtsymtab) != 0)
		return NULL;

	/* Align to 64 bits */
	symtab_size = RTE_ALIGN_CEIL(symtab_size, 8);
	strtab_size = RTE_ALIGN_CEIL(strtab_size, 8);

	rtsymtab = malloc(symtab_size);
	if (rtsymtab == NULL)
		return NULL;

	size = sizeof(*cache);
	size += symtab_size / sizeof(*rtsymtab) * sizeof(struct nfp_rtsym);
	size +=	strtab_size + 1;
	cache = malloc(size);
	if (cache == NULL)
		goto exit_free_rtsym_raw;

	cache->cpp = cpp;
	cache->num = symtab_size / sizeof(*rtsymtab);
	cache->strtab = (void *)&cache->symtab[cache->num];

	err = nfp_cpp_read(cpp, dram, symtab_addr, rtsymtab, symtab_size);
	if (err != (int)symtab_size)
		goto exit_free_cache;

	err = nfp_cpp_read(cpp, dram, strtab_addr, cache->strtab, strtab_size);
	if (err != (int)strtab_size)
		goto exit_free_cache;
	cache->strtab[strtab_size] = '\0';

	for (n = 0; n < cache->num; n++)
		nfp_rtsym_sw_entry_init(cache, strtab_size,
				&cache->symtab[n], &rtsymtab[n]);

	free(rtsymtab);

	return cache;

exit_free_cache:
	free(cache);
exit_free_rtsym_raw:
	free(rtsymtab);
	return NULL;
}

struct nfp_rtsym_table *
nfp_rtsym_table_read(struct nfp_cpp *cpp)
{
	struct nfp_mip *mip;
	struct nfp_rtsym_table *rtbl;

	mip = nfp_mip_open(cpp);
	rtbl = nfp_rtsym_table_read_real(cpp, mip);
	nfp_mip_close(mip);

	return rtbl;
}

/**
 * Get the number of RTSYM descriptors
 *
 * @param rtbl
 *   NFP RTSYM table
 *
 * @return
 *   Number of RTSYM descriptors
 */
int
nfp_rtsym_count(struct nfp_rtsym_table *rtbl)
{
	if (rtbl == NULL)
		return -EINVAL;

	return rtbl->num;
}

/**
 * Get the Nth RTSYM descriptor
 *
 * @param rtbl
 *   NFP RTSYM table
 * @param idx
 *   Index (0-based) of the RTSYM descriptor
 *
 * @return
 *   Const pointer to a struct nfp_rtsym descriptor, or NULL
 */
const struct nfp_rtsym *
nfp_rtsym_get(struct nfp_rtsym_table *rtbl,
		int idx)
{
	if (rtbl == NULL)
		return NULL;

	if (idx >= rtbl->num)
		return NULL;

	return &rtbl->symtab[idx];
}

/**
 * Return the RTSYM descriptor for a symbol name
 *
 * @param rtbl
 *   NFP RTSYM table
 * @param name
 *   Symbol name
 *
 * @return
 *   Const pointer to a struct nfp_rtsym descriptor, or NULL
 */
const struct nfp_rtsym *
nfp_rtsym_lookup(struct nfp_rtsym_table *rtbl,
		const char *name)
{
	int n;

	if (rtbl == NULL)
		return NULL;

	for (n = 0; n < rtbl->num; n++)
		if (strcmp(name, rtbl->symtab[n].name) == 0)
			return &rtbl->symtab[n];

	return NULL;
}

static uint64_t
nfp_rtsym_size(const struct nfp_rtsym *sym)
{
	switch (sym->type) {
	case NFP_RTSYM_TYPE_NONE:
		PMD_DRV_LOG(ERR, "The type of rtsym '%s' is NONE", sym->name);
		return 0;
	case NFP_RTSYM_TYPE_OBJECT:
		/* FALLTHROUGH */
	case NFP_RTSYM_TYPE_FUNCTION:
		return sym->size;
	case NFP_RTSYM_TYPE_ABS:
		return sizeof(uint64_t);
	default:
		PMD_DRV_LOG(ERR, "Unknown RTSYM type %u", sym->type);
		return 0;
	}
}

static int
nfp_rtsym_to_dest(struct nfp_cpp *cpp,
		const struct nfp_rtsym *sym,
		uint8_t action,
		uint8_t token,
		uint64_t offset,
		uint32_t *cpp_id,
		uint64_t *addr)
{
	if (sym->type != NFP_RTSYM_TYPE_OBJECT) {
		PMD_DRV_LOG(ERR, "rtsym '%s': direct access to non-object rtsym",
				sym->name);
		return -EINVAL;
	}

	*addr = sym->addr + offset;

	if (sym->target >= 0) {
		*cpp_id = NFP_CPP_ISLAND_ID(sym->target, action, token, sym->domain);
	} else if (sym->target == NFP_RTSYM_TARGET_EMU_CACHE) {
		int locality_off = nfp_cpp_mu_locality_lsb(cpp);

		*addr &= ~(NFP_MU_ADDR_ACCESS_TYPE_MASK << locality_off);
		*addr |= NFP_MU_ADDR_ACCESS_TYPE_DIRECT << locality_off;

		*cpp_id = NFP_CPP_ISLAND_ID(NFP_CPP_TARGET_MU, action, token,
				sym->domain);
	} else {
		PMD_DRV_LOG(ERR, "rtsym '%s': unhandled target encoding: %d",
				sym->name, sym->target);
		return -EINVAL;
	}

	return 0;
}

static int
nfp_rtsym_read_real(struct nfp_cpp *cpp,
		const struct nfp_rtsym *sym,
		uint8_t action,
		uint8_t token,
		uint64_t offset,
		void *buf,
		size_t len)
{
	int err;
	uint64_t addr;
	uint32_t cpp_id;
	size_t length = len;
	uint64_t sym_size = nfp_rtsym_size(sym);

	if (offset >= sym_size) {
		PMD_DRV_LOG(ERR, "rtsym '%s' read out of bounds", sym->name);
		return -ENXIO;
	}

	if (length > sym_size - offset)
		length = sym_size - offset;

	if (sym->type == NFP_RTSYM_TYPE_ABS) {
		union {
			uint64_t value_64;
			uint8_t value_8[8];
		} tmp;

		tmp.value_64 = sym->addr;
		memcpy(buf, &tmp.value_8[offset], length);

		return length;
	}

	err = nfp_rtsym_to_dest(cpp, sym, action, token, offset, &cpp_id, &addr);
	if (err != 0)
		return err;

	return nfp_cpp_read(cpp, cpp_id, addr, buf, length);
}

int
nfp_rtsym_read(struct nfp_cpp *cpp,
		const struct nfp_rtsym *sym,
		uint64_t offset,
		void *buf,
		size_t len)
{
	return nfp_rtsym_read_real(cpp, sym, NFP_CPP_ACTION_RW, 0, offset, buf, len);
}

static int
nfp_rtsym_readl_real(struct nfp_cpp *cpp,
		const struct nfp_rtsym *sym,
		uint8_t action,
		uint8_t token,
		uint64_t offset,
		uint32_t *value)
{
	int ret;
	uint64_t addr;
	uint32_t cpp_id;

	if (offset + 4 > nfp_rtsym_size(sym)) {
		PMD_DRV_LOG(ERR, "rtsym '%s': readl out of bounds", sym->name);
		return -ENXIO;
	}

	ret = nfp_rtsym_to_dest(cpp, sym, action, token, offset, &cpp_id, &addr);
	if (ret != 0)
		return ret;

	return nfp_cpp_readl(cpp, cpp_id, addr, value);
}

int
nfp_rtsym_readl(struct nfp_cpp *cpp,
		const struct nfp_rtsym *sym,
		uint64_t offset,
		uint32_t *value)
{
	return nfp_rtsym_readl_real(cpp, sym, NFP_CPP_ACTION_RW, 0, offset, value);
}

static int
nfp_rtsym_readq_real(struct nfp_cpp *cpp,
		const struct nfp_rtsym *sym,
		uint8_t action,
		uint8_t token,
		uint64_t offset,
		uint64_t *value)
{
	int ret;
	uint64_t addr;
	uint32_t cpp_id;

	if (offset + 8 > nfp_rtsym_size(sym)) {
		PMD_DRV_LOG(ERR, "rtsym '%s': readq out of bounds", sym->name);
		return -ENXIO;
	}

	if (sym->type == NFP_RTSYM_TYPE_ABS) {
		*value = sym->addr;
		return 0;
	}

	ret = nfp_rtsym_to_dest(cpp, sym, action, token, offset, &cpp_id, &addr);
	if (ret != 0)
		return ret;

	return nfp_cpp_readq(cpp, cpp_id, addr, value);
}

int
nfp_rtsym_readq(struct nfp_cpp *cpp,
		const struct nfp_rtsym *sym,
		uint64_t offset,
		uint64_t *value)
{
	return nfp_rtsym_readq_real(cpp, sym, NFP_CPP_ACTION_RW, 0, offset, value);
}

static int
nfp_rtsym_write_real(struct nfp_cpp *cpp,
		const struct nfp_rtsym *sym,
		uint8_t action,
		uint8_t token,
		uint64_t offset,
		void *buf,
		size_t len)
{
	int err;
	uint64_t addr;
	uint32_t cpp_id;
	size_t length = len;
	uint64_t sym_size = nfp_rtsym_size(sym);

	if (offset > sym_size) {
		PMD_DRV_LOG(ERR, "rtsym '%s' write out of bounds", sym->name);
		return -ENXIO;
	}

	if (length > sym_size - offset)
		length = sym_size - offset;

	err = nfp_rtsym_to_dest(cpp, sym, action, token, offset, &cpp_id, &addr);
	if (err != 0)
		return err;

	return nfp_cpp_write(cpp, cpp_id, addr, buf, length);
}

int
nfp_rtsym_write(struct nfp_cpp *cpp,
		const struct nfp_rtsym *sym,
		uint64_t offset,
		void *buf,
		size_t len)
{
	return nfp_rtsym_write_real(cpp, sym, NFP_CPP_ACTION_RW, 0, offset, buf, len);
}

static int
nfp_rtsym_writel_real(struct nfp_cpp *cpp,
		const struct nfp_rtsym *sym,
		uint8_t action,
		uint8_t token,
		uint64_t offset,
		uint32_t value)
{
	int err;
	uint64_t addr;
	uint32_t cpp_id;

	if (offset + 4 > nfp_rtsym_size(sym)) {
		PMD_DRV_LOG(ERR, "rtsym '%s' write out of bounds", sym->name);
		return -ENXIO;
	}

	err = nfp_rtsym_to_dest(cpp, sym, action, token, offset, &cpp_id, &addr);
	if (err != 0)
		return err;

	return nfp_cpp_writel(cpp, cpp_id, addr, value);
}

int
nfp_rtsym_writel(struct nfp_cpp *cpp,
		const struct nfp_rtsym *sym,
		uint64_t offset,
		uint32_t value)
{
	return nfp_rtsym_writel_real(cpp, sym, NFP_CPP_ACTION_RW, 0, offset, value);
}

static int
nfp_rtsym_writeq_real(struct nfp_cpp *cpp,
		const struct nfp_rtsym *sym,
		uint8_t action,
		uint8_t token,
		uint64_t offset,
		uint64_t value)
{
	int err;
	uint64_t addr;
	uint32_t cpp_id;

	if (offset + 8 > nfp_rtsym_size(sym)) {
		PMD_DRV_LOG(ERR, "rtsym '%s' write out of bounds", sym->name);
		return -ENXIO;
	}

	err = nfp_rtsym_to_dest(cpp, sym, action, token, offset, &cpp_id, &addr);
	if (err != 0)
		return err;

	return nfp_cpp_writeq(cpp, cpp_id, addr, value);
}

int
nfp_rtsym_writeq(struct nfp_cpp *cpp,
		const struct nfp_rtsym *sym,
		uint64_t offset,
		uint64_t value)
{
	return nfp_rtsym_writeq_real(cpp, sym, NFP_CPP_ACTION_RW, 0, offset, value);
}

/**
 * Read a simple unsigned scalar value from symbol
 *
 * Lookup a symbol, map, read it and return it's value. Value of the symbol
 * will be interpreted as a simple little-endian unsigned value. Symbol can
 * be 4 or 8 bytes in size.
 *
 * @param rtbl
 *   NFP RTSYM table
 * @param name
 *   Symbol name
 * @param error
 *   Pointer to error code (optional)
 *
 * @return
 *   Value read, on error sets the error and returns ~0ULL.
 */
uint64_t
nfp_rtsym_read_le(struct nfp_rtsym_table *rtbl,
		const char *name,
		int *error)
{
	int err;
	uint64_t val;
	uint32_t val32;
	const struct nfp_rtsym *sym;

	sym = nfp_rtsym_lookup(rtbl, name);
	if (sym == NULL) {
		err = -ENOENT;
		goto exit;
	}

	switch (sym->size) {
	case 4:
		err = nfp_rtsym_readl(rtbl->cpp, sym, 0, &val32);
		val = val32;
		break;
	case 8:
		err = nfp_rtsym_readq(rtbl->cpp, sym, 0, &val);
		break;
	default:
		PMD_DRV_LOG(ERR, "rtsym '%s' unsupported size: %#lx",
				name, sym->size);
		err = -EINVAL;
		break;
	}

exit:
	if (error != NULL)
		*error = err;

	if (err != 0)
		return ~0ULL;

	return val;
}

/**
 * Write an unsigned scalar value to a symbol
 *
 * Lookup a symbol and write a value to it. Symbol can be 4 or 8 bytes in size.
 * If 4 bytes then the lower 32-bits of 'value' are used. Value will be
 * written as simple little-endian unsigned value.
 *
 * @param rtbl
 *   NFP RTSYM table
 * @param name
 *   Symbol name
 * @param value
 *   Value to write
 *
 * @return
 *   0 on success or error code.
 */
int
nfp_rtsym_write_le(struct nfp_rtsym_table *rtbl,
		const char *name,
		uint64_t value)
{
	int err;
	uint64_t sym_size;
	const struct nfp_rtsym *sym;

	sym = nfp_rtsym_lookup(rtbl, name);
	if (sym == NULL)
		return -ENOENT;

	sym_size = nfp_rtsym_size(sym);
	switch (sym_size) {
	case 4:
		err = nfp_rtsym_writel(rtbl->cpp, sym, 0, value);
		break;
	case 8:
		err = nfp_rtsym_writeq(rtbl->cpp, sym, 0, value);
		break;
	default:
		PMD_DRV_LOG(ERR, "rtsym '%s' unsupported size: %#lx",
				name, sym_size);
		err = -EINVAL;
		break;
	}

	return err;
}

uint8_t *
nfp_rtsym_map(struct nfp_rtsym_table *rtbl,
		const char *name,
		uint32_t min_size,
		struct nfp_cpp_area **area)
{
	int ret;
	uint8_t *mem;
	uint64_t addr;
	uint32_t cpp_id;
	const struct nfp_rtsym *sym;

	sym = nfp_rtsym_lookup(rtbl, name);
	if (sym == NULL) {
		PMD_DRV_LOG(ERR, "Symbol lookup fails for %s", name);
		return NULL;
	}

	ret = nfp_rtsym_to_dest(rtbl->cpp, sym, NFP_CPP_ACTION_RW, 0, 0,
			&cpp_id, &addr);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "rtsym '%s': mapping failed", name);
		return NULL;
	}

	if (sym->size < min_size) {
		PMD_DRV_LOG(ERR, "Symbol %s too small (%" PRIu64 " < %u)", name,
				sym->size, min_size);
		return NULL;
	}

	mem = nfp_cpp_map_area(rtbl->cpp, cpp_id, addr, sym->size, area);
	if (mem == NULL) {
		PMD_DRV_LOG(ERR, "Failed to map symbol %s", name);
		return NULL;
	}

	return mem;
}
