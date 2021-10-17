/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020-2021 NXP
 */

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <dirent.h>

#include <rte_common.h>
#include <rte_bus_vdev.h>
#include <rte_malloc.h>
#include <rte_ring.h>
#include <rte_kvargs.h>

#include <rte_bbdev.h>
#include <rte_bbdev_pmd.h>

#include <bbdev_la12xx_pmd_logs.h>
#include <bbdev_la12xx_ipc.h>
#include <bbdev_la12xx.h>

#define DRIVER_NAME baseband_la12xx

/*  Initialisation params structure that can be used by LA12xx BBDEV driver */
struct bbdev_la12xx_params {
	uint8_t queues_num; /*< LA12xx BBDEV queues number */
	int8_t modem_id; /*< LA12xx modem instance id */
};

#define LA12XX_MAX_NB_QUEUES_ARG	"max_nb_queues"
#define LA12XX_VDEV_MODEM_ID_ARG	"modem"
#define LA12XX_MAX_MODEM 4

#define LA12XX_MAX_CORES	4
#define LA12XX_LDPC_ENC_CORE	0
#define LA12XX_LDPC_DEC_CORE	1

#define LA12XX_MAX_LDPC_ENC_QUEUES	4
#define LA12XX_MAX_LDPC_DEC_QUEUES	4

static const char * const bbdev_la12xx_valid_params[] = {
	LA12XX_MAX_NB_QUEUES_ARG,
	LA12XX_VDEV_MODEM_ID_ARG,
};

static const struct rte_bbdev_op_cap bbdev_capabilities[] = {
	{
		.type   = RTE_BBDEV_OP_LDPC_ENC,
		.cap.ldpc_enc = {
			.capability_flags =
					RTE_BBDEV_LDPC_RATE_MATCH |
					RTE_BBDEV_LDPC_CRC_24A_ATTACH |
					RTE_BBDEV_LDPC_CRC_24B_ATTACH,
			.num_buffers_src =
					RTE_BBDEV_LDPC_MAX_CODE_BLOCKS,
			.num_buffers_dst =
					RTE_BBDEV_LDPC_MAX_CODE_BLOCKS,
		}
	},
	{
		.type   = RTE_BBDEV_OP_LDPC_DEC,
		.cap.ldpc_dec = {
			.capability_flags =
				RTE_BBDEV_LDPC_CRC_TYPE_24A_CHECK |
					RTE_BBDEV_LDPC_CRC_TYPE_24B_CHECK |
					RTE_BBDEV_LDPC_CRC_TYPE_24B_DROP,
			.num_buffers_src =
					RTE_BBDEV_LDPC_MAX_CODE_BLOCKS,
			.num_buffers_hard_out =
					RTE_BBDEV_LDPC_MAX_CODE_BLOCKS,
			.llr_size = 8,
			.llr_decimals = 1,
		}
	},
	RTE_BBDEV_END_OF_CAPABILITIES_LIST()
};

static struct rte_bbdev_queue_conf default_queue_conf = {
	.queue_size = MAX_CHANNEL_DEPTH,
};

/* Get device info */
static void
la12xx_info_get(struct rte_bbdev *dev __rte_unused,
		struct rte_bbdev_driver_info *dev_info)
{
	PMD_INIT_FUNC_TRACE();

	dev_info->driver_name = RTE_STR(DRIVER_NAME);
	dev_info->max_num_queues = LA12XX_MAX_QUEUES;
	dev_info->queue_size_lim = MAX_CHANNEL_DEPTH;
	dev_info->hardware_accelerated = true;
	dev_info->max_dl_queue_priority = 0;
	dev_info->max_ul_queue_priority = 0;
	dev_info->data_endianness = RTE_BIG_ENDIAN;
	dev_info->default_queue_conf = default_queue_conf;
	dev_info->capabilities = bbdev_capabilities;
	dev_info->cpu_flag_reqs = NULL;
	dev_info->min_alignment = 64;

	rte_bbdev_log_debug("got device info from %u", dev->data->dev_id);
}

/* Release queue */
static int
la12xx_queue_release(struct rte_bbdev *dev, uint16_t q_id)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(q_id);

	PMD_INIT_FUNC_TRACE();

	return 0;
}

#define HUGEPG_OFFSET(A) \
		((uint64_t) ((unsigned long) (A) \
		- ((uint64_t)ipc_priv->hugepg_start.host_vaddr)))

#define MODEM_P2V(A) \
	((uint64_t) ((unsigned long) (A) \
		+ (unsigned long)(ipc_priv->peb_start.host_vaddr)))

static int
ipc_queue_configure(uint32_t channel_id,
		    ipc_t instance,
		    struct bbdev_la12xx_q_priv *q_priv)
{
	ipc_userspace_t *ipc_priv = (ipc_userspace_t *)instance;
	ipc_instance_t *ipc_instance = ipc_priv->instance;
	ipc_ch_t *ch;
	void *vaddr;
	uint32_t i = 0;
	uint32_t msg_size = sizeof(struct bbdev_ipc_enqueue_op);

	PMD_INIT_FUNC_TRACE();

	rte_bbdev_log_debug("%x %p", ipc_instance->initialized,
		ipc_priv->instance);
	ch = &(ipc_instance->ch_list[channel_id]);

	rte_bbdev_log_debug("channel: %u, depth: %u, msg size: %u",
		channel_id, q_priv->queue_size, msg_size);

	/* Start init of channel */
	ch->md.ring_size = rte_cpu_to_be_32(q_priv->queue_size);
	ch->md.pi = 0;
	ch->md.ci = 0;
	ch->md.msg_size = msg_size;
	for (i = 0; i < q_priv->queue_size; i++) {
		vaddr = rte_malloc(NULL, msg_size, RTE_CACHE_LINE_SIZE);
		if (!vaddr)
			return IPC_HOST_BUF_ALLOC_FAIL;
		/* Only offset now */
		ch->bd_h[i].modem_ptr =
			rte_cpu_to_be_32(HUGEPG_OFFSET(vaddr));
		ch->bd_h[i].host_virt_l = lower_32_bits(vaddr);
		ch->bd_h[i].host_virt_h = upper_32_bits(vaddr);
		q_priv->msg_ch_vaddr[i] = vaddr;
		/* Not sure use of this len may be for CRC*/
		ch->bd_h[i].len = 0;
	}
	ch->host_ipc_params =
		rte_cpu_to_be_32(HUGEPG_OFFSET(q_priv->host_params));

	rte_bbdev_log_debug("Channel configured");
	return IPC_SUCCESS;
}

static int
la12xx_e200_queue_setup(struct rte_bbdev *dev,
		struct bbdev_la12xx_q_priv *q_priv)
{
	struct bbdev_la12xx_private *priv = dev->data->dev_private;
	ipc_userspace_t *ipc_priv = priv->ipc_priv;
	struct gul_hif *mhif;
	ipc_metadata_t *ipc_md;
	ipc_ch_t *ch;
	int instance_id = 0, i;
	int ret;

	PMD_INIT_FUNC_TRACE();

	switch (q_priv->op_type) {
	case RTE_BBDEV_OP_LDPC_ENC:
		q_priv->la12xx_core_id = LA12XX_LDPC_ENC_CORE;
		break;
	case RTE_BBDEV_OP_LDPC_DEC:
		q_priv->la12xx_core_id = LA12XX_LDPC_DEC_CORE;
		break;
	default:
		rte_bbdev_log(ERR, "Unsupported op type\n");
		return -1;
	}

	mhif = (struct gul_hif *)ipc_priv->mhif_start.host_vaddr;
	/* offset is from start of PEB */
	ipc_md = (ipc_metadata_t *)((uintptr_t)ipc_priv->peb_start.host_vaddr +
		mhif->ipc_regs.ipc_mdata_offset);
	ch = &ipc_md->instance_list[instance_id].ch_list[q_priv->q_id];

	if (q_priv->q_id < priv->num_valid_queues) {
		ipc_br_md_t *md = &(ch->md);

		q_priv->feca_blk_id = rte_cpu_to_be_32(ch->feca_blk_id);
		q_priv->feca_blk_id_be32 = ch->feca_blk_id;
		q_priv->host_pi = rte_be_to_cpu_32(md->pi);
		q_priv->host_ci = rte_be_to_cpu_32(md->ci);
		q_priv->host_params = (host_ipc_params_t *)(uintptr_t)
			(rte_be_to_cpu_32(ch->host_ipc_params) +
			((uint64_t)ipc_priv->hugepg_start.host_vaddr));

		for (i = 0; i < q_priv->queue_size; i++) {
			uint32_t h, l;

			h = ch->bd_h[i].host_virt_h;
			l = ch->bd_h[i].host_virt_l;
			q_priv->msg_ch_vaddr[i] = (void *)join_32_bits(h, l);
		}

		rte_bbdev_log(WARNING,
			"Queue [%d] already configured, not configuring again",
			q_priv->q_id);
		return 0;
	}

	rte_bbdev_log_debug("setting up queue %d", q_priv->q_id);

	/* Call ipc_configure_channel */
	ret = ipc_queue_configure(q_priv->q_id, ipc_priv, q_priv);
	if (ret) {
		rte_bbdev_log(ERR, "Unable to setup queue (%d) (err=%d)",
		       q_priv->q_id, ret);
		return ret;
	}

	/* Set queue properties for LA12xx device */
	switch (q_priv->op_type) {
	case RTE_BBDEV_OP_LDPC_ENC:
		if (priv->num_ldpc_enc_queues >= LA12XX_MAX_LDPC_ENC_QUEUES) {
			rte_bbdev_log(ERR,
				"num_ldpc_enc_queues reached max value");
			return -1;
		}
		ch->la12xx_core_id =
			rte_cpu_to_be_32(LA12XX_LDPC_ENC_CORE);
		ch->feca_blk_id = rte_cpu_to_be_32(priv->num_ldpc_enc_queues++);
		break;
	case RTE_BBDEV_OP_LDPC_DEC:
		if (priv->num_ldpc_dec_queues >= LA12XX_MAX_LDPC_DEC_QUEUES) {
			rte_bbdev_log(ERR,
				"num_ldpc_dec_queues reached max value");
			return -1;
		}
		ch->la12xx_core_id =
			rte_cpu_to_be_32(LA12XX_LDPC_DEC_CORE);
		ch->feca_blk_id = rte_cpu_to_be_32(priv->num_ldpc_dec_queues++);
		break;
	default:
		rte_bbdev_log(ERR, "Not supported op type\n");
		return -1;
	}
	ch->op_type = rte_cpu_to_be_32(q_priv->op_type);
	ch->depth = rte_cpu_to_be_32(q_priv->queue_size);

	/* Store queue config here */
	q_priv->feca_blk_id = rte_cpu_to_be_32(ch->feca_blk_id);
	q_priv->feca_blk_id_be32 = ch->feca_blk_id;

	return 0;
}

/* Setup a queue */
static int
la12xx_queue_setup(struct rte_bbdev *dev, uint16_t q_id,
		const struct rte_bbdev_queue_conf *queue_conf)
{
	struct bbdev_la12xx_private *priv = dev->data->dev_private;
	struct rte_bbdev_queue_data *q_data;
	struct bbdev_la12xx_q_priv *q_priv;
	int ret;

	PMD_INIT_FUNC_TRACE();

	/* Move to setup_queues callback */
	q_data = &dev->data->queues[q_id];
	q_data->queue_private = rte_zmalloc(NULL,
		sizeof(struct bbdev_la12xx_q_priv), 0);
	if (!q_data->queue_private) {
		rte_bbdev_log(ERR, "Memory allocation failed for qpriv");
		return -ENOMEM;
	}
	q_priv = q_data->queue_private;
	q_priv->q_id = q_id;
	q_priv->bbdev_priv = dev->data->dev_private;
	q_priv->queue_size = queue_conf->queue_size;
	q_priv->op_type = queue_conf->op_type;

	ret = la12xx_e200_queue_setup(dev, q_priv);
	if (ret) {
		rte_bbdev_log(ERR, "e200_queue_setup failed for qid: %d",
				     q_id);
		return ret;
	}

	/* Store queue config here */
	priv->num_valid_queues++;

	return 0;
}

static int
la12xx_start(struct rte_bbdev *dev)
{
	struct bbdev_la12xx_private *priv = dev->data->dev_private;
	ipc_userspace_t *ipc_priv = priv->ipc_priv;
	int ready = 0;
	struct gul_hif *hif_start;

	PMD_INIT_FUNC_TRACE();

	hif_start = (struct gul_hif *)ipc_priv->mhif_start.host_vaddr;

	/* Set Host Read bit */
	SET_HIF_HOST_RDY(hif_start, HIF_HOST_READY_IPC_APP);

	/* Now wait for modem ready bit */
	while (!ready)
		ready = CHK_HIF_MOD_RDY(hif_start, HIF_MOD_READY_IPC_APP);

	return 0;
}

static const struct rte_bbdev_ops pmd_ops = {
	.info_get = la12xx_info_get,
	.queue_setup = la12xx_queue_setup,
	.queue_release = la12xx_queue_release,
	.start = la12xx_start
};

static inline int
is_bd_ring_full(uint32_t ci, uint32_t ci_flag,
		uint32_t pi, uint32_t pi_flag)
{
	if (pi == ci) {
		if (pi_flag != ci_flag)
			return 1; /* Ring is Full */
	}
	return 0;
}

static inline int
prepare_ldpc_enc_op(struct rte_bbdev_enc_op *bbdev_enc_op,
		    struct bbdev_la12xx_q_priv *q_priv __rte_unused,
		    struct rte_bbdev_op_data *in_op_data __rte_unused,
		    struct rte_bbdev_op_data *out_op_data)
{
	struct rte_bbdev_op_ldpc_enc *ldpc_enc = &bbdev_enc_op->ldpc_enc;
	uint32_t total_out_bits;

	total_out_bits = (ldpc_enc->tb_params.cab *
		ldpc_enc->tb_params.ea) + (ldpc_enc->tb_params.c -
		ldpc_enc->tb_params.cab) * ldpc_enc->tb_params.eb;

	ldpc_enc->output.length = (total_out_bits + 7)/8;

	rte_pktmbuf_append(out_op_data->data, ldpc_enc->output.length);

	return 0;
}

static inline int
prepare_ldpc_dec_op(struct rte_bbdev_dec_op *bbdev_dec_op,
		    struct bbdev_ipc_dequeue_op *bbdev_ipc_op,
		    struct bbdev_la12xx_q_priv *q_priv  __rte_unused,
		    struct rte_bbdev_op_data *out_op_data  __rte_unused)
{
	struct rte_bbdev_op_ldpc_dec *ldpc_dec = &bbdev_dec_op->ldpc_dec;
	uint32_t total_out_bits;
	uint32_t num_code_blocks = 0;
	uint16_t sys_cols;

	sys_cols = (ldpc_dec->basegraph == 1) ? 22 : 10;
	if (ldpc_dec->tb_params.c == 1) {
		total_out_bits = ((sys_cols * ldpc_dec->z_c) -
				ldpc_dec->n_filler);
		/* 5G-NR protocol uses 16 bit CRC when output packet
		 * size <= 3824 (bits). Otherwise 24 bit CRC is used.
		 * Adjust the output bits accordingly
		 */
		if (total_out_bits - 16 <= 3824)
			total_out_bits -= 16;
		else
			total_out_bits -= 24;
		ldpc_dec->hard_output.length = (total_out_bits / 8);
	} else {
		total_out_bits = (((sys_cols * ldpc_dec->z_c) -
				ldpc_dec->n_filler - 24) *
				ldpc_dec->tb_params.c);
		ldpc_dec->hard_output.length = (total_out_bits / 8) - 3;
	}

	num_code_blocks = ldpc_dec->tb_params.c;

	bbdev_ipc_op->num_code_blocks = rte_cpu_to_be_32(num_code_blocks);

	return 0;
}

static int
enqueue_single_op(struct bbdev_la12xx_q_priv *q_priv, void *bbdev_op)
{
	struct bbdev_la12xx_private *priv = q_priv->bbdev_priv;
	ipc_userspace_t *ipc_priv = priv->ipc_priv;
	ipc_instance_t *ipc_instance = ipc_priv->instance;
	struct bbdev_ipc_dequeue_op *bbdev_ipc_op;
	struct rte_bbdev_op_ldpc_enc *ldpc_enc;
	struct rte_bbdev_op_ldpc_dec *ldpc_dec;
	uint32_t q_id = q_priv->q_id;
	uint32_t ci, ci_flag, pi, pi_flag;
	ipc_ch_t *ch = &(ipc_instance->ch_list[q_id]);
	ipc_br_md_t *md = &(ch->md);
	size_t virt;
	char *huge_start_addr =
		(char *)q_priv->bbdev_priv->ipc_priv->hugepg_start.host_vaddr;
	struct rte_bbdev_op_data *in_op_data, *out_op_data;
	char *data_ptr;
	uint32_t l1_pcie_addr;
	int ret;

	ci = IPC_GET_CI_INDEX(q_priv->host_ci);
	ci_flag = IPC_GET_CI_FLAG(q_priv->host_ci);

	pi = IPC_GET_PI_INDEX(q_priv->host_pi);
	pi_flag = IPC_GET_PI_FLAG(q_priv->host_pi);

	rte_bbdev_dp_log(DEBUG, "before bd_ring_full: pi: %u, ci: %u,"
		"pi_flag: %u, ci_flag: %u, ring size: %u",
		pi, ci, pi_flag, ci_flag, q_priv->queue_size);

	if (is_bd_ring_full(ci, ci_flag, pi, pi_flag)) {
		rte_bbdev_dp_log(DEBUG, "bd ring full for queue id: %d", q_id);
		return IPC_CH_FULL;
	}

	virt = MODEM_P2V(q_priv->host_params->bd_m_modem_ptr[pi]);
	bbdev_ipc_op = (struct bbdev_ipc_dequeue_op *)virt;
	q_priv->bbdev_op[pi] = bbdev_op;

	switch (q_priv->op_type) {
	case RTE_BBDEV_OP_LDPC_ENC:
		ldpc_enc = &(((struct rte_bbdev_enc_op *)bbdev_op)->ldpc_enc);
		in_op_data = &ldpc_enc->input;
		out_op_data = &ldpc_enc->output;

		ret = prepare_ldpc_enc_op(bbdev_op, q_priv,
					  in_op_data, out_op_data);
		if (ret) {
			rte_bbdev_log(ERR, "process_ldpc_enc_op fail, ret: %d",
				ret);
			return ret;
		}
		break;

	case RTE_BBDEV_OP_LDPC_DEC:
		ldpc_dec = &(((struct rte_bbdev_dec_op *)bbdev_op)->ldpc_dec);
		in_op_data = &ldpc_dec->input;

		out_op_data = &ldpc_dec->hard_output;

		ret = prepare_ldpc_dec_op(bbdev_op, bbdev_ipc_op,
					  q_priv, out_op_data);
		if (ret) {
			rte_bbdev_log(ERR, "process_ldpc_dec_op fail, ret: %d",
				ret);
			return ret;
		}
		break;

	default:
		rte_bbdev_log(ERR, "unsupported bbdev_ipc op type");
		return -1;
	}

	if (in_op_data->data) {
		data_ptr = rte_pktmbuf_mtod(in_op_data->data, char *);
		l1_pcie_addr = (uint32_t)GUL_USER_HUGE_PAGE_ADDR +
			       data_ptr - huge_start_addr;
		bbdev_ipc_op->in_addr = l1_pcie_addr;
		bbdev_ipc_op->in_len = in_op_data->length;
	}

	if (out_op_data->data) {
		data_ptr = rte_pktmbuf_mtod(out_op_data->data, char *);
		l1_pcie_addr = (uint32_t)GUL_USER_HUGE_PAGE_ADDR +
				data_ptr - huge_start_addr;
		bbdev_ipc_op->out_addr = rte_cpu_to_be_32(l1_pcie_addr);
		bbdev_ipc_op->out_len = rte_cpu_to_be_32(out_op_data->length);
	}

	/* Move Producer Index forward */
	pi++;
	/* Flip the PI flag, if wrapping */
	if (unlikely(q_priv->queue_size == pi)) {
		pi = 0;
		pi_flag = pi_flag ? 0 : 1;
	}

	if (pi_flag)
		IPC_SET_PI_FLAG(pi);
	else
		IPC_RESET_PI_FLAG(pi);
	q_priv->host_pi = pi;

	/* Wait for Data Copy & pi_flag update to complete before updating pi */
	rte_mb();
	/* now update pi */
	md->pi = rte_cpu_to_be_32(pi);

	rte_bbdev_dp_log(DEBUG, "enter: pi: %u, ci: %u,"
			"pi_flag: %u, ci_flag: %u, ring size: %u",
			pi, ci, pi_flag, ci_flag, q_priv->queue_size);

	return 0;
}

/* Enqueue decode burst */
static uint16_t
enqueue_dec_ops(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_dec_op **ops, uint16_t nb_ops)
{
	struct bbdev_la12xx_q_priv *q_priv = q_data->queue_private;
	int nb_enqueued, ret;

	for (nb_enqueued = 0; nb_enqueued < nb_ops; nb_enqueued++) {
		ret = enqueue_single_op(q_priv, ops[nb_enqueued]);
		if (ret)
			break;
	}

	q_data->queue_stats.enqueue_err_count += nb_ops - nb_enqueued;
	q_data->queue_stats.enqueued_count += nb_enqueued;

	return nb_enqueued;
}

/* Enqueue encode burst */
static uint16_t
enqueue_enc_ops(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_enc_op **ops, uint16_t nb_ops)
{
	struct bbdev_la12xx_q_priv *q_priv = q_data->queue_private;
	int nb_enqueued, ret;

	for (nb_enqueued = 0; nb_enqueued < nb_ops; nb_enqueued++) {
		ret = enqueue_single_op(q_priv, ops[nb_enqueued]);
		if (ret)
			break;
	}

	q_data->queue_stats.enqueue_err_count += nb_ops - nb_enqueued;
	q_data->queue_stats.enqueued_count += nb_enqueued;

	return nb_enqueued;
}

/* Dequeue encode burst */
static void *
dequeue_single_op(struct bbdev_la12xx_q_priv *q_priv, void *dst)
{
	void *op;
	uint32_t ci, ci_flag;
	uint32_t temp_ci;

	temp_ci = q_priv->host_params->ci;
	if (temp_ci == q_priv->host_ci)
		return NULL;

	ci = IPC_GET_CI_INDEX(q_priv->host_ci);
	ci_flag = IPC_GET_CI_FLAG(q_priv->host_ci);

	rte_bbdev_dp_log(DEBUG,
		"ci: %u, ci_flag: %u, ring size: %u",
		ci, ci_flag, q_priv->queue_size);

	op = q_priv->bbdev_op[ci];

	rte_memcpy(dst, q_priv->msg_ch_vaddr[ci],
		sizeof(struct bbdev_ipc_enqueue_op));

	/* Move Consumer Index forward */
	ci++;
	/* Flip the CI flag, if wrapping */
	if (q_priv->queue_size == ci) {
		ci = 0;
		ci_flag = ci_flag ? 0 : 1;
	}
	if (ci_flag)
		IPC_SET_CI_FLAG(ci);
	else
		IPC_RESET_CI_FLAG(ci);

	q_priv->host_ci = ci;

	rte_bbdev_dp_log(DEBUG,
		"exit: ci: %u, ci_flag: %u, ring size: %u",
		ci, ci_flag, q_priv->queue_size);

	return op;
}

/* Dequeue decode burst */
static uint16_t
dequeue_dec_ops(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_dec_op **ops, uint16_t nb_ops)
{
	struct bbdev_la12xx_q_priv *q_priv = q_data->queue_private;
	struct bbdev_ipc_enqueue_op bbdev_ipc_op;
	int nb_dequeued;

	for (nb_dequeued = 0; nb_dequeued < nb_ops; nb_dequeued++) {
		ops[nb_dequeued] = dequeue_single_op(q_priv, &bbdev_ipc_op);
		if (!ops[nb_dequeued])
			break;
		ops[nb_dequeued]->status = bbdev_ipc_op.status;
	}
	q_data->queue_stats.dequeued_count += nb_dequeued;

	return nb_dequeued;
}

/* Dequeue encode burst */
static uint16_t
dequeue_enc_ops(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_enc_op **ops, uint16_t nb_ops)
{
	struct bbdev_la12xx_q_priv *q_priv = q_data->queue_private;
	struct bbdev_ipc_enqueue_op bbdev_ipc_op;
	int nb_enqueued;

	for (nb_enqueued = 0; nb_enqueued < nb_ops; nb_enqueued++) {
		ops[nb_enqueued] = dequeue_single_op(q_priv, &bbdev_ipc_op);
		if (!ops[nb_enqueued])
			break;
		ops[nb_enqueued]->status = bbdev_ipc_op.status;
	}
	q_data->queue_stats.enqueued_count += nb_enqueued;

	return nb_enqueued;
}

static struct hugepage_info *
get_hugepage_info(void)
{
	struct hugepage_info *hp_info;
	struct rte_memseg *mseg;

	PMD_INIT_FUNC_TRACE();

	/* TODO - find a better way */
	hp_info = rte_malloc(NULL, sizeof(struct hugepage_info), 0);
	if (!hp_info) {
		rte_bbdev_log(ERR, "Unable to allocate on local heap");
		return NULL;
	}

	mseg = rte_mem_virt2memseg(hp_info, NULL);
	hp_info->vaddr = mseg->addr;
	hp_info->paddr = rte_mem_virt2phy(mseg->addr);
	hp_info->len = mseg->len;

	return hp_info;
}

static int
open_ipc_dev(int modem_id)
{
	char dev_initials[16], dev_path[PATH_MAX];
	struct dirent *entry;
	int dev_ipc = 0;
	DIR *dir;

	dir = opendir("/dev/");
	if (!dir) {
		rte_bbdev_log(ERR, "Unable to open /dev/");
		return -1;
	}

	sprintf(dev_initials, "gulipcgul%d", modem_id);

	while ((entry = readdir(dir)) != NULL) {
		if (!strncmp(dev_initials, entry->d_name,
		    sizeof(dev_initials) - 1))
			break;
	}

	if (!entry) {
		rte_bbdev_log(ERR, "Error: No gulipcgul%d device", modem_id);
		return -1;
	}

	sprintf(dev_path, "/dev/%s", entry->d_name);
	dev_ipc = open(dev_path, O_RDWR);
	if (dev_ipc  < 0) {
		rte_bbdev_log(ERR, "Error: Cannot open %s", dev_path);
		return -errno;
	}

	return dev_ipc;
}

static int
setup_la12xx_dev(struct rte_bbdev *dev)
{
	struct bbdev_la12xx_private *priv = dev->data->dev_private;
	ipc_userspace_t *ipc_priv = priv->ipc_priv;
	struct hugepage_info *hp = NULL;
	ipc_channel_us_t *ipc_priv_ch = NULL;
	int dev_ipc = 0, dev_mem = 0, i;
	ipc_metadata_t *ipc_md;
	struct gul_hif *mhif;
	uint32_t phy_align = 0;
	int ret;

	PMD_INIT_FUNC_TRACE();

	if (!ipc_priv) {
		/* TODO - get a better way */
		/* Get the hugepage info against it */
		hp = get_hugepage_info();
		if (!hp) {
			rte_bbdev_log(ERR, "Unable to get hugepage info");
			ret = -ENOMEM;
			goto err;
		}

		rte_bbdev_log_debug("0x%" PRIx64 " %p 0x%" PRIx64,
				hp->paddr, hp->vaddr, hp->len);

		ipc_priv = rte_zmalloc(0, sizeof(ipc_userspace_t), 0);
		if (ipc_priv == NULL) {
			rte_bbdev_log(ERR,
				"Unable to allocate memory for ipc priv");
			ret = -ENOMEM;
			goto err;
		}

		for (i = 0; i < IPC_MAX_CHANNEL_COUNT; i++) {
			ipc_priv_ch = rte_zmalloc(0,
				sizeof(ipc_channel_us_t), 0);
			if (ipc_priv_ch == NULL) {
				rte_bbdev_log(ERR,
					"Unable to allocate memory for channels");
				ret = -ENOMEM;
			}
			ipc_priv->channels[i] = ipc_priv_ch;
		}

		dev_mem = open("/dev/mem", O_RDWR);
		if (dev_mem < 0) {
			rte_bbdev_log(ERR, "Error: Cannot open /dev/mem");
			ret = -errno;
			goto err;
		}

		ipc_priv->instance_id = 0;
		ipc_priv->dev_mem = dev_mem;

		rte_bbdev_log_debug("hugepg input 0x%" PRIx64 "%p 0x%" PRIx64,
			hp->paddr, hp->vaddr, hp->len);

		ipc_priv->sys_map.hugepg_start.host_phys = hp->paddr;
		ipc_priv->sys_map.hugepg_start.size = hp->len;

		ipc_priv->hugepg_start.host_phys = hp->paddr;
		ipc_priv->hugepg_start.host_vaddr = hp->vaddr;
		ipc_priv->hugepg_start.size = hp->len;

		rte_free(hp);
	}

	dev_ipc = open_ipc_dev(priv->modem_id);
	if (dev_ipc < 0) {
		rte_bbdev_log(ERR, "Error: open_ipc_dev failed");
		goto err;
	}
	ipc_priv->dev_ipc = dev_ipc;

	ret = ioctl(ipc_priv->dev_ipc, IOCTL_GUL_IPC_GET_SYS_MAP,
		    &ipc_priv->sys_map);
	if (ret) {
		rte_bbdev_log(ERR,
			"IOCTL_GUL_IPC_GET_SYS_MAP ioctl failed");
		goto err;
	}

	phy_align = (ipc_priv->sys_map.mhif_start.host_phys % 0x1000);
	ipc_priv->mhif_start.host_vaddr =
		mmap(0, ipc_priv->sys_map.mhif_start.size + phy_align,
		     (PROT_READ | PROT_WRITE), MAP_SHARED, ipc_priv->dev_mem,
		     (ipc_priv->sys_map.mhif_start.host_phys - phy_align));
	if (ipc_priv->mhif_start.host_vaddr == MAP_FAILED) {
		rte_bbdev_log(ERR, "MAP failed:");
		ret = -errno;
		goto err;
	}

	ipc_priv->mhif_start.host_vaddr = (void *) ((uintptr_t)
		(ipc_priv->mhif_start.host_vaddr) + phy_align);

	phy_align = (ipc_priv->sys_map.peb_start.host_phys % 0x1000);
	ipc_priv->peb_start.host_vaddr =
		mmap(0, ipc_priv->sys_map.peb_start.size + phy_align,
		     (PROT_READ | PROT_WRITE), MAP_SHARED, ipc_priv->dev_mem,
		     (ipc_priv->sys_map.peb_start.host_phys - phy_align));
	if (ipc_priv->peb_start.host_vaddr == MAP_FAILED) {
		rte_bbdev_log(ERR, "MAP failed:");
		ret = -errno;
		goto err;
	}

	ipc_priv->peb_start.host_vaddr = (void *)((uintptr_t)
		(ipc_priv->peb_start.host_vaddr) + phy_align);

	phy_align = (ipc_priv->sys_map.modem_ccsrbar.host_phys % 0x1000);
	ipc_priv->modem_ccsrbar.host_vaddr =
		mmap(0, ipc_priv->sys_map.modem_ccsrbar.size + phy_align,
		     (PROT_READ | PROT_WRITE), MAP_SHARED, ipc_priv->dev_mem,
		     (ipc_priv->sys_map.modem_ccsrbar.host_phys - phy_align));
	if (ipc_priv->modem_ccsrbar.host_vaddr == MAP_FAILED) {
		rte_bbdev_log(ERR, "MAP failed:");
		ret = -errno;
		goto err;
	}

	ipc_priv->modem_ccsrbar.host_vaddr = (void *)((uintptr_t)
		(ipc_priv->modem_ccsrbar.host_vaddr) + phy_align);

	ipc_priv->hugepg_start.modem_phys =
		ipc_priv->sys_map.hugepg_start.modem_phys;

	ipc_priv->mhif_start.host_phys =
		ipc_priv->sys_map.mhif_start.host_phys;
	ipc_priv->mhif_start.size = ipc_priv->sys_map.mhif_start.size;
	ipc_priv->peb_start.host_phys = ipc_priv->sys_map.peb_start.host_phys;
	ipc_priv->peb_start.size = ipc_priv->sys_map.peb_start.size;

	rte_bbdev_log(INFO, "peb 0x%" PRIx64 "%p 0x%" PRIx32,
			ipc_priv->peb_start.host_phys,
			ipc_priv->peb_start.host_vaddr,
			ipc_priv->peb_start.size);
	rte_bbdev_log(INFO, "hugepg 0x%" PRIx64 "%p 0x%" PRIx32,
			ipc_priv->hugepg_start.host_phys,
			ipc_priv->hugepg_start.host_vaddr,
			ipc_priv->hugepg_start.size);
	rte_bbdev_log(INFO, "mhif 0x%" PRIx64 "%p 0x%" PRIx32,
			ipc_priv->mhif_start.host_phys,
			ipc_priv->mhif_start.host_vaddr,
			ipc_priv->mhif_start.size);
	mhif = (struct gul_hif *)ipc_priv->mhif_start.host_vaddr;

	/* offset is from start of PEB */
	ipc_md = (ipc_metadata_t *)((uintptr_t)ipc_priv->peb_start.host_vaddr +
			mhif->ipc_regs.ipc_mdata_offset);

	if (sizeof(ipc_metadata_t) != mhif->ipc_regs.ipc_mdata_size) {
		rte_bbdev_log(ERR,
			"ipc_metadata_t =0x%" PRIx64
			", mhif->ipc_regs.ipc_mdata_size=0x%" PRIx32,
			(uint64_t)(sizeof(ipc_metadata_t)),
			mhif->ipc_regs.ipc_mdata_size);
		rte_bbdev_log(ERR, "--> mhif->ipc_regs.ipc_mdata_offset= 0x%"
			PRIx32, mhif->ipc_regs.ipc_mdata_offset);
		rte_bbdev_log(ERR, "gul_hif size=0x%" PRIx64,
			(uint64_t)(sizeof(struct gul_hif)));
		return IPC_MD_SZ_MISS_MATCH;
	}

	ipc_priv->instance = (ipc_instance_t *)
		(&ipc_md->instance_list[ipc_priv->instance_id]);

	rte_bbdev_log_debug("finish host init");

	priv->ipc_priv = ipc_priv;

	return 0;

err:
	rte_free(hp);
	rte_free(ipc_priv);
	rte_free(ipc_priv_ch);
	if (dev_mem)
		close(dev_mem);
	if (dev_ipc)
		close(dev_ipc);

	return ret;
}

static inline int
parse_u16_arg(const char *key, const char *value, void *extra_args)
{
	uint16_t *u16 = extra_args;

	uint64_t result;
	if ((value == NULL) || (extra_args == NULL))
		return -EINVAL;
	errno = 0;
	result = strtoul(value, NULL, 0);
	if ((result >= (1 << 16)) || (errno != 0)) {
		rte_bbdev_log(ERR, "Invalid value %" PRIu64 " for %s",
			      result, key);
		return -ERANGE;
	}
	*u16 = (uint16_t)result;
	return 0;
}

/* Parse integer from integer argument */
static int
parse_integer_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	int i;
	char *end;

	errno = 0;

	i = strtol(value, &end, 10);
	if (*end != 0 || errno != 0 || i < 0 || i > LA12XX_MAX_MODEM) {
		rte_bbdev_log(ERR, "Supported Port IDS are 0 to %d",
			LA12XX_MAX_MODEM - 1);
		return -EINVAL;
	}

	*((uint32_t *)extra_args) = i;

	return 0;
}

/* Parse parameters used to create device */
static int
parse_bbdev_la12xx_params(struct bbdev_la12xx_params *params,
		const char *input_args)
{
	struct rte_kvargs *kvlist = NULL;
	int ret = 0;

	if (params == NULL)
		return -EINVAL;
	if (input_args) {
		kvlist = rte_kvargs_parse(input_args,
				bbdev_la12xx_valid_params);
		if (kvlist == NULL)
			return -EFAULT;

		ret = rte_kvargs_process(kvlist, bbdev_la12xx_valid_params[0],
					&parse_u16_arg, &params->queues_num);
		if (ret < 0)
			goto exit;

		ret = rte_kvargs_process(kvlist,
					bbdev_la12xx_valid_params[1],
					&parse_integer_arg,
					&params->modem_id);

		if (params->modem_id >= LA12XX_MAX_MODEM) {
			rte_bbdev_log(ERR, "Invalid modem id, must be < %u",
					LA12XX_MAX_MODEM);
			goto exit;
		}
	}

exit:
	if (kvlist)
		rte_kvargs_free(kvlist);
	return ret;
}

/* Create device */
static int
la12xx_bbdev_create(struct rte_vdev_device *vdev,
		struct bbdev_la12xx_params *init_params)
{
	struct rte_bbdev *bbdev;
	const char *name = rte_vdev_device_name(vdev);
	struct bbdev_la12xx_private *priv;
	int ret;

	PMD_INIT_FUNC_TRACE();

	bbdev = rte_bbdev_allocate(name);
	if (bbdev == NULL)
		return -ENODEV;

	bbdev->data->dev_private = rte_zmalloc(name,
			sizeof(struct bbdev_la12xx_private),
			RTE_CACHE_LINE_SIZE);
	if (bbdev->data->dev_private == NULL) {
		rte_bbdev_release(bbdev);
		return -ENOMEM;
	}

	priv = bbdev->data->dev_private;
	priv->modem_id = init_params->modem_id;
	/* if modem id is not configured */
	if (priv->modem_id == -1)
		priv->modem_id = bbdev->data->dev_id;

	/* Reset Global variables */
	priv->num_ldpc_enc_queues = 0;
	priv->num_ldpc_dec_queues = 0;
	priv->num_valid_queues = 0;
	priv->max_nb_queues = init_params->queues_num;

	rte_bbdev_log(INFO, "Setting Up %s: DevId=%d, ModemId=%d",
				name, bbdev->data->dev_id, priv->modem_id);
	ret = setup_la12xx_dev(bbdev);
	if (ret) {
		rte_bbdev_log(ERR, "IPC Setup failed for %s", name);
		rte_free(bbdev->data->dev_private);
		return ret;
	}
	bbdev->dev_ops = &pmd_ops;
	bbdev->device = &vdev->device;
	bbdev->data->socket_id = 0;
	bbdev->intr_handle = NULL;

	/* register rx/tx burst functions for data path */
	bbdev->dequeue_enc_ops = NULL;
	bbdev->dequeue_dec_ops = NULL;
	bbdev->enqueue_enc_ops = NULL;
	bbdev->enqueue_dec_ops = NULL;
	bbdev->dequeue_ldpc_enc_ops = dequeue_enc_ops;
	bbdev->dequeue_ldpc_dec_ops = dequeue_dec_ops;
	bbdev->enqueue_ldpc_enc_ops = enqueue_enc_ops;
	bbdev->enqueue_ldpc_dec_ops = enqueue_dec_ops;

	return 0;
}

/* Initialise device */
static int
la12xx_bbdev_probe(struct rte_vdev_device *vdev)
{
	struct bbdev_la12xx_params init_params = {
		8, -1,
	};
	const char *name;
	const char *input_args;

	PMD_INIT_FUNC_TRACE();

	if (vdev == NULL)
		return -EINVAL;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	input_args = rte_vdev_device_args(vdev);
	parse_bbdev_la12xx_params(&init_params, input_args);

	return la12xx_bbdev_create(vdev, &init_params);
}

/* Uninitialise device */
static int
la12xx_bbdev_remove(struct rte_vdev_device *vdev)
{
	struct rte_bbdev *bbdev;
	const char *name;

	PMD_INIT_FUNC_TRACE();

	if (vdev == NULL)
		return -EINVAL;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	bbdev = rte_bbdev_get_named_dev(name);
	if (bbdev == NULL)
		return -EINVAL;

	rte_free(bbdev->data->dev_private);

	return rte_bbdev_release(bbdev);
}

static struct rte_vdev_driver bbdev_la12xx_pmd_drv = {
	.probe = la12xx_bbdev_probe,
	.remove = la12xx_bbdev_remove
};

RTE_PMD_REGISTER_VDEV(DRIVER_NAME, bbdev_la12xx_pmd_drv);
RTE_PMD_REGISTER_PARAM_STRING(DRIVER_NAME,
	LA12XX_MAX_NB_QUEUES_ARG"=<int>"
	LA12XX_VDEV_MODEM_ID_ARG "=<int> ");
RTE_LOG_REGISTER_DEFAULT(bbdev_la12xx_logtype, NOTICE);
