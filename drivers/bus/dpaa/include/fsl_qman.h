/*-
 * This file is provided under a dual BSD/GPLv2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 *   BSD LICENSE
 *
 * Copyright 2008-2012 Freescale Semiconductor, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the above-listed copyright holders nor the
 * names of any contributors may be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 *   GPL LICENSE SUMMARY
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __FSL_QMAN_H
#define __FSL_QMAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <dpaa_rbtree.h>

/* Last updated for v00.800 of the BG */

/* Hardware constants */
#define QM_CHANNEL_SWPORTAL0 0
#define QMAN_CHANNEL_POOL1 0x21
#define QMAN_CHANNEL_CAAM 0x80
#define QMAN_CHANNEL_PME 0xa0
#define QMAN_CHANNEL_POOL1_REV3 0x401
#define QMAN_CHANNEL_CAAM_REV3 0x840
#define QMAN_CHANNEL_PME_REV3 0x860
extern u16 qm_channel_pool1;
extern u16 qm_channel_caam;
extern u16 qm_channel_pme;
enum qm_dc_portal {
	qm_dc_portal_fman0 = 0,
	qm_dc_portal_fman1 = 1,
	qm_dc_portal_caam = 2,
	qm_dc_portal_pme = 3
};

/* Portal processing (interrupt) sources */
#define QM_PIRQ_CCSCI	0x00200000	/* CEETM Congestion State Change */
#define QM_PIRQ_CSCI	0x00100000	/* Congestion State Change */
#define QM_PIRQ_EQCI	0x00080000	/* Enqueue Command Committed */
#define QM_PIRQ_EQRI	0x00040000	/* EQCR Ring (below threshold) */
#define QM_PIRQ_DQRI	0x00020000	/* DQRR Ring (non-empty) */
#define QM_PIRQ_MRI	0x00010000	/* MR Ring (non-empty) */
/*
 * This mask contains all the interrupt sources that need handling except DQRI,
 * ie. that if present should trigger slow-path processing.
 */
#define QM_PIRQ_SLOW	(QM_PIRQ_CSCI | QM_PIRQ_EQCI | QM_PIRQ_EQRI | \
			QM_PIRQ_MRI | QM_PIRQ_CCSCI)

/* For qman_static_dequeue_*** APIs */
#define QM_SDQCR_CHANNELS_POOL_MASK	0x00007fff
/* for n in [1,15] */
#define QM_SDQCR_CHANNELS_POOL(n)	(0x00008000 >> (n))
/* for conversion from n of qm_channel */
static inline u32 QM_SDQCR_CHANNELS_POOL_CONV(u16 channel)
{
	return QM_SDQCR_CHANNELS_POOL(channel + 1 - qm_channel_pool1);
}

/* For qman_volatile_dequeue(); Choose one PRECEDENCE. EXACT is optional. Use
 * NUMFRAMES(n) (6-bit) or NUMFRAMES_TILLEMPTY to fill in the frame-count. Use
 * FQID(n) to fill in the frame queue ID.
 */
#define QM_VDQCR_PRECEDENCE_VDQCR	0x0
#define QM_VDQCR_PRECEDENCE_SDQCR	0x80000000
#define QM_VDQCR_EXACT			0x40000000
#define QM_VDQCR_NUMFRAMES_MASK		0x3f000000
#define QM_VDQCR_NUMFRAMES_SET(n)	(((n) & 0x3f) << 24)
#define QM_VDQCR_NUMFRAMES_GET(n)	(((n) >> 24) & 0x3f)
#define QM_VDQCR_NUMFRAMES_TILLEMPTY	QM_VDQCR_NUMFRAMES_SET(0)

/* --- QMan data structures (and associated constants) --- */

/* Represents s/w corenet portal mapped data structures */
struct qm_eqcr_entry;	/* EQCR (EnQueue Command Ring) entries */
struct qm_dqrr_entry;	/* DQRR (DeQueue Response Ring) entries */
struct qm_mr_entry;	/* MR (Message Ring) entries */
struct qm_mc_command;	/* MC (Management Command) command */
struct qm_mc_result;	/* MC result */

#define QM_FD_FORMAT_SG		0x4
#define QM_FD_FORMAT_LONG	0x2
#define QM_FD_FORMAT_COMPOUND	0x1
enum qm_fd_format {
	/*
	 * 'contig' implies a contiguous buffer, whereas 'sg' implies a
	 * scatter-gather table. 'big' implies a 29-bit length with no offset
	 * field, otherwise length is 20-bit and offset is 9-bit. 'compound'
	 * implies a s/g-like table, where each entry itself represents a frame
	 * (contiguous or scatter-gather) and the 29-bit "length" is
	 * interpreted purely for congestion calculations, ie. a "congestion
	 * weight".
	 */
	qm_fd_contig = 0,
	qm_fd_contig_big = QM_FD_FORMAT_LONG,
	qm_fd_sg = QM_FD_FORMAT_SG,
	qm_fd_sg_big = QM_FD_FORMAT_SG | QM_FD_FORMAT_LONG,
	qm_fd_compound = QM_FD_FORMAT_COMPOUND
};

/* Capitalised versions are un-typed but can be used in static expressions */
#define QM_FD_CONTIG	0
#define QM_FD_CONTIG_BIG QM_FD_FORMAT_LONG
#define QM_FD_SG	QM_FD_FORMAT_SG
#define QM_FD_SG_BIG	(QM_FD_FORMAT_SG | QM_FD_FORMAT_LONG)
#define QM_FD_COMPOUND	QM_FD_FORMAT_COMPOUND

/* "Frame Descriptor (FD)" */
struct qm_fd {
	union {
		struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			u8 dd:2;	/* dynamic debug */
			u8 liodn_offset:6;
			u8 bpid:8;	/* Buffer Pool ID */
			u8 eliodn_offset:4;
			u8 __reserved:4;
			u8 addr_hi;	/* high 8-bits of 40-bit address */
			u32 addr_lo;	/* low 32-bits of 40-bit address */
#else
			u8 liodn_offset:6;
			u8 dd:2;	/* dynamic debug */
			u8 bpid:8;	/* Buffer Pool ID */
			u8 __reserved:4;
			u8 eliodn_offset:4;
			u8 addr_hi;	/* high 8-bits of 40-bit address */
			u32 addr_lo;	/* low 32-bits of 40-bit address */
#endif
		};
		struct {
			u64 __notaddress:24;
			/* More efficient address accessor */
			u64 addr:40;
		};
		u64 opaque_addr;
	};
	/* The 'format' field indicates the interpretation of the remaining 29
	 * bits of the 32-bit word. For packing reasons, it is duplicated in the
	 * other union elements. Note, union'd structs are difficult to use with
	 * static initialisation under gcc, in which case use the "opaque" form
	 * with one of the macros.
	 */
	union {
		/* For easier/faster copying of this part of the fd (eg. from a
		 * DQRR entry to an EQCR entry) copy 'opaque'
		 */
		u32 opaque;
		/* If 'format' is _contig or _sg, 20b length and 9b offset */
		struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			enum qm_fd_format format:3;
			u16 offset:9;
			u32 length20:20;
#else
			u32 length20:20;
			u16 offset:9;
			enum qm_fd_format format:3;
#endif
		};
		/* If 'format' is _contig_big or _sg_big, 29b length */
		struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			enum qm_fd_format _format1:3;
			u32 length29:29;
#else
			u32 length29:29;
			enum qm_fd_format _format1:3;
#endif
		};
		/* If 'format' is _compound, 29b "congestion weight" */
		struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			enum qm_fd_format _format2:3;
			u32 cong_weight:29;
#else
			u32 cong_weight:29;
			enum qm_fd_format _format2:3;
#endif
		};
	};
	union {
		u32 cmd;
		u32 status;
	};
} __attribute__((aligned(8)));
#define QM_FD_DD_NULL		0x00
#define QM_FD_PID_MASK		0x3f
static inline u64 qm_fd_addr_get64(const struct qm_fd *fd)
{
	return fd->addr;
}

static inline dma_addr_t qm_fd_addr(const struct qm_fd *fd)
{
	return (dma_addr_t)fd->addr;
}

/* Macro, so we compile better if 'v' isn't always 64-bit */
#define qm_fd_addr_set64(fd, v) \
	do { \
		struct qm_fd *__fd931 = (fd); \
		__fd931->addr = v; \
	} while (0)

/* Scatter/Gather table entry */
struct qm_sg_entry {
	union {
		struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			u8 __reserved1[3];
			u8 addr_hi;	/* high 8-bits of 40-bit address */
			u32 addr_lo;	/* low 32-bits of 40-bit address */
#else
			u32 addr_lo;	/* low 32-bits of 40-bit address */
			u8 addr_hi;	/* high 8-bits of 40-bit address */
			u8 __reserved1[3];
#endif
		};
		struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			u64 __notaddress:24;
			u64 addr:40;
#else
			u64 addr:40;
			u64 __notaddress:24;
#endif
		};
		u64 opaque;
	};
	union {
		struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			u32 extension:1;	/* Extension bit */
			u32 final:1;		/* Final bit */
			u32 length:30;
#else
			u32 length:30;
			u32 final:1;		/* Final bit */
			u32 extension:1;	/* Extension bit */
#endif
		};
		u32 val;
	};
	u8 __reserved2;
	u8 bpid;
	union {
		struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			u16 __reserved3:3;
			u16 offset:13;
#else
			u16 offset:13;
			u16 __reserved3:3;
#endif
		};
		u16 val_off;
	};
} __packed;
static inline u64 qm_sg_entry_get64(const struct qm_sg_entry *sg)
{
	return sg->addr;
}

static inline dma_addr_t qm_sg_addr(const struct qm_sg_entry *sg)
{
	return (dma_addr_t)sg->addr;
}

/* Macro, so we compile better if 'v' isn't always 64-bit */
#define qm_sg_entry_set64(sg, v) \
	do { \
		struct qm_sg_entry *__sg931 = (sg); \
		__sg931->addr = v; \
	} while (0)

/* See 1.5.8.1: "Enqueue Command" */
struct qm_eqcr_entry {
	u8 __dont_write_directly__verb;
	u8 dca;
	u16 seqnum;
	u32 orp;	/* 24-bit */
	u32 fqid;	/* 24-bit */
	u32 tag;
	struct qm_fd fd;
	u8 __reserved3[32];
} __packed;


/* "Frame Dequeue Response" */
struct qm_dqrr_entry {
	u8 verb;
	u8 stat;
	u16 seqnum;	/* 15-bit */
	u8 tok;
	u8 __reserved2[3];
	u32 fqid;	/* 24-bit */
	u32 contextB;
	struct qm_fd fd;
	u8 __reserved4[32];
};

#define QM_DQRR_VERB_VBIT		0x80
#define QM_DQRR_VERB_MASK		0x7f	/* where the verb contains; */
#define QM_DQRR_VERB_FRAME_DEQUEUE	0x60	/* "this format" */
#define QM_DQRR_STAT_FQ_EMPTY		0x80	/* FQ empty */
#define QM_DQRR_STAT_FQ_HELDACTIVE	0x40	/* FQ held active */
#define QM_DQRR_STAT_FQ_FORCEELIGIBLE	0x20	/* FQ was force-eligible'd */
#define QM_DQRR_STAT_FD_VALID		0x10	/* has a non-NULL FD */
#define QM_DQRR_STAT_UNSCHEDULED	0x02	/* Unscheduled dequeue */
#define QM_DQRR_STAT_DQCR_EXPIRED	0x01	/* VDQCR or PDQCR expired*/


/* "ERN Message Response" */
/* "FQ State Change Notification" */
struct qm_mr_entry {
	u8 verb;
	union {
		struct {
			u8 dca;
			u16 seqnum;
			u8 rc;		/* Rejection Code */
			u32 orp:24;
			u32 fqid;	/* 24-bit */
			u32 tag;
			struct qm_fd fd;
		} __packed ern;
		struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			u8 colour:2;	/* See QM_MR_DCERN_COLOUR_* */
			u8 __reserved1:4;
			enum qm_dc_portal portal:2;
#else
			enum qm_dc_portal portal:3;
			u8 __reserved1:3;
			u8 colour:2;	/* See QM_MR_DCERN_COLOUR_* */
#endif
			u16 __reserved2;
			u8 rc;		/* Rejection Code */
			u32 __reserved3:24;
			u32 fqid;	/* 24-bit */
			u32 tag;
			struct qm_fd fd;
		} __packed dcern;
		struct {
			u8 fqs;		/* Frame Queue Status */
			u8 __reserved1[6];
			u32 fqid;	/* 24-bit */
			u32 contextB;
			u8 __reserved2[16];
		} __packed fq;		/* FQRN/FQRNI/FQRL/FQPN */
	};
	u8 __reserved2[32];
} __packed;
#define QM_MR_VERB_VBIT			0x80
/*
 * ERNs originating from direct-connect portals ("dcern") use 0x20 as a verb
 * which would be invalid as a s/w enqueue verb. A s/w ERN can be distinguished
 * from the other MR types by noting if the 0x20 bit is unset.
 */
#define QM_MR_VERB_TYPE_MASK		0x27
#define QM_MR_VERB_DC_ERN		0x20
#define QM_MR_VERB_FQRN			0x21
#define QM_MR_VERB_FQRNI		0x22
#define QM_MR_VERB_FQRL			0x23
#define QM_MR_VERB_FQPN			0x24
#define QM_MR_RC_MASK			0xf0	/* contains one of; */
#define QM_MR_RC_CGR_TAILDROP		0x00
#define QM_MR_RC_WRED			0x10
#define QM_MR_RC_ERROR			0x20
#define QM_MR_RC_ORPWINDOW_EARLY	0x30
#define QM_MR_RC_ORPWINDOW_LATE		0x40
#define QM_MR_RC_FQ_TAILDROP		0x50
#define QM_MR_RC_ORPWINDOW_RETIRED	0x60
#define QM_MR_RC_ORP_ZERO		0x70
#define QM_MR_FQS_ORLPRESENT		0x02	/* ORL fragments to come */
#define QM_MR_FQS_NOTEMPTY		0x01	/* FQ has enqueued frames */
#define QM_MR_DCERN_COLOUR_GREEN	0x00
#define QM_MR_DCERN_COLOUR_YELLOW	0x01
#define QM_MR_DCERN_COLOUR_RED		0x02
#define QM_MR_DCERN_COLOUR_OVERRIDE	0x03
/*
 * An identical structure of FQD fields is present in the "Init FQ" command and
 * the "Query FQ" result, it's suctioned out into the "struct qm_fqd" type.
 * Within that, the 'stashing' and 'taildrop' pieces are also factored out, the
 * latter has two inlines to assist with converting to/from the mant+exp
 * representation.
 */
struct qm_fqd_stashing {
	/* See QM_STASHING_EXCL_<...> */
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	u8 exclusive;
	u8 __reserved1:2;
	/* Numbers of cachelines */
	u8 annotation_cl:2;
	u8 data_cl:2;
	u8 context_cl:2;
#else
	u8 context_cl:2;
	u8 data_cl:2;
	u8 annotation_cl:2;
	u8 __reserved1:2;
	u8 exclusive;
#endif
} __packed;
struct qm_fqd_taildrop {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	u16 __reserved1:3;
	u16 mant:8;
	u16 exp:5;
#else
	u16 exp:5;
	u16 mant:8;
	u16 __reserved1:3;
#endif
} __packed;
struct qm_fqd_oac {
	/* "Overhead Accounting Control", see QM_OAC_<...> */
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	u8 oac:2; /* "Overhead Accounting Control" */
	u8 __reserved1:6;
#else
	u8 __reserved1:6;
	u8 oac:2; /* "Overhead Accounting Control" */
#endif
	/* Two's-complement value (-128 to +127) */
	signed char oal; /* "Overhead Accounting Length" */
} __packed;
struct qm_fqd {
	union {
		u8 orpc;
		struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			u8 __reserved1:2;
			u8 orprws:3;
			u8 oa:1;
			u8 olws:2;
#else
			u8 olws:2;
			u8 oa:1;
			u8 orprws:3;
			u8 __reserved1:2;
#endif
		} __packed;
	};
	u8 cgid;
	u16 fq_ctrl;	/* See QM_FQCTRL_<...> */
	union {
		u16 dest_wq;
		struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			u16 channel:13; /* qm_channel */
			u16 wq:3;
#else
			u16 wq:3;
			u16 channel:13; /* qm_channel */
#endif
		} __packed dest;
	};
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	u16 __reserved2:1;
	u16 ics_cred:15;
#else
	u16 __reserved2:1;
	u16 ics_cred:15;
#endif
	/*
	 * For "Initialize Frame Queue" commands, the write-enable mask
	 * determines whether 'td' or 'oac_init' is observed. For query
	 * commands, this field is always 'td', and 'oac_query' (below) reflects
	 * the Overhead ACcounting values.
	 */
	union {
		uint16_t opaque_td;
		struct qm_fqd_taildrop td;
		struct qm_fqd_oac oac_init;
	};
	u32 context_b;
	union {
		/* Treat it as 64-bit opaque */
		u64 opaque;
		struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			u32 hi;
			u32 lo;
#else
			u32 lo;
			u32 hi;
#endif
		};
		/* Treat it as s/w portal stashing config */
		/* see "FQD Context_A field used for [...]" */
		struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			struct qm_fqd_stashing stashing;
			/*
			 * 48-bit address of FQ context to
			 * stash, must be cacheline-aligned
			 */
			u16 context_hi;
			u32 context_lo;
#else
			u32 context_lo;
			u16 context_hi;
			struct qm_fqd_stashing stashing;
#endif
		} __packed;
	} context_a;
	struct qm_fqd_oac oac_query;
} __packed;
/* 64-bit converters for context_hi/lo */
static inline u64 qm_fqd_stashing_get64(const struct qm_fqd *fqd)
{
	return ((u64)fqd->context_a.context_hi << 32) |
		(u64)fqd->context_a.context_lo;
}

static inline dma_addr_t qm_fqd_stashing_addr(const struct qm_fqd *fqd)
{
	return (dma_addr_t)qm_fqd_stashing_get64(fqd);
}

static inline u64 qm_fqd_context_a_get64(const struct qm_fqd *fqd)
{
	return ((u64)fqd->context_a.hi << 32) |
		(u64)fqd->context_a.lo;
}

static inline void qm_fqd_stashing_set64(struct qm_fqd *fqd, u64 addr)
{
		fqd->context_a.context_hi = upper_32_bits(addr);
		fqd->context_a.context_lo = lower_32_bits(addr);
}

static inline void qm_fqd_context_a_set64(struct qm_fqd *fqd, u64 addr)
{
	fqd->context_a.hi = upper_32_bits(addr);
	fqd->context_a.lo = lower_32_bits(addr);
}

/* convert a threshold value into mant+exp representation */
static inline int qm_fqd_taildrop_set(struct qm_fqd_taildrop *td, u32 val,
				      int roundup)
{
	u32 e = 0;
	int oddbit = 0;

	if (val > 0xe0000000)
		return -ERANGE;
	while (val > 0xff) {
		oddbit = val & 1;
		val >>= 1;
		e++;
		if (roundup && oddbit)
			val++;
	}
	td->exp = e;
	td->mant = val;
	return 0;
}

/* and the other direction */
static inline u32 qm_fqd_taildrop_get(const struct qm_fqd_taildrop *td)
{
	return (u32)td->mant << td->exp;
}


/* See "Frame Queue Descriptor (FQD)" */
/* Frame Queue Descriptor (FQD) field 'fq_ctrl' uses these constants */
#define QM_FQCTRL_MASK		0x07ff	/* 'fq_ctrl' flags; */
#define QM_FQCTRL_CGE		0x0400	/* Congestion Group Enable */
#define QM_FQCTRL_TDE		0x0200	/* Tail-Drop Enable */
#define QM_FQCTRL_ORP		0x0100	/* ORP Enable */
#define QM_FQCTRL_CTXASTASHING	0x0080	/* Context-A stashing */
#define QM_FQCTRL_CPCSTASH	0x0040	/* CPC Stash Enable */
#define QM_FQCTRL_FORCESFDR	0x0008	/* High-priority SFDRs */
#define QM_FQCTRL_AVOIDBLOCK	0x0004	/* Don't block active */
#define QM_FQCTRL_HOLDACTIVE	0x0002	/* Hold active in portal */
#define QM_FQCTRL_PREFERINCACHE	0x0001	/* Aggressively cache FQD */
#define QM_FQCTRL_LOCKINCACHE	QM_FQCTRL_PREFERINCACHE /* older naming */

/* See "FQD Context_A field used for [...] */
/* Frame Queue Descriptor (FQD) field 'CONTEXT_A' uses these constants */
#define QM_STASHING_EXCL_ANNOTATION	0x04
#define QM_STASHING_EXCL_DATA		0x02
#define QM_STASHING_EXCL_CTX		0x01

/* See "Intra Class Scheduling" */
/* FQD field 'OAC' (Overhead ACcounting) uses these constants */
#define QM_OAC_ICS		0x2 /* Accounting for Intra-Class Scheduling */
#define QM_OAC_CG		0x1 /* Accounting for Congestion Groups */

/*
 * This struct represents the 32-bit "WR_PARM_[GYR]" parameters in CGR fields
 * and associated commands/responses. The WRED parameters are calculated from
 * these fields as follows;
 *   MaxTH = MA * (2 ^ Mn)
 *   Slope = SA / (2 ^ Sn)
 *    MaxP = 4 * (Pn + 1)
 */
struct qm_cgr_wr_parm {
	union {
		u32 word;
		struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			u32 MA:8;
			u32 Mn:5;
			u32 SA:7; /* must be between 64-127 */
			u32 Sn:6;
			u32 Pn:6;
#else
			u32 Pn:6;
			u32 Sn:6;
			u32 SA:7; /* must be between 64-127 */
			u32 Mn:5;
			u32 MA:8;
#endif
		} __packed;
	};
} __packed;
/*
 * This struct represents the 13-bit "CS_THRES" CGR field. In the corresponding
 * management commands, this is padded to a 16-bit structure field, so that's
 * how we represent it here. The congestion state threshold is calculated from
 * these fields as follows;
 *   CS threshold = TA * (2 ^ Tn)
 */
struct qm_cgr_cs_thres {
	union {
		u16 hword;
		struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			u16 __reserved:3;
			u16 TA:8;
			u16 Tn:5;
#else
			u16 Tn:5;
			u16 TA:8;
			u16 __reserved:3;
#endif
		} __packed;
	};
} __packed;
/*
 * This identical structure of CGR fields is present in the "Init/Modify CGR"
 * commands and the "Query CGR" result. It's suctioned out here into its own
 * struct.
 */
struct __qm_mc_cgr {
	struct qm_cgr_wr_parm wr_parm_g;
	struct qm_cgr_wr_parm wr_parm_y;
	struct qm_cgr_wr_parm wr_parm_r;
	u8 wr_en_g;	/* boolean, use QM_CGR_EN */
	u8 wr_en_y;	/* boolean, use QM_CGR_EN */
	u8 wr_en_r;	/* boolean, use QM_CGR_EN */
	u8 cscn_en;	/* boolean, use QM_CGR_EN */
	union {
		struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			u16 cscn_targ_upd_ctrl; /* use QM_CSCN_TARG_UDP_ */
			u16 cscn_targ_dcp_low;  /* CSCN_TARG_DCP low-16bits */
#else
			u16 cscn_targ_dcp_low;  /* CSCN_TARG_DCP low-16bits */
			u16 cscn_targ_upd_ctrl; /* use QM_CSCN_TARG_UDP_ */
#endif
		};
		u32 cscn_targ;	/* use QM_CGR_TARG_* */
	};
	u8 cstd_en;	/* boolean, use QM_CGR_EN */
	u8 cs;		/* boolean, only used in query response */
	union {
		struct qm_cgr_cs_thres cs_thres;
		/* use qm_cgr_cs_thres_set64() */
		u16 __cs_thres;
	};
	u8 mode;	/* QMAN_CGR_MODE_FRAME not supported in rev1.0 */
} __packed;
#define QM_CGR_EN		0x01 /* For wr_en_*, cscn_en, cstd_en */
#define QM_CGR_TARG_UDP_CTRL_WRITE_BIT	0x8000 /* value written to portal bit*/
#define QM_CGR_TARG_UDP_CTRL_DCP	0x4000 /* 0: SWP, 1: DCP */
#define QM_CGR_TARG_PORTAL(n)	(0x80000000 >> (n)) /* s/w portal, 0-9 */
#define QM_CGR_TARG_FMAN0	0x00200000 /* direct-connect portal: fman0 */
#define QM_CGR_TARG_FMAN1	0x00100000 /*			   : fman1 */
/* Convert CGR thresholds to/from "cs_thres" format */
static inline u64 qm_cgr_cs_thres_get64(const struct qm_cgr_cs_thres *th)
{
	return (u64)th->TA << th->Tn;
}

static inline int qm_cgr_cs_thres_set64(struct qm_cgr_cs_thres *th, u64 val,
					int roundup)
{
	u32 e = 0;
	int oddbit = 0;

	while (val > 0xff) {
		oddbit = val & 1;
		val >>= 1;
		e++;
		if (roundup && oddbit)
			val++;
	}
	th->Tn = e;
	th->TA = val;
	return 0;
}

/* See 1.5.8.5.1: "Initialize FQ" */
/* See 1.5.8.5.2: "Query FQ" */
/* See 1.5.8.5.3: "Query FQ Non-Programmable Fields" */
/* See 1.5.8.5.4: "Alter FQ State Commands " */
/* See 1.5.8.6.1: "Initialize/Modify CGR" */
/* See 1.5.8.6.2: "CGR Test Write" */
/* See 1.5.8.6.3: "Query CGR" */
/* See 1.5.8.6.4: "Query Congestion Group State" */
struct qm_mcc_initfq {
	u8 __reserved1;
	u16 we_mask;	/* Write Enable Mask */
	u32 fqid;	/* 24-bit */
	u16 count;	/* Initialises 'count+1' FQDs */
	struct qm_fqd fqd; /* the FQD fields go here */
	u8 __reserved3[30];
} __packed;
struct qm_mcc_queryfq {
	u8 __reserved1[3];
	u32 fqid;	/* 24-bit */
	u8 __reserved2[56];
} __packed;
struct qm_mcc_queryfq_np {
	u8 __reserved1[3];
	u32 fqid;	/* 24-bit */
	u8 __reserved2[56];
} __packed;
struct qm_mcc_alterfq {
	u8 __reserved1[3];
	u32 fqid;	/* 24-bit */
	u8 __reserved2;
	u8 count;	/* number of consecutive FQID */
	u8 __reserved3[10];
	u32 context_b;	/* frame queue context b */
	u8 __reserved4[40];
} __packed;
struct qm_mcc_initcgr {
	u8 __reserved1;
	u16 we_mask;	/* Write Enable Mask */
	struct __qm_mc_cgr cgr;	/* CGR fields */
	u8 __reserved2[2];
	u8 cgid;
	u8 __reserved4[32];
} __packed;
struct qm_mcc_cgrtestwrite {
	u8 __reserved1[2];
	u8 i_bcnt_hi:8;/* high 8-bits of 40-bit "Instant" */
	u32 i_bcnt_lo;	/* low 32-bits of 40-bit */
	u8 __reserved2[23];
	u8 cgid;
	u8 __reserved3[32];
} __packed;
struct qm_mcc_querycgr {
	u8 __reserved1[30];
	u8 cgid;
	u8 __reserved2[32];
} __packed;
struct qm_mcc_querycongestion {
	u8 __reserved[63];
} __packed;
struct qm_mcc_querywq {
	u8 __reserved;
	/* select channel if verb != QUERYWQ_DEDICATED */
	union {
		u16 channel_wq; /* ignores wq (3 lsbits) */
		struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			u16 id:13; /* qm_channel */
			u16 __reserved1:3;
#else
			u16 __reserved1:3;
			u16 id:13; /* qm_channel */
#endif
		} __packed channel;
	};
	u8 __reserved2[60];
} __packed;

struct qm_mc_command {
	u8 __dont_write_directly__verb;
	union {
		struct qm_mcc_initfq initfq;
		struct qm_mcc_queryfq queryfq;
		struct qm_mcc_queryfq_np queryfq_np;
		struct qm_mcc_alterfq alterfq;
		struct qm_mcc_initcgr initcgr;
		struct qm_mcc_cgrtestwrite cgrtestwrite;
		struct qm_mcc_querycgr querycgr;
		struct qm_mcc_querycongestion querycongestion;
		struct qm_mcc_querywq querywq;
	};
} __packed;

/* INITFQ-specific flags */
#define QM_INITFQ_WE_MASK		0x01ff	/* 'Write Enable' flags; */
#define QM_INITFQ_WE_OAC		0x0100
#define QM_INITFQ_WE_ORPC		0x0080
#define QM_INITFQ_WE_CGID		0x0040
#define QM_INITFQ_WE_FQCTRL		0x0020
#define QM_INITFQ_WE_DESTWQ		0x0010
#define QM_INITFQ_WE_ICSCRED		0x0008
#define QM_INITFQ_WE_TDTHRESH		0x0004
#define QM_INITFQ_WE_CONTEXTB		0x0002
#define QM_INITFQ_WE_CONTEXTA		0x0001
/* INITCGR/MODIFYCGR-specific flags */
#define QM_CGR_WE_MASK			0x07ff	/* 'Write Enable Mask'; */
#define QM_CGR_WE_WR_PARM_G		0x0400
#define QM_CGR_WE_WR_PARM_Y		0x0200
#define QM_CGR_WE_WR_PARM_R		0x0100
#define QM_CGR_WE_WR_EN_G		0x0080
#define QM_CGR_WE_WR_EN_Y		0x0040
#define QM_CGR_WE_WR_EN_R		0x0020
#define QM_CGR_WE_CSCN_EN		0x0010
#define QM_CGR_WE_CSCN_TARG		0x0008
#define QM_CGR_WE_CSTD_EN		0x0004
#define QM_CGR_WE_CS_THRES		0x0002
#define QM_CGR_WE_MODE			0x0001

struct qm_mcr_initfq {
	u8 __reserved1[62];
} __packed;
struct qm_mcr_queryfq {
	u8 __reserved1[8];
	struct qm_fqd fqd;	/* the FQD fields are here */
	u8 __reserved2[30];
} __packed;
struct qm_mcr_queryfq_np {
	u8 __reserved1;
	u8 state;	/* QM_MCR_NP_STATE_*** */
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	u8 __reserved2;
	u32 fqd_link:24;
	u16 __reserved3:2;
	u16 odp_seq:14;
	u16 __reserved4:2;
	u16 orp_nesn:14;
	u16 __reserved5:1;
	u16 orp_ea_hseq:15;
	u16 __reserved6:1;
	u16 orp_ea_tseq:15;
	u8 __reserved7;
	u32 orp_ea_hptr:24;
	u8 __reserved8;
	u32 orp_ea_tptr:24;
	u8 __reserved9;
	u32 pfdr_hptr:24;
	u8 __reserved10;
	u32 pfdr_tptr:24;
	u8 __reserved11[5];
	u8 __reserved12:7;
	u8 is:1;
	u16 ics_surp;
	u32 byte_cnt;
	u8 __reserved13;
	u32 frm_cnt:24;
	u32 __reserved14;
	u16 ra1_sfdr;	/* QM_MCR_NP_RA1_*** */
	u16 ra2_sfdr;	/* QM_MCR_NP_RA2_*** */
	u16 __reserved15;
	u16 od1_sfdr;	/* QM_MCR_NP_OD1_*** */
	u16 od2_sfdr;	/* QM_MCR_NP_OD2_*** */
	u16 od3_sfdr;	/* QM_MCR_NP_OD3_*** */
#else
	u8 __reserved2;
	u32 fqd_link:24;

	u16 odp_seq:14;
	u16 __reserved3:2;

	u16 orp_nesn:14;
	u16 __reserved4:2;

	u16 orp_ea_hseq:15;
	u16 __reserved5:1;

	u16 orp_ea_tseq:15;
	u16 __reserved6:1;

	u8 __reserved7;
	u32 orp_ea_hptr:24;

	u8 __reserved8;
	u32 orp_ea_tptr:24;

	u8 __reserved9;
	u32 pfdr_hptr:24;

	u8 __reserved10;
	u32 pfdr_tptr:24;

	u8 __reserved11[5];
	u8 is:1;
	u8 __reserved12:7;
	u16 ics_surp;
	u32 byte_cnt;
	u8 __reserved13;
	u32 frm_cnt:24;
	u32 __reserved14;
	u16 ra1_sfdr;	/* QM_MCR_NP_RA1_*** */
	u16 ra2_sfdr;	/* QM_MCR_NP_RA2_*** */
	u16 __reserved15;
	u16 od1_sfdr;	/* QM_MCR_NP_OD1_*** */
	u16 od2_sfdr;	/* QM_MCR_NP_OD2_*** */
	u16 od3_sfdr;	/* QM_MCR_NP_OD3_*** */
#endif
} __packed;

struct qm_mcr_alterfq {
	u8 fqs;		/* Frame Queue Status */
	u8 __reserved1[61];
} __packed;
struct qm_mcr_initcgr {
	u8 __reserved1[62];
} __packed;
struct qm_mcr_cgrtestwrite {
	u16 __reserved1;
	struct __qm_mc_cgr cgr; /* CGR fields */
	u8 __reserved2[3];
	u32 __reserved3:24;
	u32 i_bcnt_hi:8;/* high 8-bits of 40-bit "Instant" */
	u32 i_bcnt_lo;	/* low 32-bits of 40-bit */
	u32 __reserved4:24;
	u32 a_bcnt_hi:8;/* high 8-bits of 40-bit "Average" */
	u32 a_bcnt_lo;	/* low 32-bits of 40-bit */
	u16 lgt;	/* Last Group Tick */
	u16 wr_prob_g;
	u16 wr_prob_y;
	u16 wr_prob_r;
	u8 __reserved5[8];
} __packed;
struct qm_mcr_querycgr {
	u16 __reserved1;
	struct __qm_mc_cgr cgr; /* CGR fields */
	u8 __reserved2[3];
	union {
		struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			u32 __reserved3:24;
			u32 i_bcnt_hi:8;/* high 8-bits of 40-bit "Instant" */
			u32 i_bcnt_lo;	/* low 32-bits of 40-bit */
#else
			u32 i_bcnt_lo;	/* low 32-bits of 40-bit */
			u32 i_bcnt_hi:8;/* high 8-bits of 40-bit "Instant" */
			u32 __reserved3:24;
#endif
		};
		u64 i_bcnt;
	};
	union {
		struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			u32 __reserved4:24;
			u32 a_bcnt_hi:8;/* high 8-bits of 40-bit "Average" */
			u32 a_bcnt_lo;	/* low 32-bits of 40-bit */
#else
			u32 a_bcnt_lo;	/* low 32-bits of 40-bit */
			u32 a_bcnt_hi:8;/* high 8-bits of 40-bit "Average" */
			u32 __reserved4:24;
#endif
		};
		u64 a_bcnt;
	};
	union {
		u32 cscn_targ_swp[4];
		u8 __reserved5[16];
	};
} __packed;

struct __qm_mcr_querycongestion {
	u32 state[8];
};

struct qm_mcr_querycongestion {
	u8 __reserved[30];
	/* Access this struct using QM_MCR_QUERYCONGESTION() */
	struct __qm_mcr_querycongestion state;
} __packed;
struct qm_mcr_querywq {
	union {
		u16 channel_wq; /* ignores wq (3 lsbits) */
		struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			u16 id:13; /* qm_channel */
			u16 __reserved:3;
#else
			u16 __reserved:3;
			u16 id:13; /* qm_channel */
#endif
		} __packed channel;
	};
	u8 __reserved[28];
	u32 wq_len[8];
} __packed;

struct qm_mc_result {
	u8 verb;
	u8 result;
	union {
		struct qm_mcr_initfq initfq;
		struct qm_mcr_queryfq queryfq;
		struct qm_mcr_queryfq_np queryfq_np;
		struct qm_mcr_alterfq alterfq;
		struct qm_mcr_initcgr initcgr;
		struct qm_mcr_cgrtestwrite cgrtestwrite;
		struct qm_mcr_querycgr querycgr;
		struct qm_mcr_querycongestion querycongestion;
		struct qm_mcr_querywq querywq;
	};
} __packed;

#define QM_MCR_VERB_RRID		0x80
#define QM_MCR_VERB_MASK		QM_MCC_VERB_MASK
#define QM_MCR_VERB_INITFQ_PARKED	QM_MCC_VERB_INITFQ_PARKED
#define QM_MCR_VERB_INITFQ_SCHED	QM_MCC_VERB_INITFQ_SCHED
#define QM_MCR_VERB_QUERYFQ		QM_MCC_VERB_QUERYFQ
#define QM_MCR_VERB_QUERYFQ_NP		QM_MCC_VERB_QUERYFQ_NP
#define QM_MCR_VERB_QUERYWQ		QM_MCC_VERB_QUERYWQ
#define QM_MCR_VERB_QUERYWQ_DEDICATED	QM_MCC_VERB_QUERYWQ_DEDICATED
#define QM_MCR_VERB_ALTER_SCHED		QM_MCC_VERB_ALTER_SCHED
#define QM_MCR_VERB_ALTER_FE		QM_MCC_VERB_ALTER_FE
#define QM_MCR_VERB_ALTER_RETIRE	QM_MCC_VERB_ALTER_RETIRE
#define QM_MCR_VERB_ALTER_OOS		QM_MCC_VERB_ALTER_OOS
#define QM_MCR_RESULT_NULL		0x00
#define QM_MCR_RESULT_OK		0xf0
#define QM_MCR_RESULT_ERR_FQID		0xf1
#define QM_MCR_RESULT_ERR_FQSTATE	0xf2
#define QM_MCR_RESULT_ERR_NOTEMPTY	0xf3	/* OOS fails if FQ is !empty */
#define QM_MCR_RESULT_ERR_BADCHANNEL	0xf4
#define QM_MCR_RESULT_PENDING		0xf8
#define QM_MCR_RESULT_ERR_BADCOMMAND	0xff
#define QM_MCR_NP_STATE_FE		0x10
#define QM_MCR_NP_STATE_R		0x08
#define QM_MCR_NP_STATE_MASK		0x07	/* Reads FQD::STATE; */
#define QM_MCR_NP_STATE_OOS		0x00
#define QM_MCR_NP_STATE_RETIRED		0x01
#define QM_MCR_NP_STATE_TEN_SCHED	0x02
#define QM_MCR_NP_STATE_TRU_SCHED	0x03
#define QM_MCR_NP_STATE_PARKED		0x04
#define QM_MCR_NP_STATE_ACTIVE		0x05
#define QM_MCR_NP_PTR_MASK		0x07ff	/* for RA[12] & OD[123] */
#define QM_MCR_NP_RA1_NRA(v)		(((v) >> 14) & 0x3)	/* FQD::NRA */
#define QM_MCR_NP_RA2_IT(v)		(((v) >> 14) & 0x1)	/* FQD::IT */
#define QM_MCR_NP_OD1_NOD(v)		(((v) >> 14) & 0x3)	/* FQD::NOD */
#define QM_MCR_NP_OD3_NPC(v)		(((v) >> 14) & 0x3)	/* FQD::NPC */
#define QM_MCR_FQS_ORLPRESENT		0x02	/* ORL fragments to come */
#define QM_MCR_FQS_NOTEMPTY		0x01	/* FQ has enqueued frames */
/* This extracts the state for congestion group 'n' from a query response.
 * Eg.
 *   u8 cgr = [...];
 *   struct qm_mc_result *res = [...];
 *   printf("congestion group %d congestion state: %d\n", cgr,
 *       QM_MCR_QUERYCONGESTION(&res->querycongestion.state, cgr));
 */
#define __CGR_WORD(num)		(num >> 5)
#define __CGR_SHIFT(num)	(num & 0x1f)
#define __CGR_NUM		(sizeof(struct __qm_mcr_querycongestion) << 3)
static inline int QM_MCR_QUERYCONGESTION(struct __qm_mcr_querycongestion *p,
					 u8 cgr)
{
	return p->state[__CGR_WORD(cgr)] & (0x80000000 >> __CGR_SHIFT(cgr));
}

	/* Portal and Frame Queues */
/* Represents a managed portal */
struct qman_portal;

/*
 * This object type represents QMan frame queue descriptors (FQD), it is
 * cacheline-aligned, and initialised by qman_create_fq(). The structure is
 * defined further down.
 */
struct qman_fq;

/*
 * This object type represents a QMan congestion group, it is defined further
 * down.
 */
struct qman_cgr;

/*
 * This enum, and the callback type that returns it, are used when handling
 * dequeued frames via DQRR. Note that for "null" callbacks registered with the
 * portal object (for handling dequeues that do not demux because context_b is
 * NULL), the return value *MUST* be qman_cb_dqrr_consume.
 */
enum qman_cb_dqrr_result {
	/* DQRR entry can be consumed */
	qman_cb_dqrr_consume,
	/* Like _consume, but requests parking - FQ must be held-active */
	qman_cb_dqrr_park,
	/* Does not consume, for DCA mode only. This allows out-of-order
	 * consumes by explicit calls to qman_dca() and/or the use of implicit
	 * DCA via EQCR entries.
	 */
	qman_cb_dqrr_defer,
	/*
	 * Stop processing without consuming this ring entry. Exits the current
	 * qman_p_poll_dqrr() or interrupt-handling, as appropriate. If within
	 * an interrupt handler, the callback would typically call
	 * qman_irqsource_remove(QM_PIRQ_DQRI) before returning this value,
	 * otherwise the interrupt will reassert immediately.
	 */
	qman_cb_dqrr_stop,
	/* Like qman_cb_dqrr_stop, but consumes the current entry. */
	qman_cb_dqrr_consume_stop
};

typedef enum qman_cb_dqrr_result (*qman_cb_dqrr)(struct qman_portal *qm,
					struct qman_fq *fq,
					const struct qm_dqrr_entry *dqrr);

/*
 * This callback type is used when handling ERNs, FQRNs and FQRLs via MR. They
 * are always consumed after the callback returns.
 */
typedef void (*qman_cb_mr)(struct qman_portal *qm, struct qman_fq *fq,
				const struct qm_mr_entry *msg);

/* This callback type is used when handling DCP ERNs */
typedef void (*qman_cb_dc_ern)(struct qman_portal *qm,
				const struct qm_mr_entry *msg);
/*
 * s/w-visible states. Ie. tentatively scheduled + truly scheduled + active +
 * held-active + held-suspended are just "sched". Things like "retired" will not
 * be assumed until it is complete (ie. QMAN_FQ_STATE_CHANGING is set until
 * then, to indicate it's completing and to gate attempts to retry the retire
 * command). Note, park commands do not set QMAN_FQ_STATE_CHANGING because it's
 * technically impossible in the case of enqueue DCAs (which refer to DQRR ring
 * index rather than the FQ that ring entry corresponds to), so repeated park
 * commands are allowed (if you're silly enough to try) but won't change FQ
 * state, and the resulting park notifications move FQs from "sched" to
 * "parked".
 */
enum qman_fq_state {
	qman_fq_state_oos,
	qman_fq_state_parked,
	qman_fq_state_sched,
	qman_fq_state_retired
};


/*
 * Frame queue objects (struct qman_fq) are stored within memory passed to
 * qman_create_fq(), as this allows stashing of caller-provided demux callback
 * pointers at no extra cost to stashing of (driver-internal) FQ state. If the
 * caller wishes to add per-FQ state and have it benefit from dequeue-stashing,
 * they should;
 *
 * (a) extend the qman_fq structure with their state; eg.
 *
 *     // myfq is allocated and driver_fq callbacks filled in;
 *     struct my_fq {
 *	   struct qman_fq base;
 *	   int an_extra_field;
 *	   [ ... add other fields to be associated with each FQ ...]
 *     } *myfq = some_my_fq_allocator();
 *     struct qman_fq *fq = qman_create_fq(fqid, flags, &myfq->base);
 *
 *     // in a dequeue callback, access extra fields from 'fq' via a cast;
 *     struct my_fq *myfq = (struct my_fq *)fq;
 *     do_something_with(myfq->an_extra_field);
 *     [...]
 *
 * (b) when and if configuring the FQ for context stashing, specify how ever
 *     many cachelines are required to stash 'struct my_fq', to accelerate not
 *     only the QMan driver but the callback as well.
 */

struct qman_fq_cb {
	qman_cb_dqrr dqrr;	/* for dequeued frames */
	qman_cb_mr ern;		/* for s/w ERNs */
	qman_cb_mr fqs;		/* frame-queue state changes*/
};

struct qman_fq {
	/* Caller of qman_create_fq() provides these demux callbacks */
	struct qman_fq_cb cb;
	/*
	 * These are internal to the driver, don't touch. In particular, they
	 * may change, be removed, or extended (so you shouldn't rely on
	 * sizeof(qman_fq) being a constant).
	 */
	spinlock_t fqlock;
	u32 fqid;
	/* DPDK Interface */
	void *dpaa_intf;

	volatile unsigned long flags;
	enum qman_fq_state state;
	int cgr_groupid;
	struct rb_node node;
};

/*
 * This callback type is used when handling congestion group entry/exit.
 * 'congested' is non-zero on congestion-entry, and zero on congestion-exit.
 */
typedef void (*qman_cb_cgr)(struct qman_portal *qm,
			    struct qman_cgr *cgr, int congested);

struct qman_cgr {
	/* Set these prior to qman_create_cgr() */
	u32 cgrid; /* 0..255, but u32 to allow specials like -1, 256, etc.*/
	qman_cb_cgr cb;
	/* These are private to the driver */
	u16 chan; /* portal channel this object is created on */
	struct list_head node;
};


#ifdef __cplusplus
}
#endif

#endif /* __FSL_QMAN_H */
