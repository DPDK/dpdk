/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#ifndef _RTE_OCTEONTX_ZIP_VF_H_
#define _RTE_OCTEONTX_ZIP_VF_H_

#include <unistd.h>

#include <rte_bus_pci.h>
#include <rte_comp.h>
#include <rte_compressdev.h>
#include <rte_compressdev_pmd.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_spinlock.h>

#include <zip_regs.h>

int octtx_zip_logtype_driver;

/* ZIP VF Control/Status registers (CSRs): */
/* VF_BAR0: */
#define ZIP_VQ_ENA              (0x10)
#define ZIP_VQ_SBUF_ADDR        (0x20)
#define ZIP_VF_PF_MBOXX(x)      (0x400 | (x)<<3)
#define ZIP_VQ_DOORBELL         (0x1000)

/**< Vendor ID */
#define PCI_VENDOR_ID_CAVIUM	0x177D
/**< PCI device id of ZIP VF */
#define PCI_DEVICE_ID_OCTEONTX_ZIPVF	0xA037

/* maxmum number of zip vf devices */
#define ZIP_MAX_VFS 8

/* max size of one chunk */
#define ZIP_MAX_CHUNK_SIZE	8192

/* each instruction is fixed 128 bytes */
#define ZIP_CMD_SIZE		128

#define ZIP_CMD_SIZE_WORDS	(ZIP_CMD_SIZE >> 3) /* 16 64_bit words */

/* size of next chunk buffer pointer */
#define ZIP_MAX_NCBP_SIZE	8

/* size of instruction queue in units of instruction size */
#define ZIP_MAX_NUM_CMDS	((ZIP_MAX_CHUNK_SIZE - ZIP_MAX_NCBP_SIZE) / \
				ZIP_CMD_SIZE) /* 63 */

/* size of instruct queue in bytes */
#define ZIP_MAX_CMDQ_SIZE	((ZIP_MAX_NUM_CMDS * ZIP_CMD_SIZE) + \
				ZIP_MAX_NCBP_SIZE)/* ~8072ull */

#define ZIP_BUF_SIZE	256

#define ZIP_SGPTR_ALIGN	16
#define ZIP_CMDQ_ALIGN	128
#define MAX_SG_LEN	((ZIP_BUF_SIZE - ZIP_SGPTR_ALIGN) / sizeof(void *))

/**< ZIP PMD specified queue pairs */
#define ZIP_MAX_VF_QUEUE	1

#define ZIP_ALIGN_ROUNDUP(x, _align) \
	((_align) * (((x) + (_align) - 1) / (_align)))

/**< ZIP PMD device name */
#define COMPRESSDEV_NAME_ZIP_PMD	compress_octeonx

#define ZIP_PMD_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, \
	octtx_zip_logtype_driver, "%s(): "fmt "\n", \
	__func__, ##args)

#define ZIP_PMD_INFO(fmt, args...) \
	ZIP_PMD_LOG(INFO, fmt, ## args)
#define ZIP_PMD_ERR(fmt, args...) \
	ZIP_PMD_LOG(ERR, fmt, ## args)

/* resources required to process stream */
enum NUM_BUFS_PER_STREAM {
	RES_BUF = 0,
	CMD_BUF,
	HASH_CTX_BUF,
	DECOMP_CTX_BUF,
	IN_DATA_BUF,
	OUT_DATA_BUF,
	HISTORY_DATA_BUF,
	MAX_BUFS_PER_STREAM
};

struct zip_stream;
struct zipvf_qp;

/* Algorithm handler function prototype */
typedef int (*comp_func_t)(struct rte_comp_op *op,
			   struct zipvf_qp *qp, struct zip_stream *zstrm);

/**
 * ZIP private stream structure
 */
struct zip_stream {
	union zip_inst_s *inst;
	/* zip instruction pointer */
	comp_func_t func;
	/* function to process comp operation */
	void *bufs[MAX_BUFS_PER_STREAM];
} __rte_cache_aligned;


/**
 * ZIP instruction Queue
 */
struct zipvf_cmdq {
	rte_spinlock_t qlock;
	/* queue lock */
	uint64_t *sw_head;
	/* pointer to start of 8-byte word length queue-head */
	uint8_t *va;
	/* pointer to instruction queue virtual address */
	rte_iova_t iova;
	/* iova addr of cmdq head*/
};

/**
 * ZIP device queue structure
 */
struct zipvf_qp {
	struct zipvf_cmdq cmdq;
	/* Hardware instruction queue structure */
	struct rte_ring *processed_pkts;
	/* Ring for placing processed packets */
	struct rte_compressdev_stats qp_stats;
	/* Queue pair statistics */
	uint16_t id;
	/* Queue Pair Identifier */
	const char *name;
	/* Unique Queue Pair Name */
	struct zip_vf *vf;
	/* pointer to device, queue belongs to */
} __rte_cache_aligned;

/**
 * ZIP VF device structure.
 */
struct zip_vf {
	int vfid;
	/* vf index */
	struct rte_pci_device *pdev;
	/* pci device */
	void *vbar0;
	/* CSR base address for underlying BAR0 VF.*/
	uint64_t dom_sdom;
	/* Storing mbox domain and subdomain id for app rerun*/
	uint32_t  max_nb_queue_pairs;
	/* pointer to device qps */
	struct rte_mempool *zip_mp;
	/* pointer to pools */
} __rte_cache_aligned;


static inline void
zipvf_prepare_in_buf(struct zip_stream *zstrm, struct rte_comp_op *op)
{
	uint32_t offset, inlen;
	struct rte_mbuf *m_src;
	union zip_inst_s *inst = zstrm->inst;

	inlen = op->src.length;
	offset = op->src.offset;
	m_src = op->m_src;

	/* Prepare direct input data pointer */
	inst->s.dg = 0;
	inst->s.inp_ptr_addr.s.addr =
			rte_pktmbuf_iova_offset(m_src, offset);
	inst->s.inp_ptr_ctl.s.length = inlen;
}

static inline void
zipvf_prepare_out_buf(struct zip_stream *zstrm, struct rte_comp_op *op)
{
	uint32_t offset;
	struct rte_mbuf *m_dst;
	union zip_inst_s *inst = zstrm->inst;

	offset = op->dst.offset;
	m_dst = op->m_dst;

	/* Prepare direct input data pointer */
	inst->s.ds = 0;
	inst->s.out_ptr_addr.s.addr =
			rte_pktmbuf_iova_offset(m_dst, offset);
	inst->s.totaloutputlength = rte_pktmbuf_pkt_len(m_dst) -
			op->dst.offset;
	inst->s.out_ptr_ctl.s.length = inst->s.totaloutputlength;
}

static inline void
zipvf_prepare_cmd_stateless(struct rte_comp_op *op, struct zip_stream *zstrm)
{
	union zip_inst_s *inst = zstrm->inst;

	/* set flush flag to always 1*/
	inst->s.ef = 1;

	if (inst->s.op == ZIP_OP_E_DECOMP)
		inst->s.sf = 1;
	else
		inst->s.sf = 0;

	/* Set input checksum */
	inst->s.adlercrc32 = op->input_chksum;

	/* Prepare gather buffers */
	zipvf_prepare_in_buf(zstrm, op);
	zipvf_prepare_out_buf(zstrm, op);
}

#ifdef ZIP_DBG
static inline void
zip_dump_instruction(void *inst)
{
	union zip_inst_s *cmd83 = (union zip_inst_s *)inst;
	printf("####### START ########\n");
	printf("doneint:%d totaloutputlength:%d\n", cmd83->s.doneint,
		cmd83->s.totaloutputlength);
	printf("exnum:%d iv:%d exbits:%d hmif:%d halg:%d\n", cmd83->s.exn,
		cmd83->s.iv, cmd83->s.exbits, cmd83->s.hmif, cmd83->s.halg);
	printf("flush:%d speed:%d cc:%d\n", cmd83->s.sf,
		cmd83->s.ss, cmd83->s.cc);
	printf("eof:%d bof:%d op:%d dscatter:%d dgather:%d hgather:%d\n",
		cmd83->s.ef, cmd83->s.bf, cmd83->s.op, cmd83->s.ds,
		cmd83->s.dg, cmd83->s.hg);
	printf("historylength:%d adler32:%d\n", cmd83->s.historylength,
		cmd83->s.adlercrc32);
	printf("ctx_ptr.addr:0x%"PRIx64"\n", cmd83->s.ctx_ptr_addr.s.addr);
	printf("ctx_ptr.len:%d\n", cmd83->s.ctx_ptr_ctl.s.length);
	printf("history_ptr.addr:0x%"PRIx64"\n", cmd83->s.his_ptr_addr.s.addr);
	printf("history_ptr.len:%d\n", cmd83->s.his_ptr_ctl.s.length);
	printf("inp_ptr.addr:0x%"PRIx64"\n", cmd83->s.inp_ptr_addr.s.addr);
	printf("inp_ptr.len:%d\n", cmd83->s.inp_ptr_ctl.s.length);
	printf("out_ptr.addr:0x%"PRIx64"\n", cmd83->s.out_ptr_addr.s.addr);
	printf("out_ptr.len:%d\n", cmd83->s.out_ptr_ctl.s.length);
	printf("result_ptr.len:%d\n", cmd83->s.res_ptr_ctl.s.length);
	printf("####### END ########\n");
}
#endif

int
zipvf_create(struct rte_compressdev *compressdev);

int
zipvf_destroy(struct rte_compressdev *compressdev);

int
zipvf_q_init(struct zipvf_qp *qp);

int
zipvf_q_term(struct zipvf_qp *qp);

void
zipvf_push_command(struct zipvf_qp *qp, union zip_inst_s *zcmd);

int
zip_process_op(struct rte_comp_op *op,
				struct zipvf_qp *qp,
				struct zip_stream *zstrm);

uint64_t
zip_reg_read64(uint8_t *hw_addr, uint64_t offset);

void
zip_reg_write64(uint8_t *hw_addr, uint64_t offset, uint64_t val);

#endif /* _RTE_ZIP_VF_H_ */
