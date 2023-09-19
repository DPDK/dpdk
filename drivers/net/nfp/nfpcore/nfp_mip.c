/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#include "nfp_mip.h"

#include <rte_byteorder.h>

#include "nfp_logs.h"
#include "nfp_nffw.h"

#define NFP_MIP_SIGNATURE        rte_cpu_to_le_32(0x0050494d)  /* "MIP\0" */
#define NFP_MIP_VERSION          rte_cpu_to_le_32(1)
#define NFP_MIP_MAX_OFFSET       (256 * 1024)

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
nfp_mip_try_read(struct nfp_cpp *cpp,
		uint32_t cpp_id,
		uint64_t addr,
		struct nfp_mip *mip)
{
	int ret;

	ret = nfp_cpp_read(cpp, cpp_id, addr, mip, sizeof(*mip));
	if (ret != sizeof(*mip)) {
		PMD_DRV_LOG(ERR, "Failed to read MIP data");
		return -EIO;
	}

	if (mip->signature != NFP_MIP_SIGNATURE) {
		PMD_DRV_LOG(ERR, "Incorrect MIP signature %#08x",
				rte_le_to_cpu_32(mip->signature));
		return -EINVAL;
	}

	if (mip->mip_version != NFP_MIP_VERSION) {
		PMD_DRV_LOG(ERR, "Unsupported MIP version %d",
				rte_le_to_cpu_32(mip->mip_version));
		return -EINVAL;
	}

	return 0;
}

/* Try to locate MIP using the resource table */
static int
nfp_mip_read_resource(struct nfp_cpp *cpp,
		struct nfp_mip *mip)
{
	int err;
	uint64_t addr;
	uint32_t cpp_id;
	struct nfp_nffw_info *nffw_info;

	nffw_info = nfp_nffw_info_open(cpp);
	if (nffw_info == NULL)
		return -ENODEV;

	err = nfp_nffw_info_mip_first(nffw_info, &cpp_id, &addr);
	if (err != 0)
		goto exit_close_nffw;

	err = nfp_mip_try_read(cpp, cpp_id, addr, mip);

exit_close_nffw:
	nfp_nffw_info_close(nffw_info);
	return err;
}

/**
 * Copy MIP structure from NFP device and return it. The returned
 * structure is handled internally by the library and should be
 * freed by calling @nfp_mip_close().
 *
 * @param cpp
 *   NFP CPP Handle
 *
 * @return
 *   Pointer to MIP, NULL on failure.
 */
struct nfp_mip *
nfp_mip_open(struct nfp_cpp *cpp)
{
	int err;
	struct nfp_mip *mip;

	mip = malloc(sizeof(*mip));
	if (mip == NULL)
		return NULL;

	err = nfp_mip_read_resource(cpp, mip);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Failed to read MIP resource");
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

/**
 * Get the address and size of the MIP symbol table.
 *
 * @param mip
 *   MIP handle
 * @param addr
 *   Location for NFP DDR address of MIP symbol table
 * @param size
 *   Location for size of MIP symbol table
 */
void
nfp_mip_symtab(const struct nfp_mip *mip,
		uint32_t *addr,
		uint32_t *size)
{
	*addr = rte_le_to_cpu_32(mip->symtab_addr);
	*size = rte_le_to_cpu_32(mip->symtab_size);
}

/**
 * Get the address and size of the MIP symbol name table.
 *
 * @param mip
 *   MIP handle
 * @param addr
 *   Location for NFP DDR address of MIP symbol name table
 * @param size
 *   Location for size of MIP symbol name table
 */
void
nfp_mip_strtab(const struct nfp_mip *mip,
		uint32_t *addr,
		uint32_t *size)
{
	*addr = rte_le_to_cpu_32(mip->strtab_addr);
	*size = rte_le_to_cpu_32(mip->strtab_size);
}
