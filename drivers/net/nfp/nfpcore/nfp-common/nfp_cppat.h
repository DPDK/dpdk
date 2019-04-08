/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef __NFP_CPPAT_H__
#define __NFP_CPPAT_H__

#include "nfp_platform.h"
#include "nfp_resid.h"

/* This file contains helpers for creating CPP commands
 *
 * All magic NFP-6xxx IMB 'mode' numbers here are from:
 * Databook (1 August 2013)
 * - System Overview and Connectivity
 * -- Internal Connectivity
 * --- Distributed Switch Fabric - Command Push/Pull (DSF-CPP) Bus
 * ---- CPP addressing
 * ----- Table 3.6. CPP Address Translation Mode Commands
 */

#define _NIC_NFP6000_MU_LOCALITY_DIRECT 2

static inline int
_nfp6000_decode_basic(uint64_t addr, int *dest_island, int cpp_tgt, int mode,
		      int addr40, int isld1, int isld0);

static uint64_t
_nic_mask64(int msb, int lsb, int at0)
{
	uint64_t v;
	int w = msb - lsb + 1;

	if (w == 64)
		return ~(uint64_t)0;

	if ((lsb + w) > 64)
		return 0;

	v = (UINT64_C(1) << w) - 1;

	if (at0)
		return v;

	return v << lsb;
}

/* For VQDR, we may not modify the Channel bits, which might overlap
 * with the Index bit. When it does, we need to ensure that isld0 == isld1.
 */
static inline int
_nfp6000_encode_basic(uint64_t *addr, int dest_island, int cpp_tgt, int mode,
		      int addr40, int isld1, int isld0)
{
	uint64_t _u64;
	int iid_lsb, idx_lsb;
	int i, v = 0;
	int isld[2];

	isld[0] = isld0;
	isld[1] = isld1;

	switch (cpp_tgt) {
	case NFP6000_CPPTGT_MU:
		/* This function doesn't handle MU */
		return NFP_ERRNO(EINVAL);
	case NFP6000_CPPTGT_CTXPB:
		/* This function doesn't handle CTXPB */
		return NFP_ERRNO(EINVAL);
	default:
		break;
	}

	switch (mode) {
	case 0:
		if (cpp_tgt == NFP6000_CPPTGT_VQDR && !addr40) {
			/*
			 * In this specific mode we'd rather not modify the
			 * address but we can verify if the existing contents
			 * will point to a valid island.
			 */
			i = _nfp6000_decode_basic(*addr, &v, cpp_tgt, mode,
						  addr40, isld1,
						  isld0);
			if (i != 0)
				/* Full Island ID and channel bits overlap */
				return i;

			/*
			 * If dest_island is invalid, the current address won't
			 * go where expected.
			 */
			if (dest_island != -1 && dest_island != v)
				return NFP_ERRNO(EINVAL);

			/* If dest_island was -1, we don't care */
			return 0;
		}

		iid_lsb = (addr40) ? 34 : 26;

		/* <39:34> or <31:26> */
		_u64 = _nic_mask64((iid_lsb + 5), iid_lsb, 0);
		*addr &= ~_u64;
		*addr |= (((uint64_t)dest_island) << iid_lsb) & _u64;
		return 0;
	case 1:
		if (cpp_tgt == NFP6000_CPPTGT_VQDR && !addr40) {
			i = _nfp6000_decode_basic(*addr, &v, cpp_tgt, mode,
						  addr40, isld1, isld0);
			if (i != 0)
				/* Full Island ID and channel bits overlap */
				return i;

			/*
			 * If dest_island is invalid, the current address won't
			 * go where expected.
			 */
			if (dest_island != -1 && dest_island != v)
				return NFP_ERRNO(EINVAL);

			/* If dest_island was -1, we don't care */
			return 0;
		}

		idx_lsb = (addr40) ? 39 : 31;
		if (dest_island == isld0) {
			/* Only need to clear the Index bit */
			*addr &= ~_nic_mask64(idx_lsb, idx_lsb, 0);
			return 0;
		}

		if (dest_island == isld1) {
			/* Only need to set the Index bit */
			*addr |= (UINT64_C(1) << idx_lsb);
			return 0;
		}

		return NFP_ERRNO(ENODEV);
	case 2:
		if (cpp_tgt == NFP6000_CPPTGT_VQDR && !addr40) {
			/* iid<0> = addr<30> = channel<0> */
			/* channel<1> = addr<31> = Index */

			/*
			 * Special case where we allow channel bits to be set
			 * before hand and with them select an island.
			 * So we need to confirm that it's at least plausible.
			 */
			i = _nfp6000_decode_basic(*addr, &v, cpp_tgt, mode,
						  addr40, isld1, isld0);
			if (i != 0)
				/* Full Island ID and channel bits overlap */
				return i;

			/*
			 * If dest_island is invalid, the current address won't
			 * go where expected.
			 */
			if (dest_island != -1 && dest_island != v)
				return NFP_ERRNO(EINVAL);

			/* If dest_island was -1, we don't care */
			return 0;
		}

		/*
		 * Make sure we compare against isldN values by clearing the
		 * LSB. This is what the silicon does.
		 **/
		isld[0] &= ~1;
		isld[1] &= ~1;

		idx_lsb = (addr40) ? 39 : 31;
		iid_lsb = idx_lsb - 1;

		/*
		 * Try each option, take first one that fits. Not sure if we
		 * would want to do some smarter searching and prefer 0 or non-0
		 * island IDs.
		 */

		for (i = 0; i < 2; i++) {
			for (v = 0; v < 2; v++) {
				if (dest_island != (isld[i] | v))
					continue;
				*addr &= ~_nic_mask64(idx_lsb, iid_lsb, 0);
				*addr |= (((uint64_t)i) << idx_lsb);
				*addr |= (((uint64_t)v) << iid_lsb);
				return 0;
			}
		}

		return NFP_ERRNO(ENODEV);
	case 3:
		if (cpp_tgt == NFP6000_CPPTGT_VQDR && !addr40) {
			/*
			 * iid<0> = addr<29> = data
			 * iid<1> = addr<30> = channel<0>
			 * channel<1> = addr<31> = Index
			 */
			i = _nfp6000_decode_basic(*addr, &v, cpp_tgt, mode,
						  addr40, isld1, isld0);
			if (i != 0)
				/* Full Island ID and channel bits overlap */
				return i;

			if (dest_island != -1 && dest_island != v)
				return NFP_ERRNO(EINVAL);

			/* If dest_island was -1, we don't care */
			return 0;
		}

		isld[0] &= ~3;
		isld[1] &= ~3;

		idx_lsb = (addr40) ? 39 : 31;
		iid_lsb = idx_lsb - 2;

		for (i = 0; i < 2; i++) {
			for (v = 0; v < 4; v++) {
				if (dest_island != (isld[i] | v))
					continue;
				*addr &= ~_nic_mask64(idx_lsb, iid_lsb, 0);
				*addr |= (((uint64_t)i) << idx_lsb);
				*addr |= (((uint64_t)v) << iid_lsb);
				return 0;
			}
		}
		return NFP_ERRNO(ENODEV);
	default:
		break;
	}

	return NFP_ERRNO(EINVAL);
}

static inline int
_nfp6000_decode_basic(uint64_t addr, int *dest_island, int cpp_tgt, int mode,
		      int addr40, int isld1, int isld0)
{
	int iid_lsb, idx_lsb;

	switch (cpp_tgt) {
	case NFP6000_CPPTGT_MU:
		/* This function doesn't handle MU */
		return NFP_ERRNO(EINVAL);
	case NFP6000_CPPTGT_CTXPB:
		/* This function doesn't handle CTXPB */
		return NFP_ERRNO(EINVAL);
	default:
		break;
	}

	switch (mode) {
	case 0:
		/*
		 * For VQDR, in this mode for 32-bit addressing it would be
		 * islands 0, 16, 32 and 48 depending on channel and upper
		 * address bits. Since those are not all valid islands, most
		 * decode cases would result in bad island IDs, but we do them
		 * anyway since this is decoding an address that is already
		 * assumed to be used as-is to get to sram.
		 */
		iid_lsb = (addr40) ? 34 : 26;
		*dest_island = (int)(addr >> iid_lsb) & 0x3F;
		return 0;
	case 1:
		/*
		 * For VQDR 32-bit, this would decode as:
		 *	Channel 0: island#0
		 *	Channel 1: island#0
		 *	Channel 2: island#1
		 *	Channel 3: island#1
		 *
		 * That would be valid as long as both islands have VQDR.
		 * Let's allow this.
		 */

		idx_lsb = (addr40) ? 39 : 31;
		if (addr & _nic_mask64(idx_lsb, idx_lsb, 0))
			*dest_island = isld1;
		else
			*dest_island = isld0;

		return 0;
	case 2:
		/*
		 * For VQDR 32-bit:
		 *	Channel 0: (island#0 | 0)
		 *	Channel 1: (island#0 | 1)
		 *	Channel 2: (island#1 | 0)
		 *	Channel 3: (island#1 | 1)
		 *
		 * Make sure we compare against isldN values by clearing the
		 * LSB. This is what the silicon does.
		 */
		isld0 &= ~1;
		isld1 &= ~1;

		idx_lsb = (addr40) ? 39 : 31;
		iid_lsb = idx_lsb - 1;

		if (addr & _nic_mask64(idx_lsb, idx_lsb, 0))
			*dest_island = isld1 | (int)((addr >> iid_lsb) & 1);
		else
			*dest_island = isld0 | (int)((addr >> iid_lsb) & 1);

		return 0;
	case 3:
		/*
		 * In this mode the data address starts to affect the island ID
		 * so rather not allow it. In some really specific case one
		 * could use this to send the upper half of the VQDR channel to
		 * another MU, but this is getting very specific. However, as
		 * above for mode 0, this is the decoder and the caller should
		 * validate the resulting IID. This blindly does what the
		 * silicon would do.
		 */

		isld0 &= ~3;
		isld1 &= ~3;

		idx_lsb = (addr40) ? 39 : 31;
		iid_lsb = idx_lsb - 2;

		if (addr & _nic_mask64(idx_lsb, idx_lsb, 0))
			*dest_island = isld1 | (int)((addr >> iid_lsb) & 3);
		else
			*dest_island = isld0 | (int)((addr >> iid_lsb) & 3);

		return 0;
	default:
		break;
	}

	return NFP_ERRNO(EINVAL);
}

static inline int
_nfp6000_cppat_mu_locality_lsb(int mode, int addr40)
{
	switch (mode) {
	case 0:
	case 1:
	case 2:
	case 3:
		return (addr40) ? 38 : 30;
	default:
		break;
	}
	return NFP_ERRNO(EINVAL);
}

static inline int
_nfp6000_encode_mu(uint64_t *addr, int dest_island, int mode, int addr40,
		   int isld1, int isld0)
{
	uint64_t _u64;
	int iid_lsb, idx_lsb, locality_lsb;
	int i, v;
	int isld[2];
	int da;

	isld[0] = isld0;
	isld[1] = isld1;
	locality_lsb = _nfp6000_cppat_mu_locality_lsb(mode, addr40);

	if (locality_lsb < 0)
		return NFP_ERRNO(EINVAL);

	if (((*addr >> locality_lsb) & 3) == _NIC_NFP6000_MU_LOCALITY_DIRECT)
		da = 1;
	else
		da = 0;

	switch (mode) {
	case 0:
		iid_lsb = (addr40) ? 32 : 24;
		_u64 = _nic_mask64((iid_lsb + 5), iid_lsb, 0);
		*addr &= ~_u64;
		*addr |= (((uint64_t)dest_island) << iid_lsb) & _u64;
		return 0;
	case 1:
		if (da) {
			iid_lsb = (addr40) ? 32 : 24;
			_u64 = _nic_mask64((iid_lsb + 5), iid_lsb, 0);
			*addr &= ~_u64;
			*addr |= (((uint64_t)dest_island) << iid_lsb) & _u64;
			return 0;
		}

		idx_lsb = (addr40) ? 37 : 29;
		if (dest_island == isld0) {
			*addr &= ~_nic_mask64(idx_lsb, idx_lsb, 0);
			return 0;
		}

		if (dest_island == isld1) {
			*addr |= (UINT64_C(1) << idx_lsb);
			return 0;
		}

		return NFP_ERRNO(ENODEV);
	case 2:
		if (da) {
			iid_lsb = (addr40) ? 32 : 24;
			_u64 = _nic_mask64((iid_lsb + 5), iid_lsb, 0);
			*addr &= ~_u64;
			*addr |= (((uint64_t)dest_island) << iid_lsb) & _u64;
			return 0;
		}

		/*
		 * Make sure we compare against isldN values by clearing the
		 * LSB. This is what the silicon does.
		 */
		isld[0] &= ~1;
		isld[1] &= ~1;

		idx_lsb = (addr40) ? 37 : 29;
		iid_lsb = idx_lsb - 1;

		/*
		 * Try each option, take first one that fits. Not sure if we
		 * would want to do some smarter searching and prefer 0 or
		 * non-0 island IDs.
		 */

		for (i = 0; i < 2; i++) {
			for (v = 0; v < 2; v++) {
				if (dest_island != (isld[i] | v))
					continue;
				*addr &= ~_nic_mask64(idx_lsb, iid_lsb, 0);
				*addr |= (((uint64_t)i) << idx_lsb);
				*addr |= (((uint64_t)v) << iid_lsb);
				return 0;
			}
		}
		return NFP_ERRNO(ENODEV);
	case 3:
		/*
		 * Only the EMU will use 40 bit addressing. Silently set the
		 * direct locality bit for everyone else. The SDK toolchain
		 * uses dest_island <= 0 to test for atypical address encodings
		 * to support access to local-island CTM with a 32-but address
		 * (high-locality is effectively ignored and just used for
		 * routing to island #0).
		 */
		if (dest_island > 0 &&
		    (dest_island < 24 || dest_island > 26)) {
			*addr |= ((uint64_t)_NIC_NFP6000_MU_LOCALITY_DIRECT)
				 << locality_lsb;
			da = 1;
		}

		if (da) {
			iid_lsb = (addr40) ? 32 : 24;
			_u64 = _nic_mask64((iid_lsb + 5), iid_lsb, 0);
			*addr &= ~_u64;
			*addr |= (((uint64_t)dest_island) << iid_lsb) & _u64;
			return 0;
		}

		isld[0] &= ~3;
		isld[1] &= ~3;

		idx_lsb = (addr40) ? 37 : 29;
		iid_lsb = idx_lsb - 2;

		for (i = 0; i < 2; i++) {
			for (v = 0; v < 4; v++) {
				if (dest_island != (isld[i] | v))
					continue;
				*addr &= ~_nic_mask64(idx_lsb, iid_lsb, 0);
				*addr |= (((uint64_t)i) << idx_lsb);
				*addr |= (((uint64_t)v) << iid_lsb);
				return 0;
			}
		}

		return NFP_ERRNO(ENODEV);
	default:
		break;
	}

	return NFP_ERRNO(EINVAL);
}

static inline int
_nfp6000_decode_mu(uint64_t addr, int *dest_island, int mode, int addr40,
		   int isld1, int isld0)
{
	int iid_lsb, idx_lsb, locality_lsb;
	int da;

	locality_lsb = _nfp6000_cppat_mu_locality_lsb(mode, addr40);

	if (((addr >> locality_lsb) & 3) == _NIC_NFP6000_MU_LOCALITY_DIRECT)
		da = 1;
	else
		da = 0;

	switch (mode) {
	case 0:
		iid_lsb = (addr40) ? 32 : 24;
		*dest_island = (int)(addr >> iid_lsb) & 0x3F;
		return 0;
	case 1:
		if (da) {
			iid_lsb = (addr40) ? 32 : 24;
			*dest_island = (int)(addr >> iid_lsb) & 0x3F;
			return 0;
		}

		idx_lsb = (addr40) ? 37 : 29;

		if (addr & _nic_mask64(idx_lsb, idx_lsb, 0))
			*dest_island = isld1;
		else
			*dest_island = isld0;

		return 0;
	case 2:
		if (da) {
			iid_lsb = (addr40) ? 32 : 24;
			*dest_island = (int)(addr >> iid_lsb) & 0x3F;
			return 0;
		}
		/*
		 * Make sure we compare against isldN values by clearing the
		 * LSB. This is what the silicon does.
		 */
		isld0 &= ~1;
		isld1 &= ~1;

		idx_lsb = (addr40) ? 37 : 29;
		iid_lsb = idx_lsb - 1;

		if (addr & _nic_mask64(idx_lsb, idx_lsb, 0))
			*dest_island = isld1 | (int)((addr >> iid_lsb) & 1);
		else
			*dest_island = isld0 | (int)((addr >> iid_lsb) & 1);

		return 0;
	case 3:
		if (da) {
			iid_lsb = (addr40) ? 32 : 24;
			*dest_island = (int)(addr >> iid_lsb) & 0x3F;
			return 0;
		}

		isld0 &= ~3;
		isld1 &= ~3;

		idx_lsb = (addr40) ? 37 : 29;
		iid_lsb = idx_lsb - 2;

		if (addr & _nic_mask64(idx_lsb, idx_lsb, 0))
			*dest_island = isld1 | (int)((addr >> iid_lsb) & 3);
		else
			*dest_island = isld0 | (int)((addr >> iid_lsb) & 3);

		return 0;
	default:
		break;
	}

	return NFP_ERRNO(EINVAL);
}

static inline int
_nfp6000_cppat_addr_encode(uint64_t *addr, int dest_island, int cpp_tgt,
			   int mode, int addr40, int isld1, int isld0)
{
	switch (cpp_tgt) {
	case NFP6000_CPPTGT_NBI:
	case NFP6000_CPPTGT_VQDR:
	case NFP6000_CPPTGT_ILA:
	case NFP6000_CPPTGT_PCIE:
	case NFP6000_CPPTGT_ARM:
	case NFP6000_CPPTGT_CRYPTO:
	case NFP6000_CPPTGT_CLS:
		return _nfp6000_encode_basic(addr, dest_island, cpp_tgt, mode,
					     addr40, isld1, isld0);

	case NFP6000_CPPTGT_MU:
		return _nfp6000_encode_mu(addr, dest_island, mode, addr40,
					  isld1, isld0);

	case NFP6000_CPPTGT_CTXPB:
		if (mode != 1 || addr40 != 0)
			return NFP_ERRNO(EINVAL);

		*addr &= ~_nic_mask64(29, 24, 0);
		*addr |= (((uint64_t)dest_island) << 24) &
			  _nic_mask64(29, 24, 0);
		return 0;
	default:
		break;
	}

	return NFP_ERRNO(EINVAL);
}

static inline int
_nfp6000_cppat_addr_decode(uint64_t addr, int *dest_island, int cpp_tgt,
			   int mode, int addr40, int isld1, int isld0)
{
	switch (cpp_tgt) {
	case NFP6000_CPPTGT_NBI:
	case NFP6000_CPPTGT_VQDR:
	case NFP6000_CPPTGT_ILA:
	case NFP6000_CPPTGT_PCIE:
	case NFP6000_CPPTGT_ARM:
	case NFP6000_CPPTGT_CRYPTO:
	case NFP6000_CPPTGT_CLS:
		return _nfp6000_decode_basic(addr, dest_island, cpp_tgt, mode,
					     addr40, isld1, isld0);

	case NFP6000_CPPTGT_MU:
		return _nfp6000_decode_mu(addr, dest_island, mode, addr40,
					  isld1, isld0);

	case NFP6000_CPPTGT_CTXPB:
		if (mode != 1 || addr40 != 0)
			return -EINVAL;
		*dest_island = (int)(addr >> 24) & 0x3F;
		return 0;
	default:
		break;
	}

	return -EINVAL;
}

static inline int
_nfp6000_cppat_addr_iid_clear(uint64_t *addr, int cpp_tgt, int mode, int addr40)
{
	int iid_lsb, locality_lsb, da;

	switch (cpp_tgt) {
	case NFP6000_CPPTGT_NBI:
	case NFP6000_CPPTGT_VQDR:
	case NFP6000_CPPTGT_ILA:
	case NFP6000_CPPTGT_PCIE:
	case NFP6000_CPPTGT_ARM:
	case NFP6000_CPPTGT_CRYPTO:
	case NFP6000_CPPTGT_CLS:
		switch (mode) {
		case 0:
			iid_lsb = (addr40) ? 34 : 26;
			*addr &= ~(UINT64_C(0x3F) << iid_lsb);
			return 0;
		case 1:
			iid_lsb = (addr40) ? 39 : 31;
			*addr &= ~_nic_mask64(iid_lsb, iid_lsb, 0);
			return 0;
		case 2:
			iid_lsb = (addr40) ? 38 : 30;
			*addr &= ~_nic_mask64(iid_lsb + 1, iid_lsb, 0);
			return 0;
		case 3:
			iid_lsb = (addr40) ? 37 : 29;
			*addr &= ~_nic_mask64(iid_lsb + 2, iid_lsb, 0);
			return 0;
		default:
			break;
		}
	case NFP6000_CPPTGT_MU:
		locality_lsb = _nfp6000_cppat_mu_locality_lsb(mode, addr40);
		da = (((*addr >> locality_lsb) & 3) ==
		      _NIC_NFP6000_MU_LOCALITY_DIRECT);
		switch (mode) {
		case 0:
			iid_lsb = (addr40) ? 32 : 24;
			*addr &= ~(UINT64_C(0x3F) << iid_lsb);
			return 0;
		case 1:
			if (da) {
				iid_lsb = (addr40) ? 32 : 24;
				*addr &= ~(UINT64_C(0x3F) << iid_lsb);
				return 0;
			}
			iid_lsb = (addr40) ? 37 : 29;
			*addr &= ~_nic_mask64(iid_lsb, iid_lsb, 0);
			return 0;
		case 2:
			if (da) {
				iid_lsb = (addr40) ? 32 : 24;
				*addr &= ~(UINT64_C(0x3F) << iid_lsb);
				return 0;
			}

			iid_lsb = (addr40) ? 36 : 28;
			*addr &= ~_nic_mask64(iid_lsb + 1, iid_lsb, 0);
			return 0;
		case 3:
			if (da) {
				iid_lsb = (addr40) ? 32 : 24;
				*addr &= ~(UINT64_C(0x3F) << iid_lsb);
				return 0;
			}

			iid_lsb = (addr40) ? 35 : 27;
			*addr &= ~_nic_mask64(iid_lsb + 2, iid_lsb, 0);
			return 0;
		default:
			break;
		}
	case NFP6000_CPPTGT_CTXPB:
		if (mode != 1 || addr40 != 0)
			return 0;
		*addr &= ~(UINT64_C(0x3F) << 24);
		return 0;
	default:
		break;
	}

	return NFP_ERRNO(EINVAL);
}

#endif /* __NFP_CPPAT_H__ */
