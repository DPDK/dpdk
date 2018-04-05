/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <rte_byteorder.h>

#include "nfp_cpp.h"
#include "nfp_mip.h"
#include "nfp_nffw.h"

#define NFP_MIP_SIGNATURE	rte_cpu_to_le_32(0x0050494d)  /* "MIP\0" */
#define NFP_MIP_VERSION		rte_cpu_to_le_32(1)
#define NFP_MIP_MAX_OFFSET	(256 * 1024)

struct nfp_mip {
	uint32_t signature;
	uint32_t mip_version;
	uint32_t mip_size;
	uint32_t first_entry;

	uint32_t version;
	uint32_t buildnum;
	uint32_t buildtime;
	uint32_t loadtime;

	uint32_t symtab_addr;
	uint32_t symtab_size;
	uint32_t strtab_addr;
	uint32_t strtab_size;

	char name[16];
	char toolchain[32];
};

/* Read memory and check if it could be a valid MIP */
static int
nfp_mip_try_read(struct nfp_cpp *cpp, uint32_t cpp_id, uint64_t addr,
		 struct nfp_mip *mip)
{
	int ret;

	ret = nfp_cpp_read(cpp, cpp_id, addr, mip, sizeof(*mip));
	if (ret != sizeof(*mip)) {
		printf("Failed to read MIP data (%d, %zu)\n",
			ret, sizeof(*mip));
		return -EIO;
	}
	if (mip->signature != NFP_MIP_SIGNATURE) {
		printf("Incorrect MIP signature (0x%08x)\n",
			 rte_le_to_cpu_32(mip->signature));
		return -EINVAL;
	}
	if (mip->mip_version != NFP_MIP_VERSION) {
		printf("Unsupported MIP version (%d)\n",
			 rte_le_to_cpu_32(mip->mip_version));
		return -EINVAL;
	}

	return 0;
}

/* Try to locate MIP using the resource table */
static int
nfp_mip_read_resource(struct nfp_cpp *cpp, struct nfp_mip *mip)
{
	struct nfp_nffw_info *nffw_info;
	uint32_t cpp_id;
	uint64_t addr;
	int err;

	nffw_info = nfp_nffw_info_open(cpp);
	if (!nffw_info)
		return -ENODEV;

	err = nfp_nffw_info_mip_first(nffw_info, &cpp_id, &addr);
	if (err)
		goto exit_close_nffw;

	err = nfp_mip_try_read(cpp, cpp_id, addr, mip);
exit_close_nffw:
	nfp_nffw_info_close(nffw_info);
	return err;
}

/*
 * nfp_mip_open() - Get device MIP structure
 * @cpp:	NFP CPP Handle
 *
 * Copy MIP structure from NFP device and return it.  The returned
 * structure is handled internally by the library and should be
 * freed by calling nfp_mip_close().
 *
 * Return: pointer to mip, NULL on failure.
 */
struct nfp_mip *
nfp_mip_open(struct nfp_cpp *cpp)
{
	struct nfp_mip *mip;
	int err;

	mip = malloc(sizeof(*mip));
	if (!mip)
		return NULL;

	err = nfp_mip_read_resource(cpp, mip);
	if (err) {
		free(mip);
		return NULL;
	}

	mip->name[sizeof(mip->name) - 1] = 0;

	return mip;
}

void
nfp_mip_close(struct nfp_mip *mip)
{
	free(mip);
}

const char *
nfp_mip_name(const struct nfp_mip *mip)
{
	return mip->name;
}

/*
 * nfp_mip_symtab() - Get the address and size of the MIP symbol table
 * @mip:	MIP handle
 * @addr:	Location for NFP DDR address of MIP symbol table
 * @size:	Location for size of MIP symbol table
 */
void
nfp_mip_symtab(const struct nfp_mip *mip, uint32_t *addr, uint32_t *size)
{
	*addr = rte_le_to_cpu_32(mip->symtab_addr);
	*size = rte_le_to_cpu_32(mip->symtab_size);
}

/*
 * nfp_mip_strtab() - Get the address and size of the MIP symbol name table
 * @mip:	MIP handle
 * @addr:	Location for NFP DDR address of MIP symbol name table
 * @size:	Location for size of MIP symbol name table
 */
void
nfp_mip_strtab(const struct nfp_mip *mip, uint32_t *addr, uint32_t *size)
{
	*addr = rte_le_to_cpu_32(mip->strtab_addr);
	*size = rte_le_to_cpu_32(mip->strtab_size);
}
