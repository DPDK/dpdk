/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#ifndef _RTE_OCTEONTX_ZIP_VF_H_
#define _RTE_OCTEONTX_ZIP_VF_H_

#include <unistd.h>

#include <bus_pci_driver.h>
#include <rte_comp.h>
#include <rte_compressdev.h>
#include <rte_compressdev_pmd.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_spinlock.h>

#include <zip_regs.h>

extern int octtx_zip_logtype_driver;

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
#define PCI_DEVICE_ID_OCTEONTX2_ZIPVF	0xA083

/* maximum number of zip vf devices */
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
#define ZIP_SGBUF_SIZE	(5 * 1024)
#define ZIP_BURST_SIZE	64

#define ZIP_MAXSEG_SIZE      59460
#define ZIP_EXTRABUF_SIZE    4096
#define ZIP_MAX_SEGS         300
#define ZIP_MAX_DATA_SIZE    (16*1024*1024)

#define ZIP_SGPTR_ALIGN	16
#define ZIP_CMDQ_ALIGN	128
#define MAX_SG_LEN	((ZIP_BUF_SIZE - ZIP_SGPTR_ALIGN) / sizeof(void *))

/**< ZIP PMD specified queue pairs */
#define ZIP_MAX_VF_QUEUE	1

#define ZIP_ALIGN_ROUNDUP(x, _align) \
	((_align) * (((x) + (_align) - 1) / (_align)))

/**< ZIP PMD device name */
#define COMPRESSDEV_NAME_ZIP_PMD	compress_octeontx

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
typedef int (*comp_func_t)(struct rte_comp_op *op, struct zipvf_qp *qp,
			   struct zip_stream *zstrm, int num);

/* Scatter gather list */
struct zipvf_sginfo {
	union zip_zptr_addr_s  sg_addr;
	union zip_zptr_ctl_s   sg_ctl;
} __rte_aligned(16);

/**
 * ZIP private stream structure
 */
struct zip_stream {
	union zip_inst_s *inst[ZIP_BURST_SIZE];
	/* zip instruction pointer */
	comp_func_t func;
	/* function to process comp operation */
	void *bufs[MAX_BUFS_PER_STREAM * ZIP_BURST_SIZE];
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
	struct zipvf_sginfo *g_info;
	struct zipvf_sginfo *s_info;
	/* SGL pointers */
	uint64_t num_sgbuf;
	uint64_t enqed;
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
	struct rte_mempool *sg_mp;
	/* pointer to pools */
} __rte_cache_aligned;


static inline int
zipvf_prepare_sgl(struct rte_mbuf *buf, int64_t offset, struct zipvf_sginfo *sg_list,
		  uint32_t data_len, const uint16_t max_segs, struct zipvf_qp *qp)
{
	struct zipvf_sginfo *sginfo = (struct zipvf_sginfo *)sg_list;
	uint32_t tot_buf_len, sgidx;
	int ret = -EINVAL;

	for (sgidx = tot_buf_len = 0; buf && sgidx < max_segs; buf = buf->next) {
		if (offset >= rte_pktmbuf_data_len(buf)) {
			offset -= rte_pktmbuf_data_len(buf);
			continue;
		}

		sginfo[sgidx].sg_ctl.s.length = (uint16_t)(rte_pktmbuf_data_len(buf) - offset);
		sginfo[sgidx].sg_addr.s.addr = rte_pktmbuf_iova_offset(buf, offset);

		offset = 0;
		tot_buf_len += sginfo[sgidx].sg_ctl.s.length;

		if (tot_buf_len >= data_len) {
			sginfo[sgidx].sg_ctl.s.length -= tot_buf_len - data_len;
			ret = 0;
			break;
		}

		ZIP_PMD_LOG(DEBUG, "ZIP SGL buf[%d], len = %d, iova = 0x%"PRIx64"\n",
			    sgidx, sginfo[sgidx].sg_ctl.s.length, sginfo[sgidx].sg_addr.s.addr);
		++sgidx;
	}

	if (unlikely(ret != 0)) {
		if (sgidx == max_segs)
			ZIP_PMD_ERR("Exceeded max segments in ZIP SGL (%u)", max_segs);
		else
			ZIP_PMD_ERR("Mbuf chain is too short");
	}
	qp->num_sgbuf = ++sgidx;

	ZIP_PMD_LOG(DEBUG, "Tot_buf_len:%d max_segs:%"PRIx64"\n", tot_buf_len,
		    qp->num_sgbuf);
	return ret;
}

static inline int
zipvf_prepare_in_buf(union zip_inst_s *inst, struct zipvf_qp *qp, struct rte_comp_op *op)
{
	uint32_t offset, inlen;
	struct rte_mbuf *m_src;
	int ret = 0;

	inlen = op->src.length;
	offset = op->src.offset;
	m_src = op->m_src;

	/* Gather input */
	if (op->m_src->next != NULL && inlen > ZIP_MAXSEG_SIZE) {
		inst->s.dg = 1;

		ret = zipvf_prepare_sgl(m_src, offset, qp->g_info, inlen,
					op->m_src->nb_segs, qp);

		inst->s.inp_ptr_addr.s.addr = rte_mem_virt2iova(qp->g_info);
		inst->s.inp_ptr_ctl.s.length = qp->num_sgbuf;
		inst->s.inp_ptr_ctl.s.fw = 0;

		ZIP_PMD_LOG(DEBUG, "Gather(input): len(nb_segs):%d, iova: 0x%"PRIx64"\n",
			    inst->s.inp_ptr_ctl.s.length, inst->s.inp_ptr_addr.s.addr);
		return ret;
	}

	/* Prepare direct input data pointer */
	inst->s.dg = 0;
	inst->s.inp_ptr_addr.s.addr = rte_pktmbuf_iova_offset(m_src, offset);
	inst->s.inp_ptr_ctl.s.length = inlen;

	ZIP_PMD_LOG(DEBUG, "Direct input - inlen:%d\n", inlen);
	return ret;
}

static inline int
zipvf_prepare_out_buf(union zip_inst_s *inst, struct zipvf_qp *qp, struct rte_comp_op *op)
{
	uint32_t offset, outlen;
	struct rte_mbuf *m_dst;
	int ret = 0;

	offset = op->dst.offset;
	m_dst = op->m_dst;
	outlen = rte_pktmbuf_pkt_len(m_dst) - op->dst.offset;

	/* Scatter output */
	if (op->m_dst->next != NULL && outlen > ZIP_MAXSEG_SIZE) {
		inst->s.ds = 1;
		inst->s.totaloutputlength = outlen;

		ret = zipvf_prepare_sgl(m_dst, offset, qp->s_info, inst->s.totaloutputlength,
					m_dst->nb_segs, qp);

		inst->s.out_ptr_addr.s.addr = rte_mem_virt2iova(qp->s_info);
		inst->s.out_ptr_ctl.s.length = qp->num_sgbuf;

		ZIP_PMD_LOG(DEBUG, "Scatter(output): nb_segs:%d, iova:0x%"PRIx64"\n",
			    inst->s.out_ptr_ctl.s.length, inst->s.out_ptr_addr.s.addr);
		return ret;
	}

	/* Prepare direct output data pointer */
	inst->s.ds = 0;
	inst->s.out_ptr_addr.s.addr = rte_pktmbuf_iova_offset(m_dst, offset);
	inst->s.totaloutputlength = rte_pktmbuf_pkt_len(m_dst) - op->dst.offset;
	if (inst->s.totaloutputlength == ZIP_MAXSEG_SIZE)
		inst->s.totaloutputlength += ZIP_EXTRABUF_SIZE; /* DSTOP */

	inst->s.out_ptr_ctl.s.length = inst->s.totaloutputlength;

	ZIP_PMD_LOG(DEBUG, "Direct output - outlen:%d\n", inst->s.totaloutputlength);
	return ret;
}

static inline int
zipvf_prepare_cmd_stateless(struct rte_comp_op *op, struct zipvf_qp *qp,
			    union zip_inst_s *inst)
{
	/* set flush flag to always 1*/
	inst->s.ef = 1;

	if (inst->s.op == ZIP_OP_E_DECOMP)
		inst->s.sf = 1;
	else
		inst->s.sf = 0;

	/* Set input checksum */
	inst->s.adlercrc32 = op->input_chksum;

	/* Prepare input/output buffers */
	if (zipvf_prepare_in_buf(inst, qp, op)) {
		ZIP_PMD_ERR("Con't fill input SGL ");
		return -EINVAL;
	}

	if (zipvf_prepare_out_buf(inst, qp, op)) {
		ZIP_PMD_ERR("Con't fill output SGL ");
		return -EINVAL;
	}

	return 0;
}

#ifdef ZIP_DBG
static inline void
zip_dump_instruction(void *inst)
{
	union zip_inst_s *cmd83 = (union zip_inst_s *)inst;

	printf("####### START ########\n");
	printf("ZIP Instr:0x%"PRIx64"\n", cmd83);
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
	printf("result_ptr.addr:0x%"PRIx64"\n", cmd83->s.res_ptr_addr.s.addr);
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
zip_process_op(struct rte_comp_op *op, struct zipvf_qp *qp,
	       struct zip_stream *zstrm, int num);

uint64_t
zip_reg_read64(uint8_t *hw_addr, uint64_t offset);

void
zip_reg_write64(uint8_t *hw_addr, uint64_t offset, uint64_t val);

#endif /* _RTE_ZIP_VF_H_ */
