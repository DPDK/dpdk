/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_crypto.h>
#include <cryptodev_pmd.h>
#include <rte_cycles.h>
#include <rte_errno.h>

#include "nitrox_sym_reqmgr.h"
#include "nitrox_logs.h"

#define MAX_SGBUF_CNT 16
#define MAX_SGCOMP_CNT 5
/* SLC_STORE_INFO */
#define MIN_UDD_LEN 16
/* PKT_IN_HDR + SLC_STORE_INFO */
#define FDATA_SIZE 32
/* Base destination port for the solicited requests */
#define SOLICIT_BASE_DPORT 256
#define PENDING_SIG 0xFFFFFFFFFFFFFFFFUL
#define CMD_TIMEOUT 2

struct gphdr {
	uint16_t param0;
	uint16_t param1;
	uint16_t param2;
	uint16_t param3;
};

union pkt_instr_hdr {
	uint64_t value;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t raz_48_63 : 16;
		uint64_t g : 1;
		uint64_t gsz : 7;
		uint64_t ihi : 1;
		uint64_t ssz : 7;
		uint64_t raz_30_31 : 2;
		uint64_t fsz : 6;
		uint64_t raz_16_23 : 8;
		uint64_t tlen : 16;
#else
		uint64_t tlen : 16;
		uint64_t raz_16_23 : 8;
		uint64_t fsz : 6;
		uint64_t raz_30_31 : 2;
		uint64_t ssz : 7;
		uint64_t ihi : 1;
		uint64_t gsz : 7;
		uint64_t g : 1;
		uint64_t raz_48_63 : 16;
#endif
	} s;
};

union pkt_hdr {
	uint64_t value[2];
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t opcode : 8;
		uint64_t arg : 8;
		uint64_t ctxc : 2;
		uint64_t unca : 1;
		uint64_t raz_44 : 1;
		uint64_t info : 3;
		uint64_t destport : 9;
		uint64_t unc : 8;
		uint64_t raz_19_23 : 5;
		uint64_t grp : 3;
		uint64_t raz_15 : 1;
		uint64_t ctxl : 7;
		uint64_t uddl : 8;
#else
		uint64_t uddl : 8;
		uint64_t ctxl : 7;
		uint64_t raz_15 : 1;
		uint64_t grp : 3;
		uint64_t raz_19_23 : 5;
		uint64_t unc : 8;
		uint64_t destport : 9;
		uint64_t info : 3;
		uint64_t raz_44 : 1;
		uint64_t unca : 1;
		uint64_t ctxc : 2;
		uint64_t arg : 8;
		uint64_t opcode : 8;
#endif
		uint64_t ctxp;
	} s;
};

union slc_store_info {
	uint64_t value[2];
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint64_t raz_39_63 : 25;
		uint64_t ssz : 7;
		uint64_t raz_0_31 : 32;
#else
		uint64_t raz_0_31 : 32;
		uint64_t ssz : 7;
		uint64_t raz_39_63 : 25;
#endif
		uint64_t rptr;
	} s;
};

struct nps_pkt_instr {
	uint64_t dptr0;
	union pkt_instr_hdr ih;
	union pkt_hdr irh;
	union slc_store_info slc;
	uint64_t fdata[2];
};

struct resp_hdr {
	uint64_t orh;
	uint64_t completion;
};

struct nitrox_sglist {
	uint16_t len;
	uint16_t raz0;
	uint32_t raz1;
	rte_iova_t iova;
	void *virt;
};

struct nitrox_sgcomp {
	uint16_t len[4];
	uint64_t iova[4];
};

struct nitrox_sgtable {
	uint8_t map_bufs_cnt;
	uint8_t nr_sgcomp;
	uint16_t total_bytes;

	struct nitrox_sglist sglist[MAX_SGBUF_CNT];
	struct nitrox_sgcomp sgcomp[MAX_SGCOMP_CNT];
};

struct iv {
	uint8_t *virt;
	rte_iova_t iova;
	uint16_t len;
};

struct nitrox_softreq {
	struct nitrox_crypto_ctx *ctx;
	struct rte_crypto_op *op;
	struct gphdr gph;
	struct nps_pkt_instr instr;
	struct resp_hdr resp;
	struct nitrox_sgtable in;
	struct nitrox_sgtable out;
	struct iv iv;
	uint64_t timeout;
	rte_iova_t dptr;
	rte_iova_t rptr;
	rte_iova_t iova;
};

static void
softreq_init(struct nitrox_softreq *sr, rte_iova_t iova)
{
	memset(sr, 0, sizeof(*sr));
	sr->iova = iova;
}

/*
 * 64-Byte Instruction Format
 *
 *  ----------------------
 *  |      DPTR0         | 8 bytes
 *  ----------------------
 *  |  PKT_IN_INSTR_HDR  | 8 bytes
 *  ----------------------
 *  |    PKT_IN_HDR      | 16 bytes
 *  ----------------------
 *  |    SLC_INFO        | 16 bytes
 *  ----------------------
 *  |   Front data       | 16 bytes
 *  ----------------------
 */
static void
create_se_instr(struct nitrox_softreq *sr, uint8_t qno)
{
	struct nitrox_crypto_ctx *ctx = sr->ctx;
	rte_iova_t ctx_handle;

	/* fill the packet instruction */
	/* word 0 */
	sr->instr.dptr0 = rte_cpu_to_be_64(sr->dptr);

	/* word 1 */
	sr->instr.ih.value = 0;
	sr->instr.ih.s.g = 1;
	sr->instr.ih.s.gsz = sr->in.map_bufs_cnt;
	sr->instr.ih.s.ssz = sr->out.map_bufs_cnt;
	sr->instr.ih.s.fsz = FDATA_SIZE + sizeof(struct gphdr);
	sr->instr.ih.s.tlen = sr->instr.ih.s.fsz + sr->in.total_bytes;
	sr->instr.ih.value = rte_cpu_to_be_64(sr->instr.ih.value);

	/* word 2 */
	sr->instr.irh.value[0] = 0;
	sr->instr.irh.s.uddl = MIN_UDD_LEN;
	/* context length in 64-bit words */
	sr->instr.irh.s.ctxl = RTE_ALIGN_MUL_CEIL(sizeof(ctx->fctx), 8) / 8;
	/* offset from solicit base port 256 */
	sr->instr.irh.s.destport = SOLICIT_BASE_DPORT + qno;
	/* Invalid context cache */
	sr->instr.irh.s.ctxc = 0x3;
	sr->instr.irh.s.arg = ctx->req_op;
	sr->instr.irh.s.opcode = ctx->opcode;
	sr->instr.irh.value[0] = rte_cpu_to_be_64(sr->instr.irh.value[0]);

	/* word 3 */
	ctx_handle = ctx->iova + offsetof(struct nitrox_crypto_ctx, fctx);
	sr->instr.irh.s.ctxp = rte_cpu_to_be_64(ctx_handle);

	/* word 4 */
	sr->instr.slc.value[0] = 0;
	sr->instr.slc.s.ssz = sr->out.map_bufs_cnt;
	sr->instr.slc.value[0] = rte_cpu_to_be_64(sr->instr.slc.value[0]);

	/* word 5 */
	sr->instr.slc.s.rptr = rte_cpu_to_be_64(sr->rptr);
	/*
	 * No conversion for front data,
	 * It goes into payload
	 * put GP Header in front data
	 */
	memcpy(&sr->instr.fdata[0], &sr->gph, sizeof(sr->instr.fdata[0]));
	sr->instr.fdata[1] = 0;
}

static void
softreq_copy_iv(struct nitrox_softreq *sr, uint8_t salt_size)
{
	uint16_t offset = sr->ctx->iv.offset + salt_size;

	sr->iv.virt = rte_crypto_op_ctod_offset(sr->op, uint8_t *, offset);
	sr->iv.iova = rte_crypto_op_ctophys_offset(sr->op, offset);
	sr->iv.len = sr->ctx->iv.length - salt_size;
}

static void
fill_sglist(struct nitrox_sgtable *sgtbl, uint16_t len, rte_iova_t iova,
	    void *virt)
{
	struct nitrox_sglist *sglist = sgtbl->sglist;
	uint8_t cnt = sgtbl->map_bufs_cnt;

	if (unlikely(!len))
		return;

	sglist[cnt].len = len;
	sglist[cnt].iova = iova;
	sglist[cnt].virt = virt;
	sgtbl->total_bytes += len;
	cnt++;
	sgtbl->map_bufs_cnt = cnt;
}

static int
create_sglist_from_mbuf(struct nitrox_sgtable *sgtbl, struct rte_mbuf *mbuf,
			uint32_t off, int datalen)
{
	struct nitrox_sglist *sglist = sgtbl->sglist;
	uint8_t cnt = sgtbl->map_bufs_cnt;
	struct rte_mbuf *m;
	int mlen;

	if (unlikely(datalen <= 0))
		return 0;

	for (m = mbuf; m && off > rte_pktmbuf_data_len(m); m = m->next)
		off -= rte_pktmbuf_data_len(m);

	if (unlikely(!m))
		return -EIO;

	mlen = rte_pktmbuf_data_len(m) - off;
	if (datalen <= mlen)
		mlen = datalen;
	sglist[cnt].len = mlen;
	sglist[cnt].iova = rte_pktmbuf_iova_offset(m, off);
	sglist[cnt].virt = rte_pktmbuf_mtod_offset(m, uint8_t *, off);
	sgtbl->total_bytes += mlen;
	cnt++;
	datalen -= mlen;
	for (m = m->next; m && datalen; m = m->next) {
		mlen = rte_pktmbuf_data_len(m) < datalen ?
			rte_pktmbuf_data_len(m) : datalen;
		sglist[cnt].len = mlen;
		sglist[cnt].iova = rte_pktmbuf_iova(m);
		sglist[cnt].virt = rte_pktmbuf_mtod(m, uint8_t *);
		sgtbl->total_bytes += mlen;
		cnt++;
		datalen -= mlen;
	}

	RTE_VERIFY(cnt <= MAX_SGBUF_CNT);
	sgtbl->map_bufs_cnt = cnt;
	return 0;
}

static void
create_sgcomp(struct nitrox_sgtable *sgtbl)
{
	int i, j, nr_sgcomp;
	struct nitrox_sgcomp *sgcomp = sgtbl->sgcomp;
	struct nitrox_sglist *sglist = sgtbl->sglist;

	nr_sgcomp = RTE_ALIGN_MUL_CEIL(sgtbl->map_bufs_cnt, 4) / 4;
	sgtbl->nr_sgcomp = nr_sgcomp;
	for (i = 0; i < nr_sgcomp; i++, sgcomp++) {
		for (j = 0; j < 4; j++, sglist++) {
			sgcomp->len[j] = rte_cpu_to_be_16(sglist->len);
			sgcomp->iova[j] = rte_cpu_to_be_64(sglist->iova);
		}
	}
}

static int
create_cipher_inbuf(struct nitrox_softreq *sr)
{
	int err;
	struct rte_crypto_op *op = sr->op;

	fill_sglist(&sr->in, sr->iv.len, sr->iv.iova, sr->iv.virt);
	err = create_sglist_from_mbuf(&sr->in, op->sym->m_src,
				      op->sym->cipher.data.offset,
				      op->sym->cipher.data.length);
	if (unlikely(err))
		return err;

	create_sgcomp(&sr->in);
	sr->dptr = sr->iova + offsetof(struct nitrox_softreq, in.sgcomp);

	return 0;
}

static int
create_cipher_outbuf(struct nitrox_softreq *sr)
{
	struct rte_crypto_op *op = sr->op;
	int err, cnt = 0;
	struct rte_mbuf *m_dst = op->sym->m_dst ? op->sym->m_dst :
		op->sym->m_src;

	sr->resp.orh = PENDING_SIG;
	sr->out.sglist[cnt].len = sizeof(sr->resp.orh);
	sr->out.sglist[cnt].iova = sr->iova + offsetof(struct nitrox_softreq,
						       resp.orh);
	sr->out.sglist[cnt].virt = &sr->resp.orh;
	cnt++;

	sr->out.map_bufs_cnt = cnt;
	fill_sglist(&sr->out, sr->iv.len, sr->iv.iova, sr->iv.virt);
	err = create_sglist_from_mbuf(&sr->out, m_dst,
				      op->sym->cipher.data.offset,
				      op->sym->cipher.data.length);
	if (unlikely(err))
		return err;

	cnt = sr->out.map_bufs_cnt;
	sr->resp.completion = PENDING_SIG;
	sr->out.sglist[cnt].len = sizeof(sr->resp.completion);
	sr->out.sglist[cnt].iova = sr->iova + offsetof(struct nitrox_softreq,
						     resp.completion);
	sr->out.sglist[cnt].virt = &sr->resp.completion;
	cnt++;

	RTE_VERIFY(cnt <= MAX_SGBUF_CNT);
	sr->out.map_bufs_cnt = cnt;

	create_sgcomp(&sr->out);
	sr->rptr = sr->iova + offsetof(struct nitrox_softreq, out.sgcomp);

	return 0;
}

static void
create_cipher_gph(uint32_t cryptlen, uint16_t ivlen, struct gphdr *gph)
{
	gph->param0 = rte_cpu_to_be_16(cryptlen);
	gph->param1 = 0;
	gph->param2 = rte_cpu_to_be_16(ivlen);
	gph->param3 = 0;
}

static int
process_cipher_data(struct nitrox_softreq *sr)
{
	struct rte_crypto_op *op = sr->op;
	int err;

	softreq_copy_iv(sr, 0);
	err = create_cipher_inbuf(sr);
	if (unlikely(err))
		return err;

	err = create_cipher_outbuf(sr);
	if (unlikely(err))
		return err;

	create_cipher_gph(op->sym->cipher.data.length, sr->iv.len, &sr->gph);

	return 0;
}

static int
extract_cipher_auth_digest(struct nitrox_softreq *sr,
			   struct nitrox_sglist *digest)
{
	struct rte_crypto_op *op = sr->op;
	struct rte_mbuf *mdst = op->sym->m_dst ? op->sym->m_dst :
					op->sym->m_src;

	if (sr->ctx->req_op == NITROX_OP_DECRYPT &&
	    unlikely(!op->sym->auth.digest.data))
		return -EINVAL;

	digest->len = sr->ctx->digest_length;
	if (op->sym->auth.digest.data) {
		digest->iova = op->sym->auth.digest.phys_addr;
		digest->virt = op->sym->auth.digest.data;
		return 0;
	}

	if (unlikely(rte_pktmbuf_data_len(mdst) < op->sym->auth.data.offset +
	       op->sym->auth.data.length + digest->len))
		return -EINVAL;

	digest->iova = rte_pktmbuf_iova_offset(mdst,
					op->sym->auth.data.offset +
					op->sym->auth.data.length);
	digest->virt = rte_pktmbuf_mtod_offset(mdst, uint8_t *,
					op->sym->auth.data.offset +
					op->sym->auth.data.length);
	return 0;
}

static int
create_cipher_auth_sglist(struct nitrox_softreq *sr,
			  struct nitrox_sgtable *sgtbl, struct rte_mbuf *mbuf)
{
	struct rte_crypto_op *op = sr->op;
	int auth_only_len;
	int err;

	fill_sglist(sgtbl, sr->iv.len, sr->iv.iova, sr->iv.virt);
	auth_only_len = op->sym->auth.data.length - op->sym->cipher.data.length;
	if (unlikely(auth_only_len < 0))
		return -EINVAL;

	if (unlikely(
		op->sym->cipher.data.offset + op->sym->cipher.data.length !=
		op->sym->auth.data.offset + op->sym->auth.data.length)) {
		NITROX_LOG(ERR, "Auth only data after cipher data not supported\n");
		return -ENOTSUP;
	}

	err = create_sglist_from_mbuf(sgtbl, mbuf, op->sym->auth.data.offset,
				      auth_only_len);
	if (unlikely(err))
		return err;

	err = create_sglist_from_mbuf(sgtbl, mbuf, op->sym->cipher.data.offset,
				      op->sym->cipher.data.length);
	if (unlikely(err))
		return err;

	return 0;
}

static int
create_combined_sglist(struct nitrox_softreq *sr, struct nitrox_sgtable *sgtbl,
		       struct rte_mbuf *mbuf)
{
	struct rte_crypto_op *op = sr->op;

	fill_sglist(sgtbl, sr->iv.len, sr->iv.iova, sr->iv.virt);
	fill_sglist(sgtbl, sr->ctx->aad_length, op->sym->aead.aad.phys_addr,
		    op->sym->aead.aad.data);
	return create_sglist_from_mbuf(sgtbl, mbuf, op->sym->cipher.data.offset,
				       op->sym->cipher.data.length);
}

static int
create_aead_sglist(struct nitrox_softreq *sr, struct nitrox_sgtable *sgtbl,
		   struct rte_mbuf *mbuf)
{
	int err;

	switch (sr->ctx->nitrox_chain) {
	case NITROX_CHAIN_CIPHER_AUTH:
	case NITROX_CHAIN_AUTH_CIPHER:
		err = create_cipher_auth_sglist(sr, sgtbl, mbuf);
		break;
	case NITROX_CHAIN_COMBINED:
		err = create_combined_sglist(sr, sgtbl, mbuf);
		break;
	default:
		err = -EINVAL;
		break;
	}

	return err;
}

static int
create_aead_inbuf(struct nitrox_softreq *sr, struct nitrox_sglist *digest)
{
	int err;
	struct nitrox_crypto_ctx *ctx = sr->ctx;

	err = create_aead_sglist(sr, &sr->in, sr->op->sym->m_src);
	if (unlikely(err))
		return err;

	if (ctx->req_op == NITROX_OP_DECRYPT)
		fill_sglist(&sr->in, digest->len, digest->iova, digest->virt);

	create_sgcomp(&sr->in);
	sr->dptr = sr->iova + offsetof(struct nitrox_softreq, in.sgcomp);
	return 0;
}

static int
create_aead_oop_outbuf(struct nitrox_softreq *sr, struct nitrox_sglist *digest)
{
	int err;
	struct nitrox_crypto_ctx *ctx = sr->ctx;

	err = create_aead_sglist(sr, &sr->out, sr->op->sym->m_dst);
	if (unlikely(err))
		return err;

	if (ctx->req_op == NITROX_OP_ENCRYPT)
		fill_sglist(&sr->out, digest->len, digest->iova, digest->virt);

	return 0;
}

static void
create_aead_inplace_outbuf(struct nitrox_softreq *sr,
			   struct nitrox_sglist *digest)
{
	int i, cnt;
	struct nitrox_crypto_ctx *ctx = sr->ctx;

	cnt = sr->out.map_bufs_cnt;
	for (i = 0; i < sr->in.map_bufs_cnt; i++, cnt++) {
		sr->out.sglist[cnt].len = sr->in.sglist[i].len;
		sr->out.sglist[cnt].iova = sr->in.sglist[i].iova;
		sr->out.sglist[cnt].virt = sr->in.sglist[i].virt;
	}

	sr->out.map_bufs_cnt = cnt;
	if (ctx->req_op == NITROX_OP_ENCRYPT) {
		fill_sglist(&sr->out, digest->len, digest->iova,
			    digest->virt);
	} else if (ctx->req_op == NITROX_OP_DECRYPT) {
		sr->out.map_bufs_cnt--;
	}
}

static int
create_aead_outbuf(struct nitrox_softreq *sr, struct nitrox_sglist *digest)
{
	struct rte_crypto_op *op = sr->op;
	int cnt = 0;

	sr->resp.orh = PENDING_SIG;
	sr->out.sglist[cnt].len = sizeof(sr->resp.orh);
	sr->out.sglist[cnt].iova = sr->iova + offsetof(struct nitrox_softreq,
						       resp.orh);
	sr->out.sglist[cnt].virt = &sr->resp.orh;
	cnt++;
	sr->out.map_bufs_cnt = cnt;
	if (op->sym->m_dst) {
		int err;

		err = create_aead_oop_outbuf(sr, digest);
		if (unlikely(err))
			return err;
	} else {
		create_aead_inplace_outbuf(sr, digest);
	}

	cnt = sr->out.map_bufs_cnt;
	sr->resp.completion = PENDING_SIG;
	sr->out.sglist[cnt].len = sizeof(sr->resp.completion);
	sr->out.sglist[cnt].iova = sr->iova + offsetof(struct nitrox_softreq,
						     resp.completion);
	sr->out.sglist[cnt].virt = &sr->resp.completion;
	cnt++;
	RTE_VERIFY(cnt <= MAX_SGBUF_CNT);
	sr->out.map_bufs_cnt = cnt;

	create_sgcomp(&sr->out);
	sr->rptr = sr->iova + offsetof(struct nitrox_softreq, out.sgcomp);
	return 0;
}

static void
create_aead_gph(uint32_t cryptlen, uint16_t ivlen, uint32_t authlen,
		struct gphdr *gph)
{
	int auth_only_len;
	union {
		struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
			uint16_t iv_offset : 8;
			uint16_t auth_offset	: 8;
#else
			uint16_t auth_offset	: 8;
			uint16_t iv_offset : 8;
#endif
		};
		uint16_t value;
	} param3;

	gph->param0 = rte_cpu_to_be_16(cryptlen);
	gph->param1 = rte_cpu_to_be_16(authlen);

	auth_only_len = authlen - cryptlen;
	gph->param2 = rte_cpu_to_be_16(ivlen + auth_only_len);

	param3.iv_offset = 0;
	param3.auth_offset = ivlen;
	gph->param3 = rte_cpu_to_be_16(param3.value);
}

static int
process_cipher_auth_data(struct nitrox_softreq *sr)
{
	struct rte_crypto_op *op = sr->op;
	int err;
	struct nitrox_sglist digest;

	softreq_copy_iv(sr, 0);
	err = extract_cipher_auth_digest(sr, &digest);
	if (unlikely(err))
		return err;

	err = create_aead_inbuf(sr, &digest);
	if (unlikely(err))
		return err;

	err = create_aead_outbuf(sr, &digest);
	if (unlikely(err))
		return err;

	create_aead_gph(op->sym->cipher.data.length, sr->iv.len,
			op->sym->auth.data.length, &sr->gph);
	return 0;
}

static int
softreq_copy_salt(struct nitrox_softreq *sr)
{
	struct nitrox_crypto_ctx *ctx = sr->ctx;
	uint8_t *addr;

	if (unlikely(ctx->iv.length < AES_GCM_SALT_SIZE)) {
		NITROX_LOG(ERR, "Invalid IV length %d\n", ctx->iv.length);
		return -EINVAL;
	}

	addr = rte_crypto_op_ctod_offset(sr->op, uint8_t *, ctx->iv.offset);
	if (!memcmp(ctx->salt, addr, AES_GCM_SALT_SIZE))
		return 0;

	memcpy(ctx->salt, addr, AES_GCM_SALT_SIZE);
	memcpy(ctx->fctx.crypto.iv, addr, AES_GCM_SALT_SIZE);
	return 0;
}

static int
extract_combined_digest(struct nitrox_softreq *sr, struct nitrox_sglist *digest)
{
	struct rte_crypto_op *op = sr->op;
	struct rte_mbuf *mdst = op->sym->m_dst ? op->sym->m_dst :
		op->sym->m_src;

	digest->len = sr->ctx->digest_length;
	if (op->sym->aead.digest.data) {
		digest->iova = op->sym->aead.digest.phys_addr;
		digest->virt = op->sym->aead.digest.data;

		return 0;
	}

	if (unlikely(rte_pktmbuf_data_len(mdst) < op->sym->aead.data.offset +
	       op->sym->aead.data.length + digest->len))
		return -EINVAL;

	digest->iova = rte_pktmbuf_iova_offset(mdst,
					op->sym->aead.data.offset +
					op->sym->aead.data.length);
	digest->virt = rte_pktmbuf_mtod_offset(mdst, uint8_t *,
					op->sym->aead.data.offset +
					op->sym->aead.data.length);

	return 0;
}

static int
process_combined_data(struct nitrox_softreq *sr)
{
	int err;
	struct nitrox_sglist digest;
	struct rte_crypto_op *op = sr->op;

	err = softreq_copy_salt(sr);
	if (unlikely(err))
		return err;

	softreq_copy_iv(sr, AES_GCM_SALT_SIZE);
	err = extract_combined_digest(sr, &digest);
	if (unlikely(err))
		return err;

	err = create_aead_inbuf(sr, &digest);
	if (unlikely(err))
		return err;

	err = create_aead_outbuf(sr, &digest);
	if (unlikely(err))
		return err;

	create_aead_gph(op->sym->aead.data.length, sr->iv.len,
			op->sym->aead.data.length + sr->ctx->aad_length,
			&sr->gph);

	return 0;
}

static int
process_softreq(struct nitrox_softreq *sr)
{
	struct nitrox_crypto_ctx *ctx = sr->ctx;
	int err = 0;

	switch (ctx->nitrox_chain) {
	case NITROX_CHAIN_CIPHER_ONLY:
		err = process_cipher_data(sr);
		break;
	case NITROX_CHAIN_CIPHER_AUTH:
	case NITROX_CHAIN_AUTH_CIPHER:
		err = process_cipher_auth_data(sr);
		break;
	case NITROX_CHAIN_COMBINED:
		err = process_combined_data(sr);
		break;
	default:
		err = -EINVAL;
		break;
	}

	return err;
}

int
nitrox_process_se_req(uint16_t qno, struct rte_crypto_op *op,
		      struct nitrox_crypto_ctx *ctx,
		      struct nitrox_softreq *sr)
{
	int err;

	softreq_init(sr, sr->iova);
	sr->ctx = ctx;
	sr->op = op;
	err = process_softreq(sr);
	if (unlikely(err))
		return err;

	create_se_instr(sr, qno);
	sr->timeout = rte_get_timer_cycles() + CMD_TIMEOUT * rte_get_timer_hz();
	return 0;
}

int
nitrox_check_se_req(struct nitrox_softreq *sr, struct rte_crypto_op **op)
{
	uint64_t cc;
	uint64_t orh;
	int err;

	cc = *(volatile uint64_t *)(&sr->resp.completion);
	orh = *(volatile uint64_t *)(&sr->resp.orh);
	if (cc != PENDING_SIG)
		err = orh & 0xff;
	else if ((orh != PENDING_SIG) && (orh & 0xff))
		err = orh & 0xff;
	else if (rte_get_timer_cycles() >= sr->timeout)
		err = 0xff;
	else
		return -EAGAIN;

	if (unlikely(err))
		NITROX_LOG(ERR, "Request err 0x%x, orh 0x%"PRIx64"\n", err,
			   sr->resp.orh);

	*op = sr->op;
	return err;
}

void *
nitrox_sym_instr_addr(struct nitrox_softreq *sr)
{
	return &sr->instr;
}

static void
req_pool_obj_init(__rte_unused struct rte_mempool *mp,
		  __rte_unused void *opaque, void *obj,
		  __rte_unused unsigned int obj_idx)
{
	softreq_init(obj, rte_mempool_virt2iova(obj));
}

struct rte_mempool *
nitrox_sym_req_pool_create(struct rte_cryptodev *cdev, uint32_t nobjs,
			   uint16_t qp_id, int socket_id)
{
	char softreq_pool_name[RTE_RING_NAMESIZE];
	struct rte_mempool *mp;

	snprintf(softreq_pool_name, RTE_RING_NAMESIZE, "%s_sr_%d",
		 cdev->data->name, qp_id);
	mp = rte_mempool_create(softreq_pool_name,
				RTE_ALIGN_MUL_CEIL(nobjs, 64),
				sizeof(struct nitrox_softreq),
				64, 0, NULL, NULL, req_pool_obj_init, NULL,
				socket_id, 0);
	if (unlikely(!mp))
		NITROX_LOG(ERR, "Failed to create req pool, qid %d, err %d\n",
			   qp_id, rte_errno);

	return mp;
}

void
nitrox_sym_req_pool_free(struct rte_mempool *mp)
{
	rte_mempool_free(mp);
}
