/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#include "nfp_cpp.h"
#include "nfp_nffw.h"
#include "nfp_mip.h"
#include "nfp6000/nfp6000.h"
#include "nfp_resource.h"

/*
 * flg_info_version = flags[0]<27:16>
 * This is a small version counter intended only to detect if the current
 * implementation can read the current struct. Struct changes should be very
 * rare and as such a 12-bit counter should cover large spans of time. By the
 * time it wraps around, we don't expect to have 4096 versions of this struct
 * to be in use at the same time.
 */
static uint32_t
nffw_res_info_version_get(const struct nfp_nffw_info_data *res)
{
	return (res->flags[0] >> 16) & 0xfff;
}

/* flg_init = flags[0]<0> */
static uint32_t
nffw_res_flg_init_get(const struct nfp_nffw_info_data *res)
{
	return (res->flags[0] >> 0) & 1;
}

/* loaded = loaded__mu_da__mip_off_hi<31:31> */
static uint32_t
nffw_fwinfo_loaded_get(const struct nffw_fwinfo *fi)
{
	return (fi->loaded__mu_da__mip_off_hi >> 31) & 1;
}

/* mip_cppid = mip_cppid */
static uint32_t
nffw_fwinfo_mip_cppid_get(const struct nffw_fwinfo *fi)
{
	return fi->mip_cppid;
}

/* loaded = loaded__mu_da__mip_off_hi<8:8> */
static uint32_t
nffw_fwinfo_mip_mu_da_get(const struct nffw_fwinfo *fi)
{
	return (fi->loaded__mu_da__mip_off_hi >> 8) & 1;
}

/* mip_offset = (loaded__mu_da__mip_off_hi<7:0> << 8) | mip_offset_lo */
static uint64_t
nffw_fwinfo_mip_offset_get(const struct nffw_fwinfo *fi)
{
	uint64_t mip_off_hi = fi->loaded__mu_da__mip_off_hi;

	return (mip_off_hi & 0xFF) << 32 | fi->mip_offset_lo;
}

#define NFP_IMB_TGTADDRESSMODECFG_MODE_of(_x)		(((_x) >> 13) & 0x7)
#define NFP_IMB_TGTADDRESSMODECFG_ADDRMODE		BIT(12)
#define   NFP_IMB_TGTADDRESSMODECFG_ADDRMODE_32_BIT	0
#define   NFP_IMB_TGTADDRESSMODECFG_ADDRMODE_40_BIT	BIT(12)

static int
nfp_mip_mu_locality_lsb(struct nfp_cpp *cpp)
{
	unsigned int mode, addr40;
	uint32_t xpbaddr, imbcppat;
	int err;

	/* Hardcoded XPB IMB Base, island 0 */
	xpbaddr = 0x000a0000 + NFP_CPP_TARGET_MU * 4;
	err = nfp_xpb_readl(cpp, xpbaddr, &imbcppat);
	if (err < 0)
		return err;

	mode = NFP_IMB_TGTADDRESSMODECFG_MODE_of(imbcppat);
	addr40 = !!(imbcppat & NFP_IMB_TGTADDRESSMODECFG_ADDRMODE);

	return nfp_cppat_mu_locality_lsb(mode, addr40);
}

static unsigned int
nffw_res_fwinfos(struct nfp_nffw_info_data *fwinf, struct nffw_fwinfo **arr)
{
	/*
	 * For the this code, version 0 is most likely to be version 1 in this
	 * case. Since the kernel driver does not take responsibility for
	 * initialising the nfp.nffw resource, any previous code (CA firmware or
	 * userspace) that left the version 0 and did set the init flag is going
	 * to be version 1.
	 */
	switch (nffw_res_info_version_get(fwinf)) {
	case 0:
	case 1:
		*arr = &fwinf->info.v1.fwinfo[0];
		return NFFW_FWINFO_CNT_V1;
	case 2:
		*arr = &fwinf->info.v2.fwinfo[0];
		return NFFW_FWINFO_CNT_V2;
	default:
		*arr = NULL;
		return 0;
	}
}

/*
 * nfp_nffw_info_open() - Acquire the lock on the NFFW table
 * @cpp:	NFP CPP handle
 *
 * Return: 0, or -ERRNO
 */
struct nfp_nffw_info *
nfp_nffw_info_open(struct nfp_cpp *cpp)
{
	struct nfp_nffw_info_data *fwinf;
	struct nfp_nffw_info *state;
	uint32_t info_ver;
	int err;

	state = malloc(sizeof(*state));
	if (!state)
		return NULL;

	memset(state, 0, sizeof(*state));

	state->res = nfp_resource_acquire(cpp, NFP_RESOURCE_NFP_NFFW);
	if (!state->res)
		goto err_free;

	fwinf = &state->fwinf;

	if (sizeof(*fwinf) > nfp_resource_size(state->res))
		goto err_release;

	err = nfp_cpp_read(cpp, nfp_resource_cpp_id(state->res),
			   nfp_resource_address(state->res),
			   fwinf, sizeof(*fwinf));
	if (err < (int)sizeof(*fwinf))
		goto err_release;

	if (!nffw_res_flg_init_get(fwinf))
		goto err_release;

	info_ver = nffw_res_info_version_get(fwinf);
	if (info_ver > NFFW_INFO_VERSION_CURRENT)
		goto err_release;

	state->cpp = cpp;
	return state;

err_release:
	nfp_resource_release(state->res);
err_free:
	free(state);
	return NULL;
}

/*
 * nfp_nffw_info_release() - Release the lock on the NFFW table
 * @state:	NFP FW info state
 *
 * Return: 0, or -ERRNO
 */
void
nfp_nffw_info_close(struct nfp_nffw_info *state)
{
	nfp_resource_release(state->res);
	free(state);
}

/*
 * nfp_nffw_info_fwid_first() - Return the first firmware ID in the NFFW
 * @state:	NFP FW info state
 *
 * Return: First NFFW firmware info, NULL on failure
 */
static struct nffw_fwinfo *
nfp_nffw_info_fwid_first(struct nfp_nffw_info *state)
{
	struct nffw_fwinfo *fwinfo;
	unsigned int cnt, i;

	cnt = nffw_res_fwinfos(&state->fwinf, &fwinfo);
	if (!cnt)
		return NULL;

	for (i = 0; i < cnt; i++)
		if (nffw_fwinfo_loaded_get(&fwinfo[i]))
			return &fwinfo[i];

	return NULL;
}

/*
 * nfp_nffw_info_mip_first() - Retrieve the location of the first FW's MIP
 * @state:	NFP FW info state
 * @cpp_id:	Pointer to the CPP ID of the MIP
 * @off:	Pointer to the CPP Address of the MIP
 *
 * Return: 0, or -ERRNO
 */
int
nfp_nffw_info_mip_first(struct nfp_nffw_info *state, uint32_t *cpp_id,
			uint64_t *off)
{
	struct nffw_fwinfo *fwinfo;

	fwinfo = nfp_nffw_info_fwid_first(state);
	if (!fwinfo)
		return -EINVAL;

	*cpp_id = nffw_fwinfo_mip_cppid_get(fwinfo);
	*off = nffw_fwinfo_mip_offset_get(fwinfo);

	if (nffw_fwinfo_mip_mu_da_get(fwinfo)) {
		int locality_off;

		if (NFP_CPP_ID_TARGET_of(*cpp_id) != NFP_CPP_TARGET_MU)
			return 0;

		locality_off = nfp_mip_mu_locality_lsb(state->cpp);
		if (locality_off < 0)
			return locality_off;

		*off &= ~(NFP_MU_ADDR_ACCESS_TYPE_MASK << locality_off);
		*off |= NFP_MU_ADDR_ACCESS_TYPE_DIRECT << locality_off;
	}

	return 0;
}
