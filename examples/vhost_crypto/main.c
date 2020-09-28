/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>
#include <getopt.h>

#include <rte_malloc.h>
#include <rte_cycles.h>
#include <rte_vhost.h>
#include <rte_cryptodev.h>
#include <rte_vhost_crypto.h>
#include <rte_string_fns.h>

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

#define MAX_NB_SOCKETS			(4)
#define MAX_NB_WORKER_CORES		(16)

struct lcore_option {
	uint32_t lcore_id;
	char *socket_files[MAX_NB_SOCKETS];
	uint32_t nb_sockets;
	uint8_t cid;
	uint16_t qid;
};

struct vhost_crypto_info {
	int vids[MAX_NB_SOCKETS];
	uint32_t nb_vids;
	struct rte_mempool *sess_pool;
	struct rte_mempool *sess_priv_pool;
	struct rte_mempool *cop_pool;
	uint8_t cid;
	uint32_t qid;
	uint32_t nb_inflight_ops;
	volatile uint32_t initialized[MAX_NB_SOCKETS];
} __rte_cache_aligned;

struct vhost_crypto_options {
	struct lcore_option los[MAX_NB_WORKER_CORES];
	struct vhost_crypto_info *infos[MAX_NB_WORKER_CORES];
	uint32_t nb_los;
	uint32_t zero_copy;
	uint32_t guest_polling;
} options;

#define CONFIG_KEYWORD		"config"
#define SOCKET_FILE_KEYWORD	"socket-file"
#define ZERO_COPY_KEYWORD	"zero-copy"
#define POLLING_KEYWORD		"guest-polling"

#define NB_SOCKET_FIELDS	(2)

static uint32_t
find_lo(uint32_t lcore_id)
{
	uint32_t i;

	for (i = 0; i < options.nb_los; i++)
		if (options.los[i].lcore_id == lcore_id)
			return i;

	return UINT32_MAX;
}

/** support *SOCKET_FILE_PATH:CRYPTODEV_ID* format */
static int
parse_socket_arg(char *arg)
{
	uint32_t nb_sockets;
	uint32_t lcore_id;
	char *str_fld[NB_SOCKET_FIELDS];
	struct lcore_option *lo;
	uint32_t idx;
	char *end;

	if (rte_strsplit(arg, strlen(arg), str_fld, NB_SOCKET_FIELDS, ',') !=
				NB_SOCKET_FIELDS) {
		RTE_LOG(ERR, USER1, "Invalid socket parameter '%s'\n", arg);
		return -EINVAL;
	}

	errno = 0;
	lcore_id = strtoul(str_fld[0], &end, 0);
	if (errno != 0 || end == str_fld[0] || lcore_id > 255)
		return -EINVAL;

	idx = find_lo(lcore_id);
	if (idx == UINT32_MAX) {
		if (options.nb_los == MAX_NB_WORKER_CORES)
			return -ENOMEM;
		lo = &options.los[options.nb_los];
		lo->lcore_id = lcore_id;
		options.nb_los++;
	} else
		lo = &options.los[idx];

	nb_sockets = lo->nb_sockets;

	if (nb_sockets >= MAX_NB_SOCKETS) {
		RTE_LOG(ERR, USER1, "Too many socket files!\n");
		return -ENOMEM;
	}

	lo->socket_files[nb_sockets] = strdup(str_fld[1]);
	if (!lo->socket_files[nb_sockets]) {
		RTE_LOG(ERR, USER1, "Insufficient memory\n");
		return -ENOMEM;
	}

	lo->nb_sockets++;

	return 0;
}

static int
parse_config(char *q_arg)
{
	struct lcore_option *lo;
	char s[256];
	const char *p, *p0 = q_arg;
	char *end;
	enum fieldnames {
		FLD_LCORE = 0,
		FLD_CID,
		FLD_QID,
		_NUM_FLD
	};
	uint32_t flds[_NUM_FLD];
	char *str_fld[_NUM_FLD];
	uint32_t i;
	uint32_t size;

	while ((p = strchr(p0, '(')) != NULL) {
		++p;
		p0 = strchr(p, ')');
		if (p0 == NULL)
			return -1;

		size = p0 - p;
		if (size >= sizeof(s))
			return -1;

		snprintf(s, sizeof(s), "%.*s", size, p);
		if (rte_strsplit(s, sizeof(s), str_fld, _NUM_FLD, ',') !=
				_NUM_FLD)
			return -1;
		for (i = 0; i < _NUM_FLD; i++) {
			errno = 0;
			flds[i] = strtoul(str_fld[i], &end, 0);
			if (errno != 0 || end == str_fld[i] || flds[i] > 255)
				return -EINVAL;
		}

		if (flds[FLD_LCORE] > RTE_MAX_LCORE)
			return -EINVAL;

		i = find_lo(flds[FLD_LCORE]);
		if (i == UINT32_MAX) {
			if (options.nb_los == MAX_NB_WORKER_CORES)
				return -ENOMEM;
			lo = &options.los[options.nb_los];
			options.nb_los++;
		} else
			lo = &options.los[i];

		lo->lcore_id = flds[FLD_LCORE];
		lo->cid = flds[FLD_CID];
		lo->qid = flds[FLD_QID];
	}

	return 0;
}

static void
vhost_crypto_usage(const char *prgname)
{
	printf("%s [EAL options] --\n"
		"  --%s <lcore>,SOCKET-FILE-PATH\n"
		"  --%s (lcore,cdev_id,queue_id)[,(lcore,cdev_id,queue_id)]"
		"  --%s: zero copy\n"
		"  --%s: guest polling\n",
		prgname, SOCKET_FILE_KEYWORD, CONFIG_KEYWORD,
		ZERO_COPY_KEYWORD, POLLING_KEYWORD);
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
			{CONFIG_KEYWORD, required_argument, 0, 0},
			{ZERO_COPY_KEYWORD, no_argument, 0, 0},
			{POLLING_KEYWORD, no_argument, 0, 0},
			{NULL, 0, 0, 0}
	};

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
					CONFIG_KEYWORD) == 0) {
				ret = parse_config(optarg);
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

	return 0;
}

static int
new_device(int vid)
{
	struct vhost_crypto_info *info = NULL;
	char path[PATH_MAX];
	uint32_t i, j;
	int ret;

	ret = rte_vhost_get_ifname(vid, path, PATH_MAX);
	if (ret) {
		RTE_LOG(ERR, USER1, "Cannot find matched socket\n");
		return ret;
	}

	for (i = 0; i < options.nb_los; i++) {
		for (j = 0; j < options.los[i].nb_sockets; j++) {
			if (strcmp(path, options.los[i].socket_files[j]) == 0) {
				info = options.infos[i];
				break;
			}
		}

		if (info)
			break;
	}

	if (!info) {
		RTE_LOG(ERR, USER1, "Cannot find recorded socket\n");
		return -ENOENT;
	}

	ret = rte_vhost_crypto_create(vid, info->cid, info->sess_pool,
			info->sess_priv_pool,
			rte_lcore_to_socket_id(options.los[i].lcore_id));
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

	info->vids[j] = vid;
	info->initialized[j] = 1;

	rte_wmb();

	RTE_LOG(INFO, USER1, "New Vhost-crypto Device %s, Device ID %d\n", path,
			vid);
	return 0;
}

static void
destroy_device(int vid)
{
	struct vhost_crypto_info *info = NULL;
	uint32_t i, j;

	for (i = 0; i < options.nb_los; i++) {
		for (j = 0; j < options.los[i].nb_sockets; j++) {
			if (options.infos[i]->vids[j] == vid) {
				info = options.infos[i];
				break;
			}
		}
		if (info)
			break;
	}

	if (!info) {
		RTE_LOG(ERR, USER1, "Cannot find socket file from list\n");
		return;
	}

	do {

	} while (info->nb_inflight_ops);

	info->initialized[j] = 0;

	rte_wmb();

	rte_vhost_crypto_free(vid);

	RTE_LOG(INFO, USER1, "Vhost Crypto Device %i Removed\n", vid);
}

static const struct vhost_device_ops virtio_crypto_device_ops = {
	.new_device =  new_device,
	.destroy_device = destroy_device,
};

static int
vhost_crypto_worker(void *arg)
{
	struct rte_crypto_op *ops[NB_VIRTIO_QUEUES][MAX_PKT_BURST + 1];
	struct rte_crypto_op *ops_deq[NB_VIRTIO_QUEUES][MAX_PKT_BURST + 1];
	struct vhost_crypto_info *info = arg;
	uint16_t nb_callfds;
	int callfds[VIRTIO_CRYPTO_MAX_NUM_BURST_VQS];
	uint32_t lcore_id = rte_lcore_id();
	uint32_t burst_size = MAX_PKT_BURST;
	uint32_t i, j, k;
	uint32_t to_fetch, fetched;

	int ret = 0;

	RTE_LOG(INFO, USER1, "Processing on Core %u started\n", lcore_id);

	for (i = 0; i < NB_VIRTIO_QUEUES; i++) {
		if (rte_crypto_op_bulk_alloc(info->cop_pool,
				RTE_CRYPTO_OP_TYPE_SYMMETRIC, ops[i],
				burst_size) < burst_size) {
			RTE_LOG(ERR, USER1, "Failed to alloc cops\n");
			ret = -1;
			goto exit;
		}
	}

	while (1) {
		for (i = 0; i < info->nb_vids; i++) {
			if (unlikely(info->initialized[i] == 0))
				continue;

			for (j = 0; j < NB_VIRTIO_QUEUES; j++) {
				to_fetch = RTE_MIN(burst_size,
						(NB_CRYPTO_DESCRIPTORS -
						info->nb_inflight_ops));
				fetched = rte_vhost_crypto_fetch_requests(
						info->vids[i], j, ops[j],
						to_fetch);
				info->nb_inflight_ops +=
						rte_cryptodev_enqueue_burst(
						info->cid, info->qid, ops[j],
						fetched);
				if (unlikely(rte_crypto_op_bulk_alloc(
						info->cop_pool,
						RTE_CRYPTO_OP_TYPE_SYMMETRIC,
						ops[j], fetched) < fetched)) {
					RTE_LOG(ERR, USER1, "Failed realloc\n");
					return -1;
				}

				fetched = rte_cryptodev_dequeue_burst(
						info->cid, info->qid,
						ops_deq[j], RTE_MIN(burst_size,
						info->nb_inflight_ops));
				fetched = rte_vhost_crypto_finalize_requests(
						ops_deq[j], fetched, callfds,
						&nb_callfds);

				info->nb_inflight_ops -= fetched;

				if (!options.guest_polling) {
					for (k = 0; k < nb_callfds; k++)
						eventfd_write(callfds[k],
								(eventfd_t)1);
				}

				rte_mempool_put_bulk(info->cop_pool,
						(void **)ops_deq[j], fetched);
			}
		}
	}
exit:
	return ret;
}

static void
free_resource(void)
{
	uint32_t i, j;

	for (i = 0; i < options.nb_los; i++) {
		struct lcore_option *lo = &options.los[i];
		struct vhost_crypto_info *info = options.infos[i];

		if (!info)
			continue;

		rte_mempool_free(info->cop_pool);
		rte_mempool_free(info->sess_pool);
		rte_mempool_free(info->sess_priv_pool);

		for (j = 0; j < lo->nb_sockets; j++) {
			rte_vhost_driver_unregister(lo->socket_files[i]);
			free(lo->socket_files[i]);
		}

		rte_free(info);
	}

	memset(&options, 0, sizeof(options));
}

int
main(int argc, char *argv[])
{
	struct rte_cryptodev_qp_conf qp_conf;
	struct rte_cryptodev_config config;
	struct rte_cryptodev_info dev_info;
	char name[128];
	uint32_t i, j, lcore;
	int ret;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		return -1;
	argc -= ret;
	argv += ret;

	ret = vhost_crypto_parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Failed to parse arguments!\n");

	for (i = 0; i < options.nb_los; i++) {
		struct lcore_option *lo = &options.los[i];
		struct vhost_crypto_info *info;

		info = rte_zmalloc_socket(NULL, sizeof(*info),
				RTE_CACHE_LINE_SIZE, rte_lcore_to_socket_id(
						lo->lcore_id));
		if (!info) {
			ret = -ENOMEM;
			goto error_exit;
		}

		info->cid = lo->cid;
		info->qid = lo->qid;
		info->nb_vids = lo->nb_sockets;

		rte_cryptodev_info_get(info->cid, &dev_info);
		if (options.zero_copy == RTE_VHOST_CRYPTO_ZERO_COPY_ENABLE) {
#define VHOST_CRYPTO_CDEV_NAME_AESNI_MB_PMD	crypto_aesni_mb
#define VHOST_CRYPTO_CDEV_NAME_AESNI_GCM_PMD	crypto_aesni_gcm
			if (strstr(dev_info.driver_name,
				RTE_STR(VHOST_CRYPTO_CDEV_NAME_AESNI_MB_PMD)) ||
				strstr(dev_info.driver_name,
				RTE_STR(VHOST_CRYPTO_CDEV_NAME_AESNI_GCM_PMD))) {
				RTE_LOG(ERR, USER1, "Cannot enable zero-copy in %s\n",
					dev_info.driver_name);
				ret = -EPERM;
				goto error_exit;
			}
		}

		if (dev_info.max_nb_queue_pairs < info->qid + 1) {
			RTE_LOG(ERR, USER1, "Number of queues cannot over %u",
					dev_info.max_nb_queue_pairs);
			goto error_exit;
		}

		config.nb_queue_pairs = dev_info.max_nb_queue_pairs;
		config.socket_id = rte_lcore_to_socket_id(lo->lcore_id);
		config.ff_disable = RTE_CRYPTODEV_FF_SECURITY;

		ret = rte_cryptodev_configure(info->cid, &config);
		if (ret < 0) {
			RTE_LOG(ERR, USER1, "Failed to configure cryptodev %u",
					info->cid);
			goto error_exit;
		}

		snprintf(name, 127, "SESS_POOL_%u", lo->lcore_id);
		info->sess_pool = rte_cryptodev_sym_session_pool_create(name,
				SESSION_MAP_ENTRIES, 0, 0, 0,
				rte_lcore_to_socket_id(lo->lcore_id));

		snprintf(name, 127, "SESS_POOL_PRIV_%u", lo->lcore_id);
		info->sess_priv_pool = rte_mempool_create(name,
				SESSION_MAP_ENTRIES,
				rte_cryptodev_sym_get_private_session_size(
				info->cid), 64, 0, NULL, NULL, NULL, NULL,
				rte_lcore_to_socket_id(lo->lcore_id), 0);
		if (!info->sess_priv_pool || !info->sess_pool) {
			RTE_LOG(ERR, USER1, "Failed to create mempool");
			goto error_exit;
		}

		snprintf(name, 127, "COPPOOL_%u", lo->lcore_id);
		info->cop_pool = rte_crypto_op_pool_create(name,
				RTE_CRYPTO_OP_TYPE_SYMMETRIC, NB_MEMPOOL_OBJS,
				NB_CACHE_OBJS, VHOST_CRYPTO_MAX_IV_LEN,
				rte_lcore_to_socket_id(lo->lcore_id));

		if (!info->cop_pool) {
			RTE_LOG(ERR, USER1, "Failed to create crypto pool");
			ret = -ENOMEM;
			goto error_exit;
		}

		options.infos[i] = info;

		qp_conf.nb_descriptors = NB_CRYPTO_DESCRIPTORS;
		qp_conf.mp_session = info->sess_pool;
		qp_conf.mp_session_private = info->sess_priv_pool;

		for (j = 0; j < dev_info.max_nb_queue_pairs; j++) {
			ret = rte_cryptodev_queue_pair_setup(info->cid, j,
					&qp_conf, rte_lcore_to_socket_id(
							lo->lcore_id));
			if (ret < 0) {
				RTE_LOG(ERR, USER1, "Failed to configure qp\n");
				goto error_exit;
			}
		}
	}

	for (i = 0; i < options.nb_los; i++) {
		struct lcore_option *lo = &options.los[i];
		struct vhost_crypto_info *info = options.infos[i];

		ret = rte_cryptodev_start(lo->cid);
		if (ret < 0) {
			RTE_LOG(ERR, USER1, "Failed to start cryptodev\n");
			goto error_exit;
		}

		if (rte_eal_remote_launch(vhost_crypto_worker, info,
				lo->lcore_id) < 0) {
			RTE_LOG(ERR, USER1, "Failed to start worker lcore");
			goto error_exit;
		}

		for (j = 0; j < lo->nb_sockets; j++) {
			ret = rte_vhost_driver_register(lo->socket_files[j],
				RTE_VHOST_USER_DEQUEUE_ZERO_COPY);
			if (ret < 0) {
				RTE_LOG(ERR, USER1, "socket %s already exists\n",
					lo->socket_files[j]);
				goto error_exit;
			}

			rte_vhost_driver_callback_register(lo->socket_files[j],
				&virtio_crypto_device_ops);

			ret = rte_vhost_driver_start(lo->socket_files[j]);
			if (ret < 0)  {
				RTE_LOG(ERR, USER1, "failed to start vhost.\n");
				goto error_exit;
			}
		}
	}

	RTE_LCORE_FOREACH(lcore)
		rte_eal_wait_lcore(lcore);

	free_resource();

	return 0;

error_exit:

	free_resource();

	return -1;
}
