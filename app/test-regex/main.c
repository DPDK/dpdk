/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>

#include <rte_eal.h>
#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_cycles.h>
#include <rte_regexdev.h>

#define MAX_FILE_NAME 255
#define MBUF_CACHE_SIZE 256
#define MBUF_SIZE (1 << 8)
#define START_BURST_SIZE 32u

enum app_args {
	ARG_HELP,
	ARG_RULES_FILE_NAME,
	ARG_DATA_FILE_NAME,
	ARG_NUM_OF_JOBS,
	ARG_PERF_MODE,
	ARG_NUM_OF_ITERATIONS,
	ARG_NUM_OF_QPS,
	ARG_NUM_OF_LCORES,
	ARG_NUM_OF_MBUF_SEGS,
};

struct job_ctx {
	struct rte_mbuf *mbuf;
};

struct qp_params {
	uint32_t total_enqueue;
	uint32_t total_dequeue;
	uint32_t total_matches;
	struct rte_regex_ops **ops;
	struct job_ctx *jobs_ctx;
	char *buf;
	uint64_t start;
	uint64_t cycles;
};

struct qps_per_lcore {
	unsigned int lcore_id;
	int socket;
	uint16_t qp_id_base;
	uint16_t nb_qps;
};

struct regex_conf {
	uint32_t nb_jobs;
	bool perf_mode;
	uint32_t nb_iterations;
	char *data_file;
	uint8_t nb_max_matches;
	uint32_t nb_qps;
	uint16_t qp_id_base;
	char *data_buf;
	long data_len;
	long job_len;
	uint32_t nb_segs;
};

static void
usage(const char *prog_name)
{
	printf("%s [EAL options] --\n"
		" --rules NAME: precompiled rules file\n"
		" --data NAME: data file to use\n"
		" --nb_jobs: number of jobs to use\n"
		" --perf N: only outputs the performance data\n"
		" --nb_iter N: number of iteration to run\n"
		" --nb_qps N: number of queues to use\n"
		" --nb_lcores N: number of lcores to use\n"
		" --nb_segs N: number of mbuf segments\n",
		prog_name);
}

static void
args_parse(int argc, char **argv, char *rules_file, char *data_file,
	   uint32_t *nb_jobs, bool *perf_mode, uint32_t *nb_iterations,
	   uint32_t *nb_qps, uint32_t *nb_lcores, uint32_t *nb_segs)
{
	char **argvopt;
	int opt;
	int opt_idx;
	size_t len;
	static struct option lgopts[] = {
		{ "help",  0, 0, ARG_HELP},
		/* Rules database file to load. */
		{ "rules",  1, 0, ARG_RULES_FILE_NAME},
		/* Data file to load. */
		{ "data",  1, 0, ARG_DATA_FILE_NAME},
		/* Number of jobs to create. */
		{ "nb_jobs",  1, 0, ARG_NUM_OF_JOBS},
		/* Perf test only */
		{ "perf", 0, 0, ARG_PERF_MODE},
		/* Number of iterations to run with perf test */
		{ "nb_iter", 1, 0, ARG_NUM_OF_ITERATIONS},
		/* Number of QPs. */
		{ "nb_qps", 1, 0, ARG_NUM_OF_QPS},
		/* Number of lcores. */
		{ "nb_lcores", 1, 0, ARG_NUM_OF_LCORES},
		/* Number of mbuf segments. */
		{ "nb_segs", 1, 0, ARG_NUM_OF_MBUF_SEGS},
		/* End of options */
		{ 0, 0, 0, 0 }
	};

	argvopt = argv;
	while ((opt = getopt_long(argc, argvopt, "",
				lgopts, &opt_idx)) != EOF) {
		switch (opt) {
		case ARG_RULES_FILE_NAME:
			len = strnlen(optarg, MAX_FILE_NAME - 1);
			if (len == MAX_FILE_NAME)
				rte_exit(EXIT_FAILURE,
					 "Rule file name to long max %d\n",
					 MAX_FILE_NAME - 1);
			strncpy(rules_file, optarg, MAX_FILE_NAME - 1);
			break;
		case ARG_DATA_FILE_NAME:
			len = strnlen(optarg, MAX_FILE_NAME - 1);
			if (len == MAX_FILE_NAME)
				rte_exit(EXIT_FAILURE,
					 "Data file name to long max %d\n",
					 MAX_FILE_NAME - 1);
			strncpy(data_file, optarg, MAX_FILE_NAME - 1);
			break;
		case ARG_NUM_OF_JOBS:
			*nb_jobs = atoi(optarg);
			break;
		case ARG_PERF_MODE:
			*perf_mode = true;
			break;
		case ARG_NUM_OF_ITERATIONS:
			*nb_iterations = atoi(optarg);
			break;
		case ARG_NUM_OF_QPS:
			*nb_qps = atoi(optarg);
			break;
		case ARG_NUM_OF_LCORES:
			*nb_lcores = atoi(optarg);
			break;
		case ARG_NUM_OF_MBUF_SEGS:
			*nb_segs = atoi(optarg);
			break;
		case ARG_HELP:
			usage(argv[0]);
			break;
		default:
			usage(argv[0]);
			rte_exit(EXIT_FAILURE, "Invalid option: %s\n", argv[optind]);
			break;
		}
	}

	if (!perf_mode)
		*nb_iterations = 1;
}

static long
read_file(char *file, char **buf)
{
	FILE *fp;
	long buf_len = 0;
	size_t read_len;
	int res = 0;

	fp = fopen(file, "r");
	if (!fp)
		return -EIO;
	if (fseek(fp, 0L, SEEK_END) == 0) {
		buf_len = ftell(fp);
		if (buf_len == -1) {
			res = EIO;
			goto error;
		}
		*buf = rte_malloc(NULL, sizeof(char) * (buf_len + 1), 4096);
		if (!*buf) {
			res = ENOMEM;
			goto error;
		}
		if (fseek(fp, 0L, SEEK_SET) != 0) {
			res = EIO;
			goto error;
		}
		read_len = fread(*buf, sizeof(char), buf_len, fp);
		if (read_len != (unsigned long)buf_len) {
			res = EIO;
			goto error;
		}
	}
	fclose(fp);
	return buf_len;
error:
	printf("Error, can't open file %s\n, err = %d", file, res);
	if (fp)
		fclose(fp);
	rte_free(*buf);
	return -res;
}

static int
clone_buf(char *data_buf, char **buf, long data_len)
{
	char *dest_buf;
	dest_buf =
		rte_malloc(NULL, sizeof(char) * (data_len + 1), 4096);
	if (!dest_buf)
		return -ENOMEM;
	memcpy(dest_buf, data_buf, data_len + 1);
	*buf = dest_buf;
	return 0;
}

static int
init_port(uint16_t *nb_max_payload, char *rules_file, uint8_t *nb_max_matches,
	  uint32_t nb_qps)
{
	uint16_t id;
	uint16_t qp_id;
	uint16_t num_devs;
	char *rules = NULL;
	long rules_len;
	struct rte_regexdev_info info;
	struct rte_regexdev_config dev_conf = {
		.nb_queue_pairs = nb_qps,
		.nb_groups = 1,
	};
	struct rte_regexdev_qp_conf qp_conf = {
		.nb_desc = 1024,
		.qp_conf_flags = 0,
	};
	int res = 0;

	num_devs = rte_regexdev_count();
	if (num_devs == 0) {
		printf("Error, no devices detected.\n");
		return -EINVAL;
	}

	rules_len = read_file(rules_file, &rules);
	if (rules_len < 0) {
		printf("Error, can't read rules files.\n");
		res = -EIO;
		goto error;
	}

	for (id = 0; id < num_devs; id++) {
		res = rte_regexdev_info_get(id, &info);
		if (res != 0) {
			printf("Error, can't get device info.\n");
			goto error;
		}
		printf(":: initializing dev: %d\n", id);
		*nb_max_matches = info.max_matches;
		*nb_max_payload = info.max_payload_size;
		if (info.regexdev_capa & RTE_REGEXDEV_SUPP_MATCH_AS_END_F)
			dev_conf.dev_cfg_flags |=
			RTE_REGEXDEV_CFG_MATCH_AS_END_F;
		dev_conf.nb_max_matches = info.max_matches;
		dev_conf.nb_rules_per_group = info.max_rules_per_group;
		dev_conf.rule_db_len = rules_len;
		dev_conf.rule_db = rules;
		res = rte_regexdev_configure(id, &dev_conf);
		if (res < 0) {
			printf("Error, can't configure device %d.\n", id);
			goto error;
		}
		if (info.regexdev_capa & RTE_REGEXDEV_CAPA_QUEUE_PAIR_OOS_F)
			qp_conf.qp_conf_flags |=
			RTE_REGEX_QUEUE_PAIR_CFG_OOS_F;
		for (qp_id = 0; qp_id < nb_qps; qp_id++) {
			res = rte_regexdev_queue_pair_setup(id, qp_id,
							    &qp_conf);
			if (res < 0) {
				printf("Error, can't setup queue pair %u for "
				       "device %d.\n", qp_id, id);
				goto error;
			}
		}
		printf(":: initializing device: %d done\n", id);
	}
	rte_free(rules);
	return 0;
error:
	rte_free(rules);
	return res;
}

static void
extbuf_free_cb(void *addr __rte_unused, void *fcb_opaque __rte_unused)
{
}

static inline struct rte_mbuf *
regex_create_segmented_mbuf(struct rte_mempool *mbuf_pool, int pkt_len,
		int nb_segs, void *buf) {

	struct rte_mbuf *m = NULL, *mbuf = NULL;
	uint8_t *dst;
	char *src = buf;
	int data_len = 0;
	int i, size;
	int t_len;

	if (pkt_len < 1) {
		printf("Packet size must be 1 or more (is %d)\n", pkt_len);
		return NULL;
	}

	if (nb_segs < 1) {
		printf("Number of segments must be 1 or more (is %d)\n",
				nb_segs);
		return NULL;
	}

	t_len = pkt_len >= nb_segs ? (pkt_len / nb_segs +
				     !!(pkt_len % nb_segs)) : 1;
	size = pkt_len;

	/* Create chained mbuf_src and fill it with buf data */
	for (i = 0; size > 0; i++) {

		m = rte_pktmbuf_alloc(mbuf_pool);
		if (i == 0)
			mbuf = m;

		if (m == NULL) {
			printf("Cannot create segment for source mbuf");
			goto fail;
		}

		data_len = size > t_len ? t_len : size;
		memset(rte_pktmbuf_mtod(m, uint8_t *), 0,
				rte_pktmbuf_tailroom(m));
		memcpy(rte_pktmbuf_mtod(m, uint8_t *), src, data_len);
		dst = (uint8_t *)rte_pktmbuf_append(m, data_len);
		if (dst == NULL) {
			printf("Cannot append %d bytes to the mbuf\n",
					data_len);
			goto fail;
		}

		if (mbuf != m)
			rte_pktmbuf_chain(mbuf, m);
		src += data_len;
		size -= data_len;

	}
	return mbuf;

fail:
	rte_pktmbuf_free(mbuf);
	return NULL;
}

static int
run_regex(void *args)
{
	struct regex_conf *rgxc = args;
	uint32_t nb_jobs = rgxc->nb_jobs;
	uint32_t nb_segs = rgxc->nb_segs;
	uint32_t nb_iterations = rgxc->nb_iterations;
	uint8_t nb_max_matches = rgxc->nb_max_matches;
	uint32_t nb_qps = rgxc->nb_qps;
	uint16_t qp_id_base  = rgxc->qp_id_base;
	char *data_buf = rgxc->data_buf;
	long data_len = rgxc->data_len;
	long job_len = rgxc->job_len;
	long remainder;
	long act_job_len = 0;
	bool last_job = false;
	char *buf = NULL;
	uint32_t actual_jobs = 0;
	uint32_t i;
	uint32_t job_id;
	uint16_t qp_id;
	uint16_t dev_id = 0;
	uint8_t nb_matches;
	struct rte_regexdev_match *match;
	long pos;
	unsigned long d_ind = 0;
	struct rte_mbuf_ext_shared_info shinfo;
	int res = 0;
	long double time;
	struct rte_mempool *mbuf_mp;
	struct qp_params *qp;
	struct qp_params *qps = NULL;
	bool update;
	uint16_t qps_used = 0;
	char mbuf_pool[16];

	shinfo.free_cb = extbuf_free_cb;
	snprintf(mbuf_pool,
		 sizeof(mbuf_pool),
		 "mbuf_pool_%2u", qp_id_base);
	mbuf_mp = rte_pktmbuf_pool_create(mbuf_pool,
			rte_align32pow2(nb_jobs * nb_qps * nb_segs),
			0, 0, (nb_segs == 1) ? MBUF_SIZE :
			(rte_align32pow2(job_len) / nb_segs +
			RTE_PKTMBUF_HEADROOM),
			rte_socket_id());
	if (mbuf_mp == NULL) {
		printf("Error, can't create memory pool\n");
		return -ENOMEM;
	}

	qps = rte_malloc(NULL, sizeof(*qps) * nb_qps, 0);
	if (!qps) {
		printf("Error, can't allocate memory for QPs\n");
		res = -ENOMEM;
		goto end;
	}

	for (qp_id = 0; qp_id < nb_qps; qp_id++) {
		struct rte_regex_ops **ops;
		struct job_ctx *jobs_ctx;

		qps_used++;
		qp = &qps[qp_id];
		qp->jobs_ctx = NULL;
		qp->buf = NULL;
		qp->ops = ops = rte_malloc(NULL, sizeof(*ops) * nb_jobs, 0);
		if (!ops) {
			printf("Error, can't allocate memory for ops.\n");
			res = -ENOMEM;
			goto end;
		}

		qp->jobs_ctx = jobs_ctx =
			rte_malloc(NULL, sizeof(*jobs_ctx) * nb_jobs, 0);
		if (!jobs_ctx) {
			printf("Error, can't allocate memory for jobs_ctx.\n");
			res = -ENOMEM;
			goto end;
		}

		if (clone_buf(data_buf, &buf, data_len)) {
			printf("Error, can't clone buf.\n");
			res = -EXIT_FAILURE;
			goto end;
		}

		/* Assign each mbuf with the data to handle. */
		actual_jobs = 0;
		pos = 0;
		remainder = data_len % nb_jobs;

		/* Allocate the jobs and assign each job with an mbuf. */
		for (i = 0; (pos < data_len) && (i < nb_jobs) ; i++) {
			act_job_len = RTE_MIN(job_len, data_len - pos);

			if (i == (nb_jobs - 1)) {
				last_job = true;
				act_job_len += remainder;
			}

			ops[i] = rte_malloc(NULL, sizeof(*ops[0]) +
					nb_max_matches *
					sizeof(struct rte_regexdev_match), 0);
			if (!ops[i]) {
				printf("Error, can't allocate "
				       "memory for op.\n");
				res = -ENOMEM;
				goto end;
			}
			if (nb_segs > 1) {
				ops[i]->mbuf = regex_create_segmented_mbuf
							(mbuf_mp, act_job_len,
							 nb_segs, &buf[pos]);
			} else {
				ops[i]->mbuf = rte_pktmbuf_alloc(mbuf_mp);
				if (ops[i]->mbuf) {
					rte_pktmbuf_attach_extbuf(ops[i]->mbuf,
					&buf[pos], 0, act_job_len, &shinfo);

					if (!last_job)
						ops[i]->mbuf->data_len = job_len;
					else
						ops[i]->mbuf->data_len = act_job_len;

					ops[i]->mbuf->pkt_len = act_job_len;
				}
			}
			if (!ops[i]->mbuf) {
				printf("Error, can't add mbuf.\n");
				res = -ENOMEM;
				goto end;
			}

			jobs_ctx[i].mbuf = ops[i]->mbuf;
			ops[i]->user_id = i;
			ops[i]->group_id0 = 1;
			pos += act_job_len;
			actual_jobs++;
		}

		qp->buf = buf;
		qp->total_matches = 0;
		qp->start = 0;
		qp->cycles = 0;
	}

	for (i = 0; i < nb_iterations; i++) {
		for (qp_id = 0; qp_id < nb_qps; qp_id++) {
			qp = &qps[qp_id];
			qp->total_enqueue = 0;
			qp->total_dequeue = 0;
			/* Re-set user id after dequeue to match data in mbuf. */
			for (job_id = 0 ; job_id < nb_jobs; job_id++)
				qp->ops[job_id]->user_id = job_id;
		}
		do {
			update = false;
			for (qp_id = 0; qp_id < nb_qps; qp_id++) {
				qp = &qps[qp_id];
				if (qp->total_dequeue < actual_jobs) {
					qp->start = rte_rdtsc_precise();
					struct rte_regex_ops **
						cur_ops_to_enqueue = qp->ops +
						qp->total_enqueue;

					if (actual_jobs - qp->total_enqueue)
						qp->total_enqueue +=
						rte_regexdev_enqueue_burst
							(dev_id,
							qp_id_base + qp_id,
							cur_ops_to_enqueue,
							actual_jobs -
							qp->total_enqueue);
				}
			}
			for (qp_id = 0; qp_id < nb_qps; qp_id++) {
				qp = &qps[qp_id];
				if (qp->total_dequeue < actual_jobs) {
					struct rte_regex_ops **
						cur_ops_to_dequeue = qp->ops +
						qp->total_dequeue;

					qp->total_dequeue +=
						rte_regexdev_dequeue_burst
							(dev_id,
							qp_id_base + qp_id,
							cur_ops_to_dequeue,
							qp->total_enqueue -
							qp->total_dequeue);
					qp->cycles +=
					     (rte_rdtsc_precise() - qp->start);
					update = true;
				}
			}
		} while (update);
	}
	for (qp_id = 0; qp_id < nb_qps; qp_id++) {
		qp = &qps[qp_id];
		time = (long double)qp->cycles / rte_get_timer_hz();
		printf("Core=%u QP=%u Job=%ld Bytes Last Job=%ld Bytes Time=%Lf sec Perf=%Lf "
		       "Gbps\n", rte_lcore_id(), qp_id + qp_id_base,
		       job_len, act_job_len, time,
		       (((double)data_len * nb_iterations * 8)
		       / time) / 1000000000.0);
	}

	if (rgxc->perf_mode)
		goto end;
	for (qp_id = 0; qp_id < nb_qps; qp_id++) {
		printf("\n############ Core=%u QP=%u ############\n",
		       rte_lcore_id(), qp_id + qp_id_base);
		qp = &qps[qp_id];
		/* Log results per job. */
		for (d_ind = 0; d_ind < qp->total_dequeue; d_ind++) {
			nb_matches = qp->ops[d_ind % actual_jobs]->nb_matches;
			printf("Job id %"PRIu64" number of matches = %d\n",
					qp->ops[d_ind]->user_id, nb_matches);
			qp->total_matches += nb_matches;
			match = qp->ops[d_ind % actual_jobs]->matches;
			for (i = 0; i < nb_matches; i++) {
				printf("match %d, rule = %d, "
				       "start = %d,len = %d\n",
				       i, match->rule_id, match->start_offset,
				       match->len);
				match++;
			}
		}
		printf("Total matches = %d\n", qp->total_matches);
		printf("All Matches:\n");
		/* Log absolute results. */
		for (d_ind = 0; d_ind < qp->total_dequeue; d_ind++) {
			nb_matches = qp->ops[d_ind % actual_jobs]->nb_matches;
			qp->total_matches += nb_matches;
			match = qp->ops[d_ind % actual_jobs]->matches;
			for (i = 0; i < nb_matches; i++) {
				printf("start = %d, len = %d, rule = %d\n",
					match->start_offset +
					(int)(qp->ops[d_ind % actual_jobs]->user_id * job_len),
					match->len, match->rule_id);
				match++;
			}
		}
	}
end:
	for (qp_id = 0; qp_id < qps_used; qp_id++) {
		qp = &qps[qp_id];
		for (i = 0; i < actual_jobs && qp->ops; i++)
			rte_free(qp->ops[i]);
		rte_free(qp->ops);
		qp->ops = NULL;
		for (i = 0; i < actual_jobs && qp->jobs_ctx; i++)
			rte_pktmbuf_free(qp->jobs_ctx[i].mbuf);
		rte_free(qp->jobs_ctx);
		qp->jobs_ctx = NULL;
		rte_free(qp->buf);
		qp->buf = NULL;
	}
	rte_mempool_free(mbuf_mp);
	rte_free(qps);
	return res;
}

static int
distribute_qps_to_lcores(uint32_t nb_cores, uint32_t nb_qps,
			 struct qps_per_lcore **qpl)
{
	int socket;
	unsigned lcore_id;
	uint32_t i;
	uint16_t min_qp_id;
	uint16_t max_qp_id;
	struct qps_per_lcore *qps_per_lcore;
	uint32_t detected_lcores;

	if (nb_qps < nb_cores) {
		nb_cores = nb_qps;
		printf("Reducing number of cores to number of QPs (%u)\n",
		       nb_cores);
	}
	/* Allocate qps_per_lcore array */
	qps_per_lcore =
		rte_malloc(NULL, sizeof(*qps_per_lcore) * nb_cores, 0);
	if (!qps_per_lcore)
		rte_exit(EXIT_FAILURE, "Failed to create qps_per_lcore array\n");
	*qpl = qps_per_lcore;
	detected_lcores = 0;
	min_qp_id = 0;

	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		if (detected_lcores >= nb_cores)
			break;
		qps_per_lcore[detected_lcores].lcore_id = lcore_id;
		socket = rte_lcore_to_socket_id(lcore_id);
		if (socket == SOCKET_ID_ANY)
			socket = 0;
		qps_per_lcore[detected_lcores].socket = socket;
		qps_per_lcore[detected_lcores].qp_id_base = min_qp_id;
		max_qp_id = min_qp_id + nb_qps / nb_cores - 1;
		if (nb_qps % nb_cores > detected_lcores)
			max_qp_id++;
		qps_per_lcore[detected_lcores].nb_qps = max_qp_id -
							min_qp_id + 1;
		min_qp_id = max_qp_id + 1;
		detected_lcores++;
	}
	if (detected_lcores != nb_cores)
		return -1;

	for (i = 0; i < detected_lcores; i++) {
		printf("===> Core %d: allocated queues: ",
		       qps_per_lcore[i].lcore_id);
		min_qp_id = qps_per_lcore[i].qp_id_base;
		max_qp_id =
			qps_per_lcore[i].qp_id_base + qps_per_lcore[i].nb_qps;
		while (min_qp_id < max_qp_id) {
			printf("%u ", min_qp_id);
			min_qp_id++;
		}
		printf("\n");
	}
	return 0;
}

int
main(int argc, char **argv)
{
	char rules_file[MAX_FILE_NAME];
	char data_file[MAX_FILE_NAME];
	uint32_t nb_jobs = 0;
	bool perf_mode = 0;
	uint32_t nb_iterations = 0;
	int ret;
	uint16_t nb_max_payload = 0;
	uint8_t nb_max_matches = 0;
	uint32_t nb_qps = 1;
	char *data_buf;
	long data_len;
	long job_len;
	uint32_t nb_lcores = 1, nb_segs = 1;
	struct regex_conf *rgxc;
	uint32_t i;
	struct qps_per_lcore *qps_per_lcore;

	/* Init EAL. */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "EAL init failed\n");
	argc -= ret;
	argv += ret;
	if (argc > 1)
		args_parse(argc, argv, rules_file, data_file, &nb_jobs,
				&perf_mode, &nb_iterations, &nb_qps,
				&nb_lcores, &nb_segs);

	if (nb_qps == 0)
		rte_exit(EXIT_FAILURE, "Number of QPs must be greater than 0\n");
	if (nb_lcores == 0)
		rte_exit(EXIT_FAILURE, "Number of lcores must be greater than 0\n");
	if (distribute_qps_to_lcores(nb_lcores, nb_qps, &qps_per_lcore) < 0)
		rte_exit(EXIT_FAILURE, "Failed to distribute queues to lcores!\n");
	ret = init_port(&nb_max_payload, rules_file,
			&nb_max_matches, nb_qps);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "init port failed\n");

	data_len = read_file(data_file, &data_buf);
	if (data_len <= 0)
		rte_exit(EXIT_FAILURE, "Error, can't read file, or file is empty.\n");

	job_len = data_len / nb_jobs;
	if (job_len == 0)
		rte_exit(EXIT_FAILURE, "Error, To many jobs, for the given input.\n");

	if (job_len > nb_max_payload)
		rte_exit(EXIT_FAILURE, "Error, not enough jobs to cover input.\n");

	rgxc = rte_malloc(NULL, sizeof(*rgxc) * nb_lcores, 0);
	if (!rgxc)
		rte_exit(EXIT_FAILURE, "Failed to create Regex Conf\n");
	for (i = 0; i < nb_lcores; i++) {
		rgxc[i] = (struct regex_conf){
			.nb_jobs = nb_jobs,
			.nb_segs = nb_segs,
			.perf_mode = perf_mode,
			.nb_iterations = nb_iterations,
			.nb_max_matches = nb_max_matches,
			.nb_qps = qps_per_lcore[i].nb_qps,
			.qp_id_base = qps_per_lcore[i].qp_id_base,
			.data_buf = data_buf,
			.data_len = data_len,
			.job_len = job_len,
		};
		rte_eal_remote_launch(run_regex, &rgxc[i],
				      qps_per_lcore[i].lcore_id);
	}
	rte_eal_mp_wait_lcore();
	rte_free(data_buf);
	rte_free(rgxc);
	rte_free(qps_per_lcore);
	return EXIT_SUCCESS;
}
