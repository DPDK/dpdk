/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <sys/stat.h>
#include <getopt.h>
#include <dirent.h>

#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_string_fns.h>

#include "fips_validation.h"

#define REQ_FILE_PATH_KEYWORD	"req-file"
#define RSP_FILE_PATH_KEYWORD	"rsp-file"
#define FOLDER_KEYWORD		"path-is-folder"
#define CRYPTODEV_KEYWORD	"cryptodev"
#define CRYPTODEV_ID_KEYWORD	"cryptodev-id"

struct fips_test_vector vec;
struct fips_test_interim_info info;

struct cryptodev_fips_validate_env {
	const char *req_path;
	const char *rsp_path;
	uint32_t is_path_folder;
	uint32_t dev_id;
	struct rte_mempool *mpool;
	struct rte_mempool *op_pool;
	struct rte_mbuf *mbuf;
	struct rte_crypto_op *op;
	struct rte_cryptodev_sym_session *sess;
} env;

static int
cryptodev_fips_validate_app_int(void)
{
	struct rte_cryptodev_config conf = {rte_socket_id(), 1};
	struct rte_cryptodev_qp_conf qp_conf = {128};
	int ret;

	ret = rte_cryptodev_configure(env.dev_id, &conf);
	if (ret < 0)
		return ret;

	env.mpool = rte_pktmbuf_pool_create("FIPS_MEMPOOL", 128, 0, 0,
			UINT16_MAX, rte_socket_id());
	if (!env.mpool)
		return ret;

	ret = rte_cryptodev_queue_pair_setup(env.dev_id, 0, &qp_conf,
			rte_socket_id(), env.mpool);
	if (ret < 0)
		return ret;

	ret = -ENOMEM;

	env.op_pool = rte_crypto_op_pool_create(
			"FIPS_OP_POOL",
			RTE_CRYPTO_OP_TYPE_SYMMETRIC,
			1, 0,
			16,
			rte_socket_id());
	if (!env.op_pool)
		goto error_exit;

	env.mbuf = rte_pktmbuf_alloc(env.mpool);
	if (!env.mbuf)
		goto error_exit;

	env.op = rte_crypto_op_alloc(env.op_pool, RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	if (!env.op)
		goto error_exit;

	return 0;

error_exit:
	rte_mempool_free(env.mpool);
	if (env.op_pool)
		rte_mempool_free(env.op_pool);

	return ret;
}

static void
cryptodev_fips_validate_app_uninit(void)
{
	rte_pktmbuf_free(env.mbuf);
	rte_crypto_op_free(env.op);
	rte_cryptodev_sym_session_clear(env.dev_id, env.sess);
	rte_cryptodev_sym_session_free(env.sess);
	rte_mempool_free(env.mpool);
	rte_mempool_free(env.op_pool);
}

static int
fips_test_one_file(void);

static int
parse_cryptodev_arg(char *arg)
{
	int id = rte_cryptodev_get_dev_id(arg);

	if (id < 0) {
		RTE_LOG(ERR, USER1, "Error %i: invalid cryptodev name %s\n",
				id, arg);
		return id;
	}

	env.dev_id = (uint32_t)id;

	return 0;
}

static int
parse_cryptodev_id_arg(char *arg)
{
	uint32_t cryptodev_id;

	if (parser_read_uint32(&cryptodev_id, arg) < 0) {
		RTE_LOG(ERR, USER1, "Error %i: invalid cryptodev id %s\n",
				-EINVAL, arg);
		return -1;
	}


	if (!rte_cryptodev_pmd_is_valid_dev(cryptodev_id)) {
		RTE_LOG(ERR, USER1, "Error %i: invalid cryptodev id %s\n",
				cryptodev_id, arg);
		return -1;
	}

	env.dev_id = (uint32_t)cryptodev_id;

	return 0;
}

static void
cryptodev_fips_validate_usage(const char *prgname)
{
	printf("%s [EAL options] --\n"
		"  --%s: REQUEST-FILE-PATH\n"
		"  --%s: RESPONSE-FILE-PATH\n"
		"  --%s: indicating both paths are folders\n"
		"  --%s: CRYPTODEV-NAME\n"
		"  --%s: CRYPTODEV-ID-NAME\n",
		prgname, REQ_FILE_PATH_KEYWORD, RSP_FILE_PATH_KEYWORD,
		FOLDER_KEYWORD, CRYPTODEV_KEYWORD, CRYPTODEV_ID_KEYWORD);
}

static int
cryptodev_fips_validate_parse_args(int argc, char **argv)
{
	int opt, ret;
	char *prgname = argv[0];
	char **argvopt;
	int option_index;
	struct option lgopts[] = {
			{REQ_FILE_PATH_KEYWORD, required_argument, 0, 0},
			{RSP_FILE_PATH_KEYWORD, required_argument, 0, 0},
			{FOLDER_KEYWORD, no_argument, 0, 0},
			{CRYPTODEV_KEYWORD, required_argument, 0, 0},
			{CRYPTODEV_ID_KEYWORD, required_argument, 0, 0},
			{NULL, 0, 0, 0}
	};

	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, "s:",
				  lgopts, &option_index)) != EOF) {

		switch (opt) {
		case 0:
			if (strcmp(lgopts[option_index].name,
					REQ_FILE_PATH_KEYWORD) == 0)
				env.req_path = optarg;
			else if (strcmp(lgopts[option_index].name,
					RSP_FILE_PATH_KEYWORD) == 0)
				env.rsp_path = optarg;
			else if (strcmp(lgopts[option_index].name,
					FOLDER_KEYWORD) == 0)
				env.is_path_folder = 1;
			else if (strcmp(lgopts[option_index].name,
					CRYPTODEV_KEYWORD) == 0) {
				ret = parse_cryptodev_arg(optarg);
				if (ret < 0) {
					cryptodev_fips_validate_usage(prgname);
					return -EINVAL;
				}
			} else if (strcmp(lgopts[option_index].name,
					CRYPTODEV_ID_KEYWORD) == 0) {
				ret = parse_cryptodev_id_arg(optarg);
				if (ret < 0) {
					cryptodev_fips_validate_usage(prgname);
					return -EINVAL;
				}
			} else {
				cryptodev_fips_validate_usage(prgname);
				return -EINVAL;
			}
			break;
		default:
			return -1;
		}
	}

	if (env.req_path == NULL || env.rsp_path == NULL ||
			env.dev_id == UINT32_MAX) {
		cryptodev_fips_validate_usage(prgname);
		return -EINVAL;
	}

	return 0;
}

int
main(int argc, char *argv[])
{
	int ret;

	ret = rte_eal_init(argc, argv);
	if (ret < 0) {
		RTE_LOG(ERR, USER1, "Error %i: Failed init\n", ret);
		return -1;
	}

	argc -= ret;
	argv += ret;

	ret = cryptodev_fips_validate_parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Failed to parse arguments!\n");

	ret = cryptodev_fips_validate_app_int();
	if (ret < 0) {
		RTE_LOG(ERR, USER1, "Error %i: Failed init\n", ret);
		return -1;
	}

	if (!env.is_path_folder) {
		printf("Processing file %s... ", env.req_path);

		ret = fips_test_init(env.req_path, env.rsp_path,
			rte_cryptodev_name_get(env.dev_id));
		if (ret < 0) {
			RTE_LOG(ERR, USER1, "Error %i: Failed test %s\n",
					ret, env.req_path);
			goto exit;
		}


		ret = fips_test_one_file();
		if (ret < 0) {
			RTE_LOG(ERR, USER1, "Error %i: Failed test %s\n",
					ret, env.req_path);
			goto exit;
		}

		printf("Done\n");

	} else {
		struct dirent *dir;
		DIR *d_req, *d_rsp;
		char req_path[1024];
		char rsp_path[1024];

		d_req = opendir(env.req_path);
		if (!d_req) {
			RTE_LOG(ERR, USER1, "Error %i: Path %s not exist\n",
					-EINVAL, env.req_path);
			goto exit;
		}

		d_rsp = opendir(env.rsp_path);
		if (!d_rsp) {
			ret = mkdir(env.rsp_path, 0700);
			if (ret == 0)
				d_rsp = opendir(env.rsp_path);
			else {
				RTE_LOG(ERR, USER1, "Error %i: Invalid %s\n",
						-EINVAL, env.rsp_path);
				goto exit;
			}
		}
		closedir(d_rsp);

		while ((dir = readdir(d_req)) != NULL) {
			if (strstr(dir->d_name, "req") == NULL)
				continue;

			snprintf(req_path, 1023, "%s/%s", env.req_path,
					dir->d_name);
			snprintf(rsp_path, 1023, "%s/%s", env.rsp_path,
					dir->d_name);
			strlcpy(strstr(rsp_path, "req"), "rsp", 4);

			printf("Processing file %s... ", req_path);

			ret = fips_test_init(req_path, rsp_path,
			rte_cryptodev_name_get(env.dev_id));
			if (ret < 0) {
				RTE_LOG(ERR, USER1, "Error %i: Failed test %s\n",
						ret, req_path);
				break;
			}

			ret = fips_test_one_file();
			if (ret < 0) {
				RTE_LOG(ERR, USER1, "Error %i: Failed test %s\n",
						ret, req_path);
				break;
			}

			printf("Done\n");
		}

		closedir(d_req);
	}


exit:
	fips_test_clear();
	cryptodev_fips_validate_app_uninit();

	return ret;

}

static void
print_test_block(void)
{
	uint32_t i;

	for (i = 0; i < info.nb_vec_lines; i++)
		printf("%s\n", info.vec[i]);

	printf("\n");
}

static int
fips_test_one_file(void)
{
	int fetch_ret = 0, ret;

	while (fetch_ret == 0) {
		fetch_ret = fips_test_fetch_one_block();
		if (fetch_ret < 0) {
			RTE_LOG(ERR, USER1, "Error %i: Fetch block\n",
					fetch_ret);
			ret = fetch_ret;
			goto error_one_case;
		}

		if (info.nb_vec_lines == 0) {
			if (fetch_ret == -EOF)
				break;

			fprintf(info.fp_wr, "\n");
			continue;
		}

		ret = fips_test_parse_one_case();
		switch (ret) {
		case 0:
			if (ret == 0)
				break;
			RTE_LOG(ERR, USER1, "Error %i: test block\n",
					ret);
			goto error_one_case;
		case 1:
			break;
		default:
			RTE_LOG(ERR, USER1, "Error %i: Parse block\n",
					ret);
			goto error_one_case;
		}

		continue;
error_one_case:
		print_test_block();
	}

	fips_test_clear();

}
