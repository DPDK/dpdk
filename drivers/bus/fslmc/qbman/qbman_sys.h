/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 2014-2016 Freescale Semiconductor, Inc.
 *
 */
/* qbman_sys_decl.h and qbman_sys.h are the two platform-specific files in the
 * driver. They are only included via qbman_private.h, which is itself a
 * platform-independent file and is included by all the other driver source.
 *
 * qbman_sys_decl.h is included prior to all other declarations and logic, and
 * it exists to provide compatibility with any linux interfaces our
 * single-source driver code is dependent on (eg. kmalloc). Ie. this file
 * provides linux compatibility.
 *
 * This qbman_sys.h header, on the other hand, is included *after* any common
 * and platform-neutral declarations and logic in qbman_private.h, and exists to
 * implement any platform-specific logic of the qbman driver itself. Ie. it is
 * *not* to provide linux compatibility.
 */

#ifndef _QBMAN_SYS_H_
#define _QBMAN_SYS_H_

#include "qbman_sys_decl.h"

#define CENA_WRITE_ENABLE 0
#define CINH_WRITE_ENABLE 1

/* CINH register offsets */
#define QBMAN_CINH_SWP_EQCR_PI      0x800
#define QBMAN_CINH_SWP_EQCR_CI      0x840
#define QBMAN_CINH_SWP_EQAR         0x8c0
#define QBMAN_CINH_SWP_CR_RT        0x900
#define QBMAN_CINH_SWP_VDQCR_RT     0x940
#define QBMAN_CINH_SWP_EQCR_AM_RT   0x980
#define QBMAN_CINH_SWP_RCR_AM_RT    0x9c0
#define QBMAN_CINH_SWP_DQPI         0xa00
#define QBMAN_CINH_SWP_DQRR_ITR     0xa80
#define QBMAN_CINH_SWP_DCAP         0xac0
#define QBMAN_CINH_SWP_SDQCR        0xb00
#define QBMAN_CINH_SWP_EQCR_AM_RT2  0xb40
#define QBMAN_CINH_SWP_RCR_PI       0xc00
#define QBMAN_CINH_SWP_RAR          0xcc0
#define QBMAN_CINH_SWP_ISR          0xe00
#define QBMAN_CINH_SWP_IER          0xe40
#define QBMAN_CINH_SWP_ISDR         0xe80
#define QBMAN_CINH_SWP_IIR          0xec0
#define QBMAN_CINH_SWP_ITPR         0xf40

/* CENA register offsets */
#define QBMAN_CENA_SWP_EQCR(n) (0x000 + ((uint32_t)(n) << 6))
#define QBMAN_CENA_SWP_DQRR(n) (0x200 + ((uint32_t)(n) << 6))
#define QBMAN_CENA_SWP_RCR(n)  (0x400 + ((uint32_t)(n) << 6))
#define QBMAN_CENA_SWP_CR      0x600
#define QBMAN_CENA_SWP_RR(vb)  (0x700 + ((uint32_t)(vb) >> 1))
#define QBMAN_CENA_SWP_VDQCR   0x780
#define QBMAN_CENA_SWP_EQCR_CI 0x840
#define QBMAN_CENA_SWP_EQCR_CI_MEMBACK 0x1840

/* CENA register offsets in memory-backed mode */
#define QBMAN_CENA_SWP_DQRR_MEM(n)  (0x800 + ((uint32_t)(n) << 6))
#define QBMAN_CENA_SWP_RCR_MEM(n)   (0x1400 + ((uint32_t)(n) << 6))
#define QBMAN_CENA_SWP_CR_MEM       0x1600
#define QBMAN_CENA_SWP_RR_MEM       0x1680
#define QBMAN_CENA_SWP_VDQCR_MEM    0x1780

/* Debugging assists */
static inline void __hexdump(unsigned long start, unsigned long end,
			     unsigned long p, size_t sz, const unsigned char *c)
{
	while (start < end) {
		unsigned int pos = 0;
		char buf[64];
		int nl = 0;

		pos += sprintf(buf + pos, "%08lx: ", start);
		do {
			if ((start < p) || (start >= (p + sz)))
				pos += sprintf(buf + pos, "..");
			else
				pos += sprintf(buf + pos, "%02x", *(c++));
			if (!(++start & 15)) {
				buf[pos++] = '\n';
				nl = 1;
			} else {
				nl = 0;
				if (!(start & 1))
					buf[pos++] = ' ';
				if (!(start & 3))
					buf[pos++] = ' ';
			}
		} while (start & 15);
		if (!nl)
			buf[pos++] = '\n';
		buf[pos] = '\0';
		pr_info("%s", buf);
	}
}

static inline void hexdump(const void *ptr, size_t sz)
{
	unsigned long p = (unsigned long)ptr;
	unsigned long start = p & ~15;
	unsigned long end = (p + sz + 15) & ~15;
	const unsigned char *c = ptr;

	__hexdump(start, end, p, sz, c);
}

/* Currently, the CENA support code expects each 32-bit word to be written in
 * host order, and these are converted to hardware (little-endian) order on
 * command submission. However, 64-bit quantities are must be written (and read)
 * as two 32-bit words with the least-significant word first, irrespective of
 * host endianness.
 */
static inline void u64_to_le32_copy(void *d, const uint64_t *s,
				    unsigned int cnt)
{
	uint32_t *dd = d;
	const uint32_t *ss = (const uint32_t *)s;

	while (cnt--) {
		/* TBD: the toolchain was choking on the use of 64-bit types up
		 * until recently so this works entirely with 32-bit variables.
		 * When 64-bit types become usable again, investigate better
		 * ways of doing this.
		 */
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
		*(dd++) = ss[1];
		*(dd++) = ss[0];
		ss += 2;
#else
		*(dd++) = *(ss++);
		*(dd++) = *(ss++);
#endif
	}
}

static inline void u64_from_le32_copy(uint64_t *d, const void *s,
				      unsigned int cnt)
{
	const uint32_t *ss = s;
	uint32_t *dd = (uint32_t *)d;

	while (cnt--) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
		dd[1] = *(ss++);
		dd[0] = *(ss++);
		dd += 2;
#else
		*(dd++) = *(ss++);
		*(dd++) = *(ss++);
#endif
	}
}

	/******************/
	/* Portal access  */
	/******************/
struct qbman_swp_sys {
	/* On GPP, the sys support for qbman_swp is here. The CENA region isi
	 * not an mmap() of the real portal registers, but an allocated
	 * place-holder, because the actual writes/reads to/from the portal are
	 * marshalled from these allocated areas using QBMan's "MC access
	 * registers". CINH accesses are atomic so there's no need for a
	 * place-holder.
	 */
	uint8_t *cena;
	uint8_t *addr_cena;
	uint8_t *addr_cinh;
	uint32_t idx;
	enum qbman_eqcr_mode eqcr_mode;
};

/* P_OFFSET is (ACCESS_CMD,0,12) - offset within the portal
 * C is (ACCESS_CMD,12,1) - is inhibited? (0==CENA, 1==CINH)
 * SWP_IDX is (ACCESS_CMD,16,10) - Software portal index
 * P is (ACCESS_CMD,28,1) - (0==special portal, 1==any portal)
 * T is (ACCESS_CMD,29,1) - Command type (0==READ, 1==WRITE)
 * E is (ACCESS_CMD,31,1) - Command execute (1 to issue, poll for 0==complete)
 */

static inline void qbman_cinh_write(struct qbman_swp_sys *s, uint32_t offset,
				    uint32_t val)
{
	__raw_writel(val, s->addr_cinh + offset);
#ifdef QBMAN_CINH_TRACE
	pr_info("qbman_cinh_write(%p:%d:0x%03x) 0x%08x\n",
		s->addr_cinh, s->idx, offset, val);
#endif
}

static inline uint32_t qbman_cinh_read(struct qbman_swp_sys *s, uint32_t offset)
{
	uint32_t reg = __raw_readl(s->addr_cinh + offset);
#ifdef QBMAN_CINH_TRACE
	pr_info("qbman_cinh_read(%p:%d:0x%03x) 0x%08x\n",
		s->addr_cinh, s->idx, offset, reg);
#endif
	return reg;
}

static inline void *qbman_cena_write_start(struct qbman_swp_sys *s,
					   uint32_t offset)
{
	void *shadow = s->cena + offset;

#ifdef QBMAN_CENA_TRACE
	pr_info("qbman_cena_write_start(%p:%d:0x%03x) %p\n",
		s->addr_cena, s->idx, offset, shadow);
#endif
	QBMAN_BUG_ON(offset & 63);
	dcbz(shadow);
	return shadow;
}

static inline void *qbman_cena_write_start_wo_shadow(struct qbman_swp_sys *s,
						     uint32_t offset)
{
#ifdef QBMAN_CENA_TRACE
	pr_info("qbman_cena_write_start(%p:%d:0x%03x)\n",
		s->addr_cena, s->idx, offset);
#endif
	QBMAN_BUG_ON(offset & 63);
#ifdef RTE_ARCH_64
	return (s->addr_cena + offset);
#else
	return (s->addr_cinh + offset);
#endif
}

static inline void qbman_cena_write_complete(struct qbman_swp_sys *s,
					     uint32_t offset, void *cmd)
{
	const uint32_t *shadow = cmd;
	int loop;
#ifdef QBMAN_CENA_TRACE
	pr_info("qbman_cena_write_complete(%p:%d:0x%03x) %p\n",
		s->addr_cena, s->idx, offset, shadow);
	hexdump(cmd, 64);
#endif
#ifdef RTE_ARCH_64
	for (loop = 15; loop >= 1; loop--)
		__raw_writel(shadow[loop], s->addr_cena +
					 offset + loop * 4);
	lwsync();
		__raw_writel(shadow[0], s->addr_cena + offset);
#else
	for (loop = 15; loop >= 1; loop--)
		__raw_writel(shadow[loop], s->addr_cinh +
					 offset + loop * 4);
	lwsync();
	__raw_writel(shadow[0], s->addr_cinh + offset);
#endif
	dcbf(s->addr_cena + offset);
}

static inline void qbman_cena_write_complete_wo_shadow(struct qbman_swp_sys *s,
						       uint32_t offset)
{
#ifdef QBMAN_CENA_TRACE
	pr_info("qbman_cena_write_complete(%p:%d:0x%03x)\n",
		s->addr_cena, s->idx, offset);
#endif
	dcbf(s->addr_cena + offset);
}

static inline uint32_t qbman_cena_read_reg(struct qbman_swp_sys *s,
					   uint32_t offset)
{
	return __raw_readl(s->addr_cena + offset);
}

static inline void *qbman_cena_read(struct qbman_swp_sys *s, uint32_t offset)
{
	uint32_t *shadow = (uint32_t *)(s->cena + offset);
	unsigned int loop;
#ifdef QBMAN_CENA_TRACE
	pr_info("qbman_cena_read(%p:%d:0x%03x) %p\n",
		s->addr_cena, s->idx, offset, shadow);
#endif

#ifdef RTE_ARCH_64
	for (loop = 0; loop < 16; loop++)
		shadow[loop] = __raw_readl(s->addr_cena + offset
					+ loop * 4);
#else
	for (loop = 0; loop < 16; loop++)
		shadow[loop] = __raw_readl(s->addr_cinh + offset
					+ loop * 4);
#endif
#ifdef QBMAN_CENA_TRACE
	hexdump(shadow, 64);
#endif
	return shadow;
}

static inline void *qbman_cena_read_wo_shadow(struct qbman_swp_sys *s,
					      uint32_t offset)
{
#ifdef QBMAN_CENA_TRACE
	pr_info("qbman_cena_read(%p:%d:0x%03x)\n",
		s->addr_cena, s->idx, offset);
#endif
	return s->addr_cena + offset;
}

static inline void qbman_cena_invalidate(struct qbman_swp_sys *s,
					 uint32_t offset)
{
	dccivac(s->addr_cena + offset);
}

static inline void qbman_cena_invalidate_prefetch(struct qbman_swp_sys *s,
						  uint32_t offset)
{
	dccivac(s->addr_cena + offset);
	prefetch_for_load(s->addr_cena + offset);
}

static inline void qbman_cena_prefetch(struct qbman_swp_sys *s,
				       uint32_t offset)
{
	prefetch_for_load(s->addr_cena + offset);
}

	/******************/
	/* Portal support */
	/******************/

/* The SWP_CFG portal register is special, in that it is used by the
 * platform-specific code rather than the platform-independent code in
 * qbman_portal.c. So use of it is declared locally here.
 */
#define QBMAN_CINH_SWP_CFG   0xd00

#define SWP_CFG_DQRR_MF_SHIFT 20
#define SWP_CFG_EST_SHIFT     16
#define SWP_CFG_CPBS_SHIFT    15
#define SWP_CFG_WN_SHIFT      14
#define SWP_CFG_RPM_SHIFT     12
#define SWP_CFG_DCM_SHIFT     10
#define SWP_CFG_EPM_SHIFT     8
#define SWP_CFG_VPM_SHIFT     7
#define SWP_CFG_CPM_SHIFT     6
#define SWP_CFG_SD_SHIFT      5
#define SWP_CFG_SP_SHIFT      4
#define SWP_CFG_SE_SHIFT      3
#define SWP_CFG_DP_SHIFT      2
#define SWP_CFG_DE_SHIFT      1
#define SWP_CFG_EP_SHIFT      0

static inline uint32_t qbman_set_swp_cfg(uint8_t max_fill, uint8_t wn,
					 uint8_t est, uint8_t rpm, uint8_t dcm,
					uint8_t epm, int sd, int sp, int se,
					int dp, int de, int ep)
{
	uint32_t reg;

	reg = (max_fill << SWP_CFG_DQRR_MF_SHIFT |
		est << SWP_CFG_EST_SHIFT |
		wn << SWP_CFG_WN_SHIFT |
		rpm << SWP_CFG_RPM_SHIFT |
		dcm << SWP_CFG_DCM_SHIFT |
		epm << SWP_CFG_EPM_SHIFT |
		sd << SWP_CFG_SD_SHIFT |
		sp << SWP_CFG_SP_SHIFT |
		se << SWP_CFG_SE_SHIFT |
		dp << SWP_CFG_DP_SHIFT |
		de << SWP_CFG_DE_SHIFT |
		ep << SWP_CFG_EP_SHIFT);

	return reg;
}

#define QMAN_RT_MODE	0x00000100

#define QMAN_REV_4000	0x04000000
#define QMAN_REV_4100	0x04010000
#define QMAN_REV_4101	0x04010001
#define QMAN_REV_5000	0x05000000
#define QMAN_REV_MASK	0xffff0000

static inline int qbman_swp_sys_init(struct qbman_swp_sys *s,
				     const struct qbman_swp_desc *d,
				     uint8_t dqrr_size)
{
	uint32_t reg;
	int i;
#ifdef RTE_ARCH_64
	uint8_t wn = CENA_WRITE_ENABLE;
#else
	uint8_t wn = CINH_WRITE_ENABLE;
#endif

	s->addr_cena = d->cena_bar;
	s->addr_cinh = d->cinh_bar;
	s->idx = (uint32_t)d->idx;
	s->cena = malloc(64*1024);
	if (!s->cena) {
		pr_err("Could not allocate page for cena shadow\n");
		return -1;
	}
	s->eqcr_mode = d->eqcr_mode;
	QBMAN_BUG_ON(d->idx < 0);
#ifdef QBMAN_CHECKING
	/* We should never be asked to initialise for a portal that isn't in
	 * the power-on state. (Ie. don't forget to reset portals when they are
	 * decommissioned!)
	 */
	reg = qbman_cinh_read(s, QBMAN_CINH_SWP_CFG);
	QBMAN_BUG_ON(reg);
#endif
	if ((d->qman_version & QMAN_REV_MASK) >= QMAN_REV_5000)
		memset(s->addr_cena, 0, 64*1024);
	else {
		/* Invalidate the portal memory.
		 * This ensures no stale cache lines
		 */
		for (i = 0; i < 0x1000; i += 64)
			dccivac(s->addr_cena + i);
	}

	if (s->eqcr_mode == qman_eqcr_vb_array)
		reg = qbman_set_swp_cfg(dqrr_size, wn,
					0, 3, 2, 3, 1, 1, 1, 1, 1, 1);
	else {
		if ((d->qman_version & QMAN_REV_MASK) < QMAN_REV_5000)
			reg = qbman_set_swp_cfg(dqrr_size, wn,
						1, 3, 2, 2, 1, 1, 1, 1, 1, 1);
		else
			reg = qbman_set_swp_cfg(dqrr_size, wn,
						1, 3, 2, 0, 1, 1, 1, 1, 1, 1);
	}

	if ((d->qman_version & QMAN_REV_MASK) >= QMAN_REV_5000) {
		reg |= 1 << SWP_CFG_CPBS_SHIFT | /* memory-backed mode */
		       1 << SWP_CFG_VPM_SHIFT |  /* VDQCR read triggered mode */
		       1 << SWP_CFG_CPM_SHIFT;   /* CR read triggered mode */
	}

	qbman_cinh_write(s, QBMAN_CINH_SWP_CFG, reg);
	reg = qbman_cinh_read(s, QBMAN_CINH_SWP_CFG);
	if (!reg) {
		pr_err("The portal %d is not enabled!\n", s->idx);
		free(s->cena);
		return -1;
	}

	if ((d->qman_version & QMAN_REV_MASK) >= QMAN_REV_5000) {
		qbman_cinh_write(s, QBMAN_CINH_SWP_EQCR_PI, QMAN_RT_MODE);
		qbman_cinh_write(s, QBMAN_CINH_SWP_RCR_PI, QMAN_RT_MODE);
	}

	return 0;
}

static inline void qbman_swp_sys_finish(struct qbman_swp_sys *s)
{
	free(s->cena);
}

#endif /* _QBMAN_SYS_H_ */
