/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 2014 Freescale Semiconductor, Inc.
 * Copyright 2017-2018 NXP
 *
 */
#ifndef _FSL_QBMAN_BASE_H
#define _FSL_QBMAN_BASE_H

/**
 * DOC: QBMan basic structures
 *
 * The QBMan block descriptor, software portal descriptor and Frame descriptor
 * are defined here.
 *
 */

/**
 * struct qbman_block_desc - qbman block descriptor structure
 * @ccsr_reg_bar: CCSR register map.
 * @irq_rerr: Recoverable error interrupt line.
 * @irq_nrerr: Non-recoverable error interrupt line
 *
 * Descriptor for a QBMan instance on the SoC. On partitions/targets that do not
 * control this QBMan instance, these values may simply be place-holders. The
 * idea is simply that we be able to distinguish between them, eg. so that SWP
 * descriptors can identify which QBMan instance they belong to.
 */
struct qbman_block_desc {
	void *ccsr_reg_bar;
	int irq_rerr;
	int irq_nrerr;
};

enum qbman_eqcr_mode {
	qman_eqcr_vb_ring = 2, /* Valid bit, with eqcr in ring mode */
	qman_eqcr_vb_array,    /* Valid bit, with eqcr in array mode */
};

enum qbman_cena_access_mode {
	qman_cena_fastest_access = 0, /* Use memory backed node if available */
	qman_cena_direct_access,      /* Use direct access to the CENA region */
};

/**
 * struct qbman_swp_desc - qbman software portal descriptor structure
 * @block: The QBMan instance.
 * @cena_bar: Cache-enabled portal register map.
 * @cinh_bar: Cache-inhibited portal register map.
 * @irq: -1 if unused (or unassigned)
 * @idx: SWPs within a QBMan are indexed. -1 if opaque to the user.
 * @qman_version: the qman version.
 * @eqcr_mode: Select the eqcr mode, currently only valid bit ring mode and
 * valid bit array mode are supported.
 * @cena_access_mode: Mode used to access the CENA region, direct
 *                    or memory backed.
 *
 * Descriptor for a QBMan software portal, expressed in terms that make sense to
 * the user context. Ie. on MC, this information is likely to be true-physical,
 * and instantiated statically at compile-time. On GPP, this information is
 * likely to be obtained via "discovery" over a partition's "MC bus"
 * (ie. in response to a MC portal command), and would take into account any
 * virtualisation of the GPP user's address space and/or interrupt numbering.
 */
struct qbman_swp_desc {
	const struct qbman_block_desc *block;
	uint8_t *cena_bar;
	uint8_t *cinh_bar;
	int irq;
	int idx;
	uint32_t qman_version;
	enum qbman_eqcr_mode eqcr_mode;
	enum qbman_cena_access_mode cena_access_mode;
};

/* Driver object for managing a QBMan portal */
struct qbman_swp;

/**
 * struct qbman_fd - basci structure for qbman frame descriptor
 * @words: for easier/faster copying the whole FD structure.
 * @addr_lo: the lower 32 bits of the address in FD.
 * @addr_hi: the upper 32 bits of the address in FD.
 * @len: the length field in FD.
 * @bpid_offset: represent the bpid and offset fields in FD. offset in
 * the MS 16 bits, BPID in the LS 16 bits.
 * @frc: frame context
 * @ctrl: the 32bit control bits including dd, sc,... va, err.
 * @flc_lo: the lower 32bit of flow context.
 * @flc_hi: the upper 32bits of flow context.
 *
 * Place-holder for FDs, we represent it via the simplest form that we need for
 * now. Different overlays may be needed to support different options, etc. (It
 * is impractical to define One True Struct, because the resulting encoding
 * routines (lots of read-modify-writes) would be worst-case performance whether
 * or not circumstances required them.)
 *
 * Note, as with all data-structures exchanged between software and hardware (be
 * they located in the portal register map or DMA'd to and from main-memory),
 * the driver ensures that the caller of the driver API sees the data-structures
 * in host-endianness. "struct qbman_fd" is no exception. The 32-bit words
 * contained within this structure are represented in host-endianness, even if
 * hardware always treats them as little-endian. As such, if any of these fields
 * are interpreted in a binary (rather than numerical) fashion by hardware
 * blocks (eg. accelerators), then the user should be careful. We illustrate
 * with an example;
 *
 * Suppose the desired behaviour of an accelerator is controlled by the "frc"
 * field of the FDs that are sent to it. Suppose also that the behaviour desired
 * by the user corresponds to an "frc" value which is expressed as the literal
 * sequence of bytes 0xfe, 0xed, 0xab, and 0xba. So "frc" should be the 32-bit
 * value in which 0xfe is the first byte and 0xba is the last byte, and as
 * hardware is little-endian, this amounts to a 32-bit "value" of 0xbaabedfe. If
 * the software is little-endian also, this can simply be achieved by setting
 * frc=0xbaabedfe. On the other hand, if software is big-endian, it should set
 * frc=0xfeedabba! The best away of avoiding trouble with this sort of thing is
 * to treat the 32-bit words as numerical values, in which the offset of a field
 * from the beginning of the first byte (as required or generated by hardware)
 * is numerically encoded by a left-shift (ie. by raising the field to a
 * corresponding power of 2).  Ie. in the current example, software could set
 * "frc" in the following way, and it would work correctly on both little-endian
 * and big-endian operation;
 *    fd.frc = (0xfe << 0) | (0xed << 8) | (0xab << 16) | (0xba << 24);
 */
struct qbman_fd {
	union {
		uint32_t words[8];
		struct qbman_fd_simple {
			uint32_t addr_lo;
			uint32_t addr_hi;
			uint32_t len;
			uint32_t bpid_offset;
			uint32_t frc;
			uint32_t ctrl;
			uint32_t flc_lo;
			uint32_t flc_hi;
		} simple;
	};
};

#endif /* !_FSL_QBMAN_BASE_H */
