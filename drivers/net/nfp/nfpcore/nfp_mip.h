/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef __NFP_MIP_H__
#define __NFP_MIP_H__

#include "nfp_cpp.h"

/* "MIP\0" */
#define NFP_MIP_SIGNATURE        rte_cpu_to_le_32(0x0050494d)
#define NFP_MIP_VERSION          rte_cpu_to_le_32(1)

/* nfp_mip_entry_type */
#define NFP_MIP_TYPE_FWINFO      0x70000002

/* Each packed struct field is stored as Little Endian */
struct nfp_mip {
	rte_le32_t signature;
	rte_le32_t mip_version;

	rte_le32_t mip_size;
	rte_le32_t first_entry;

	rte_le32_t version;
	rte_le32_t buildnum;
	rte_le32_t buildtime;
	rte_le32_t loadtime;

	rte_le32_t symtab_addr;
	rte_le32_t symtab_size;
	rte_le32_t strtab_addr;
	rte_le32_t strtab_size;

	char name[16];
	char toolchain[32];
};

struct nfp_mip_entry {
	uint32_t type;
	uint32_t version;
	uint32_t offset_next;
};

/*
 * A key-value pair has no imposed limit, but it is recommended that
 * consumers only allocate enough memory for keys they plan to process and
 * skip over unused keys or ignore values that are longer than expected.
 *
 * For MIPv1, this will be preceded by struct nfp_mip_entry.
 * The entry size will be the size of key_value_strs, round to the next
 * 4-byte multiple. If entry size is 0, then there are no key-value strings
 * and it will not contain an empty string.
 *
 * The following keys are reserved and possibly set by the linker. The
 * convention is to start linker-set keys with a capital letter. Reserved
 * entries will be placed first in key_value_strs, user entries will be
 * placed next and be sorted alphabetically.
 * TypeId - Present if a user specified fw_typeid when linking.
 *
 * The following keys are reserved, but not used. Their values are in the
 * root MIP struct.
 */
struct nfp_mip_fwinfo_entry {
	/** The byte size of @p key_value_strs. */
	uint32_t kv_len;

	/** The number of key-value pairs in the following string. */
	uint32_t num;

	/**
	 * A series of NUL terminated strings, terminated by an extra
	 * NUL which is also the last byte of the entry, so an iterator
	 * can either check on size or when key[0] == '\0'.
	 */
	char key_value_strs[];
};

struct nfp_mip *nfp_mip_open(struct nfp_cpp *cpp);
void nfp_mip_close(struct nfp_mip *mip);

const char *nfp_mip_name(const struct nfp_mip *mip);
uint32_t nfp_mip_fw_version(const struct nfp_mip *mip);
void nfp_mip_symtab(const struct nfp_mip *mip, uint32_t *addr, uint32_t *size);
void nfp_mip_strtab(const struct nfp_mip *mip, uint32_t *addr, uint32_t *size);

#endif /* __NFP_MIP_H__ */
