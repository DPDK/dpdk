/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <rte_time.h>
#include <rte_mbuf.h>
#include <rte_dmadev.h>
#include <rte_malloc.h>
#include <rte_lcore.h>
#include <rte_random.h>

#include "main.h"

#define MAX_DMA_CPL_NB 255

#define TEST_WAIT_U_SECOND 10000

#define CSV_LINE_DMA_FMT "Scenario %u,%u,%s,%u,%u,%u,%u,%.2lf,%" PRIu64 ",%.3lf,%.3lf\n"
#define CSV_LINE_CPU_FMT "Scenario %u,%u,NA,NA,NA,%u,%u,%.2lf,%" PRIu64 ",%.3lf,%.3lf\n"

#define CSV_TOTAL_LINE_FMT "Scenario %u Summary, , , , , ,%u,%.2lf,%.1lf,%.3lf,%.3lf\n"

struct worker_info {
	bool ready_flag;
	bool start_flag;
	bool stop_flag;
	uint32_t total_cpl;
	uint32_t test_cpl;
};

struct sge_info {
	struct rte_dma_sge *srcs;
	struct rte_dma_sge *dsts;
	uint8_t nb_srcs;
	uint8_t nb_dsts;
};

struct lcore_params {
	uint8_t scenario_id;
	unsigned int lcore_id;
	char *dma_name;
	uint16_t worker_id;
	uint16_t dev_id;
	uint32_t nr_buf;
	uint16_t kick_batch;
	uint32_t buf_size;
	uint16_t test_secs;
	struct rte_mbuf **srcs;
	struct rte_mbuf **dsts;
	struct sge_info sge;
	struct rte_dma_op **dma_ops;
	volatile struct worker_info worker_info;
};

static struct rte_mempool *src_pool;
static struct rte_mempool *dst_pool;

static struct lcore_params *lcores[MAX_WORKER_NB];

#define PRINT_ERR(...) print_err(__func__, __LINE__, __VA_ARGS__)

static inline int
__rte_format_printf(3, 4)
print_err(const char *func, int lineno, const char *format, ...)
{
	va_list ap;
	int ret;

	ret = fprintf(stderr, "In %s:%d - ", func, lineno);
	va_start(ap, format);
	ret += vfprintf(stderr, format, ap);
	va_end(ap);

	return ret;
}

static inline void
calc_result(uint32_t buf_size, uint32_t nr_buf, uint16_t nb_workers, uint16_t test_secs,
				uint32_t total_cnt, float *memory, uint32_t *ave_cycle,
				float *bandwidth, float *mops)
{
	float ops;

	*memory = (float)(buf_size * (nr_buf / nb_workers) * 2) / (1024 * 1024);
	*ave_cycle = test_secs * rte_get_timer_hz() / total_cnt;
	ops = (float)total_cnt / test_secs;
	*mops = ops / (1000 * 1000);
	*bandwidth = (ops * buf_size * 8) / (1000 * 1000 * 1000);
}

static void
output_result(struct test_configure *cfg, struct lcore_params *para,
			uint16_t kick_batch, uint64_t ave_cycle, uint32_t buf_size,
			uint32_t nr_buf, float memory, float bandwidth, float mops)
{
	uint16_t ring_size = cfg->ring_size.cur;
	uint8_t scenario_id = cfg->scenario_id;
	uint32_t lcore_id = para->lcore_id;
	char *dma_name = para->dma_name;

	if (cfg->test_type == TEST_TYPE_DMA_MEM_COPY) {
		printf("lcore %u, DMA %s, DMA Ring Size: %u, Kick Batch Size: %u", lcore_id,
		       dma_name, ring_size, kick_batch);
		if (cfg->is_sg)
			printf(" DMA src sges: %u, dst sges: %u",
			       para->sge.nb_srcs, para->sge.nb_dsts);
		printf(".\n");
	} else {
		printf("lcore %u\n", lcore_id);
	}

	printf("Average Cycles/op: %" PRIu64 ", Buffer Size: %u B, Buffer Number: %u, Memory: %.2lf MB, Frequency: %.3lf Ghz.\n",
			ave_cycle, buf_size, nr_buf, memory, rte_get_timer_hz()/1000000000.0);
	printf("Average Bandwidth: %.3lf Gbps, MOps: %.3lf\n", bandwidth, mops);

	if (cfg->test_type == TEST_TYPE_DMA_MEM_COPY)
		output_csv(CSV_LINE_DMA_FMT,
			scenario_id, lcore_id, dma_name, ring_size, kick_batch, buf_size,
			nr_buf, memory, ave_cycle, bandwidth, mops);
	else
		output_csv(CSV_LINE_CPU_FMT,
			scenario_id, lcore_id, buf_size,
			nr_buf, memory, ave_cycle, bandwidth, mops);
}

static inline void
cache_flush_buf(__rte_unused struct rte_mbuf **array,
		__rte_unused uint32_t buf_size,
		__rte_unused uint32_t nr_buf)
{
#ifdef RTE_ARCH_X86_64
	char *data;
	struct rte_mbuf **srcs = array;
	uint32_t i, offset;

	for (i = 0; i < nr_buf; i++) {
		data = rte_pktmbuf_mtod(srcs[i], char *);
		for (offset = 0; offset < buf_size; offset += 64)
			__builtin_ia32_clflush(data + offset);
	}
#endif
}

static int
vchan_data_populate(uint32_t dev_id, struct rte_dma_vchan_conf *qconf,
		    struct test_configure *cfg, uint16_t dev_num)
{
	struct vchan_dev_config *vchan_dconfig;
	struct rte_dma_info info;

	vchan_dconfig = &cfg->dma_config[dev_num].vchan_dev;
	qconf->direction = vchan_dconfig->tdir;

	rte_dma_info_get(dev_id, &info);
	if (!(RTE_BIT64(qconf->direction) & info.dev_capa))
		return -1;

	qconf->nb_desc = cfg->ring_size.cur;

	switch (qconf->direction) {
	case RTE_DMA_DIR_MEM_TO_DEV:
		qconf->dst_port.pcie.vfen = 1;
		qconf->dst_port.port_type = RTE_DMA_PORT_PCIE;
		qconf->dst_port.pcie.coreid = vchan_dconfig->port.pcie.coreid;
		qconf->dst_port.pcie.vfid = vchan_dconfig->port.pcie.vfid;
		qconf->dst_port.pcie.pfid = vchan_dconfig->port.pcie.pfid;
		break;
	case RTE_DMA_DIR_DEV_TO_MEM:
		qconf->src_port.pcie.vfen = 1;
		qconf->src_port.port_type = RTE_DMA_PORT_PCIE;
		qconf->src_port.pcie.coreid = vchan_dconfig->port.pcie.coreid;
		qconf->src_port.pcie.vfid = vchan_dconfig->port.pcie.vfid;
		qconf->src_port.pcie.pfid = vchan_dconfig->port.pcie.pfid;
		break;
	case RTE_DMA_DIR_MEM_TO_MEM:
	case RTE_DMA_DIR_DEV_TO_DEV:
		break;
	}

	return 0;
}

/* Configuration of device. */
static void
configure_dmadev_queue(uint32_t dev_id, struct test_configure *cfg, uint8_t sges_max,
		       uint16_t dev_num)
{
	uint16_t vchan = 0;
	struct rte_dma_info info;
	struct rte_dma_conf dev_config = { .nb_vchans = 1 };
	struct rte_dma_vchan_conf qconf = { 0 };

	if (vchan_data_populate(dev_id, &qconf, cfg, dev_num) != 0)
		rte_exit(EXIT_FAILURE, "Error with vchan data populate.\n");

	if (rte_dma_info_get(dev_id, &info) != 0)
		rte_exit(EXIT_FAILURE, "Error with getting device info.\n");

	if (cfg->use_ops && !(info.dev_capa & RTE_DMA_CAPA_OPS_ENQ_DEQ))
		rte_exit(EXIT_FAILURE, "Error with device %s not support enq_deq ops.\n",
			 info.dev_name);

	if (cfg->use_ops)
		dev_config.flags = RTE_DMA_CFG_FLAG_ENQ_DEQ;

	if (rte_dma_configure(dev_id, &dev_config) != 0)
		rte_exit(EXIT_FAILURE, "Error with dma configure.\n");

	if (rte_dma_vchan_setup(dev_id, vchan, &qconf) != 0)
		rte_exit(EXIT_FAILURE, "Error with queue configuration.\n");

	if (rte_dma_info_get(dev_id, &info) != 0)
		rte_exit(EXIT_FAILURE, "Error with getting device info.\n");

	if (info.nb_vchans != 1)
		rte_exit(EXIT_FAILURE, "Error, no configured queues reported on device id. %u\n",
				dev_id);

	if (info.max_sges < sges_max)
		rte_exit(EXIT_FAILURE, "Error with unsupported max_sges on device id %u.\n",
				dev_id);

	if (rte_dma_start(dev_id) != 0)
		rte_exit(EXIT_FAILURE, "Error with dma start.\n");
}

static int
config_dmadevs(struct test_configure *cfg)
{
	uint32_t nb_workers = cfg->num_worker;
	struct lcore_dma_map_t *ldm;
	uint32_t i;
	int dev_id;
	uint16_t nb_dmadevs = 0;
	uint8_t nb_sges = 0;
	char *dma_name;

	if (cfg->test_type != TEST_TYPE_DMA_MEM_COPY)
		return 0;

	if (cfg->is_sg)
		nb_sges = RTE_MAX(cfg->nb_src_sges, cfg->nb_dst_sges);

	for (i = 0; i < nb_workers; i++) {
		ldm = &cfg->dma_config[i].lcore_dma_map;
		dma_name = ldm->dma_names;
		dev_id = rte_dma_get_dev_id_by_name(dma_name);
		if (dev_id < 0) {
			fprintf(stderr, "Error: Fail to find DMA %s.\n", dma_name);
			goto end;
		}

		ldm->dma_id = dev_id;
		configure_dmadev_queue(dev_id, cfg, nb_sges, nb_dmadevs);
		++nb_dmadevs;
	}

end:
	if (nb_dmadevs < nb_workers) {
		printf("Not enough dmadevs (%u) for all workers (%u).\n", nb_dmadevs, nb_workers);
		return -1;
	}

	printf("Number of used dmadevs: %u.\n", nb_dmadevs);

	return 0;
}

static void
stop_dmadev(struct test_configure *cfg, bool *stopped)
{
	struct lcore_dma_map_t *lcore_dma_map;
	uint32_t i;

	if (*stopped)
		return;

	if (cfg->test_type == TEST_TYPE_DMA_MEM_COPY) {
		for (i = 0; i < cfg->num_worker; i++) {
			lcore_dma_map = &cfg->dma_config[i].lcore_dma_map;
			printf("Stopping dmadev %d\n", lcore_dma_map->dma_id);
			rte_dma_stop(lcore_dma_map->dma_id);
		}
	}
	*stopped = true;
}

static void
error_exit(int dev_id)
{
	rte_dma_stop(dev_id);
	rte_dma_close(dev_id);
	rte_exit(EXIT_FAILURE, "DMA error\n");
}

static inline void
do_dma_submit_and_poll(uint16_t dev_id, uint64_t *async_cnt,
			volatile struct worker_info *worker_info)
{
	int ret;
	uint16_t nr_cpl;

	ret = rte_dma_submit(dev_id, 0);
	if (ret < 0)
		error_exit(dev_id);

	nr_cpl = rte_dma_completed(dev_id, 0, MAX_DMA_CPL_NB, NULL, NULL);
	*async_cnt -= nr_cpl;
	worker_info->total_cpl += nr_cpl;
}

static int
do_dma_submit_and_wait_cpl(uint16_t dev_id, uint64_t async_cnt, bool use_ops)
{
#define MAX_WAIT_MSEC	1000
#define MAX_POLL	1000
#define DEQ_SZ		64
	struct rte_dma_op *op[DEQ_SZ];
	enum rte_dma_vchan_status st;
	uint32_t poll_cnt = 0;
	uint32_t wait_ms = 0;
	uint16_t nr_cpl;

	if (!use_ops)
		rte_dma_submit(dev_id, 0);

	if (rte_dma_vchan_status(dev_id, 0, &st) < 0) {
		rte_delay_ms(MAX_WAIT_MSEC);
		goto wait_cpl;
	}

	while (st == RTE_DMA_VCHAN_ACTIVE && wait_ms++ < MAX_WAIT_MSEC) {
		rte_delay_ms(1);
		rte_dma_vchan_status(dev_id, 0, &st);
	}

wait_cpl:
	while ((async_cnt > 0) && (poll_cnt++ < MAX_POLL)) {
		if (use_ops)
			nr_cpl = rte_dma_dequeue_ops(dev_id, 0, op, DEQ_SZ);
		else
			nr_cpl = rte_dma_completed(dev_id, 0, MAX_DMA_CPL_NB, NULL, NULL);
		async_cnt -= nr_cpl;
	}
	if (async_cnt > 0)
		PRINT_ERR("Error: wait DMA %u failed!\n", dev_id);

	return async_cnt == 0 ? 0 : -1;
}

static inline int
do_dma_plain_mem_copy(void *p)
{
	struct lcore_params *para = (struct lcore_params *)p;
	volatile struct worker_info *worker_info = &(para->worker_info);
	const uint16_t dev_id = para->dev_id;
	const uint32_t nr_buf = para->nr_buf;
	const uint16_t kick_batch = para->kick_batch;
	const uint32_t buf_size = para->buf_size;
	struct rte_mbuf **srcs = para->srcs;
	struct rte_mbuf **dsts = para->dsts;
	uint64_t async_cnt = 0;
	uint32_t i;
	int ret;

	worker_info->stop_flag = false;
	worker_info->ready_flag = true;

	while (!worker_info->start_flag)
		;

	while (1) {
		for (i = 0; i < nr_buf; i++) {
dma_copy:
			ret = rte_dma_copy(dev_id, 0, rte_mbuf_data_iova(srcs[i]),
				rte_mbuf_data_iova(dsts[i]), buf_size, 0);
			if (unlikely(ret < 0)) {
				if (ret == -ENOSPC) {
					do_dma_submit_and_poll(dev_id, &async_cnt, worker_info);
					goto dma_copy;
				} else
					error_exit(dev_id);
			}
			async_cnt++;

			if ((async_cnt % kick_batch) == 0)
				do_dma_submit_and_poll(dev_id, &async_cnt, worker_info);
		}

		if (worker_info->stop_flag)
			break;
	}

	return do_dma_submit_and_wait_cpl(dev_id, async_cnt, false);
}

static inline int
do_dma_sg_mem_copy(void *p)
{
	struct lcore_params *para = (struct lcore_params *)p;
	volatile struct worker_info *worker_info = &(para->worker_info);
	struct rte_dma_sge *src_sges = para->sge.srcs;
	struct rte_dma_sge *dst_sges = para->sge.dsts;
	const uint8_t nb_src_sges = para->sge.nb_srcs;
	const uint8_t nb_dst_sges = para->sge.nb_dsts;
	const uint16_t kick_batch = para->kick_batch;
	const uint16_t dev_id = para->dev_id;
	uint32_t nr_buf = para->nr_buf;
	uint64_t async_cnt = 0;
	uint32_t i, j;
	int ret;

	nr_buf /= RTE_MAX(nb_src_sges, nb_dst_sges);
	worker_info->stop_flag = false;
	worker_info->ready_flag = true;

	while (!worker_info->start_flag)
		;

	while (1) {
		j = 0;
		for (i = 0; i < nr_buf; i++) {
dma_copy:
			ret = rte_dma_copy_sg(dev_id, 0,
				&src_sges[i * nb_src_sges], &dst_sges[j * nb_dst_sges],
				nb_src_sges, nb_dst_sges, 0);
			if (unlikely(ret < 0)) {
				if (ret == -ENOSPC) {
					do_dma_submit_and_poll(dev_id, &async_cnt, worker_info);
					goto dma_copy;
				} else
					error_exit(dev_id);
			}
			async_cnt++;
			j++;

			if ((async_cnt % kick_batch) == 0)
				do_dma_submit_and_poll(dev_id, &async_cnt, worker_info);
		}

		if (worker_info->stop_flag)
			break;
	}

	return do_dma_submit_and_wait_cpl(dev_id, async_cnt, false);
}

static inline int
do_dma_enq_deq_mem_copy(void *p)
{
#define DEQ_SZ 64
	struct lcore_params *para = (struct lcore_params *)p;
	volatile struct worker_info *worker_info = &(para->worker_info);
	struct rte_dma_op **dma_ops = para->dma_ops;
	uint16_t kick_batch = para->kick_batch, sz;
	const uint16_t dev_id = para->dev_id;
	uint32_t nr_buf = para->nr_buf;
	struct rte_dma_op *op[DEQ_SZ];
	uint64_t tenq, tdeq;
	uint16_t enq, deq;
	uint32_t i;

	worker_info->stop_flag = false;
	worker_info->ready_flag = true;

	while (!worker_info->start_flag)
		;

	if (kick_batch > nr_buf)
		kick_batch = nr_buf;

	tenq = 0;
	tdeq = 0;
	while (1) {
		for (i = 0; i < nr_buf; i += kick_batch) {
			sz = RTE_MIN(nr_buf - i, kick_batch);
			enq = rte_dma_enqueue_ops(dev_id, 0, &dma_ops[i], sz);
			while (enq < sz) {
				do {
					deq = rte_dma_dequeue_ops(dev_id, 0, op, DEQ_SZ);
					tdeq += deq;
				} while (deq);
				enq += rte_dma_enqueue_ops(dev_id, 0, &dma_ops[i + enq], sz - enq);
				if (worker_info->stop_flag)
					break;
			}
			tenq += enq;

			worker_info->total_cpl += enq;
		}

		if (worker_info->stop_flag)
			break;
	}

	return do_dma_submit_and_wait_cpl(dev_id, tenq - tdeq, true);
}

static inline int
do_cpu_mem_copy(void *p)
{
	struct lcore_params *para = (struct lcore_params *)p;
	volatile struct worker_info *worker_info = &(para->worker_info);
	const uint32_t nr_buf = para->nr_buf;
	const uint32_t buf_size = para->buf_size;
	struct rte_mbuf **srcs = para->srcs;
	struct rte_mbuf **dsts = para->dsts;
	uint32_t i;

	worker_info->stop_flag = false;
	worker_info->ready_flag = true;

	while (!worker_info->start_flag)
		;

	while (1) {
		for (i = 0; i < nr_buf; i++) {
			const void *src = rte_pktmbuf_mtod(dsts[i], void *);
			void *dst = rte_pktmbuf_mtod(srcs[i], void *);

			/* copy buffer form src to dst */
			rte_memcpy(dst, src, (size_t)buf_size);
			worker_info->total_cpl++;
		}
		if (worker_info->stop_flag)
			break;
	}

	return 0;
}

static void
dummy_free_ext_buf(void *addr, void *opaque)
{
	RTE_SET_USED(addr);
	RTE_SET_USED(opaque);
}

static int
setup_memory_env(struct test_configure *cfg, uint32_t nr_buf,
		 struct rte_mbuf ***srcs, struct rte_mbuf ***dsts,
		 struct rte_dma_sge **src_sges, struct rte_dma_sge **dst_sges,
		 struct rte_dma_op ***dma_ops)
{
	unsigned int cur_buf_size = cfg->buf_size.cur;
	unsigned int buf_size = cur_buf_size + RTE_PKTMBUF_HEADROOM;
	bool is_src_numa_incorrect, is_dst_numa_incorrect;
	unsigned int nr_sockets;
	uintptr_t ops;
	uint32_t i;

	nr_sockets = rte_socket_count();
	is_src_numa_incorrect = (cfg->src_numa_node >= nr_sockets);
	is_dst_numa_incorrect = (cfg->dst_numa_node >= nr_sockets);

	if (is_src_numa_incorrect || is_dst_numa_incorrect) {
		PRINT_ERR("Error: Incorrect NUMA config for %s.\n",
			(is_src_numa_incorrect && is_dst_numa_incorrect) ? "source & destination" :
				(is_src_numa_incorrect) ? "source" : "destination");
		return -1;
	}

	if (buf_size > UINT16_MAX) {
		PRINT_ERR("Error: Invalid buf size: %u\n", cur_buf_size);
		return -1;
	}

	src_pool = rte_pktmbuf_pool_create("Benchmark_DMA_SRC",
			nr_buf,
			0,
			0,
			buf_size,
			cfg->src_numa_node);
	if (src_pool == NULL) {
		PRINT_ERR("Error with source mempool creation.\n");
		return -1;
	}

	dst_pool = rte_pktmbuf_pool_create("Benchmark_DMA_DST",
			nr_buf,
			0,
			0,
			buf_size,
			cfg->dst_numa_node);
	if (dst_pool == NULL) {
		PRINT_ERR("Error with destination mempool creation.\n");
		return -1;
	}

	*srcs = rte_malloc(NULL, nr_buf * sizeof(struct rte_mbuf *), 0);
	if (*srcs == NULL) {
		printf("Error: srcs malloc failed.\n");
		return -1;
	}

	*dsts = rte_malloc(NULL, nr_buf * sizeof(struct rte_mbuf *), 0);
	if (*dsts == NULL) {
		printf("Error: dsts malloc failed.\n");
		return -1;
	}

	if (rte_pktmbuf_alloc_bulk(src_pool, *srcs, nr_buf) != 0) {
		printf("alloc src mbufs failed.\n");
		return -1;
	}

	if (rte_pktmbuf_alloc_bulk(dst_pool, *dsts, nr_buf) != 0) {
		printf("alloc dst mbufs failed.\n");
		return -1;
	}

	for (i = 0; i < nr_buf; i++) {
		memset(rte_pktmbuf_mtod((*srcs)[i], void *), rte_rand(), cur_buf_size);
		memset(rte_pktmbuf_mtod((*dsts)[i], void *), 0, cur_buf_size);
	}

	if (cfg->is_sg) {
		uint8_t nb_src_sges = cfg->nb_src_sges;
		uint8_t nb_dst_sges = cfg->nb_dst_sges;
		uint32_t sglen_src, sglen_dst;

		*src_sges = rte_zmalloc(NULL, nr_buf * sizeof(struct rte_dma_sge),
					RTE_CACHE_LINE_SIZE);
		if (*src_sges == NULL) {
			printf("Error: src_sges array malloc failed.\n");
			return -1;
		}

		*dst_sges = rte_zmalloc(NULL, nr_buf * sizeof(struct rte_dma_sge),
					RTE_CACHE_LINE_SIZE);
		if (*dst_sges == NULL) {
			printf("Error: dst_sges array malloc failed.\n");
			return -1;
		}

		sglen_src = cur_buf_size / nb_src_sges;
		sglen_dst = cur_buf_size / nb_dst_sges;

		for (i = 0; i < nr_buf; i++) {
			(*src_sges)[i].addr = rte_pktmbuf_iova((*srcs)[i]);
			(*src_sges)[i].length = sglen_src;
			if (!((i+1) % nb_src_sges))
				(*src_sges)[i].length += (cur_buf_size % nb_src_sges);

			(*dst_sges)[i].addr = rte_pktmbuf_iova((*dsts)[i]);
			(*dst_sges)[i].length = sglen_dst;
			if (!((i+1) % nb_dst_sges))
				(*dst_sges)[i].length += (cur_buf_size % nb_dst_sges);
		}

		if (cfg->use_ops) {
			nr_buf /= RTE_MAX(nb_src_sges, nb_dst_sges);
			*dma_ops = rte_zmalloc(NULL, nr_buf * (sizeof(struct rte_dma_op *)),
					       RTE_CACHE_LINE_SIZE);
			if (*dma_ops == NULL) {
				printf("Error: dma_ops container malloc failed.\n");
				return -1;
			}

			ops = (uintptr_t)rte_zmalloc(
				NULL,
				nr_buf * (sizeof(struct rte_dma_op) + ((nb_src_sges + nb_dst_sges) *
								       sizeof(struct rte_dma_sge))),
				RTE_CACHE_LINE_SIZE);
			if (ops == 0) {
				printf("Error: dma_ops malloc failed.\n");
				return -1;
			}

			for (i = 0; i < nr_buf; i++)
				(*dma_ops)[i] =
					(struct rte_dma_op *)(ops +
							      (i * (sizeof(struct rte_dma_op) +
								    ((nb_src_sges + nb_dst_sges) *
								     sizeof(struct rte_dma_sge)))));
		}
	}

	return 0;
}

static void
teardown_memory_env(uint32_t nr_buf, struct rte_mbuf **srcs, struct rte_mbuf **dsts,
		    struct rte_dma_sge *src_sges, struct rte_dma_sge *dst_sges,
		    struct rte_dma_op **dma_ops)
{
	/* free mbufs used in the test */
	if (srcs != NULL)
		rte_pktmbuf_free_bulk(srcs, nr_buf);
	if (dsts != NULL)
		rte_pktmbuf_free_bulk(dsts, nr_buf);

	/* free the points for the mbufs */
	rte_free(srcs);
	srcs = NULL;
	rte_free(dsts);
	dsts = NULL;

	rte_mempool_free(src_pool);
	src_pool = NULL;

	rte_mempool_free(dst_pool);
	dst_pool = NULL;

	/* free sges for mbufs */
	rte_free(src_sges);
	src_sges = NULL;

	rte_free(dst_sges);
	dst_sges = NULL;

	rte_free(dma_ops);
}

static uint32_t
align_buffer_count(struct test_configure *cfg, uint32_t *nr_sgsrc, uint32_t *nr_sgdst)
{
	uint16_t nb_workers = cfg->num_worker;
	uint32_t nr_buf;

	nr_buf = (cfg->mem_size.cur * 1024 * 1024) / (cfg->buf_size.cur * 2);
	nr_buf -= (nr_buf % nb_workers);

	if (nr_sgsrc == NULL || nr_sgdst == NULL)
		return nr_buf;

	if (cfg->is_sg) {
		nr_buf /= nb_workers;
		nr_buf -= nr_buf % (cfg->nb_src_sges * cfg->nb_dst_sges);
		nr_buf *= nb_workers;

		if (cfg->nb_dst_sges > cfg->nb_src_sges) {
			*nr_sgsrc = (nr_buf / cfg->nb_dst_sges * cfg->nb_src_sges);
			*nr_sgdst = nr_buf;
		} else {
			*nr_sgsrc = nr_buf;
			*nr_sgdst = (nr_buf / cfg->nb_src_sges * cfg->nb_dst_sges);
		}
	}

	return nr_buf;
}

static lcore_function_t *
get_work_function(struct test_configure *cfg)
{
	lcore_function_t *fn;

	if (cfg->test_type == TEST_TYPE_DMA_MEM_COPY) {
		if (!cfg->is_sg)
			fn = do_dma_plain_mem_copy;
		else {
			if (cfg->use_ops)
				fn = do_dma_enq_deq_mem_copy;
			else
				fn = do_dma_sg_mem_copy;
		}
	} else {
		fn = do_cpu_mem_copy;
	}

	return fn;
}

static int
attach_ext_buffer(struct vchan_dev_config *vchan_dev, struct lcore_params *lcore, bool is_sg,
		  uint32_t nr_sgsrc, uint32_t nr_sgdst)
{
	static struct rte_mbuf_ext_shared_info *ext_buf_info;
	struct rte_dma_sge **src_sges, **dst_sges;
	struct rte_mbuf **srcs, **dsts;
	unsigned int cur_buf_size;
	unsigned int buf_size;
	uint32_t nr_buf;
	uint32_t i;

	cur_buf_size = lcore->buf_size;
	buf_size = cur_buf_size + RTE_PKTMBUF_HEADROOM;
	nr_buf = lcore->nr_buf;
	srcs = lcore->srcs;
	dsts = lcore->dsts;

	ext_buf_info = rte_malloc(NULL, sizeof(struct rte_mbuf_ext_shared_info), 0);
	if (ext_buf_info == NULL) {
		printf("Error: ext_buf_info malloc failed.\n");
		return -1;
	}
	ext_buf_info->free_cb = dummy_free_ext_buf;
	ext_buf_info->fcb_opaque = NULL;

	if (vchan_dev->tdir == RTE_DMA_DIR_DEV_TO_MEM) {
		for (i = 0; i < nr_buf; i++) {
			/* Using mbuf structure to hold remote iova address. */
			rte_pktmbuf_attach_extbuf(srcs[i],
				(void *)(vchan_dev->raddr + (i * buf_size)),
				(rte_iova_t)(vchan_dev->raddr + (i * buf_size)), 0, ext_buf_info);
			rte_mbuf_ext_refcnt_update(ext_buf_info, 1);
		}
	}

	if (vchan_dev->tdir == RTE_DMA_DIR_MEM_TO_DEV) {
		for (i = 0; i < nr_buf; i++) {
			/* Using mbuf structure to hold remote iova address. */
			rte_pktmbuf_attach_extbuf(dsts[i],
				(void *)(vchan_dev->raddr + (i * buf_size)),
				(rte_iova_t)(vchan_dev->raddr + (i * buf_size)), 0, ext_buf_info);
			rte_mbuf_ext_refcnt_update(ext_buf_info, 1);
		}
	}

	if (is_sg) {
		uint8_t nb_src_sges = lcore->sge.nb_srcs;
		uint8_t nb_dst_sges = lcore->sge.nb_dsts;
		uint32_t sglen_src, sglen_dst;

		src_sges = &lcore->sge.srcs;
		dst_sges = &lcore->sge.dsts;

		sglen_src = cur_buf_size / nb_src_sges;
		sglen_dst = cur_buf_size / nb_dst_sges;

		if (vchan_dev->tdir == RTE_DMA_DIR_DEV_TO_MEM) {
			for (i = 0; i < nr_sgsrc; i++) {
				(*src_sges)[i].addr = rte_pktmbuf_iova(srcs[i]);
				(*src_sges)[i].length = sglen_src;
				if (!((i+1) % nb_src_sges))
					(*src_sges)[i].length += (cur_buf_size % nb_src_sges);
			}
		}

		if (vchan_dev->tdir == RTE_DMA_DIR_MEM_TO_DEV) {
			for (i = 0; i < nr_sgdst; i++) {
				(*dst_sges)[i].addr = rte_pktmbuf_iova(dsts[i]);
				(*dst_sges)[i].length = sglen_dst;
				if (!((i+1) % nb_dst_sges))
					(*dst_sges)[i].length += (cur_buf_size % nb_dst_sges);
			}
		}
	}

	return 0;
}

static int
verify_data(struct test_configure *cfg, struct rte_mbuf **srcs, struct rte_mbuf **dsts,
	    uint32_t nr_buf)
{
	struct rte_mbuf **src_buf = NULL, **dst_buf = NULL;
	uint32_t nr_buf_pt = nr_buf / cfg->num_worker;
	struct vchan_dev_config *vchan_dev = NULL;
	unsigned int buf_size = cfg->buf_size.cur;
	uint32_t offset, work_idx, i, j;

	for (work_idx = 0; work_idx < cfg->num_worker; work_idx++)  {
		vchan_dev = &cfg->dma_config[work_idx].vchan_dev;
		offset = nr_buf / cfg->num_worker * work_idx;
		src_buf = srcs + offset;
		dst_buf = dsts + offset;

		if (vchan_dev->tdir == RTE_DMA_DIR_MEM_TO_MEM && !cfg->is_sg) {
			for (i = 0; i < nr_buf_pt; i++) {
				if (memcmp(rte_pktmbuf_mtod(src_buf[i], void *),
							    rte_pktmbuf_mtod(dst_buf[i], void *),
							    cfg->buf_size.cur) != 0) {
					printf("Copy validation fails for buffer number %d\n", i);
					return -1;
				}
			}
			continue;
		}

		if (vchan_dev->tdir == RTE_DMA_DIR_MEM_TO_MEM && cfg->is_sg) {
			size_t src_remsz = buf_size % cfg->nb_src_sges;
			size_t dst_remsz = buf_size % cfg->nb_dst_sges;
			size_t src_sz = buf_size / cfg->nb_src_sges;
			size_t dst_sz = buf_size / cfg->nb_dst_sges;
			uint8_t src[buf_size], dst[buf_size];
			uint8_t *sbuf, *dbuf, *ptr;

			for (i = 0; i < (nr_buf_pt / RTE_MAX(cfg->nb_src_sges, cfg->nb_dst_sges));
				i++) {
				sbuf = src;
				dbuf = dst;
				ptr = NULL;

				for (j = 0; j < cfg->nb_src_sges; j++) {
					ptr = rte_pktmbuf_mtod(src_buf[i * cfg->nb_src_sges + j],
						uint8_t *);
					memcpy(sbuf, ptr, src_sz);
					sbuf += src_sz;
				}
				if (src_remsz)
					memcpy(sbuf, ptr + src_sz, src_remsz);

				for (j = 0; j < cfg->nb_dst_sges; j++) {
					ptr = rte_pktmbuf_mtod(dst_buf[i * cfg->nb_dst_sges + j],
						uint8_t *);
					memcpy(dbuf, ptr, dst_sz);
					dbuf += dst_sz;
				}
				if (dst_remsz)
					memcpy(dbuf, ptr + dst_sz, dst_remsz);

				if (memcmp(src, dst, buf_size) != 0) {
					printf("SG Copy validation fails for buffer number %d\n",
						i * cfg->nb_src_sges);
					return -1;
				}
			}
			continue;
		}
	}

	return 0;
}

static int
setup_worker(struct test_configure *cfg, uint32_t nr_buf,
	     struct rte_mbuf **srcs, struct rte_mbuf **dsts,
	     struct rte_dma_sge *src_sges, struct rte_dma_sge *dst_sges,
	     struct rte_dma_op **dma_ops,
	     uint32_t nr_sgsrc, uint32_t nr_sgdst)
{
	struct lcore_dma_map_t *lcore_dma_map = NULL;
	struct vchan_dev_config *vchan_dev = NULL;
	unsigned int buf_size = cfg->buf_size.cur;
	uint16_t kick_batch = cfg->kick_batch.cur;
	uint16_t test_secs = global_cfg.test_secs;
	uint16_t nb_workers = cfg->num_worker;
	unsigned int lcore_id = 0;
	uint32_t i, j, k;
	uint32_t nr_ops;
	uint32_t offset;

	for (i = 0; i < nb_workers; i++) {
		lcore_dma_map = &cfg->dma_config[i].lcore_dma_map;
		vchan_dev = &cfg->dma_config[i].vchan_dev;

		lcore_id = lcore_dma_map->lcore;
		offset = nr_buf / nb_workers * i;
		lcores[i] = rte_malloc(NULL, sizeof(struct lcore_params), 0);
		if (lcores[i] == NULL) {
			printf("lcore parameters malloc failure for lcore %d\n", lcore_id);
			return -1;
		}
		if (cfg->test_type == TEST_TYPE_DMA_MEM_COPY) {
			lcores[i]->dma_name = lcore_dma_map->dma_names;
			lcores[i]->dev_id = lcore_dma_map->dma_id;
			lcores[i]->kick_batch = kick_batch;
		}

		lcores[i]->worker_id = i;
		lcores[i]->nr_buf = (uint32_t)(nr_buf / nb_workers);
		lcores[i]->buf_size = buf_size;
		lcores[i]->test_secs = test_secs;
		lcores[i]->srcs = srcs + offset;
		lcores[i]->dsts = dsts + offset;
		lcores[i]->scenario_id = cfg->scenario_id;
		lcores[i]->lcore_id = lcore_id;

		if (cfg->is_sg) {
			lcores[i]->sge.nb_srcs = cfg->nb_src_sges;
			lcores[i]->sge.nb_dsts = cfg->nb_dst_sges;
			lcores[i]->sge.srcs = src_sges + (nr_sgsrc / nb_workers * i);
			lcores[i]->sge.dsts = dst_sges + (nr_sgdst / nb_workers * i);
		}

		if (vchan_dev->tdir == RTE_DMA_DIR_DEV_TO_MEM ||
		    vchan_dev->tdir == RTE_DMA_DIR_MEM_TO_DEV) {
			if (attach_ext_buffer(vchan_dev, lcores[i], cfg->is_sg,
					      (nr_sgsrc/nb_workers), (nr_sgdst/nb_workers)) < 0)
				return -1;
		}

		if (cfg->is_sg && cfg->use_ops) {
			nr_ops = nr_buf / RTE_MAX(cfg->nb_src_sges, cfg->nb_dst_sges);
			lcores[i]->nr_buf = nr_ops / nb_workers;
			lcores[i]->dma_ops = dma_ops + (nr_ops / nb_workers * i);
			for (j = 0; j < (nr_ops / nb_workers); j++) {
				for (k = 0; k < cfg->nb_src_sges; k++)
					lcores[i]->dma_ops[j]->src_dst_seg[k] =
						lcores[i]->sge.srcs[(j * cfg->nb_src_sges) + k];

				for (k = 0; k < cfg->nb_dst_sges; k++)
					lcores[i]->dma_ops[j]->src_dst_seg[k + cfg->nb_src_sges] =
						lcores[i]->sge.dsts[(j * cfg->nb_dst_sges) + k];

				lcores[i]->dma_ops[j]->nb_src = cfg->nb_src_sges;
				lcores[i]->dma_ops[j]->nb_dst = cfg->nb_dst_sges;
				lcores[i]->dma_ops[j]->vchan = 0;
			}
		}

		rte_eal_remote_launch(get_work_function(cfg), (void *)(lcores[i]), lcore_id);
	}

	return 0;
}

static void
teardown_worker_res(struct test_configure *cfg, uint32_t nr_buf,
		    struct rte_mbuf **srcs, struct rte_mbuf **dsts)
{
	uint16_t nb_workers = cfg->num_worker;
	struct vchan_dev_config *vchan_dev;
	struct rte_mbuf **m;
	uint32_t offset;
	uint32_t i, j;

	for (i = 0; i < nb_workers; i++) {
		struct rte_mbuf **sbuf = NULL, **dbuf = NULL;
		vchan_dev = &cfg->dma_config[i].vchan_dev;
		offset = nr_buf / nb_workers * i;
		m = NULL;
		if (vchan_dev->tdir == RTE_DMA_DIR_DEV_TO_MEM) {
			sbuf = srcs + offset;
			m = sbuf;
		} else if (vchan_dev->tdir == RTE_DMA_DIR_MEM_TO_DEV) {
			dbuf = dsts + offset;
			m = dbuf;
		}

		if (m) {
			for (j = 0; j < (nr_buf / nb_workers); j++)
				rte_pktmbuf_detach_extbuf(m[j]);

			if (m[0]->shinfo && rte_mbuf_ext_refcnt_read(m[0]->shinfo) == 0)
				rte_free(m[0]->shinfo);
		}

		rte_free(lcores[i]);
		lcores[i] = NULL;
	}
}

int
mem_copy_benchmark(struct test_configure *cfg)
{
	struct rte_mbuf **srcs = NULL, **dsts = NULL;
	struct rte_dma_sge *src_sges = NULL, *dst_sges = NULL;
	struct vchan_dev_config *vchan_dev = NULL;
	unsigned int buf_size = cfg->buf_size.cur;
	uint16_t kick_batch = cfg->kick_batch.cur;
	uint16_t test_secs = global_cfg.test_secs;
	uint16_t nb_workers = cfg->num_worker;
	uint32_t nr_sgsrc = 0, nr_sgdst = 0;
	struct rte_dma_op **dma_ops = NULL;
	float bandwidth, bandwidth_total;
	uint32_t avg_cycles_total;
	bool dev_stopped = false;
	uint32_t avg_cycles = 0;
	float mops, mops_total;
	float memory = 0;
	uint32_t nr_buf;
	int ret = -1;
	uint32_t i;

	nr_buf = align_buffer_count(cfg, &nr_sgsrc, &nr_sgdst);

	if (setup_memory_env(cfg, nr_buf, &srcs, &dsts, &src_sges, &dst_sges, &dma_ops) < 0)
		goto out;

	if (config_dmadevs(cfg) < 0)
		goto out;

	if (global_cfg.cache_flush > 0) {
		cache_flush_buf(srcs, buf_size, nr_buf);
		cache_flush_buf(dsts, buf_size, nr_buf);
		rte_mb();
	}

	printf("Start testing....\n");

	ret = setup_worker(cfg, nr_buf, srcs, dsts, src_sges, dst_sges, dma_ops,
			   nr_sgsrc, nr_sgdst);
	if (ret != 0)
		goto stop_dmadev;

	while (1) {
		bool ready = true;
		for (i = 0; i < nb_workers; i++) {
			if (lcores[i]->worker_info.ready_flag == false) {
				ready = 0;
				break;
			}
		}
		if (ready)
			break;
	}

	for (i = 0; i < nb_workers; i++)
		lcores[i]->worker_info.start_flag = true;

	usleep(TEST_WAIT_U_SECOND);
	for (i = 0; i < nb_workers; i++)
		lcores[i]->worker_info.test_cpl = lcores[i]->worker_info.total_cpl;

	usleep(test_secs * 1000 * 1000);
	for (i = 0; i < nb_workers; i++)
		lcores[i]->worker_info.test_cpl = lcores[i]->worker_info.total_cpl -
						lcores[i]->worker_info.test_cpl;

	for (i = 0; i < nb_workers; i++)
		lcores[i]->worker_info.stop_flag = true;

	rte_eal_mp_wait_lcore();

	stop_dmadev(cfg, &dev_stopped);

	ret = verify_data(cfg, srcs, dsts, nr_buf);
	if (ret != 0)
		goto out;

	mops_total = 0;
	bandwidth_total = 0;
	avg_cycles_total = 0;
	for (i = 0; i < nb_workers; i++) {
		vchan_dev = &cfg->dma_config[i].vchan_dev;
		calc_result(buf_size, nr_buf, nb_workers, test_secs,
			lcores[i]->worker_info.test_cpl,
			&memory, &avg_cycles, &bandwidth, &mops);
		printf("Direction: %s\n", vchan_dev->tdir == 0 ? "mem2mem" :
			vchan_dev->tdir == 1 ? "mem2dev" : "dev2mem");
		output_result(cfg, lcores[i], kick_batch, avg_cycles, buf_size,
			nr_buf / nb_workers, memory, bandwidth, mops);
		mops_total += mops;
		bandwidth_total += bandwidth;
		avg_cycles_total += avg_cycles;
	}
	printf("\nAverage Cycles/op per worker: %.1lf, Total Bandwidth: %.3lf Gbps, Total MOps: %.3lf\n",
		(avg_cycles_total * (float) 1.0) / nb_workers, bandwidth_total, mops_total);
	output_csv(CSV_TOTAL_LINE_FMT, cfg->scenario_id, nr_buf, memory * nb_workers,
			(avg_cycles_total * (float) 1.0) / nb_workers, bandwidth_total, mops_total);

stop_dmadev:
	stop_dmadev(cfg, &dev_stopped);

out:
	teardown_worker_res(cfg, nr_buf, srcs, dsts);
	teardown_memory_env(nr_buf, srcs, dsts, src_sges, dst_sges, dma_ops);

	return ret;
}
