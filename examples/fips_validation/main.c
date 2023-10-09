/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <sys/stat.h>
#include <getopt.h>
#include <dirent.h>
#include <stdlib.h>

#include <rte_cryptodev.h>
#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_string_fns.h>
#include <rte_random.h>

#include "fips_validation.h"
#include "fips_dev_self_test.h"

enum {
#define OPT_REQ_FILE_PATH           "req-file"
	OPT_REQ_FILE_PATH_NUM = 256,
#define OPT_RSP_FILE_PATH           "rsp-file"
	OPT_RSP_FILE_PATH_NUM,
#define OPT_MBUF_DATAROOM           "mbuf-dataroom"
	OPT_MBUF_DATAROOM_NUM,
#define OPT_FOLDER                  "path-is-folder"
	OPT_FOLDER_NUM,
#define OPT_CRYPTODEV               "cryptodev"
	OPT_CRYPTODEV_NUM,
#define OPT_CRYPTODEV_ID            "cryptodev-id"
	OPT_CRYPTODEV_ID_NUM,
#define OPT_CRYPTODEV_ST            "self-test"
	OPT_CRYPTODEV_ST_NUM,
#define OPT_CRYPTODEV_BK_ID         "broken-test-id"
	OPT_CRYPTODEV_BK_ID_NUM,
#define OPT_CRYPTODEV_BK_DIR_KEY    "broken-test-dir"
	OPT_CRYPTODEV_BK_DIR_KEY_NUM,
#define OPT_USE_JSON                "use-json"
	OPT_USE_JSON_NUM,
#define OPT_CRYPTODEV_ASYM          "asymmetric"
	OPT_CRYPTODEV_ASYM_NUM,
};

struct fips_test_vector vec;
struct fips_test_interim_info info;

#ifdef USE_JANSSON
struct fips_test_json_info json_info;
#endif /* USE_JANSSON */

struct cryptodev_fips_validate_env {
	const char *req_path;
	const char *rsp_path;
	uint32_t is_path_folder;
	uint8_t dev_id;
	struct rte_mempool *mpool;
	struct fips_sym_env {
		struct rte_mempool *sess_mpool;
		struct rte_mempool *op_pool;
		struct rte_cryptodev_sym_session *sess;
		struct rte_crypto_op *op;
	} sym;
	struct fips_asym_env {
		struct rte_mempool *sess_mpool;
		struct rte_mempool *op_pool;
		struct rte_cryptodev_asym_session *sess;
		struct rte_crypto_op *op;
	} asym;
	struct rte_crypto_op *op;
	uint8_t dev_support_sgl;
	uint16_t mbuf_data_room;
	struct rte_mbuf *mbuf;
	uint8_t *digest;
	uint16_t digest_len;
	bool is_asym_test;
	uint16_t self_test;
	struct fips_dev_broken_test_config *broken_test_config;
} env;

static int
cryptodev_fips_validate_app_sym_init(void)
{
	uint32_t sess_sz = rte_cryptodev_sym_get_private_session_size(
							env.dev_id);
	struct rte_cryptodev_info dev_info;
	struct fips_sym_env *sym = &env.sym;
	int ret;

	rte_cryptodev_info_get(env.dev_id, &dev_info);
	if (dev_info.feature_flags & RTE_CRYPTODEV_FF_IN_PLACE_SGL)
		env.dev_support_sgl = 1;
	else
		env.dev_support_sgl = 0;

	ret = -ENOMEM;
	sym->sess_mpool = rte_cryptodev_sym_session_pool_create(
			"FIPS_SYM_SESS_MEMPOOL", 16, sess_sz, 0, 0, rte_socket_id());
	if (!sym->sess_mpool)
		goto error_exit;

	sym->op_pool = rte_crypto_op_pool_create(
			"FIPS_OP_SYM_POOL",
			RTE_CRYPTO_OP_TYPE_SYMMETRIC,
			1, 0,
			16,
			rte_socket_id());
	if (!sym->op_pool)
		goto error_exit;

	sym->op = rte_crypto_op_alloc(sym->op_pool, RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	if (!sym->op)
		goto error_exit;

	return 0;

error_exit:
	rte_mempool_free(sym->sess_mpool);
	rte_mempool_free(sym->op_pool);
	return ret;
}

static void
cryptodev_fips_validate_app_sym_uninit(void)
{
	struct fips_sym_env *sym = &env.sym;

	rte_pktmbuf_free(env.mbuf);
	rte_crypto_op_free(sym->op);
	rte_cryptodev_sym_session_free(env.dev_id, sym->sess);
	rte_mempool_free(sym->sess_mpool);
	rte_mempool_free(sym->op_pool);
}

static int
cryptodev_fips_validate_app_asym_init(void)
{
	struct fips_asym_env *asym = &env.asym;
	int ret;

	ret = -ENOMEM;
	asym->sess_mpool = rte_cryptodev_asym_session_pool_create(
			"FIPS_ASYM_SESS_MEMPOOL", 16, 0, 0, rte_socket_id());
	if (!asym->sess_mpool)
		goto error_exit;

	asym->op_pool = rte_crypto_op_pool_create(
			"FIPS_OP_ASYM_POOL",
			RTE_CRYPTO_OP_TYPE_ASYMMETRIC,
			1, 0,
			16,
			rte_socket_id());
	if (!asym->op_pool)
		goto error_exit;

	asym->op = rte_crypto_op_alloc(asym->op_pool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (!asym->op)
		goto error_exit;

	return 0;

error_exit:
	rte_mempool_free(asym->sess_mpool);
	rte_mempool_free(asym->op_pool);
	return ret;
}

static void
cryptodev_fips_validate_app_asym_uninit(void)
{
	struct fips_asym_env *asym = &env.asym;

	rte_crypto_op_free(asym->op);
	rte_cryptodev_asym_session_free(env.dev_id, asym->sess);
	rte_mempool_free(asym->sess_mpool);
	rte_mempool_free(asym->op_pool);
}

static int
cryptodev_fips_validate_app_init(void)
{
	struct rte_cryptodev_config conf = {rte_socket_id(), 1, 0};
	struct rte_cryptodev_qp_conf qp_conf = {128, NULL};
	uint32_t nb_mbufs = UINT16_MAX / env.mbuf_data_room + 1;
	int ret;

	if (env.self_test) {
		ret = fips_dev_self_test(env.dev_id, env.broken_test_config);
		if (ret < 0) {
			rte_cryptodev_stop(env.dev_id);
			rte_cryptodev_close(env.dev_id);

			return ret;
		}
	}

	ret = rte_cryptodev_configure(env.dev_id, &conf);
	if (ret < 0)
		return ret;

	ret = -ENOMEM;
	env.mpool = rte_pktmbuf_pool_create("FIPS_MEMPOOL", nb_mbufs,
			0, 0, sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM +
			env.mbuf_data_room, rte_socket_id());
	if (!env.mpool)
		return ret;

	ret = cryptodev_fips_validate_app_sym_init();
	if (ret < 0)
		goto error_exit;

	if (env.is_asym_test) {
		ret = cryptodev_fips_validate_app_asym_init();
		if (ret < 0)
			goto error_exit;
	}

	qp_conf.mp_session = env.sym.sess_mpool;

	ret = rte_cryptodev_queue_pair_setup(env.dev_id, 0, &qp_conf,
			rte_socket_id());
	if (ret < 0)
		goto error_exit;

	ret = rte_cryptodev_start(env.dev_id);
	if (ret < 0)
		goto error_exit;

	return 0;

error_exit:
	rte_mempool_free(env.mpool);
	return ret;
}

static void
cryptodev_fips_validate_app_uninit(void)
{
	cryptodev_fips_validate_app_sym_uninit();

	if (env.is_asym_test)
		cryptodev_fips_validate_app_asym_uninit();

	rte_mempool_free(env.mpool);
	rte_cryptodev_stop(env.dev_id);
	rte_cryptodev_close(env.dev_id);
}

static int
fips_test_one_file(void);

#ifdef USE_JANSSON
static int
fips_test_one_json_file(void);
#endif /* USE_JANSSON */

static int
parse_cryptodev_arg(char *arg)
{
	int id = rte_cryptodev_get_dev_id(arg);

	if (id < 0) {
		RTE_LOG(ERR, USER1, "Error %i: invalid cryptodev name %s\n",
				id, arg);
		return id;
	}

	env.dev_id = (uint8_t)id;

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


	if (!rte_cryptodev_is_valid_dev(cryptodev_id)) {
		RTE_LOG(ERR, USER1, "Error %i: invalid cryptodev id %s\n",
				cryptodev_id, arg);
		return -1;
	}

	env.dev_id = (uint8_t)cryptodev_id;

	return 0;
}

static void
cryptodev_fips_validate_usage(const char *prgname)
{
	uint32_t def_mbuf_seg_size = DEF_MBUF_SEG_SIZE;
	printf("%s [EAL options] --\n"
		"  --%s: REQUEST-FILE-PATH\n"
		"  --%s: RESPONSE-FILE-PATH\n"
		"  --%s: indicating both paths are folders\n"
		"  --%s: mbuf dataroom size (default %u bytes)\n"
		"  --%s: CRYPTODEV-NAME\n"
		"  --%s: CRYPTODEV-ID-NAME\n"
		"  --%s: self test indicator\n"
		"  --%s: self broken test ID\n"
		"  --%s: self broken test direction\n",
		prgname, OPT_REQ_FILE_PATH, OPT_RSP_FILE_PATH,
		OPT_FOLDER, OPT_MBUF_DATAROOM, def_mbuf_seg_size,
		OPT_CRYPTODEV, OPT_CRYPTODEV_ID, OPT_CRYPTODEV_ST,
		OPT_CRYPTODEV_BK_ID, OPT_CRYPTODEV_BK_DIR_KEY);
}

static int
cryptodev_fips_validate_parse_args(int argc, char **argv)
{
	int opt, ret;
	char *prgname = argv[0];
	char **argvopt;
	int option_index;
	struct option lgopts[] = {
		{OPT_REQ_FILE_PATH, required_argument,
				NULL, OPT_REQ_FILE_PATH_NUM},
		{OPT_RSP_FILE_PATH, required_argument,
				NULL, OPT_RSP_FILE_PATH_NUM},
		{OPT_FOLDER, no_argument,
				NULL, OPT_FOLDER_NUM},
		{OPT_MBUF_DATAROOM, required_argument,
				NULL, OPT_MBUF_DATAROOM_NUM},
		{OPT_CRYPTODEV, required_argument,
				NULL, OPT_CRYPTODEV_NUM},
		{OPT_CRYPTODEV_ID, required_argument,
				NULL, OPT_CRYPTODEV_ID_NUM},
		{OPT_CRYPTODEV_ST, no_argument,
				NULL, OPT_CRYPTODEV_ST_NUM},
		{OPT_CRYPTODEV_BK_ID, required_argument,
				NULL, OPT_CRYPTODEV_BK_ID_NUM},
		{OPT_CRYPTODEV_BK_DIR_KEY, required_argument,
				NULL, OPT_CRYPTODEV_BK_DIR_KEY_NUM},
		{OPT_CRYPTODEV_ASYM, no_argument,
				NULL, OPT_CRYPTODEV_ASYM_NUM},
		{NULL, 0, 0, 0}
	};

	argvopt = argv;

	env.mbuf_data_room = DEF_MBUF_SEG_SIZE;
	if (rte_cryptodev_count())
		env.dev_id = 0;
	else {
		cryptodev_fips_validate_usage(prgname);
		return -EINVAL;
	}

	while ((opt = getopt_long(argc, argvopt, "s:",
				  lgopts, &option_index)) != EOF) {

		switch (opt) {
		case OPT_REQ_FILE_PATH_NUM:
			env.req_path = optarg;
			break;

		case OPT_RSP_FILE_PATH_NUM:
			env.rsp_path = optarg;
			break;

		case OPT_FOLDER_NUM:
			env.is_path_folder = 1;
			break;

		case OPT_CRYPTODEV_NUM:
			ret = parse_cryptodev_arg(optarg);
			if (ret < 0) {
				cryptodev_fips_validate_usage(prgname);
				return -EINVAL;
			}
			break;

		case OPT_CRYPTODEV_ID_NUM:
			ret = parse_cryptodev_id_arg(optarg);
			if (ret < 0) {
				cryptodev_fips_validate_usage(prgname);
				return -EINVAL;
			}
			break;

		case OPT_CRYPTODEV_ST_NUM:
			env.self_test = 1;
			break;

		case OPT_CRYPTODEV_BK_ID_NUM:
			if (!env.broken_test_config) {
				env.broken_test_config = rte_malloc(
					NULL,
					sizeof(*env.broken_test_config),
					0);
				if (!env.broken_test_config)
					return -ENOMEM;

				env.broken_test_config->expect_fail_dir =
					self_test_dir_enc_auth_gen;
			}

			if (parser_read_uint32(
				&env.broken_test_config->expect_fail_test_idx,
					optarg) < 0) {
				rte_free(env.broken_test_config);
				cryptodev_fips_validate_usage(prgname);
				return -EINVAL;
			}
			break;

		case OPT_CRYPTODEV_BK_DIR_KEY_NUM:
			if (!env.broken_test_config) {
				env.broken_test_config = rte_malloc(
					NULL,
					sizeof(*env.broken_test_config),
					0);
				if (!env.broken_test_config)
					return -ENOMEM;

				env.broken_test_config->expect_fail_test_idx =
					0;
			}

			if (strcmp(optarg, "enc") == 0)
				env.broken_test_config->expect_fail_dir =
					self_test_dir_enc_auth_gen;
			else if (strcmp(optarg, "dec")
					== 0)
				env.broken_test_config->expect_fail_dir =
					self_test_dir_dec_auth_verify;
			else {
				rte_free(env.broken_test_config);
				cryptodev_fips_validate_usage(prgname);
				return -EINVAL;
			}
			break;


		case OPT_MBUF_DATAROOM_NUM:
			if (parser_read_uint16(&env.mbuf_data_room,
					optarg) < 0) {
				cryptodev_fips_validate_usage(prgname);
				return -EINVAL;
			}

			if (env.mbuf_data_room == 0) {
				cryptodev_fips_validate_usage(prgname);
				return -EINVAL;
			}
			break;

		case OPT_CRYPTODEV_ASYM_NUM:
			env.is_asym_test = true;
			break;

		default:
			cryptodev_fips_validate_usage(prgname);
			return -EINVAL;
		}
	}

	if ((env.req_path == NULL && env.rsp_path != NULL) ||
			(env.req_path != NULL && env.rsp_path == NULL)) {
		RTE_LOG(ERR, USER1, "Missing req path or rsp path\n");
		cryptodev_fips_validate_usage(prgname);
		return -EINVAL;
	}

	if (env.req_path == NULL && env.self_test == 0) {
		RTE_LOG(ERR, USER1, "--self-test must be set if req path is missing\n");
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

	ret = cryptodev_fips_validate_app_init();
	if (ret < 0) {
		RTE_LOG(ERR, USER1, "Error %i: Failed init\n", ret);
		return -1;
	}

	if (env.req_path == NULL || env.rsp_path == NULL) {
		printf("No request, exit.\n");
		goto exit;
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

#ifdef USE_JANSSON
		if (info.file_type == FIPS_TYPE_JSON) {
			ret = fips_test_one_json_file();
			json_decref(json_info.json_root);
		}  else {
			ret = fips_test_one_file();
		}
#else /* USE_JANSSON */
		ret = fips_test_one_file();
#endif /* USE_JANSSON */

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

#ifdef USE_JANSSON
			if (info.file_type == FIPS_TYPE_JSON) {
				ret = fips_test_one_json_file();
				json_decref(json_info.json_root);
			} else {
				ret = fips_test_one_file();
			}
#else /* USE_JANSSON */
			ret = fips_test_one_file();
#endif /* USE_JANSSON */

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

	/* clean up the EAL */
	rte_eal_cleanup();

	return ret;

}

#define IV_OFF (sizeof(struct rte_crypto_op) + sizeof(struct rte_crypto_sym_op))
#define CRYPTODEV_FIPS_MAX_RETRIES	16

struct fips_test_ops test_ops;

static int
prepare_data_mbufs(struct fips_val *val)
{
	struct rte_mbuf *m, *head = 0;
	uint8_t *src = val->val;
	uint32_t total_len = val->len;
	uint16_t nb_seg;
	int ret = 0;

	rte_pktmbuf_free(env.mbuf);

	if (total_len > RTE_MBUF_MAX_NB_SEGS) {
		RTE_LOG(ERR, USER1, "Data len %u too big\n", total_len);
		return -EPERM;
	}

	nb_seg = total_len / env.mbuf_data_room;
	if (total_len % env.mbuf_data_room)
		nb_seg++;

	m = rte_pktmbuf_alloc(env.mpool);
	if (!m) {
		RTE_LOG(ERR, USER1, "Error %i: Not enough mbuf\n",
				-ENOMEM);
		return -ENOMEM;
	}
	head = m;

	while (nb_seg) {
		uint16_t len = RTE_MIN(total_len, env.mbuf_data_room);
		uint8_t *dst = (uint8_t *)rte_pktmbuf_append(m, len);

		if (!dst) {
			RTE_LOG(ERR, USER1, "Error %i: MBUF too small\n",
					-ENOMEM);
			ret = -ENOMEM;
			goto error_exit;
		}

		memcpy(dst, src, len);

		if (head != m) {
			ret = rte_pktmbuf_chain(head, m);
			if (ret) {
				rte_pktmbuf_free(m);
				RTE_LOG(ERR, USER1, "Error %i: SGL build\n",
						ret);
				goto error_exit;
			}
		}
		total_len -= len;

		if (total_len) {
			if (!env.dev_support_sgl) {
				RTE_LOG(ERR, USER1, "SGL not supported\n");
				ret = -EPERM;
				goto error_exit;
			}

			m = rte_pktmbuf_alloc(env.mpool);
			if (!m) {
				RTE_LOG(ERR, USER1, "Error %i: No memory\n",
						-ENOMEM);
				goto error_exit;
			}
		} else
			break;

		src += len;
		nb_seg--;
	}

	if (total_len) {
		RTE_LOG(ERR, USER1, "Error %i: Failed to store all data\n",
				-ENOMEM);
		goto error_exit;
	}

	env.mbuf = head;

	return 0;

error_exit:
	rte_pktmbuf_free(head);
	return ret;
}

static int
prepare_cipher_op(void)
{
	struct rte_crypto_sym_op *sym = env.op->sym;
	uint8_t *iv = rte_crypto_op_ctod_offset(env.op, uint8_t *, IV_OFF);
	int ret;

	__rte_crypto_op_reset(env.op, RTE_CRYPTO_OP_TYPE_SYMMETRIC);

	memcpy(iv, vec.iv.val, vec.iv.len);

	if (info.op == FIPS_TEST_ENC_AUTH_GEN) {
		ret = prepare_data_mbufs(&vec.pt);
		if (ret < 0)
			return ret;

		sym->cipher.data.length = vec.pt.len;
	} else {
		ret = prepare_data_mbufs(&vec.ct);
		if (ret < 0)
			return ret;

		sym->cipher.data.length = vec.ct.len;
	}

	rte_crypto_op_attach_sym_session(env.op, env.sym.sess);

	sym->m_src = env.mbuf;
	sym->cipher.data.offset = 0;

	return 0;
}

int
prepare_auth_op(void)
{
	struct rte_crypto_sym_op *sym = env.op->sym;
	int ret;

	__rte_crypto_op_reset(env.op, RTE_CRYPTO_OP_TYPE_SYMMETRIC);

	if (info.interim_info.gcm_data.gen_iv == 1) {
		uint32_t i;

		if (!vec.iv.val) {
			vec.iv.val = rte_malloc(0, vec.iv.len, 0);
			if (!vec.iv.val)
				return -ENOMEM;
		}

		for (i = 0; i < vec.iv.len; i++) {
			int random = rte_rand();
			vec.iv.val[i] = (uint8_t)random;
		}
	}

	if (vec.iv.len) {
		uint8_t *iv = rte_crypto_op_ctod_offset(env.op, uint8_t *,
				IV_OFF);
		memset(iv, 0, vec.iv.len);
		if (vec.iv.val)
			memcpy(iv, vec.iv.val, vec.iv.len);
	}

	ret = prepare_data_mbufs(&vec.pt);
	if (ret < 0)
		return ret;

	rte_free(env.digest);

	env.digest = rte_zmalloc(NULL, vec.cipher_auth.digest.len,
			RTE_CACHE_LINE_SIZE);
	if (!env.digest) {
		RTE_LOG(ERR, USER1, "Not enough memory\n");
		return -ENOMEM;
	}
	env.digest_len = vec.cipher_auth.digest.len;

	sym->m_src = env.mbuf;
	sym->auth.data.offset = 0;
	sym->auth.data.length = vec.pt.len;
	sym->auth.digest.data = env.digest;
	sym->auth.digest.phys_addr = rte_malloc_virt2iova(env.digest);

	if (info.op == FIPS_TEST_DEC_AUTH_VERIF)
		memcpy(env.digest, vec.cipher_auth.digest.val,
				vec.cipher_auth.digest.len);

	rte_crypto_op_attach_sym_session(env.op, env.sym.sess);

	return 0;
}

int
prepare_aead_op(void)
{
	struct rte_crypto_sym_op *sym = env.op->sym;
	uint8_t *iv = rte_crypto_op_ctod_offset(env.op, uint8_t *, IV_OFF);
	int ret;

	__rte_crypto_op_reset(env.op, RTE_CRYPTO_OP_TYPE_SYMMETRIC);

	if (info.algo == FIPS_TEST_ALGO_AES_CCM)
		iv++;

	if (vec.iv.val)
		memcpy(iv, vec.iv.val, vec.iv.len);
	else
		/* if REQ file has iv length but not data, default as all 0 */
		memset(iv, 0, vec.iv.len);

	if (info.op == FIPS_TEST_ENC_AUTH_GEN) {
		ret = prepare_data_mbufs(&vec.pt);
		if (ret < 0)
			return ret;

		rte_free(env.digest);
		env.digest = rte_zmalloc(NULL, vec.aead.digest.len,
				RTE_CACHE_LINE_SIZE);
		if (!env.digest) {
			RTE_LOG(ERR, USER1, "Not enough memory\n");
			return -ENOMEM;
		}
		env.digest_len = vec.aead.digest.len;

		sym->aead.data.length = vec.pt.len;
		sym->aead.digest.data = env.digest;
		sym->aead.digest.phys_addr = rte_malloc_virt2iova(env.digest);
	} else {
		ret = prepare_data_mbufs(&vec.ct);
		if (ret < 0)
			return ret;
		env.digest_len = vec.aead.digest.len;
		sym->aead.data.length = vec.ct.len;
		sym->aead.digest.data = vec.aead.digest.val;
		sym->aead.digest.phys_addr = rte_malloc_virt2iova(
				sym->aead.digest.data);
	}

	sym->m_src = env.mbuf;
	sym->aead.data.offset = 0;
	sym->aead.aad.data = vec.aead.aad.val;
	sym->aead.aad.phys_addr = rte_malloc_virt2iova(sym->aead.aad.data);

	rte_crypto_op_attach_sym_session(env.op, env.sym.sess);

	return 0;
}

static int
get_hash_oid(enum rte_crypto_auth_algorithm hash, uint8_t *buf)
{
	uint8_t id_sha512[] = {0x30, 0x51, 0x30, 0x0d, 0x06, 0x09,
				  0x60, 0x86, 0x48, 0x01, 0x65, 0x03,
				  0x04, 0x02, 0x03, 0x05, 0x00, 0x04,
				  0x40};
	uint8_t id_sha384[] = {0x30, 0x41, 0x30, 0x0d, 0x06, 0x09,
				  0x60, 0x86, 0x48, 0x01, 0x65, 0x03,
				  0x04, 0x02, 0x02, 0x05, 0x00, 0x04,
				  0x30};
	uint8_t id_sha256[] = {0x30, 0x31, 0x30, 0x0d, 0x06, 0x09,
				  0x60, 0x86, 0x48, 0x01, 0x65, 0x03,
				  0x04, 0x02, 0x01, 0x05, 0x00, 0x04,
				  0x20};
	uint8_t id_sha224[] = {0x30, 0x2d, 0x30, 0x0d, 0x06, 0x09,
				  0x60, 0x86, 0x48, 0x01, 0x65, 0x03,
				  0x04, 0x02, 0x04, 0x05, 0x00, 0x04,
				  0x1c};
	uint8_t id_sha1[] = {0x30, 0x21, 0x30, 0x09, 0x06, 0x05,
				0x2b, 0x0e, 0x03, 0x02, 0x1a, 0x05,
				0x00, 0x04, 0x14};
	uint8_t *id = NULL;
	int id_len = 0;

	switch (hash) {
	case RTE_CRYPTO_AUTH_SHA1:
		id = id_sha1;
		id_len = sizeof(id_sha1);
		break;
	case RTE_CRYPTO_AUTH_SHA224:
		id = id_sha224;
		id_len = sizeof(id_sha224);
		break;
	case RTE_CRYPTO_AUTH_SHA256:
		id = id_sha256;
		id_len = sizeof(id_sha256);
		break;
	case RTE_CRYPTO_AUTH_SHA384:
		id = id_sha384;
		id_len = sizeof(id_sha384);
		break;
	case RTE_CRYPTO_AUTH_SHA512:
		id = id_sha512;
		id_len = sizeof(id_sha512);
		break;
	default:
		id_len = -1;
		break;
	}

	if (id != NULL)
		rte_memcpy(buf, id, id_len);

	return id_len;
}

static int
prepare_rsa_op(void)
{
	struct rte_crypto_asym_op *asym;
	struct fips_val msg;

	__rte_crypto_op_reset(env.op, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);

	asym = env.op->asym;
	asym->rsa.padding.type = info.interim_info.rsa_data.padding;
	asym->rsa.padding.hash = info.interim_info.rsa_data.auth;

	if (env.digest) {
		if (asym->rsa.padding.type == RTE_CRYPTO_RSA_PADDING_PKCS1_5) {
			int b_len = 0;
			uint8_t b[32];

			b_len = get_hash_oid(asym->rsa.padding.hash, b);
			if (b_len < 0) {
				RTE_LOG(ERR, USER1, "Failed to get digest info for hash %d\n",
					asym->rsa.padding.hash);
				return -EINVAL;
			}

			if (b_len) {
				msg.len = env.digest_len + b_len;
				msg.val = rte_zmalloc(NULL, msg.len, 0);
				rte_memcpy(msg.val, b, b_len);
				rte_memcpy(msg.val + b_len, env.digest, env.digest_len);
				rte_free(env.digest);
				env.digest = msg.val;
				env.digest_len = msg.len;
			}
		}
		msg.val = env.digest;
		msg.len = env.digest_len;
	} else {
		msg.val = vec.pt.val;
		msg.len = vec.pt.len;
	}

	if (info.op == FIPS_TEST_ASYM_SIGGEN) {
		asym->rsa.op_type = RTE_CRYPTO_ASYM_OP_SIGN;
		asym->rsa.message.data = msg.val;
		asym->rsa.message.length = msg.len;

		rte_free(vec.rsa.signature.val);

		vec.rsa.signature.val = rte_zmalloc(NULL, vec.rsa.n.len, 0);
		vec.rsa.signature.len = vec.rsa.n.len;
		asym->rsa.sign.data = vec.rsa.signature.val;
		asym->rsa.sign.length = 0;
	} else if (info.op == FIPS_TEST_ASYM_SIGVER) {
		asym->rsa.op_type = RTE_CRYPTO_ASYM_OP_VERIFY;
		asym->rsa.message.data = msg.val;
		asym->rsa.message.length = msg.len;
		asym->rsa.sign.data = vec.rsa.signature.val;
		asym->rsa.sign.length = vec.rsa.signature.len;
	} else {
		RTE_LOG(ERR, USER1, "Invalid op %d\n", info.op);
		return -EINVAL;
	}

	rte_crypto_op_attach_asym_session(env.op, env.asym.sess);

	return 0;
}

static int
prepare_ecdsa_op(void)
{
	struct rte_crypto_asym_op *asym;
	struct fips_val msg;

	__rte_crypto_op_reset(env.op, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);

	asym = env.op->asym;
	if (env.digest) {
		msg.val = env.digest;
		msg.len = env.digest_len;
	} else {
		msg.val = vec.pt.val;
		msg.len = vec.pt.len;
	}

	if (info.op == FIPS_TEST_ASYM_SIGGEN) {
		asym->ecdsa.op_type = RTE_CRYPTO_ASYM_OP_SIGN;
		asym->ecdsa.message.data = msg.val;
		asym->ecdsa.message.length = msg.len;
		asym->ecdsa.k.data = vec.ecdsa.k.val;
		asym->ecdsa.k.length = vec.ecdsa.k.len;

		rte_free(vec.ecdsa.r.val);

		rte_free(vec.ecdsa.s.val);

		vec.ecdsa.r.len = info.interim_info.ecdsa_data.curve_len;
		vec.ecdsa.r.val = rte_zmalloc(NULL, vec.ecdsa.r.len, 0);

		vec.ecdsa.s.len = vec.ecdsa.r.len;
		vec.ecdsa.s.val = rte_zmalloc(NULL, vec.ecdsa.s.len, 0);

		asym->ecdsa.r.data = vec.ecdsa.r.val;
		asym->ecdsa.r.length = 0;
		asym->ecdsa.s.data = vec.ecdsa.s.val;
		asym->ecdsa.s.length = 0;
	} else if (info.op == FIPS_TEST_ASYM_SIGVER) {
		asym->ecdsa.op_type = RTE_CRYPTO_ASYM_OP_VERIFY;
		asym->ecdsa.message.data = msg.val;
		asym->ecdsa.message.length = msg.len;
		asym->ecdsa.r.data = vec.ecdsa.r.val;
		asym->ecdsa.r.length = vec.ecdsa.r.len;
		asym->ecdsa.s.data = vec.ecdsa.s.val;
		asym->ecdsa.s.length = vec.ecdsa.s.len;
	} else {
		RTE_LOG(ERR, USER1, "Invalid op %d\n", info.op);
		return -EINVAL;
	}

	rte_crypto_op_attach_asym_session(env.op, env.asym.sess);

	return 0;
}

static int
prepare_ecfpm_op(void)
{
	struct rte_crypto_asym_op *asym;

	__rte_crypto_op_reset(env.op, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);

	asym = env.op->asym;
	asym->ecpm.scalar.data = vec.ecdsa.pkey.val;
	asym->ecpm.scalar.length = vec.ecdsa.pkey.len;

	rte_free(vec.ecdsa.qx.val);

	rte_free(vec.ecdsa.qy.val);

	vec.ecdsa.qx.len = info.interim_info.ecdsa_data.curve_len;
	vec.ecdsa.qx.val = rte_zmalloc(NULL, vec.ecdsa.qx.len, 0);

	vec.ecdsa.qy.len = vec.ecdsa.qx.len;
	vec.ecdsa.qy.val = rte_zmalloc(NULL, vec.ecdsa.qy.len, 0);

	asym->ecpm.r.x.data = vec.ecdsa.qx.val;
	asym->ecpm.r.x.length = 0;
	asym->ecpm.r.y.data = vec.ecdsa.qy.val;
	asym->ecpm.r.y.length = 0;

	rte_crypto_op_attach_asym_session(env.op, env.asym.sess);

	return 0;
}

static int
prepare_aes_xform(struct rte_crypto_sym_xform *xform)
{
	const struct rte_cryptodev_symmetric_capability *cap;
	struct rte_cryptodev_sym_capability_idx cap_idx;
	struct rte_crypto_cipher_xform *cipher_xform = &xform->cipher;

	xform->type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	if (info.interim_info.aes_data.cipher_algo == RTE_CRYPTO_CIPHER_AES_CBC)
		cipher_xform->algo = RTE_CRYPTO_CIPHER_AES_CBC;
	else if (info.interim_info.aes_data.cipher_algo ==
			RTE_CRYPTO_CIPHER_AES_CTR)
		cipher_xform->algo = RTE_CRYPTO_CIPHER_AES_CTR;
	else
		cipher_xform->algo = RTE_CRYPTO_CIPHER_AES_ECB;

	cipher_xform->op = (info.op == FIPS_TEST_ENC_AUTH_GEN) ?
			RTE_CRYPTO_CIPHER_OP_ENCRYPT :
			RTE_CRYPTO_CIPHER_OP_DECRYPT;
	cipher_xform->key.data = vec.cipher_auth.key.val;
	cipher_xform->key.length = vec.cipher_auth.key.len;
	if (cipher_xform->algo == RTE_CRYPTO_CIPHER_AES_CBC ||
			cipher_xform->algo == RTE_CRYPTO_CIPHER_AES_CTR) {
		cipher_xform->iv.length = vec.iv.len;
		cipher_xform->iv.offset = IV_OFF;
	} else {
		cipher_xform->iv.length = 0;
		cipher_xform->iv.offset = 0;
	}
	cap_idx.algo.cipher = cipher_xform->algo;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;

	cap = rte_cryptodev_sym_capability_get(env.dev_id, &cap_idx);
	if (!cap) {
		RTE_LOG(ERR, USER1, "Failed to get capability for cdev %u\n",
				env.dev_id);
		return -EINVAL;
	}

	if (rte_cryptodev_sym_capability_check_cipher(cap,
			cipher_xform->key.length,
			cipher_xform->iv.length) != 0) {
		RTE_LOG(ERR, USER1, "PMD %s key length %u IV length %u\n",
				info.device_name, cipher_xform->key.length,
				cipher_xform->iv.length);
		return -EPERM;
	}

	return 0;
}

static int
prepare_tdes_xform(struct rte_crypto_sym_xform *xform)
{
	const struct rte_cryptodev_symmetric_capability *cap;
	struct rte_cryptodev_sym_capability_idx cap_idx;
	struct rte_crypto_cipher_xform *cipher_xform = &xform->cipher;

	xform->type = RTE_CRYPTO_SYM_XFORM_CIPHER;

	if (info.interim_info.tdes_data.test_mode == TDES_MODE_CBC)
		cipher_xform->algo = RTE_CRYPTO_CIPHER_3DES_CBC;
	else
		cipher_xform->algo = RTE_CRYPTO_CIPHER_3DES_ECB;
	cipher_xform->op = (info.op == FIPS_TEST_ENC_AUTH_GEN) ?
			RTE_CRYPTO_CIPHER_OP_ENCRYPT :
			RTE_CRYPTO_CIPHER_OP_DECRYPT;
	cipher_xform->key.data = vec.cipher_auth.key.val;
	cipher_xform->key.length = vec.cipher_auth.key.len;

	if (cipher_xform->algo == RTE_CRYPTO_CIPHER_3DES_CBC) {
		cipher_xform->iv.length = vec.iv.len;
		cipher_xform->iv.offset = IV_OFF;
	} else {
		cipher_xform->iv.length = 0;
		cipher_xform->iv.offset = 0;
	}
	cap_idx.algo.cipher = cipher_xform->algo;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;

	cap = rte_cryptodev_sym_capability_get(env.dev_id, &cap_idx);
	if (!cap) {
		RTE_LOG(ERR, USER1, "Failed to get capability for cdev %u\n",
				env.dev_id);
		return -EINVAL;
	}

	if (rte_cryptodev_sym_capability_check_cipher(cap,
			cipher_xform->key.length,
			cipher_xform->iv.length) != 0) {
		RTE_LOG(ERR, USER1, "PMD %s key length %u IV length %u\n",
				info.device_name, cipher_xform->key.length,
				cipher_xform->iv.length);
		return -EPERM;
	}

	return 0;
}

static int
prepare_hmac_xform(struct rte_crypto_sym_xform *xform)
{
	const struct rte_cryptodev_symmetric_capability *cap;
	struct rte_cryptodev_sym_capability_idx cap_idx;
	struct rte_crypto_auth_xform *auth_xform = &xform->auth;

	xform->type = RTE_CRYPTO_SYM_XFORM_AUTH;

	auth_xform->algo = info.interim_info.hmac_data.algo;
	auth_xform->op = RTE_CRYPTO_AUTH_OP_GENERATE;
	auth_xform->digest_length = vec.cipher_auth.digest.len;
	auth_xform->key.data = vec.cipher_auth.key.val;
	auth_xform->key.length = vec.cipher_auth.key.len;

	cap_idx.algo.auth = auth_xform->algo;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;

	cap = rte_cryptodev_sym_capability_get(env.dev_id, &cap_idx);
	if (!cap) {
		RTE_LOG(ERR, USER1, "Failed to get capability for cdev %u\n",
				env.dev_id);
		return -EINVAL;
	}

	if (rte_cryptodev_sym_capability_check_auth(cap,
			auth_xform->key.length,
			auth_xform->digest_length, 0) != 0) {
		RTE_LOG(ERR, USER1, "PMD %s key length %u Digest length %u\n",
				info.device_name, auth_xform->key.length,
				auth_xform->digest_length);
		return -EPERM;
	}

	return 0;
}

int
prepare_gcm_xform(struct rte_crypto_sym_xform *xform)
{
	const struct rte_cryptodev_symmetric_capability *cap;
	struct rte_cryptodev_sym_capability_idx cap_idx;
	struct rte_crypto_aead_xform *aead_xform = &xform->aead;

	xform->type = RTE_CRYPTO_SYM_XFORM_AEAD;

	aead_xform->algo = RTE_CRYPTO_AEAD_AES_GCM;
	aead_xform->aad_length = vec.aead.aad.len;
	aead_xform->digest_length = vec.aead.digest.len;
	aead_xform->iv.offset = IV_OFF;
	aead_xform->iv.length = vec.iv.len;
	aead_xform->key.data = vec.aead.key.val;
	aead_xform->key.length = vec.aead.key.len;
	aead_xform->op = (info.op == FIPS_TEST_ENC_AUTH_GEN) ?
			RTE_CRYPTO_AEAD_OP_ENCRYPT :
			RTE_CRYPTO_AEAD_OP_DECRYPT;

	cap_idx.algo.aead = aead_xform->algo;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AEAD;

	cap = rte_cryptodev_sym_capability_get(env.dev_id, &cap_idx);
	if (!cap) {
		RTE_LOG(ERR, USER1, "Failed to get capability for cdev %u\n",
				env.dev_id);
		return -EINVAL;
	}

	if (rte_cryptodev_sym_capability_check_aead(cap,
			aead_xform->key.length,
			aead_xform->digest_length, aead_xform->aad_length,
			aead_xform->iv.length) != 0) {
		RTE_LOG(ERR, USER1,
			"PMD %s key_len %u tag_len %u aad_len %u iv_len %u\n",
				info.device_name, aead_xform->key.length,
				aead_xform->digest_length,
				aead_xform->aad_length,
				aead_xform->iv.length);
		return -EPERM;
	}

	return 0;
}

int
prepare_gmac_xform(struct rte_crypto_sym_xform *xform)
{
	const struct rte_cryptodev_symmetric_capability *cap;
	struct rte_cryptodev_sym_capability_idx cap_idx;
	struct rte_crypto_auth_xform *auth_xform = &xform->auth;

	xform->type = RTE_CRYPTO_SYM_XFORM_AUTH;

	auth_xform->algo = RTE_CRYPTO_AUTH_AES_GMAC;
	auth_xform->op = (info.op == FIPS_TEST_ENC_AUTH_GEN) ?
			RTE_CRYPTO_AUTH_OP_GENERATE :
			RTE_CRYPTO_AUTH_OP_VERIFY;
	auth_xform->iv.offset = IV_OFF;
	auth_xform->iv.length = vec.iv.len;
	auth_xform->digest_length = vec.aead.digest.len;
	auth_xform->key.data = vec.aead.key.val;
	auth_xform->key.length = vec.aead.key.len;

	cap_idx.algo.auth = auth_xform->algo;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;

	cap = rte_cryptodev_sym_capability_get(env.dev_id, &cap_idx);
	if (!cap) {
		RTE_LOG(ERR, USER1, "Failed to get capability for cdev %u\n",
				env.dev_id);
		return -EINVAL;
	}

	if (rte_cryptodev_sym_capability_check_auth(cap,
			auth_xform->key.length,
			auth_xform->digest_length,
			auth_xform->iv.length) != 0) {

		RTE_LOG(ERR, USER1,
			"PMD %s key length %u Digest length %u IV length %u\n",
				info.device_name, auth_xform->key.length,
				auth_xform->digest_length,
				auth_xform->iv.length);
		return -EPERM;
	}

	return 0;
}

static int
prepare_cmac_xform(struct rte_crypto_sym_xform *xform)
{
	const struct rte_cryptodev_symmetric_capability *cap;
	struct rte_cryptodev_sym_capability_idx cap_idx;
	struct rte_crypto_auth_xform *auth_xform = &xform->auth;

	xform->type = RTE_CRYPTO_SYM_XFORM_AUTH;

	auth_xform->algo = RTE_CRYPTO_AUTH_AES_CMAC;
	auth_xform->op = (info.op == FIPS_TEST_ENC_AUTH_GEN) ?
			RTE_CRYPTO_AUTH_OP_GENERATE : RTE_CRYPTO_AUTH_OP_VERIFY;
	auth_xform->digest_length = vec.cipher_auth.digest.len;
	auth_xform->key.data = vec.cipher_auth.key.val;
	auth_xform->key.length = vec.cipher_auth.key.len;

	cap_idx.algo.auth = auth_xform->algo;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;

	cap = rte_cryptodev_sym_capability_get(env.dev_id, &cap_idx);
	if (!cap) {
		RTE_LOG(ERR, USER1, "Failed to get capability for cdev %u\n",
				env.dev_id);
		return -EINVAL;
	}

	if (rte_cryptodev_sym_capability_check_auth(cap,
			auth_xform->key.length,
			auth_xform->digest_length, 0) != 0) {
		RTE_LOG(ERR, USER1, "PMD %s key length %u Digest length %u\n",
				info.device_name, auth_xform->key.length,
				auth_xform->digest_length);
		return -EPERM;
	}

	return 0;
}

static int
prepare_ccm_xform(struct rte_crypto_sym_xform *xform)
{
	const struct rte_cryptodev_symmetric_capability *cap;
	struct rte_cryptodev_sym_capability_idx cap_idx;
	struct rte_crypto_aead_xform *aead_xform = &xform->aead;

	xform->type = RTE_CRYPTO_SYM_XFORM_AEAD;

	aead_xform->algo = RTE_CRYPTO_AEAD_AES_CCM;
	aead_xform->aad_length = vec.aead.aad.len;
	aead_xform->digest_length = vec.aead.digest.len;
	aead_xform->iv.offset = IV_OFF;
	aead_xform->iv.length = vec.iv.len;
	aead_xform->key.data = vec.aead.key.val;
	aead_xform->key.length = vec.aead.key.len;
	aead_xform->op = (info.op == FIPS_TEST_ENC_AUTH_GEN) ?
			RTE_CRYPTO_AEAD_OP_ENCRYPT :
			RTE_CRYPTO_AEAD_OP_DECRYPT;

	cap_idx.algo.aead = aead_xform->algo;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AEAD;

	cap = rte_cryptodev_sym_capability_get(env.dev_id, &cap_idx);
	if (!cap) {
		RTE_LOG(ERR, USER1, "Failed to get capability for cdev %u\n",
				env.dev_id);
		return -EINVAL;
	}

	if (rte_cryptodev_sym_capability_check_aead(cap,
			aead_xform->key.length,
			aead_xform->digest_length, aead_xform->aad_length,
			aead_xform->iv.length) != 0) {
		RTE_LOG(ERR, USER1,
			"PMD %s key_len %u tag_len %u aad_len %u iv_len %u\n",
				info.device_name, aead_xform->key.length,
				aead_xform->digest_length,
				aead_xform->aad_length,
				aead_xform->iv.length);
		return -EPERM;
	}

	return 0;
}

static int
prepare_sha_xform(struct rte_crypto_sym_xform *xform)
{
	const struct rte_cryptodev_symmetric_capability *cap;
	struct rte_cryptodev_sym_capability_idx cap_idx;
	struct rte_crypto_auth_xform *auth_xform = &xform->auth;

	xform->type = RTE_CRYPTO_SYM_XFORM_AUTH;

	auth_xform->algo = info.interim_info.sha_data.algo;
	auth_xform->op = RTE_CRYPTO_AUTH_OP_GENERATE;
	auth_xform->digest_length = vec.cipher_auth.digest.len;

	cap_idx.algo.auth = auth_xform->algo;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;

	cap = rte_cryptodev_sym_capability_get(env.dev_id, &cap_idx);
	if (!cap) {
		RTE_LOG(ERR, USER1, "Failed to get capability for cdev %u\n",
				env.dev_id);
		return -EINVAL;
	}

	if (rte_cryptodev_sym_capability_check_auth(cap,
			auth_xform->key.length,
			auth_xform->digest_length, 0) != 0) {
		RTE_LOG(ERR, USER1, "PMD %s key length %u digest length %u\n",
				info.device_name, auth_xform->key.length,
				auth_xform->digest_length);
		return -EPERM;
	}

	return 0;
}

static int
prepare_xts_xform(struct rte_crypto_sym_xform *xform)
{
	const struct rte_cryptodev_symmetric_capability *cap;
	struct rte_cryptodev_sym_capability_idx cap_idx;
	struct rte_crypto_cipher_xform *cipher_xform = &xform->cipher;

	xform->type = RTE_CRYPTO_SYM_XFORM_CIPHER;

	cipher_xform->algo = RTE_CRYPTO_CIPHER_AES_XTS;
	cipher_xform->op = (info.op == FIPS_TEST_ENC_AUTH_GEN) ?
			RTE_CRYPTO_CIPHER_OP_ENCRYPT :
			RTE_CRYPTO_CIPHER_OP_DECRYPT;
	cipher_xform->key.data = vec.cipher_auth.key.val;
	cipher_xform->key.length = vec.cipher_auth.key.len;
	cipher_xform->iv.length = vec.iv.len;
	cipher_xform->iv.offset = IV_OFF;

	cap_idx.algo.cipher = RTE_CRYPTO_CIPHER_AES_XTS;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;

	cap = rte_cryptodev_sym_capability_get(env.dev_id, &cap_idx);
	if (!cap) {
		RTE_LOG(ERR, USER1, "Failed to get capability for cdev %u\n",
				env.dev_id);
		return -EINVAL;
	}

	if (rte_cryptodev_sym_capability_check_cipher(cap,
			cipher_xform->key.length,
			cipher_xform->iv.length) != 0) {
		RTE_LOG(ERR, USER1, "PMD %s key length %u IV length %u\n",
				info.device_name, cipher_xform->key.length,
				cipher_xform->iv.length);
		return -EPERM;
	}

	return 0;
}

static int
prepare_rsa_xform(struct rte_crypto_asym_xform *xform)
{
	const struct rte_cryptodev_asymmetric_xform_capability *cap;
	struct rte_cryptodev_asym_capability_idx cap_idx;
	struct rte_cryptodev_info dev_info;

	xform->xform_type = RTE_CRYPTO_ASYM_XFORM_RSA;
	xform->next = NULL;

	cap_idx.type = xform->xform_type;
	cap = rte_cryptodev_asym_capability_get(env.dev_id, &cap_idx);
	if (!cap) {
		RTE_LOG(ERR, USER1, "Failed to get capability for cdev %u\n",
				env.dev_id);
		return -EINVAL;
	}

	switch (info.op) {
	case FIPS_TEST_ASYM_SIGGEN:
		if (!rte_cryptodev_asym_xform_capability_check_optype(cap,
			RTE_CRYPTO_ASYM_OP_SIGN)) {
			RTE_LOG(ERR, USER1, "PMD %s xform_op %u\n",
				info.device_name, RTE_CRYPTO_ASYM_OP_SIGN);
			return -EPERM;
		}
		break;
	case FIPS_TEST_ASYM_SIGVER:
		if (!rte_cryptodev_asym_xform_capability_check_optype(cap,
			RTE_CRYPTO_ASYM_OP_VERIFY)) {
			RTE_LOG(ERR, USER1, "PMD %s xform_op %u\n",
				info.device_name, RTE_CRYPTO_ASYM_OP_VERIFY);
			return -EPERM;
		}
		break;
	case FIPS_TEST_ASYM_KEYGEN:
		break;
	default:
		break;
	}

	rte_cryptodev_info_get(env.dev_id, &dev_info);
	xform->rsa.key_type = info.interim_info.rsa_data.privkey;
	switch (xform->rsa.key_type) {
	case RTE_RSA_KEY_TYPE_QT:
		if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_RSA_PRIV_OP_KEY_QT)) {
			RTE_LOG(ERR, USER1, "PMD %s does not support QT key type\n",
				info.device_name);
			return -EPERM;
		}
		xform->rsa.qt.p.data = vec.rsa.p.val;
		xform->rsa.qt.p.length = vec.rsa.p.len;
		xform->rsa.qt.q.data = vec.rsa.q.val;
		xform->rsa.qt.q.length = vec.rsa.q.len;
		xform->rsa.qt.dP.data = vec.rsa.dp.val;
		xform->rsa.qt.dP.length = vec.rsa.dp.len;
		xform->rsa.qt.dQ.data = vec.rsa.dq.val;
		xform->rsa.qt.dQ.length = vec.rsa.dq.len;
		xform->rsa.qt.qInv.data = vec.rsa.qinv.val;
		xform->rsa.qt.qInv.length = vec.rsa.qinv.len;
		break;
	case RTE_RSA_KEY_TYPE_EXP:
		if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_RSA_PRIV_OP_KEY_EXP)) {
			RTE_LOG(ERR, USER1, "PMD %s does not support EXP key type\n",
				info.device_name);
			return -EPERM;
		}
		xform->rsa.d.data = vec.rsa.d.val;
		xform->rsa.d.length = vec.rsa.d.len;
		break;
	default:
		break;
	}

	xform->rsa.e.data = vec.rsa.e.val;
	xform->rsa.e.length = vec.rsa.e.len;
	xform->rsa.n.data = vec.rsa.n.val;
	xform->rsa.n.length = vec.rsa.n.len;
	return 0;
}

static int
prepare_ecdsa_xform(struct rte_crypto_asym_xform *xform)
{
	const struct rte_cryptodev_asymmetric_xform_capability *cap;
	struct rte_cryptodev_asym_capability_idx cap_idx;

	xform->xform_type = RTE_CRYPTO_ASYM_XFORM_ECDSA;
	xform->next = NULL;

	cap_idx.type = xform->xform_type;
	cap = rte_cryptodev_asym_capability_get(env.dev_id, &cap_idx);
	if (!cap) {
		RTE_LOG(ERR, USER1, "Failed to get capability for cdev %u\n",
				env.dev_id);
		return -EINVAL;
	}

	switch (info.op) {
	case FIPS_TEST_ASYM_SIGGEN:
		if (!rte_cryptodev_asym_xform_capability_check_optype(cap,
			RTE_CRYPTO_ASYM_OP_SIGN)) {
			RTE_LOG(ERR, USER1, "PMD %s xform_op %u\n",
				info.device_name, RTE_CRYPTO_ASYM_OP_SIGN);
			return -EPERM;
		}

		xform->ec.pkey.data = vec.ecdsa.pkey.val;
		xform->ec.pkey.length = vec.ecdsa.pkey.len;
		break;
	case FIPS_TEST_ASYM_SIGVER:
		if (!rte_cryptodev_asym_xform_capability_check_optype(cap,
			RTE_CRYPTO_ASYM_OP_VERIFY)) {
			RTE_LOG(ERR, USER1, "PMD %s xform_op %u\n",
				info.device_name, RTE_CRYPTO_ASYM_OP_VERIFY);
			return -EPERM;
		}

		xform->ec.q.x.data = vec.ecdsa.qx.val;
		xform->ec.q.x.length = vec.ecdsa.qx.len;
		xform->ec.q.y.data = vec.ecdsa.qy.val;
		xform->ec.q.y.length = vec.ecdsa.qy.len;
		break;
	default:
		break;
	}

	xform->ec.curve_id = info.interim_info.ecdsa_data.curve_id;
	return 0;
}

static int
prepare_ecfpm_xform(struct rte_crypto_asym_xform *xform)
{
	const struct rte_cryptodev_asymmetric_xform_capability *cap;
	struct rte_cryptodev_asym_capability_idx cap_idx;

	xform->xform_type = RTE_CRYPTO_ASYM_XFORM_ECFPM;
	xform->next = NULL;

	cap_idx.type = xform->xform_type;
	cap = rte_cryptodev_asym_capability_get(env.dev_id, &cap_idx);
	if (!cap) {
		RTE_LOG(ERR, USER1, "Failed to get capability for cdev %u\n",
				env.dev_id);
		return -EINVAL;
	}

	xform->ec.curve_id = info.interim_info.ecdsa_data.curve_id;
	return 0;
}

static int
get_writeback_data(struct fips_val *val)
{
	struct rte_mbuf *m = env.mbuf;
	uint16_t data_len = rte_pktmbuf_pkt_len(m);
	uint16_t total_len = data_len + env.digest_len;
	uint8_t *src, *dst, *wb_data;

	/* in case val is reused for MCT test, try to free the buffer first */
	if (val->val) {
		rte_free(val->val);
		val->val = NULL;
	}

	wb_data = dst = rte_malloc(NULL, total_len, 0);
	if (!dst) {
		RTE_LOG(ERR, USER1, "Error %i: Not enough memory\n", -ENOMEM);
		return -ENOMEM;
	}

	while (m && data_len) {
		uint16_t seg_len = RTE_MIN(rte_pktmbuf_data_len(m), data_len);

		src = rte_pktmbuf_mtod(m, uint8_t *);
		memcpy(dst, src, seg_len);
		m = m->next;
		data_len -= seg_len;
		dst += seg_len;
	}

	if (data_len) {
		RTE_LOG(ERR, USER1, "Error -1: write back data\n");
		rte_free(wb_data);
		return -1;
	}

	if (env.digest)
		memcpy(dst, env.digest, env.digest_len);

	val->val = wb_data;
	val->len = total_len;

	return 0;
}

static int
fips_run_sym_test(void)
{
	struct rte_crypto_sym_xform xform = {0};
	uint16_t n_deqd;
	int ret;

	if (!test_ops.prepare_sym_xform || !test_ops.prepare_sym_op)
		return -EINVAL;

	ret = test_ops.prepare_sym_xform(&xform);
	if (ret < 0)
		return ret;

	env.sym.sess = rte_cryptodev_sym_session_create(env.dev_id, &xform,
						env.sym.sess_mpool);
	if (!env.sym.sess)
		return -ENOMEM;

	ret = test_ops.prepare_sym_op();
	if (ret < 0) {
		RTE_LOG(ERR, USER1, "Error %i: Prepare op\n",
				ret);
		goto exit;
	}

	if (rte_cryptodev_enqueue_burst(env.dev_id, 0, &env.op, 1) < 1) {
		RTE_LOG(ERR, USER1, "Error: Failed enqueue\n");
		ret = -1;
		goto exit;
	}

	do {
		struct rte_crypto_op *deqd_op;

		n_deqd = rte_cryptodev_dequeue_burst(env.dev_id, 0, &deqd_op, 1);
	} while (n_deqd == 0);

	vec.status = env.op->status;

exit:
	rte_cryptodev_sym_session_free(env.dev_id, env.sym.sess);
	env.sym.sess = NULL;
	return ret;
}

static int
fips_run_asym_test(void)
{
	struct rte_crypto_asym_xform xform = {0};
	struct rte_crypto_asym_op *asym;
	struct rte_crypto_op *deqd_op;
	int ret;

	if (info.op == FIPS_TEST_ASYM_KEYGEN && info.algo != FIPS_TEST_ALGO_ECDSA) {
		RTE_SET_USED(asym);
		ret = 0;
		goto exit;
	}

	if (!test_ops.prepare_asym_xform || !test_ops.prepare_asym_op)
		return -EINVAL;

	asym = env.op->asym;
	ret = test_ops.prepare_asym_xform(&xform);
	if (ret < 0)
		return ret;

	ret = rte_cryptodev_asym_session_create(env.dev_id, &xform, env.asym.sess_mpool,
			(void *)&env.asym.sess);
	if (ret < 0)
		return ret;

	ret = test_ops.prepare_asym_op();
	if (ret < 0) {
		RTE_LOG(ERR, USER1, "Error %i: Prepare op\n", ret);
		goto exit;
	}

	if (rte_cryptodev_enqueue_burst(env.dev_id, 0, &env.op, 1) < 1) {
		RTE_LOG(ERR, USER1, "Error: Failed enqueue\n");
		ret = -1;
		goto exit;
	}

	while (rte_cryptodev_dequeue_burst(env.dev_id, 0, &deqd_op, 1) == 0)
		rte_pause();

	vec.status = env.op->status;

 exit:
	if (env.asym.sess)
		rte_cryptodev_asym_session_free(env.dev_id, env.asym.sess);

	env.asym.sess = NULL;
	return ret;
}

static int
fips_run_test(void)
{
	int ret;

	env.op = env.sym.op;
	if (env.is_asym_test) {
		if (info.op == FIPS_TEST_ASYM_KEYGEN &&
			info.algo == FIPS_TEST_ALGO_ECDSA) {
			env.op = env.asym.op;
			test_ops.prepare_asym_xform = prepare_ecfpm_xform;
			test_ops.prepare_asym_op = prepare_ecfpm_op;
			ret = fips_run_asym_test();
			if (ret < 0)
				return ret;

			info.interim_info.ecdsa_data.pubkey_gen = 0;
			return ret;
		}

		vec.cipher_auth.digest.len = parse_test_sha_hash_size(
						info.interim_info.rsa_data.auth);
		test_ops.prepare_sym_xform = prepare_sha_xform;
		test_ops.prepare_sym_op = prepare_auth_op;
		ret = fips_run_sym_test();
		if (ret < 0)
			return ret;
	} else {
		return fips_run_sym_test();
	}

	env.op = env.asym.op;
	if (info.op == FIPS_TEST_ASYM_SIGGEN &&
		info.algo == FIPS_TEST_ALGO_ECDSA &&
		info.interim_info.ecdsa_data.pubkey_gen == 1) {
		fips_prepare_asym_xform_t ecdsa_xform;
		fips_prepare_op_t ecdsa_op;

		ecdsa_xform = test_ops.prepare_asym_xform;
		ecdsa_op = test_ops.prepare_asym_op;
		info.op = FIPS_TEST_ASYM_KEYGEN;
		test_ops.prepare_asym_xform = prepare_ecfpm_xform;
		test_ops.prepare_asym_op = prepare_ecfpm_op;
		ret = fips_run_asym_test();
		if (ret < 0)
			return ret;

		info.post_interim_writeback(NULL);
		info.interim_info.ecdsa_data.pubkey_gen = 0;

		test_ops.prepare_asym_xform = ecdsa_xform;
		test_ops.prepare_asym_op = ecdsa_op;
		info.op = FIPS_TEST_ASYM_SIGGEN;
		ret = fips_run_asym_test();
	} else {
		ret = fips_run_asym_test();
	}

	return ret;
}

static int
fips_generic_test(void)
{
	struct fips_val val = {NULL, 0};
	int ret;

	if (info.file_type != FIPS_TYPE_JSON)
		fips_test_write_one_case();

	ret = fips_run_test();
	if (ret < 0) {
		if (ret == -EPERM || ret == -ENOTSUP) {
			if (info.file_type == FIPS_TYPE_JSON)
				return ret;

			fprintf(info.fp_wr, "Bypass\n\n");
			return 0;
		}

		return ret;
	}

	if (!env.is_asym_test) {
		ret = get_writeback_data(&val);
		if (ret < 0)
			return ret;
	}

	switch (info.file_type) {
	case FIPS_TYPE_REQ:
	case FIPS_TYPE_RSP:
	case FIPS_TYPE_JSON:
		if (info.parse_writeback == NULL)
			return -EPERM;
		ret = info.parse_writeback(&val);
		if (ret < 0)
			return ret;
		break;
	case FIPS_TYPE_FAX:
		if (info.kat_check == NULL)
			return -EPERM;
		ret = info.kat_check(&val);
		if (ret < 0)
			return ret;
		break;
	default:
		break;
	}

	if (info.file_type != FIPS_TYPE_JSON)
		fprintf(info.fp_wr, "\n");
	rte_free(val.val);

	return 0;
}

static int
fips_mct_tdes_test(void)
{
#define TDES_BLOCK_SIZE		8
#define TDES_EXTERN_ITER	400
#define TDES_INTERN_ITER	10000
	struct fips_val val[3] = {{NULL, 0},}, val_key, pt, ct, iv;
	uint8_t prev_out[TDES_BLOCK_SIZE] = {0};
	uint8_t prev_prev_out[TDES_BLOCK_SIZE] = {0};
	uint8_t prev_in[TDES_BLOCK_SIZE] = {0};
	uint32_t i, j, k;
	int ret;
	int test_mode = info.interim_info.tdes_data.test_mode;

	pt.len = vec.pt.len;
	pt.val = rte_malloc(NULL, pt.len, 0);
	ct.len = vec.ct.len;
	ct.val = rte_malloc(NULL, ct.len, 0);
	iv.len = vec.iv.len;
	iv.val = rte_malloc(NULL, iv.len, 0);

	for (i = 0; i < TDES_EXTERN_ITER; i++) {
		if (info.file_type != FIPS_TYPE_JSON) {
			if ((i == 0) && (info.version == 21.4f)) {
				if (!(strstr(info.vec[0], "COUNT")))
					fprintf(info.fp_wr, "%s%u\n", "COUNT = ", 0);
			}

			if (i != 0)
				update_info_vec(i);

			fips_test_write_one_case();
		}

		for (j = 0; j < TDES_INTERN_ITER; j++) {
			ret = fips_run_test();
			if (ret < 0) {
				if (ret == -EPERM) {
					if (info.file_type == FIPS_TYPE_JSON)
						return ret;

					fprintf(info.fp_wr, "Bypass\n");
					return 0;
				}
				return ret;
			}

			ret = get_writeback_data(&val[0]);
			if (ret < 0)
				return ret;

			if (info.op == FIPS_TEST_DEC_AUTH_VERIF)
				memcpy(prev_in, vec.ct.val, TDES_BLOCK_SIZE);

			if (j == 0) {
				memcpy(prev_out, val[0].val, TDES_BLOCK_SIZE);
				memcpy(pt.val, vec.pt.val, pt.len);
				memcpy(ct.val, vec.ct.val, ct.len);
				memcpy(iv.val, vec.iv.val, iv.len);

				if (info.op == FIPS_TEST_ENC_AUTH_GEN) {
					if (test_mode == TDES_MODE_ECB) {
						memcpy(vec.pt.val, val[0].val,
							   TDES_BLOCK_SIZE);
					} else {
						memcpy(vec.pt.val, vec.iv.val,
							   TDES_BLOCK_SIZE);
						memcpy(vec.iv.val, val[0].val,
							   TDES_BLOCK_SIZE);
					}
					val[1].val = pt.val;
					val[1].len = pt.len;
					val[2].val = iv.val;
					val[2].len = iv.len;
				} else {
					if (test_mode == TDES_MODE_ECB) {
						memcpy(vec.ct.val, val[0].val,
							   TDES_BLOCK_SIZE);
					} else {
						memcpy(vec.iv.val, vec.ct.val,
							   TDES_BLOCK_SIZE);
						memcpy(vec.ct.val, val[0].val,
							   TDES_BLOCK_SIZE);
					}
					val[1].val = ct.val;
					val[1].len = ct.len;
					val[2].val = iv.val;
					val[2].len = iv.len;
				}
				continue;
			}

			if (info.op == FIPS_TEST_ENC_AUTH_GEN) {
				if (test_mode == TDES_MODE_ECB) {
					memcpy(vec.pt.val, val[0].val,
						   TDES_BLOCK_SIZE);
				} else {
					memcpy(vec.iv.val, val[0].val,
						   TDES_BLOCK_SIZE);
					memcpy(vec.pt.val, prev_out,
						   TDES_BLOCK_SIZE);
				}
			} else {
				if (test_mode == TDES_MODE_ECB) {
					memcpy(vec.ct.val, val[0].val,
						   TDES_BLOCK_SIZE);
				} else {
					memcpy(vec.iv.val, vec.ct.val,
						   TDES_BLOCK_SIZE);
					memcpy(vec.ct.val, val[0].val,
						   TDES_BLOCK_SIZE);
				}
			}

			if (j == TDES_INTERN_ITER - 1)
				continue;

			memcpy(prev_out, val[0].val, TDES_BLOCK_SIZE);

			if (j == TDES_INTERN_ITER - 3)
				memcpy(prev_prev_out, val[0].val, TDES_BLOCK_SIZE);
		}

		info.parse_writeback(val);
		if (info.file_type != FIPS_TYPE_JSON)
			fprintf(info.fp_wr, "\n");

		if (i == TDES_EXTERN_ITER - 1)
			continue;

		/** update key */
		memcpy(&val_key, &vec.cipher_auth.key, sizeof(val_key));

		if (info.interim_info.tdes_data.nb_keys == 0) {
			if (memcmp(val_key.val, val_key.val + 8, 8) == 0)
				info.interim_info.tdes_data.nb_keys = 1;
			else if (memcmp(val_key.val, val_key.val + 16, 8) == 0)
				info.interim_info.tdes_data.nb_keys = 2;
			else
				info.interim_info.tdes_data.nb_keys = 3;

		}

		for (k = 0; k < TDES_BLOCK_SIZE; k++) {

			switch (info.interim_info.tdes_data.nb_keys) {
			case 3:
				val_key.val[k] ^= val[0].val[k];
				val_key.val[k + 8] ^= prev_out[k];
				val_key.val[k + 16] ^= prev_prev_out[k];
				break;
			case 2:
				val_key.val[k] ^= val[0].val[k];
				val_key.val[k + 8] ^= prev_out[k];
				val_key.val[k + 16] ^= val[0].val[k];
				break;
			default: /* case 1 */
				val_key.val[k] ^= val[0].val[k];
				val_key.val[k + 8] ^= val[0].val[k];
				val_key.val[k + 16] ^= val[0].val[k];
				break;
			}

		}

		for (k = 0; k < 24; k++)
			val_key.val[k] = (rte_popcount32(val_key.val[k]) &
					0x1) ?
					val_key.val[k] : (val_key.val[k] ^ 0x1);

		if (info.op == FIPS_TEST_ENC_AUTH_GEN) {
			if (test_mode == TDES_MODE_ECB) {
				memcpy(vec.pt.val, val[0].val, TDES_BLOCK_SIZE);
			} else {
				memcpy(vec.iv.val, val[0].val, TDES_BLOCK_SIZE);
				memcpy(vec.pt.val, prev_out, TDES_BLOCK_SIZE);
			}
		} else {
			if (test_mode == TDES_MODE_ECB) {
				memcpy(vec.ct.val, val[0].val, TDES_BLOCK_SIZE);
			} else {
				memcpy(vec.iv.val, prev_out, TDES_BLOCK_SIZE);
				memcpy(vec.ct.val, val[0].val, TDES_BLOCK_SIZE);
			}
		}
	}

	rte_free(val[0].val);
	rte_free(pt.val);
	rte_free(ct.val);
	rte_free(iv.val);

	return 0;
}

static int
fips_mct_aes_ecb_test(void)
{
#define AES_BLOCK_SIZE	16
#define AES_EXTERN_ITER	100
#define AES_INTERN_ITER	1000
	struct fips_val val = {NULL, 0}, val_key;
	uint8_t prev_out[AES_BLOCK_SIZE] = {0};
	uint32_t i, j, k;
	int ret;

	for (i = 0; i < AES_EXTERN_ITER; i++) {
		if (i != 0)
			update_info_vec(i);

		fips_test_write_one_case();

		for (j = 0; j < AES_INTERN_ITER; j++) {
			ret = fips_run_test();
			if (ret < 0) {
				if (ret == -EPERM) {
					if (info.file_type == FIPS_TYPE_JSON)
						return ret;

					fprintf(info.fp_wr, "Bypass\n");
					return 0;
				}

				return ret;
			}

			ret = get_writeback_data(&val);
			if (ret < 0)
				return ret;

			if (info.op == FIPS_TEST_ENC_AUTH_GEN)
				memcpy(vec.pt.val, val.val, AES_BLOCK_SIZE);
			else
				memcpy(vec.ct.val, val.val, AES_BLOCK_SIZE);

			if (j == AES_INTERN_ITER - 1)
				continue;

			memcpy(prev_out, val.val, AES_BLOCK_SIZE);
		}

		info.parse_writeback(&val);
		fprintf(info.fp_wr, "\n");

		if (i == AES_EXTERN_ITER - 1)
			continue;

		/** update key */
		memcpy(&val_key, &vec.cipher_auth.key, sizeof(val_key));
		for (k = 0; k < vec.cipher_auth.key.len; k++) {
			switch (vec.cipher_auth.key.len) {
			case 16:
				val_key.val[k] ^= val.val[k];
				break;
			case 24:
				if (k < 8)
					val_key.val[k] ^= prev_out[k + 8];
				else
					val_key.val[k] ^= val.val[k - 8];
				break;
			case 32:
				if (k < 16)
					val_key.val[k] ^= prev_out[k];
				else
					val_key.val[k] ^= val.val[k - 16];
				break;
			default:
				return -1;
			}
		}
	}

	rte_free(val.val);

	return 0;
}
static int
fips_mct_aes_test(void)
{
#define AES_BLOCK_SIZE	16
#define AES_EXTERN_ITER	100
#define AES_INTERN_ITER	1000
	struct fips_val val[3] = {{NULL, 0},}, val_key,  pt, ct, iv;
	uint8_t prev_out[AES_BLOCK_SIZE] = {0};
	uint8_t prev_in[AES_BLOCK_SIZE] = {0};
	uint32_t i, j, k;
	int ret;

	if (info.interim_info.aes_data.cipher_algo == RTE_CRYPTO_CIPHER_AES_ECB)
		return fips_mct_aes_ecb_test();

	pt.len = vec.pt.len;
	pt.val = rte_malloc(NULL, pt.len, 0);
	ct.len = vec.ct.len;
	ct.val = rte_malloc(NULL, ct.len, 0);
	iv.len = vec.iv.len;
	iv.val = rte_malloc(NULL, iv.len, 0);
	for (i = 0; i < AES_EXTERN_ITER; i++) {
		if (info.file_type != FIPS_TYPE_JSON) {
			if (i != 0)
				update_info_vec(i);

			fips_test_write_one_case();
		}

		for (j = 0; j < AES_INTERN_ITER; j++) {
			ret = fips_run_test();
			if (ret < 0) {
				if (ret == -EPERM) {
					if (info.file_type == FIPS_TYPE_JSON)
						return ret;

					fprintf(info.fp_wr, "Bypass\n");
					return 0;
				}

				return ret;
			}

			ret = get_writeback_data(&val[0]);
			if (ret < 0)
				return ret;

			if (info.op == FIPS_TEST_DEC_AUTH_VERIF)
				memcpy(prev_in, vec.ct.val, AES_BLOCK_SIZE);

			if (j == 0) {
				memcpy(prev_out, val[0].val, AES_BLOCK_SIZE);
				memcpy(pt.val, vec.pt.val, pt.len);
				memcpy(ct.val, vec.ct.val, ct.len);
				memcpy(iv.val, vec.iv.val, iv.len);

				if (info.op == FIPS_TEST_ENC_AUTH_GEN) {
					memcpy(vec.pt.val, vec.iv.val, AES_BLOCK_SIZE);
					memcpy(vec.iv.val, val[0].val, AES_BLOCK_SIZE);
					val[1].val = pt.val;
					val[1].len = pt.len;
					val[2].val = iv.val;
					val[2].len = iv.len;
				} else {
					memcpy(vec.ct.val, vec.iv.val, AES_BLOCK_SIZE);
					memcpy(vec.iv.val, prev_in, AES_BLOCK_SIZE);
					val[1].val = ct.val;
					val[1].len = ct.len;
					val[2].val = iv.val;
					val[2].len = iv.len;
				}
				continue;
			}

			if (info.op == FIPS_TEST_ENC_AUTH_GEN) {
				memcpy(vec.iv.val, val[0].val, AES_BLOCK_SIZE);
				memcpy(vec.pt.val, prev_out, AES_BLOCK_SIZE);
			} else {
				memcpy(vec.iv.val, prev_in, AES_BLOCK_SIZE);
				memcpy(vec.ct.val, prev_out, AES_BLOCK_SIZE);
			}

			if (j == AES_INTERN_ITER - 1)
				continue;

			memcpy(prev_out, val[0].val, AES_BLOCK_SIZE);
		}

		info.parse_writeback(val);
		if (info.file_type != FIPS_TYPE_JSON)
			fprintf(info.fp_wr, "\n");

		if (i == AES_EXTERN_ITER - 1)
			continue;

		/** update key */
		memcpy(&val_key, &vec.cipher_auth.key, sizeof(val_key));
		for (k = 0; k < vec.cipher_auth.key.len; k++) {
			switch (vec.cipher_auth.key.len) {
			case 16:
				val_key.val[k] ^= val[0].val[k];
				break;
			case 24:
				if (k < 8)
					val_key.val[k] ^= prev_out[k + 8];
				else
					val_key.val[k] ^= val[0].val[k - 8];
				break;
			case 32:
				if (k < 16)
					val_key.val[k] ^= prev_out[k];
				else
					val_key.val[k] ^= val[0].val[k - 16];
				break;
			default:
				return -1;
			}
		}

		if (info.op == FIPS_TEST_DEC_AUTH_VERIF)
			memcpy(vec.iv.val, val[0].val, AES_BLOCK_SIZE);
	}

	rte_free(val[0].val);
	rte_free(pt.val);
	rte_free(ct.val);
	rte_free(iv.val);

	return 0;
}

static int
fips_mct_sha_test(void)
{
#define SHA_EXTERN_ITER	100
#define SHA_INTERN_ITER	1000
	uint8_t md_blocks = info.interim_info.sha_data.md_blocks;
	struct fips_val val = {NULL, 0};
	struct fips_val  md[md_blocks];
	int ret;
	uint32_t i, j, k, offset, max_outlen;

	max_outlen = md_blocks * vec.cipher_auth.digest.len;

	rte_free(vec.cipher_auth.digest.val);
	vec.cipher_auth.digest.val = rte_malloc(NULL, max_outlen, 0);

	if (vec.pt.val)
		memcpy(vec.cipher_auth.digest.val, vec.pt.val, vec.cipher_auth.digest.len);

	rte_free(vec.pt.val);
	vec.pt.val = rte_malloc(NULL, (MAX_DIGEST_SIZE*md_blocks), 0);

	for (i = 0; i < md_blocks; i++)
		md[i].val = rte_malloc(NULL, (MAX_DIGEST_SIZE*2), 0);

	if (info.file_type != FIPS_TYPE_JSON) {
		fips_test_write_one_case();
		fprintf(info.fp_wr, "\n");
	}

	for (j = 0; j < SHA_EXTERN_ITER; j++) {
		for (i = 0; i < md_blocks; i++) {
			memcpy(md[i].val, vec.cipher_auth.digest.val,
				vec.cipher_auth.digest.len);
			md[i].len = vec.cipher_auth.digest.len;
		}

		for (i = 0; i < (SHA_INTERN_ITER); i++) {
			offset = 0;
			for (k = 0; k < md_blocks; k++) {
				memcpy(vec.pt.val + offset, md[k].val, (size_t)md[k].len);
				offset += md[k].len;
			}
			vec.pt.len = offset;

			ret = fips_run_test();
			if (ret < 0) {
				if (ret == -EPERM || ret == -ENOTSUP) {
					if (info.file_type == FIPS_TYPE_JSON)
						return ret;

					fprintf(info.fp_wr, "Bypass\n\n");
					return 0;
				}
				return ret;
			}

			ret = get_writeback_data(&val);
			if (ret < 0)
				return ret;

			for (k = 1; k < md_blocks; k++) {
				memcpy(md[k-1].val, md[k].val, md[k].len);
				md[k-1].len = md[k].len;
			}

			memcpy(md[md_blocks-1].val, (val.val + vec.pt.len),
				vec.cipher_auth.digest.len);
			md[md_blocks-1].len = vec.cipher_auth.digest.len;
		}

		memcpy(vec.cipher_auth.digest.val, md[md_blocks-1].val, md[md_blocks-1].len);
		vec.cipher_auth.digest.len = md[md_blocks-1].len;

		if (info.file_type != FIPS_TYPE_JSON)
			fprintf(info.fp_wr, "COUNT = %u\n", j);

		info.parse_writeback(&val);

		if (info.file_type != FIPS_TYPE_JSON)
			fprintf(info.fp_wr, "\n");
	}

	for (i = 0; i < (md_blocks); i++)
		rte_free(md[i].val);

	rte_free(vec.pt.val);

	rte_free(val.val);
	return 0;
}

static int
fips_mct_shake_test(void)
{
#define SHAKE_EXTERN_ITER	100
#define SHAKE_INTERN_ITER	1000
	uint32_t i, j, range, outlen, max_outlen;
	struct fips_val val = {NULL, 0}, md;
	uint8_t rightmost[2];
	uint16_t *rightptr;
	int ret;

	max_outlen = vec.cipher_auth.digest.len;

	rte_free(vec.cipher_auth.digest.val);
	vec.cipher_auth.digest.val = rte_malloc(NULL, max_outlen, 0);

	if (vec.pt.val)
		memcpy(vec.cipher_auth.digest.val, vec.pt.val, vec.pt.len);

	rte_free(vec.pt.val);
	vec.pt.val = rte_malloc(NULL, 16, 0);
	vec.pt.len = 16;

	md.val = rte_malloc(NULL, max_outlen, 0);
	md.len = max_outlen;

	if (info.file_type != FIPS_TYPE_JSON) {
		fips_test_write_one_case();
		fprintf(info.fp_wr, "\n");
	}

	range = max_outlen - info.interim_info.sha_data.min_outlen + 1;
	outlen = max_outlen;
	for (j = 0; j < SHAKE_EXTERN_ITER; j++) {
		memset(md.val, 0, max_outlen);
		memcpy(md.val, vec.cipher_auth.digest.val,
			vec.cipher_auth.digest.len);

		for (i = 0; i < (SHAKE_INTERN_ITER); i++) {
			memset(vec.pt.val, 0, vec.pt.len);
			memcpy(vec.pt.val, md.val, vec.pt.len);
			vec.cipher_auth.digest.len = outlen;
			ret = fips_run_test();
			if (ret < 0) {
				if (ret == -EPERM || ret == -ENOTSUP) {
					if (info.file_type == FIPS_TYPE_JSON)
						return ret;

					fprintf(info.fp_wr, "Bypass\n\n");
					return 0;
				}
				return ret;
			}

			ret = get_writeback_data(&val);
			if (ret < 0)
				return ret;

			memset(md.val, 0, max_outlen);
			memcpy(md.val, (val.val + vec.pt.len),
				vec.cipher_auth.digest.len);
			md.len = outlen;
			rightmost[0] = md.val[md.len-1];
			rightmost[1] = md.val[md.len-2];
			rightptr = (uint16_t *)rightmost;
			outlen = info.interim_info.sha_data.min_outlen +
				(*rightptr % range);
		}

		memcpy(vec.cipher_auth.digest.val, md.val, md.len);
		vec.cipher_auth.digest.len = md.len;

		if (info.file_type != FIPS_TYPE_JSON)
			fprintf(info.fp_wr, "COUNT = %u\n", j);

		info.parse_writeback(&val);

		if (info.file_type != FIPS_TYPE_JSON)
			fprintf(info.fp_wr, "\n");
	}

	rte_free(md.val);
	rte_free(vec.pt.val);
	rte_free(val.val);
	return 0;
}

static int
init_test_ops(void)
{
	switch (info.algo) {
	case FIPS_TEST_ALGO_AES_CBC:
	case FIPS_TEST_ALGO_AES_CTR:
	case FIPS_TEST_ALGO_AES:
		test_ops.prepare_sym_op = prepare_cipher_op;
		test_ops.prepare_sym_xform  = prepare_aes_xform;
		if (info.interim_info.aes_data.test_type == AESAVS_TYPE_MCT)
			test_ops.test = fips_mct_aes_test;
		else
			test_ops.test = fips_generic_test;
		break;
	case FIPS_TEST_ALGO_HMAC:
		test_ops.prepare_sym_op = prepare_auth_op;
		test_ops.prepare_sym_xform = prepare_hmac_xform;
		test_ops.test = fips_generic_test;
		break;
	case FIPS_TEST_ALGO_TDES:
		test_ops.prepare_sym_op = prepare_cipher_op;
		test_ops.prepare_sym_xform = prepare_tdes_xform;
		if (info.interim_info.tdes_data.test_type == TDES_MCT)
			test_ops.test = fips_mct_tdes_test;
		else
			test_ops.test = fips_generic_test;
		break;
	case FIPS_TEST_ALGO_AES_GMAC:
		test_ops.prepare_sym_op = prepare_auth_op;
		test_ops.prepare_sym_xform = prepare_gmac_xform;
		test_ops.test = fips_generic_test;
		break;
	case FIPS_TEST_ALGO_AES_GCM:
		test_ops.prepare_sym_op = prepare_aead_op;
		test_ops.prepare_sym_xform = prepare_gcm_xform;
		test_ops.test = fips_generic_test;
		break;
	case FIPS_TEST_ALGO_AES_CMAC:
		test_ops.prepare_sym_op = prepare_auth_op;
		test_ops.prepare_sym_xform = prepare_cmac_xform;
		test_ops.test = fips_generic_test;
		break;
	case FIPS_TEST_ALGO_AES_CCM:
		test_ops.prepare_sym_op = prepare_aead_op;
		test_ops.prepare_sym_xform = prepare_ccm_xform;
		test_ops.test = fips_generic_test;
		break;
	case FIPS_TEST_ALGO_SHA:
		test_ops.prepare_sym_op = prepare_auth_op;
		test_ops.prepare_sym_xform = prepare_sha_xform;
		if (info.interim_info.sha_data.test_type == SHA_MCT)
			if (info.interim_info.sha_data.algo == RTE_CRYPTO_AUTH_SHAKE_128 ||
				info.interim_info.sha_data.algo == RTE_CRYPTO_AUTH_SHAKE_256)
				test_ops.test = fips_mct_shake_test;
			else
				test_ops.test = fips_mct_sha_test;
		else
			test_ops.test = fips_generic_test;
		break;
	case FIPS_TEST_ALGO_AES_XTS:
		test_ops.prepare_sym_op = prepare_cipher_op;
		test_ops.prepare_sym_xform = prepare_xts_xform;
		test_ops.test = fips_generic_test;
		break;
	case FIPS_TEST_ALGO_RSA:
		test_ops.prepare_asym_op = prepare_rsa_op;
		test_ops.prepare_asym_xform = prepare_rsa_xform;
		test_ops.test = fips_generic_test;
		break;
	case FIPS_TEST_ALGO_ECDSA:
		if (info.op == FIPS_TEST_ASYM_KEYGEN) {
			test_ops.prepare_asym_op = prepare_ecfpm_op;
			test_ops.prepare_asym_xform = prepare_ecfpm_xform;
			test_ops.test = fips_generic_test;
		} else {
			test_ops.prepare_asym_op = prepare_ecdsa_op;
			test_ops.prepare_asym_xform = prepare_ecdsa_xform;
			test_ops.test = fips_generic_test;
		}
		break;
	default:
		if (strstr(info.file_name, "TECB") ||
				strstr(info.file_name, "TCBC")) {
			info.algo = FIPS_TEST_ALGO_TDES;
			test_ops.prepare_sym_op = prepare_cipher_op;
			test_ops.prepare_sym_xform = prepare_tdes_xform;
			if (info.interim_info.tdes_data.test_type == TDES_MCT)
				test_ops.test = fips_mct_tdes_test;
			else
				test_ops.test = fips_generic_test;
			break;
		}
		return -1;
	}

	return 0;
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

	ret = init_test_ops();
	if (ret < 0) {
		RTE_LOG(ERR, USER1, "Error %i: Init test op\n", ret);
		return ret;
	}

	while (ret >= 0 && fetch_ret == 0) {
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
			ret = test_ops.test();
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

	if (env.digest) {
		rte_free(env.digest);
		env.digest = NULL;
		env.digest_len = 0;
	}
	rte_pktmbuf_free(env.mbuf);

	return ret;
}

#ifdef USE_JANSSON
static int
fips_test_json_init_writeback(void)
{
	json_t *session_info, *session_write;
	session_info = json_array_get(json_info.json_root, 0);
	session_write = json_object();
	json_info.json_write_root = json_array();

	json_object_set(session_write, "jwt",
		json_object_get(session_info, "jwt"));
	json_object_set(session_write, "url",
		json_object_get(session_info, "url"));
	json_object_set(session_write, "isSample",
		json_object_get(session_info, "isSample"));

	json_info.is_sample = json_boolean_value(
		json_object_get(session_info, "isSample"));

	json_array_append_new(json_info.json_write_root, session_write);
	return 0;
}

static int
fips_test_one_test_case(void)
{
	int ret;

	ret = fips_test_parse_one_json_case();

	switch (ret) {
	case 0:
		ret = test_ops.test();
		if ((ret == 0) || (ret == -EPERM || ret == -ENOTSUP))
			break;
		RTE_LOG(ERR, USER1, "Error %i: test block\n",
				ret);
		break;
	default:
		RTE_LOG(ERR, USER1, "Error %i: Parse block\n",
				ret);
	}
	return ret;
}

static int
fips_test_one_test_group(void)
{
	int ret;
	json_t *tests, *write_tests;
	size_t test_idx, tests_size;

	write_tests = json_array();
	json_info.json_write_group = json_object();
	json_object_set(json_info.json_write_group, "tgId",
		json_object_get(json_info.json_test_group, "tgId"));
	json_object_set_new(json_info.json_write_group, "tests", write_tests);

	switch (info.algo) {
	case FIPS_TEST_ALGO_AES_GMAC:
	case FIPS_TEST_ALGO_AES_GCM:
		ret = parse_test_gcm_json_init();
		break;
	case FIPS_TEST_ALGO_AES_CCM:
		ret = parse_test_ccm_json_init();
		break;
	case FIPS_TEST_ALGO_HMAC:
		ret = parse_test_hmac_json_init();
		break;
	case FIPS_TEST_ALGO_AES_CMAC:
		ret = parse_test_cmac_json_init();
		break;
	case FIPS_TEST_ALGO_AES_XTS:
		ret = parse_test_xts_json_init();
		break;
	case FIPS_TEST_ALGO_AES_CBC:
	case FIPS_TEST_ALGO_AES_CTR:
	case FIPS_TEST_ALGO_AES:
		ret = parse_test_aes_json_init();
		break;
	case FIPS_TEST_ALGO_SHA:
		ret = parse_test_sha_json_init();
		break;
	case FIPS_TEST_ALGO_TDES:
		ret = parse_test_tdes_json_init();
		break;
	case FIPS_TEST_ALGO_RSA:
		ret = parse_test_rsa_json_init();
		break;
	case FIPS_TEST_ALGO_ECDSA:
		ret = parse_test_ecdsa_json_init();
		break;
	default:
		return -EINVAL;
	}

	if (ret < 0)
		return ret;

	ret = fips_test_parse_one_json_group();
	if (ret < 0)
		return ret;

	ret = init_test_ops();
	if (ret < 0)
		return ret;

	tests = json_object_get(json_info.json_test_group, "tests");
	tests_size = json_array_size(tests);
	for (test_idx = 0; test_idx < tests_size; test_idx++) {
		json_info.json_test_case = json_array_get(tests, test_idx);
		if (fips_test_one_test_case() == 0)
			json_array_append_new(write_tests, json_info.json_write_case);
	}

	return 0;
}

static int
fips_test_one_vector_set(void)
{
	int ret;
	json_t *test_groups, *write_groups, *write_version, *write_set, *mode;
	size_t group_idx, num_groups;

	test_groups = json_object_get(json_info.json_vector_set, "testGroups");
	num_groups = json_array_size(test_groups);

	json_info.json_write_set = json_array();
	write_version = json_object();
	json_object_set_new(write_version, "acvVersion", json_string(ACVVERSION));
	json_array_append_new(json_info.json_write_set, write_version);

	write_set = json_object();
	json_array_append(json_info.json_write_set, write_set);
	write_groups = json_array();

	json_object_set(write_set, "vsId",
		json_object_get(json_info.json_vector_set, "vsId"));
	json_object_set(write_set, "algorithm",
		json_object_get(json_info.json_vector_set, "algorithm"));
	mode = json_object_get(json_info.json_vector_set, "mode");
	if (mode != NULL)
		json_object_set_new(write_set, "mode", mode);

	json_object_set(write_set, "revision",
		json_object_get(json_info.json_vector_set, "revision"));
	json_object_set_new(write_set, "isSample",
		json_boolean(json_info.is_sample));
	json_object_set_new(write_set, "testGroups", write_groups);

	ret = fips_test_parse_one_json_vector_set();
	if (ret < 0) {
		RTE_LOG(ERR, USER1, "Error: Unsupported or invalid vector set algorithm: %s\n",
			json_string_value(json_object_get(json_info.json_vector_set, "algorithm")));
		return ret;
	}

	for (group_idx = 0; group_idx < num_groups; group_idx++) {
		json_info.json_test_group = json_array_get(test_groups, group_idx);
		ret = fips_test_one_test_group();
		json_array_append_new(write_groups, json_info.json_write_group);
	}

	return 0;
}

static int
fips_test_one_json_file(void)
{
	size_t vector_set_idx, root_size;

	root_size = json_array_size(json_info.json_root);
	fips_test_json_init_writeback();

	for (vector_set_idx = 1; vector_set_idx < root_size; vector_set_idx++) {
		/* Vector set index starts at 1, the 0th index contains test session
		 * information.
		 */
		json_info.json_vector_set = json_array_get(json_info.json_root, vector_set_idx);
		fips_test_one_vector_set();
		json_array_append_new(json_info.json_write_root, json_info.json_write_set);
		json_incref(json_info.json_write_set);
	}

	json_dumpf(json_info.json_write_root, info.fp_wr, JSON_INDENT(4));
	json_decref(json_info.json_write_root);

	return 0;
}
#endif /* USE_JANSSON */
