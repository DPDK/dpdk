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
#include "fips_dev_self_test.h"

#define REQ_FILE_PATH_KEYWORD	"req-file"
#define RSP_FILE_PATH_KEYWORD	"rsp-file"
#define MBUF_DATAROOM_KEYWORD	"mbuf-dataroom"
#define FOLDER_KEYWORD		"path-is-folder"
#define CRYPTODEV_KEYWORD	"cryptodev"
#define CRYPTODEV_ID_KEYWORD	"cryptodev-id"
#define CRYPTODEV_ST_KEYWORD	"self-test"
#define CRYPTODEV_BK_ID_KEYWORD	"broken-test-id"
#define CRYPTODEV_BK_DIR_KEY	"broken-test-dir"
#define CRYPTODEV_ENC_KEYWORD	"enc"
#define CRYPTODEV_DEC_KEYWORD	"dec"

struct fips_test_vector vec;
struct fips_test_interim_info info;

struct cryptodev_fips_validate_env {
	const char *req_path;
	const char *rsp_path;
	uint32_t is_path_folder;
	uint8_t dev_id;
	uint8_t dev_support_sgl;
	uint16_t mbuf_data_room;
	struct rte_mempool *mpool;
	struct rte_mempool *sess_mpool;
	struct rte_mempool *sess_priv_mpool;
	struct rte_mempool *op_pool;
	struct rte_mbuf *mbuf;
	uint8_t *digest;
	uint16_t digest_len;
	struct rte_crypto_op *op;
	struct rte_cryptodev_sym_session *sess;
	uint16_t self_test;
	struct fips_dev_broken_test_config *broken_test_config;
} env;

static int
cryptodev_fips_validate_app_int(void)
{
	struct rte_cryptodev_config conf = {rte_socket_id(), 1, 0};
	struct rte_cryptodev_qp_conf qp_conf = {128, NULL, NULL};
	struct rte_cryptodev_info dev_info;
	uint32_t sess_sz = rte_cryptodev_sym_get_private_session_size(
			env.dev_id);
	uint32_t nb_mbufs = UINT16_MAX / env.mbuf_data_room + 1;
	int ret;

	if (env.self_test) {
		ret = fips_dev_self_test(env.dev_id, env.broken_test_config);
		if (ret < 0) {
			struct rte_cryptodev *cryptodev =
					rte_cryptodev_pmd_get_dev(env.dev_id);

			rte_cryptodev_pmd_destroy(cryptodev);

			return ret;
		}
	}

	ret = rte_cryptodev_configure(env.dev_id, &conf);
	if (ret < 0)
		return ret;

	rte_cryptodev_info_get(env.dev_id, &dev_info);
	if (dev_info.feature_flags & RTE_CRYPTODEV_FF_IN_PLACE_SGL)
		env.dev_support_sgl = 1;
	else
		env.dev_support_sgl = 0;

	env.mpool = rte_pktmbuf_pool_create("FIPS_MEMPOOL", nb_mbufs,
			0, 0, sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM +
			env.mbuf_data_room, rte_socket_id());
	if (!env.mpool)
		return ret;

	ret = rte_cryptodev_queue_pair_setup(env.dev_id, 0, &qp_conf,
			rte_socket_id());
	if (ret < 0)
		return ret;

	ret = -ENOMEM;

	env.sess_mpool = rte_cryptodev_sym_session_pool_create(
			"FIPS_SESS_MEMPOOL", 16, 0, 0, 0, rte_socket_id());
	if (!env.sess_mpool)
		goto error_exit;

	env.sess_priv_mpool = rte_mempool_create("FIPS_SESS_PRIV_MEMPOOL",
			16, sess_sz, 0, 0, NULL, NULL, NULL,
			NULL, rte_socket_id(), 0);
	if (!env.sess_priv_mpool)
		goto error_exit;

	env.op_pool = rte_crypto_op_pool_create(
			"FIPS_OP_POOL",
			RTE_CRYPTO_OP_TYPE_SYMMETRIC,
			1, 0,
			16,
			rte_socket_id());
	if (!env.op_pool)
		goto error_exit;

	env.op = rte_crypto_op_alloc(env.op_pool, RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	if (!env.op)
		goto error_exit;

	qp_conf.mp_session = env.sess_mpool;
	qp_conf.mp_session_private = env.sess_priv_mpool;

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
	if (env.sess_mpool)
		rte_mempool_free(env.sess_mpool);
	if (env.sess_priv_mpool)
		rte_mempool_free(env.sess_priv_mpool);
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
	rte_mempool_free(env.sess_mpool);
	rte_mempool_free(env.sess_priv_mpool);
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


	if (!rte_cryptodev_pmd_is_valid_dev(cryptodev_id)) {
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
		prgname, REQ_FILE_PATH_KEYWORD, RSP_FILE_PATH_KEYWORD,
		FOLDER_KEYWORD, MBUF_DATAROOM_KEYWORD, def_mbuf_seg_size,
		CRYPTODEV_KEYWORD, CRYPTODEV_ID_KEYWORD, CRYPTODEV_ST_KEYWORD,
		CRYPTODEV_BK_ID_KEYWORD, CRYPTODEV_BK_DIR_KEY);
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
			{MBUF_DATAROOM_KEYWORD, required_argument, 0, 0},
			{CRYPTODEV_KEYWORD, required_argument, 0, 0},
			{CRYPTODEV_ID_KEYWORD, required_argument, 0, 0},
			{CRYPTODEV_ST_KEYWORD, no_argument, 0, 0},
			{CRYPTODEV_BK_ID_KEYWORD, required_argument, 0, 0},
			{CRYPTODEV_BK_DIR_KEY, required_argument, 0, 0},
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
			} else if (strcmp(lgopts[option_index].name,
					CRYPTODEV_ST_KEYWORD) == 0) {
				env.self_test = 1;
			} else if (strcmp(lgopts[option_index].name,
					CRYPTODEV_BK_ID_KEYWORD) == 0) {
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
			} else if (strcmp(lgopts[option_index].name,
					CRYPTODEV_BK_DIR_KEY) == 0) {
				if (!env.broken_test_config) {
					env.broken_test_config = rte_malloc(
						NULL,
						sizeof(*env.broken_test_config),
						0);
					if (!env.broken_test_config)
						return -ENOMEM;

					env.broken_test_config->
						expect_fail_test_idx = 0;
				}

				if (strcmp(optarg, CRYPTODEV_ENC_KEYWORD) == 0)
					env.broken_test_config->expect_fail_dir =
						self_test_dir_enc_auth_gen;
				else if (strcmp(optarg, CRYPTODEV_DEC_KEYWORD)
						== 0)
					env.broken_test_config->expect_fail_dir =
						self_test_dir_dec_auth_verify;
				else {
					rte_free(env.broken_test_config);
					cryptodev_fips_validate_usage(prgname);
					return -EINVAL;
				}
			} else if (strcmp(lgopts[option_index].name,
					MBUF_DATAROOM_KEYWORD) == 0) {
				uint32_t data_room_size;

				if (parser_read_uint32(&data_room_size,
						optarg) < 0) {
					cryptodev_fips_validate_usage(prgname);
					return -EINVAL;
				}

				if (data_room_size == 0 ||
						data_room_size > UINT16_MAX) {
					cryptodev_fips_validate_usage(prgname);
					return -EINVAL;
				}

				env.mbuf_data_room = data_room_size;
			} else {
				cryptodev_fips_validate_usage(prgname);
				return -EINVAL;
			}
			break;
		default:
			return -1;
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

	ret = cryptodev_fips_validate_app_int();
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

	if (env.mbuf)
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
	if (head)
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

	rte_crypto_op_attach_sym_session(env.op, env.sess);

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

	if (env.digest)
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

	rte_crypto_op_attach_sym_session(env.op, env.sess);

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

		if (env.digest)
			rte_free(env.digest);
		env.digest = rte_zmalloc(NULL, vec.aead.digest.len,
				RTE_CACHE_LINE_SIZE);
		if (!env.digest) {
			RTE_LOG(ERR, USER1, "Not enough memory\n");
			return -ENOMEM;
		}
		env.digest_len = vec.cipher_auth.digest.len;

		sym->aead.data.length = vec.pt.len;
		sym->aead.digest.data = env.digest;
		sym->aead.digest.phys_addr = rte_malloc_virt2iova(env.digest);
	} else {
		ret = prepare_data_mbufs(&vec.ct);
		if (ret < 0)
			return ret;

		sym->aead.data.length = vec.ct.len;
		sym->aead.digest.data = vec.aead.digest.val;
		sym->aead.digest.phys_addr = rte_malloc_virt2iova(
				sym->aead.digest.data);
	}

	sym->m_src = env.mbuf;
	sym->aead.data.offset = 0;
	sym->aead.aad.data = vec.aead.aad.val;
	sym->aead.aad.phys_addr = rte_malloc_virt2iova(sym->aead.aad.data);

	rte_crypto_op_attach_sym_session(env.op, env.sess);

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
	else
		cipher_xform->algo = RTE_CRYPTO_CIPHER_AES_ECB;

	cipher_xform->op = (info.op == FIPS_TEST_ENC_AUTH_GEN) ?
			RTE_CRYPTO_CIPHER_OP_ENCRYPT :
			RTE_CRYPTO_CIPHER_OP_DECRYPT;
	cipher_xform->key.data = vec.cipher_auth.key.val;
	cipher_xform->key.length = vec.cipher_auth.key.len;
	if (cipher_xform->algo == RTE_CRYPTO_CIPHER_AES_CBC) {
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
		RTE_LOG(ERR, USER1, "PMD %s key length %u IV length %u\n",
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
		RTE_LOG(ERR, USER1, "PMD %s key length %u IV length %u\n",
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
get_writeback_data(struct fips_val *val)
{
	struct rte_mbuf *m = env.mbuf;
	uint16_t data_len = rte_pktmbuf_pkt_len(m);
	uint16_t total_len = data_len + env.digest_len;
	uint8_t *src, *dst, *wb_data;

	/* in case val is reused for MCT test, try to free the buffer first */
	if (val->val) {
		free(val->val);
		val->val = NULL;
	}

	wb_data = dst = calloc(1, total_len);
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
		free(wb_data);
		return -1;
	}

	if (env.digest)
		memcpy(dst, env.digest, env.digest_len);

	val->val = wb_data;
	val->len = total_len;

	return 0;
}

static int
fips_run_test(void)
{
	struct rte_crypto_sym_xform xform = {0};
	uint16_t n_deqd;
	int ret;

	ret = test_ops.prepare_xform(&xform);
	if (ret < 0)
		return ret;

	env.sess = rte_cryptodev_sym_session_create(env.sess_mpool);
	if (!env.sess)
		return -ENOMEM;

	ret = rte_cryptodev_sym_session_init(env.dev_id,
			env.sess, &xform, env.sess_priv_mpool);
	if (ret < 0) {
		RTE_LOG(ERR, USER1, "Error %i: Init session\n",
				ret);
		goto exit;
	}

	ret = test_ops.prepare_op();
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

		n_deqd = rte_cryptodev_dequeue_burst(env.dev_id, 0, &deqd_op,
				1);
	} while (n_deqd == 0);

	vec.status = env.op->status;

exit:
	rte_cryptodev_sym_session_clear(env.dev_id, env.sess);
	rte_cryptodev_sym_session_free(env.sess);
	env.sess = NULL;

	return ret;
}

static int
fips_generic_test(void)
{
	struct fips_val val = {NULL, 0};
	int ret;

	fips_test_write_one_case();

	ret = fips_run_test();
	if (ret < 0) {
		if (ret == -EPERM || ret == -ENOTSUP) {
			fprintf(info.fp_wr, "Bypass\n\n");
			return 0;
		}

		return ret;
	}

	ret = get_writeback_data(&val);
	if (ret < 0)
		return ret;

	switch (info.file_type) {
	case FIPS_TYPE_REQ:
	case FIPS_TYPE_RSP:
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
	}

	fprintf(info.fp_wr, "\n");
	free(val.val);

	return 0;
}

static int
fips_mct_tdes_test(void)
{
#define TDES_BLOCK_SIZE		8
#define TDES_EXTERN_ITER	400
#define TDES_INTERN_ITER	10000
	struct fips_val val = {NULL, 0}, val_key;
	uint8_t prev_out[TDES_BLOCK_SIZE] = {0};
	uint8_t prev_prev_out[TDES_BLOCK_SIZE] = {0};
	uint8_t prev_in[TDES_BLOCK_SIZE] = {0};
	uint32_t i, j, k;
	int ret;
	int test_mode = info.interim_info.tdes_data.test_mode;

	for (i = 0; i < TDES_EXTERN_ITER; i++) {
		if ((i == 0) && (info.version == 21.4f)) {
			if (!(strstr(info.vec[0], "COUNT")))
				fprintf(info.fp_wr, "%s%u\n", "COUNT = ", 0);
		}

		if (i != 0)
			update_info_vec(i);

		fips_test_write_one_case();

		for (j = 0; j < TDES_INTERN_ITER; j++) {
			ret = fips_run_test();
			if (ret < 0) {
				if (ret == -EPERM) {
					fprintf(info.fp_wr, "Bypass\n");
					return 0;
				}
				return ret;
			}

			ret = get_writeback_data(&val);
			if (ret < 0)
				return ret;

			if (info.op == FIPS_TEST_DEC_AUTH_VERIF)
				memcpy(prev_in, vec.ct.val, TDES_BLOCK_SIZE);

			if (j == 0) {
				memcpy(prev_out, val.val, TDES_BLOCK_SIZE);

				if (info.op == FIPS_TEST_ENC_AUTH_GEN) {
					if (test_mode == TDES_MODE_ECB) {
						memcpy(vec.pt.val, val.val,
							   TDES_BLOCK_SIZE);
					} else {
						memcpy(vec.pt.val, vec.iv.val,
							   TDES_BLOCK_SIZE);
						memcpy(vec.iv.val, val.val,
							   TDES_BLOCK_SIZE);
					}

				} else {
					if (test_mode == TDES_MODE_ECB) {
						memcpy(vec.ct.val, val.val,
							   TDES_BLOCK_SIZE);
					} else {
						memcpy(vec.iv.val, vec.ct.val,
							   TDES_BLOCK_SIZE);
						memcpy(vec.ct.val, val.val,
							   TDES_BLOCK_SIZE);
					}
				}
				continue;
			}

			if (info.op == FIPS_TEST_ENC_AUTH_GEN) {
				if (test_mode == TDES_MODE_ECB) {
					memcpy(vec.pt.val, val.val,
						   TDES_BLOCK_SIZE);
				} else {
					memcpy(vec.iv.val, val.val,
						   TDES_BLOCK_SIZE);
					memcpy(vec.pt.val, prev_out,
						   TDES_BLOCK_SIZE);
				}
			} else {
				if (test_mode == TDES_MODE_ECB) {
					memcpy(vec.ct.val, val.val,
						   TDES_BLOCK_SIZE);
				} else {
					memcpy(vec.iv.val, vec.ct.val,
						   TDES_BLOCK_SIZE);
					memcpy(vec.ct.val, val.val,
						   TDES_BLOCK_SIZE);
				}
			}

			if (j == TDES_INTERN_ITER - 1)
				continue;

			memcpy(prev_out, val.val, TDES_BLOCK_SIZE);

			if (j == TDES_INTERN_ITER - 3)
				memcpy(prev_prev_out, val.val, TDES_BLOCK_SIZE);
		}

		info.parse_writeback(&val);
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
				val_key.val[k] ^= val.val[k];
				val_key.val[k + 8] ^= prev_out[k];
				val_key.val[k + 16] ^= prev_prev_out[k];
				break;
			case 2:
				val_key.val[k] ^= val.val[k];
				val_key.val[k + 8] ^= prev_out[k];
				val_key.val[k + 16] ^= val.val[k];
				break;
			default: /* case 1 */
				val_key.val[k] ^= val.val[k];
				val_key.val[k + 8] ^= val.val[k];
				val_key.val[k + 16] ^= val.val[k];
				break;
			}

		}

		for (k = 0; k < 24; k++)
			val_key.val[k] = (__builtin_popcount(val_key.val[k]) &
					0x1) ?
					val_key.val[k] : (val_key.val[k] ^ 0x1);

		if (info.op == FIPS_TEST_ENC_AUTH_GEN) {
			if (test_mode == TDES_MODE_ECB) {
				memcpy(vec.pt.val, val.val, TDES_BLOCK_SIZE);
			} else {
				memcpy(vec.iv.val, val.val, TDES_BLOCK_SIZE);
				memcpy(vec.pt.val, prev_out, TDES_BLOCK_SIZE);
			}
		} else {
			if (test_mode == TDES_MODE_ECB) {
				memcpy(vec.ct.val, val.val, TDES_BLOCK_SIZE);
			} else {
				memcpy(vec.iv.val, prev_out, TDES_BLOCK_SIZE);
				memcpy(vec.ct.val, val.val, TDES_BLOCK_SIZE);
			}
		}
	}

	if (val.val)
		free(val.val);

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

	if (val.val)
		free(val.val);

	return 0;
}
static int
fips_mct_aes_test(void)
{
#define AES_BLOCK_SIZE	16
#define AES_EXTERN_ITER	100
#define AES_INTERN_ITER	1000
	struct fips_val val = {NULL, 0}, val_key;
	uint8_t prev_out[AES_BLOCK_SIZE] = {0};
	uint8_t prev_in[AES_BLOCK_SIZE] = {0};
	uint32_t i, j, k;
	int ret;

	if (info.interim_info.aes_data.cipher_algo == RTE_CRYPTO_CIPHER_AES_ECB)
		return fips_mct_aes_ecb_test();

	for (i = 0; i < AES_EXTERN_ITER; i++) {
		if (i != 0)
			update_info_vec(i);

		fips_test_write_one_case();

		for (j = 0; j < AES_INTERN_ITER; j++) {
			ret = fips_run_test();
			if (ret < 0) {
				if (ret == -EPERM) {
					fprintf(info.fp_wr, "Bypass\n");
					return 0;
				}

				return ret;
			}

			ret = get_writeback_data(&val);
			if (ret < 0)
				return ret;

			if (info.op == FIPS_TEST_DEC_AUTH_VERIF)
				memcpy(prev_in, vec.ct.val, AES_BLOCK_SIZE);

			if (j == 0) {
				memcpy(prev_out, val.val, AES_BLOCK_SIZE);

				if (info.op == FIPS_TEST_ENC_AUTH_GEN) {
					memcpy(vec.pt.val, vec.iv.val,
							AES_BLOCK_SIZE);
					memcpy(vec.iv.val, val.val,
							AES_BLOCK_SIZE);
				} else {
					memcpy(vec.ct.val, vec.iv.val,
							AES_BLOCK_SIZE);
					memcpy(vec.iv.val, prev_in,
							AES_BLOCK_SIZE);
				}
				continue;
			}

			if (info.op == FIPS_TEST_ENC_AUTH_GEN) {
				memcpy(vec.iv.val, val.val, AES_BLOCK_SIZE);
				memcpy(vec.pt.val, prev_out, AES_BLOCK_SIZE);
			} else {
				memcpy(vec.iv.val, prev_in, AES_BLOCK_SIZE);
				memcpy(vec.ct.val, prev_out, AES_BLOCK_SIZE);
			}

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

		if (info.op == FIPS_TEST_DEC_AUTH_VERIF)
			memcpy(vec.iv.val, val.val, AES_BLOCK_SIZE);
	}

	if (val.val)
		free(val.val);

	return 0;
}

static int
fips_mct_sha_test(void)
{
#define SHA_EXTERN_ITER	100
#define SHA_INTERN_ITER	1000
#define SHA_MD_BLOCK	3
	struct fips_val val = {NULL, 0}, md[SHA_MD_BLOCK];
	char temp[MAX_DIGEST_SIZE*2];
	int ret;
	uint32_t i, j;

	for (i = 0; i < SHA_MD_BLOCK; i++)
		md[i].val = rte_malloc(NULL, (MAX_DIGEST_SIZE*2), 0);

	rte_free(vec.pt.val);
	vec.pt.val = rte_malloc(NULL, (MAX_DIGEST_SIZE*SHA_MD_BLOCK), 0);

	fips_test_write_one_case();
	fprintf(info.fp_wr, "\n");

	for (j = 0; j < SHA_EXTERN_ITER; j++) {

		memcpy(md[0].val, vec.cipher_auth.digest.val,
			vec.cipher_auth.digest.len);
		md[0].len = vec.cipher_auth.digest.len;
		memcpy(md[1].val, vec.cipher_auth.digest.val,
			vec.cipher_auth.digest.len);
		md[1].len = vec.cipher_auth.digest.len;
		memcpy(md[2].val, vec.cipher_auth.digest.val,
			vec.cipher_auth.digest.len);
		md[2].len = vec.cipher_auth.digest.len;

		for (i = 0; i < (SHA_INTERN_ITER); i++) {

			memcpy(vec.pt.val, md[0].val,
				(size_t)md[0].len);
			memcpy((vec.pt.val + md[0].len), md[1].val,
				(size_t)md[1].len);
			memcpy((vec.pt.val + md[0].len + md[1].len),
				md[2].val,
				(size_t)md[2].len);
			vec.pt.len = md[0].len + md[1].len + md[2].len;

			ret = fips_run_test();
			if (ret < 0) {
				if (ret == -EPERM || ret == -ENOTSUP) {
					fprintf(info.fp_wr, "Bypass\n\n");
					return 0;
				}
				return ret;
			}

			ret = get_writeback_data(&val);
			if (ret < 0)
				return ret;

			memcpy(md[0].val, md[1].val, md[1].len);
			md[0].len = md[1].len;
			memcpy(md[1].val, md[2].val, md[2].len);
			md[1].len = md[2].len;

			memcpy(md[2].val, (val.val + vec.pt.len),
				vec.cipher_auth.digest.len);
			md[2].len = vec.cipher_auth.digest.len;
		}

		memcpy(vec.cipher_auth.digest.val, md[2].val, md[2].len);
		vec.cipher_auth.digest.len = md[2].len;

		fprintf(info.fp_wr, "COUNT = %u\n", j);

		writeback_hex_str("", temp, &vec.cipher_auth.digest);

		fprintf(info.fp_wr, "MD = %s\n\n", temp);
	}

	for (i = 0; i < (SHA_MD_BLOCK); i++)
		rte_free(md[i].val);

	rte_free(vec.pt.val);

	if (val.val)
		free(val.val);

	return 0;
}


static int
init_test_ops(void)
{
	switch (info.algo) {
	case FIPS_TEST_ALGO_AES:
		test_ops.prepare_op = prepare_cipher_op;
		test_ops.prepare_xform  = prepare_aes_xform;
		if (info.interim_info.aes_data.test_type == AESAVS_TYPE_MCT)
			test_ops.test = fips_mct_aes_test;
		else
			test_ops.test = fips_generic_test;
		break;
	case FIPS_TEST_ALGO_HMAC:
		test_ops.prepare_op = prepare_auth_op;
		test_ops.prepare_xform = prepare_hmac_xform;
		test_ops.test = fips_generic_test;
		break;
	case FIPS_TEST_ALGO_TDES:
		test_ops.prepare_op = prepare_cipher_op;
		test_ops.prepare_xform  = prepare_tdes_xform;
		if (info.interim_info.tdes_data.test_type == TDES_MCT)
			test_ops.test = fips_mct_tdes_test;
		else
			test_ops.test = fips_generic_test;
		break;
	case FIPS_TEST_ALGO_AES_GCM:
		test_ops.prepare_op = prepare_aead_op;
		test_ops.prepare_xform = prepare_gcm_xform;
		test_ops.test = fips_generic_test;
		break;
	case FIPS_TEST_ALGO_AES_CMAC:
		test_ops.prepare_op = prepare_auth_op;
		test_ops.prepare_xform = prepare_cmac_xform;
		test_ops.test = fips_generic_test;
		break;
	case FIPS_TEST_ALGO_AES_CCM:
		test_ops.prepare_op = prepare_aead_op;
		test_ops.prepare_xform = prepare_ccm_xform;
		test_ops.test = fips_generic_test;
		break;
	case FIPS_TEST_ALGO_SHA:
		test_ops.prepare_op = prepare_auth_op;
		test_ops.prepare_xform = prepare_sha_xform;
		if (info.interim_info.sha_data.test_type == SHA_MCT)
			test_ops.test = fips_mct_sha_test;
		else
			test_ops.test = fips_generic_test;
		break;
	case FIPS_TEST_ALGO_AES_XTS:
		test_ops.prepare_op = prepare_cipher_op;
		test_ops.prepare_xform = prepare_xts_xform;
		test_ops.test = fips_generic_test;
		break;
	default:
		if (strstr(info.file_name, "TECB") ||
				strstr(info.file_name, "TCBC")) {
			info.algo = FIPS_TEST_ALGO_TDES;
			test_ops.prepare_op = prepare_cipher_op;
			test_ops.prepare_xform	= prepare_tdes_xform;
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
	}
	if (env.mbuf)
		rte_pktmbuf_free(env.mbuf);

	return ret;
}
