/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2016-2021 NXP
 *
 */

#include <time.h>
#include <net/if.h>
#include <unistd.h>

#include <rte_ip.h>
#include <rte_mbuf.h>
#include <rte_cryptodev.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_kvargs.h>
#include <rte_dev.h>
#include <cryptodev_pmd.h>
#include <rte_common.h>
#include <rte_fslmc.h>
#include <fslmc_vfio.h>
#include <dpaa2_hw_pvt.h>
#include <dpaa2_hw_dpio.h>
#include <dpaa2_hw_mempool.h>
#include <fsl_dpopr.h>
#include <fsl_dpseci.h>
#include <fsl_mc_sys.h>

#include "dpaa2_sec_priv.h"
#include "dpaa2_sec_event.h"
#include "dpaa2_sec_logs.h"

/* RTA header files */
#include <desc/ipsec.h>
#include <desc/pdcp.h>
#include <desc/sdap.h>
#include <desc/algo.h>

/* Minimum job descriptor consists of a oneword job descriptor HEADER and
 * a pointer to the shared descriptor
 */
#define MIN_JOB_DESC_SIZE	(CAAM_CMD_SZ + CAAM_PTR_SZ)
#define FSL_VENDOR_ID           0x1957
#define FSL_DEVICE_ID           0x410
#define FSL_SUBSYSTEM_SEC       1
#define FSL_MC_DPSECI_DEVID     3

#define NO_PREFETCH 0
/* FLE_POOL_NUM_BUFS is set as per the ipsec-secgw application */
#define FLE_POOL_NUM_BUFS	32000
#define FLE_POOL_BUF_SIZE	256
#define FLE_POOL_CACHE_SIZE	512
#define FLE_SG_MEM_SIZE(num)	(FLE_POOL_BUF_SIZE + ((num) * 32))
#define SEC_FLC_DHR_OUTBOUND	-114
#define SEC_FLC_DHR_INBOUND	0

static uint8_t cryptodev_driver_id;

#ifdef RTE_LIB_SECURITY
static inline int
build_proto_compound_sg_fd(dpaa2_sec_session *sess,
			   struct rte_crypto_op *op,
			   struct qbman_fd *fd, uint16_t bpid)
{
	struct rte_crypto_sym_op *sym_op = op->sym;
	struct ctxt_priv *priv = sess->ctxt;
	struct qbman_fle *fle, *sge, *ip_fle, *op_fle;
	struct sec_flow_context *flc;
	struct rte_mbuf *mbuf;
	uint32_t in_len = 0, out_len = 0;

	if (sym_op->m_dst)
		mbuf = sym_op->m_dst;
	else
		mbuf = sym_op->m_src;

	/* first FLE entry used to store mbuf and session ctxt */
	fle = (struct qbman_fle *)rte_malloc(NULL,
			FLE_SG_MEM_SIZE(mbuf->nb_segs + sym_op->m_src->nb_segs),
			RTE_CACHE_LINE_SIZE);
	if (unlikely(!fle)) {
		DPAA2_SEC_DP_ERR("Proto:SG: Memory alloc failed for SGE");
		return -ENOMEM;
	}
	memset(fle, 0, FLE_SG_MEM_SIZE(mbuf->nb_segs + sym_op->m_src->nb_segs));
	DPAA2_SET_FLE_ADDR(fle, (size_t)op);
	DPAA2_FLE_SAVE_CTXT(fle, (ptrdiff_t)priv);

	/* Save the shared descriptor */
	flc = &priv->flc_desc[0].flc;

	op_fle = fle + 1;
	ip_fle = fle + 2;
	sge = fle + 3;

	if (likely(bpid < MAX_BPID)) {
		DPAA2_SET_FD_BPID(fd, bpid);
		DPAA2_SET_FLE_BPID(op_fle, bpid);
		DPAA2_SET_FLE_BPID(ip_fle, bpid);
	} else {
		DPAA2_SET_FD_IVP(fd);
		DPAA2_SET_FLE_IVP(op_fle);
		DPAA2_SET_FLE_IVP(ip_fle);
	}

	/* Configure FD as a FRAME LIST */
	DPAA2_SET_FD_ADDR(fd, DPAA2_VADDR_TO_IOVA(op_fle));
	DPAA2_SET_FD_COMPOUND_FMT(fd);
	DPAA2_SET_FD_FLC(fd, DPAA2_VADDR_TO_IOVA(flc));

	/* Configure Output FLE with Scatter/Gather Entry */
	DPAA2_SET_FLE_SG_EXT(op_fle);
	DPAA2_SET_FLE_ADDR(op_fle, DPAA2_VADDR_TO_IOVA(sge));

	/* Configure Output SGE for Encap/Decap */
	DPAA2_SET_FLE_ADDR(sge, DPAA2_MBUF_VADDR_TO_IOVA(mbuf));
	DPAA2_SET_FLE_OFFSET(sge, mbuf->data_off);
	/* o/p segs */
	while (mbuf->next) {
		sge->length = mbuf->data_len;
		out_len += sge->length;
		sge++;
		mbuf = mbuf->next;
		DPAA2_SET_FLE_ADDR(sge, DPAA2_MBUF_VADDR_TO_IOVA(mbuf));
		DPAA2_SET_FLE_OFFSET(sge, mbuf->data_off);
	}
	/* using buf_len for last buf - so that extra data can be added */
	sge->length = mbuf->buf_len - mbuf->data_off;
	out_len += sge->length;

	DPAA2_SET_FLE_FIN(sge);
	op_fle->length = out_len;

	sge++;
	mbuf = sym_op->m_src;

	/* Configure Input FLE with Scatter/Gather Entry */
	DPAA2_SET_FLE_ADDR(ip_fle, DPAA2_VADDR_TO_IOVA(sge));
	DPAA2_SET_FLE_SG_EXT(ip_fle);
	DPAA2_SET_FLE_FIN(ip_fle);

	/* Configure input SGE for Encap/Decap */
	DPAA2_SET_FLE_ADDR(sge, DPAA2_MBUF_VADDR_TO_IOVA(mbuf));
	DPAA2_SET_FLE_OFFSET(sge, mbuf->data_off);
	sge->length = mbuf->data_len;
	in_len += sge->length;

	mbuf = mbuf->next;
	/* i/p segs */
	while (mbuf) {
		sge++;
		DPAA2_SET_FLE_ADDR(sge, DPAA2_MBUF_VADDR_TO_IOVA(mbuf));
		DPAA2_SET_FLE_OFFSET(sge, mbuf->data_off);
		sge->length = mbuf->data_len;
		in_len += sge->length;
		mbuf = mbuf->next;
	}
	ip_fle->length = in_len;
	DPAA2_SET_FLE_FIN(sge);

	/* In case of PDCP, per packet HFN is stored in
	 * mbuf priv after sym_op.
	 */
	if (sess->ctxt_type == DPAA2_SEC_PDCP && sess->pdcp.hfn_ovd) {
		uint32_t hfn_ovd = *(uint32_t *)((uint8_t *)op +
					sess->pdcp.hfn_ovd_offset);
		/*enable HFN override override */
		DPAA2_SET_FLE_INTERNAL_JD(ip_fle, hfn_ovd);
		DPAA2_SET_FLE_INTERNAL_JD(op_fle, hfn_ovd);
		DPAA2_SET_FD_INTERNAL_JD(fd, hfn_ovd);
	}
	DPAA2_SET_FD_LEN(fd, ip_fle->length);

	return 0;
}

static inline int
build_proto_compound_fd(dpaa2_sec_session *sess,
	       struct rte_crypto_op *op,
	       struct qbman_fd *fd, uint16_t bpid)
{
	struct rte_crypto_sym_op *sym_op = op->sym;
	struct ctxt_priv *priv = sess->ctxt;
	struct qbman_fle *fle, *ip_fle, *op_fle;
	struct sec_flow_context *flc;
	struct rte_mbuf *src_mbuf = sym_op->m_src;
	struct rte_mbuf *dst_mbuf = sym_op->m_dst;
	int retval;

	if (!dst_mbuf)
		dst_mbuf = src_mbuf;

	/* Save the shared descriptor */
	flc = &priv->flc_desc[0].flc;

	/* we are using the first FLE entry to store Mbuf */
	retval = rte_mempool_get(priv->fle_pool, (void **)(&fle));
	if (retval) {
		DPAA2_SEC_DP_ERR("Memory alloc failed");
		return -ENOMEM;
	}
	memset(fle, 0, FLE_POOL_BUF_SIZE);
	DPAA2_SET_FLE_ADDR(fle, (size_t)op);
	DPAA2_FLE_SAVE_CTXT(fle, (ptrdiff_t)priv);

	op_fle = fle + 1;
	ip_fle = fle + 2;

	if (likely(bpid < MAX_BPID)) {
		DPAA2_SET_FD_BPID(fd, bpid);
		DPAA2_SET_FLE_BPID(op_fle, bpid);
		DPAA2_SET_FLE_BPID(ip_fle, bpid);
	} else {
		DPAA2_SET_FD_IVP(fd);
		DPAA2_SET_FLE_IVP(op_fle);
		DPAA2_SET_FLE_IVP(ip_fle);
	}

	/* Configure FD as a FRAME LIST */
	DPAA2_SET_FD_ADDR(fd, DPAA2_VADDR_TO_IOVA(op_fle));
	DPAA2_SET_FD_COMPOUND_FMT(fd);
	DPAA2_SET_FD_FLC(fd, DPAA2_VADDR_TO_IOVA(flc));

	/* Configure Output FLE with dst mbuf data  */
	DPAA2_SET_FLE_ADDR(op_fle, DPAA2_MBUF_VADDR_TO_IOVA(dst_mbuf));
	DPAA2_SET_FLE_OFFSET(op_fle, dst_mbuf->data_off);
	DPAA2_SET_FLE_LEN(op_fle, dst_mbuf->buf_len);

	/* Configure Input FLE with src mbuf data */
	DPAA2_SET_FLE_ADDR(ip_fle, DPAA2_MBUF_VADDR_TO_IOVA(src_mbuf));
	DPAA2_SET_FLE_OFFSET(ip_fle, src_mbuf->data_off);
	DPAA2_SET_FLE_LEN(ip_fle, src_mbuf->pkt_len);

	DPAA2_SET_FD_LEN(fd, ip_fle->length);
	DPAA2_SET_FLE_FIN(ip_fle);

	/* In case of PDCP, per packet HFN is stored in
	 * mbuf priv after sym_op.
	 */
	if (sess->ctxt_type == DPAA2_SEC_PDCP && sess->pdcp.hfn_ovd) {
		uint32_t hfn_ovd = *(uint32_t *)((uint8_t *)op +
					sess->pdcp.hfn_ovd_offset);
		/*enable HFN override override */
		DPAA2_SET_FLE_INTERNAL_JD(ip_fle, hfn_ovd);
		DPAA2_SET_FLE_INTERNAL_JD(op_fle, hfn_ovd);
		DPAA2_SET_FD_INTERNAL_JD(fd, hfn_ovd);
	}

	return 0;

}

static inline int
build_proto_fd(dpaa2_sec_session *sess,
	       struct rte_crypto_op *op,
	       struct qbman_fd *fd, uint16_t bpid)
{
	struct rte_crypto_sym_op *sym_op = op->sym;
	if (sym_op->m_dst)
		return build_proto_compound_fd(sess, op, fd, bpid);

	struct ctxt_priv *priv = sess->ctxt;
	struct sec_flow_context *flc;
	struct rte_mbuf *mbuf = sym_op->m_src;

	if (likely(bpid < MAX_BPID))
		DPAA2_SET_FD_BPID(fd, bpid);
	else
		DPAA2_SET_FD_IVP(fd);

	/* Save the shared descriptor */
	flc = &priv->flc_desc[0].flc;

	DPAA2_SET_FD_ADDR(fd, DPAA2_MBUF_VADDR_TO_IOVA(sym_op->m_src));
	DPAA2_SET_FD_OFFSET(fd, sym_op->m_src->data_off);
	DPAA2_SET_FD_LEN(fd, sym_op->m_src->pkt_len);
	DPAA2_SET_FD_FLC(fd, DPAA2_VADDR_TO_IOVA(flc));

	/* save physical address of mbuf */
	op->sym->aead.digest.phys_addr = mbuf->buf_iova;
	mbuf->buf_iova = (size_t)op;

	return 0;
}
#endif

static inline int
build_authenc_gcm_sg_fd(dpaa2_sec_session *sess,
		 struct rte_crypto_op *op,
		 struct qbman_fd *fd, __rte_unused uint16_t bpid)
{
	struct rte_crypto_sym_op *sym_op = op->sym;
	struct ctxt_priv *priv = sess->ctxt;
	struct qbman_fle *fle, *sge, *ip_fle, *op_fle;
	struct sec_flow_context *flc;
	uint32_t auth_only_len = sess->ext_params.aead_ctxt.auth_only_len;
	int icv_len = sess->digest_length;
	uint8_t *old_icv;
	struct rte_mbuf *mbuf;
	uint8_t *IV_ptr = rte_crypto_op_ctod_offset(op, uint8_t *,
			sess->iv.offset);

	if (sym_op->m_dst)
		mbuf = sym_op->m_dst;
	else
		mbuf = sym_op->m_src;

	/* first FLE entry used to store mbuf and session ctxt */
	fle = (struct qbman_fle *)rte_malloc(NULL,
			FLE_SG_MEM_SIZE(mbuf->nb_segs + sym_op->m_src->nb_segs),
			RTE_CACHE_LINE_SIZE);
	if (unlikely(!fle)) {
		DPAA2_SEC_ERR("GCM SG: Memory alloc failed for SGE");
		return -ENOMEM;
	}
	memset(fle, 0, FLE_SG_MEM_SIZE(mbuf->nb_segs + sym_op->m_src->nb_segs));
	DPAA2_SET_FLE_ADDR(fle, (size_t)op);
	DPAA2_FLE_SAVE_CTXT(fle, (size_t)priv);

	op_fle = fle + 1;
	ip_fle = fle + 2;
	sge = fle + 3;

	/* Save the shared descriptor */
	flc = &priv->flc_desc[0].flc;

	/* Configure FD as a FRAME LIST */
	DPAA2_SET_FD_ADDR(fd, DPAA2_VADDR_TO_IOVA(op_fle));
	DPAA2_SET_FD_COMPOUND_FMT(fd);
	DPAA2_SET_FD_FLC(fd, DPAA2_VADDR_TO_IOVA(flc));

	DPAA2_SEC_DP_DEBUG("GCM SG: auth_off: 0x%x/length %d, digest-len=%d\n"
		   "iv-len=%d data_off: 0x%x\n",
		   sym_op->aead.data.offset,
		   sym_op->aead.data.length,
		   sess->digest_length,
		   sess->iv.length,
		   sym_op->m_src->data_off);

	/* Configure Output FLE with Scatter/Gather Entry */
	DPAA2_SET_FLE_SG_EXT(op_fle);
	DPAA2_SET_FLE_ADDR(op_fle, DPAA2_VADDR_TO_IOVA(sge));

	if (auth_only_len)
		DPAA2_SET_FLE_INTERNAL_JD(op_fle, auth_only_len);

	op_fle->length = (sess->dir == DIR_ENC) ?
			(sym_op->aead.data.length + icv_len) :
			sym_op->aead.data.length;

	/* Configure Output SGE for Encap/Decap */
	DPAA2_SET_FLE_ADDR(sge, DPAA2_MBUF_VADDR_TO_IOVA(mbuf));
	DPAA2_SET_FLE_OFFSET(sge, mbuf->data_off + sym_op->aead.data.offset);
	sge->length = mbuf->data_len - sym_op->aead.data.offset;

	mbuf = mbuf->next;
	/* o/p segs */
	while (mbuf) {
		sge++;
		DPAA2_SET_FLE_ADDR(sge, DPAA2_MBUF_VADDR_TO_IOVA(mbuf));
		DPAA2_SET_FLE_OFFSET(sge, mbuf->data_off);
		sge->length = mbuf->data_len;
		mbuf = mbuf->next;
	}
	sge->length -= icv_len;

	if (sess->dir == DIR_ENC) {
		sge++;
		DPAA2_SET_FLE_ADDR(sge,
				DPAA2_VADDR_TO_IOVA(sym_op->aead.digest.data));
		sge->length = icv_len;
	}
	DPAA2_SET_FLE_FIN(sge);

	sge++;
	mbuf = sym_op->m_src;

	/* Configure Input FLE with Scatter/Gather Entry */
	DPAA2_SET_FLE_ADDR(ip_fle, DPAA2_VADDR_TO_IOVA(sge));
	DPAA2_SET_FLE_SG_EXT(ip_fle);
	DPAA2_SET_FLE_FIN(ip_fle);
	ip_fle->length = (sess->dir == DIR_ENC) ?
		(sym_op->aead.data.length + sess->iv.length + auth_only_len) :
		(sym_op->aead.data.length + sess->iv.length + auth_only_len +
		 icv_len);

	/* Configure Input SGE for Encap/Decap */
	DPAA2_SET_FLE_ADDR(sge, DPAA2_VADDR_TO_IOVA(IV_ptr));
	sge->length = sess->iv.length;

	sge++;
	if (auth_only_len) {
		DPAA2_SET_FLE_ADDR(sge,
				DPAA2_VADDR_TO_IOVA(sym_op->aead.aad.data));
		sge->length = auth_only_len;
		sge++;
	}

	DPAA2_SET_FLE_ADDR(sge, DPAA2_MBUF_VADDR_TO_IOVA(mbuf));
	DPAA2_SET_FLE_OFFSET(sge, sym_op->aead.data.offset +
				mbuf->data_off);
	sge->length = mbuf->data_len - sym_op->aead.data.offset;

	mbuf = mbuf->next;
	/* i/p segs */
	while (mbuf) {
		sge++;
		DPAA2_SET_FLE_ADDR(sge, DPAA2_MBUF_VADDR_TO_IOVA(mbuf));
		DPAA2_SET_FLE_OFFSET(sge, mbuf->data_off);
		sge->length = mbuf->data_len;
		mbuf = mbuf->next;
	}

	if (sess->dir == DIR_DEC) {
		sge++;
		old_icv = (uint8_t *)(sge + 1);
		memcpy(old_icv,	sym_op->aead.digest.data, icv_len);
		DPAA2_SET_FLE_ADDR(sge, DPAA2_VADDR_TO_IOVA(old_icv));
		sge->length = icv_len;
	}

	DPAA2_SET_FLE_FIN(sge);
	if (auth_only_len) {
		DPAA2_SET_FLE_INTERNAL_JD(ip_fle, auth_only_len);
		DPAA2_SET_FD_INTERNAL_JD(fd, auth_only_len);
	}
	DPAA2_SET_FD_LEN(fd, ip_fle->length);

	return 0;
}

static inline int
build_authenc_gcm_fd(dpaa2_sec_session *sess,
		     struct rte_crypto_op *op,
		     struct qbman_fd *fd, uint16_t bpid)
{
	struct rte_crypto_sym_op *sym_op = op->sym;
	struct ctxt_priv *priv = sess->ctxt;
	struct qbman_fle *fle, *sge;
	struct sec_flow_context *flc;
	uint32_t auth_only_len = sess->ext_params.aead_ctxt.auth_only_len;
	int icv_len = sess->digest_length, retval;
	uint8_t *old_icv;
	struct rte_mbuf *dst;
	uint8_t *IV_ptr = rte_crypto_op_ctod_offset(op, uint8_t *,
			sess->iv.offset);

	if (sym_op->m_dst)
		dst = sym_op->m_dst;
	else
		dst = sym_op->m_src;

	/* TODO we are using the first FLE entry to store Mbuf and session ctxt.
	 * Currently we donot know which FLE has the mbuf stored.
	 * So while retreiving we can go back 1 FLE from the FD -ADDR
	 * to get the MBUF Addr from the previous FLE.
	 * We can have a better approach to use the inline Mbuf
	 */
	retval = rte_mempool_get(priv->fle_pool, (void **)(&fle));
	if (retval) {
		DPAA2_SEC_ERR("GCM: Memory alloc failed for SGE");
		return -ENOMEM;
	}
	memset(fle, 0, FLE_POOL_BUF_SIZE);
	DPAA2_SET_FLE_ADDR(fle, (size_t)op);
	DPAA2_FLE_SAVE_CTXT(fle, (ptrdiff_t)priv);
	fle = fle + 1;
	sge = fle + 2;
	if (likely(bpid < MAX_BPID)) {
		DPAA2_SET_FD_BPID(fd, bpid);
		DPAA2_SET_FLE_BPID(fle, bpid);
		DPAA2_SET_FLE_BPID(fle + 1, bpid);
		DPAA2_SET_FLE_BPID(sge, bpid);
		DPAA2_SET_FLE_BPID(sge + 1, bpid);
		DPAA2_SET_FLE_BPID(sge + 2, bpid);
		DPAA2_SET_FLE_BPID(sge + 3, bpid);
	} else {
		DPAA2_SET_FD_IVP(fd);
		DPAA2_SET_FLE_IVP(fle);
		DPAA2_SET_FLE_IVP((fle + 1));
		DPAA2_SET_FLE_IVP(sge);
		DPAA2_SET_FLE_IVP((sge + 1));
		DPAA2_SET_FLE_IVP((sge + 2));
		DPAA2_SET_FLE_IVP((sge + 3));
	}

	/* Save the shared descriptor */
	flc = &priv->flc_desc[0].flc;
	/* Configure FD as a FRAME LIST */
	DPAA2_SET_FD_ADDR(fd, DPAA2_VADDR_TO_IOVA(fle));
	DPAA2_SET_FD_COMPOUND_FMT(fd);
	DPAA2_SET_FD_FLC(fd, DPAA2_VADDR_TO_IOVA(flc));

	DPAA2_SEC_DP_DEBUG("GCM: auth_off: 0x%x/length %d, digest-len=%d\n"
		   "iv-len=%d data_off: 0x%x\n",
		   sym_op->aead.data.offset,
		   sym_op->aead.data.length,
		   sess->digest_length,
		   sess->iv.length,
		   sym_op->m_src->data_off);

	/* Configure Output FLE with Scatter/Gather Entry */
	DPAA2_SET_FLE_ADDR(fle, DPAA2_VADDR_TO_IOVA(sge));
	if (auth_only_len)
		DPAA2_SET_FLE_INTERNAL_JD(fle, auth_only_len);
	fle->length = (sess->dir == DIR_ENC) ?
			(sym_op->aead.data.length + icv_len) :
			sym_op->aead.data.length;

	DPAA2_SET_FLE_SG_EXT(fle);

	/* Configure Output SGE for Encap/Decap */
	DPAA2_SET_FLE_ADDR(sge, DPAA2_MBUF_VADDR_TO_IOVA(dst));
	DPAA2_SET_FLE_OFFSET(sge, dst->data_off + sym_op->aead.data.offset);
	sge->length = sym_op->aead.data.length;

	if (sess->dir == DIR_ENC) {
		sge++;
		DPAA2_SET_FLE_ADDR(sge,
				DPAA2_VADDR_TO_IOVA(sym_op->aead.digest.data));
		sge->length = sess->digest_length;
	}
	DPAA2_SET_FLE_FIN(sge);

	sge++;
	fle++;

	/* Configure Input FLE with Scatter/Gather Entry */
	DPAA2_SET_FLE_ADDR(fle, DPAA2_VADDR_TO_IOVA(sge));
	DPAA2_SET_FLE_SG_EXT(fle);
	DPAA2_SET_FLE_FIN(fle);
	fle->length = (sess->dir == DIR_ENC) ?
		(sym_op->aead.data.length + sess->iv.length + auth_only_len) :
		(sym_op->aead.data.length + sess->iv.length + auth_only_len +
		 sess->digest_length);

	/* Configure Input SGE for Encap/Decap */
	DPAA2_SET_FLE_ADDR(sge, DPAA2_VADDR_TO_IOVA(IV_ptr));
	sge->length = sess->iv.length;
	sge++;
	if (auth_only_len) {
		DPAA2_SET_FLE_ADDR(sge,
				DPAA2_VADDR_TO_IOVA(sym_op->aead.aad.data));
		sge->length = auth_only_len;
		DPAA2_SET_FLE_BPID(sge, bpid);
		sge++;
	}

	DPAA2_SET_FLE_ADDR(sge, DPAA2_MBUF_VADDR_TO_IOVA(sym_op->m_src));
	DPAA2_SET_FLE_OFFSET(sge, sym_op->aead.data.offset +
				sym_op->m_src->data_off);
	sge->length = sym_op->aead.data.length;
	if (sess->dir == DIR_DEC) {
		sge++;
		old_icv = (uint8_t *)(sge + 1);
		memcpy(old_icv,	sym_op->aead.digest.data,
		       sess->digest_length);
		DPAA2_SET_FLE_ADDR(sge, DPAA2_VADDR_TO_IOVA(old_icv));
		sge->length = sess->digest_length;
	}
	DPAA2_SET_FLE_FIN(sge);

	if (auth_only_len) {
		DPAA2_SET_FLE_INTERNAL_JD(fle, auth_only_len);
		DPAA2_SET_FD_INTERNAL_JD(fd, auth_only_len);
	}

	DPAA2_SET_FD_LEN(fd, fle->length);
	return 0;
}

static inline int
build_authenc_sg_fd(dpaa2_sec_session *sess,
		 struct rte_crypto_op *op,
		 struct qbman_fd *fd, __rte_unused uint16_t bpid)
{
	struct rte_crypto_sym_op *sym_op = op->sym;
	struct ctxt_priv *priv = sess->ctxt;
	struct qbman_fle *fle, *sge, *ip_fle, *op_fle;
	struct sec_flow_context *flc;
	uint16_t auth_hdr_len = sym_op->cipher.data.offset -
				sym_op->auth.data.offset;
	uint16_t auth_tail_len = sym_op->auth.data.length -
				sym_op->cipher.data.length - auth_hdr_len;
	uint32_t auth_only_len = (auth_tail_len << 16) | auth_hdr_len;
	int icv_len = sess->digest_length;
	uint8_t *old_icv;
	struct rte_mbuf *mbuf;
	uint8_t *iv_ptr = rte_crypto_op_ctod_offset(op, uint8_t *,
			sess->iv.offset);

	if (sym_op->m_dst)
		mbuf = sym_op->m_dst;
	else
		mbuf = sym_op->m_src;

	/* first FLE entry used to store mbuf and session ctxt */
	fle = (struct qbman_fle *)rte_malloc(NULL,
			FLE_SG_MEM_SIZE(mbuf->nb_segs + sym_op->m_src->nb_segs),
			RTE_CACHE_LINE_SIZE);
	if (unlikely(!fle)) {
		DPAA2_SEC_ERR("AUTHENC SG: Memory alloc failed for SGE");
		return -ENOMEM;
	}
	memset(fle, 0, FLE_SG_MEM_SIZE(mbuf->nb_segs + sym_op->m_src->nb_segs));
	DPAA2_SET_FLE_ADDR(fle, (size_t)op);
	DPAA2_FLE_SAVE_CTXT(fle, (ptrdiff_t)priv);

	op_fle = fle + 1;
	ip_fle = fle + 2;
	sge = fle + 3;

	/* Save the shared descriptor */
	flc = &priv->flc_desc[0].flc;

	/* Configure FD as a FRAME LIST */
	DPAA2_SET_FD_ADDR(fd, DPAA2_VADDR_TO_IOVA(op_fle));
	DPAA2_SET_FD_COMPOUND_FMT(fd);
	DPAA2_SET_FD_FLC(fd, DPAA2_VADDR_TO_IOVA(flc));

	DPAA2_SEC_DP_DEBUG(
		"AUTHENC SG: auth_off: 0x%x/length %d, digest-len=%d\n"
		"cipher_off: 0x%x/length %d, iv-len=%d data_off: 0x%x\n",
		sym_op->auth.data.offset,
		sym_op->auth.data.length,
		sess->digest_length,
		sym_op->cipher.data.offset,
		sym_op->cipher.data.length,
		sess->iv.length,
		sym_op->m_src->data_off);

	/* Configure Output FLE with Scatter/Gather Entry */
	DPAA2_SET_FLE_SG_EXT(op_fle);
	DPAA2_SET_FLE_ADDR(op_fle, DPAA2_VADDR_TO_IOVA(sge));

	if (auth_only_len)
		DPAA2_SET_FLE_INTERNAL_JD(op_fle, auth_only_len);

	op_fle->length = (sess->dir == DIR_ENC) ?
			(sym_op->cipher.data.length + icv_len) :
			sym_op->cipher.data.length;

	/* Configure Output SGE for Encap/Decap */
	DPAA2_SET_FLE_ADDR(sge, DPAA2_MBUF_VADDR_TO_IOVA(mbuf));
	DPAA2_SET_FLE_OFFSET(sge, mbuf->data_off + sym_op->auth.data.offset);
	sge->length = mbuf->data_len - sym_op->auth.data.offset;

	mbuf = mbuf->next;
	/* o/p segs */
	while (mbuf) {
		sge++;
		DPAA2_SET_FLE_ADDR(sge, DPAA2_MBUF_VADDR_TO_IOVA(mbuf));
		DPAA2_SET_FLE_OFFSET(sge, mbuf->data_off);
		sge->length = mbuf->data_len;
		mbuf = mbuf->next;
	}
	sge->length -= icv_len;

	if (sess->dir == DIR_ENC) {
		sge++;
		DPAA2_SET_FLE_ADDR(sge,
				DPAA2_VADDR_TO_IOVA(sym_op->auth.digest.data));
		sge->length = icv_len;
	}
	DPAA2_SET_FLE_FIN(sge);

	sge++;
	mbuf = sym_op->m_src;

	/* Configure Input FLE with Scatter/Gather Entry */
	DPAA2_SET_FLE_ADDR(ip_fle, DPAA2_VADDR_TO_IOVA(sge));
	DPAA2_SET_FLE_SG_EXT(ip_fle);
	DPAA2_SET_FLE_FIN(ip_fle);
	ip_fle->length = (sess->dir == DIR_ENC) ?
			(sym_op->auth.data.length + sess->iv.length) :
			(sym_op->auth.data.length + sess->iv.length +
			 icv_len);

	/* Configure Input SGE for Encap/Decap */
	DPAA2_SET_FLE_ADDR(sge, DPAA2_VADDR_TO_IOVA(iv_ptr));
	sge->length = sess->iv.length;

	sge++;
	DPAA2_SET_FLE_ADDR(sge, DPAA2_MBUF_VADDR_TO_IOVA(mbuf));
	DPAA2_SET_FLE_OFFSET(sge, sym_op->auth.data.offset +
				mbuf->data_off);
	sge->length = mbuf->data_len - sym_op->auth.data.offset;

	mbuf = mbuf->next;
	/* i/p segs */
	while (mbuf) {
		sge++;
		DPAA2_SET_FLE_ADDR(sge, DPAA2_MBUF_VADDR_TO_IOVA(mbuf));
		DPAA2_SET_FLE_OFFSET(sge, mbuf->data_off);
		sge->length = mbuf->data_len;
		mbuf = mbuf->next;
	}
	sge->length -= icv_len;

	if (sess->dir == DIR_DEC) {
		sge++;
		old_icv = (uint8_t *)(sge + 1);
		memcpy(old_icv,	sym_op->auth.digest.data,
		       icv_len);
		DPAA2_SET_FLE_ADDR(sge, DPAA2_VADDR_TO_IOVA(old_icv));
		sge->length = icv_len;
	}

	DPAA2_SET_FLE_FIN(sge);
	if (auth_only_len) {
		DPAA2_SET_FLE_INTERNAL_JD(ip_fle, auth_only_len);
		DPAA2_SET_FD_INTERNAL_JD(fd, auth_only_len);
	}
	DPAA2_SET_FD_LEN(fd, ip_fle->length);

	return 0;
}

static inline int
build_authenc_fd(dpaa2_sec_session *sess,
		 struct rte_crypto_op *op,
		 struct qbman_fd *fd, uint16_t bpid)
{
	struct rte_crypto_sym_op *sym_op = op->sym;
	struct ctxt_priv *priv = sess->ctxt;
	struct qbman_fle *fle, *sge;
	struct sec_flow_context *flc;
	uint16_t auth_hdr_len = sym_op->cipher.data.offset -
				sym_op->auth.data.offset;
	uint16_t auth_tail_len = sym_op->auth.data.length -
				sym_op->cipher.data.length - auth_hdr_len;
	uint32_t auth_only_len = (auth_tail_len << 16) | auth_hdr_len;

	int icv_len = sess->digest_length, retval;
	uint8_t *old_icv;
	uint8_t *iv_ptr = rte_crypto_op_ctod_offset(op, uint8_t *,
			sess->iv.offset);
	struct rte_mbuf *dst;

	if (sym_op->m_dst)
		dst = sym_op->m_dst;
	else
		dst = sym_op->m_src;

	/* we are using the first FLE entry to store Mbuf.
	 * Currently we donot know which FLE has the mbuf stored.
	 * So while retreiving we can go back 1 FLE from the FD -ADDR
	 * to get the MBUF Addr from the previous FLE.
	 * We can have a better approach to use the inline Mbuf
	 */
	retval = rte_mempool_get(priv->fle_pool, (void **)(&fle));
	if (retval) {
		DPAA2_SEC_ERR("Memory alloc failed for SGE");
		return -ENOMEM;
	}
	memset(fle, 0, FLE_POOL_BUF_SIZE);
	DPAA2_SET_FLE_ADDR(fle, (size_t)op);
	DPAA2_FLE_SAVE_CTXT(fle, (ptrdiff_t)priv);
	fle = fle + 1;
	sge = fle + 2;
	if (likely(bpid < MAX_BPID)) {
		DPAA2_SET_FD_BPID(fd, bpid);
		DPAA2_SET_FLE_BPID(fle, bpid);
		DPAA2_SET_FLE_BPID(fle + 1, bpid);
		DPAA2_SET_FLE_BPID(sge, bpid);
		DPAA2_SET_FLE_BPID(sge + 1, bpid);
		DPAA2_SET_FLE_BPID(sge + 2, bpid);
		DPAA2_SET_FLE_BPID(sge + 3, bpid);
	} else {
		DPAA2_SET_FD_IVP(fd);
		DPAA2_SET_FLE_IVP(fle);
		DPAA2_SET_FLE_IVP((fle + 1));
		DPAA2_SET_FLE_IVP(sge);
		DPAA2_SET_FLE_IVP((sge + 1));
		DPAA2_SET_FLE_IVP((sge + 2));
		DPAA2_SET_FLE_IVP((sge + 3));
	}

	/* Save the shared descriptor */
	flc = &priv->flc_desc[0].flc;
	/* Configure FD as a FRAME LIST */
	DPAA2_SET_FD_ADDR(fd, DPAA2_VADDR_TO_IOVA(fle));
	DPAA2_SET_FD_COMPOUND_FMT(fd);
	DPAA2_SET_FD_FLC(fd, DPAA2_VADDR_TO_IOVA(flc));

	DPAA2_SEC_DP_DEBUG(
		"AUTHENC: auth_off: 0x%x/length %d, digest-len=%d\n"
		"cipher_off: 0x%x/length %d, iv-len=%d data_off: 0x%x\n",
		sym_op->auth.data.offset,
		sym_op->auth.data.length,
		sess->digest_length,
		sym_op->cipher.data.offset,
		sym_op->cipher.data.length,
		sess->iv.length,
		sym_op->m_src->data_off);

	/* Configure Output FLE with Scatter/Gather Entry */
	DPAA2_SET_FLE_ADDR(fle, DPAA2_VADDR_TO_IOVA(sge));
	if (auth_only_len)
		DPAA2_SET_FLE_INTERNAL_JD(fle, auth_only_len);
	fle->length = (sess->dir == DIR_ENC) ?
			(sym_op->cipher.data.length + icv_len) :
			sym_op->cipher.data.length;

	DPAA2_SET_FLE_SG_EXT(fle);

	/* Configure Output SGE for Encap/Decap */
	DPAA2_SET_FLE_ADDR(sge, DPAA2_MBUF_VADDR_TO_IOVA(dst));
	DPAA2_SET_FLE_OFFSET(sge, sym_op->cipher.data.offset +
				dst->data_off);
	sge->length = sym_op->cipher.data.length;

	if (sess->dir == DIR_ENC) {
		sge++;
		DPAA2_SET_FLE_ADDR(sge,
				DPAA2_VADDR_TO_IOVA(sym_op->auth.digest.data));
		sge->length = sess->digest_length;
		DPAA2_SET_FD_LEN(fd, (sym_op->auth.data.length +
					sess->iv.length));
	}
	DPAA2_SET_FLE_FIN(sge);

	sge++;
	fle++;

	/* Configure Input FLE with Scatter/Gather Entry */
	DPAA2_SET_FLE_ADDR(fle, DPAA2_VADDR_TO_IOVA(sge));
	DPAA2_SET_FLE_SG_EXT(fle);
	DPAA2_SET_FLE_FIN(fle);
	fle->length = (sess->dir == DIR_ENC) ?
			(sym_op->auth.data.length + sess->iv.length) :
			(sym_op->auth.data.length + sess->iv.length +
			 sess->digest_length);

	/* Configure Input SGE for Encap/Decap */
	DPAA2_SET_FLE_ADDR(sge, DPAA2_VADDR_TO_IOVA(iv_ptr));
	sge->length = sess->iv.length;
	sge++;

	DPAA2_SET_FLE_ADDR(sge, DPAA2_MBUF_VADDR_TO_IOVA(sym_op->m_src));
	DPAA2_SET_FLE_OFFSET(sge, sym_op->auth.data.offset +
				sym_op->m_src->data_off);
	sge->length = sym_op->auth.data.length;
	if (sess->dir == DIR_DEC) {
		sge++;
		old_icv = (uint8_t *)(sge + 1);
		memcpy(old_icv,	sym_op->auth.digest.data,
		       sess->digest_length);
		DPAA2_SET_FLE_ADDR(sge, DPAA2_VADDR_TO_IOVA(old_icv));
		sge->length = sess->digest_length;
		DPAA2_SET_FD_LEN(fd, (sym_op->auth.data.length +
				 sess->digest_length +
				 sess->iv.length));
	}
	DPAA2_SET_FLE_FIN(sge);
	if (auth_only_len) {
		DPAA2_SET_FLE_INTERNAL_JD(fle, auth_only_len);
		DPAA2_SET_FD_INTERNAL_JD(fd, auth_only_len);
	}
	return 0;
}

static inline int build_auth_sg_fd(
		dpaa2_sec_session *sess,
		struct rte_crypto_op *op,
		struct qbman_fd *fd,
		__rte_unused uint16_t bpid)
{
	struct rte_crypto_sym_op *sym_op = op->sym;
	struct qbman_fle *fle, *sge, *ip_fle, *op_fle;
	struct sec_flow_context *flc;
	struct ctxt_priv *priv = sess->ctxt;
	int data_len, data_offset;
	uint8_t *old_digest;
	struct rte_mbuf *mbuf;

	data_len = sym_op->auth.data.length;
	data_offset = sym_op->auth.data.offset;

	if (sess->auth_alg == RTE_CRYPTO_AUTH_SNOW3G_UIA2 ||
	    sess->auth_alg == RTE_CRYPTO_AUTH_ZUC_EIA3) {
		if ((data_len & 7) || (data_offset & 7)) {
			DPAA2_SEC_ERR("AUTH: len/offset must be full bytes");
			return -ENOTSUP;
		}

		data_len = data_len >> 3;
		data_offset = data_offset >> 3;
	}

	mbuf = sym_op->m_src;
	fle = (struct qbman_fle *)rte_malloc(NULL,
			FLE_SG_MEM_SIZE(mbuf->nb_segs),
			RTE_CACHE_LINE_SIZE);
	if (unlikely(!fle)) {
		DPAA2_SEC_ERR("AUTH SG: Memory alloc failed for SGE");
		return -ENOMEM;
	}
	memset(fle, 0, FLE_SG_MEM_SIZE(mbuf->nb_segs));
	/* first FLE entry used to store mbuf and session ctxt */
	DPAA2_SET_FLE_ADDR(fle, (size_t)op);
	DPAA2_FLE_SAVE_CTXT(fle, (ptrdiff_t)priv);
	op_fle = fle + 1;
	ip_fle = fle + 2;
	sge = fle + 3;

	flc = &priv->flc_desc[DESC_INITFINAL].flc;
	/* sg FD */
	DPAA2_SET_FD_FLC(fd, DPAA2_VADDR_TO_IOVA(flc));
	DPAA2_SET_FD_ADDR(fd, DPAA2_VADDR_TO_IOVA(op_fle));
	DPAA2_SET_FD_COMPOUND_FMT(fd);

	/* o/p fle */
	DPAA2_SET_FLE_ADDR(op_fle,
				DPAA2_VADDR_TO_IOVA(sym_op->auth.digest.data));
	op_fle->length = sess->digest_length;

	/* i/p fle */
	DPAA2_SET_FLE_SG_EXT(ip_fle);
	DPAA2_SET_FLE_ADDR(ip_fle, DPAA2_VADDR_TO_IOVA(sge));
	ip_fle->length = data_len;

	if (sess->iv.length) {
		uint8_t *iv_ptr;

		iv_ptr = rte_crypto_op_ctod_offset(op, uint8_t *,
						   sess->iv.offset);

		if (sess->auth_alg == RTE_CRYPTO_AUTH_SNOW3G_UIA2) {
			iv_ptr = conv_to_snow_f9_iv(iv_ptr);
			sge->length = 12;
		} else if (sess->auth_alg == RTE_CRYPTO_AUTH_ZUC_EIA3) {
			iv_ptr = conv_to_zuc_eia_iv(iv_ptr);
			sge->length = 8;
		} else {
			sge->length = sess->iv.length;
		}
		DPAA2_SET_FLE_ADDR(sge, DPAA2_VADDR_TO_IOVA(iv_ptr));
		ip_fle->length += sge->length;
		sge++;
	}
	/* i/p 1st seg */
	DPAA2_SET_FLE_ADDR(sge, DPAA2_MBUF_VADDR_TO_IOVA(mbuf));
	DPAA2_SET_FLE_OFFSET(sge, data_offset + mbuf->data_off);

	if (data_len <= (mbuf->data_len - data_offset)) {
		sge->length = data_len;
		data_len = 0;
	} else {
		sge->length = mbuf->data_len - data_offset;

		/* remaining i/p segs */
		while ((data_len = data_len - sge->length) &&
		       (mbuf = mbuf->next)) {
			sge++;
			DPAA2_SET_FLE_ADDR(sge, DPAA2_MBUF_VADDR_TO_IOVA(mbuf));
			DPAA2_SET_FLE_OFFSET(sge, mbuf->data_off);
			if (data_len > mbuf->data_len)
				sge->length = mbuf->data_len;
			else
				sge->length = data_len;
		}
	}

	if (sess->dir == DIR_DEC) {
		/* Digest verification case */
		sge++;
		old_digest = (uint8_t *)(sge + 1);
		rte_memcpy(old_digest, sym_op->auth.digest.data,
			   sess->digest_length);
		DPAA2_SET_FLE_ADDR(sge, DPAA2_VADDR_TO_IOVA(old_digest));
		sge->length = sess->digest_length;
		ip_fle->length += sess->digest_length;
	}
	DPAA2_SET_FLE_FIN(sge);
	DPAA2_SET_FLE_FIN(ip_fle);
	DPAA2_SET_FD_LEN(fd, ip_fle->length);

	return 0;
}

static inline int
build_auth_fd(dpaa2_sec_session *sess, struct rte_crypto_op *op,
	      struct qbman_fd *fd, uint16_t bpid)
{
	struct rte_crypto_sym_op *sym_op = op->sym;
	struct qbman_fle *fle, *sge;
	struct sec_flow_context *flc;
	struct ctxt_priv *priv = sess->ctxt;
	int data_len, data_offset;
	uint8_t *old_digest;
	int retval;

	data_len = sym_op->auth.data.length;
	data_offset = sym_op->auth.data.offset;

	if (sess->auth_alg == RTE_CRYPTO_AUTH_SNOW3G_UIA2 ||
	    sess->auth_alg == RTE_CRYPTO_AUTH_ZUC_EIA3) {
		if ((data_len & 7) || (data_offset & 7)) {
			DPAA2_SEC_ERR("AUTH: len/offset must be full bytes");
			return -ENOTSUP;
		}

		data_len = data_len >> 3;
		data_offset = data_offset >> 3;
	}

	retval = rte_mempool_get(priv->fle_pool, (void **)(&fle));
	if (retval) {
		DPAA2_SEC_ERR("AUTH Memory alloc failed for SGE");
		return -ENOMEM;
	}
	memset(fle, 0, FLE_POOL_BUF_SIZE);
	/* TODO we are using the first FLE entry to store Mbuf.
	 * Currently we donot know which FLE has the mbuf stored.
	 * So while retreiving we can go back 1 FLE from the FD -ADDR
	 * to get the MBUF Addr from the previous FLE.
	 * We can have a better approach to use the inline Mbuf
	 */
	DPAA2_SET_FLE_ADDR(fle, (size_t)op);
	DPAA2_FLE_SAVE_CTXT(fle, (ptrdiff_t)priv);
	fle = fle + 1;
	sge = fle + 2;

	if (likely(bpid < MAX_BPID)) {
		DPAA2_SET_FD_BPID(fd, bpid);
		DPAA2_SET_FLE_BPID(fle, bpid);
		DPAA2_SET_FLE_BPID(fle + 1, bpid);
		DPAA2_SET_FLE_BPID(sge, bpid);
		DPAA2_SET_FLE_BPID(sge + 1, bpid);
	} else {
		DPAA2_SET_FD_IVP(fd);
		DPAA2_SET_FLE_IVP(fle);
		DPAA2_SET_FLE_IVP((fle + 1));
		DPAA2_SET_FLE_IVP(sge);
		DPAA2_SET_FLE_IVP((sge + 1));
	}

	flc = &priv->flc_desc[DESC_INITFINAL].flc;
	DPAA2_SET_FD_FLC(fd, DPAA2_VADDR_TO_IOVA(flc));
	DPAA2_SET_FD_ADDR(fd, DPAA2_VADDR_TO_IOVA(fle));
	DPAA2_SET_FD_COMPOUND_FMT(fd);

	DPAA2_SET_FLE_ADDR(fle, DPAA2_VADDR_TO_IOVA(sym_op->auth.digest.data));
	fle->length = sess->digest_length;
	fle++;

	/* Setting input FLE */
	DPAA2_SET_FLE_ADDR(fle, DPAA2_VADDR_TO_IOVA(sge));
	DPAA2_SET_FLE_SG_EXT(fle);
	fle->length = data_len;

	if (sess->iv.length) {
		uint8_t *iv_ptr;

		iv_ptr = rte_crypto_op_ctod_offset(op, uint8_t *,
						   sess->iv.offset);

		if (sess->auth_alg == RTE_CRYPTO_AUTH_SNOW3G_UIA2) {
			iv_ptr = conv_to_snow_f9_iv(iv_ptr);
			sge->length = 12;
		} else if (sess->auth_alg == RTE_CRYPTO_AUTH_ZUC_EIA3) {
			iv_ptr = conv_to_zuc_eia_iv(iv_ptr);
			sge->length = 8;
		} else {
			sge->length = sess->iv.length;
		}

		DPAA2_SET_FLE_ADDR(sge, DPAA2_VADDR_TO_IOVA(iv_ptr));
		fle->length = fle->length + sge->length;
		sge++;
	}

	/* Setting data to authenticate */
	DPAA2_SET_FLE_ADDR(sge, DPAA2_MBUF_VADDR_TO_IOVA(sym_op->m_src));
	DPAA2_SET_FLE_OFFSET(sge, data_offset + sym_op->m_src->data_off);
	sge->length = data_len;

	if (sess->dir == DIR_DEC) {
		sge++;
		old_digest = (uint8_t *)(sge + 1);
		rte_memcpy(old_digest, sym_op->auth.digest.data,
			   sess->digest_length);
		DPAA2_SET_FLE_ADDR(sge, DPAA2_VADDR_TO_IOVA(old_digest));
		sge->length = sess->digest_length;
		fle->length = fle->length + sess->digest_length;
	}

	DPAA2_SET_FLE_FIN(sge);
	DPAA2_SET_FLE_FIN(fle);
	DPAA2_SET_FD_LEN(fd, fle->length);

	return 0;
}

static int
build_cipher_sg_fd(dpaa2_sec_session *sess, struct rte_crypto_op *op,
		struct qbman_fd *fd, __rte_unused uint16_t bpid)
{
	struct rte_crypto_sym_op *sym_op = op->sym;
	struct qbman_fle *ip_fle, *op_fle, *sge, *fle;
	int data_len, data_offset;
	struct sec_flow_context *flc;
	struct ctxt_priv *priv = sess->ctxt;
	struct rte_mbuf *mbuf;
	uint8_t *iv_ptr = rte_crypto_op_ctod_offset(op, uint8_t *,
			sess->iv.offset);

	data_len = sym_op->cipher.data.length;
	data_offset = sym_op->cipher.data.offset;

	if (sess->cipher_alg == RTE_CRYPTO_CIPHER_SNOW3G_UEA2 ||
		sess->cipher_alg == RTE_CRYPTO_CIPHER_ZUC_EEA3) {
		if ((data_len & 7) || (data_offset & 7)) {
			DPAA2_SEC_ERR("CIPHER: len/offset must be full bytes");
			return -ENOTSUP;
		}

		data_len = data_len >> 3;
		data_offset = data_offset >> 3;
	}

	if (sym_op->m_dst)
		mbuf = sym_op->m_dst;
	else
		mbuf = sym_op->m_src;

	/* first FLE entry used to store mbuf and session ctxt */
	fle = (struct qbman_fle *)rte_malloc(NULL,
			FLE_SG_MEM_SIZE(mbuf->nb_segs + sym_op->m_src->nb_segs),
			RTE_CACHE_LINE_SIZE);
	if (!fle) {
		DPAA2_SEC_ERR("CIPHER SG: Memory alloc failed for SGE");
		return -ENOMEM;
	}
	memset(fle, 0, FLE_SG_MEM_SIZE(mbuf->nb_segs + sym_op->m_src->nb_segs));
	/* first FLE entry used to store mbuf and session ctxt */
	DPAA2_SET_FLE_ADDR(fle, (size_t)op);
	DPAA2_FLE_SAVE_CTXT(fle, (ptrdiff_t)priv);

	op_fle = fle + 1;
	ip_fle = fle + 2;
	sge = fle + 3;

	flc = &priv->flc_desc[0].flc;

	DPAA2_SEC_DP_DEBUG(
		"CIPHER SG: cipher_off: 0x%x/length %d, ivlen=%d"
		" data_off: 0x%x\n",
		data_offset,
		data_len,
		sess->iv.length,
		sym_op->m_src->data_off);

	/* o/p fle */
	DPAA2_SET_FLE_ADDR(op_fle, DPAA2_VADDR_TO_IOVA(sge));
	op_fle->length = data_len;
	DPAA2_SET_FLE_SG_EXT(op_fle);

	/* o/p 1st seg */
	DPAA2_SET_FLE_ADDR(sge, DPAA2_MBUF_VADDR_TO_IOVA(mbuf));
	DPAA2_SET_FLE_OFFSET(sge, data_offset + mbuf->data_off);
	sge->length = mbuf->data_len - data_offset;

	mbuf = mbuf->next;
	/* o/p segs */
	while (mbuf) {
		sge++;
		DPAA2_SET_FLE_ADDR(sge, DPAA2_MBUF_VADDR_TO_IOVA(mbuf));
		DPAA2_SET_FLE_OFFSET(sge, mbuf->data_off);
		sge->length = mbuf->data_len;
		mbuf = mbuf->next;
	}
	DPAA2_SET_FLE_FIN(sge);

	DPAA2_SEC_DP_DEBUG(
		"CIPHER SG: 1 - flc = %p, fle = %p FLEaddr = %x-%x, len %d\n",
		flc, fle, fle->addr_hi, fle->addr_lo,
		fle->length);

	/* i/p fle */
	mbuf = sym_op->m_src;
	sge++;
	DPAA2_SET_FLE_ADDR(ip_fle, DPAA2_VADDR_TO_IOVA(sge));
	ip_fle->length = sess->iv.length + data_len;
	DPAA2_SET_FLE_SG_EXT(ip_fle);

	/* i/p IV */
	DPAA2_SET_FLE_ADDR(sge, DPAA2_VADDR_TO_IOVA(iv_ptr));
	DPAA2_SET_FLE_OFFSET(sge, 0);
	sge->length = sess->iv.length;

	sge++;

	/* i/p 1st seg */
	DPAA2_SET_FLE_ADDR(sge, DPAA2_MBUF_VADDR_TO_IOVA(mbuf));
	DPAA2_SET_FLE_OFFSET(sge, data_offset + mbuf->data_off);
	sge->length = mbuf->data_len - data_offset;

	mbuf = mbuf->next;
	/* i/p segs */
	while (mbuf) {
		sge++;
		DPAA2_SET_FLE_ADDR(sge, DPAA2_MBUF_VADDR_TO_IOVA(mbuf));
		DPAA2_SET_FLE_OFFSET(sge, mbuf->data_off);
		sge->length = mbuf->data_len;
		mbuf = mbuf->next;
	}
	DPAA2_SET_FLE_FIN(sge);
	DPAA2_SET_FLE_FIN(ip_fle);

	/* sg fd */
	DPAA2_SET_FD_ADDR(fd, DPAA2_VADDR_TO_IOVA(op_fle));
	DPAA2_SET_FD_LEN(fd, ip_fle->length);
	DPAA2_SET_FD_COMPOUND_FMT(fd);
	DPAA2_SET_FD_FLC(fd, DPAA2_VADDR_TO_IOVA(flc));

	DPAA2_SEC_DP_DEBUG(
		"CIPHER SG: fdaddr =%" PRIx64 " bpid =%d meta =%d"
		" off =%d, len =%d\n",
		DPAA2_GET_FD_ADDR(fd),
		DPAA2_GET_FD_BPID(fd),
		rte_dpaa2_bpid_info[bpid].meta_data_size,
		DPAA2_GET_FD_OFFSET(fd),
		DPAA2_GET_FD_LEN(fd));
	return 0;
}

static int
build_cipher_fd(dpaa2_sec_session *sess, struct rte_crypto_op *op,
		struct qbman_fd *fd, uint16_t bpid)
{
	struct rte_crypto_sym_op *sym_op = op->sym;
	struct qbman_fle *fle, *sge;
	int retval, data_len, data_offset;
	struct sec_flow_context *flc;
	struct ctxt_priv *priv = sess->ctxt;
	uint8_t *iv_ptr = rte_crypto_op_ctod_offset(op, uint8_t *,
			sess->iv.offset);
	struct rte_mbuf *dst;

	data_len = sym_op->cipher.data.length;
	data_offset = sym_op->cipher.data.offset;

	if (sess->cipher_alg == RTE_CRYPTO_CIPHER_SNOW3G_UEA2 ||
		sess->cipher_alg == RTE_CRYPTO_CIPHER_ZUC_EEA3) {
		if ((data_len & 7) || (data_offset & 7)) {
			DPAA2_SEC_ERR("CIPHER: len/offset must be full bytes");
			return -ENOTSUP;
		}

		data_len = data_len >> 3;
		data_offset = data_offset >> 3;
	}

	if (sym_op->m_dst)
		dst = sym_op->m_dst;
	else
		dst = sym_op->m_src;

	retval = rte_mempool_get(priv->fle_pool, (void **)(&fle));
	if (retval) {
		DPAA2_SEC_ERR("CIPHER: Memory alloc failed for SGE");
		return -ENOMEM;
	}
	memset(fle, 0, FLE_POOL_BUF_SIZE);
	/* TODO we are using the first FLE entry to store Mbuf.
	 * Currently we donot know which FLE has the mbuf stored.
	 * So while retreiving we can go back 1 FLE from the FD -ADDR
	 * to get the MBUF Addr from the previous FLE.
	 * We can have a better approach to use the inline Mbuf
	 */
	DPAA2_SET_FLE_ADDR(fle, (size_t)op);
	DPAA2_FLE_SAVE_CTXT(fle, (ptrdiff_t)priv);
	fle = fle + 1;
	sge = fle + 2;

	if (likely(bpid < MAX_BPID)) {
		DPAA2_SET_FD_BPID(fd, bpid);
		DPAA2_SET_FLE_BPID(fle, bpid);
		DPAA2_SET_FLE_BPID(fle + 1, bpid);
		DPAA2_SET_FLE_BPID(sge, bpid);
		DPAA2_SET_FLE_BPID(sge + 1, bpid);
	} else {
		DPAA2_SET_FD_IVP(fd);
		DPAA2_SET_FLE_IVP(fle);
		DPAA2_SET_FLE_IVP((fle + 1));
		DPAA2_SET_FLE_IVP(sge);
		DPAA2_SET_FLE_IVP((sge + 1));
	}

	flc = &priv->flc_desc[0].flc;
	DPAA2_SET_FD_ADDR(fd, DPAA2_VADDR_TO_IOVA(fle));
	DPAA2_SET_FD_LEN(fd, data_len + sess->iv.length);
	DPAA2_SET_FD_COMPOUND_FMT(fd);
	DPAA2_SET_FD_FLC(fd, DPAA2_VADDR_TO_IOVA(flc));

	DPAA2_SEC_DP_DEBUG(
		"CIPHER: cipher_off: 0x%x/length %d, ivlen=%d,"
		" data_off: 0x%x\n",
		data_offset,
		data_len,
		sess->iv.length,
		sym_op->m_src->data_off);

	DPAA2_SET_FLE_ADDR(fle, DPAA2_MBUF_VADDR_TO_IOVA(dst));
	DPAA2_SET_FLE_OFFSET(fle, data_offset + dst->data_off);

	fle->length = data_len + sess->iv.length;

	DPAA2_SEC_DP_DEBUG(
		"CIPHER: 1 - flc = %p, fle = %p FLEaddr = %x-%x, length %d\n",
		flc, fle, fle->addr_hi, fle->addr_lo,
		fle->length);

	fle++;

	DPAA2_SET_FLE_ADDR(fle, DPAA2_VADDR_TO_IOVA(sge));
	fle->length = data_len + sess->iv.length;

	DPAA2_SET_FLE_SG_EXT(fle);

	DPAA2_SET_FLE_ADDR(sge, DPAA2_VADDR_TO_IOVA(iv_ptr));
	sge->length = sess->iv.length;

	sge++;
	DPAA2_SET_FLE_ADDR(sge, DPAA2_MBUF_VADDR_TO_IOVA(sym_op->m_src));
	DPAA2_SET_FLE_OFFSET(sge, data_offset + sym_op->m_src->data_off);

	sge->length = data_len;
	DPAA2_SET_FLE_FIN(sge);
	DPAA2_SET_FLE_FIN(fle);

	DPAA2_SEC_DP_DEBUG(
		"CIPHER: fdaddr =%" PRIx64 " bpid =%d meta =%d"
		" off =%d, len =%d\n",
		DPAA2_GET_FD_ADDR(fd),
		DPAA2_GET_FD_BPID(fd),
		rte_dpaa2_bpid_info[bpid].meta_data_size,
		DPAA2_GET_FD_OFFSET(fd),
		DPAA2_GET_FD_LEN(fd));

	return 0;
}

static inline int
build_sec_fd(struct rte_crypto_op *op,
	     struct qbman_fd *fd, uint16_t bpid)
{
	int ret = -1;
	dpaa2_sec_session *sess;

	if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION)
		sess = (dpaa2_sec_session *)get_sym_session_private_data(
				op->sym->session, cryptodev_driver_id);
#ifdef RTE_LIB_SECURITY
	else if (op->sess_type == RTE_CRYPTO_OP_SECURITY_SESSION)
		sess = (dpaa2_sec_session *)get_sec_session_private_data(
				op->sym->sec_session);
#endif
	else
		return -ENOTSUP;

	if (!sess)
		return -EINVAL;

	/* Any of the buffer is segmented*/
	if (!rte_pktmbuf_is_contiguous(op->sym->m_src) ||
		  ((op->sym->m_dst != NULL) &&
		   !rte_pktmbuf_is_contiguous(op->sym->m_dst))) {
		switch (sess->ctxt_type) {
		case DPAA2_SEC_CIPHER:
			ret = build_cipher_sg_fd(sess, op, fd, bpid);
			break;
		case DPAA2_SEC_AUTH:
			ret = build_auth_sg_fd(sess, op, fd, bpid);
			break;
		case DPAA2_SEC_AEAD:
			ret = build_authenc_gcm_sg_fd(sess, op, fd, bpid);
			break;
		case DPAA2_SEC_CIPHER_HASH:
			ret = build_authenc_sg_fd(sess, op, fd, bpid);
			break;
#ifdef RTE_LIB_SECURITY
		case DPAA2_SEC_IPSEC:
		case DPAA2_SEC_PDCP:
			ret = build_proto_compound_sg_fd(sess, op, fd, bpid);
			break;
#endif
		case DPAA2_SEC_HASH_CIPHER:
		default:
			DPAA2_SEC_ERR("error: Unsupported session");
		}
	} else {
		switch (sess->ctxt_type) {
		case DPAA2_SEC_CIPHER:
			ret = build_cipher_fd(sess, op, fd, bpid);
			break;
		case DPAA2_SEC_AUTH:
			ret = build_auth_fd(sess, op, fd, bpid);
			break;
		case DPAA2_SEC_AEAD:
			ret = build_authenc_gcm_fd(sess, op, fd, bpid);
			break;
		case DPAA2_SEC_CIPHER_HASH:
			ret = build_authenc_fd(sess, op, fd, bpid);
			break;
#ifdef RTE_LIB_SECURITY
		case DPAA2_SEC_IPSEC:
			ret = build_proto_fd(sess, op, fd, bpid);
			break;
		case DPAA2_SEC_PDCP:
			ret = build_proto_compound_fd(sess, op, fd, bpid);
			break;
#endif
		case DPAA2_SEC_HASH_CIPHER:
		default:
			DPAA2_SEC_ERR("error: Unsupported session");
			ret = -ENOTSUP;
		}
	}
	return ret;
}

static uint16_t
dpaa2_sec_enqueue_burst(void *qp, struct rte_crypto_op **ops,
			uint16_t nb_ops)
{
	/* Function to transmit the frames to given device and VQ*/
	uint32_t loop;
	int32_t ret;
	struct qbman_fd fd_arr[MAX_TX_RING_SLOTS];
	uint32_t frames_to_send, retry_count;
	struct qbman_eq_desc eqdesc;
	struct dpaa2_sec_qp *dpaa2_qp = (struct dpaa2_sec_qp *)qp;
	struct qbman_swp *swp;
	uint16_t num_tx = 0;
	uint32_t flags[MAX_TX_RING_SLOTS] = {0};
	/*todo - need to support multiple buffer pools */
	uint16_t bpid;
	struct rte_mempool *mb_pool;

	if (unlikely(nb_ops == 0))
		return 0;

	if (ops[0]->sess_type == RTE_CRYPTO_OP_SESSIONLESS) {
		DPAA2_SEC_ERR("sessionless crypto op not supported");
		return 0;
	}
	/*Prepare enqueue descriptor*/
	qbman_eq_desc_clear(&eqdesc);
	qbman_eq_desc_set_no_orp(&eqdesc, DPAA2_EQ_RESP_ERR_FQ);
	qbman_eq_desc_set_response(&eqdesc, 0, 0);
	qbman_eq_desc_set_fq(&eqdesc, dpaa2_qp->tx_vq.fqid);

	if (!DPAA2_PER_LCORE_DPIO) {
		ret = dpaa2_affine_qbman_swp();
		if (ret) {
			DPAA2_SEC_ERR(
				"Failed to allocate IO portal, tid: %d\n",
				rte_gettid());
			return 0;
		}
	}
	swp = DPAA2_PER_LCORE_PORTAL;

	while (nb_ops) {
		frames_to_send = (nb_ops > dpaa2_eqcr_size) ?
			dpaa2_eqcr_size : nb_ops;

		for (loop = 0; loop < frames_to_send; loop++) {
			if (*dpaa2_seqn((*ops)->sym->m_src)) {
				uint8_t dqrr_index =
					*dpaa2_seqn((*ops)->sym->m_src) - 1;

				flags[loop] = QBMAN_ENQUEUE_FLAG_DCA | dqrr_index;
				DPAA2_PER_LCORE_DQRR_SIZE--;
				DPAA2_PER_LCORE_DQRR_HELD &= ~(1 << dqrr_index);
				*dpaa2_seqn((*ops)->sym->m_src) =
					DPAA2_INVALID_MBUF_SEQN;
			}

			/*Clear the unused FD fields before sending*/
			memset(&fd_arr[loop], 0, sizeof(struct qbman_fd));
			mb_pool = (*ops)->sym->m_src->pool;
			bpid = mempool_to_bpid(mb_pool);
			ret = build_sec_fd(*ops, &fd_arr[loop], bpid);
			if (ret) {
				DPAA2_SEC_ERR("error: Improper packet contents"
					      " for crypto operation");
				goto skip_tx;
			}
			ops++;
		}

		loop = 0;
		retry_count = 0;
		while (loop < frames_to_send) {
			ret = qbman_swp_enqueue_multiple(swp, &eqdesc,
							 &fd_arr[loop],
							 &flags[loop],
							 frames_to_send - loop);
			if (unlikely(ret < 0)) {
				retry_count++;
				if (retry_count > DPAA2_MAX_TX_RETRY_COUNT) {
					num_tx += loop;
					nb_ops -= loop;
					goto skip_tx;
				}
			} else {
				loop += ret;
				retry_count = 0;
			}
		}

		num_tx += loop;
		nb_ops -= loop;
	}
skip_tx:
	dpaa2_qp->tx_vq.tx_pkts += num_tx;
	dpaa2_qp->tx_vq.err_pkts += nb_ops;
	return num_tx;
}

#ifdef RTE_LIB_SECURITY
static inline struct rte_crypto_op *
sec_simple_fd_to_mbuf(const struct qbman_fd *fd)
{
	struct rte_crypto_op *op;
	uint16_t len = DPAA2_GET_FD_LEN(fd);
	int16_t diff = 0;
	dpaa2_sec_session *sess_priv __rte_unused;

	struct rte_mbuf *mbuf = DPAA2_INLINE_MBUF_FROM_BUF(
		DPAA2_IOVA_TO_VADDR(DPAA2_GET_FD_ADDR(fd)),
		rte_dpaa2_bpid_info[DPAA2_GET_FD_BPID(fd)].meta_data_size);

	diff = len - mbuf->pkt_len;
	mbuf->pkt_len += diff;
	mbuf->data_len += diff;
	op = (struct rte_crypto_op *)(size_t)mbuf->buf_iova;
	mbuf->buf_iova = op->sym->aead.digest.phys_addr;
	op->sym->aead.digest.phys_addr = 0L;

	sess_priv = (dpaa2_sec_session *)get_sec_session_private_data(
				op->sym->sec_session);
	if (sess_priv->dir == DIR_ENC)
		mbuf->data_off += SEC_FLC_DHR_OUTBOUND;
	else
		mbuf->data_off += SEC_FLC_DHR_INBOUND;

	return op;
}
#endif

static inline struct rte_crypto_op *
sec_fd_to_mbuf(const struct qbman_fd *fd)
{
	struct qbman_fle *fle;
	struct rte_crypto_op *op;
	struct ctxt_priv *priv;
	struct rte_mbuf *dst, *src;

#ifdef RTE_LIB_SECURITY
	if (DPAA2_FD_GET_FORMAT(fd) == qbman_fd_single)
		return sec_simple_fd_to_mbuf(fd);
#endif
	fle = (struct qbman_fle *)DPAA2_IOVA_TO_VADDR(DPAA2_GET_FD_ADDR(fd));

	DPAA2_SEC_DP_DEBUG("FLE addr = %x - %x, offset = %x\n",
			   fle->addr_hi, fle->addr_lo, fle->fin_bpid_offset);

	/* we are using the first FLE entry to store Mbuf.
	 * Currently we donot know which FLE has the mbuf stored.
	 * So while retreiving we can go back 1 FLE from the FD -ADDR
	 * to get the MBUF Addr from the previous FLE.
	 * We can have a better approach to use the inline Mbuf
	 */

	if (unlikely(DPAA2_GET_FD_IVP(fd))) {
		/* TODO complete it. */
		DPAA2_SEC_ERR("error: non inline buffer");
		return NULL;
	}
	op = (struct rte_crypto_op *)DPAA2_GET_FLE_ADDR((fle - 1));

	/* Prefeth op */
	src = op->sym->m_src;
	rte_prefetch0(src);

	if (op->sym->m_dst) {
		dst = op->sym->m_dst;
		rte_prefetch0(dst);
	} else
		dst = src;

#ifdef RTE_LIB_SECURITY
	if (op->sess_type == RTE_CRYPTO_OP_SECURITY_SESSION) {
		uint16_t len = DPAA2_GET_FD_LEN(fd);
		dst->pkt_len = len;
		while (dst->next != NULL) {
			len -= dst->data_len;
			dst = dst->next;
		}
		dst->data_len = len;
	}
#endif
	DPAA2_SEC_DP_DEBUG("mbuf %p BMAN buf addr %p,"
		" fdaddr =%" PRIx64 " bpid =%d meta =%d off =%d, len =%d\n",
		(void *)dst,
		dst->buf_addr,
		DPAA2_GET_FD_ADDR(fd),
		DPAA2_GET_FD_BPID(fd),
		rte_dpaa2_bpid_info[DPAA2_GET_FD_BPID(fd)].meta_data_size,
		DPAA2_GET_FD_OFFSET(fd),
		DPAA2_GET_FD_LEN(fd));

	/* free the fle memory */
	if (likely(rte_pktmbuf_is_contiguous(src))) {
		priv = (struct ctxt_priv *)(size_t)DPAA2_GET_FLE_CTXT(fle - 1);
		rte_mempool_put(priv->fle_pool, (void *)(fle-1));
	} else
		rte_free((void *)(fle-1));

	return op;
}

static uint16_t
dpaa2_sec_dequeue_burst(void *qp, struct rte_crypto_op **ops,
			uint16_t nb_ops)
{
	/* Function is responsible to receive frames for a given device and VQ*/
	struct dpaa2_sec_qp *dpaa2_qp = (struct dpaa2_sec_qp *)qp;
	struct qbman_result *dq_storage;
	uint32_t fqid = dpaa2_qp->rx_vq.fqid;
	int ret, num_rx = 0;
	uint8_t is_last = 0, status;
	struct qbman_swp *swp;
	const struct qbman_fd *fd;
	struct qbman_pull_desc pulldesc;

	if (!DPAA2_PER_LCORE_DPIO) {
		ret = dpaa2_affine_qbman_swp();
		if (ret) {
			DPAA2_SEC_ERR(
				"Failed to allocate IO portal, tid: %d\n",
				rte_gettid());
			return 0;
		}
	}
	swp = DPAA2_PER_LCORE_PORTAL;
	dq_storage = dpaa2_qp->rx_vq.q_storage->dq_storage[0];

	qbman_pull_desc_clear(&pulldesc);
	qbman_pull_desc_set_numframes(&pulldesc,
				      (nb_ops > dpaa2_dqrr_size) ?
				      dpaa2_dqrr_size : nb_ops);
	qbman_pull_desc_set_fq(&pulldesc, fqid);
	qbman_pull_desc_set_storage(&pulldesc, dq_storage,
				    (dma_addr_t)DPAA2_VADDR_TO_IOVA(dq_storage),
				    1);

	/*Issue a volatile dequeue command. */
	while (1) {
		if (qbman_swp_pull(swp, &pulldesc)) {
			DPAA2_SEC_WARN(
				"SEC VDQ command is not issued : QBMAN busy");
			/* Portal was busy, try again */
			continue;
		}
		break;
	};

	/* Receive the packets till Last Dequeue entry is found with
	 * respect to the above issues PULL command.
	 */
	while (!is_last) {
		/* Check if the previous issued command is completed.
		 * Also seems like the SWP is shared between the Ethernet Driver
		 * and the SEC driver.
		 */
		while (!qbman_check_command_complete(dq_storage))
			;

		/* Loop until the dq_storage is updated with
		 * new token by QBMAN
		 */
		while (!qbman_check_new_result(dq_storage))
			;
		/* Check whether Last Pull command is Expired and
		 * setting Condition for Loop termination
		 */
		if (qbman_result_DQ_is_pull_complete(dq_storage)) {
			is_last = 1;
			/* Check for valid frame. */
			status = (uint8_t)qbman_result_DQ_flags(dq_storage);
			if (unlikely(
				(status & QBMAN_DQ_STAT_VALIDFRAME) == 0)) {
				DPAA2_SEC_DP_DEBUG("No frame is delivered\n");
				continue;
			}
		}

		fd = qbman_result_DQ_fd(dq_storage);
		ops[num_rx] = sec_fd_to_mbuf(fd);

		if (unlikely(fd->simple.frc)) {
			/* TODO Parse SEC errors */
			DPAA2_SEC_DP_ERR("SEC returned Error - %x\n",
				      fd->simple.frc);
			dpaa2_qp->rx_vq.err_pkts += 1;
			ops[num_rx]->status = RTE_CRYPTO_OP_STATUS_ERROR;
		} else {
			ops[num_rx]->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
		}

		num_rx++;
		dq_storage++;
	} /* End of Packet Rx loop */

	dpaa2_qp->rx_vq.rx_pkts += num_rx;

	DPAA2_SEC_DP_DEBUG("SEC RX pkts %d err pkts %" PRIu64 "\n", num_rx,
				dpaa2_qp->rx_vq.err_pkts);
	/*Return the total number of packets received to DPAA2 app*/
	return num_rx;
}

/** Release queue pair */
static int
dpaa2_sec_queue_pair_release(struct rte_cryptodev *dev, uint16_t queue_pair_id)
{
	struct dpaa2_sec_qp *qp =
		(struct dpaa2_sec_qp *)dev->data->queue_pairs[queue_pair_id];

	PMD_INIT_FUNC_TRACE();

	if (qp->rx_vq.q_storage) {
		dpaa2_free_dq_storage(qp->rx_vq.q_storage);
		rte_free(qp->rx_vq.q_storage);
	}
	rte_free(qp);

	dev->data->queue_pairs[queue_pair_id] = NULL;

	return 0;
}

/** Setup a queue pair */
static int
dpaa2_sec_queue_pair_setup(struct rte_cryptodev *dev, uint16_t qp_id,
		__rte_unused const struct rte_cryptodev_qp_conf *qp_conf,
		__rte_unused int socket_id)
{
	struct dpaa2_sec_dev_private *priv = dev->data->dev_private;
	struct dpaa2_sec_qp *qp;
	struct fsl_mc_io *dpseci = (struct fsl_mc_io *)priv->hw;
	struct dpseci_rx_queue_cfg cfg;
	int32_t retcode;

	PMD_INIT_FUNC_TRACE();

	/* If qp is already in use free ring memory and qp metadata. */
	if (dev->data->queue_pairs[qp_id] != NULL) {
		DPAA2_SEC_INFO("QP already setup");
		return 0;
	}

	DPAA2_SEC_DEBUG("dev =%p, queue =%d, conf =%p",
		    dev, qp_id, qp_conf);

	memset(&cfg, 0, sizeof(struct dpseci_rx_queue_cfg));

	qp = rte_malloc(NULL, sizeof(struct dpaa2_sec_qp),
			RTE_CACHE_LINE_SIZE);
	if (!qp) {
		DPAA2_SEC_ERR("malloc failed for rx/tx queues");
		return -ENOMEM;
	}

	qp->rx_vq.crypto_data = dev->data;
	qp->tx_vq.crypto_data = dev->data;
	qp->rx_vq.q_storage = rte_malloc("sec dq storage",
		sizeof(struct queue_storage_info_t),
		RTE_CACHE_LINE_SIZE);
	if (!qp->rx_vq.q_storage) {
		DPAA2_SEC_ERR("malloc failed for q_storage");
		return -ENOMEM;
	}
	memset(qp->rx_vq.q_storage, 0, sizeof(struct queue_storage_info_t));

	if (dpaa2_alloc_dq_storage(qp->rx_vq.q_storage)) {
		DPAA2_SEC_ERR("Unable to allocate dequeue storage");
		return -ENOMEM;
	}

	dev->data->queue_pairs[qp_id] = qp;

	cfg.options = cfg.options | DPSECI_QUEUE_OPT_USER_CTX;
	cfg.user_ctx = (size_t)(&qp->rx_vq);
	retcode = dpseci_set_rx_queue(dpseci, CMD_PRI_LOW, priv->token,
				      qp_id, &cfg);
	return retcode;
}

/** Returns the size of the aesni gcm session structure */
static unsigned int
dpaa2_sec_sym_session_get_size(struct rte_cryptodev *dev __rte_unused)
{
	PMD_INIT_FUNC_TRACE();

	return sizeof(dpaa2_sec_session);
}

static int
dpaa2_sec_cipher_init(struct rte_cryptodev *dev,
		      struct rte_crypto_sym_xform *xform,
		      dpaa2_sec_session *session)
{
	struct dpaa2_sec_dev_private *dev_priv = dev->data->dev_private;
	struct alginfo cipherdata;
	int bufsize, ret = 0;
	struct ctxt_priv *priv;
	struct sec_flow_context *flc;

	PMD_INIT_FUNC_TRACE();

	/* For SEC CIPHER only one descriptor is required. */
	priv = (struct ctxt_priv *)rte_zmalloc(NULL,
			sizeof(struct ctxt_priv) + sizeof(struct sec_flc_desc),
			RTE_CACHE_LINE_SIZE);
	if (priv == NULL) {
		DPAA2_SEC_ERR("No Memory for priv CTXT");
		return -ENOMEM;
	}

	priv->fle_pool = dev_priv->fle_pool;

	flc = &priv->flc_desc[0].flc;

	session->ctxt_type = DPAA2_SEC_CIPHER;
	session->cipher_key.data = rte_zmalloc(NULL, xform->cipher.key.length,
			RTE_CACHE_LINE_SIZE);
	if (session->cipher_key.data == NULL && xform->cipher.key.length > 0) {
		DPAA2_SEC_ERR("No Memory for cipher key");
		rte_free(priv);
		return -ENOMEM;
	}
	session->cipher_key.length = xform->cipher.key.length;

	memcpy(session->cipher_key.data, xform->cipher.key.data,
	       xform->cipher.key.length);
	cipherdata.key = (size_t)session->cipher_key.data;
	cipherdata.keylen = session->cipher_key.length;
	cipherdata.key_enc_flags = 0;
	cipherdata.key_type = RTA_DATA_IMM;

	/* Set IV parameters */
	session->iv.offset = xform->cipher.iv.offset;
	session->iv.length = xform->cipher.iv.length;
	session->dir = (xform->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT) ?
				DIR_ENC : DIR_DEC;

	switch (xform->cipher.algo) {
	case RTE_CRYPTO_CIPHER_AES_CBC:
		cipherdata.algtype = OP_ALG_ALGSEL_AES;
		cipherdata.algmode = OP_ALG_AAI_CBC;
		session->cipher_alg = RTE_CRYPTO_CIPHER_AES_CBC;
		bufsize = cnstr_shdsc_blkcipher(priv->flc_desc[0].desc, 1, 0,
						SHR_NEVER, &cipherdata,
						session->iv.length,
						session->dir);
		break;
	case RTE_CRYPTO_CIPHER_3DES_CBC:
		cipherdata.algtype = OP_ALG_ALGSEL_3DES;
		cipherdata.algmode = OP_ALG_AAI_CBC;
		session->cipher_alg = RTE_CRYPTO_CIPHER_3DES_CBC;
		bufsize = cnstr_shdsc_blkcipher(priv->flc_desc[0].desc, 1, 0,
						SHR_NEVER, &cipherdata,
						session->iv.length,
						session->dir);
		break;
	case RTE_CRYPTO_CIPHER_DES_CBC:
		cipherdata.algtype = OP_ALG_ALGSEL_DES;
		cipherdata.algmode = OP_ALG_AAI_CBC;
		session->cipher_alg = RTE_CRYPTO_CIPHER_DES_CBC;
		bufsize = cnstr_shdsc_blkcipher(priv->flc_desc[0].desc, 1, 0,
						SHR_NEVER, &cipherdata,
						session->iv.length,
						session->dir);
		break;
	case RTE_CRYPTO_CIPHER_AES_CTR:
		cipherdata.algtype = OP_ALG_ALGSEL_AES;
		cipherdata.algmode = OP_ALG_AAI_CTR;
		session->cipher_alg = RTE_CRYPTO_CIPHER_AES_CTR;
		bufsize = cnstr_shdsc_blkcipher(priv->flc_desc[0].desc, 1, 0,
						SHR_NEVER, &cipherdata,
						session->iv.length,
						session->dir);
		break;
	case RTE_CRYPTO_CIPHER_SNOW3G_UEA2:
		cipherdata.algtype = OP_ALG_ALGSEL_SNOW_F8;
		session->cipher_alg = RTE_CRYPTO_CIPHER_SNOW3G_UEA2;
		bufsize = cnstr_shdsc_snow_f8(priv->flc_desc[0].desc, 1, 0,
					      &cipherdata,
					      session->dir);
		break;
	case RTE_CRYPTO_CIPHER_ZUC_EEA3:
		cipherdata.algtype = OP_ALG_ALGSEL_ZUCE;
		session->cipher_alg = RTE_CRYPTO_CIPHER_ZUC_EEA3;
		bufsize = cnstr_shdsc_zuce(priv->flc_desc[0].desc, 1, 0,
					      &cipherdata,
					      session->dir);
		break;
	case RTE_CRYPTO_CIPHER_KASUMI_F8:
	case RTE_CRYPTO_CIPHER_AES_F8:
	case RTE_CRYPTO_CIPHER_AES_ECB:
	case RTE_CRYPTO_CIPHER_3DES_ECB:
	case RTE_CRYPTO_CIPHER_3DES_CTR:
	case RTE_CRYPTO_CIPHER_AES_XTS:
	case RTE_CRYPTO_CIPHER_ARC4:
	case RTE_CRYPTO_CIPHER_NULL:
		DPAA2_SEC_ERR("Crypto: Unsupported Cipher alg %u",
			xform->cipher.algo);
		ret = -ENOTSUP;
		goto error_out;
	default:
		DPAA2_SEC_ERR("Crypto: Undefined Cipher specified %u",
			xform->cipher.algo);
		ret = -ENOTSUP;
		goto error_out;
	}

	if (bufsize < 0) {
		DPAA2_SEC_ERR("Crypto: Descriptor build failed");
		ret = -EINVAL;
		goto error_out;
	}

	flc->word1_sdl = (uint8_t)bufsize;
	session->ctxt = priv;

#ifdef CAAM_DESC_DEBUG
	int i;
	for (i = 0; i < bufsize; i++)
		DPAA2_SEC_DEBUG("DESC[%d]:0x%x", i, priv->flc_desc[0].desc[i]);
#endif
	return ret;

error_out:
	rte_free(session->cipher_key.data);
	rte_free(priv);
	return ret;
}

static int
dpaa2_sec_auth_init(struct rte_cryptodev *dev,
		    struct rte_crypto_sym_xform *xform,
		    dpaa2_sec_session *session)
{
	struct dpaa2_sec_dev_private *dev_priv = dev->data->dev_private;
	struct alginfo authdata;
	int bufsize, ret = 0;
	struct ctxt_priv *priv;
	struct sec_flow_context *flc;

	PMD_INIT_FUNC_TRACE();

	/* For SEC AUTH three descriptors are required for various stages */
	priv = (struct ctxt_priv *)rte_zmalloc(NULL,
			sizeof(struct ctxt_priv) + 3 *
			sizeof(struct sec_flc_desc),
			RTE_CACHE_LINE_SIZE);
	if (priv == NULL) {
		DPAA2_SEC_ERR("No Memory for priv CTXT");
		return -ENOMEM;
	}

	priv->fle_pool = dev_priv->fle_pool;
	flc = &priv->flc_desc[DESC_INITFINAL].flc;

	session->ctxt_type = DPAA2_SEC_AUTH;
	session->auth_key.length = xform->auth.key.length;
	if (xform->auth.key.length) {
		session->auth_key.data = rte_zmalloc(NULL,
			xform->auth.key.length,
			RTE_CACHE_LINE_SIZE);
		if (session->auth_key.data == NULL) {
			DPAA2_SEC_ERR("Unable to allocate memory for auth key");
			rte_free(priv);
			return -ENOMEM;
		}
		memcpy(session->auth_key.data, xform->auth.key.data,
		       xform->auth.key.length);
		authdata.key = (size_t)session->auth_key.data;
		authdata.key_enc_flags = 0;
		authdata.key_type = RTA_DATA_IMM;
	}
	authdata.keylen = session->auth_key.length;

	session->digest_length = xform->auth.digest_length;
	session->dir = (xform->auth.op == RTE_CRYPTO_AUTH_OP_GENERATE) ?
				DIR_ENC : DIR_DEC;

	switch (xform->auth.algo) {
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		authdata.algtype = OP_ALG_ALGSEL_SHA1;
		authdata.algmode = OP_ALG_AAI_HMAC;
		session->auth_alg = RTE_CRYPTO_AUTH_SHA1_HMAC;
		bufsize = cnstr_shdsc_hmac(priv->flc_desc[DESC_INITFINAL].desc,
					   1, 0, SHR_NEVER, &authdata,
					   !session->dir,
					   session->digest_length);
		break;
	case RTE_CRYPTO_AUTH_MD5_HMAC:
		authdata.algtype = OP_ALG_ALGSEL_MD5;
		authdata.algmode = OP_ALG_AAI_HMAC;
		session->auth_alg = RTE_CRYPTO_AUTH_MD5_HMAC;
		bufsize = cnstr_shdsc_hmac(priv->flc_desc[DESC_INITFINAL].desc,
					   1, 0, SHR_NEVER, &authdata,
					   !session->dir,
					   session->digest_length);
		break;
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
		authdata.algtype = OP_ALG_ALGSEL_SHA256;
		authdata.algmode = OP_ALG_AAI_HMAC;
		session->auth_alg = RTE_CRYPTO_AUTH_SHA256_HMAC;
		bufsize = cnstr_shdsc_hmac(priv->flc_desc[DESC_INITFINAL].desc,
					   1, 0, SHR_NEVER, &authdata,
					   !session->dir,
					   session->digest_length);
		break;
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
		authdata.algtype = OP_ALG_ALGSEL_SHA384;
		authdata.algmode = OP_ALG_AAI_HMAC;
		session->auth_alg = RTE_CRYPTO_AUTH_SHA384_HMAC;
		bufsize = cnstr_shdsc_hmac(priv->flc_desc[DESC_INITFINAL].desc,
					   1, 0, SHR_NEVER, &authdata,
					   !session->dir,
					   session->digest_length);
		break;
	case RTE_CRYPTO_AUTH_SHA512_HMAC:
		authdata.algtype = OP_ALG_ALGSEL_SHA512;
		authdata.algmode = OP_ALG_AAI_HMAC;
		session->auth_alg = RTE_CRYPTO_AUTH_SHA512_HMAC;
		bufsize = cnstr_shdsc_hmac(priv->flc_desc[DESC_INITFINAL].desc,
					   1, 0, SHR_NEVER, &authdata,
					   !session->dir,
					   session->digest_length);
		break;
	case RTE_CRYPTO_AUTH_SHA224_HMAC:
		authdata.algtype = OP_ALG_ALGSEL_SHA224;
		authdata.algmode = OP_ALG_AAI_HMAC;
		session->auth_alg = RTE_CRYPTO_AUTH_SHA224_HMAC;
		bufsize = cnstr_shdsc_hmac(priv->flc_desc[DESC_INITFINAL].desc,
					   1, 0, SHR_NEVER, &authdata,
					   !session->dir,
					   session->digest_length);
		break;
	case RTE_CRYPTO_AUTH_SNOW3G_UIA2:
		authdata.algtype = OP_ALG_ALGSEL_SNOW_F9;
		authdata.algmode = OP_ALG_AAI_F9;
		session->auth_alg = RTE_CRYPTO_AUTH_SNOW3G_UIA2;
		session->iv.offset = xform->auth.iv.offset;
		session->iv.length = xform->auth.iv.length;
		bufsize = cnstr_shdsc_snow_f9(priv->flc_desc[DESC_INITFINAL].desc,
					      1, 0, &authdata,
					      !session->dir,
					      session->digest_length);
		break;
	case RTE_CRYPTO_AUTH_ZUC_EIA3:
		authdata.algtype = OP_ALG_ALGSEL_ZUCA;
		authdata.algmode = OP_ALG_AAI_F9;
		session->auth_alg = RTE_CRYPTO_AUTH_ZUC_EIA3;
		session->iv.offset = xform->auth.iv.offset;
		session->iv.length = xform->auth.iv.length;
		bufsize = cnstr_shdsc_zuca(priv->flc_desc[DESC_INITFINAL].desc,
					   1, 0, &authdata,
					   !session->dir,
					   session->digest_length);
		break;
	case RTE_CRYPTO_AUTH_SHA1:
		authdata.algtype = OP_ALG_ALGSEL_SHA1;
		authdata.algmode = OP_ALG_AAI_HASH;
		session->auth_alg = RTE_CRYPTO_AUTH_SHA1;
		bufsize = cnstr_shdsc_hash(priv->flc_desc[DESC_INITFINAL].desc,
					   1, 0, SHR_NEVER, &authdata,
					   !session->dir,
					   session->digest_length);
		break;
	case RTE_CRYPTO_AUTH_MD5:
		authdata.algtype = OP_ALG_ALGSEL_MD5;
		authdata.algmode = OP_ALG_AAI_HASH;
		session->auth_alg = RTE_CRYPTO_AUTH_MD5;
		bufsize = cnstr_shdsc_hash(priv->flc_desc[DESC_INITFINAL].desc,
					   1, 0, SHR_NEVER, &authdata,
					   !session->dir,
					   session->digest_length);
		break;
	case RTE_CRYPTO_AUTH_SHA256:
		authdata.algtype = OP_ALG_ALGSEL_SHA256;
		authdata.algmode = OP_ALG_AAI_HASH;
		session->auth_alg = RTE_CRYPTO_AUTH_SHA256;
		bufsize = cnstr_shdsc_hash(priv->flc_desc[DESC_INITFINAL].desc,
					   1, 0, SHR_NEVER, &authdata,
					   !session->dir,
					   session->digest_length);
		break;
	case RTE_CRYPTO_AUTH_SHA384:
		authdata.algtype = OP_ALG_ALGSEL_SHA384;
		authdata.algmode = OP_ALG_AAI_HASH;
		session->auth_alg = RTE_CRYPTO_AUTH_SHA384;
		bufsize = cnstr_shdsc_hash(priv->flc_desc[DESC_INITFINAL].desc,
					   1, 0, SHR_NEVER, &authdata,
					   !session->dir,
					   session->digest_length);
		break;
	case RTE_CRYPTO_AUTH_SHA512:
		authdata.algtype = OP_ALG_ALGSEL_SHA512;
		authdata.algmode = OP_ALG_AAI_HASH;
		session->auth_alg = RTE_CRYPTO_AUTH_SHA512;
		bufsize = cnstr_shdsc_hash(priv->flc_desc[DESC_INITFINAL].desc,
					   1, 0, SHR_NEVER, &authdata,
					   !session->dir,
					   session->digest_length);
		break;
	case RTE_CRYPTO_AUTH_SHA224:
		authdata.algtype = OP_ALG_ALGSEL_SHA224;
		authdata.algmode = OP_ALG_AAI_HASH;
		session->auth_alg = RTE_CRYPTO_AUTH_SHA224;
		bufsize = cnstr_shdsc_hash(priv->flc_desc[DESC_INITFINAL].desc,
					   1, 0, SHR_NEVER, &authdata,
					   !session->dir,
					   session->digest_length);
		break;
	case RTE_CRYPTO_AUTH_AES_XCBC_MAC:
		authdata.algtype = OP_ALG_ALGSEL_AES;
		authdata.algmode = OP_ALG_AAI_XCBC_MAC;
		session->auth_alg = RTE_CRYPTO_AUTH_AES_XCBC_MAC;
		bufsize = cnstr_shdsc_aes_mac(
					priv->flc_desc[DESC_INITFINAL].desc,
					1, 0, SHR_NEVER, &authdata,
					!session->dir,
					session->digest_length);
		break;
	case RTE_CRYPTO_AUTH_AES_CMAC:
		authdata.algtype = OP_ALG_ALGSEL_AES;
		authdata.algmode = OP_ALG_AAI_CMAC;
		session->auth_alg = RTE_CRYPTO_AUTH_AES_CMAC;
		bufsize = cnstr_shdsc_aes_mac(
					   priv->flc_desc[DESC_INITFINAL].desc,
					   1, 0, SHR_NEVER, &authdata,
					   !session->dir,
					   session->digest_length);
		break;
	case RTE_CRYPTO_AUTH_AES_CBC_MAC:
	case RTE_CRYPTO_AUTH_AES_GMAC:
	case RTE_CRYPTO_AUTH_KASUMI_F9:
	case RTE_CRYPTO_AUTH_NULL:
		DPAA2_SEC_ERR("Crypto: Unsupported auth alg %un",
			      xform->auth.algo);
		ret = -ENOTSUP;
		goto error_out;
	default:
		DPAA2_SEC_ERR("Crypto: Undefined Auth specified %u",
			      xform->auth.algo);
		ret = -ENOTSUP;
		goto error_out;
	}

	if (bufsize < 0) {
		DPAA2_SEC_ERR("Crypto: Invalid buffer length");
		ret = -EINVAL;
		goto error_out;
	}

	flc->word1_sdl = (uint8_t)bufsize;
	session->ctxt = priv;
#ifdef CAAM_DESC_DEBUG
	int i;
	for (i = 0; i < bufsize; i++)
		DPAA2_SEC_DEBUG("DESC[%d]:0x%x",
				i, priv->flc_desc[DESC_INITFINAL].desc[i]);
#endif

	return ret;

error_out:
	rte_free(session->auth_key.data);
	rte_free(priv);
	return ret;
}

static int
dpaa2_sec_aead_init(struct rte_cryptodev *dev,
		    struct rte_crypto_sym_xform *xform,
		    dpaa2_sec_session *session)
{
	struct dpaa2_sec_aead_ctxt *ctxt = &session->ext_params.aead_ctxt;
	struct dpaa2_sec_dev_private *dev_priv = dev->data->dev_private;
	struct alginfo aeaddata;
	int bufsize;
	struct ctxt_priv *priv;
	struct sec_flow_context *flc;
	struct rte_crypto_aead_xform *aead_xform = &xform->aead;
	int err, ret = 0;

	PMD_INIT_FUNC_TRACE();

	/* Set IV parameters */
	session->iv.offset = aead_xform->iv.offset;
	session->iv.length = aead_xform->iv.length;
	session->ctxt_type = DPAA2_SEC_AEAD;

	/* For SEC AEAD only one descriptor is required */
	priv = (struct ctxt_priv *)rte_zmalloc(NULL,
			sizeof(struct ctxt_priv) + sizeof(struct sec_flc_desc),
			RTE_CACHE_LINE_SIZE);
	if (priv == NULL) {
		DPAA2_SEC_ERR("No Memory for priv CTXT");
		return -ENOMEM;
	}

	priv->fle_pool = dev_priv->fle_pool;
	flc = &priv->flc_desc[0].flc;

	session->aead_key.data = rte_zmalloc(NULL, aead_xform->key.length,
					       RTE_CACHE_LINE_SIZE);
	if (session->aead_key.data == NULL && aead_xform->key.length > 0) {
		DPAA2_SEC_ERR("No Memory for aead key");
		rte_free(priv);
		return -ENOMEM;
	}
	memcpy(session->aead_key.data, aead_xform->key.data,
	       aead_xform->key.length);

	session->digest_length = aead_xform->digest_length;
	session->aead_key.length = aead_xform->key.length;
	ctxt->auth_only_len = aead_xform->aad_length;

	aeaddata.key = (size_t)session->aead_key.data;
	aeaddata.keylen = session->aead_key.length;
	aeaddata.key_enc_flags = 0;
	aeaddata.key_type = RTA_DATA_IMM;

	switch (aead_xform->algo) {
	case RTE_CRYPTO_AEAD_AES_GCM:
		aeaddata.algtype = OP_ALG_ALGSEL_AES;
		aeaddata.algmode = OP_ALG_AAI_GCM;
		session->aead_alg = RTE_CRYPTO_AEAD_AES_GCM;
		break;
	case RTE_CRYPTO_AEAD_AES_CCM:
		DPAA2_SEC_ERR("Crypto: Unsupported AEAD alg %u",
			      aead_xform->algo);
		ret = -ENOTSUP;
		goto error_out;
	default:
		DPAA2_SEC_ERR("Crypto: Undefined AEAD specified %u",
			      aead_xform->algo);
		ret = -ENOTSUP;
		goto error_out;
	}
	session->dir = (aead_xform->op == RTE_CRYPTO_AEAD_OP_ENCRYPT) ?
				DIR_ENC : DIR_DEC;

	priv->flc_desc[0].desc[0] = aeaddata.keylen;
	err = rta_inline_query(IPSEC_AUTH_VAR_AES_DEC_BASE_DESC_LEN,
			       DESC_JOB_IO_LEN,
			       (unsigned int *)priv->flc_desc[0].desc,
			       &priv->flc_desc[0].desc[1], 1);

	if (err < 0) {
		DPAA2_SEC_ERR("Crypto: Incorrect key lengths");
		ret = -EINVAL;
		goto error_out;
	}
	if (priv->flc_desc[0].desc[1] & 1) {
		aeaddata.key_type = RTA_DATA_IMM;
	} else {
		aeaddata.key = DPAA2_VADDR_TO_IOVA(aeaddata.key);
		aeaddata.key_type = RTA_DATA_PTR;
	}
	priv->flc_desc[0].desc[0] = 0;
	priv->flc_desc[0].desc[1] = 0;

	if (session->dir == DIR_ENC)
		bufsize = cnstr_shdsc_gcm_encap(
				priv->flc_desc[0].desc, 1, 0, SHR_NEVER,
				&aeaddata, session->iv.length,
				session->digest_length);
	else
		bufsize = cnstr_shdsc_gcm_decap(
				priv->flc_desc[0].desc, 1, 0, SHR_NEVER,
				&aeaddata, session->iv.length,
				session->digest_length);
	if (bufsize < 0) {
		DPAA2_SEC_ERR("Crypto: Invalid buffer length");
		ret = -EINVAL;
		goto error_out;
	}

	flc->word1_sdl = (uint8_t)bufsize;
	session->ctxt = priv;
#ifdef CAAM_DESC_DEBUG
	int i;
	for (i = 0; i < bufsize; i++)
		DPAA2_SEC_DEBUG("DESC[%d]:0x%x\n",
			    i, priv->flc_desc[0].desc[i]);
#endif
	return ret;

error_out:
	rte_free(session->aead_key.data);
	rte_free(priv);
	return ret;
}


static int
dpaa2_sec_aead_chain_init(struct rte_cryptodev *dev,
		    struct rte_crypto_sym_xform *xform,
		    dpaa2_sec_session *session)
{
	struct dpaa2_sec_dev_private *dev_priv = dev->data->dev_private;
	struct alginfo authdata, cipherdata;
	int bufsize;
	struct ctxt_priv *priv;
	struct sec_flow_context *flc;
	struct rte_crypto_cipher_xform *cipher_xform;
	struct rte_crypto_auth_xform *auth_xform;
	int err, ret = 0;

	PMD_INIT_FUNC_TRACE();

	if (session->ext_params.aead_ctxt.auth_cipher_text) {
		cipher_xform = &xform->cipher;
		auth_xform = &xform->next->auth;
		session->ctxt_type =
			(cipher_xform->op == RTE_CRYPTO_CIPHER_OP_ENCRYPT) ?
			DPAA2_SEC_CIPHER_HASH : DPAA2_SEC_HASH_CIPHER;
	} else {
		cipher_xform = &xform->next->cipher;
		auth_xform = &xform->auth;
		session->ctxt_type =
			(cipher_xform->op == RTE_CRYPTO_CIPHER_OP_ENCRYPT) ?
			DPAA2_SEC_HASH_CIPHER : DPAA2_SEC_CIPHER_HASH;
	}

	/* Set IV parameters */
	session->iv.offset = cipher_xform->iv.offset;
	session->iv.length = cipher_xform->iv.length;

	/* For SEC AEAD only one descriptor is required */
	priv = (struct ctxt_priv *)rte_zmalloc(NULL,
			sizeof(struct ctxt_priv) + sizeof(struct sec_flc_desc),
			RTE_CACHE_LINE_SIZE);
	if (priv == NULL) {
		DPAA2_SEC_ERR("No Memory for priv CTXT");
		return -ENOMEM;
	}

	priv->fle_pool = dev_priv->fle_pool;
	flc = &priv->flc_desc[0].flc;

	session->cipher_key.data = rte_zmalloc(NULL, cipher_xform->key.length,
					       RTE_CACHE_LINE_SIZE);
	if (session->cipher_key.data == NULL && cipher_xform->key.length > 0) {
		DPAA2_SEC_ERR("No Memory for cipher key");
		rte_free(priv);
		return -ENOMEM;
	}
	session->cipher_key.length = cipher_xform->key.length;
	session->auth_key.data = rte_zmalloc(NULL, auth_xform->key.length,
					     RTE_CACHE_LINE_SIZE);
	if (session->auth_key.data == NULL && auth_xform->key.length > 0) {
		DPAA2_SEC_ERR("No Memory for auth key");
		rte_free(session->cipher_key.data);
		rte_free(priv);
		return -ENOMEM;
	}
	session->auth_key.length = auth_xform->key.length;
	memcpy(session->cipher_key.data, cipher_xform->key.data,
	       cipher_xform->key.length);
	memcpy(session->auth_key.data, auth_xform->key.data,
	       auth_xform->key.length);

	authdata.key = (size_t)session->auth_key.data;
	authdata.keylen = session->auth_key.length;
	authdata.key_enc_flags = 0;
	authdata.key_type = RTA_DATA_IMM;

	session->digest_length = auth_xform->digest_length;

	switch (auth_xform->algo) {
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		authdata.algtype = OP_ALG_ALGSEL_SHA1;
		authdata.algmode = OP_ALG_AAI_HMAC;
		session->auth_alg = RTE_CRYPTO_AUTH_SHA1_HMAC;
		break;
	case RTE_CRYPTO_AUTH_MD5_HMAC:
		authdata.algtype = OP_ALG_ALGSEL_MD5;
		authdata.algmode = OP_ALG_AAI_HMAC;
		session->auth_alg = RTE_CRYPTO_AUTH_MD5_HMAC;
		break;
	case RTE_CRYPTO_AUTH_SHA224_HMAC:
		authdata.algtype = OP_ALG_ALGSEL_SHA224;
		authdata.algmode = OP_ALG_AAI_HMAC;
		session->auth_alg = RTE_CRYPTO_AUTH_SHA224_HMAC;
		break;
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
		authdata.algtype = OP_ALG_ALGSEL_SHA256;
		authdata.algmode = OP_ALG_AAI_HMAC;
		session->auth_alg = RTE_CRYPTO_AUTH_SHA256_HMAC;
		break;
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
		authdata.algtype = OP_ALG_ALGSEL_SHA384;
		authdata.algmode = OP_ALG_AAI_HMAC;
		session->auth_alg = RTE_CRYPTO_AUTH_SHA384_HMAC;
		break;
	case RTE_CRYPTO_AUTH_SHA512_HMAC:
		authdata.algtype = OP_ALG_ALGSEL_SHA512;
		authdata.algmode = OP_ALG_AAI_HMAC;
		session->auth_alg = RTE_CRYPTO_AUTH_SHA512_HMAC;
		break;
	case RTE_CRYPTO_AUTH_AES_XCBC_MAC:
		authdata.algtype = OP_ALG_ALGSEL_AES;
		authdata.algmode = OP_ALG_AAI_XCBC_MAC;
		session->auth_alg = RTE_CRYPTO_AUTH_AES_XCBC_MAC;
		break;
	case RTE_CRYPTO_AUTH_AES_CMAC:
		authdata.algtype = OP_ALG_ALGSEL_AES;
		authdata.algmode = OP_ALG_AAI_CMAC;
		session->auth_alg = RTE_CRYPTO_AUTH_AES_CMAC;
		break;
	case RTE_CRYPTO_AUTH_AES_CBC_MAC:
	case RTE_CRYPTO_AUTH_AES_GMAC:
	case RTE_CRYPTO_AUTH_SNOW3G_UIA2:
	case RTE_CRYPTO_AUTH_NULL:
	case RTE_CRYPTO_AUTH_SHA1:
	case RTE_CRYPTO_AUTH_SHA256:
	case RTE_CRYPTO_AUTH_SHA512:
	case RTE_CRYPTO_AUTH_SHA224:
	case RTE_CRYPTO_AUTH_SHA384:
	case RTE_CRYPTO_AUTH_MD5:
	case RTE_CRYPTO_AUTH_KASUMI_F9:
	case RTE_CRYPTO_AUTH_ZUC_EIA3:
		DPAA2_SEC_ERR("Crypto: Unsupported auth alg %u",
			      auth_xform->algo);
		ret = -ENOTSUP;
		goto error_out;
	default:
		DPAA2_SEC_ERR("Crypto: Undefined Auth specified %u",
			      auth_xform->algo);
		ret = -ENOTSUP;
		goto error_out;
	}
	cipherdata.key = (size_t)session->cipher_key.data;
	cipherdata.keylen = session->cipher_key.length;
	cipherdata.key_enc_flags = 0;
	cipherdata.key_type = RTA_DATA_IMM;

	switch (cipher_xform->algo) {
	case RTE_CRYPTO_CIPHER_AES_CBC:
		cipherdata.algtype = OP_ALG_ALGSEL_AES;
		cipherdata.algmode = OP_ALG_AAI_CBC;
		session->cipher_alg = RTE_CRYPTO_CIPHER_AES_CBC;
		break;
	case RTE_CRYPTO_CIPHER_3DES_CBC:
		cipherdata.algtype = OP_ALG_ALGSEL_3DES;
		cipherdata.algmode = OP_ALG_AAI_CBC;
		session->cipher_alg = RTE_CRYPTO_CIPHER_3DES_CBC;
		break;
	case RTE_CRYPTO_CIPHER_DES_CBC:
		cipherdata.algtype = OP_ALG_ALGSEL_DES;
		cipherdata.algmode = OP_ALG_AAI_CBC;
		session->cipher_alg = RTE_CRYPTO_CIPHER_DES_CBC;
		break;
	case RTE_CRYPTO_CIPHER_AES_CTR:
		cipherdata.algtype = OP_ALG_ALGSEL_AES;
		cipherdata.algmode = OP_ALG_AAI_CTR;
		session->cipher_alg = RTE_CRYPTO_CIPHER_AES_CTR;
		break;
	case RTE_CRYPTO_CIPHER_SNOW3G_UEA2:
	case RTE_CRYPTO_CIPHER_ZUC_EEA3:
	case RTE_CRYPTO_CIPHER_NULL:
	case RTE_CRYPTO_CIPHER_3DES_ECB:
	case RTE_CRYPTO_CIPHER_3DES_CTR:
	case RTE_CRYPTO_CIPHER_AES_ECB:
	case RTE_CRYPTO_CIPHER_KASUMI_F8:
		DPAA2_SEC_ERR("Crypto: Unsupported Cipher alg %u",
			      cipher_xform->algo);
		ret = -ENOTSUP;
		goto error_out;
	default:
		DPAA2_SEC_ERR("Crypto: Undefined Cipher specified %u",
			      cipher_xform->algo);
		ret = -ENOTSUP;
		goto error_out;
	}
	session->dir = (cipher_xform->op == RTE_CRYPTO_CIPHER_OP_ENCRYPT) ?
				DIR_ENC : DIR_DEC;

	priv->flc_desc[0].desc[0] = cipherdata.keylen;
	priv->flc_desc[0].desc[1] = authdata.keylen;
	err = rta_inline_query(IPSEC_AUTH_VAR_AES_DEC_BASE_DESC_LEN,
			       DESC_JOB_IO_LEN,
			       (unsigned int *)priv->flc_desc[0].desc,
			       &priv->flc_desc[0].desc[2], 2);

	if (err < 0) {
		DPAA2_SEC_ERR("Crypto: Incorrect key lengths");
		ret = -EINVAL;
		goto error_out;
	}
	if (priv->flc_desc[0].desc[2] & 1) {
		cipherdata.key_type = RTA_DATA_IMM;
	} else {
		cipherdata.key = DPAA2_VADDR_TO_IOVA(cipherdata.key);
		cipherdata.key_type = RTA_DATA_PTR;
	}
	if (priv->flc_desc[0].desc[2] & (1 << 1)) {
		authdata.key_type = RTA_DATA_IMM;
	} else {
		authdata.key = DPAA2_VADDR_TO_IOVA(authdata.key);
		authdata.key_type = RTA_DATA_PTR;
	}
	priv->flc_desc[0].desc[0] = 0;
	priv->flc_desc[0].desc[1] = 0;
	priv->flc_desc[0].desc[2] = 0;

	if (session->ctxt_type == DPAA2_SEC_CIPHER_HASH) {
		bufsize = cnstr_shdsc_authenc(priv->flc_desc[0].desc, 1,
					      0, SHR_SERIAL,
					      &cipherdata, &authdata,
					      session->iv.length,
					      session->digest_length,
					      session->dir);
		if (bufsize < 0) {
			DPAA2_SEC_ERR("Crypto: Invalid buffer length");
			ret = -EINVAL;
			goto error_out;
		}
	} else {
		DPAA2_SEC_ERR("Hash before cipher not supported");
		ret = -ENOTSUP;
		goto error_out;
	}

	flc->word1_sdl = (uint8_t)bufsize;
	session->ctxt = priv;
#ifdef CAAM_DESC_DEBUG
	int i;
	for (i = 0; i < bufsize; i++)
		DPAA2_SEC_DEBUG("DESC[%d]:0x%x",
			    i, priv->flc_desc[0].desc[i]);
#endif

	return ret;

error_out:
	rte_free(session->cipher_key.data);
	rte_free(session->auth_key.data);
	rte_free(priv);
	return ret;
}

static int
dpaa2_sec_set_session_parameters(struct rte_cryptodev *dev,
			    struct rte_crypto_sym_xform *xform,	void *sess)
{
	dpaa2_sec_session *session = sess;
	int ret;

	PMD_INIT_FUNC_TRACE();

	if (unlikely(sess == NULL)) {
		DPAA2_SEC_ERR("Invalid session struct");
		return -EINVAL;
	}

	memset(session, 0, sizeof(dpaa2_sec_session));
	/* Default IV length = 0 */
	session->iv.length = 0;

	/* Cipher Only */
	if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER && xform->next == NULL) {
		ret = dpaa2_sec_cipher_init(dev, xform, session);

	/* Authentication Only */
	} else if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
		   xform->next == NULL) {
		ret = dpaa2_sec_auth_init(dev, xform, session);

	/* Cipher then Authenticate */
	} else if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
		   xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		session->ext_params.aead_ctxt.auth_cipher_text = true;
		if (xform->cipher.algo == RTE_CRYPTO_CIPHER_NULL)
			ret = dpaa2_sec_auth_init(dev, xform, session);
		else if (xform->next->auth.algo == RTE_CRYPTO_AUTH_NULL)
			ret = dpaa2_sec_cipher_init(dev, xform, session);
		else
			ret = dpaa2_sec_aead_chain_init(dev, xform, session);
	/* Authenticate then Cipher */
	} else if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
		   xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		session->ext_params.aead_ctxt.auth_cipher_text = false;
		if (xform->auth.algo == RTE_CRYPTO_AUTH_NULL)
			ret = dpaa2_sec_cipher_init(dev, xform, session);
		else if (xform->next->cipher.algo == RTE_CRYPTO_CIPHER_NULL)
			ret = dpaa2_sec_auth_init(dev, xform, session);
		else
			ret = dpaa2_sec_aead_chain_init(dev, xform, session);
	/* AEAD operation for AES-GCM kind of Algorithms */
	} else if (xform->type == RTE_CRYPTO_SYM_XFORM_AEAD &&
		   xform->next == NULL) {
		ret = dpaa2_sec_aead_init(dev, xform, session);

	} else {
		DPAA2_SEC_ERR("Invalid crypto type");
		return -EINVAL;
	}

	return ret;
}

#ifdef RTE_LIB_SECURITY
static int
dpaa2_sec_ipsec_aead_init(struct rte_crypto_aead_xform *aead_xform,
			dpaa2_sec_session *session,
			struct alginfo *aeaddata)
{
	PMD_INIT_FUNC_TRACE();

	session->aead_key.data = rte_zmalloc(NULL, aead_xform->key.length,
					       RTE_CACHE_LINE_SIZE);
	if (session->aead_key.data == NULL && aead_xform->key.length > 0) {
		DPAA2_SEC_ERR("No Memory for aead key");
		return -ENOMEM;
	}
	memcpy(session->aead_key.data, aead_xform->key.data,
	       aead_xform->key.length);

	session->digest_length = aead_xform->digest_length;
	session->aead_key.length = aead_xform->key.length;

	aeaddata->key = (size_t)session->aead_key.data;
	aeaddata->keylen = session->aead_key.length;
	aeaddata->key_enc_flags = 0;
	aeaddata->key_type = RTA_DATA_IMM;

	switch (aead_xform->algo) {
	case RTE_CRYPTO_AEAD_AES_GCM:
		switch (session->digest_length) {
		case 8:
			aeaddata->algtype = OP_PCL_IPSEC_AES_GCM8;
			break;
		case 12:
			aeaddata->algtype = OP_PCL_IPSEC_AES_GCM12;
			break;
		case 16:
			aeaddata->algtype = OP_PCL_IPSEC_AES_GCM16;
			break;
		default:
			DPAA2_SEC_ERR("Crypto: Undefined GCM digest %d",
				      session->digest_length);
			return -EINVAL;
		}
		aeaddata->algmode = OP_ALG_AAI_GCM;
		session->aead_alg = RTE_CRYPTO_AEAD_AES_GCM;
		break;
	case RTE_CRYPTO_AEAD_AES_CCM:
		switch (session->digest_length) {
		case 8:
			aeaddata->algtype = OP_PCL_IPSEC_AES_CCM8;
			break;
		case 12:
			aeaddata->algtype = OP_PCL_IPSEC_AES_CCM12;
			break;
		case 16:
			aeaddata->algtype = OP_PCL_IPSEC_AES_CCM16;
			break;
		default:
			DPAA2_SEC_ERR("Crypto: Undefined CCM digest %d",
				      session->digest_length);
			return -EINVAL;
		}
		aeaddata->algmode = OP_ALG_AAI_CCM;
		session->aead_alg = RTE_CRYPTO_AEAD_AES_CCM;
		break;
	default:
		DPAA2_SEC_ERR("Crypto: Undefined AEAD specified %u",
			      aead_xform->algo);
		return -ENOTSUP;
	}
	session->dir = (aead_xform->op == RTE_CRYPTO_AEAD_OP_ENCRYPT) ?
				DIR_ENC : DIR_DEC;

	return 0;
}

static int
dpaa2_sec_ipsec_proto_init(struct rte_crypto_cipher_xform *cipher_xform,
	struct rte_crypto_auth_xform *auth_xform,
	dpaa2_sec_session *session,
	struct alginfo *cipherdata,
	struct alginfo *authdata)
{
	if (cipher_xform) {
		session->cipher_key.data = rte_zmalloc(NULL,
						       cipher_xform->key.length,
						       RTE_CACHE_LINE_SIZE);
		if (session->cipher_key.data == NULL &&
				cipher_xform->key.length > 0) {
			DPAA2_SEC_ERR("No Memory for cipher key");
			return -ENOMEM;
		}

		session->cipher_key.length = cipher_xform->key.length;
		memcpy(session->cipher_key.data, cipher_xform->key.data,
				cipher_xform->key.length);
		session->cipher_alg = cipher_xform->algo;
	} else {
		session->cipher_key.data = NULL;
		session->cipher_key.length = 0;
		session->cipher_alg = RTE_CRYPTO_CIPHER_NULL;
	}

	if (auth_xform) {
		session->auth_key.data = rte_zmalloc(NULL,
						auth_xform->key.length,
						RTE_CACHE_LINE_SIZE);
		if (session->auth_key.data == NULL &&
				auth_xform->key.length > 0) {
			DPAA2_SEC_ERR("No Memory for auth key");
			return -ENOMEM;
		}
		session->auth_key.length = auth_xform->key.length;
		memcpy(session->auth_key.data, auth_xform->key.data,
				auth_xform->key.length);
		session->auth_alg = auth_xform->algo;
		session->digest_length = auth_xform->digest_length;
	} else {
		session->auth_key.data = NULL;
		session->auth_key.length = 0;
		session->auth_alg = RTE_CRYPTO_AUTH_NULL;
	}

	authdata->key = (size_t)session->auth_key.data;
	authdata->keylen = session->auth_key.length;
	authdata->key_enc_flags = 0;
	authdata->key_type = RTA_DATA_IMM;
	switch (session->auth_alg) {
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		authdata->algtype = OP_PCL_IPSEC_HMAC_SHA1_96;
		authdata->algmode = OP_ALG_AAI_HMAC;
		break;
	case RTE_CRYPTO_AUTH_MD5_HMAC:
		authdata->algtype = OP_PCL_IPSEC_HMAC_MD5_96;
		authdata->algmode = OP_ALG_AAI_HMAC;
		break;
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
		authdata->algtype = OP_PCL_IPSEC_HMAC_SHA2_256_128;
		authdata->algmode = OP_ALG_AAI_HMAC;
		if (session->digest_length != 16)
			DPAA2_SEC_WARN(
			"+++Using sha256-hmac truncated len is non-standard,"
			"it will not work with lookaside proto");
		break;
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
		authdata->algtype = OP_PCL_IPSEC_HMAC_SHA2_384_192;
		authdata->algmode = OP_ALG_AAI_HMAC;
		break;
	case RTE_CRYPTO_AUTH_SHA512_HMAC:
		authdata->algtype = OP_PCL_IPSEC_HMAC_SHA2_512_256;
		authdata->algmode = OP_ALG_AAI_HMAC;
		break;
	case RTE_CRYPTO_AUTH_AES_XCBC_MAC:
		authdata->algtype = OP_PCL_IPSEC_AES_XCBC_MAC_96;
		authdata->algmode = OP_ALG_AAI_XCBC_MAC;
		break;
	case RTE_CRYPTO_AUTH_AES_CMAC:
		authdata->algtype = OP_PCL_IPSEC_AES_CMAC_96;
		authdata->algmode = OP_ALG_AAI_CMAC;
		break;
	case RTE_CRYPTO_AUTH_NULL:
		authdata->algtype = OP_PCL_IPSEC_HMAC_NULL;
		break;
	case RTE_CRYPTO_AUTH_SHA224_HMAC:
	case RTE_CRYPTO_AUTH_SNOW3G_UIA2:
	case RTE_CRYPTO_AUTH_SHA1:
	case RTE_CRYPTO_AUTH_SHA256:
	case RTE_CRYPTO_AUTH_SHA512:
	case RTE_CRYPTO_AUTH_SHA224:
	case RTE_CRYPTO_AUTH_SHA384:
	case RTE_CRYPTO_AUTH_MD5:
	case RTE_CRYPTO_AUTH_AES_GMAC:
	case RTE_CRYPTO_AUTH_KASUMI_F9:
	case RTE_CRYPTO_AUTH_AES_CBC_MAC:
	case RTE_CRYPTO_AUTH_ZUC_EIA3:
		DPAA2_SEC_ERR("Crypto: Unsupported auth alg %u",
			      session->auth_alg);
		return -ENOTSUP;
	default:
		DPAA2_SEC_ERR("Crypto: Undefined Auth specified %u",
			      session->auth_alg);
		return -ENOTSUP;
	}
	cipherdata->key = (size_t)session->cipher_key.data;
	cipherdata->keylen = session->cipher_key.length;
	cipherdata->key_enc_flags = 0;
	cipherdata->key_type = RTA_DATA_IMM;

	switch (session->cipher_alg) {
	case RTE_CRYPTO_CIPHER_AES_CBC:
		cipherdata->algtype = OP_PCL_IPSEC_AES_CBC;
		cipherdata->algmode = OP_ALG_AAI_CBC;
		break;
	case RTE_CRYPTO_CIPHER_3DES_CBC:
		cipherdata->algtype = OP_PCL_IPSEC_3DES;
		cipherdata->algmode = OP_ALG_AAI_CBC;
		break;
	case RTE_CRYPTO_CIPHER_DES_CBC:
		cipherdata->algtype = OP_PCL_IPSEC_DES;
		cipherdata->algmode = OP_ALG_AAI_CBC;
		break;
	case RTE_CRYPTO_CIPHER_AES_CTR:
		cipherdata->algtype = OP_PCL_IPSEC_AES_CTR;
		cipherdata->algmode = OP_ALG_AAI_CTR;
		break;
	case RTE_CRYPTO_CIPHER_NULL:
		cipherdata->algtype = OP_PCL_IPSEC_NULL;
		break;
	case RTE_CRYPTO_CIPHER_SNOW3G_UEA2:
	case RTE_CRYPTO_CIPHER_ZUC_EEA3:
	case RTE_CRYPTO_CIPHER_3DES_ECB:
	case RTE_CRYPTO_CIPHER_3DES_CTR:
	case RTE_CRYPTO_CIPHER_AES_ECB:
	case RTE_CRYPTO_CIPHER_KASUMI_F8:
		DPAA2_SEC_ERR("Crypto: Unsupported Cipher alg %u",
			      session->cipher_alg);
		return -ENOTSUP;
	default:
		DPAA2_SEC_ERR("Crypto: Undefined Cipher specified %u",
			      session->cipher_alg);
		return -ENOTSUP;
	}

	return 0;
}

static int
dpaa2_sec_set_ipsec_session(struct rte_cryptodev *dev,
			    struct rte_security_session_conf *conf,
			    void *sess)
{
	struct rte_security_ipsec_xform *ipsec_xform = &conf->ipsec;
	struct rte_crypto_cipher_xform *cipher_xform = NULL;
	struct rte_crypto_auth_xform *auth_xform = NULL;
	struct rte_crypto_aead_xform *aead_xform = NULL;
	dpaa2_sec_session *session = (dpaa2_sec_session *)sess;
	struct ctxt_priv *priv;
	struct alginfo authdata, cipherdata;
	int bufsize;
	struct sec_flow_context *flc;
	struct dpaa2_sec_dev_private *dev_priv = dev->data->dev_private;
	int ret = -1;

	PMD_INIT_FUNC_TRACE();

	priv = (struct ctxt_priv *)rte_zmalloc(NULL,
				sizeof(struct ctxt_priv) +
				sizeof(struct sec_flc_desc),
				RTE_CACHE_LINE_SIZE);

	if (priv == NULL) {
		DPAA2_SEC_ERR("No memory for priv CTXT");
		return -ENOMEM;
	}

	priv->fle_pool = dev_priv->fle_pool;
	flc = &priv->flc_desc[0].flc;

	memset(session, 0, sizeof(dpaa2_sec_session));

	if (conf->crypto_xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		cipher_xform = &conf->crypto_xform->cipher;
		if (conf->crypto_xform->next)
			auth_xform = &conf->crypto_xform->next->auth;
		ret = dpaa2_sec_ipsec_proto_init(cipher_xform, auth_xform,
					session, &cipherdata, &authdata);
	} else if (conf->crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		auth_xform = &conf->crypto_xform->auth;
		if (conf->crypto_xform->next)
			cipher_xform = &conf->crypto_xform->next->cipher;
		ret = dpaa2_sec_ipsec_proto_init(cipher_xform, auth_xform,
					session, &cipherdata, &authdata);
	} else if (conf->crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		aead_xform = &conf->crypto_xform->aead;
		ret = dpaa2_sec_ipsec_aead_init(aead_xform,
					session, &cipherdata);
		authdata.keylen = 0;
		authdata.algtype = 0;
	} else {
		DPAA2_SEC_ERR("XFORM not specified");
		ret = -EINVAL;
		goto out;
	}
	if (ret) {
		DPAA2_SEC_ERR("Failed to process xform");
		goto out;
	}

	session->ctxt_type = DPAA2_SEC_IPSEC;
	if (ipsec_xform->direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS) {
		uint8_t *hdr = NULL;
		struct ip ip4_hdr;
		struct rte_ipv6_hdr ip6_hdr;
		struct ipsec_encap_pdb encap_pdb;

		flc->dhr = SEC_FLC_DHR_OUTBOUND;
		/* For Sec Proto only one descriptor is required. */
		memset(&encap_pdb, 0, sizeof(struct ipsec_encap_pdb));

		/* copy algo specific data to PDB */
		switch (cipherdata.algtype) {
		case OP_PCL_IPSEC_AES_CTR:
			encap_pdb.ctr.ctr_initial = 0x00000001;
			encap_pdb.ctr.ctr_nonce = ipsec_xform->salt;
			break;
		case OP_PCL_IPSEC_AES_GCM8:
		case OP_PCL_IPSEC_AES_GCM12:
		case OP_PCL_IPSEC_AES_GCM16:
			memcpy(encap_pdb.gcm.salt,
				(uint8_t *)&(ipsec_xform->salt), 4);
			break;
		}

		encap_pdb.options = (IPVERSION << PDBNH_ESP_ENCAP_SHIFT) |
			PDBOPTS_ESP_OIHI_PDB_INL |
			PDBOPTS_ESP_IVSRC |
			PDBHMO_ESP_ENCAP_DTTL |
			PDBHMO_ESP_SNR;
		if (ipsec_xform->options.esn)
			encap_pdb.options |= PDBOPTS_ESP_ESN;
		encap_pdb.spi = ipsec_xform->spi;
		session->dir = DIR_ENC;
		if (ipsec_xform->tunnel.type ==
				RTE_SECURITY_IPSEC_TUNNEL_IPV4) {
			encap_pdb.ip_hdr_len = sizeof(struct ip);
			ip4_hdr.ip_v = IPVERSION;
			ip4_hdr.ip_hl = 5;
			ip4_hdr.ip_len = rte_cpu_to_be_16(sizeof(ip4_hdr));
			ip4_hdr.ip_tos = ipsec_xform->tunnel.ipv4.dscp;
			ip4_hdr.ip_id = 0;
			ip4_hdr.ip_off = 0;
			ip4_hdr.ip_ttl = ipsec_xform->tunnel.ipv4.ttl;
			ip4_hdr.ip_p = IPPROTO_ESP;
			ip4_hdr.ip_sum = 0;
			ip4_hdr.ip_src = ipsec_xform->tunnel.ipv4.src_ip;
			ip4_hdr.ip_dst = ipsec_xform->tunnel.ipv4.dst_ip;
			ip4_hdr.ip_sum = calc_chksum((uint16_t *)(void *)
					&ip4_hdr, sizeof(struct ip));
			hdr = (uint8_t *)&ip4_hdr;
		} else if (ipsec_xform->tunnel.type ==
				RTE_SECURITY_IPSEC_TUNNEL_IPV6) {
			ip6_hdr.vtc_flow = rte_cpu_to_be_32(
				DPAA2_IPv6_DEFAULT_VTC_FLOW |
				((ipsec_xform->tunnel.ipv6.dscp <<
					RTE_IPV6_HDR_TC_SHIFT) &
					RTE_IPV6_HDR_TC_MASK) |
				((ipsec_xform->tunnel.ipv6.flabel <<
					RTE_IPV6_HDR_FL_SHIFT) &
					RTE_IPV6_HDR_FL_MASK));
			/* Payload length will be updated by HW */
			ip6_hdr.payload_len = 0;
			ip6_hdr.hop_limits =
					ipsec_xform->tunnel.ipv6.hlimit;
			ip6_hdr.proto = (ipsec_xform->proto ==
					RTE_SECURITY_IPSEC_SA_PROTO_ESP) ?
					IPPROTO_ESP : IPPROTO_AH;
			memcpy(&ip6_hdr.src_addr,
				&ipsec_xform->tunnel.ipv6.src_addr, 16);
			memcpy(&ip6_hdr.dst_addr,
				&ipsec_xform->tunnel.ipv6.dst_addr, 16);
			encap_pdb.ip_hdr_len = sizeof(struct rte_ipv6_hdr);
			hdr = (uint8_t *)&ip6_hdr;
		}

		bufsize = cnstr_shdsc_ipsec_new_encap(priv->flc_desc[0].desc,
				1, 0, (rta_sec_era >= RTA_SEC_ERA_10) ?
				SHR_WAIT : SHR_SERIAL, &encap_pdb,
				hdr, &cipherdata, &authdata);
	} else if (ipsec_xform->direction ==
			RTE_SECURITY_IPSEC_SA_DIR_INGRESS) {
		struct ipsec_decap_pdb decap_pdb;

		flc->dhr = SEC_FLC_DHR_INBOUND;
		memset(&decap_pdb, 0, sizeof(struct ipsec_decap_pdb));
		/* copy algo specific data to PDB */
		switch (cipherdata.algtype) {
		case OP_PCL_IPSEC_AES_CTR:
			decap_pdb.ctr.ctr_initial = 0x00000001;
			decap_pdb.ctr.ctr_nonce = ipsec_xform->salt;
			break;
		case OP_PCL_IPSEC_AES_GCM8:
		case OP_PCL_IPSEC_AES_GCM12:
		case OP_PCL_IPSEC_AES_GCM16:
			memcpy(decap_pdb.gcm.salt,
				(uint8_t *)&(ipsec_xform->salt), 4);
			break;
		}

		decap_pdb.options = (ipsec_xform->tunnel.type ==
				RTE_SECURITY_IPSEC_TUNNEL_IPV4) ?
				sizeof(struct ip) << 16 :
				sizeof(struct rte_ipv6_hdr) << 16;
		if (ipsec_xform->options.esn)
			decap_pdb.options |= PDBOPTS_ESP_ESN;

		if (ipsec_xform->replay_win_sz) {
			uint32_t win_sz;
			win_sz = rte_align32pow2(ipsec_xform->replay_win_sz);

			if (rta_sec_era < RTA_SEC_ERA_10 && win_sz > 128) {
				DPAA2_SEC_INFO("Max Anti replay Win sz = 128");
				win_sz = 128;
			}
			switch (win_sz) {
			case 1:
			case 2:
			case 4:
			case 8:
			case 16:
			case 32:
				decap_pdb.options |= PDBOPTS_ESP_ARS32;
				break;
			case 64:
				decap_pdb.options |= PDBOPTS_ESP_ARS64;
				break;
			case 256:
				decap_pdb.options |= PDBOPTS_ESP_ARS256;
				break;
			case 512:
				decap_pdb.options |= PDBOPTS_ESP_ARS512;
				break;
			case 1024:
				decap_pdb.options |= PDBOPTS_ESP_ARS1024;
				break;
			case 128:
			default:
				decap_pdb.options |= PDBOPTS_ESP_ARS128;
			}
		}
		session->dir = DIR_DEC;
		bufsize = cnstr_shdsc_ipsec_new_decap(priv->flc_desc[0].desc,
				1, 0, (rta_sec_era >= RTA_SEC_ERA_10) ?
				SHR_WAIT : SHR_SERIAL,
				&decap_pdb, &cipherdata, &authdata);
	} else
		goto out;

	if (bufsize < 0) {
		DPAA2_SEC_ERR("Crypto: Invalid buffer length");
		goto out;
	}

	flc->word1_sdl = (uint8_t)bufsize;

	/* Enable the stashing control bit */
	DPAA2_SET_FLC_RSC(flc);
	flc->word2_rflc_31_0 = lower_32_bits(
			(size_t)&(((struct dpaa2_sec_qp *)
			dev->data->queue_pairs[0])->rx_vq) | 0x14);
	flc->word3_rflc_63_32 = upper_32_bits(
			(size_t)&(((struct dpaa2_sec_qp *)
			dev->data->queue_pairs[0])->rx_vq));

	/* Set EWS bit i.e. enable write-safe */
	DPAA2_SET_FLC_EWS(flc);
	/* Set BS = 1 i.e reuse input buffers as output buffers */
	DPAA2_SET_FLC_REUSE_BS(flc);
	/* Set FF = 10; reuse input buffers if they provide sufficient space */
	DPAA2_SET_FLC_REUSE_FF(flc);

	session->ctxt = priv;

	return 0;
out:
	rte_free(session->auth_key.data);
	rte_free(session->cipher_key.data);
	rte_free(priv);
	return ret;
}

static int
dpaa2_sec_set_pdcp_session(struct rte_cryptodev *dev,
			   struct rte_security_session_conf *conf,
			   void *sess)
{
	struct rte_security_pdcp_xform *pdcp_xform = &conf->pdcp;
	struct rte_crypto_sym_xform *xform = conf->crypto_xform;
	struct rte_crypto_auth_xform *auth_xform = NULL;
	struct rte_crypto_cipher_xform *cipher_xform = NULL;
	dpaa2_sec_session *session = (dpaa2_sec_session *)sess;
	struct ctxt_priv *priv;
	struct dpaa2_sec_dev_private *dev_priv = dev->data->dev_private;
	struct alginfo authdata, cipherdata;
	struct alginfo *p_authdata = NULL;
	int bufsize = -1;
	struct sec_flow_context *flc;
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	int swap = true;
#else
	int swap = false;
#endif

	PMD_INIT_FUNC_TRACE();

	memset(session, 0, sizeof(dpaa2_sec_session));

	priv = (struct ctxt_priv *)rte_zmalloc(NULL,
				sizeof(struct ctxt_priv) +
				sizeof(struct sec_flc_desc),
				RTE_CACHE_LINE_SIZE);

	if (priv == NULL) {
		DPAA2_SEC_ERR("No memory for priv CTXT");
		return -ENOMEM;
	}

	priv->fle_pool = dev_priv->fle_pool;
	flc = &priv->flc_desc[0].flc;

	/* find xfrm types */
	if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		cipher_xform = &xform->cipher;
		if (xform->next != NULL) {
			session->ext_params.aead_ctxt.auth_cipher_text = true;
			auth_xform = &xform->next->auth;
		}
	} else if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		auth_xform = &xform->auth;
		if (xform->next != NULL) {
			session->ext_params.aead_ctxt.auth_cipher_text = false;
			cipher_xform = &xform->next->cipher;
		}
	} else {
		DPAA2_SEC_ERR("Invalid crypto type");
		return -EINVAL;
	}

	session->ctxt_type = DPAA2_SEC_PDCP;
	if (cipher_xform) {
		session->cipher_key.data = rte_zmalloc(NULL,
					       cipher_xform->key.length,
					       RTE_CACHE_LINE_SIZE);
		if (session->cipher_key.data == NULL &&
				cipher_xform->key.length > 0) {
			DPAA2_SEC_ERR("No Memory for cipher key");
			rte_free(priv);
			return -ENOMEM;
		}
		session->cipher_key.length = cipher_xform->key.length;
		memcpy(session->cipher_key.data, cipher_xform->key.data,
			cipher_xform->key.length);
		session->dir =
			(cipher_xform->op == RTE_CRYPTO_CIPHER_OP_ENCRYPT) ?
					DIR_ENC : DIR_DEC;
		session->cipher_alg = cipher_xform->algo;
	} else {
		session->cipher_key.data = NULL;
		session->cipher_key.length = 0;
		session->cipher_alg = RTE_CRYPTO_CIPHER_NULL;
		session->dir = DIR_ENC;
	}

	session->pdcp.domain = pdcp_xform->domain;
	session->pdcp.bearer = pdcp_xform->bearer;
	session->pdcp.pkt_dir = pdcp_xform->pkt_dir;
	session->pdcp.sn_size = pdcp_xform->sn_size;
	session->pdcp.hfn = pdcp_xform->hfn;
	session->pdcp.hfn_threshold = pdcp_xform->hfn_threshold;
	session->pdcp.hfn_ovd = pdcp_xform->hfn_ovrd;
	/* hfv ovd offset location is stored in iv.offset value*/
	if (cipher_xform)
		session->pdcp.hfn_ovd_offset = cipher_xform->iv.offset;

	cipherdata.key = (size_t)session->cipher_key.data;
	cipherdata.keylen = session->cipher_key.length;
	cipherdata.key_enc_flags = 0;
	cipherdata.key_type = RTA_DATA_IMM;

	switch (session->cipher_alg) {
	case RTE_CRYPTO_CIPHER_SNOW3G_UEA2:
		cipherdata.algtype = PDCP_CIPHER_TYPE_SNOW;
		break;
	case RTE_CRYPTO_CIPHER_ZUC_EEA3:
		cipherdata.algtype = PDCP_CIPHER_TYPE_ZUC;
		break;
	case RTE_CRYPTO_CIPHER_AES_CTR:
		cipherdata.algtype = PDCP_CIPHER_TYPE_AES;
		break;
	case RTE_CRYPTO_CIPHER_NULL:
		cipherdata.algtype = PDCP_CIPHER_TYPE_NULL;
		break;
	default:
		DPAA2_SEC_ERR("Crypto: Undefined Cipher specified %u",
			      session->cipher_alg);
		goto out;
	}

	if (auth_xform) {
		session->auth_key.data = rte_zmalloc(NULL,
						     auth_xform->key.length,
						     RTE_CACHE_LINE_SIZE);
		if (!session->auth_key.data &&
		    auth_xform->key.length > 0) {
			DPAA2_SEC_ERR("No Memory for auth key");
			rte_free(session->cipher_key.data);
			rte_free(priv);
			return -ENOMEM;
		}
		session->auth_key.length = auth_xform->key.length;
		memcpy(session->auth_key.data, auth_xform->key.data,
		       auth_xform->key.length);
		session->auth_alg = auth_xform->algo;
	} else {
		session->auth_key.data = NULL;
		session->auth_key.length = 0;
		session->auth_alg = 0;
	}
	authdata.key = (size_t)session->auth_key.data;
	authdata.keylen = session->auth_key.length;
	authdata.key_enc_flags = 0;
	authdata.key_type = RTA_DATA_IMM;

	if (session->auth_alg) {
		switch (session->auth_alg) {
		case RTE_CRYPTO_AUTH_SNOW3G_UIA2:
			authdata.algtype = PDCP_AUTH_TYPE_SNOW;
			break;
		case RTE_CRYPTO_AUTH_ZUC_EIA3:
			authdata.algtype = PDCP_AUTH_TYPE_ZUC;
			break;
		case RTE_CRYPTO_AUTH_AES_CMAC:
			authdata.algtype = PDCP_AUTH_TYPE_AES;
			break;
		case RTE_CRYPTO_AUTH_NULL:
			authdata.algtype = PDCP_AUTH_TYPE_NULL;
			break;
		default:
			DPAA2_SEC_ERR("Crypto: Unsupported auth alg %u",
				      session->auth_alg);
			goto out;
		}

		p_authdata = &authdata;
	} else if (pdcp_xform->domain == RTE_SECURITY_PDCP_MODE_CONTROL) {
		DPAA2_SEC_ERR("Crypto: Integrity must for c-plane");
		goto out;
	}

	if (pdcp_xform->sdap_enabled) {
		int nb_keys_to_inline =
			rta_inline_pdcp_sdap_query(authdata.algtype,
					cipherdata.algtype,
					session->pdcp.sn_size,
					session->pdcp.hfn_ovd);
		if (nb_keys_to_inline >= 1) {
			cipherdata.key = DPAA2_VADDR_TO_IOVA(cipherdata.key);
			cipherdata.key_type = RTA_DATA_PTR;
		}
		if (nb_keys_to_inline >= 2) {
			authdata.key = DPAA2_VADDR_TO_IOVA(authdata.key);
			authdata.key_type = RTA_DATA_PTR;
		}
	} else {
		if (rta_inline_pdcp_query(authdata.algtype,
					cipherdata.algtype,
					session->pdcp.sn_size,
					session->pdcp.hfn_ovd)) {
			cipherdata.key = DPAA2_VADDR_TO_IOVA(cipherdata.key);
			cipherdata.key_type = RTA_DATA_PTR;
		}
	}

	if (pdcp_xform->domain == RTE_SECURITY_PDCP_MODE_CONTROL) {
		if (session->dir == DIR_ENC)
			bufsize = cnstr_shdsc_pdcp_c_plane_encap(
					priv->flc_desc[0].desc, 1, swap,
					pdcp_xform->hfn,
					session->pdcp.sn_size,
					pdcp_xform->bearer,
					pdcp_xform->pkt_dir,
					pdcp_xform->hfn_threshold,
					&cipherdata, &authdata,
					0);
		else if (session->dir == DIR_DEC)
			bufsize = cnstr_shdsc_pdcp_c_plane_decap(
					priv->flc_desc[0].desc, 1, swap,
					pdcp_xform->hfn,
					session->pdcp.sn_size,
					pdcp_xform->bearer,
					pdcp_xform->pkt_dir,
					pdcp_xform->hfn_threshold,
					&cipherdata, &authdata,
					0);

	} else if (pdcp_xform->domain == RTE_SECURITY_PDCP_MODE_SHORT_MAC) {
		bufsize = cnstr_shdsc_pdcp_short_mac(priv->flc_desc[0].desc,
						     1, swap, &authdata);
	} else {
		if (session->dir == DIR_ENC) {
			if (pdcp_xform->sdap_enabled)
				bufsize = cnstr_shdsc_pdcp_sdap_u_plane_encap(
					priv->flc_desc[0].desc, 1, swap,
					session->pdcp.sn_size,
					pdcp_xform->hfn,
					pdcp_xform->bearer,
					pdcp_xform->pkt_dir,
					pdcp_xform->hfn_threshold,
					&cipherdata, p_authdata, 0);
			else
				bufsize = cnstr_shdsc_pdcp_u_plane_encap(
					priv->flc_desc[0].desc, 1, swap,
					session->pdcp.sn_size,
					pdcp_xform->hfn,
					pdcp_xform->bearer,
					pdcp_xform->pkt_dir,
					pdcp_xform->hfn_threshold,
					&cipherdata, p_authdata, 0);
		} else if (session->dir == DIR_DEC) {
			if (pdcp_xform->sdap_enabled)
				bufsize = cnstr_shdsc_pdcp_sdap_u_plane_decap(
					priv->flc_desc[0].desc, 1, swap,
					session->pdcp.sn_size,
					pdcp_xform->hfn,
					pdcp_xform->bearer,
					pdcp_xform->pkt_dir,
					pdcp_xform->hfn_threshold,
					&cipherdata, p_authdata, 0);
			else
				bufsize = cnstr_shdsc_pdcp_u_plane_decap(
					priv->flc_desc[0].desc, 1, swap,
					session->pdcp.sn_size,
					pdcp_xform->hfn,
					pdcp_xform->bearer,
					pdcp_xform->pkt_dir,
					pdcp_xform->hfn_threshold,
					&cipherdata, p_authdata, 0);
		}
	}

	if (bufsize < 0) {
		DPAA2_SEC_ERR("Crypto: Invalid buffer length");
		goto out;
	}

	/* Enable the stashing control bit */
	DPAA2_SET_FLC_RSC(flc);
	flc->word2_rflc_31_0 = lower_32_bits(
			(size_t)&(((struct dpaa2_sec_qp *)
			dev->data->queue_pairs[0])->rx_vq) | 0x14);
	flc->word3_rflc_63_32 = upper_32_bits(
			(size_t)&(((struct dpaa2_sec_qp *)
			dev->data->queue_pairs[0])->rx_vq));

	flc->word1_sdl = (uint8_t)bufsize;

	/* TODO - check the perf impact or
	 * align as per descriptor type
	 * Set EWS bit i.e. enable write-safe
	 * DPAA2_SET_FLC_EWS(flc);
	 */

	/* Set BS = 1 i.e reuse input buffers as output buffers */
	DPAA2_SET_FLC_REUSE_BS(flc);
	/* Set FF = 10; reuse input buffers if they provide sufficient space */
	DPAA2_SET_FLC_REUSE_FF(flc);

	session->ctxt = priv;

	return 0;
out:
	rte_free(session->auth_key.data);
	rte_free(session->cipher_key.data);
	rte_free(priv);
	return -EINVAL;
}

static int
dpaa2_sec_security_session_create(void *dev,
				  struct rte_security_session_conf *conf,
				  struct rte_security_session *sess,
				  struct rte_mempool *mempool)
{
	void *sess_private_data;
	struct rte_cryptodev *cdev = (struct rte_cryptodev *)dev;
	int ret;

	if (rte_mempool_get(mempool, &sess_private_data)) {
		DPAA2_SEC_ERR("Couldn't get object from session mempool");
		return -ENOMEM;
	}

	switch (conf->protocol) {
	case RTE_SECURITY_PROTOCOL_IPSEC:
		ret = dpaa2_sec_set_ipsec_session(cdev, conf,
				sess_private_data);
		break;
	case RTE_SECURITY_PROTOCOL_MACSEC:
		return -ENOTSUP;
	case RTE_SECURITY_PROTOCOL_PDCP:
		ret = dpaa2_sec_set_pdcp_session(cdev, conf,
				sess_private_data);
		break;
	default:
		return -EINVAL;
	}
	if (ret != 0) {
		DPAA2_SEC_ERR("Failed to configure session parameters");
		/* Return session to mempool */
		rte_mempool_put(mempool, sess_private_data);
		return ret;
	}

	set_sec_session_private_data(sess, sess_private_data);

	return ret;
}

/** Clear the memory of session so it doesn't leave key material behind */
static int
dpaa2_sec_security_session_destroy(void *dev __rte_unused,
		struct rte_security_session *sess)
{
	PMD_INIT_FUNC_TRACE();
	void *sess_priv = get_sec_session_private_data(sess);

	dpaa2_sec_session *s = (dpaa2_sec_session *)sess_priv;

	if (sess_priv) {
		struct rte_mempool *sess_mp = rte_mempool_from_obj(sess_priv);

		rte_free(s->ctxt);
		rte_free(s->cipher_key.data);
		rte_free(s->auth_key.data);
		memset(s, 0, sizeof(dpaa2_sec_session));
		set_sec_session_private_data(sess, NULL);
		rte_mempool_put(sess_mp, sess_priv);
	}
	return 0;
}
#endif
static int
dpaa2_sec_sym_session_configure(struct rte_cryptodev *dev,
		struct rte_crypto_sym_xform *xform,
		struct rte_cryptodev_sym_session *sess,
		struct rte_mempool *mempool)
{
	void *sess_private_data;
	int ret;

	if (rte_mempool_get(mempool, &sess_private_data)) {
		DPAA2_SEC_ERR("Couldn't get object from session mempool");
		return -ENOMEM;
	}

	ret = dpaa2_sec_set_session_parameters(dev, xform, sess_private_data);
	if (ret != 0) {
		DPAA2_SEC_ERR("Failed to configure session parameters");
		/* Return session to mempool */
		rte_mempool_put(mempool, sess_private_data);
		return ret;
	}

	set_sym_session_private_data(sess, dev->driver_id,
		sess_private_data);

	return 0;
}

/** Clear the memory of session so it doesn't leave key material behind */
static void
dpaa2_sec_sym_session_clear(struct rte_cryptodev *dev,
		struct rte_cryptodev_sym_session *sess)
{
	PMD_INIT_FUNC_TRACE();
	uint8_t index = dev->driver_id;
	void *sess_priv = get_sym_session_private_data(sess, index);
	dpaa2_sec_session *s = (dpaa2_sec_session *)sess_priv;

	if (sess_priv) {
		rte_free(s->ctxt);
		rte_free(s->cipher_key.data);
		rte_free(s->auth_key.data);
		memset(s, 0, sizeof(dpaa2_sec_session));
		struct rte_mempool *sess_mp = rte_mempool_from_obj(sess_priv);
		set_sym_session_private_data(sess, index, NULL);
		rte_mempool_put(sess_mp, sess_priv);
	}
}

static int
dpaa2_sec_dev_configure(struct rte_cryptodev *dev __rte_unused,
			struct rte_cryptodev_config *config __rte_unused)
{
	PMD_INIT_FUNC_TRACE();

	return 0;
}

static int
dpaa2_sec_dev_start(struct rte_cryptodev *dev)
{
	struct dpaa2_sec_dev_private *priv = dev->data->dev_private;
	struct fsl_mc_io *dpseci = (struct fsl_mc_io *)priv->hw;
	struct dpseci_attr attr;
	struct dpaa2_queue *dpaa2_q;
	struct dpaa2_sec_qp **qp = (struct dpaa2_sec_qp **)
					dev->data->queue_pairs;
	struct dpseci_rx_queue_attr rx_attr;
	struct dpseci_tx_queue_attr tx_attr;
	int ret, i;

	PMD_INIT_FUNC_TRACE();

	memset(&attr, 0, sizeof(struct dpseci_attr));

	ret = dpseci_enable(dpseci, CMD_PRI_LOW, priv->token);
	if (ret) {
		DPAA2_SEC_ERR("DPSECI with HW_ID = %d ENABLE FAILED",
			      priv->hw_id);
		goto get_attr_failure;
	}
	ret = dpseci_get_attributes(dpseci, CMD_PRI_LOW, priv->token, &attr);
	if (ret) {
		DPAA2_SEC_ERR("DPSEC ATTRIBUTE READ FAILED, disabling DPSEC");
		goto get_attr_failure;
	}
	for (i = 0; i < attr.num_rx_queues && qp[i]; i++) {
		dpaa2_q = &qp[i]->rx_vq;
		dpseci_get_rx_queue(dpseci, CMD_PRI_LOW, priv->token, i,
				    &rx_attr);
		dpaa2_q->fqid = rx_attr.fqid;
		DPAA2_SEC_DEBUG("rx_fqid: %d", dpaa2_q->fqid);
	}
	for (i = 0; i < attr.num_tx_queues && qp[i]; i++) {
		dpaa2_q = &qp[i]->tx_vq;
		dpseci_get_tx_queue(dpseci, CMD_PRI_LOW, priv->token, i,
				    &tx_attr);
		dpaa2_q->fqid = tx_attr.fqid;
		DPAA2_SEC_DEBUG("tx_fqid: %d", dpaa2_q->fqid);
	}

	return 0;
get_attr_failure:
	dpseci_disable(dpseci, CMD_PRI_LOW, priv->token);
	return -1;
}

static void
dpaa2_sec_dev_stop(struct rte_cryptodev *dev)
{
	struct dpaa2_sec_dev_private *priv = dev->data->dev_private;
	struct fsl_mc_io *dpseci = (struct fsl_mc_io *)priv->hw;
	int ret;

	PMD_INIT_FUNC_TRACE();

	ret = dpseci_disable(dpseci, CMD_PRI_LOW, priv->token);
	if (ret) {
		DPAA2_SEC_ERR("Failure in disabling dpseci %d device",
			     priv->hw_id);
		return;
	}

	ret = dpseci_reset(dpseci, CMD_PRI_LOW, priv->token);
	if (ret < 0) {
		DPAA2_SEC_ERR("SEC Device cannot be reset:Error = %0x", ret);
		return;
	}
}

static int
dpaa2_sec_dev_close(struct rte_cryptodev *dev __rte_unused)
{
	PMD_INIT_FUNC_TRACE();

	return 0;
}

static void
dpaa2_sec_dev_infos_get(struct rte_cryptodev *dev,
			struct rte_cryptodev_info *info)
{
	struct dpaa2_sec_dev_private *internals = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();
	if (info != NULL) {
		info->max_nb_queue_pairs = internals->max_nb_queue_pairs;
		info->feature_flags = dev->feature_flags;
		info->capabilities = dpaa2_sec_capabilities;
		/* No limit of number of sessions */
		info->sym.max_nb_sessions = 0;
		info->driver_id = cryptodev_driver_id;
	}
}

static
void dpaa2_sec_stats_get(struct rte_cryptodev *dev,
			 struct rte_cryptodev_stats *stats)
{
	struct dpaa2_sec_dev_private *priv = dev->data->dev_private;
	struct fsl_mc_io dpseci;
	struct dpseci_sec_counters counters = {0};
	struct dpaa2_sec_qp **qp = (struct dpaa2_sec_qp **)
					dev->data->queue_pairs;
	int ret, i;

	PMD_INIT_FUNC_TRACE();
	if (stats == NULL) {
		DPAA2_SEC_ERR("Invalid stats ptr NULL");
		return;
	}
	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
		if (qp == NULL || qp[i] == NULL) {
			DPAA2_SEC_DEBUG("Uninitialised queue pair");
			continue;
		}

		stats->enqueued_count += qp[i]->tx_vq.tx_pkts;
		stats->dequeued_count += qp[i]->rx_vq.rx_pkts;
		stats->enqueue_err_count += qp[i]->tx_vq.err_pkts;
		stats->dequeue_err_count += qp[i]->rx_vq.err_pkts;
	}

	/* In case as secondary process access stats, MCP portal in priv-hw
	 * may have primary process address. Need the secondary process
	 * based MCP portal address for this object.
	 */
	dpseci.regs = dpaa2_get_mcp_ptr(MC_PORTAL_INDEX);
	ret = dpseci_get_sec_counters(&dpseci, CMD_PRI_LOW, priv->token,
				      &counters);
	if (ret) {
		DPAA2_SEC_ERR("SEC counters failed");
	} else {
		DPAA2_SEC_INFO("dpseci hardware stats:"
			    "\n\tNum of Requests Dequeued = %" PRIu64
			    "\n\tNum of Outbound Encrypt Requests = %" PRIu64
			    "\n\tNum of Inbound Decrypt Requests = %" PRIu64
			    "\n\tNum of Outbound Bytes Encrypted = %" PRIu64
			    "\n\tNum of Outbound Bytes Protected = %" PRIu64
			    "\n\tNum of Inbound Bytes Decrypted = %" PRIu64
			    "\n\tNum of Inbound Bytes Validated = %" PRIu64,
			    counters.dequeued_requests,
			    counters.ob_enc_requests,
			    counters.ib_dec_requests,
			    counters.ob_enc_bytes,
			    counters.ob_prot_bytes,
			    counters.ib_dec_bytes,
			    counters.ib_valid_bytes);
	}
}

static
void dpaa2_sec_stats_reset(struct rte_cryptodev *dev)
{
	int i;
	struct dpaa2_sec_qp **qp = (struct dpaa2_sec_qp **)
				   (dev->data->queue_pairs);

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
		if (qp[i] == NULL) {
			DPAA2_SEC_DEBUG("Uninitialised queue pair");
			continue;
		}
		qp[i]->tx_vq.rx_pkts = 0;
		qp[i]->tx_vq.tx_pkts = 0;
		qp[i]->tx_vq.err_pkts = 0;
		qp[i]->rx_vq.rx_pkts = 0;
		qp[i]->rx_vq.tx_pkts = 0;
		qp[i]->rx_vq.err_pkts = 0;
	}
}

static void __rte_hot
dpaa2_sec_process_parallel_event(struct qbman_swp *swp,
				 const struct qbman_fd *fd,
				 const struct qbman_result *dq,
				 struct dpaa2_queue *rxq,
				 struct rte_event *ev)
{
	/* Prefetching mbuf */
	rte_prefetch0((void *)(size_t)(DPAA2_GET_FD_ADDR(fd)-
		rte_dpaa2_bpid_info[DPAA2_GET_FD_BPID(fd)].meta_data_size));

	/* Prefetching ipsec crypto_op stored in priv data of mbuf */
	rte_prefetch0((void *)(size_t)(DPAA2_GET_FD_ADDR(fd)-64));

	ev->flow_id = rxq->ev.flow_id;
	ev->sub_event_type = rxq->ev.sub_event_type;
	ev->event_type = RTE_EVENT_TYPE_CRYPTODEV;
	ev->op = RTE_EVENT_OP_NEW;
	ev->sched_type = rxq->ev.sched_type;
	ev->queue_id = rxq->ev.queue_id;
	ev->priority = rxq->ev.priority;
	ev->event_ptr = sec_fd_to_mbuf(fd);

	qbman_swp_dqrr_consume(swp, dq);
}
static void
dpaa2_sec_process_atomic_event(struct qbman_swp *swp __rte_unused,
				 const struct qbman_fd *fd,
				 const struct qbman_result *dq,
				 struct dpaa2_queue *rxq,
				 struct rte_event *ev)
{
	uint8_t dqrr_index;
	struct rte_crypto_op *crypto_op = (struct rte_crypto_op *)ev->event_ptr;
	/* Prefetching mbuf */
	rte_prefetch0((void *)(size_t)(DPAA2_GET_FD_ADDR(fd)-
		rte_dpaa2_bpid_info[DPAA2_GET_FD_BPID(fd)].meta_data_size));

	/* Prefetching ipsec crypto_op stored in priv data of mbuf */
	rte_prefetch0((void *)(size_t)(DPAA2_GET_FD_ADDR(fd)-64));

	ev->flow_id = rxq->ev.flow_id;
	ev->sub_event_type = rxq->ev.sub_event_type;
	ev->event_type = RTE_EVENT_TYPE_CRYPTODEV;
	ev->op = RTE_EVENT_OP_NEW;
	ev->sched_type = rxq->ev.sched_type;
	ev->queue_id = rxq->ev.queue_id;
	ev->priority = rxq->ev.priority;

	ev->event_ptr = sec_fd_to_mbuf(fd);
	dqrr_index = qbman_get_dqrr_idx(dq);
	*dpaa2_seqn(crypto_op->sym->m_src) = dqrr_index + 1;
	DPAA2_PER_LCORE_DQRR_SIZE++;
	DPAA2_PER_LCORE_DQRR_HELD |= 1 << dqrr_index;
	DPAA2_PER_LCORE_DQRR_MBUF(dqrr_index) = crypto_op->sym->m_src;
}

int
dpaa2_sec_eventq_attach(const struct rte_cryptodev *dev,
		int qp_id,
		struct dpaa2_dpcon_dev *dpcon,
		const struct rte_event *event)
{
	struct dpaa2_sec_dev_private *priv = dev->data->dev_private;
	struct fsl_mc_io *dpseci = (struct fsl_mc_io *)priv->hw;
	struct dpaa2_sec_qp *qp = dev->data->queue_pairs[qp_id];
	struct dpseci_rx_queue_cfg cfg;
	uint8_t priority;
	int ret;

	if (event->sched_type == RTE_SCHED_TYPE_PARALLEL)
		qp->rx_vq.cb = dpaa2_sec_process_parallel_event;
	else if (event->sched_type == RTE_SCHED_TYPE_ATOMIC)
		qp->rx_vq.cb = dpaa2_sec_process_atomic_event;
	else
		return -EINVAL;

	priority = (RTE_EVENT_DEV_PRIORITY_LOWEST / event->priority) *
		   (dpcon->num_priorities - 1);

	memset(&cfg, 0, sizeof(struct dpseci_rx_queue_cfg));
	cfg.options = DPSECI_QUEUE_OPT_DEST;
	cfg.dest_cfg.dest_type = DPSECI_DEST_DPCON;
	cfg.dest_cfg.dest_id = dpcon->dpcon_id;
	cfg.dest_cfg.priority = priority;

	cfg.options |= DPSECI_QUEUE_OPT_USER_CTX;
	cfg.user_ctx = (size_t)(qp);
	if (event->sched_type == RTE_SCHED_TYPE_ATOMIC) {
		cfg.options |= DPSECI_QUEUE_OPT_ORDER_PRESERVATION;
		cfg.order_preservation_en = 1;
	}
	ret = dpseci_set_rx_queue(dpseci, CMD_PRI_LOW, priv->token,
				  qp_id, &cfg);
	if (ret) {
		RTE_LOG(ERR, PMD, "Error in dpseci_set_queue: ret: %d\n", ret);
		return ret;
	}

	memcpy(&qp->rx_vq.ev, event, sizeof(struct rte_event));

	return 0;
}

int
dpaa2_sec_eventq_detach(const struct rte_cryptodev *dev,
			int qp_id)
{
	struct dpaa2_sec_dev_private *priv = dev->data->dev_private;
	struct fsl_mc_io *dpseci = (struct fsl_mc_io *)priv->hw;
	struct dpseci_rx_queue_cfg cfg;
	int ret;

	memset(&cfg, 0, sizeof(struct dpseci_rx_queue_cfg));
	cfg.options = DPSECI_QUEUE_OPT_DEST;
	cfg.dest_cfg.dest_type = DPSECI_DEST_NONE;

	ret = dpseci_set_rx_queue(dpseci, CMD_PRI_LOW, priv->token,
				  qp_id, &cfg);
	if (ret)
		RTE_LOG(ERR, PMD, "Error in dpseci_set_queue: ret: %d\n", ret);

	return ret;
}

static struct rte_cryptodev_ops crypto_ops = {
	.dev_configure	      = dpaa2_sec_dev_configure,
	.dev_start	      = dpaa2_sec_dev_start,
	.dev_stop	      = dpaa2_sec_dev_stop,
	.dev_close	      = dpaa2_sec_dev_close,
	.dev_infos_get        = dpaa2_sec_dev_infos_get,
	.stats_get	      = dpaa2_sec_stats_get,
	.stats_reset	      = dpaa2_sec_stats_reset,
	.queue_pair_setup     = dpaa2_sec_queue_pair_setup,
	.queue_pair_release   = dpaa2_sec_queue_pair_release,
	.sym_session_get_size     = dpaa2_sec_sym_session_get_size,
	.sym_session_configure    = dpaa2_sec_sym_session_configure,
	.sym_session_clear        = dpaa2_sec_sym_session_clear,
};

#ifdef RTE_LIB_SECURITY
static const struct rte_security_capability *
dpaa2_sec_capabilities_get(void *device __rte_unused)
{
	return dpaa2_sec_security_cap;
}

static const struct rte_security_ops dpaa2_sec_security_ops = {
	.session_create = dpaa2_sec_security_session_create,
	.session_update = NULL,
	.session_stats_get = NULL,
	.session_destroy = dpaa2_sec_security_session_destroy,
	.set_pkt_metadata = NULL,
	.capabilities_get = dpaa2_sec_capabilities_get
};
#endif

static int
dpaa2_sec_uninit(const struct rte_cryptodev *dev)
{
	struct dpaa2_sec_dev_private *priv = dev->data->dev_private;
	struct fsl_mc_io *dpseci = (struct fsl_mc_io *)priv->hw;
	int ret;

	PMD_INIT_FUNC_TRACE();

	/* Function is reverse of dpaa2_sec_dev_init.
	 * It does the following:
	 * 1. Detach a DPSECI from attached resources i.e. buffer pools, dpbp_id
	 * 2. Close the DPSECI device
	 * 3. Free the allocated resources.
	 */

	/*Close the device at underlying layer*/
	ret = dpseci_close(dpseci, CMD_PRI_LOW, priv->token);
	if (ret) {
		DPAA2_SEC_ERR("Failure closing dpseci device: err(%d)", ret);
		return -1;
	}

	/*Free the allocated memory for ethernet private data and dpseci*/
	priv->hw = NULL;
	rte_free(dpseci);
	rte_free(dev->security_ctx);
	rte_mempool_free(priv->fle_pool);

	DPAA2_SEC_INFO("Closing DPAA2_SEC device %s on numa socket %u",
		       dev->data->name, rte_socket_id());

	return 0;
}

static int
dpaa2_sec_dev_init(struct rte_cryptodev *cryptodev)
{
	struct dpaa2_sec_dev_private *internals;
	struct rte_device *dev = cryptodev->device;
	struct rte_dpaa2_device *dpaa2_dev;
#ifdef RTE_LIB_SECURITY
	struct rte_security_ctx *security_instance;
#endif
	struct fsl_mc_io *dpseci;
	uint16_t token;
	struct dpseci_attr attr;
	int retcode, hw_id;
	char str[30];

	PMD_INIT_FUNC_TRACE();
	dpaa2_dev = container_of(dev, struct rte_dpaa2_device, device);
	hw_id = dpaa2_dev->object_id;

	cryptodev->driver_id = cryptodev_driver_id;
	cryptodev->dev_ops = &crypto_ops;

	cryptodev->enqueue_burst = dpaa2_sec_enqueue_burst;
	cryptodev->dequeue_burst = dpaa2_sec_dequeue_burst;
	cryptodev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
			RTE_CRYPTODEV_FF_HW_ACCELERATED |
			RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
			RTE_CRYPTODEV_FF_SECURITY |
			RTE_CRYPTODEV_FF_IN_PLACE_SGL |
			RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT |
			RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT |
			RTE_CRYPTODEV_FF_OOP_LB_IN_SGL_OUT |
			RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT;

	internals = cryptodev->data->dev_private;

	/*
	 * For secondary processes, we don't initialise any further as primary
	 * has already done this work. Only check we don't need a different
	 * RX function
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		DPAA2_SEC_DEBUG("Device already init by primary process");
		return 0;
	}
#ifdef RTE_LIB_SECURITY
	/* Initialize security_ctx only for primary process*/
	security_instance = rte_malloc("rte_security_instances_ops",
				sizeof(struct rte_security_ctx), 0);
	if (security_instance == NULL)
		return -ENOMEM;
	security_instance->device = (void *)cryptodev;
	security_instance->ops = &dpaa2_sec_security_ops;
	security_instance->sess_cnt = 0;
	cryptodev->security_ctx = security_instance;
#endif
	/*Open the rte device via MC and save the handle for further use*/
	dpseci = (struct fsl_mc_io *)rte_calloc(NULL, 1,
				sizeof(struct fsl_mc_io), 0);
	if (!dpseci) {
		DPAA2_SEC_ERR(
			"Error in allocating the memory for dpsec object");
		return -ENOMEM;
	}
	dpseci->regs = dpaa2_get_mcp_ptr(MC_PORTAL_INDEX);

	retcode = dpseci_open(dpseci, CMD_PRI_LOW, hw_id, &token);
	if (retcode != 0) {
		DPAA2_SEC_ERR("Cannot open the dpsec device: Error = %x",
			      retcode);
		goto init_error;
	}
	retcode = dpseci_get_attributes(dpseci, CMD_PRI_LOW, token, &attr);
	if (retcode != 0) {
		DPAA2_SEC_ERR(
			     "Cannot get dpsec device attributed: Error = %x",
			     retcode);
		goto init_error;
	}
	snprintf(cryptodev->data->name, sizeof(cryptodev->data->name),
			"dpsec-%u", hw_id);

	internals->max_nb_queue_pairs = attr.num_tx_queues;
	cryptodev->data->nb_queue_pairs = internals->max_nb_queue_pairs;
	internals->hw = dpseci;
	internals->token = token;

	snprintf(str, sizeof(str), "sec_fle_pool_p%d_%d",
			getpid(), cryptodev->data->dev_id);
	internals->fle_pool = rte_mempool_create((const char *)str,
			FLE_POOL_NUM_BUFS,
			FLE_POOL_BUF_SIZE,
			FLE_POOL_CACHE_SIZE, 0,
			NULL, NULL, NULL, NULL,
			SOCKET_ID_ANY, 0);
	if (!internals->fle_pool) {
		DPAA2_SEC_ERR("Mempool (%s) creation failed", str);
		goto init_error;
	}

	DPAA2_SEC_INFO("driver %s: created", cryptodev->data->name);
	return 0;

init_error:
	DPAA2_SEC_ERR("driver %s: create failed", cryptodev->data->name);

	/* dpaa2_sec_uninit(crypto_dev_name); */
	return -EFAULT;
}

static int
cryptodev_dpaa2_sec_probe(struct rte_dpaa2_driver *dpaa2_drv __rte_unused,
			  struct rte_dpaa2_device *dpaa2_dev)
{
	struct rte_cryptodev *cryptodev;
	char cryptodev_name[RTE_CRYPTODEV_NAME_MAX_LEN];

	int retval;

	snprintf(cryptodev_name, sizeof(cryptodev_name), "dpsec-%d",
			dpaa2_dev->object_id);

	cryptodev = rte_cryptodev_pmd_allocate(cryptodev_name, rte_socket_id());
	if (cryptodev == NULL)
		return -ENOMEM;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		cryptodev->data->dev_private = rte_zmalloc_socket(
					"cryptodev private structure",
					sizeof(struct dpaa2_sec_dev_private),
					RTE_CACHE_LINE_SIZE,
					rte_socket_id());

		if (cryptodev->data->dev_private == NULL)
			rte_panic("Cannot allocate memzone for private "
				  "device data");
	}

	dpaa2_dev->cryptodev = cryptodev;
	cryptodev->device = &dpaa2_dev->device;

	/* init user callbacks */
	TAILQ_INIT(&(cryptodev->link_intr_cbs));

	if (dpaa2_svr_family == SVR_LX2160A)
		rta_set_sec_era(RTA_SEC_ERA_10);
	else
		rta_set_sec_era(RTA_SEC_ERA_8);

	DPAA2_SEC_INFO("2-SEC ERA is %d", rta_get_sec_era());

	/* Invoke PMD device initialization function */
	retval = dpaa2_sec_dev_init(cryptodev);
	if (retval == 0)
		return 0;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_free(cryptodev->data->dev_private);

	cryptodev->attached = RTE_CRYPTODEV_DETACHED;

	return -ENXIO;
}

static int
cryptodev_dpaa2_sec_remove(struct rte_dpaa2_device *dpaa2_dev)
{
	struct rte_cryptodev *cryptodev;
	int ret;

	cryptodev = dpaa2_dev->cryptodev;
	if (cryptodev == NULL)
		return -ENODEV;

	ret = dpaa2_sec_uninit(cryptodev);
	if (ret)
		return ret;

	return rte_cryptodev_pmd_destroy(cryptodev);
}

static struct rte_dpaa2_driver rte_dpaa2_sec_driver = {
	.drv_flags = RTE_DPAA2_DRV_IOVA_AS_VA,
	.drv_type = DPAA2_CRYPTO,
	.driver = {
		.name = "DPAA2 SEC PMD"
	},
	.probe = cryptodev_dpaa2_sec_probe,
	.remove = cryptodev_dpaa2_sec_remove,
};

static struct cryptodev_driver dpaa2_sec_crypto_drv;

RTE_PMD_REGISTER_DPAA2(CRYPTODEV_NAME_DPAA2_SEC_PMD, rte_dpaa2_sec_driver);
RTE_PMD_REGISTER_CRYPTO_DRIVER(dpaa2_sec_crypto_drv,
		rte_dpaa2_sec_driver.driver, cryptodev_driver_id);
RTE_LOG_REGISTER(dpaa2_logtype_sec, pmd.crypto.dpaa2, NOTICE);
