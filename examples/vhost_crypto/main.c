/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>
#include <getopt.h>

#include <rte_malloc.h>
#include <rte_cycles.h>
#include <rte_vhost.h>
#include <rte_cryptodev.h>
#include <rte_vhost_crypto.h>

#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_string.h>
#include <cmdline.h>

#define NB_VIRTIO_QUEUES		(1)
#define MAX_PKT_BURST			(64)
#define MAX_IV_LEN			(32)
#define NB_MEMPOOL_OBJS			(8192)
#define NB_CRYPTO_DESCRIPTORS		(4096)
#define NB_CACHE_OBJS			(128)
#define SESSION_MAP_ENTRIES		(1024)
#define REFRESH_TIME_SEC		(3)

#define MAX_NB_SOCKETS			(32)
#define DEF_SOCKET_FILE			"/tmp/vhost_crypto1.socket"

struct vhost_crypto_options {
	char *socket_files[MAX_NB_SOCKETS];
	uint32_t nb_sockets;
	uint8_t cid;
	uint16_t qid;
	uint32_t zero_copy;
	uint32_t guest_polling;
} options;

struct vhost_crypto_info {
	int vids[MAX_NB_SOCKETS];
	struct rte_mempool *sess_pool;
	struct rte_mempool *cop_pool;
	uint32_t lcore_id;
	uint8_t cid;
	uint32_t qid;
	uint32_t nb_vids;
	volatile uint32_t initialized[MAX_NB_SOCKETS];

} info;

#define SOCKET_FILE_KEYWORD	"socket-file"
#define CRYPTODEV_ID_KEYWORD	"cdev-id"
#define CRYPTODEV_QUEUE_KEYWORD	"cdev-queue-id"
#define ZERO_COPY_KEYWORD	"zero-copy"
#define POLLING_KEYWORD		"guest-polling"

uint64_t vhost_cycles[2], last_v_cycles[2];
uint64_t outpkt_amount;

/** support *SOCKET_FILE_PATH:CRYPTODEV_ID* format */
static int
parse_socket_arg(char *arg)
{
	uint32_t nb_sockets = options.nb_sockets;
	size_t len = strlen(arg);

	if (nb_sockets >= MAX_NB_SOCKETS) {
		RTE_LOG(ERR, USER1, "Too many socket files!\n");
		return -ENOMEM;
	}

	options.socket_files[nb_sockets] = rte_malloc(NULL, len, 0);
	if (!options.socket_files[nb_sockets]) {
		RTE_LOG(ERR, USER1, "Insufficient memory\n");
		return -ENOMEM;
	}

	rte_memcpy(options.socket_files[nb_sockets], arg, len);

	options.nb_sockets++;

	return 0;
}

static int
parse_cryptodev_id(const char *q_arg)
{
	char *end = NULL;
	uint64_t pm;

	/* parse decimal string */
	pm = strtoul(q_arg, &end, 10);
	if (pm > rte_cryptodev_count()) {
		RTE_LOG(ERR, USER1, "Invalid Cryptodev ID %s\n", q_arg);
		return -1;
	}

	options.cid = (uint8_t)pm;

	return 0;
}

static int
parse_cdev_queue_id(const char *q_arg)
{
	char *end = NULL;
	uint64_t pm;

	/* parse decimal string */
	pm = strtoul(q_arg, &end, 10);
	if (pm == UINT64_MAX) {
		RTE_LOG(ERR, USER1, "Invalid Cryptodev Queue ID %s\n", q_arg);
		return -1;
	}

	options.qid = (uint16_t)pm;

	return 0;
}

static void
vhost_crypto_usage(const char *prgname)
{
	printf("%s [EAL options] --\n"
		"  --%s SOCKET-FILE-PATH\n"
		"  --%s CRYPTODEV_ID: crypto device id\n"
		"  --%s CDEV_QUEUE_ID: crypto device queue id\n"
		"  --%s: zero copy\n"
		"  --%s: guest polling\n",
		prgname, SOCKET_FILE_KEYWORD, CRYPTODEV_ID_KEYWORD,
		CRYPTODEV_QUEUE_KEYWORD, ZERO_COPY_KEYWORD, POLLING_KEYWORD);
}

static int
vhost_crypto_parse_args(int argc, char **argv)
{
	int opt, ret;
	char *prgname = argv[0];
	char **argvopt;
	int option_index;
	struct option lgopts[] = {
			{SOCKET_FILE_KEYWORD, required_argument, 0, 0},
			{CRYPTODEV_ID_KEYWORD, required_argument, 0, 0},
			{CRYPTODEV_QUEUE_KEYWORD, required_argument, 0, 0},
			{ZERO_COPY_KEYWORD, no_argument, 0, 0},
			{POLLING_KEYWORD, no_argument, 0, 0},
			{NULL, 0, 0, 0}
	};

	options.cid = 0;
	options.qid = 0;
	options.nb_sockets = 0;
	options.guest_polling = 0;
	options.zero_copy = RTE_VHOST_CRYPTO_ZERO_COPY_DISABLE;

	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, "s:",
				  lgopts, &option_index)) != EOF) {

		switch (opt) {
		case 0:
			if (strcmp(lgopts[option_index].name,
					SOCKET_FILE_KEYWORD) == 0) {
				ret = parse_socket_arg(optarg);
				if (ret < 0) {
					vhost_crypto_usage(prgname);
					return ret;
				}
			} else if (strcmp(lgopts[option_index].name,
					CRYPTODEV_ID_KEYWORD) == 0) {
				ret = parse_cryptodev_id(optarg);
				if (ret < 0) {
					vhost_crypto_usage(prgname);
					return ret;
				}
			} else if (strcmp(lgopts[option_index].name,
					CRYPTODEV_QUEUE_KEYWORD) == 0) {
				ret = parse_cdev_queue_id(optarg);
				if (ret < 0) {
					vhost_crypto_usage(prgname);
					return ret;
				}
			} else if (strcmp(lgopts[option_index].name,
					ZERO_COPY_KEYWORD) == 0) {
				options.zero_copy =
					RTE_VHOST_CRYPTO_ZERO_COPY_ENABLE;
			} else if (strcmp(lgopts[option_index].name,
					POLLING_KEYWORD) == 0) {
				options.guest_polling = 1;
			} else {
				vhost_crypto_usage(prgname);
				return -EINVAL;
			}
			break;
		default:
			return -1;
		}
	}

	if (options.nb_sockets == 0) {
		options.socket_files[0] = strdup(DEF_SOCKET_FILE);
		options.nb_sockets = 1;
		RTE_LOG(INFO, USER1,
				"VHOST-CRYPTO: use default socket file %s\n",
				DEF_SOCKET_FILE);
	}

	return 0;
}

static int
new_device(int vid)
{
	char path[PATH_MAX];
	uint32_t idx, i;
	int ret;

	ret = rte_vhost_get_ifname(vid, path, PATH_MAX);
	if (ret) {
		RTE_LOG(ERR, USER1, "Cannot find matched socket\n");
		return ret;
	}

	for (idx = 0; idx < options.nb_sockets; idx++) {
		if (strcmp(path, options.socket_files[idx]) == 0)
			break;
	}

	if (idx == options.nb_sockets) {
		RTE_LOG(ERR, USER1, "Cannot find recorded socket\n");
		return -ENOENT;
	}

	for (i = 0; i < 2; i++) {
		vhost_cycles[i] = 0;
		last_v_cycles[i] = 0;
	}

	ret = rte_vhost_crypto_create(vid, info.cid, info.sess_pool,
			rte_lcore_to_socket_id(info.lcore_id));
	if (ret) {
		RTE_LOG(ERR, USER1, "Cannot create vhost crypto\n");
		return ret;
	}

	ret = rte_vhost_crypto_set_zero_copy(vid, options.zero_copy);
	if (ret) {
		RTE_LOG(ERR, USER1, "Cannot %s zero copy feature\n",
				options.zero_copy == 1 ? "enable" : "disable");
		return ret;
	}

	info.vids[idx] = vid;
	info.initialized[idx] = 1;

	rte_wmb();

	RTE_LOG(INFO, USER1, "New Vhost-crypto Device %s, Device ID %d\n", path,
			vid);
	return 0;
}

static void
destroy_device(int vid)
{
	uint32_t i;

	for (i = 0; i < info.nb_vids; i++) {
		if (vid == info.vids[i])
			break;
	}

	if (i == info.nb_vids) {
		RTE_LOG(ERR, USER1, "Cannot find socket file from list\n");
		return;
	}

	info.initialized[i] = 0;

	rte_wmb();

	rte_vhost_crypto_free(vid);

	RTE_LOG(INFO, USER1, "Vhost Crypto Device %i Removed\n", vid);
}

static const struct vhost_device_ops virtio_crypto_device_ops = {
	.new_device =  new_device,
	.destroy_device = destroy_device,
};

__attribute__((unused))
static void clrscr(void)
{
	system("@cls||clear");
}

static int
vhost_crypto_worker(__rte_unused void *arg)
{
	struct rte_crypto_op *ops[NB_VIRTIO_QUEUES][MAX_PKT_BURST + 1];
	struct rte_crypto_op *ops_deq[NB_VIRTIO_QUEUES][MAX_PKT_BURST + 1];
	uint32_t nb_inflight_ops = 0;
	uint16_t nb_callfds;
	int callfds[VIRTIO_CRYPTO_MAX_NUM_BURST_VQS];
	uint32_t lcore_id = rte_lcore_id();
	uint32_t burst_size = MAX_PKT_BURST;
	uint32_t i, j, k;
	uint32_t to_fetch, fetched;
	uint64_t t_start, t_end, interval;

	int ret = 0;

	RTE_LOG(INFO, USER1, "Processing on Core %u started\n", lcore_id);

	for (i = 0; i < NB_VIRTIO_QUEUES; i++) {
		if (rte_crypto_op_bulk_alloc(info.cop_pool,
				RTE_CRYPTO_OP_TYPE_SYMMETRIC, ops[i],
				burst_size) < burst_size) {
			RTE_LOG(ERR, USER1, "Failed to alloc cops\n");
			ret = -1;
			goto exit;
		}
	}

	while (1) {
		for (i = 0; i < info.nb_vids; i++) {
			if (unlikely(info.initialized[i] == 0))
				continue;

			for (j = 0; j < NB_VIRTIO_QUEUES; j++) {
				t_start = rte_rdtsc_precise();

				to_fetch = RTE_MIN(burst_size,
						(NB_CRYPTO_DESCRIPTORS -
						nb_inflight_ops));
				fetched = rte_vhost_crypto_fetch_requests(
						info.vids[i], j, ops[j],
						to_fetch);
				nb_inflight_ops += rte_cryptodev_enqueue_burst(
						info.cid, info.qid, ops[j],
						fetched);
				if (unlikely(rte_crypto_op_bulk_alloc(
						info.cop_pool,
						RTE_CRYPTO_OP_TYPE_SYMMETRIC,
						ops[j], fetched) < fetched)) {
					RTE_LOG(ERR, USER1, "Failed realloc\n");
					return -1;
				}
				t_end = rte_rdtsc_precise();
				interval = t_end - t_start;

				vhost_cycles[fetched > 0] += interval;

				t_start = t_end;
				fetched = rte_cryptodev_dequeue_burst(
						info.cid, info.qid,
						ops_deq[j], RTE_MIN(burst_size,
						nb_inflight_ops));
				fetched = rte_vhost_crypto_finalize_requests(
						ops_deq[j], fetched, callfds,
						&nb_callfds);

				nb_inflight_ops -= fetched;
				outpkt_amount += fetched;

				if (!options.guest_polling) {
					for (k = 0; k < nb_callfds; k++)
						eventfd_write(callfds[k],
								(eventfd_t)1);
				}

				rte_mempool_put_bulk(info.cop_pool,
						(void **)ops_deq[j], fetched);
				interval = rte_rdtsc_precise() - t_start;

				vhost_cycles[fetched > 0] += interval;
			}
		}
	}
exit:
	return ret;
}


static void
unregister_drivers(int socket_num)
{
	int ret;

	ret = rte_vhost_driver_unregister(options.socket_files[socket_num]);
	if (ret != 0)
		RTE_LOG(ERR, USER1,
			"Fail to unregister vhost driver for %s.\n",
			options.socket_files[socket_num]);
}

int
main(int argc, char *argv[])
{
	struct rte_cryptodev_qp_conf qp_conf = {NB_CRYPTO_DESCRIPTORS};
	struct rte_cryptodev_config config;
	struct rte_cryptodev_info dev_info;
	uint32_t cryptodev_id;
	uint32_t worker_lcore;
	char name[128];
	uint32_t i = 0;
	int ret;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		return -1;
	argc -= ret;
	argv += ret;

	ret = vhost_crypto_parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Failed to parse arguments!\n");

	info.cid = options.cid;
	info.qid = options.qid;

	worker_lcore = rte_get_next_lcore(0, 1, 0);
	if (worker_lcore == RTE_MAX_LCORE)
		rte_exit(EXIT_FAILURE, "Not enough lcore\n");

	cryptodev_id = info.cid;
	rte_cryptodev_info_get(cryptodev_id, &dev_info);
	if (dev_info.max_nb_queue_pairs < info.qid + 1) {
		RTE_LOG(ERR, USER1, "Number of queues cannot over %u",
				dev_info.max_nb_queue_pairs);
		goto error_exit;
	}

	config.nb_queue_pairs = dev_info.max_nb_queue_pairs;
	config.socket_id = rte_lcore_to_socket_id(worker_lcore);

	ret = rte_cryptodev_configure(cryptodev_id, &config);
	if (ret < 0) {
		RTE_LOG(ERR, USER1, "Failed to configure cryptodev %u",
				cryptodev_id);
		goto error_exit;
	}

	snprintf(name, 127, "SESS_POOL_%u", worker_lcore);
	info.sess_pool = rte_mempool_create(name, SESSION_MAP_ENTRIES,
			rte_cryptodev_sym_get_private_session_size(
			cryptodev_id), 64, 0, NULL, NULL, NULL, NULL,
			rte_lcore_to_socket_id(worker_lcore), 0);
	if (!info.sess_pool) {
		RTE_LOG(ERR, USER1, "Failed to create mempool");
		goto error_exit;
	}

	snprintf(name, 127, "COPPOOL_%u", worker_lcore);
	info.cop_pool = rte_crypto_op_pool_create(name,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC, NB_MEMPOOL_OBJS,
			NB_CACHE_OBJS, 0, rte_lcore_to_socket_id(worker_lcore));

	if (!info.cop_pool) {
		RTE_LOG(ERR, USER1, "Lcore %u failed to create crypto pool",
				worker_lcore);
		ret = -1;
		goto error_exit;
	}

	info.nb_vids = options.nb_sockets;
	for (i = 0; i < MAX_NB_SOCKETS; i++)
		info.vids[i] = -1;

	for (i = 0; i < dev_info.max_nb_queue_pairs; i++) {
		ret = rte_cryptodev_queue_pair_setup(cryptodev_id, i,
				&qp_conf, rte_lcore_to_socket_id(worker_lcore),
				info.sess_pool);
		if (ret < 0) {
			RTE_LOG(ERR, USER1, "Failed to configure qp %u\n",
					info.cid);
			goto error_exit;
		}
	}

	ret = rte_cryptodev_start(cryptodev_id);
	if (ret < 0) {
		RTE_LOG(ERR, USER1, "Failed to start cryptodev %u\n", info.cid);
		goto error_exit;
	}

	info.cid = cryptodev_id;
	info.lcore_id = worker_lcore;

	if (rte_eal_remote_launch(vhost_crypto_worker, NULL, worker_lcore)
			< 0) {
		RTE_LOG(ERR, USER1, "Failed to start worker lcore");
		goto error_exit;
	}

	for (i = 0; i < options.nb_sockets; i++) {
		if (rte_vhost_driver_register(options.socket_files[i],
				RTE_VHOST_USER_DEQUEUE_ZERO_COPY) < 0) {
			RTE_LOG(ERR, USER1, "socket %s already exists\n",
					options.socket_files[i]);
			goto error_exit;
		}

		rte_vhost_driver_callback_register(options.socket_files[i],
				&virtio_crypto_device_ops);

		if (rte_vhost_driver_start(options.socket_files[i]) < 0) {
			RTE_LOG(ERR, USER1, "failed to start vhost driver.\n");
			goto error_exit;
		}
	}

	RTE_LCORE_FOREACH(worker_lcore)
		rte_eal_wait_lcore(worker_lcore);

	rte_mempool_free(info.sess_pool);
	rte_mempool_free(info.cop_pool);

	return 0;

error_exit:
	for (i = 0; i < options.nb_sockets; i++)
		unregister_drivers(i);

	rte_mempool_free(info.cop_pool);
	rte_mempool_free(info.sess_pool);

	return -1;
}
