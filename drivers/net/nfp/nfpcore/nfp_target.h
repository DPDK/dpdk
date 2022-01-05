/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef NFP_TARGET_H
#define NFP_TARGET_H

#include "nfp-common/nfp_resid.h"
#include "nfp-common/nfp_cppat.h"
#include "nfp-common/nfp_platform.h"
#include "nfp_cpp.h"

#define P32 1
#define P64 2

#define PUSHPULL(_pull, _push) (((_pull) << 4) | ((_push) << 0))

#ifndef NFP_ERRNO
#include <errno.h>
#define NFP_ERRNO(x)    (errno = (x), -1)
#endif

static inline int
pushpull_width(int pp)
{
	pp &= 0xf;

	if (pp == 0)
		return NFP_ERRNO(EINVAL);
	return (2 << pp);
}

#define PUSH_WIDTH(_pushpull)      pushpull_width((_pushpull) >> 0)
#define PULL_WIDTH(_pushpull)      pushpull_width((_pushpull) >> 4)

static inline int
target_rw(uint32_t cpp_id, int pp, int start, int len)
{
	uint8_t island = NFP_CPP_ID_ISLAND_of(cpp_id);

	if (island && (island < start || island > (start + len)))
		return NFP_ERRNO(EINVAL);

	switch (cpp_id & NFP_CPP_ID(0, ~0, ~0)) {
	case NFP_CPP_ID(0, 0, 0):
		return PUSHPULL(0, pp);
	case NFP_CPP_ID(0, 1, 0):
		return PUSHPULL(pp, 0);
	case NFP_CPP_ID(0, NFP_CPP_ACTION_RW, 0):
		return PUSHPULL(pp, pp);
	default:
		return NFP_ERRNO(EINVAL);
	}
}

static inline int
nfp6000_nbi_dma(uint32_t cpp_id)
{
	switch (cpp_id & NFP_CPP_ID(0, ~0, ~0)) {
	case NFP_CPP_ID(0, 0, 0): /* ReadNbiDma */
		return PUSHPULL(0, P64);
	case NFP_CPP_ID(0, 1, 0): /* WriteNbiDma */
		return PUSHPULL(P64, 0);
	case NFP_CPP_ID(0, NFP_CPP_ACTION_RW, 0):
		return PUSHPULL(P64, P64);
	default:
		return NFP_ERRNO(EINVAL);
	}
}

static inline int
nfp6000_nbi_stats(uint32_t cpp_id)
{
	switch (cpp_id & NFP_CPP_ID(0, ~0, ~0)) {
	case NFP_CPP_ID(0, 0, 0): /* ReadNbiStats */
		return PUSHPULL(0, P64);
	case NFP_CPP_ID(0, 1, 0): /* WriteNbiStats */
		return PUSHPULL(P64, 0);
	case NFP_CPP_ID(0, NFP_CPP_ACTION_RW, 0):
		return PUSHPULL(P64, P64);
	default:
		return NFP_ERRNO(EINVAL);
	}
}

static inline int
nfp6000_nbi_tm(uint32_t cpp_id)
{
	switch (cpp_id & NFP_CPP_ID(0, ~0, ~0)) {
	case NFP_CPP_ID(0, 0, 0): /* ReadNbiTM */
		return PUSHPULL(0, P64);
	case NFP_CPP_ID(0, 1, 0):  /* WriteNbiTM */
		return PUSHPULL(P64, 0);
	case NFP_CPP_ID(0, NFP_CPP_ACTION_RW, 0):
		return PUSHPULL(P64, P64);
	default:
		return NFP_ERRNO(EINVAL);
	}
}

static inline int
nfp6000_nbi_ppc(uint32_t cpp_id)
{
	switch (cpp_id & NFP_CPP_ID(0, ~0, ~0)) {
	case NFP_CPP_ID(0, 0, 0): /* ReadNbiPreclassifier */
		return PUSHPULL(0, P64);
	case NFP_CPP_ID(0, 1, 0): /* WriteNbiPreclassifier */
		return PUSHPULL(P64, 0);
	case NFP_CPP_ID(0, NFP_CPP_ACTION_RW, 0):
		return PUSHPULL(P64, P64);
	default:
		return NFP_ERRNO(EINVAL);
	}
}

static inline int
nfp6000_nbi(uint32_t cpp_id, uint64_t address)
{
	uint8_t island = NFP_CPP_ID_ISLAND_of(cpp_id);
	uint64_t rel_addr = address & 0x3fFFFF;

	if (island && (island < 8 || island > 9))
		return NFP_ERRNO(EINVAL);

	if (rel_addr < (1 << 20))
		return nfp6000_nbi_dma(cpp_id);
	if (rel_addr < (2 << 20))
		return nfp6000_nbi_stats(cpp_id);
	if (rel_addr < (3 << 20))
		return nfp6000_nbi_tm(cpp_id);
	return nfp6000_nbi_ppc(cpp_id);
}

/*
 * This structure ONLY includes items that can be done with a read or write of
 * 32-bit or 64-bit words. All others are not listed.
 */
static inline int
nfp6000_mu_common(uint32_t cpp_id)
{
	switch (cpp_id & NFP_CPP_ID(0, ~0, ~0)) {
	case NFP_CPP_ID(0, NFP_CPP_ACTION_RW, 0): /* read_be/write_be */
		return PUSHPULL(P64, P64);
	case NFP_CPP_ID(0, NFP_CPP_ACTION_RW, 1): /* read_le/write_le */
		return PUSHPULL(P64, P64);
	case NFP_CPP_ID(0, NFP_CPP_ACTION_RW, 2): /* {read/write}_swap_be */
		return PUSHPULL(P64, P64);
	case NFP_CPP_ID(0, NFP_CPP_ACTION_RW, 3): /* {read/write}_swap_le */
		return PUSHPULL(P64, P64);
	case NFP_CPP_ID(0, 0, 0): /* read_be */
		return PUSHPULL(0, P64);
	case NFP_CPP_ID(0, 0, 1): /* read_le */
		return PUSHPULL(0, P64);
	case NFP_CPP_ID(0, 0, 2): /* read_swap_be */
		return PUSHPULL(0, P64);
	case NFP_CPP_ID(0, 0, 3): /* read_swap_le */
		return PUSHPULL(0, P64);
	case NFP_CPP_ID(0, 1, 0): /* write_be */
		return PUSHPULL(P64, 0);
	case NFP_CPP_ID(0, 1, 1): /* write_le */
		return PUSHPULL(P64, 0);
	case NFP_CPP_ID(0, 1, 2): /* write_swap_be */
		return PUSHPULL(P64, 0);
	case NFP_CPP_ID(0, 1, 3): /* write_swap_le */
		return PUSHPULL(P64, 0);
	case NFP_CPP_ID(0, 3, 0): /* atomic_read */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 3, 2): /* mask_compare_write */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 4, 0): /* atomic_write */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 4, 2): /* atomic_write_imm */
		return PUSHPULL(0, 0);
	case NFP_CPP_ID(0, 4, 3): /* swap_imm */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 5, 0): /* set */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 5, 3): /* test_set_imm */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 6, 0): /* clr */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 6, 3): /* test_clr_imm */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 7, 0): /* add */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 7, 3): /* test_add_imm */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 8, 0): /* addsat */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 8, 3): /* test_subsat_imm */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 9, 0): /* sub */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 9, 3): /* test_sub_imm */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 10, 0): /* subsat */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 10, 3): /* test_subsat_imm */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 13, 0): /* microq128_get */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 13, 1): /* microq128_pop */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 13, 2): /* microq128_put */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 15, 0): /* xor */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 15, 3): /* test_xor_imm */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 28, 0): /* read32_be */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 28, 1): /* read32_le */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 28, 2): /* read32_swap_be */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 28, 3): /* read32_swap_le */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 31, 0): /* write32_be */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 31, 1): /* write32_le */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 31, 2): /* write32_swap_be */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 31, 3): /* write32_swap_le */
		return PUSHPULL(P32, 0);
	default:
		return NFP_ERRNO(EINVAL);
	}
}

static inline int
nfp6000_mu_ctm(uint32_t cpp_id)
{
	switch (cpp_id & NFP_CPP_ID(0, ~0, ~0)) {
	case NFP_CPP_ID(0, 16, 1): /* packet_read_packet_status */
		return PUSHPULL(0, P32);
	default:
		return nfp6000_mu_common(cpp_id);
	}
}

static inline int
nfp6000_mu_emu(uint32_t cpp_id)
{
	switch (cpp_id & NFP_CPP_ID(0, ~0, ~0)) {
	case NFP_CPP_ID(0, 18, 0): /* read_queue */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 18, 1): /* read_queue_ring */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 18, 2): /* write_queue */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 18, 3): /* write_queue_ring */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 20, 2): /* journal */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 21, 0): /* get */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 21, 1): /* get_eop */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 21, 2): /* get_freely */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 22, 0): /* pop */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 22, 1): /* pop_eop */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 22, 2): /* pop_freely */
		return PUSHPULL(0, P32);
	default:
		return nfp6000_mu_common(cpp_id);
	}
}

static inline int
nfp6000_mu_imu(uint32_t cpp_id)
{
	return nfp6000_mu_common(cpp_id);
}

static inline int
nfp6000_mu(uint32_t cpp_id, uint64_t address)
{
	int pp;
	uint8_t island = NFP_CPP_ID_ISLAND_of(cpp_id);

	if (island == 0) {
		if (address < 0x2000000000ULL)
			pp = nfp6000_mu_ctm(cpp_id);
		else if (address < 0x8000000000ULL)
			pp = nfp6000_mu_emu(cpp_id);
		else if (address < 0x9800000000ULL)
			pp = nfp6000_mu_ctm(cpp_id);
		else if (address < 0x9C00000000ULL)
			pp = nfp6000_mu_emu(cpp_id);
		else if (address < 0xA000000000ULL)
			pp = nfp6000_mu_imu(cpp_id);
		else
			pp = nfp6000_mu_ctm(cpp_id);
	} else if (island >= 24 && island <= 27) {
		pp = nfp6000_mu_emu(cpp_id);
	} else if (island >= 28 && island <= 31) {
		pp = nfp6000_mu_imu(cpp_id);
	} else if (island == 1 ||
		   (island >= 4 && island <= 7) ||
		   (island >= 12 && island <= 13) ||
		   (island >= 32 && island <= 47) ||
		   (island >= 48 && island <= 51)) {
		pp = nfp6000_mu_ctm(cpp_id);
	} else {
		pp = NFP_ERRNO(EINVAL);
	}

	return pp;
}

static inline int
nfp6000_ila(uint32_t cpp_id)
{
	uint8_t island = NFP_CPP_ID_ISLAND_of(cpp_id);

	if (island && (island < 48 || island > 51))
		return NFP_ERRNO(EINVAL);

	switch (cpp_id & NFP_CPP_ID(0, ~0, ~0)) {
	case NFP_CPP_ID(0, 0, 1): /* read_check_error */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 2, 0): /* read_int */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 3, 0): /* write_int */
		return PUSHPULL(P32, 0);
	default:
		return target_rw(cpp_id, P32, 48, 4);
	}
}

static inline int
nfp6000_pci(uint32_t cpp_id)
{
	uint8_t island = NFP_CPP_ID_ISLAND_of(cpp_id);

	if (island && (island < 4 || island > 7))
		return NFP_ERRNO(EINVAL);

	switch (cpp_id & NFP_CPP_ID(0, ~0, ~0)) {
	case NFP_CPP_ID(0, 2, 0):
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 3, 0):
		return PUSHPULL(P32, 0);
	default:
		return target_rw(cpp_id, P32, 4, 4);
	}
}

static inline int
nfp6000_crypto(uint32_t cpp_id)
{
	uint8_t island = NFP_CPP_ID_ISLAND_of(cpp_id);

	if (island && (island < 12 || island > 15))
		return NFP_ERRNO(EINVAL);

	switch (cpp_id & NFP_CPP_ID(0, ~0, ~0)) {
	case NFP_CPP_ID(0, 2, 0):
		return PUSHPULL(P64, 0);
	default:
		return target_rw(cpp_id, P64, 12, 4);
	}
}

static inline int
nfp6000_cap_xpb(uint32_t cpp_id)
{
	uint8_t island = NFP_CPP_ID_ISLAND_of(cpp_id);

	if (island > 63)
		return NFP_ERRNO(EINVAL);

	switch (cpp_id & NFP_CPP_ID(0, ~0, ~0)) {
	case NFP_CPP_ID(0, 0, 1): /* RingGet */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 0, 2): /* Interthread Signal */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 1, 1): /* RingPut */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 1, 2): /* CTNNWr */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 2, 0): /* ReflectRd, signal none */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 2, 1): /* ReflectRd, signal self */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 2, 2): /* ReflectRd, signal remote */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 2, 3): /* ReflectRd, signal both */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 3, 0): /* ReflectWr, signal none */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 3, 1): /* ReflectWr, signal self */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 3, 2): /* ReflectWr, signal remote */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 3, 3): /* ReflectWr, signal both */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, NFP_CPP_ACTION_RW, 1):
		return PUSHPULL(P32, P32);
	default:
		return target_rw(cpp_id, P32, 1, 63);
	}
}

static inline int
nfp6000_cls(uint32_t cpp_id)
{
	uint8_t island = NFP_CPP_ID_ISLAND_of(cpp_id);

	if (island > 63)
		return NFP_ERRNO(EINVAL);

	switch (cpp_id & NFP_CPP_ID(0, ~0, ~0)) {
	case NFP_CPP_ID(0, 0, 3): /* xor */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 2, 0): /* set */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 2, 1): /* clr */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 4, 0): /* add */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 4, 1): /* add64 */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 6, 0): /* sub */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 6, 1): /* sub64 */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 6, 2): /* subsat */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 8, 2): /* hash_mask */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 8, 3): /* hash_clear */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 9, 0): /* ring_get */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 9, 1): /* ring_pop */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 9, 2): /* ring_get_freely */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 9, 3): /* ring_pop_freely */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 10, 0): /* ring_put */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 10, 2): /* ring_journal */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 14, 0): /* reflect_write_sig_local */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 15, 1):  /* reflect_read_sig_local */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 17, 2): /* statistic */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 24, 0): /* ring_read */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 24, 1): /* ring_write */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, 25, 0): /* ring_workq_add_thread */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 25, 1): /* ring_workq_add_work */
		return PUSHPULL(P32, 0);
	default:
		return target_rw(cpp_id, P32, 0, 64);
	}
}

static inline int
nfp6000_target_pushpull(uint32_t cpp_id, uint64_t address)
{
	switch (NFP_CPP_ID_TARGET_of(cpp_id)) {
	case NFP6000_CPPTGT_NBI:
		return nfp6000_nbi(cpp_id, address);
	case NFP6000_CPPTGT_VQDR:
		return target_rw(cpp_id, P32, 24, 4);
	case NFP6000_CPPTGT_ILA:
		return nfp6000_ila(cpp_id);
	case NFP6000_CPPTGT_MU:
		return nfp6000_mu(cpp_id, address);
	case NFP6000_CPPTGT_PCIE:
		return nfp6000_pci(cpp_id);
	case NFP6000_CPPTGT_ARM:
		if (address < 0x10000)
			return target_rw(cpp_id, P64, 1, 1);
		else
			return target_rw(cpp_id, P32, 1, 1);
	case NFP6000_CPPTGT_CRYPTO:
		return nfp6000_crypto(cpp_id);
	case NFP6000_CPPTGT_CTXPB:
		return nfp6000_cap_xpb(cpp_id);
	case NFP6000_CPPTGT_CLS:
		return nfp6000_cls(cpp_id);
	case 0:
		return target_rw(cpp_id, P32, 4, 4);
	default:
		return NFP_ERRNO(EINVAL);
	}
}

static inline int
nfp_target_pushpull_width(int pp, int write_not_read)
{
	if (pp < 0)
		return pp;

	if (write_not_read)
		return PULL_WIDTH(pp);
	else
		return PUSH_WIDTH(pp);
}

static inline int
nfp6000_target_action_width(uint32_t cpp_id, uint64_t address,
			    int write_not_read)
{
	int pp;

	pp = nfp6000_target_pushpull(cpp_id, address);

	return nfp_target_pushpull_width(pp, write_not_read);
}

static inline int
nfp_target_action_width(uint32_t model, uint32_t cpp_id, uint64_t address,
			int write_not_read)
{
	if (NFP_CPP_MODEL_IS_6000(model)) {
		return nfp6000_target_action_width(cpp_id, address,
						   write_not_read);
	} else {
		return NFP_ERRNO(EINVAL);
	}
}

static inline int
nfp_target_cpp(uint32_t cpp_island_id, uint64_t cpp_island_address,
	       uint32_t *cpp_target_id, uint64_t *cpp_target_address,
	       const uint32_t *imb_table)
{
	int err;
	uint8_t island = NFP_CPP_ID_ISLAND_of(cpp_island_id);
	uint8_t target = NFP_CPP_ID_TARGET_of(cpp_island_id);
	uint32_t imb;

	if (target >= 16)
		return NFP_ERRNO(EINVAL);

	if (island == 0) {
		/* Already translated */
		*cpp_target_id = cpp_island_id;
		*cpp_target_address = cpp_island_address;
		return 0;
	}

	if (!imb_table) {
		/* CPP + Island only allowed on systems with IMB tables */
		return NFP_ERRNO(EINVAL);
	}

	imb = imb_table[target];

	*cpp_target_address = cpp_island_address;
	err = _nfp6000_cppat_addr_encode(cpp_target_address, island, target,
					 ((imb >> 13) & 7),
					 ((imb >> 12) & 1),
					 ((imb >> 6) & 0x3f),
					 ((imb >> 0) & 0x3f));
	if (err == 0) {
		*cpp_target_id =
		    NFP_CPP_ID(target, NFP_CPP_ID_ACTION_of(cpp_island_id),
			       NFP_CPP_ID_TOKEN_of(cpp_island_id));
	}

	return err;
}

#endif /* NFP_TARGET_H */
