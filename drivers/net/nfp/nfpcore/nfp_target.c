/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Corigine, Inc.
 * All rights reserved.
 */

#include "nfp_target.h"

#include "nfp_cpp.h"
#include "nfp6000/nfp6000.h"

#define P32 1
#define P64 2

/*
 * All magic NFP-6xxx IMB 'mode' numbers here are from:
 * Databook (1 August 2013)
 * - System Overview and Connectivity
 * -- Internal Connectivity
 * --- Distributed Switch Fabric - Command Push/Pull (DSF-CPP) Bus
 * ---- CPP addressing
 * ----- Table 3.6. CPP Address Translation Mode Commands
 */
#define NFP6000_MU_LOCALITY_DIRECT 2

static int
target_rw(uint32_t cpp_id,
		int pp,
		int start,
		int len)
{
	uint8_t island;

	island = NFP_CPP_ID_ISLAND_of(cpp_id);
	if (island != 0 && (island < start || island > (start + len)))
		return -EINVAL;

	switch (cpp_id & NFP_CPP_ID(0, ~0, ~0)) {
	case NFP_CPP_ID(0, 0, 0):
		return PUSHPULL(0, pp);
	case NFP_CPP_ID(0, 1, 0):
		return PUSHPULL(pp, 0);
	case NFP_CPP_ID(0, NFP_CPP_ACTION_RW, 0):
		return PUSHPULL(pp, pp);
	default:
		return -EINVAL;
	}
}

static int
nfp6000_nbi_dma(uint32_t cpp_id)
{
	switch (cpp_id & NFP_CPP_ID(0, ~0, ~0)) {
	case NFP_CPP_ID(0, 0, 0): /* Read NBI DMA */
		return PUSHPULL(0, P64);
	case NFP_CPP_ID(0, 1, 0): /* Write NBI DMA */
		return PUSHPULL(P64, 0);
	case NFP_CPP_ID(0, NFP_CPP_ACTION_RW, 0):
		return PUSHPULL(P64, P64);
	default:
		return -EINVAL;
	}
}

static int
nfp6000_nbi_stats(uint32_t cpp_id)
{
	switch (cpp_id & NFP_CPP_ID(0, ~0, ~0)) {
	case NFP_CPP_ID(0, 0, 0): /* Read NBI Stats */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 1, 0): /* Write NBI Stats */
		return PUSHPULL(P32, 0);
	case NFP_CPP_ID(0, NFP_CPP_ACTION_RW, 0):
		return PUSHPULL(P32, P32);
	default:
		return -EINVAL;
	}
}

static int
nfp6000_nbi_tm(uint32_t cpp_id)
{
	switch (cpp_id & NFP_CPP_ID(0, ~0, ~0)) {
	case NFP_CPP_ID(0, 0, 0): /* Read NBI TM */
		return PUSHPULL(0, P64);
	case NFP_CPP_ID(0, 1, 0): /* Write NBI TM */
		return PUSHPULL(P64, 0);
	case NFP_CPP_ID(0, NFP_CPP_ACTION_RW, 0):
		return PUSHPULL(P64, P64);
	default:
		return -EINVAL;
	}
}

static int
nfp6000_nbi_ppc(uint32_t cpp_id)
{
	switch (cpp_id & NFP_CPP_ID(0, ~0, ~0)) {
	case NFP_CPP_ID(0, 0, 0): /* Read NBI Preclassifier */
		return PUSHPULL(0, P64);
	case NFP_CPP_ID(0, 1, 0): /* Write NBI Preclassifier */
		return PUSHPULL(P64, 0);
	case NFP_CPP_ID(0, NFP_CPP_ACTION_RW, 0):
		return PUSHPULL(P64, P64);
	default:
		return -EINVAL;
	}
}

static int
nfp6000_nbi(uint32_t cpp_id,
		uint64_t address)
{
	uint8_t island;
	uint64_t rel_addr;

	island = NFP_CPP_ID_ISLAND_of(cpp_id);
	if (island != 8 && island != 9)
		return -EINVAL;

	rel_addr = address & 0x3FFFFF;
	if (rel_addr < (1 << 20))        /* [0x000000, 0x100000) */
		return nfp6000_nbi_dma(cpp_id);
	else if (rel_addr < (2 << 20))   /* [0x100000, 0x200000) */
		return nfp6000_nbi_stats(cpp_id);
	else if (rel_addr < (3 << 20))   /* [0x200000, 0x300000) */
		return nfp6000_nbi_tm(cpp_id);
	else                             /* [0x300000, 0x400000) */
		return nfp6000_nbi_ppc(cpp_id);
}

/*
 * This structure ONLY includes items that can be done with a read or write of
 * 32-bit or 64-bit words. All others are not listed.
 */
static int
nfp6000_mu_common(uint32_t cpp_id)
{
	switch (cpp_id & NFP_CPP_ID(0, ~0, ~0)) {
	case NFP_CPP_ID(0, NFP_CPP_ACTION_RW, 0): /* read_be/write_be */
		return PUSHPULL(P64, P64);
	case NFP_CPP_ID(0, NFP_CPP_ACTION_RW, 1): /* read_le/write_le */
		return PUSHPULL(P64, P64);
	case NFP_CPP_ID(0, NFP_CPP_ACTION_RW, 2): /* read_swap_be/write_swap_be */
		return PUSHPULL(P64, P64);
	case NFP_CPP_ID(0, NFP_CPP_ACTION_RW, 3): /* read_swap_le/write_swap_le */
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
		return -EINVAL;
	}
}

static int
nfp6000_mu_ctm(uint32_t cpp_id)
{
	switch (cpp_id & NFP_CPP_ID(0, ~0, ~0)) {
	case NFP_CPP_ID(0, 16, 1): /* packet_read_packet_status */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 17, 1): /* packet_credit_get */
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 17, 3): /* packet_add_thread */
		return PUSHPULL(0, P64);
	case NFP_CPP_ID(0, 18, 2): /* packet_free_and_return_pointer */
		return PUSHPULL(0, P64);
	case NFP_CPP_ID(0, 18, 3): /* packet_return_pointer */
		return PUSHPULL(0, P64);
	case NFP_CPP_ID(0, 21, 0): /* pe_dma_to_memory_indirect */
		return PUSHPULL(0, P64);
	case NFP_CPP_ID(0, 21, 1): /* pe_dma_to_memory_indirect_swap */
		return PUSHPULL(0, P64);
	case NFP_CPP_ID(0, 21, 2): /* pe_dma_to_memory_indirect_free */
		return PUSHPULL(0, P64);
	case NFP_CPP_ID(0, 21, 3): /* pe_dma_to_memory_indirect_free_swap */
		return PUSHPULL(0, P64);
	default:
		return nfp6000_mu_common(cpp_id);
	}
}

static int
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

static int
nfp6000_mu_imu(uint32_t cpp_id)
{
	return nfp6000_mu_common(cpp_id);
}

static int
nfp6000_mu(uint32_t cpp_id,
		uint64_t address)
{
	int pp;
	uint8_t island;

	island = NFP_CPP_ID_ISLAND_of(cpp_id);
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
			(island >= 32 && island <= 51)) {
		pp = nfp6000_mu_ctm(cpp_id);
	} else {
		pp = -EINVAL;
	}

	return pp;
}

static int
nfp6000_ila(uint32_t cpp_id)
{
	uint8_t island;

	island = NFP_CPP_ID_ISLAND_of(cpp_id);
	if (island != 0 && (island < 48 || island > 51))
		return -EINVAL;

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

static int
nfp6000_pci(uint32_t cpp_id)
{
	uint8_t island;

	island = NFP_CPP_ID_ISLAND_of(cpp_id);
	if (island != 0 && (island < 4 || island > 7))
		return -EINVAL;

	switch (cpp_id & NFP_CPP_ID(0, ~0, ~0)) {
	case NFP_CPP_ID(0, 2, 0):
		return PUSHPULL(0, P32);
	case NFP_CPP_ID(0, 3, 0):
		return PUSHPULL(P32, 0);
	default:
		return target_rw(cpp_id, P32, 4, 4);
	}
}

static int
nfp6000_crypto(uint32_t cpp_id)
{
	uint8_t island;

	island = NFP_CPP_ID_ISLAND_of(cpp_id);
	if (island != 0 && (island < 12 || island > 15))
		return -EINVAL;

	switch (cpp_id & NFP_CPP_ID(0, ~0, ~0)) {
	case NFP_CPP_ID(0, 2, 0):
		return PUSHPULL(P64, 0);
	default:
		return target_rw(cpp_id, P64, 12, 4);
	}
}

static int
nfp6000_cap_xpb(uint32_t cpp_id)
{
	uint8_t island;

	island = NFP_CPP_ID_ISLAND_of(cpp_id);
	if (island > 63)
		return -EINVAL;

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

static int
nfp6000_cls(uint32_t cpp_id)
{
	uint8_t island;

	island = NFP_CPP_ID_ISLAND_of(cpp_id);
	if (island > 63)
		return -EINVAL;

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

int
nfp_target_pushpull(uint32_t cpp_id,
		uint64_t address)
{
	switch (NFP_CPP_ID_TARGET_of(cpp_id)) {
	case NFP_CPP_TARGET_NBI:
		return nfp6000_nbi(cpp_id, address);
	case NFP_CPP_TARGET_QDR:
		return target_rw(cpp_id, P32, 24, 4);
	case NFP_CPP_TARGET_ILA:
		return nfp6000_ila(cpp_id);
	case NFP_CPP_TARGET_MU:
		return nfp6000_mu(cpp_id, address);
	case NFP_CPP_TARGET_PCIE:
		return nfp6000_pci(cpp_id);
	case NFP_CPP_TARGET_ARM:
		if (address < 0x10000)
			return target_rw(cpp_id, P64, 1, 1);
		else
			return target_rw(cpp_id, P32, 1, 1);
	case NFP_CPP_TARGET_CRYPTO:
		return nfp6000_crypto(cpp_id);
	case NFP_CPP_TARGET_CT_XPB:
		return nfp6000_cap_xpb(cpp_id);
	case NFP_CPP_TARGET_CLS:
		return nfp6000_cls(cpp_id);
	case NFP_CPP_TARGET_INVALID:
		return target_rw(cpp_id, P32, 4, 4);
	default:
		return -EINVAL;
	}
}

static uint64_t
nfp_mask64(int msb,
		int lsb)
{
	int width;

	if (msb < 0 || lsb < 0)
		return 0;

	width = msb - lsb + 1;
	if (width <= 0)
		return 0;

	if (width == 64)
		return ~(uint64_t)0;

	if ((lsb + width) > 64)
		return 0;

	return (RTE_BIT64(width) - 1) << lsb;
}

static int
nfp_decode_basic(uint64_t addr,
		int *dest_island,
		int cpp_tgt,
		int mode,
		int addr40,
		int isld1,
		int isld0)
{
	int iid_lsb;
	int idx_lsb;

	/* This function doesn't handle MU or CTXBP */
	if (cpp_tgt == NFP_CPP_TARGET_MU || cpp_tgt == NFP_CPP_TARGET_CT_XPB)
		return -EINVAL;

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
		if ((addr & nfp_mask64(idx_lsb, idx_lsb)) != 0)
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

		if ((addr & nfp_mask64(idx_lsb, idx_lsb)) != 0)
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

		if ((addr & nfp_mask64(idx_lsb, idx_lsb)) != 0)
			*dest_island = isld1 | (int)((addr >> iid_lsb) & 3);
		else
			*dest_island = isld0 | (int)((addr >> iid_lsb) & 3);

		return 0;
	default:
		return -EINVAL;
	}
}

static int
nfp_encode_basic_qdr(uint64_t addr,
		int dest_island,
		int cpp_tgt,
		int mode,
		int addr40,
		int isld1,
		int isld0)
{
	int v;
	int ret;

	/* Full Island ID and channel bits overlap? */
	ret = nfp_decode_basic(addr, &v, cpp_tgt, mode, addr40, isld1, isld0);
	if (ret != 0)
		return ret;

	/* The current address won't go where expected? */
	if (dest_island != -1 && dest_island != v)
		return -EINVAL;

	/* If dest_island was -1, we don't care where it goes. */
	return 0;
}

/*
 * Try each option, take first one that fits.
 * Not sure if we would want to do some smarter
 * searching and prefer 0 or non-0 island IDs.
 */
static int
nfp_encode_basic_search(uint64_t *addr,
		int dest_island,
		int *isld,
		int iid_lsb,
		int idx_lsb,
		int v_max)
{
	int i;
	int v;

	for (i = 0; i < 2; i++)
		for (v = 0; v < v_max; v++) {
			if (dest_island != (isld[i] | v))
				continue;

			*addr &= ~nfp_mask64(idx_lsb, iid_lsb);
			*addr |= ((uint64_t)i << idx_lsb);
			*addr |= ((uint64_t)v << iid_lsb);
			return 0;
		}

	return -ENODEV;
}

/*
 * For VQDR, we may not modify the Channel bits, which might overlap
 * with the Index bit. When it does, we need to ensure that isld0 == isld1.
 */
static int
nfp_encode_basic(uint64_t *addr,
		int dest_island,
		int cpp_tgt,
		int mode,
		int addr40,
		int isld1,
		int isld0)
{
	int iid_lsb;
	int idx_lsb;
	int isld[2];
	uint64_t value;

	isld[0] = isld0;
	isld[1] = isld1;

	/* This function doesn't handle MU or CTXBP */
	if (cpp_tgt == NFP_CPP_TARGET_MU || cpp_tgt == NFP_CPP_TARGET_CT_XPB)
		return -EINVAL;

	switch (mode) {
	case 0:
		if (cpp_tgt == NFP_CPP_TARGET_QDR && addr40 == 0) {
			/*
			 * In this specific mode we'd rather not modify the
			 * address but we can verify if the existing contents
			 * will point to a valid island.
			 */
			return nfp_encode_basic_qdr(*addr, cpp_tgt, dest_island,
					mode, addr40, isld1, isld0);
		}

		iid_lsb = (addr40) ? 34 : 26;

		/* <39:34> or <31:26> */
		value = nfp_mask64((iid_lsb + 5), iid_lsb);
		*addr &= ~value;
		*addr |= (((uint64_t)dest_island) << iid_lsb) & value;
		return 0;
	case 1:
		if (cpp_tgt == NFP_CPP_TARGET_QDR && addr40 == 0) {
			return nfp_encode_basic_qdr(*addr, cpp_tgt, dest_island,
					mode, addr40, isld1, isld0);
		}

		idx_lsb = (addr40) ? 39 : 31;
		if (dest_island == isld0) {
			/* Only need to clear the Index bit */
			*addr &= ~nfp_mask64(idx_lsb, idx_lsb);
			return 0;
		}

		if (dest_island == isld1) {
			/* Only need to set the Index bit */
			*addr |= (UINT64_C(1) << idx_lsb);
			return 0;
		}

		return -ENODEV;
	case 2:
		if (cpp_tgt == NFP_CPP_TARGET_QDR && addr40 == 0) {
			/* iid<0> = addr<30> = channel<0> */
			/* channel<1> = addr<31> = Index */
			return nfp_encode_basic_qdr(*addr, cpp_tgt, dest_island,
					mode, addr40, isld1, isld0);
		}

		/*
		 * Make sure we compare against isldN values by clearing the
		 * LSB. This is what the silicon does.
		 */
		isld[0] &= ~1;
		isld[1] &= ~1;

		idx_lsb = (addr40) ? 39 : 31;
		iid_lsb = idx_lsb - 1;

		return nfp_encode_basic_search(addr, dest_island, isld,
				iid_lsb, idx_lsb, 2);
	case 3:
		if (cpp_tgt == NFP_CPP_TARGET_QDR && addr40 == 0) {
			/*
			 * iid<0> = addr<29> = data
			 * iid<1> = addr<30> = channel<0>
			 * channel<1> = addr<31> = Index
			 */
			return nfp_encode_basic_qdr(*addr, cpp_tgt, dest_island,
					mode, addr40, isld1, isld0);
		}

		isld[0] &= ~3;
		isld[1] &= ~3;

		idx_lsb = (addr40) ? 39 : 31;
		iid_lsb = idx_lsb - 2;

		return nfp_encode_basic_search(addr, dest_island, isld,
				iid_lsb, idx_lsb, 4);
	default:
		return -EINVAL;
	}
}

static int
nfp_encode_mu(uint64_t *addr,
		int dest_island,
		int mode,
		int addr40,
		int isld1,
		int isld0)
{
	int da;
	int iid_lsb;
	int idx_lsb;
	int isld[2];
	uint64_t value;
	int locality_lsb;

	isld[0] = isld0;
	isld[1] = isld1;

	locality_lsb = nfp_cppat_mu_locality_lsb(mode, addr40);
	if (locality_lsb < 0)
		return -EINVAL;

	if (((*addr >> locality_lsb) & 3) == NFP6000_MU_LOCALITY_DIRECT)
		da = 1;
	else
		da = 0;

	switch (mode) {
	case 0:
		iid_lsb = (addr40 != 0) ? 32 : 24;
		value = nfp_mask64((iid_lsb + 5), iid_lsb);
		*addr &= ~value;
		*addr |= (((uint64_t)dest_island) << iid_lsb) & value;
		return 0;
	case 1:
		if (da == 1) {
			iid_lsb = (addr40 != 0) ? 32 : 24;
			value = nfp_mask64((iid_lsb + 5), iid_lsb);
			*addr &= ~value;
			*addr |= (((uint64_t)dest_island) << iid_lsb) & value;
			return 0;
		}

		idx_lsb = (addr40 != 0) ? 37 : 29;
		if (dest_island == isld0) {
			*addr &= ~nfp_mask64(idx_lsb, idx_lsb);
			return 0;
		}

		if (dest_island == isld1) {
			*addr |= (UINT64_C(1) << idx_lsb);
			return 0;
		}

		return -ENODEV;
	case 2:
		if (da == 1) {
			iid_lsb = (addr40 != 0) ? 32 : 24;
			value = nfp_mask64((iid_lsb + 5), iid_lsb);
			*addr &= ~value;
			*addr |= (((uint64_t)dest_island) << iid_lsb) & value;
			return 0;
		}

		/*
		 * Make sure we compare against isldN values by clearing the
		 * LSB. This is what the silicon does.
		 */
		isld[0] &= ~1;
		isld[1] &= ~1;

		idx_lsb = (addr40 != 0) ? 37 : 29;
		iid_lsb = idx_lsb - 1;

		return nfp_encode_basic_search(addr, dest_island, isld,
				iid_lsb, idx_lsb, 2);
	case 3:
		/*
		 * Only the EMU will use 40 bit addressing. Silently set the
		 * direct locality bit for everyone else. The SDK toolchain
		 * uses dest_island <= 0 to test for atypical address encodings
		 * to support access to local-island CTM with a 32-but address
		 * (high-locality is effectively ignored and just used for
		 * routing to island #0).
		 */
		if (dest_island > 0 && (dest_island < 24 || dest_island > 26)) {
			*addr |= ((uint64_t)NFP6000_MU_LOCALITY_DIRECT)
					<< locality_lsb;
			da = 1;
		}

		if (da == 1) {
			iid_lsb = (addr40 != 0) ? 32 : 24;
			value = nfp_mask64((iid_lsb + 5), iid_lsb);
			*addr &= ~value;
			*addr |= (((uint64_t)dest_island) << iid_lsb) & value;
			return 0;
		}

		isld[0] &= ~3;
		isld[1] &= ~3;

		idx_lsb = (addr40 != 0) ? 37 : 29;
		iid_lsb = idx_lsb - 2;

		return nfp_encode_basic_search(addr, dest_island, isld,
				iid_lsb, idx_lsb, 4);
	default:
		return -EINVAL;
	}
}

static int
nfp_cppat_addr_encode(uint64_t *addr,
		int dest_island,
		int cpp_tgt,
		int mode,
		int addr40,
		int isld1,
		int isld0)
{
	uint64_t value;

	switch (cpp_tgt) {
	case NFP_CPP_TARGET_NBI:
	case NFP_CPP_TARGET_QDR:
	case NFP_CPP_TARGET_ILA:
	case NFP_CPP_TARGET_PCIE:
	case NFP_CPP_TARGET_ARM:
	case NFP_CPP_TARGET_CRYPTO:
	case NFP_CPP_TARGET_CLS:
		return nfp_encode_basic(addr, dest_island, cpp_tgt, mode,
				addr40, isld1, isld0);
	case NFP_CPP_TARGET_MU:
		return nfp_encode_mu(addr, dest_island, mode, addr40,
				isld1, isld0);
	case NFP_CPP_TARGET_CT_XPB:
		if (mode != 1 || addr40 != 0)
			return -EINVAL;

		value = nfp_mask64(29, 24);
		*addr &= ~value;
		*addr |= (((uint64_t)dest_island) << 24) & value;
		return 0;
	default:
		return -EINVAL;
	}
}

int
nfp_target_cpp(uint32_t cpp_island_id,
		uint64_t cpp_island_address,
		uint32_t *cpp_target_id,
		uint64_t *cpp_target_address,
		const uint32_t *imb_table)
{
	int err;
	uint32_t imb;
	uint8_t island;
	uint8_t target;

	target = NFP_CPP_ID_TARGET_of(cpp_island_id);
	if (target >= 16)
		return -EINVAL;

	island = NFP_CPP_ID_ISLAND_of(cpp_island_id);
	if (island == 0) {
		/* Already translated */
		*cpp_target_id = cpp_island_id;
		*cpp_target_address = cpp_island_address;
		return 0;
	}

	/* CPP + Island only allowed on systems with IMB tables */
	if (imb_table == NULL)
		return -EINVAL;

	imb = imb_table[target];

	*cpp_target_address = cpp_island_address;
	err = nfp_cppat_addr_encode(cpp_target_address, island, target,
			((imb >> 13) & 7), ((imb >> 12) & 1),
			((imb >> 6) & 0x3f), ((imb >> 0) & 0x3f));
	if (err != 0)
		return err;

	*cpp_target_id = NFP_CPP_ID(target,
			NFP_CPP_ID_ACTION_of(cpp_island_id),
			NFP_CPP_ID_TOKEN_of(cpp_island_id));

	return 0;
}
