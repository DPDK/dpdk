/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2008-2012 Freescale Semiconductor, Inc.
 *
 */

#ifndef __FSL_QMAN_H
#define __FSL_QMAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <dpaa_rbtree.h>
#include <rte_eventdev.h>

/* FQ lookups (turn this on for 64bit user-space) */
#if (__WORDSIZE == 64)
#define CONFIG_FSL_QMAN_FQ_LOOKUP
/* if FQ lookups are supported, this controls the number of initialised,
 * s/w-consumed FQs that can be supported at any one time.
 */
#define CONFIG_FSL_QMAN_FQ_LOOKUP_MAX (32 * 1024)
#endif

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
struct __rte_aligned(8) qm_eqcr_entry {
	u8 __dont_write_directly__verb;
	u8 dca;
	u16 seqnum;
	u32 orp;	/* 24-bit */
	u32 fqid;	/* 24-bit */
	u32 tag;
	struct qm_fd fd; /* this has alignment 8 */
	u8 __reserved3[32];
} __packed;


/* "Frame Dequeue Response" */
struct __rte_aligned(8) qm_dqrr_entry {
	u8 verb;
	u8 stat;
	u16 seqnum;	/* 15-bit */
	u8 tok;
	u8 __reserved2[3];
	u32 fqid;	/* 24-bit */
	u32 contextB;
	struct qm_fd fd; /* this has alignment 8 */
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
	union {
		struct {
			u8 verb;
			u8 dca;
			u16 seqnum;
			u8 rc;		/* Rejection Code */
			u32 orp:24;
			u32 fqid;	/* 24-bit */
			u32 tag;
			struct qm_fd fd; /* this has alignment 8 */
		} __packed __rte_aligned(8) ern;
		struct {
			u8 verb;
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
			struct qm_fd fd; /* this has alignment 8 */
		} __packed __rte_aligned(8) dcern;
		struct {
			u8 verb;
			u8 fqs;		/* Frame Queue Status */
			u8 __reserved1[6];
			u32 fqid;	/* 24-bit */
			u32 contextB;
			u8 __reserved2[16];
		} __packed __rte_aligned(8) fq;	/* FQRN/FQRNI/FQRL/FQPN */
	};
	u8 __reserved2[32];
} __packed __rte_aligned(8);
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

typedef enum qman_cb_dqrr_result (*qman_dpdk_cb_dqrr)(void *event,
					struct qman_portal *qm,
					struct qman_fq *fq,
					const struct qm_dqrr_entry *dqrr,
					void **bd);

/* This callback type is used when handling buffers in dpdk pull mode */
typedef void (*qman_dpdk_pull_cb_dqrr)(struct qman_fq **fq,
					struct qm_dqrr_entry **dqrr,
					void **bufs,
					int num_bufs);

typedef void (*qman_dpdk_cb_prepare)(struct qm_dqrr_entry *dq, void **bufs);

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
	union { /* for dequeued frames */
		qman_dpdk_cb_dqrr dqrr_dpdk_cb;
		qman_dpdk_pull_cb_dqrr dqrr_dpdk_pull_cb;
		qman_cb_dqrr dqrr;
	};
	qman_dpdk_cb_prepare dqrr_prepare;
	qman_cb_mr ern;		/* for s/w ERNs */
	qman_cb_mr fqs;		/* frame-queue state changes*/
};

struct qman_fq {
	/* Caller of qman_create_fq() provides these demux callbacks */
	struct qman_fq_cb cb;

	u32 fqid_le;
	u16 ch_id;
	u8 cgr_groupid;
	u8 is_static;

	/* DPDK Interface */
	void *dpaa_intf;

	struct rte_event ev;
	/* affined portal in case of static queue */
	struct qman_portal *qp;

	volatile unsigned long flags;

	enum qman_fq_state state;
	u32 fqid;
	spinlock_t fqlock;

	struct rb_node node;
#ifdef CONFIG_FSL_QMAN_FQ_LOOKUP
	u32 key;
#endif
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

/* Flags to qman_create_fq() */
#define QMAN_FQ_FLAG_NO_ENQUEUE      0x00000001 /* can't enqueue */
#define QMAN_FQ_FLAG_NO_MODIFY       0x00000002 /* can only enqueue */
#define QMAN_FQ_FLAG_TO_DCPORTAL     0x00000004 /* consumed by CAAM/PME/Fman */
#define QMAN_FQ_FLAG_LOCKED          0x00000008 /* multi-core locking */
#define QMAN_FQ_FLAG_AS_IS           0x00000010 /* query h/w state */
#define QMAN_FQ_FLAG_DYNAMIC_FQID    0x00000020 /* (de)allocate fqid */

/* Flags to qman_destroy_fq() */
#define QMAN_FQ_DESTROY_PARKED       0x00000001 /* FQ can be parked or OOS */

/* Flags from qman_fq_state() */
#define QMAN_FQ_STATE_CHANGING       0x80000000 /* 'state' is changing */
#define QMAN_FQ_STATE_NE             0x40000000 /* retired FQ isn't empty */
#define QMAN_FQ_STATE_ORL            0x20000000 /* retired FQ has ORL */
#define QMAN_FQ_STATE_BLOCKOOS       0xe0000000 /* if any are set, no OOS */
#define QMAN_FQ_STATE_CGR_EN         0x10000000 /* CGR enabled */
#define QMAN_FQ_STATE_VDQCR          0x08000000 /* being volatile dequeued */

/* Flags to qman_init_fq() */
#define QMAN_INITFQ_FLAG_SCHED       0x00000001 /* schedule rather than park */
#define QMAN_INITFQ_FLAG_LOCAL       0x00000004 /* set dest portal */

/* Flags to qman_enqueue(). NB, the strange numbering is to align with hardware,
 * bit-wise. (NB: the PME API is sensitive to these precise numberings too, so
 * any change here should be audited in PME.)
 */
#define QMAN_ENQUEUE_FLAG_WATCH_CGR  0x00080000 /* watch congestion state */
#define QMAN_ENQUEUE_FLAG_DCA        0x00008000 /* perform enqueue-DCA */
#define QMAN_ENQUEUE_FLAG_DCA_PARK   0x00004000 /* If DCA, requests park */
#define QMAN_ENQUEUE_FLAG_DCA_PTR(p)		/* If DCA, p is DQRR entry */ \
		(((u32)(p) << 2) & 0x00000f00)
#define QMAN_ENQUEUE_FLAG_C_GREEN    0x00000000 /* choose one C_*** flag */
#define QMAN_ENQUEUE_FLAG_C_YELLOW   0x00000008
#define QMAN_ENQUEUE_FLAG_C_RED      0x00000010
#define QMAN_ENQUEUE_FLAG_C_OVERRIDE 0x00000018
/* For the ORP-specific qman_enqueue_orp() variant;
 * - this flag indicates "Not Last In Sequence", ie. all but the final fragment
 *   of a frame.
 */
#define QMAN_ENQUEUE_FLAG_NLIS       0x01000000
/* - this flag performs no enqueue but fills in an ORP sequence number that
 *   would otherwise block it (eg. if a frame has been dropped).
 */
#define QMAN_ENQUEUE_FLAG_HOLE       0x02000000
/* - this flag performs no enqueue but advances NESN to the given sequence
 *   number.
 */
#define QMAN_ENQUEUE_FLAG_NESN       0x04000000

/* Flags to qman_modify_cgr() */
#define QMAN_CGR_FLAG_USE_INIT       0x00000001
#define QMAN_CGR_MODE_FRAME          0x00000001

/**
 * qman_get_portal_index - get portal configuration index
 */
int qman_get_portal_index(void);

u32 qman_portal_dequeue(struct rte_event ev[], unsigned int poll_limit,
			void **bufs);

/**
 * qman_irqsource_add - add processing sources to be interrupt-driven
 * @bits: bitmask of QM_PIRQ_**I processing sources
 *
 * Adds processing sources that should be interrupt-driven (rather than
 * processed via qman_poll_***() functions). Returns zero for success, or
 * -EINVAL if the current CPU is sharing a portal hosted on another CPU.
 */
int qman_irqsource_add(u32 bits);

/**
 * qman_irqsource_remove - remove processing sources from being interrupt-driven
 * @bits: bitmask of QM_PIRQ_**I processing sources
 *
 * Removes processing sources from being interrupt-driven, so that they will
 * instead be processed via qman_poll_***() functions. Returns zero for success,
 * or -EINVAL if the current CPU is sharing a portal hosted on another CPU.
 */
int qman_irqsource_remove(u32 bits);

/**
 * qman_affine_channel - return the channel ID of an portal
 * @cpu: the cpu whose affine portal is the subject of the query
 *
 * If @cpu is -1, the affine portal for the current CPU will be used. It is a
 * bug to call this function for any value of @cpu (other than -1) that is not a
 * member of the cpu mask.
 */
u16 qman_affine_channel(int cpu);

unsigned int qman_portal_poll_rx(unsigned int poll_limit,
				 void **bufs, struct qman_portal *q);

/**
 * qman_set_vdq - Issue a volatile dequeue command
 * @fq: Frame Queue on which the volatile dequeue command is issued
 * @num: Number of Frames requested for volatile dequeue
 * @vdqcr_flags: QM_VDQCR_EXACT flag to for VDQCR command
 *
 * This function will issue a volatile dequeue command to the QMAN.
 */
int qman_set_vdq(struct qman_fq *fq, u16 num, uint32_t vdqcr_flags);

/**
 * qman_dequeue - Get the DQRR entry after volatile dequeue command
 * @fq: Frame Queue on which the volatile dequeue command is issued
 *
 * This function will return the DQRR entry after a volatile dequeue command
 * is issued. It will keep returning NULL until there is no packet available on
 * the DQRR.
 */
struct qm_dqrr_entry *qman_dequeue(struct qman_fq *fq);

/**
 * qman_dqrr_consume - Consume the DQRR entriy after volatile dequeue
 * @fq: Frame Queue on which the volatile dequeue command is issued
 * @dq: DQRR entry to consume. This is the one which is provided by the
 *    'qbman_dequeue' command.
 *
 * This will consume the DQRR enrey and make it available for next volatile
 * dequeue.
 */
void qman_dqrr_consume(struct qman_fq *fq,
		       struct qm_dqrr_entry *dq);

/**
 * qman_poll_dqrr - process DQRR (fast-path) entries
 * @limit: the maximum number of DQRR entries to process
 *
 * Use of this function requires that DQRR processing not be interrupt-driven.
 * Ie. the value returned by qman_irqsource_get() should not include
 * QM_PIRQ_DQRI. If the current CPU is sharing a portal hosted on another CPU,
 * this function will return -EINVAL, otherwise the return value is >=0 and
 * represents the number of DQRR entries processed.
 */
int qman_poll_dqrr(unsigned int limit);

/**
 * qman_poll
 *
 * Dispatcher logic on a cpu can use this to trigger any maintenance of the
 * affine portal. There are two classes of portal processing in question;
 * fast-path (which involves demuxing dequeue ring (DQRR) entries and tracking
 * enqueue ring (EQCR) consumption), and slow-path (which involves EQCR
 * thresholds, congestion state changes, etc). This function does whatever
 * processing is not triggered by interrupts.
 *
 * Note, if DQRR and some slow-path processing are poll-driven (rather than
 * interrupt-driven) then this function uses a heuristic to determine how often
 * to run slow-path processing - as slow-path processing introduces at least a
 * minimum latency each time it is run, whereas fast-path (DQRR) processing is
 * close to zero-cost if there is no work to be done.
 */
void qman_poll(void);

/**
 * qman_stop_dequeues - Stop h/w dequeuing to the s/w portal
 *
 * Disables DQRR processing of the portal. This is reference-counted, so
 * qman_start_dequeues() must be called as many times as qman_stop_dequeues() to
 * truly re-enable dequeuing.
 */
void qman_stop_dequeues(void);

/**
 * qman_start_dequeues - (Re)start h/w dequeuing to the s/w portal
 *
 * Enables DQRR processing of the portal. This is reference-counted, so
 * qman_start_dequeues() must be called as many times as qman_stop_dequeues() to
 * truly re-enable dequeuing.
 */
void qman_start_dequeues(void);

/**
 * qman_static_dequeue_add - Add pool channels to the portal SDQCR
 * @pools: bit-mask of pool channels, using QM_SDQCR_CHANNELS_POOL(n)
 *
 * Adds a set of pool channels to the portal's static dequeue command register
 * (SDQCR). The requested pools are limited to those the portal has dequeue
 * access to.
 */
void qman_static_dequeue_add(u32 pools, struct qman_portal *qm);

/**
 * qman_static_dequeue_del - Remove pool channels from the portal SDQCR
 * @pools: bit-mask of pool channels, using QM_SDQCR_CHANNELS_POOL(n)
 *
 * Removes a set of pool channels from the portal's static dequeue command
 * register (SDQCR). The requested pools are limited to those the portal has
 * dequeue access to.
 */
void qman_static_dequeue_del(u32 pools, struct qman_portal *qp);

/**
 * qman_static_dequeue_get - return the portal's current SDQCR
 *
 * Returns the portal's current static dequeue command register (SDQCR). The
 * entire register is returned, so if only the currently-enabled pool channels
 * are desired, mask the return value with QM_SDQCR_CHANNELS_POOL_MASK.
 */
u32 qman_static_dequeue_get(struct qman_portal *qp);

/**
 * qman_dca - Perform a Discrete Consumption Acknowledgment
 * @dq: the DQRR entry to be consumed
 * @park_request: indicates whether the held-active @fq should be parked
 *
 * Only allowed in DCA-mode portals, for DQRR entries whose handler callback had
 * previously returned 'qman_cb_dqrr_defer'. NB, as with the other APIs, this
 * does not take a 'portal' argument but implies the core affine portal from the
 * cpu that is currently executing the function. For reasons of locking, this
 * function must be called from the same CPU as that which processed the DQRR
 * entry in the first place.
 */
void qman_dca(const struct qm_dqrr_entry *dq, int park_request);

/**
 * qman_dca_index - Perform a Discrete Consumption Acknowledgment
 * @index: the DQRR index to be consumed
 * @park_request: indicates whether the held-active @fq should be parked
 *
 * Only allowed in DCA-mode portals, for DQRR entries whose handler callback had
 * previously returned 'qman_cb_dqrr_defer'. NB, as with the other APIs, this
 * does not take a 'portal' argument but implies the core affine portal from the
 * cpu that is currently executing the function. For reasons of locking, this
 * function must be called from the same CPU as that which processed the DQRR
 * entry in the first place.
 */
void qman_dca_index(u8 index, int park_request);

/**
 * qman_eqcr_is_empty - Determine if portal's EQCR is empty
 *
 * For use in situations where a cpu-affine caller needs to determine when all
 * enqueues for the local portal have been processed by Qman but can't use the
 * QMAN_ENQUEUE_FLAG_WAIT_SYNC flag to do this from the final qman_enqueue().
 * The function forces tracking of EQCR consumption (which normally doesn't
 * happen until enqueue processing needs to find space to put new enqueue
 * commands), and returns zero if the ring still has unprocessed entries,
 * non-zero if it is empty.
 */
int qman_eqcr_is_empty(void);

/**
 * qman_set_dc_ern - Set the handler for DCP enqueue rejection notifications
 * @handler: callback for processing DCP ERNs
 * @affine: whether this handler is specific to the locally affine portal
 *
 * If a hardware block's interface to Qman (ie. its direct-connect portal, or
 * DCP) is configured not to receive enqueue rejections, then any enqueues
 * through that DCP that are rejected will be sent to a given software portal.
 * If @affine is non-zero, then this handler will only be used for DCP ERNs
 * received on the portal affine to the current CPU. If multiple CPUs share a
 * portal and they all call this function, they will be setting the handler for
 * the same portal! If @affine is zero, then this handler will be global to all
 * portals handled by this instance of the driver. Only those portals that do
 * not have their own affine handler will use the global handler.
 */
void qman_set_dc_ern(qman_cb_dc_ern handler, int affine);

	/* FQ management */
	/* ------------- */
/**
 * qman_create_fq - Allocates a FQ
 * @fqid: the index of the FQD to encapsulate, must be "Out of Service"
 * @flags: bit-mask of QMAN_FQ_FLAG_*** options
 * @fq: memory for storing the 'fq', with callbacks filled in
 *
 * Creates a frame queue object for the given @fqid, unless the
 * QMAN_FQ_FLAG_DYNAMIC_FQID flag is set in @flags, in which case a FQID is
 * dynamically allocated (or the function fails if none are available). Once
 * created, the caller should not touch the memory at 'fq' except as extended to
 * adjacent memory for user-defined fields (see the definition of "struct
 * qman_fq" for more info). NO_MODIFY is only intended for enqueuing to
 * pre-existing frame-queues that aren't to be otherwise interfered with, it
 * prevents all other modifications to the frame queue. The TO_DCPORTAL flag
 * causes the driver to honour any contextB modifications requested in the
 * qm_init_fq() API, as this indicates the frame queue will be consumed by a
 * direct-connect portal (PME, CAAM, or Fman). When frame queues are consumed by
 * software portals, the contextB field is controlled by the driver and can't be
 * modified by the caller. If the AS_IS flag is specified, management commands
 * will be used on portal @p to query state for frame queue @fqid and construct
 * a frame queue object based on that, rather than assuming/requiring that it be
 * Out of Service.
 */
int qman_create_fq(u32 fqid, u32 flags, struct qman_fq *fq);

/**
 * qman_destroy_fq - Deallocates a FQ
 * @fq: the frame queue object to release
 * @flags: bit-mask of QMAN_FQ_FREE_*** options
 *
 * The memory for this frame queue object ('fq' provided in qman_create_fq()) is
 * not deallocated but the caller regains ownership, to do with as desired. The
 * FQ must be in the 'out-of-service' state unless the QMAN_FQ_FREE_PARKED flag
 * is specified, in which case it may also be in the 'parked' state.
 */
void qman_destroy_fq(struct qman_fq *fq, u32 flags);

/**
 * qman_fq_fqid - Queries the frame queue ID of a FQ object
 * @fq: the frame queue object to query
 */
u32 qman_fq_fqid(struct qman_fq *fq);

/**
 * qman_fq_state - Queries the state of a FQ object
 * @fq: the frame queue object to query
 * @state: pointer to state enum to return the FQ scheduling state
 * @flags: pointer to state flags to receive QMAN_FQ_STATE_*** bitmask
 *
 * Queries the state of the FQ object, without performing any h/w commands.
 * This captures the state, as seen by the driver, at the time the function
 * executes.
 */
void qman_fq_state(struct qman_fq *fq, enum qman_fq_state *state, u32 *flags);

/**
 * qman_init_fq - Initialises FQ fields, leaves the FQ "parked" or "scheduled"
 * @fq: the frame queue object to modify, must be 'parked' or new.
 * @flags: bit-mask of QMAN_INITFQ_FLAG_*** options
 * @opts: the FQ-modification settings, as defined in the low-level API
 *
 * The @opts parameter comes from the low-level portal API. Select
 * QMAN_INITFQ_FLAG_SCHED in @flags to cause the frame queue to be scheduled
 * rather than parked. NB, @opts can be NULL.
 *
 * Note that some fields and options within @opts may be ignored or overwritten
 * by the driver;
 * 1. the 'count' and 'fqid' fields are always ignored (this operation only
 * affects one frame queue: @fq).
 * 2. the QM_INITFQ_WE_CONTEXTB option of the 'we_mask' field and the associated
 * 'fqd' structure's 'context_b' field are sometimes overwritten;
 *   - if @fq was not created with QMAN_FQ_FLAG_TO_DCPORTAL, then context_b is
 *     initialised to a value used by the driver for demux.
 *   - if context_b is initialised for demux, so is context_a in case stashing
 *     is requested (see item 4).
 * (So caller control of context_b is only possible for TO_DCPORTAL frame queue
 * objects.)
 * 3. if @flags contains QMAN_INITFQ_FLAG_LOCAL, the 'fqd' structure's
 * 'dest::channel' field will be overwritten to match the portal used to issue
 * the command. If the WE_DESTWQ write-enable bit had already been set by the
 * caller, the channel workqueue will be left as-is, otherwise the write-enable
 * bit is set and the workqueue is set to a default of 4. If the "LOCAL" flag
 * isn't set, the destination channel/workqueue fields and the write-enable bit
 * are left as-is.
 * 4. if the driver overwrites context_a/b for demux, then if
 * QM_INITFQ_WE_CONTEXTA is set, the driver will only overwrite
 * context_a.address fields and will leave the stashing fields provided by the
 * user alone, otherwise it will zero out the context_a.stashing fields.
 */
int qman_init_fq(struct qman_fq *fq, u32 flags, struct qm_mcc_initfq *opts);

/**
 * qman_schedule_fq - Schedules a FQ
 * @fq: the frame queue object to schedule, must be 'parked'
 *
 * Schedules the frame queue, which must be Parked, which takes it to
 * Tentatively-Scheduled or Truly-Scheduled depending on its fill-level.
 */
int qman_schedule_fq(struct qman_fq *fq);

/**
 * qman_retire_fq - Retires a FQ
 * @fq: the frame queue object to retire
 * @flags: FQ flags (as per qman_fq_state) if retirement completes immediately
 *
 * Retires the frame queue. This returns zero if it succeeds immediately, +1 if
 * the retirement was started asynchronously, otherwise it returns negative for
 * failure. When this function returns zero, @flags is set to indicate whether
 * the retired FQ is empty and/or whether it has any ORL fragments (to show up
 * as ERNs). Otherwise the corresponding flags will be known when a subsequent
 * FQRN message shows up on the portal's message ring.
 *
 * NB, if the retirement is asynchronous (the FQ was in the Truly Scheduled or
 * Active state), the completion will be via the message ring as a FQRN - but
 * the corresponding callback may occur before this function returns!! Ie. the
 * caller should be prepared to accept the callback as the function is called,
 * not only once it has returned.
 */
int qman_retire_fq(struct qman_fq *fq, u32 *flags);

/**
 * qman_oos_fq - Puts a FQ "out of service"
 * @fq: the frame queue object to be put out-of-service, must be 'retired'
 *
 * The frame queue must be retired and empty, and if any order restoration list
 * was released as ERNs at the time of retirement, they must all be consumed.
 */
int qman_oos_fq(struct qman_fq *fq);

/**
 * qman_fq_flow_control - Set the XON/XOFF state of a FQ
 * @fq: the frame queue object to be set to XON/XOFF state, must not be 'oos',
 * or 'retired' or 'parked' state
 * @xon: boolean to set fq in XON or XOFF state
 *
 * The frame should be in Tentatively Scheduled state or Truly Schedule sate,
 * otherwise the IFSI interrupt will be asserted.
 */
int qman_fq_flow_control(struct qman_fq *fq, int xon);

/**
 * qman_query_fq - Queries FQD fields (via h/w query command)
 * @fq: the frame queue object to be queried
 * @fqd: storage for the queried FQD fields
 */
int qman_query_fq(struct qman_fq *fq, struct qm_fqd *fqd);

/**
 * qman_query_fq_has_pkts - Queries non-programmable FQD fields and returns '1'
 * if packets are in the frame queue. If there are no packets on frame
 * queue '0' is returned.
 * @fq: the frame queue object to be queried
 */
int qman_query_fq_has_pkts(struct qman_fq *fq);

/**
 * qman_query_fq_np - Queries non-programmable FQD fields
 * @fq: the frame queue object to be queried
 * @np: storage for the queried FQD fields
 */
int qman_query_fq_np(struct qman_fq *fq, struct qm_mcr_queryfq_np *np);

/**
 * qman_query_fq_frmcnt - Queries fq frame count
 * @fq: the frame queue object to be queried
 * @frm_cnt: number of frames in the queue
 */
int qman_query_fq_frm_cnt(struct qman_fq *fq, u32 *frm_cnt);

/**
 * qman_query_wq - Queries work queue lengths
 * @query_dedicated: If non-zero, query length of WQs in the channel dedicated
 *		to this software portal. Otherwise, query length of WQs in a
 *		channel  specified in wq.
 * @wq: storage for the queried WQs lengths. Also specified the channel to
 *	to query if query_dedicated is zero.
 */
int qman_query_wq(u8 query_dedicated, struct qm_mcr_querywq *wq);

/**
 * qman_volatile_dequeue - Issue a volatile dequeue command
 * @fq: the frame queue object to dequeue from
 * @flags: a bit-mask of QMAN_VOLATILE_FLAG_*** options
 * @vdqcr: bit mask of QM_VDQCR_*** options, as per qm_dqrr_vdqcr_set()
 *
 * Attempts to lock access to the portal's VDQCR volatile dequeue functionality.
 * The function will block and sleep if QMAN_VOLATILE_FLAG_WAIT is specified and
 * the VDQCR is already in use, otherwise returns non-zero for failure. If
 * QMAN_VOLATILE_FLAG_FINISH is specified, the function will only return once
 * the VDQCR command has finished executing (ie. once the callback for the last
 * DQRR entry resulting from the VDQCR command has been called). If not using
 * the FINISH flag, completion can be determined either by detecting the
 * presence of the QM_DQRR_STAT_UNSCHEDULED and QM_DQRR_STAT_DQCR_EXPIRED bits
 * in the "stat" field of the "struct qm_dqrr_entry" passed to the FQ's dequeue
 * callback, or by waiting for the QMAN_FQ_STATE_VDQCR bit to disappear from the
 * "flags" retrieved from qman_fq_state().
 */
int qman_volatile_dequeue(struct qman_fq *fq, u32 flags, u32 vdqcr);

/**
 * qman_enqueue - Enqueue a frame to a frame queue
 * @fq: the frame queue object to enqueue to
 * @fd: a descriptor of the frame to be enqueued
 * @flags: bit-mask of QMAN_ENQUEUE_FLAG_*** options
 *
 * Fills an entry in the EQCR of portal @qm to enqueue the frame described by
 * @fd. The descriptor details are copied from @fd to the EQCR entry, the 'pid'
 * field is ignored. The return value is non-zero on error, such as ring full
 * (and FLAG_WAIT not specified), congestion avoidance (FLAG_WATCH_CGR
 * specified), etc. If the ring is full and FLAG_WAIT is specified, this
 * function will block. If FLAG_INTERRUPT is set, the EQCI bit of the portal
 * interrupt will assert when Qman consumes the EQCR entry (subject to "status
 * disable", "enable", and "inhibit" registers). If FLAG_DCA is set, Qman will
 * perform an implied "discrete consumption acknowledgment" on the dequeue
 * ring's (DQRR) entry, at the ring index specified by the FLAG_DCA_IDX(x)
 * macro. (As an alternative to issuing explicit DCA actions on DQRR entries,
 * this implicit DCA can delay the release of a "held active" frame queue
 * corresponding to a DQRR entry until Qman consumes the EQCR entry - providing
 * order-preservation semantics in packet-forwarding scenarios.) If FLAG_DCA is
 * set, then FLAG_DCA_PARK can also be set to imply that the DQRR consumption
 * acknowledgment should "park request" the "held active" frame queue. Ie.
 * when the portal eventually releases that frame queue, it will be left in the
 * Parked state rather than Tentatively Scheduled or Truly Scheduled. If the
 * portal is watching congestion groups, the QMAN_ENQUEUE_FLAG_WATCH_CGR flag
 * is requested, and the FQ is a member of a congestion group, then this
 * function returns -EAGAIN if the congestion group is currently congested.
 * Note, this does not eliminate ERNs, as the async interface means we can be
 * sending enqueue commands to an un-congested FQ that becomes congested before
 * the enqueue commands are processed, but it does minimise needless thrashing
 * of an already busy hardware resource by throttling many of the to-be-dropped
 * enqueues "at the source".
 */
int qman_enqueue(struct qman_fq *fq, const struct qm_fd *fd, u32 flags);

int qman_enqueue_multi(struct qman_fq *fq, const struct qm_fd *fd, u32 *flags,
		       int frames_to_send);

/**
 * qman_enqueue_multi_fq - Enqueue multiple frames to their respective frame
 * queues.
 * @fq[]: Array of frame queue objects to enqueue to
 * @fd: pointer to first descriptor of frame to be enqueued
 * @frames_to_send: number of frames to be sent.
 *
 * This API is similar to qman_enqueue_multi(), but it takes fd which needs
 * to be processed by different frame queues.
 */
int
qman_enqueue_multi_fq(struct qman_fq *fq[], const struct qm_fd *fd,
		      int frames_to_send);

typedef int (*qman_cb_precommit) (void *arg);

/**
 * qman_enqueue_orp - Enqueue a frame to a frame queue using an ORP
 * @fq: the frame queue object to enqueue to
 * @fd: a descriptor of the frame to be enqueued
 * @flags: bit-mask of QMAN_ENQUEUE_FLAG_*** options
 * @orp: the frame queue object used as an order restoration point.
 * @orp_seqnum: the sequence number of this frame in the order restoration path
 *
 * Similar to qman_enqueue(), but with the addition of an Order Restoration
 * Point (@orp) and corresponding sequence number (@orp_seqnum) for this
 * enqueue operation to employ order restoration. Each frame queue object acts
 * as an Order Definition Point (ODP) by providing each frame dequeued from it
 * with an incrementing sequence number, this value is generally ignored unless
 * that sequence of dequeued frames will need order restoration later. Each
 * frame queue object also encapsulates an Order Restoration Point (ORP), which
 * is a re-assembly context for re-ordering frames relative to their sequence
 * numbers as they are enqueued. The ORP does not have to be within the frame
 * queue that receives the enqueued frame, in fact it is usually the frame
 * queue from which the frames were originally dequeued. For the purposes of
 * order restoration, multiple frames (or "fragments") can be enqueued for a
 * single sequence number by setting the QMAN_ENQUEUE_FLAG_NLIS flag for all
 * enqueues except the final fragment of a given sequence number. Ordering
 * between sequence numbers is guaranteed, even if fragments of different
 * sequence numbers are interlaced with one another. Fragments of the same
 * sequence number will retain the order in which they are enqueued. If no
 * enqueue is to performed, QMAN_ENQUEUE_FLAG_HOLE indicates that the given
 * sequence number is to be "skipped" by the ORP logic (eg. if a frame has been
 * dropped from a sequence), or QMAN_ENQUEUE_FLAG_NESN indicates that the given
 * sequence number should become the ORP's "Next Expected Sequence Number".
 *
 * Side note: a frame queue object can be used purely as an ORP, without
 * carrying any frames at all. Care should be taken not to deallocate a frame
 * queue object that is being actively used as an ORP, as a future allocation
 * of the frame queue object may start using the internal ORP before the
 * previous use has finished.
 */
int qman_enqueue_orp(struct qman_fq *fq, const struct qm_fd *fd, u32 flags,
		     struct qman_fq *orp, u16 orp_seqnum);

/**
 * qman_alloc_fqid_range - Allocate a contiguous range of FQIDs
 * @result: is set by the API to the base FQID of the allocated range
 * @count: the number of FQIDs required
 * @align: required alignment of the allocated range
 * @partial: non-zero if the API can return fewer than @count FQIDs
 *
 * Returns the number of frame queues allocated, or a negative error code. If
 * @partial is non zero, the allocation request may return a smaller range of
 * FQs than requested (though alignment will be as requested). If @partial is
 * zero, the return value will either be 'count' or negative.
 */
int qman_alloc_fqid_range(u32 *result, u32 count, u32 align, int partial);
static inline int qman_alloc_fqid(u32 *result)
{
	int ret = qman_alloc_fqid_range(result, 1, 0, 0);

	return (ret > 0) ? 0 : ret;
}

/**
 * qman_release_fqid_range - Release the specified range of frame queue IDs
 * @fqid: the base FQID of the range to deallocate
 * @count: the number of FQIDs in the range
 *
 * This function can also be used to seed the allocator with ranges of FQIDs
 * that it can subsequently allocate from.
 */
void qman_release_fqid_range(u32 fqid, unsigned int count);
static inline void qman_release_fqid(u32 fqid)
{
	qman_release_fqid_range(fqid, 1);
}

void qman_seed_fqid_range(u32 fqid, unsigned int count);

int qman_shutdown_fq(u32 fqid);

/**
 * qman_reserve_fqid_range - Reserve the specified range of frame queue IDs
 * @fqid: the base FQID of the range to deallocate
 * @count: the number of FQIDs in the range
 */
int qman_reserve_fqid_range(u32 fqid, unsigned int count);
static inline int qman_reserve_fqid(u32 fqid)
{
	return qman_reserve_fqid_range(fqid, 1);
}

/* Pool-channel management */
/**
 * qman_alloc_pool_range - Allocate a contiguous range of pool-channel IDs
 * @result: is set by the API to the base pool-channel ID of the allocated range
 * @count: the number of pool-channel IDs required
 * @align: required alignment of the allocated range
 * @partial: non-zero if the API can return fewer than @count
 *
 * Returns the number of pool-channel IDs allocated, or a negative error code.
 * If @partial is non zero, the allocation request may return a smaller range of
 * than requested (though alignment will be as requested). If @partial is zero,
 * the return value will either be 'count' or negative.
 */
int qman_alloc_pool_range(u32 *result, u32 count, u32 align, int partial);
static inline int qman_alloc_pool(u32 *result)
{
	int ret = qman_alloc_pool_range(result, 1, 0, 0);

	return (ret > 0) ? 0 : ret;
}

/**
 * qman_release_pool_range - Release the specified range of pool-channel IDs
 * @id: the base pool-channel ID of the range to deallocate
 * @count: the number of pool-channel IDs in the range
 */
void qman_release_pool_range(u32 id, unsigned int count);
static inline void qman_release_pool(u32 id)
{
	qman_release_pool_range(id, 1);
}

/**
 * qman_reserve_pool_range - Reserve the specified range of pool-channel IDs
 * @id: the base pool-channel ID of the range to reserve
 * @count: the number of pool-channel IDs in the range
 */
int qman_reserve_pool_range(u32 id, unsigned int count);
static inline int qman_reserve_pool(u32 id)
{
	return qman_reserve_pool_range(id, 1);
}

void qman_seed_pool_range(u32 id, unsigned int count);

	/* CGR management */
	/* -------------- */
/**
 * qman_create_cgr - Register a congestion group object
 * @cgr: the 'cgr' object, with fields filled in
 * @flags: QMAN_CGR_FLAG_* values
 * @opts: optional state of CGR settings
 *
 * Registers this object to receiving congestion entry/exit callbacks on the
 * portal affine to the cpu portal on which this API is executed. If opts is
 * NULL then only the callback (cgr->cb) function is registered. If @flags
 * contains QMAN_CGR_FLAG_USE_INIT, then an init hw command (which will reset
 * any unspecified parameters) will be used rather than a modify hw hardware
 * (which only modifies the specified parameters).
 */
int qman_create_cgr(struct qman_cgr *cgr, u32 flags,
		    struct qm_mcc_initcgr *opts);

/**
 * qman_create_cgr_to_dcp - Register a congestion group object to DCP portal
 * @cgr: the 'cgr' object, with fields filled in
 * @flags: QMAN_CGR_FLAG_* values
 * @dcp_portal: the DCP portal to which the cgr object is registered.
 * @opts: optional state of CGR settings
 *
 */
int qman_create_cgr_to_dcp(struct qman_cgr *cgr, u32 flags, u16 dcp_portal,
			   struct qm_mcc_initcgr *opts);

/**
 * qman_delete_cgr - Deregisters a congestion group object
 * @cgr: the 'cgr' object to deregister
 *
 * "Unplugs" this CGR object from the portal affine to the cpu on which this API
 * is executed. This must be excuted on the same affine portal on which it was
 * created.
 */
int qman_delete_cgr(struct qman_cgr *cgr);

/**
 * qman_modify_cgr - Modify CGR fields
 * @cgr: the 'cgr' object to modify
 * @flags: QMAN_CGR_FLAG_* values
 * @opts: the CGR-modification settings
 *
 * The @opts parameter comes from the low-level portal API, and can be NULL.
 * Note that some fields and options within @opts may be ignored or overwritten
 * by the driver, in particular the 'cgrid' field is ignored (this operation
 * only affects the given CGR object). If @flags contains
 * QMAN_CGR_FLAG_USE_INIT, then an init hw command (which will reset any
 * unspecified parameters) will be used rather than a modify hw hardware (which
 * only modifies the specified parameters).
 */
int qman_modify_cgr(struct qman_cgr *cgr, u32 flags,
		    struct qm_mcc_initcgr *opts);

/**
 * qman_query_cgr - Queries CGR fields
 * @cgr: the 'cgr' object to query
 * @result: storage for the queried congestion group record
 */
int qman_query_cgr(struct qman_cgr *cgr, struct qm_mcr_querycgr *result);

/**
 * qman_query_congestion - Queries the state of all congestion groups
 * @congestion: storage for the queried state of all congestion groups
 */
int qman_query_congestion(struct qm_mcr_querycongestion *congestion);

/**
 * qman_alloc_cgrid_range - Allocate a contiguous range of CGR IDs
 * @result: is set by the API to the base CGR ID of the allocated range
 * @count: the number of CGR IDs required
 * @align: required alignment of the allocated range
 * @partial: non-zero if the API can return fewer than @count
 *
 * Returns the number of CGR IDs allocated, or a negative error code.
 * If @partial is non zero, the allocation request may return a smaller range of
 * than requested (though alignment will be as requested). If @partial is zero,
 * the return value will either be 'count' or negative.
 */
int qman_alloc_cgrid_range(u32 *result, u32 count, u32 align, int partial);
static inline int qman_alloc_cgrid(u32 *result)
{
	int ret = qman_alloc_cgrid_range(result, 1, 0, 0);

	return (ret > 0) ? 0 : ret;
}

/**
 * qman_release_cgrid_range - Release the specified range of CGR IDs
 * @id: the base CGR ID of the range to deallocate
 * @count: the number of CGR IDs in the range
 */
void qman_release_cgrid_range(u32 id, unsigned int count);
static inline void qman_release_cgrid(u32 id)
{
	qman_release_cgrid_range(id, 1);
}

/**
 * qman_reserve_cgrid_range - Reserve the specified range of CGR ID
 * @id: the base CGR ID of the range to reserve
 * @count: the number of CGR IDs in the range
 */
int qman_reserve_cgrid_range(u32 id, unsigned int count);
static inline int qman_reserve_cgrid(u32 id)
{
	return qman_reserve_cgrid_range(id, 1);
}

void qman_seed_cgrid_range(u32 id, unsigned int count);

	/* Helpers */
	/* ------- */
/**
 * qman_poll_fq_for_init - Check if an FQ has been initialised from OOS
 * @fqid: the FQID that will be initialised by other s/w
 *
 * In many situations, a FQID is provided for communication between s/w
 * entities, and whilst the consumer is responsible for initialising and
 * scheduling the FQ, the producer(s) generally create a wrapper FQ object using
 * and only call qman_enqueue() (no FQ initialisation, scheduling, etc). Ie;
 *     qman_create_fq(..., QMAN_FQ_FLAG_NO_MODIFY, ...);
 * However, data can not be enqueued to the FQ until it is initialised out of
 * the OOS state - this function polls for that condition. It is particularly
 * useful for users of IPC functions - each endpoint's Rx FQ is the other
 * endpoint's Tx FQ, so each side can initialise and schedule their Rx FQ object
 * and then use this API on the (NO_MODIFY) Tx FQ object in order to
 * synchronise. The function returns zero for success, +1 if the FQ is still in
 * the OOS state, or negative if there was an error.
 */
static inline int qman_poll_fq_for_init(struct qman_fq *fq)
{
	struct qm_mcr_queryfq_np np;
	int err;

	err = qman_query_fq_np(fq, &np);
	if (err)
		return err;
	if ((np.state & QM_MCR_NP_STATE_MASK) == QM_MCR_NP_STATE_OOS)
		return 1;
	return 0;
}

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define cpu_to_hw_sg(x)
#define hw_sg_to_cpu(x)
#else
#define cpu_to_hw_sg(x)  __cpu_to_hw_sg(x)
#define hw_sg_to_cpu(x)  __hw_sg_to_cpu(x)

static inline void __cpu_to_hw_sg(struct qm_sg_entry *sgentry)
{
	sgentry->opaque = cpu_to_be64(sgentry->opaque);
	sgentry->val = cpu_to_be32(sgentry->val);
	sgentry->val_off = cpu_to_be16(sgentry->val_off);
}

static inline void __hw_sg_to_cpu(struct qm_sg_entry *sgentry)
{
	sgentry->opaque = be64_to_cpu(sgentry->opaque);
	sgentry->val = be32_to_cpu(sgentry->val);
	sgentry->val_off = be16_to_cpu(sgentry->val_off);
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* __FSL_QMAN_H */
