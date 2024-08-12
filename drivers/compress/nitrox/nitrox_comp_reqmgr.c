/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include <rte_compressdev_pmd.h>
#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_hexdump.h>

#include "nitrox_comp_reqmgr.h"
#include "nitrox_logs.h"
#include "rte_comp.h"

#define NITROX_INSTR_BUFFER_DEBUG 0
#define NITROX_ZIP_SGL_COUNT 16
#define NITROX_ZIP_MAX_ZPTRS 2048
#define NITROX_ZIP_MAX_DATASIZE ((1 << 24) - 1)
#define NITROX_ZIP_MAX_ONFSIZE 1024
#define CMD_TIMEOUT 2

union nitrox_zip_instr_word0 {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t raz0   :  8;
		uint64_t tol    : 24;
		uint64_t raz1   :  5;
		uint64_t exn    :  3;
		uint64_t raz2   :  1;
		uint64_t exbits :  7;
		uint64_t raz3   :  3;
		uint64_t ca     :  1;
		uint64_t sf     :  1;
		uint64_t ss     :  2;
		uint64_t cc     :  2;
		uint64_t ef     :  1;
		uint64_t bf     :  1;
		uint64_t co     :  1;
		uint64_t raz4   :  1;
		uint64_t ds     :  1;
		uint64_t dg     :  1;
		uint64_t hg     :  1;
#else
		uint64_t hg     :  1;
		uint64_t dg     :  1;
		uint64_t ds     :  1;
		uint64_t raz4   :  1;
		uint64_t co     :  1;
		uint64_t bf     :  1;
		uint64_t ef     :  1;
		uint64_t cc     :  2;
		uint64_t ss     :  2;
		uint64_t sf     :  1;
		uint64_t ca     :  1;
		uint64_t raz3   :  3;
		uint64_t exbits :  7;
		uint64_t raz2   :  1;
		uint64_t exn    :  3;
		uint64_t raz1   :  5;
		uint64_t tol    : 24;
		uint64_t raz0   :  8;
#endif

	};
};

union nitrox_zip_instr_word1 {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t hl             : 16;
		uint64_t raz0		: 16;
		uint64_t adlercrc32	: 32;
#else
		uint64_t adlercrc32     : 32;
		uint64_t raz0           : 16;
		uint64_t hl             : 16;
#endif
	};
};

union nitrox_zip_instr_word2 {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t raz0   : 20;
		uint64_t cptr   : 44;
#else
		uint64_t cptr   : 44;
		uint64_t raz0   : 20;
#endif
	};
};

union nitrox_zip_instr_word3 {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t raz0   :  4;
		uint64_t hlen   : 16;
		uint64_t hptr   : 44;
#else
		uint64_t hptr   : 44;
		uint64_t hlen   : 16;
		uint64_t raz0   :  4;
#endif
	};
};

union nitrox_zip_instr_word4 {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t raz0   :  4;
		uint64_t ilen   : 16;
		uint64_t iptr   : 44;
#else
		uint64_t iptr   : 44;
		uint64_t ilen   : 16;
		uint64_t raz0   :  4;
#endif
	};
};

union nitrox_zip_instr_word5 {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t raz0 :  4;
		uint64_t olen : 16;
		uint64_t optr : 44;
#else
		uint64_t optr : 44;
		uint64_t olen : 16;
		uint64_t raz0 :  4;
#endif
	};
};

union nitrox_zip_instr_word6 {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t raz0   : 20;
		uint64_t rptr   : 44;
#else
		uint64_t rptr   : 44;
		uint64_t raz0   : 20;
#endif
	};
};

union nitrox_zip_instr_word7 {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t grp    :  3;
		uint64_t raz0   : 41;
		uint64_t addr_msb: 20;
#else
		uint64_t addr_msb: 20;
		uint64_t raz0   : 41;
		uint64_t grp    :  3;
#endif
	};
};

struct nitrox_zip_instr {
	union nitrox_zip_instr_word0 w0;
	union nitrox_zip_instr_word1 w1;
	union nitrox_zip_instr_word2 w2;
	union nitrox_zip_instr_word3 w3;
	union nitrox_zip_instr_word4 w4;
	union nitrox_zip_instr_word5 w5;
	union nitrox_zip_instr_word6 w6;
	union nitrox_zip_instr_word7 w7;
};

union nitrox_zip_result_word0 {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t crc32  : 32;
		uint64_t adler32: 32;
#else
		uint64_t adler32: 32;
		uint64_t crc32  : 32;
#endif
	};
};

union nitrox_zip_result_word1 {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t tbyteswritten  : 32;
		uint64_t tbytesread     : 32;
#else
		uint64_t tbytesread     : 32;
		uint64_t tbyteswritten  : 32;
#endif
	};
};

union nitrox_zip_result_word2 {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t tbits  : 32;
		uint64_t raz0   :  5;
		uint64_t exn    :  3;
		uint64_t raz1   :  1;
		uint64_t exbits :  7;
		uint64_t raz2   :  7;
		uint64_t ef     :  1;
		uint64_t compcode:  8;
#else
		uint64_t compcode:  8;
		uint64_t ef     :  1;
		uint64_t raz2   :  7;
		uint64_t exbits :  7;
		uint64_t raz1   :  1;
		uint64_t exn    :  3;
		uint64_t raz0   :  5;
		uint64_t tbits  : 32;
#endif
	};
};

struct nitrox_zip_result {
	union nitrox_zip_result_word0 w0;
	union nitrox_zip_result_word1 w1;
	union nitrox_zip_result_word2 w2;
};

union nitrox_zip_zptr {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t raz0   :  3;
		uint64_t le     :  1;
		uint64_t length : 16;
		uint64_t addr   : 44;
#else
		uint64_t addr   : 44;
		uint64_t length : 16;
		uint64_t le     :  1;
		uint64_t raz0   :  3;
#endif
	} s;
};

struct nitrox_zip_iova_addr {
	union {
		uint64_t u64;
		struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
			uint64_t addr_msb: 20;
			uint64_t addr	: 44;
#else
			uint64_t addr	: 44;
			uint64_t addr_msb: 20;
#endif
		} zda;

		struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
			uint64_t addr_msb: 20;
			uint64_t addr	: 41;
			uint64_t align_8bytes: 3;
#else
			uint64_t align_8bytes: 3;
			uint64_t addr	: 41;
			uint64_t addr_msb: 20;
#endif
		} z8a;
	};
};

enum nitrox_zip_comp_code {
	NITROX_CC_NOTDONE     = 0,
	NITROX_CC_SUCCESS     = 1,
	NITROX_CC_DTRUNC      = 2,
	NITROX_CC_STOP        = 3,
	NITROX_CC_ITRUNK      = 4,
	NITROX_CC_RBLOCK      = 5,
	NITROX_CC_NLEN        = 6,
	NITROX_CC_BADCODE     = 7,
	NITROX_CC_BADCODE2    = 8,
	NITROX_CC_ZERO_LEN    = 9,
	NITROX_CC_PARITY      = 10,
	NITROX_CC_FATAL       = 11,
	NITROX_CC_TIMEOUT     = 12,
	NITROX_CC_NPCI_ERR    = 13,
};

struct nitrox_sgtable {
	union nitrox_zip_zptr *sgl;
	uint64_t addr_msb;
	uint32_t total_bytes;
	uint16_t nb_sgls;
	uint16_t filled_sgls;
};

struct nitrox_softreq {
	struct nitrox_zip_instr instr;
	alignas(8) struct nitrox_zip_result zip_res;
	uint8_t decomp_threshold[NITROX_ZIP_MAX_ONFSIZE];
	struct rte_comp_op *op;
	struct nitrox_sgtable src;
	struct nitrox_sgtable dst;
	uint64_t timeout;
};

#if NITROX_INSTR_BUFFER_DEBUG
static void nitrox_dump_databuf(const char *name, struct rte_mbuf *m,
				uint32_t off, uint32_t datalen)
{
	uint32_t mlen;

	if (!rte_log_can_log(nitrox_logtype, RTE_LOG_DEBUG))
		return;

	for (; m && off > rte_pktmbuf_data_len(m); m = m->next)
		off -= rte_pktmbuf_data_len(m);

	mlen = rte_pktmbuf_data_len(m) - off;
	if (datalen <= mlen)
		mlen = datalen;

	rte_hexdump(rte_log_get_stream(), name,
		    rte_pktmbuf_mtod_offset(m, char *, off), mlen);
	for (m = m->next; m && datalen; m = m->next) {
		mlen = rte_pktmbuf_data_len(m) < datalen ?
			rte_pktmbuf_data_len(m) : datalen;
		rte_hexdump(rte_log_get_stream(), name,
			    rte_pktmbuf_mtod(m, char *), mlen);
	}

	NITROX_LOG_LINE(DEBUG,);
}

static void nitrox_dump_zip_instr(struct nitrox_zip_instr *instr,
				  union nitrox_zip_zptr *hptr_arr,
				  union nitrox_zip_zptr *iptr_arr,
				  union nitrox_zip_zptr *optr_arr)
{
	uint64_t value;
	int i = 0;

	NITROX_LOG_LINE(DEBUG, "\nZIP instruction..(%p)", instr);
	NITROX_LOG_LINE(DEBUG, "\tWORD0 = 0x%016"PRIx64, instr->w0.u64);
	NITROX_LOG_LINE(DEBUG, "\t\tTOL        = %d", instr->w0.tol);
	NITROX_LOG_LINE(DEBUG, "\t\tEXNUM      = %d", instr->w0.exn);
	NITROX_LOG_LINE(DEBUG, "\t\tEXBITS     = %x", instr->w0.exbits);
	NITROX_LOG_LINE(DEBUG, "\t\tCA         = %d", instr->w0.ca);
	NITROX_LOG_LINE(DEBUG, "\t\tSF         = %d", instr->w0.sf);
	NITROX_LOG_LINE(DEBUG, "\t\tSS         = %d", instr->w0.ss);
	NITROX_LOG_LINE(DEBUG, "\t\tCC         = %d", instr->w0.cc);
	NITROX_LOG_LINE(DEBUG, "\t\tEF         = %d", instr->w0.ef);
	NITROX_LOG_LINE(DEBUG, "\t\tBF         = %d", instr->w0.bf);
	NITROX_LOG_LINE(DEBUG, "\t\tCO         = %d", instr->w0.co);
	NITROX_LOG_LINE(DEBUG, "\t\tDS         = %d", instr->w0.ds);
	NITROX_LOG_LINE(DEBUG, "\t\tDG         = %d", instr->w0.dg);
	NITROX_LOG_LINE(DEBUG, "\t\tHG         = %d", instr->w0.hg);
	NITROX_LOG_LINE(DEBUG,);

	NITROX_LOG_LINE(DEBUG, "\tWORD1 = 0x%016"PRIx64, instr->w1.u64);
	NITROX_LOG_LINE(DEBUG, "\t\tHL         = %d", instr->w1.hl);
	NITROX_LOG_LINE(DEBUG, "\t\tADLERCRC32 = 0x%08x", instr->w1.adlercrc32);
	NITROX_LOG_LINE(DEBUG,);

	value = instr->w2.cptr;
	NITROX_LOG_LINE(DEBUG, "\tWORD2 = 0x%016"PRIx64, instr->w2.u64);
	NITROX_LOG_LINE(DEBUG, "\t\tCPTR = 0x%11"PRIx64, value);
	NITROX_LOG_LINE(DEBUG,);

	value = instr->w3.hptr;
	NITROX_LOG_LINE(DEBUG, "\tWORD3 = 0x%016"PRIx64, instr->w3.u64);
	NITROX_LOG_LINE(DEBUG, "\t\tHLEN       = %d", instr->w3.hlen);
	NITROX_LOG_LINE(DEBUG, "\t\tHPTR = 0x%11"PRIx64, value);

	if (instr->w0.hg && hptr_arr) {
		for (i = 0; i < instr->w3.hlen; i++) {
			value = hptr_arr[i].s.addr;
			NITROX_LOG_LINE(DEBUG, "\t\t\tZPTR[%d] : Length = %d Addr = 0x%11"PRIx64,
				     i, hptr_arr[i].s.length, value);
		}
	}

	NITROX_LOG_LINE(DEBUG,);

	value = instr->w4.iptr;
	NITROX_LOG_LINE(DEBUG, "\tWORD4 = 0x%016"PRIx64, instr->w4.u64);
	NITROX_LOG_LINE(DEBUG, "\t\tILEN       = %d", instr->w4.ilen);
	NITROX_LOG_LINE(DEBUG, "\t\tIPTR       = 0x%11"PRIx64, value);
	if (instr->w0.dg && iptr_arr) {
		for (i = 0; i < instr->w4.ilen; i++) {
			value = iptr_arr[i].s.addr;
			NITROX_LOG_LINE(DEBUG, "\t\t\tZPTR[%d] : Length = %d Addr = 0x%11"PRIx64,
				     i, iptr_arr[i].s.length, value);
		}
	}

	NITROX_LOG_LINE(DEBUG,);

	value = instr->w5.optr;
	NITROX_LOG_LINE(DEBUG, "\tWORD5 = 0x%016"PRIx64, instr->w5.u64);
	NITROX_LOG_LINE(DEBUG, "\t\t OLEN = %d", instr->w5.olen);
	NITROX_LOG_LINE(DEBUG, "\t\t OPTR = 0x%11"PRIx64, value);
	if (instr->w0.ds && optr_arr) {
		for (i = 0; i < instr->w5.olen; i++) {
			value = optr_arr[i].s.addr;
			NITROX_LOG_LINE(DEBUG, "\t\t\tZPTR[%d] : Length = %d Addr = 0x%11"PRIx64,
				     i, optr_arr[i].s.length, value);
		}
	}

	NITROX_LOG_LINE(DEBUG,);

	value = instr->w6.rptr;
	NITROX_LOG_LINE(DEBUG, "\tWORD6 = 0x%016"PRIx64, instr->w6.u64);
	NITROX_LOG_LINE(DEBUG, "\t\tRPTR = 0x%11"PRIx64, value);
	NITROX_LOG_LINE(DEBUG,);

	NITROX_LOG_LINE(DEBUG, "\tWORD7 = 0x%016"PRIx64, instr->w7.u64);
	NITROX_LOG_LINE(DEBUG, "\t\tGRP        = %x", instr->w7.grp);
	NITROX_LOG_LINE(DEBUG, "\t\tADDR_MSB   = 0x%5x", instr->w7.addr_msb);
	NITROX_LOG_LINE(DEBUG,);
}

static void nitrox_dump_zip_result(struct nitrox_zip_instr *instr,
				   struct nitrox_zip_result *result)
{
	NITROX_LOG_LINE(DEBUG, "ZIP result..(instr %p)", instr);
	NITROX_LOG_LINE(DEBUG, "\tWORD0 = 0x%016"PRIx64, result->w0.u64);
	NITROX_LOG_LINE(DEBUG, "\t\tCRC32          = 0x%8x", result->w0.crc32);
	NITROX_LOG_LINE(DEBUG, "\t\tADLER32        = 0x%8x", result->w0.adler32);
	NITROX_LOG_LINE(DEBUG,);

	NITROX_LOG_LINE(DEBUG, "\tWORD1 = 0x%016"PRIx64, result->w1.u64);
	NITROX_LOG_LINE(DEBUG, "\t\tTBYTESWRITTEN  = %u", result->w1.tbyteswritten);
	NITROX_LOG_LINE(DEBUG, "\t\tTBYTESREAD     = %u", result->w1.tbytesread);
	NITROX_LOG_LINE(DEBUG,);

	NITROX_LOG_LINE(DEBUG, "\tWORD2 = 0x%016"PRIx64, result->w2.u64);
	NITROX_LOG_LINE(DEBUG, "\t\tTBITS          = %u", result->w2.tbits);
	NITROX_LOG_LINE(DEBUG, "\t\tEXN            = %d", result->w2.exn);
	NITROX_LOG_LINE(DEBUG, "\t\tEBITS          = %x", result->w2.exbits);
	NITROX_LOG_LINE(DEBUG, "\t\tEF             = %d", result->w2.ef);
	NITROX_LOG_LINE(DEBUG, "\t\tCOMPCODE       = 0x%2x", result->w2.compcode);
	NITROX_LOG_LINE(DEBUG,);
}
#else
#define nitrox_dump_databuf(name, m, off, datalen)
#define nitrox_dump_zip_instr(instr, hptr_arr, iptr_arr, optr_arr)
#define nitrox_dump_zip_result(instr, result)
#endif

static int handle_zero_length_compression(struct nitrox_softreq *sr,
					  struct nitrox_comp_xform *xform)
{
	union {
		uint32_t num;
		uint8_t bytes[4];
	} fblk;
	uint32_t dstlen, rlen;
	struct rte_mbuf *m;
	uint32_t off;
	uint32_t mlen;
	uint32_t i = 0;
	uint8_t *ptr;

	fblk.num = xform->exn ? (xform->exbits & 0x7F) : 0;
	fblk.num |= (0x3 << xform->exn);
	memset(&sr->zip_res, 0, sizeof(sr->zip_res));
	sr->zip_res.w1.tbytesread = xform->hlen;
	sr->zip_res.w1.tbyteswritten = 2;
	sr->zip_res.w2.ef = 1;
	if (xform->exn == 7)
		sr->zip_res.w1.tbyteswritten++;

	rlen = sr->zip_res.w1.tbyteswritten;
	dstlen = rte_pktmbuf_pkt_len(sr->op->m_dst) - sr->op->dst.offset;
	if (unlikely(dstlen < rlen))
		return -EIO;

	off = sr->op->dst.offset;
	for (m = sr->op->m_dst; m && off > rte_pktmbuf_data_len(m); m = m->next)
		off -= rte_pktmbuf_data_len(m);

	if (unlikely(!m))
		return -EIO;

	mlen = rte_pktmbuf_data_len(m) - off;
	if (rlen <= mlen)
		mlen = rlen;

	ptr = rte_pktmbuf_mtod_offset(m, uint8_t *, off);
	memcpy(ptr, fblk.bytes, mlen);
	i += mlen;
	rlen -= mlen;
	for (m = m->next; m && rlen; m = m->next) {
		mlen = rte_pktmbuf_data_len(m) < rlen ?
			rte_pktmbuf_data_len(m) : rlen;
		ptr = rte_pktmbuf_mtod(m, uint8_t *);
		memcpy(ptr, &fblk.bytes[i], mlen);
		i += mlen;
		rlen -= mlen;
	}

	if (unlikely(rlen != 0))
		return -EIO;

	sr->zip_res.w2.compcode = NITROX_CC_SUCCESS;
	sr->op->status = RTE_COMP_OP_STATUS_SUCCESS;
	sr->zip_res.w0.u64 = rte_cpu_to_be_64(sr->zip_res.w0.u64);
	sr->zip_res.w1.u64 = rte_cpu_to_be_64(sr->zip_res.w1.u64);
	sr->zip_res.w2.u64 = rte_cpu_to_be_64(sr->zip_res.w2.u64);
	return 0;
}

static int create_sglist_from_mbuf(struct nitrox_sgtable *sgtbl,
				   struct rte_mbuf *mbuf, uint32_t off,
				   uint32_t datalen, uint8_t extra_segs,
				   int socket_id)
{
	struct rte_mbuf *m;
	union nitrox_zip_zptr *sgl;
	struct nitrox_zip_iova_addr zip_addr;
	uint16_t nb_segs;
	uint16_t i;
	uint32_t mlen;

	if (unlikely(datalen > NITROX_ZIP_MAX_DATASIZE)) {
		NITROX_LOG_LINE(ERR, "Unsupported datalen %d, max supported %d",
			   datalen, NITROX_ZIP_MAX_DATASIZE);
		return -ENOTSUP;
	}

	nb_segs = mbuf->nb_segs + extra_segs;
	for (m = mbuf; m && off > rte_pktmbuf_data_len(m); m = m->next) {
		off -= rte_pktmbuf_data_len(m);
		nb_segs--;
	}

	if (unlikely(nb_segs > NITROX_ZIP_MAX_ZPTRS)) {
		NITROX_LOG_LINE(ERR, "Mbuf has more segments %d than supported",
			   nb_segs);
		return -ENOTSUP;
	}

	if (unlikely(nb_segs > sgtbl->nb_sgls)) {
		union nitrox_zip_zptr *sgl;

		NITROX_LOG_LINE(INFO, "Mbuf has more segs %d than allocated %d",
			   nb_segs, sgtbl->nb_sgls);
		sgl = rte_realloc_socket(sgtbl->sgl,
					 sizeof(*sgtbl->sgl) * nb_segs,
					 8, socket_id);
		if (unlikely(!sgl)) {
			NITROX_LOG_LINE(ERR, "Failed to expand sglist memory");
			return -ENOMEM;
		}

		sgtbl->sgl = sgl;
		sgtbl->nb_sgls = nb_segs;
	}

	sgtbl->filled_sgls = 0;
	sgtbl->total_bytes = 0;
	sgl = sgtbl->sgl;
	if (!m)
		return 0;

	mlen = rte_pktmbuf_data_len(m) - off;
	if (datalen <= mlen)
		mlen = datalen;

	i = 0;
	zip_addr.u64 = rte_pktmbuf_iova_offset(m, off);
	sgl[i].s.addr = zip_addr.zda.addr;
	sgl[i].s.length = mlen;
	sgl[i].s.le = 0;
	sgtbl->total_bytes += mlen;
	sgtbl->addr_msb = zip_addr.zda.addr_msb;
	datalen -= mlen;
	i++;
	for (m = m->next; m && datalen; m = m->next) {
		mlen = rte_pktmbuf_data_len(m) < datalen ?
			rte_pktmbuf_data_len(m) : datalen;
		zip_addr.u64 = rte_pktmbuf_iova(m);
		if (unlikely(zip_addr.zda.addr_msb != sgtbl->addr_msb)) {
			NITROX_LOG_LINE(ERR, "zip_ptrs have different msb addr");
			return -ENOTSUP;
		}

		sgl[i].s.addr = zip_addr.zda.addr;
		sgl[i].s.length = mlen;
		sgl[i].s.le = 0;
		sgtbl->total_bytes += mlen;
		datalen -= mlen;
		i++;
	}

	sgtbl->filled_sgls = i;
	return 0;
}

static int softreq_init(struct nitrox_softreq *sr,
			struct nitrox_comp_xform *xform)
{
	struct rte_mempool *mp;
	int err;
	bool need_decomp_threshold;

	mp = rte_mempool_from_obj(sr);
	if (unlikely(mp == NULL))
		return -EINVAL;

	err = create_sglist_from_mbuf(&sr->src, sr->op->m_src,
				      sr->op->src.offset,
				      sr->op->src.length, 0, mp->socket_id);
	if (unlikely(err))
		return err;

	need_decomp_threshold = (sr->op->op_type == RTE_COMP_OP_STATELESS &&
				 xform->op == NITROX_COMP_OP_DECOMPRESS);
	err = create_sglist_from_mbuf(&sr->dst, sr->op->m_dst,
			sr->op->dst.offset,
			rte_pktmbuf_pkt_len(sr->op->m_dst) - sr->op->dst.offset,
			need_decomp_threshold ? 1 : 0,
			mp->socket_id);
	if (unlikely(err))
		return err;

	if (need_decomp_threshold) {
		struct nitrox_zip_iova_addr zip_addr;
		int i;

		zip_addr.u64 = rte_mempool_virt2iova(sr) +
			offsetof(struct nitrox_softreq, decomp_threshold);
		i = sr->dst.filled_sgls;
		sr->dst.sgl[i].s.addr = zip_addr.zda.addr;
		sr->dst.sgl[i].s.length = NITROX_ZIP_MAX_ONFSIZE;
		sr->dst.sgl[i].s.le = 0;
		sr->dst.total_bytes += NITROX_ZIP_MAX_ONFSIZE;
		sr->dst.filled_sgls++;
	}

	return 0;
}

static void nitrox_zip_instr_to_b64(struct nitrox_softreq *sr)
{
	struct nitrox_zip_instr *instr = &sr->instr;
	int i;

	for (i = 0; instr->w0.dg && (i < instr->w4.ilen); i++)
		sr->src.sgl[i].u64 = rte_cpu_to_be_64(sr->src.sgl[i].u64);

	for (i = 0; instr->w0.ds && (i < instr->w5.olen); i++)
		sr->dst.sgl[i].u64 = rte_cpu_to_be_64(sr->dst.sgl[i].u64);

	instr->w0.u64 = rte_cpu_to_be_64(instr->w0.u64);
	instr->w1.u64 = rte_cpu_to_be_64(instr->w1.u64);
	instr->w2.u64 = rte_cpu_to_be_64(instr->w2.u64);
	instr->w3.u64 = rte_cpu_to_be_64(instr->w3.u64);
	instr->w4.u64 = rte_cpu_to_be_64(instr->w4.u64);
	instr->w5.u64 = rte_cpu_to_be_64(instr->w5.u64);
	instr->w6.u64 = rte_cpu_to_be_64(instr->w6.u64);
	instr->w7.u64 = rte_cpu_to_be_64(instr->w7.u64);
}

static int process_zip_request(struct nitrox_softreq *sr)
{
	struct nitrox_zip_instr *instr;
	struct nitrox_comp_xform *xform;
	struct nitrox_zip_iova_addr zip_addr;
	uint64_t iptr_msb, optr_msb, rptr_msb, cptr_msb, hptr_msb;
	int err;

	xform = sr->op->private_xform;
	if (unlikely(xform == NULL)) {
		NITROX_LOG_LINE(ERR, "Invalid stateless comp op");
		return -EINVAL;
	}

	if (unlikely(sr->op->op_type == RTE_COMP_OP_STATEFUL &&
		     xform->op == NITROX_COMP_OP_COMPRESS &&
		     sr->op->flush_flag == RTE_COMP_FLUSH_FINAL &&
		     sr->op->src.length == 0))
		return handle_zero_length_compression(sr, xform);

	if (unlikely(sr->op->op_type == RTE_COMP_OP_STATELESS &&
		     xform->op == NITROX_COMP_OP_COMPRESS &&
		     sr->op->flush_flag != RTE_COMP_FLUSH_FULL &&
		     sr->op->flush_flag != RTE_COMP_FLUSH_FINAL)) {
		NITROX_LOG_LINE(ERR, "Invalid flush flag %d in stateless op",
			   sr->op->flush_flag);
		return -EINVAL;
	}

	err = softreq_init(sr, xform);
	if (unlikely(err))
		return err;

	instr = &sr->instr;
	memset(instr, 0, sizeof(*instr));
	/* word 0 */
	instr->w0.tol = sr->dst.total_bytes;
	instr->w0.exn = xform->exn;
	instr->w0.exbits = xform->exbits;
	instr->w0.ca = 0;
	if (xform->op == NITROX_COMP_OP_DECOMPRESS ||
	    sr->op->flush_flag == RTE_COMP_FLUSH_SYNC ||
	    sr->op->flush_flag == RTE_COMP_FLUSH_FULL)
		instr->w0.sf = 1;
	else
		instr->w0.sf = 0;

	instr->w0.ss = xform->level;
	instr->w0.cc = xform->algo;
	if (sr->op->flush_flag == RTE_COMP_FLUSH_FINAL)
		instr->w0.ef = 1;
	else
		instr->w0.ef = 0;

	instr->w0.bf = xform->bf;
	instr->w0.co = xform->op;
	if (sr->dst.filled_sgls > 1)
		instr->w0.ds = 1;
	else
		instr->w0.ds = 0;

	if (sr->src.filled_sgls > 1)
		instr->w0.dg = 1;
	else
		instr->w0.dg = 0;

	instr->w0.hg = 0;

	/* word 1 */
	instr->w1.hl = xform->hlen;
	if (sr->op->op_type == RTE_COMP_OP_STATEFUL && !xform->bf)
		instr->w1.adlercrc32 = xform->chksum;
	else if (sr->op->op_type == RTE_COMP_OP_STATELESS &&
		 sr->op->input_chksum != 0)
		instr->w1.adlercrc32 = sr->op->input_chksum;
	else if (xform->chksum_type == NITROX_CHKSUM_TYPE_ADLER32)
		instr->w1.adlercrc32 = 1;
	else if (xform->chksum_type == NITROX_CHKSUM_TYPE_CRC32)
		instr->w1.adlercrc32 = 0;

	/* word 2 */
	if (xform->context)
		zip_addr.u64 = rte_malloc_virt2iova(xform->context);
	else
		zip_addr.u64 = 0;

	instr->w2.cptr = zip_addr.zda.addr;
	cptr_msb = zip_addr.zda.addr_msb;

	/* word 3 */
	instr->w3.hlen = xform->hlen;
	if (xform->history_window)
		zip_addr.u64 = rte_malloc_virt2iova(xform->history_window);
	else
		zip_addr.u64 = 0;

	instr->w3.hptr = zip_addr.zda.addr;
	hptr_msb = zip_addr.zda.addr_msb;

	/* word 4 */
	if (sr->src.filled_sgls == 1) {
		instr->w4.ilen = sr->src.sgl[0].s.length;
		instr->w4.iptr = sr->src.sgl[0].s.addr;
		iptr_msb = sr->src.addr_msb;
	} else {
		zip_addr.u64 = rte_malloc_virt2iova(sr->src.sgl);
		instr->w4.ilen = sr->src.filled_sgls;
		instr->w4.iptr = zip_addr.zda.addr;
		iptr_msb = zip_addr.zda.addr_msb;
	}

	/* word 5 */
	if (sr->dst.filled_sgls == 1) {
		instr->w5.olen = sr->dst.sgl[0].s.length;
		instr->w5.optr = sr->dst.sgl[0].s.addr;
		optr_msb = sr->dst.addr_msb;
	} else {
		zip_addr.u64 = rte_malloc_virt2iova(sr->dst.sgl);
		instr->w5.olen = sr->dst.filled_sgls;
		instr->w5.optr = zip_addr.zda.addr;
		optr_msb = zip_addr.zda.addr_msb;
	}

	/* word 6 */
	memset(&sr->zip_res, 0, sizeof(sr->zip_res));
	zip_addr.u64 = rte_mempool_virt2iova(sr) +
		offsetof(struct nitrox_softreq, zip_res);
	instr->w6.rptr = zip_addr.zda.addr;
	rptr_msb = zip_addr.zda.addr_msb;

	if (unlikely(iptr_msb != optr_msb || iptr_msb != rptr_msb ||
	    (xform->history_window && (iptr_msb != hptr_msb)) ||
	    (xform->context && (iptr_msb != cptr_msb)))) {
		NITROX_LOG_LINE(ERR, "addr_msb is not same for all addresses");
		return -ENOTSUP;
	}

	/* word 7 */
	instr->w7.addr_msb = iptr_msb;
	instr->w7.grp = 0;

	nitrox_dump_zip_instr(instr, NULL, sr->src.sgl, sr->dst.sgl);
	nitrox_dump_databuf("IN", sr->op->m_src, sr->op->src.offset,
			    sr->op->src.length);
	nitrox_zip_instr_to_b64(sr);
	return 0;
}

int
nitrox_process_comp_req(struct rte_comp_op *op, struct nitrox_softreq *sr)
{
	int err;

	sr->op = op;
	sr->op->status = RTE_COMP_OP_STATUS_NOT_PROCESSED;
	err = process_zip_request(sr);
	if (unlikely(err))
		goto err_exit;

	sr->timeout = rte_get_timer_cycles() + CMD_TIMEOUT * rte_get_timer_hz();
	return 0;
err_exit:
	if (err == -ENOMEM)
		sr->op->status = RTE_COMP_OP_STATUS_ERROR;
	else
		sr->op->status = RTE_COMP_OP_STATUS_INVALID_ARGS;

	return err;
}

static struct nitrox_zip_result zip_result_to_cpu64(struct nitrox_zip_result *r)
{
	struct nitrox_zip_result out_res;

	out_res.w2.u64 = rte_be_to_cpu_64(r->w2.u64);
	out_res.w1.u64 = rte_be_to_cpu_64(r->w1.u64);
	out_res.w0.u64 = rte_be_to_cpu_64(r->w0.u64);
	return out_res;
}

static int post_process_zip_stateless(struct nitrox_softreq *sr,
				      struct nitrox_comp_xform *xform,
				      struct nitrox_zip_result *zip_res)
{
	int output_unused_bytes;

	if (unlikely(zip_res->w2.compcode != NITROX_CC_SUCCESS)) {
		struct rte_comp_op *op = sr->op;

		NITROX_LOG_LINE(ERR, "Dequeue error 0x%x",
			   zip_res->w2.compcode);
		if (zip_res->w2.compcode == NITROX_CC_STOP ||
		    zip_res->w2.compcode == NITROX_CC_DTRUNC)
			op->status = RTE_COMP_OP_STATUS_OUT_OF_SPACE_TERMINATED;
		else
			op->status = RTE_COMP_OP_STATUS_ERROR;

		op->consumed = 0;
		op->produced = 0;
		return -EFAULT;
	}

	output_unused_bytes = sr->dst.total_bytes - zip_res->w1.tbyteswritten;
	if (unlikely(xform->op == NITROX_COMP_OP_DECOMPRESS &&
		     output_unused_bytes < NITROX_ZIP_MAX_ONFSIZE)) {
		NITROX_LOG_LINE(ERR, "TOL %d, Total bytes written %d",
			   sr->dst.total_bytes, zip_res->w1.tbyteswritten);
		sr->op->status = RTE_COMP_OP_STATUS_OUT_OF_SPACE_TERMINATED;
		sr->op->consumed = 0;
		sr->op->produced = sr->dst.total_bytes - NITROX_ZIP_MAX_ONFSIZE;
		return -EIO;
	}

	if (xform->chksum_type == NITROX_CHKSUM_TYPE_CRC32)
		sr->op->output_chksum = zip_res->w0.crc32;
	else if (xform->chksum_type == NITROX_CHKSUM_TYPE_ADLER32)
		sr->op->output_chksum = zip_res->w0.adler32;

	sr->op->consumed = RTE_MIN(sr->op->src.length,
				   (uint32_t)zip_res->w1.tbytesread);
	sr->op->produced = zip_res->w1.tbyteswritten;
	sr->op->status = RTE_COMP_OP_STATUS_SUCCESS;
	return 0;
}

static int update_history(struct rte_mbuf *mbuf, uint32_t off, uint16_t datalen,
			   uint8_t *dst)
{
	struct rte_mbuf *m;
	uint32_t mlen;
	uint16_t copied = 0;

	for (m = mbuf; m && off > rte_pktmbuf_data_len(m); m = m->next)
		off -= rte_pktmbuf_data_len(m);

	if (unlikely(!m)) {
		NITROX_LOG_LINE(ERR, "Failed to update history. Invalid mbuf");
		return -EINVAL;
	}

	mlen = rte_pktmbuf_data_len(m) - off;
	if (datalen <= mlen)
		mlen = datalen;

	memcpy(&dst[copied], rte_pktmbuf_mtod_offset(m, char *, off), mlen);
	copied += mlen;
	datalen -= mlen;
	for (m = m->next; m && datalen; m = m->next) {
		mlen = rte_pktmbuf_data_len(m) < datalen ?
			rte_pktmbuf_data_len(m) : datalen;
		memcpy(&dst[copied], rte_pktmbuf_mtod(m, char *), mlen);
		copied += mlen;
		datalen -= mlen;
	}

	if (unlikely(datalen != 0)) {
		NITROX_LOG_LINE(ERR, "Failed to update history. Invalid datalen");
		return -EINVAL;
	}

	return 0;
}

static void reset_nitrox_xform(struct nitrox_comp_xform *xform)
{
	xform->hlen = 0;
	xform->exn = 0;
	xform->exbits = 0;
	xform->bf = true;
}

static int post_process_zip_stateful(struct nitrox_softreq *sr,
				     struct nitrox_comp_xform *xform,
				     struct nitrox_zip_result *zip_res)
{
	uint32_t bytesread = 0;
	uint32_t chksum = 0;

	if (unlikely(zip_res->w2.compcode == NITROX_CC_DTRUNC)) {
		sr->op->consumed = 0;
		sr->op->produced = 0;
		xform->hlen = 0;
		sr->op->status = RTE_COMP_OP_STATUS_OUT_OF_SPACE_RECOVERABLE;
		NITROX_LOG_LINE(ERR, "Dequeue compress DTRUNC error");
		return 0;
	} else if (unlikely(zip_res->w2.compcode == NITROX_CC_STOP)) {
		sr->op->status = RTE_COMP_OP_STATUS_OUT_OF_SPACE_RECOVERABLE;
		NITROX_LOG_LINE(NOTICE, "Dequeue decompress dynamic STOP");
	} else if (zip_res->w2.compcode == NITROX_CC_SUCCESS) {
		sr->op->status = RTE_COMP_OP_STATUS_SUCCESS;
	} else {
		xform->hlen = 0;
		xform->exn = 0;
		xform->exbits = 0;
		xform->bf = true;
		sr->op->status = RTE_COMP_OP_STATUS_ERROR;
		NITROX_LOG_LINE(ERR, "Dequeue error 0x%x",
			   zip_res->w2.compcode);
		return -EFAULT;
	}

	if (xform->op == NITROX_COMP_OP_COMPRESS) {
		if (zip_res->w1.tbytesread < xform->hlen) {
			NITROX_LOG_LINE(ERR, "Invalid bytesread");
			reset_nitrox_xform(xform);
			sr->op->status = RTE_COMP_OP_STATUS_ERROR;
			return -EFAULT;
		}

		bytesread = zip_res->w1.tbytesread - xform->hlen;
	} else {
		bytesread = RTE_MIN(sr->op->src.length,
				    (uint32_t)zip_res->w1.tbytesread);
	}

	if ((xform->op == NITROX_COMP_OP_COMPRESS &&
	    (sr->op->flush_flag == RTE_COMP_FLUSH_NONE ||
	    sr->op->flush_flag == RTE_COMP_FLUSH_SYNC)) ||
	    (xform->op == NITROX_COMP_OP_DECOMPRESS && !zip_res->w2.ef)) {
		struct rte_mbuf *mbuf;
		uint32_t pktlen, m_off;
		int err;

		if (xform->op == NITROX_COMP_OP_COMPRESS) {
			mbuf = sr->op->m_src;
			pktlen = bytesread;
			m_off = sr->op->src.offset;
		} else {
			mbuf = sr->op->m_dst;
			pktlen = zip_res->w1.tbyteswritten;
			m_off = sr->op->dst.offset;
		}

		if (pktlen >= xform->window_size) {
			m_off += pktlen - xform->window_size;
			err = update_history(mbuf, m_off, xform->window_size,
				       xform->history_window);
			xform->hlen = xform->window_size;
		} else if ((xform->hlen + pktlen) <= xform->window_size) {
			err = update_history(mbuf, m_off, pktlen,
				       &xform->history_window[xform->hlen]);
			xform->hlen += pktlen;
		} else {
			uint16_t shift_off, shift_len;

			shift_off = pktlen + xform->hlen - xform->window_size;
			shift_len = xform->hlen - shift_off;
			memmove(xform->history_window,
				&xform->history_window[shift_off],
				shift_len);
			err = update_history(mbuf, m_off, pktlen,
				       &xform->history_window[shift_len]);
			xform->hlen = xform->window_size;

		}

		if (unlikely(err)) {
			sr->op->status = RTE_COMP_OP_STATUS_ERROR;
			return err;
		}

		if (xform->op == NITROX_COMP_OP_COMPRESS) {
			xform->exn = zip_res->w2.exn;
			xform->exbits = zip_res->w2.exbits;
		}

		xform->bf = false;
	} else {
		reset_nitrox_xform(xform);
	}

	if (xform->chksum_type == NITROX_CHKSUM_TYPE_CRC32)
		chksum = zip_res->w0.crc32;
	else if (xform->chksum_type == NITROX_CHKSUM_TYPE_ADLER32)
		chksum = zip_res->w0.adler32;

	if (xform->bf)
		sr->op->output_chksum = chksum;
	else
		xform->chksum = chksum;

	sr->op->consumed = bytesread;
	sr->op->produced = zip_res->w1.tbyteswritten;
	return 0;
}

int
nitrox_check_comp_req(struct nitrox_softreq *sr, struct rte_comp_op **op)
{
	struct nitrox_zip_result zip_res;
	struct nitrox_comp_xform *xform;
	int err = 0;

	zip_res = zip_result_to_cpu64(&sr->zip_res);
	if (zip_res.w2.compcode == NITROX_CC_NOTDONE) {
		if (rte_get_timer_cycles() >= sr->timeout) {
			NITROX_LOG_LINE(ERR, "Op timedout");
			sr->op->status = RTE_COMP_OP_STATUS_ERROR;
			err = -ETIMEDOUT;
			goto exit;
		} else {
			return -EAGAIN;
		}
	}

	xform = sr->op->private_xform;
	if (sr->op->op_type == RTE_COMP_OP_STATELESS)
		err = post_process_zip_stateless(sr, xform, &zip_res);
	else
		err = post_process_zip_stateful(sr, xform, &zip_res);

	if (sr->op->status == RTE_COMP_OP_STATUS_SUCCESS &&
	    xform->op == NITROX_COMP_OP_COMPRESS &&
	    sr->op->flush_flag == RTE_COMP_FLUSH_FINAL &&
	    zip_res.w2.exn) {
		uint32_t datalen = zip_res.w1.tbyteswritten;
		uint32_t off = sr->op->dst.offset;
		struct rte_mbuf *m = sr->op->m_dst;
		uint32_t mlen;
		uint8_t *last_byte;

		for (; m && off > rte_pktmbuf_data_len(m); m = m->next)
			off -= rte_pktmbuf_data_len(m);

		if (unlikely(m == NULL)) {
			err = -EINVAL;
			goto exit;
		}

		mlen = rte_pktmbuf_data_len(m) - off;
		for (; m && (datalen > mlen); m = m->next)
			datalen -= mlen;

		if (unlikely(m == NULL)) {
			err = -EINVAL;
			goto exit;
		}

		last_byte = rte_pktmbuf_mtod_offset(m, uint8_t *, datalen - 1);
		*last_byte = zip_res.w2.exbits & 0xFF;
	}

exit:
	*op = sr->op;
	nitrox_dump_zip_result(&sr->instr, &zip_res);
	nitrox_dump_databuf("OUT after", sr->op->m_dst, sr->op->dst.offset,
		sr->op->produced);
	return err;
}

void *
nitrox_comp_instr_addr(struct nitrox_softreq *sr)
{
	return &sr->instr;
}

static void req_pool_obj_free(struct rte_mempool *mp, void *opaque, void *obj,
			      unsigned int obj_idx)
{
	struct nitrox_softreq *sr;

	RTE_SET_USED(mp);
	RTE_SET_USED(opaque);
	RTE_SET_USED(obj_idx);
	sr = obj;
	rte_free(sr->src.sgl);
	sr->src.sgl = NULL;
	rte_free(sr->dst.sgl);
	sr->dst.sgl = NULL;
}

void
nitrox_comp_req_pool_free(struct rte_mempool *mp)
{
	rte_mempool_obj_iter(mp, req_pool_obj_free, NULL);
	rte_mempool_free(mp);
}

static void req_pool_obj_init(struct rte_mempool *mp, void *arg, void *obj,
			      unsigned int obj_idx)
{
	struct nitrox_softreq *sr;
	int *err = arg;

	RTE_SET_USED(mp);
	RTE_SET_USED(obj_idx);
	sr = obj;
	sr->src.sgl = rte_zmalloc_socket(NULL,
				sizeof(*sr->src.sgl) * NITROX_ZIP_SGL_COUNT,
				8, mp->socket_id);
	sr->dst.sgl = rte_zmalloc_socket(NULL,
				sizeof(*sr->dst.sgl) * NITROX_ZIP_SGL_COUNT,
				8, mp->socket_id);
	if (sr->src.sgl == NULL || sr->dst.sgl == NULL) {
		NITROX_LOG_LINE(ERR, "Failed to allocate zip_sgl memory");
		*err = -ENOMEM;
	}

	sr->src.nb_sgls = NITROX_ZIP_SGL_COUNT;
	sr->src.filled_sgls = 0;
	sr->dst.nb_sgls = NITROX_ZIP_SGL_COUNT;
	sr->dst.filled_sgls = 0;
}

struct rte_mempool *
nitrox_comp_req_pool_create(struct rte_compressdev *dev, uint32_t nobjs,
			   uint16_t qp_id, int socket_id)
{
	char softreq_pool_name[RTE_RING_NAMESIZE];
	struct rte_mempool *mp;
	int err = 0;

	snprintf(softreq_pool_name, RTE_RING_NAMESIZE, "%s_sr_%d",
		 dev->data->name, qp_id);
	mp = rte_mempool_create(softreq_pool_name,
				RTE_ALIGN_MUL_CEIL(nobjs, 64),
				sizeof(struct nitrox_softreq),
				64, 0, NULL, NULL, req_pool_obj_init, &err,
				socket_id, 0);
	if (unlikely(!mp))
		NITROX_LOG_LINE(ERR, "Failed to create req pool, qid %d, err %d",
			   qp_id, rte_errno);

	if (unlikely(err)) {
		nitrox_comp_req_pool_free(mp);
		return NULL;
	}

	return mp;
}
