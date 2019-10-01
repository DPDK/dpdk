/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_crypto.h>
#include <rte_cryptodev.h>
#include <rte_cycles.h>
#include <rte_errno.h>

#include "nitrox_sym_reqmgr.h"
#include "nitrox_logs.h"

#define PENDING_SIG 0xFFFFFFFFFFFFFFFFUL
#define CMD_TIMEOUT 2

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

struct nitrox_softreq {
	struct nitrox_crypto_ctx *ctx;
	struct rte_crypto_op *op;
	struct nps_pkt_instr instr;
	struct resp_hdr resp;
	uint64_t timeout;
	rte_iova_t iova;
};

static void
softreq_init(struct nitrox_softreq *sr, rte_iova_t iova)
{
	memset(sr, 0, sizeof(*sr));
	sr->iova = iova;
}

static int
process_cipher_auth_data(struct nitrox_softreq *sr)
{
	RTE_SET_USED(sr);
	return 0;
}

static int
process_softreq(struct nitrox_softreq *sr)
{
	struct nitrox_crypto_ctx *ctx = sr->ctx;
	int err = 0;

	switch (ctx->nitrox_chain) {
	case NITROX_CHAIN_CIPHER_AUTH:
	case NITROX_CHAIN_AUTH_CIPHER:
		err = process_cipher_auth_data(sr);
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
	RTE_SET_USED(qno);
	softreq_init(sr, sr->iova);
	sr->ctx = ctx;
	sr->op = op;
	process_softreq(sr);
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
		err = 0;
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
