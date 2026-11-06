/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026
 */

#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <jansson.h>

#include <rte_cryptodev.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_errno.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_memory.h>

#define DEFAULT_MBUF_DATA_ROOM 16384
#define NUM_QP_DESCRIPTORS 128
#define MAX_IV_LEN 512
#define MAX_EC_BYTESIZE 66
/* CCM AAD is prefixed with an 18-byte reserved area (B0 block + length). */
#define AES_CCM_AAD_OFFSET 18
#define IV_OFFSET (sizeof(struct rte_crypto_op) + sizeof(struct rte_crypto_sym_op))

struct wycheproof_data {
	uint8_t *key;
	uint8_t *iv;
	uint8_t *aad;
	uint8_t *msg;
	uint8_t *ct;
	uint8_t *tag;
	uint32_t key_len;
	uint32_t iv_len;
	uint32_t aad_len;
	uint32_t msg_len;
	uint32_t ct_len;
	uint32_t tag_len;
	json_int_t tc_id;
	const char *result;
};

struct app_env {
	uint8_t dev_id;
	uint8_t hash_dev_id;
	bool hash_dev_own;
	uint32_t mbuf_data_room;
	const char *vectors_path;
	bool debug;
	struct rte_mempool *mbuf_pool;
	struct rte_mempool *session_pool;
	struct rte_mempool *op_pool;
	struct rte_mempool *asym_session_pool;
	struct rte_mempool *asym_op_pool;
};

struct app_stats {
	uint64_t passed;
	uint64_t failed;
	uint64_t skipped_capability;
	uint64_t skipped_acceptable;
	uint64_t skipped_unsupported;
};

static struct app_env env = {
	.mbuf_data_room = DEFAULT_MBUF_DATA_ROOM,
};

static void
usage(const char *program)
{
	printf("%s [EAL options] -- "
		"--vectors PATH [--cryptodev NAME | --cryptodev-id ID]\n"
		"\t[--mbuf-dataroom BYTES] [--debug]\n",
		program);
}

static void
debug_vector(const char *type, const char *algorithm,
		const struct wycheproof_data *vector, const char *reason)
{
	if (env.debug)
		printf("%s %s tcId=%" JSON_INTEGER_FORMAT " result=%s: %s\n", type,
			algorithm, vector->tc_id, vector->result, reason);
}

static void
debug_file(const char *type, const char *path, const char *reason)
{
	if (env.debug)
		printf("%s %s: %s\n", type, path, reason);
}

static int
parse_uint32(const char *text, uint32_t *value)
{
	char *end;
	unsigned long parsed;

	errno = 0;
	parsed = strtoul(text, &end, 10);
	if (errno != 0 || *text == '\0' || *end != '\0' || parsed > UINT32_MAX)
		return -EINVAL;

	*value = parsed;
	return 0;
}

static int
parse_args(int argc, char **argv)
{
	static const struct option options[] = {
		{ "vectors", required_argument, NULL, 'v' },
		{ "cryptodev", required_argument, NULL, 'c' },
		{ "cryptodev-id", required_argument, NULL, 'd' },
		{ "mbuf-dataroom", required_argument, NULL, 'm' },
		{ "debug", no_argument, NULL, 'D' },
		{ NULL, 0, NULL, 0 },
	};
	bool have_device = false;
	int option;

	if (rte_cryptodev_count() == 0)
		return -ENODEV;
	env.dev_id = 0;

	while ((option = getopt_long(argc, argv, "v:c:d:m:D", options, NULL)) != -1) {
		uint32_t value;
		int dev_id;

		switch (option) {
		case 'v':
			env.vectors_path = optarg;
			break;
		case 'c':
			dev_id = rte_cryptodev_get_dev_id(optarg);
			if (dev_id < 0)
				return dev_id;
			env.dev_id = dev_id;
			have_device = true;
			break;
		case 'd':
			if (parse_uint32(optarg, &value) != 0 || value > UINT8_MAX ||
					!rte_cryptodev_is_valid_dev(value))
				return -EINVAL;
			env.dev_id = value;
			have_device = true;
			break;
		case 'm':
			if (parse_uint32(optarg, &env.mbuf_data_room) != 0 ||
					env.mbuf_data_room < RTE_PKTMBUF_HEADROOM ||
					env.mbuf_data_room > UINT16_MAX)
				return -EINVAL;
			break;
		case 'D':
			env.debug = true;
			break;
		default:
			return -EINVAL;
		}
	}

	if (env.vectors_path == NULL)
		return -EINVAL;
	if (!have_device)
		printf("Using cryptodev %u (%s)\n", env.dev_id,
			rte_cryptodev_name_get(env.dev_id));

	return 0;
}

static int
app_init(void)
{
	struct rte_cryptodev_config config = {
		.socket_id = rte_socket_id(),
		.nb_queue_pairs = 1,
		.ff_disable = 0,
	};
	struct rte_cryptodev_qp_conf queue_pair = {
		.nb_descriptors = NUM_QP_DESCRIPTORS,
		.mp_session = NULL,
	};
	struct rte_cryptodev_info dev_info;
	uint32_t session_size;
	int ret;

	rte_cryptodev_info_get(env.dev_id, &dev_info);
	if (dev_info.max_nb_queue_pairs < config.nb_queue_pairs)
		return -ENOTSUP;

	ret = rte_cryptodev_configure(env.dev_id, &config);
	if (ret < 0)
		return ret;

	env.mbuf_pool = rte_pktmbuf_pool_create("WYCHEPROOF_MBUF_POOL", 64, 0, 0,
		env.mbuf_data_room, rte_socket_id());
	if (env.mbuf_pool == NULL) {
		ret = -rte_errno;
		goto error;
	}

	session_size = rte_cryptodev_sym_get_private_session_size(env.dev_id);
	{
		/* The hashing device may differ from the target device, so size the
		 * session pool for the largest sym session across all devices.
		 */
		uint8_t d;

		for (d = 0; d < rte_cryptodev_count(); d++) {
			uint32_t s = rte_cryptodev_sym_get_private_session_size(d);

			if (s > session_size)
				session_size = s;
		}
	}
	env.session_pool = rte_cryptodev_sym_session_pool_create("WYCHEPROOF_SESSION_POOL",
		64, session_size, 0, 0, rte_socket_id());
	if (env.session_pool == NULL) {
		ret = -rte_errno;
		goto error;
	}

	/* priv_size is measured from the end of the sym op; the IV lives there. */
	env.op_pool = rte_crypto_op_pool_create("WYCHEPROOF_OP_POOL",
		RTE_CRYPTO_OP_TYPE_SYMMETRIC, 64, 0, MAX_IV_LEN, rte_socket_id());
	if (env.op_pool == NULL) {
		ret = -rte_errno;
		goto error;
	}

	/* Asymmetric pools are best-effort; DSA/ECDSA paths gate on the
	 * device asymmetric feature flag at runtime.
	 */
	env.asym_session_pool = rte_cryptodev_asym_session_pool_create(
		"WYCHEPROOF_ASYM_SESS", 16, 0, 0, rte_socket_id());
	env.asym_op_pool = rte_crypto_op_pool_create("WYCHEPROOF_ASYM_OP",
		RTE_CRYPTO_OP_TYPE_ASYMMETRIC, 64, 0, 0, rte_socket_id());

	queue_pair.mp_session = env.session_pool;
	ret = rte_cryptodev_queue_pair_setup(env.dev_id, 0, &queue_pair,
		rte_socket_id());
	if (ret < 0)
		goto error;

	ret = rte_cryptodev_start(env.dev_id);
	if (ret < 0)
		goto error;

	/* Digest computation for DSA/ECDSA needs a symmetric-auth device. The
	 * target device may be asym-only (for example QAT), so pick a separate
	 * sym-capable device for hashing when needed.
	 */
	env.hash_dev_id = env.dev_id;
	env.hash_dev_own = false;
	{
		struct rte_cryptodev_sym_capability_idx hash_idx = {
			.type = RTE_CRYPTO_SYM_XFORM_AUTH,
			.algo.auth = RTE_CRYPTO_AUTH_SHA256,
		};
		uint8_t count = rte_cryptodev_count();
		uint8_t d;

		if (rte_cryptodev_sym_capability_get(env.dev_id, &hash_idx) == NULL) {
			for (d = 0; d < count; d++) {
				struct rte_cryptodev_qp_conf hash_qp = {
					.nb_descriptors = NUM_QP_DESCRIPTORS,
					.mp_session = NULL,
				};

				if (d == env.dev_id)
					continue;
				if (rte_cryptodev_sym_capability_get(d, &hash_idx) == NULL)
					continue;
				if (rte_cryptodev_configure(d, &config) < 0)
					continue;
				hash_qp.mp_session = env.session_pool;
				if (rte_cryptodev_queue_pair_setup(d, 0, &hash_qp,
						rte_socket_id()) < 0 ||
						rte_cryptodev_start(d) < 0) {
					rte_cryptodev_close(d);
					continue;
				}
				env.hash_dev_id = d;
				env.hash_dev_own = true;
				break;
			}
		}
	}

	return 0;

error:
	rte_mempool_free(env.asym_op_pool);
	rte_mempool_free(env.asym_session_pool);
	rte_mempool_free(env.op_pool);
	rte_mempool_free(env.session_pool);
	rte_mempool_free(env.mbuf_pool);
	env.asym_op_pool = NULL;
	env.asym_session_pool = NULL;
	env.op_pool = NULL;
	env.session_pool = NULL;
	env.mbuf_pool = NULL;
	rte_cryptodev_close(env.dev_id);
	return ret;
}

static void
app_uninit(void)
{
	if (env.hash_dev_own) {
		rte_cryptodev_stop(env.hash_dev_id);
		rte_cryptodev_close(env.hash_dev_id);
	}
	rte_cryptodev_stop(env.dev_id);
	rte_cryptodev_close(env.dev_id);
	rte_mempool_free(env.asym_op_pool);
	rte_mempool_free(env.asym_session_pool);
	rte_mempool_free(env.op_pool);
	rte_mempool_free(env.session_pool);
	rte_mempool_free(env.mbuf_pool);
}

static void
free_vector(struct wycheproof_data *vector)
{
	rte_free_sensitive(vector->key);
	rte_free(vector->iv);
	rte_free(vector->aad);
	rte_free(vector->msg);
	rte_free(vector->ct);
	rte_free(vector->tag);
	memset(vector, 0, sizeof(*vector));
}

static int
hex_value(char character)
{
	if (character >= '0' && character <= '9')
		return character - '0';
	if (character >= 'a' && character <= 'f')
		return character - 'a' + 10;
	if (character >= 'A' && character <= 'F')
		return character - 'A' + 10;
	return -1;
}

static int
decode_hex(json_t *value, uint8_t **buffer, uint32_t *length)
{
	const char *hex;
	size_t hex_length;
	uint8_t *decoded;
	size_t index;

	if (!json_is_string(value))
		return -EINVAL;

	hex = json_string_value(value);
	hex_length = strlen(hex);
	if ((hex_length & 1) != 0 || hex_length / 2 > UINT32_MAX)
		return -EINVAL;

	*length = hex_length / 2;
	if (*length == 0)
		return 0;

	decoded = rte_malloc(NULL, *length, 0);
	if (decoded == NULL)
		return -ENOMEM;

	for (index = 0; index < *length; index++) {
		int high = hex_value(hex[index * 2]);
		int low = hex_value(hex[index * 2 + 1]);

		if (high < 0 || low < 0) {
			rte_free(decoded);
			return -EINVAL;
		}
		decoded[index] = (high << 4) | low;
	}

	*buffer = decoded;
	return 0;
}

static int
parse_vector(json_t *test, struct wycheproof_data *vector)
{
	json_t *tc_id;
	int ret;

	memset(vector, 0, sizeof(*vector));
	tc_id = json_object_get(test, "tcId");
	vector->result = json_string_value(json_object_get(test, "result"));
	if (!json_is_integer(tc_id) || vector->result == NULL)
		return -EINVAL;
	vector->tc_id = json_integer_value(tc_id);

	ret = decode_hex(json_object_get(test, "key"), &vector->key, &vector->key_len);
	if (ret != 0)
		goto error;
	ret = decode_hex(json_object_get(test, "iv"), &vector->iv, &vector->iv_len);
	if (ret != 0)
		goto error;
	ret = decode_hex(json_object_get(test, "aad"), &vector->aad, &vector->aad_len);
	if (ret != 0)
		goto error;
	ret = decode_hex(json_object_get(test, "msg"), &vector->msg, &vector->msg_len);
	if (ret != 0)
		goto error;
	ret = decode_hex(json_object_get(test, "ct"), &vector->ct, &vector->ct_len);
	if (ret != 0)
		goto error;
	ret = decode_hex(json_object_get(test, "tag"), &vector->tag, &vector->tag_len);
	if (ret != 0)
		goto error;

	return 0;

error:
	free_vector(vector);
	return ret;
}

static int
parse_mac_vector(json_t *test, struct wycheproof_data *vector)
{
	json_t *tc_id;
	int ret;

	memset(vector, 0, sizeof(*vector));
	tc_id = json_object_get(test, "tcId");
	vector->result = json_string_value(json_object_get(test, "result"));
	if (!json_is_integer(tc_id) || vector->result == NULL)
		return -EINVAL;
	vector->tc_id = json_integer_value(tc_id);

	ret = decode_hex(json_object_get(test, "key"), &vector->key, &vector->key_len);
	if (ret != 0)
		goto error;
	ret = decode_hex(json_object_get(test, "msg"), &vector->msg, &vector->msg_len);
	if (ret != 0)
		goto error;
	ret = decode_hex(json_object_get(test, "tag"), &vector->tag, &vector->tag_len);
	if (ret != 0)
		goto error;

	return 0;

error:
	free_vector(vector);
	return ret;
}

static int
parse_mac_with_iv_vector(json_t *test, struct wycheproof_data *vector)
{
	int ret;

	ret = parse_mac_vector(test, vector);
	if (ret != 0)
		return ret;
	ret = decode_hex(json_object_get(test, "iv"), &vector->iv, &vector->iv_len);
	if (ret != 0)
		free_vector(vector);
	return ret;
}

/* Bounded wait for one completed op; NULL on timeout so a stuck PMD fails the
 * vector instead of hanging the tool.
 */
static struct rte_crypto_op *
dequeue_one(uint8_t dev_id)
{
	struct rte_crypto_op *completed = NULL;
	uint64_t deadline = rte_get_timer_cycles() + rte_get_timer_hz();

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &completed, 1) == 0) {
		if (rte_get_timer_cycles() > deadline)
			return NULL;
		rte_pause();
	}
	return completed;
}

static int
check_aead_capability(const struct wycheproof_data *vector,
		enum rte_crypto_aead_algorithm algorithm)
{
	struct rte_cryptodev_sym_capability_idx index = {
		.type = RTE_CRYPTO_SYM_XFORM_AEAD,
		.algo.aead = algorithm,
	};
	const struct rte_cryptodev_symmetric_capability *capability;

	capability = rte_cryptodev_sym_capability_get(env.dev_id, &index);
	if (capability == NULL)
		return -ENOTSUP;

	return rte_cryptodev_sym_capability_check_aead(capability, vector->key_len,
		vector->tag_len, vector->aad_len, vector->iv_len);
}

static int
run_aead(const struct wycheproof_data *vector, enum rte_crypto_aead_algorithm algorithm,
	enum rte_crypto_aead_operation operation, uint8_t **output, uint8_t **digest,
	enum rte_crypto_op_status *status)
{
	struct rte_crypto_sym_xform xform = { 0 };
	struct rte_cryptodev_sym_session *session = NULL;
	struct rte_crypto_op *operation_op = NULL;
	struct rte_mbuf *mbuf = NULL;
	struct rte_crypto_op *completed = NULL;
	struct rte_crypto_sym_op *sym_op;
	uint8_t *aad = NULL;
	uint8_t *input;
	uint32_t input_len;
	int ret = -ENOMEM;

	*output = NULL;
	*digest = NULL;
	xform.type = RTE_CRYPTO_SYM_XFORM_AEAD;
	xform.aead.algo = algorithm;
	xform.aead.op = operation;
	xform.aead.key.data = vector->key;
	xform.aead.key.length = vector->key_len;
	xform.aead.iv.offset = IV_OFFSET;
	xform.aead.iv.length = vector->iv_len;
	xform.aead.aad_length = vector->aad_len;
	xform.aead.digest_length = vector->tag_len;

	session = rte_cryptodev_sym_session_create(env.dev_id, &xform, env.session_pool);
	if (session == NULL)
		goto out;

	operation_op = rte_crypto_op_alloc(env.op_pool, RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	if (operation_op == NULL)
		goto out;

	mbuf = rte_pktmbuf_alloc(env.mbuf_pool);
	if (mbuf == NULL)
		goto out;
	input = operation == RTE_CRYPTO_AEAD_OP_ENCRYPT ? vector->msg : vector->ct;
	input_len = operation == RTE_CRYPTO_AEAD_OP_ENCRYPT ? vector->msg_len : vector->ct_len;
	if (input_len > rte_pktmbuf_tailroom(mbuf)) {
		ret = -EMSGSIZE;
		goto out;
	}
	if (input_len != 0)
		memcpy(rte_pktmbuf_append(mbuf, input_len), input, input_len);

	*digest = rte_malloc(NULL, vector->tag_len, RTE_CACHE_LINE_SIZE);
	if (*digest == NULL && vector->tag_len != 0)
		goto out;
	if (operation == RTE_CRYPTO_AEAD_OP_DECRYPT && vector->tag_len != 0)
		memcpy(*digest, vector->tag, vector->tag_len);

	sym_op = operation_op->sym;
	sym_op->m_src = mbuf;
	sym_op->aead.data.offset = 0;
	sym_op->aead.data.length = input_len;
	if (algorithm == RTE_CRYPTO_AEAD_AES_CCM) {
		aad = rte_zmalloc(NULL,
			RTE_ALIGN_CEIL(vector->aad_len + AES_CCM_AAD_OFFSET, 16), 0);
		if (aad == NULL)
			goto out;
		if (vector->aad_len != 0)
			memcpy(aad + AES_CCM_AAD_OFFSET, vector->aad, vector->aad_len);
		sym_op->aead.aad.data = aad;
		sym_op->aead.aad.phys_addr = rte_malloc_virt2iova(aad);
		memcpy(rte_crypto_op_ctod_offset(operation_op, uint8_t *, IV_OFFSET) + 1,
			vector->iv, vector->iv_len);
	} else {
		sym_op->aead.aad.data = vector->aad;
		sym_op->aead.aad.phys_addr = rte_malloc_virt2iova(vector->aad);
		memcpy(rte_crypto_op_ctod_offset(operation_op, uint8_t *, IV_OFFSET), vector->iv,
			vector->iv_len);
	}
	sym_op->aead.digest.data = *digest;
	sym_op->aead.digest.phys_addr = rte_malloc_virt2iova(*digest);
	rte_crypto_op_attach_sym_session(operation_op, session);

	if (rte_cryptodev_enqueue_burst(env.dev_id, 0, &operation_op, 1) != 1) {
		ret = -EIO;
		goto out;
	}
	completed = dequeue_one(env.dev_id);
	if (completed == NULL)
		/* Device wedged: op still owned by the PMD. Abort without freeing
		 * in-flight state; teardown stops the device.
		 */
		return -ETIMEDOUT;

	*status = completed->status;
	if (*status == RTE_CRYPTO_OP_STATUS_SUCCESS && input_len != 0) {
		*output = rte_malloc(NULL, input_len, 0);
		if (*output == NULL)
			goto out;
		memcpy(*output, rte_pktmbuf_mtod(mbuf, uint8_t *), input_len);
	}
	ret = 0;

out:
	rte_free(aad);
	rte_pktmbuf_free(mbuf);
	rte_crypto_op_free(operation_op);
	rte_cryptodev_sym_session_free(env.dev_id, session);
	if (ret != 0) {
		rte_free(*output);
		rte_free(*digest);
		*output = NULL;
		*digest = NULL;
	}
	return ret;
}

static int
run_hmac(const struct wycheproof_data *vector, enum rte_crypto_auth_algorithm algorithm,
		enum rte_crypto_auth_operation operation, uint8_t **digest,
		enum rte_crypto_op_status *status)
{
	struct rte_crypto_sym_xform xform = { 0 };
	struct rte_cryptodev_sym_session *session = NULL;
	struct rte_crypto_op *op = NULL;
	struct rte_crypto_op *completed = NULL;
	struct rte_mbuf *mbuf = NULL;
	const struct rte_cryptodev_symmetric_capability *capability;
	struct rte_cryptodev_sym_capability_idx index = {
		.type = RTE_CRYPTO_SYM_XFORM_AUTH,
		.algo.auth = algorithm,
	};
	int ret = -ENOMEM;

	*digest = NULL;
	capability = rte_cryptodev_sym_capability_get(env.dev_id, &index);
	if (capability == NULL || rte_cryptodev_sym_capability_check_auth(capability,
			vector->key_len, vector->tag_len, 0) != 0)
		return -ENOTSUP;

	xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	xform.auth.algo = algorithm;
	xform.auth.op = operation;
	xform.auth.key.data = vector->key;
	xform.auth.key.length = vector->key_len;
	xform.auth.digest_length = vector->tag_len;
	session = rte_cryptodev_sym_session_create(env.dev_id, &xform, env.session_pool);
	if (session == NULL)
		goto out;

	op = rte_crypto_op_alloc(env.op_pool, RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	if (op == NULL)
		goto out;
	mbuf = rte_pktmbuf_alloc(env.mbuf_pool);
	if (mbuf == NULL)
		goto out;
	if (vector->msg_len > rte_pktmbuf_tailroom(mbuf)) {
		ret = -EMSGSIZE;
		goto out;
	}
	if (vector->msg_len != 0)
		memcpy(rte_pktmbuf_append(mbuf, vector->msg_len), vector->msg, vector->msg_len);

	*digest = rte_malloc(NULL, vector->tag_len, RTE_CACHE_LINE_SIZE);
	if (*digest == NULL && vector->tag_len != 0)
		goto out;
	if (operation == RTE_CRYPTO_AUTH_OP_VERIFY && vector->tag_len != 0)
		memcpy(*digest, vector->tag, vector->tag_len);

	op->sym->m_src = mbuf;
	op->sym->auth.data.offset = 0;
	op->sym->auth.data.length = vector->msg_len;
	op->sym->auth.digest.data = *digest;
	op->sym->auth.digest.phys_addr = rte_malloc_virt2iova(*digest);
	rte_crypto_op_attach_sym_session(op, session);
	if (rte_cryptodev_enqueue_burst(env.dev_id, 0, &op, 1) != 1) {
		ret = -EIO;
		goto out;
	}
	completed = dequeue_one(env.dev_id);
	if (completed == NULL)
		/* Device wedged: op still owned by the PMD. Abort without freeing
		 * in-flight state; teardown stops the device.
		 */
		return -ETIMEDOUT;
	*status = completed->status;
	ret = 0;

out:
	rte_pktmbuf_free(mbuf);
	rte_crypto_op_free(op);
	rte_cryptodev_sym_session_free(env.dev_id, session);
	if (ret != 0) {
		rte_free(*digest);
		*digest = NULL;
	}
	return ret;
}

static int
run_gmac(const struct wycheproof_data *vector, enum rte_crypto_auth_operation operation,
		uint8_t **digest, enum rte_crypto_op_status *status)
{
	struct rte_crypto_sym_xform xform = { 0 };
	struct rte_cryptodev_sym_session *session = NULL;
	struct rte_crypto_op *op = NULL;
	struct rte_crypto_op *completed = NULL;
	struct rte_mbuf *mbuf = NULL;
	const struct rte_cryptodev_symmetric_capability *capability;
	struct rte_cryptodev_sym_capability_idx index = {
		.type = RTE_CRYPTO_SYM_XFORM_AUTH,
		.algo.auth = RTE_CRYPTO_AUTH_AES_GMAC,
	};
	int ret = -ENOMEM;

	*digest = NULL;
	capability = rte_cryptodev_sym_capability_get(env.dev_id, &index);
	if (capability == NULL || rte_cryptodev_sym_capability_check_auth(capability,
			vector->key_len, vector->tag_len, vector->iv_len) != 0)
		return -ENOTSUP;

	xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	xform.auth.algo = RTE_CRYPTO_AUTH_AES_GMAC;
	xform.auth.op = operation;
	xform.auth.key.data = vector->key;
	xform.auth.key.length = vector->key_len;
	xform.auth.iv.offset = IV_OFFSET;
	xform.auth.iv.length = vector->iv_len;
	xform.auth.digest_length = vector->tag_len;
	session = rte_cryptodev_sym_session_create(env.dev_id, &xform, env.session_pool);
	if (session == NULL)
		goto out;

	op = rte_crypto_op_alloc(env.op_pool, RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	if (op == NULL)
		goto out;
	mbuf = rte_pktmbuf_alloc(env.mbuf_pool);
	if (mbuf == NULL)
		goto out;
	if (vector->msg_len > rte_pktmbuf_tailroom(mbuf)) {
		ret = -EMSGSIZE;
		goto out;
	}
	if (vector->msg_len != 0)
		memcpy(rte_pktmbuf_append(mbuf, vector->msg_len), vector->msg, vector->msg_len);

	*digest = rte_malloc(NULL, vector->tag_len, RTE_CACHE_LINE_SIZE);
	if (*digest == NULL && vector->tag_len != 0)
		goto out;
	if (operation == RTE_CRYPTO_AUTH_OP_VERIFY && vector->tag_len != 0)
		memcpy(*digest, vector->tag, vector->tag_len);

	op->sym->m_src = mbuf;
	op->sym->auth.data.offset = 0;
	op->sym->auth.data.length = vector->msg_len;
	op->sym->auth.digest.data = *digest;
	op->sym->auth.digest.phys_addr = rte_malloc_virt2iova(*digest);
	memcpy(rte_crypto_op_ctod_offset(op, uint8_t *, IV_OFFSET), vector->iv, vector->iv_len);
	rte_crypto_op_attach_sym_session(op, session);
	if (rte_cryptodev_enqueue_burst(env.dev_id, 0, &op, 1) != 1) {
		ret = -EIO;
		goto out;
	}
	completed = dequeue_one(env.dev_id);
	if (completed == NULL)
		/* Device wedged: op still owned by the PMD. Abort without freeing
		 * in-flight state; teardown stops the device.
		 */
		return -ETIMEDOUT;
	*status = completed->status;
	ret = 0;

out:
	rte_pktmbuf_free(mbuf);
	rte_crypto_op_free(op);
	rte_cryptodev_sym_session_free(env.dev_id, session);
	if (ret != 0) {
		rte_free(*digest);
		*digest = NULL;
	}
	return ret;
}

static int
validate_aead_vector(const struct wycheproof_data *vector,
		enum rte_crypto_aead_algorithm algorithm, const char *name,
		struct app_stats *stats)
{
	uint8_t *output = NULL;
	uint8_t *digest = NULL;
	enum rte_crypto_op_status status = RTE_CRYPTO_OP_STATUS_ERROR;
	int ret;

	if (strcmp(vector->result, "acceptable") == 0) {
		stats->skipped_acceptable++;
		debug_vector("SKIP", name, vector, "acceptable result policy");
		return 0;
	}
	if (strcmp(vector->result, "valid") != 0 && strcmp(vector->result, "invalid") != 0) {
		stats->skipped_unsupported++;
		debug_vector("SKIP", name, vector, "unknown result");
		return 0;
	}
	/* CCM writes the IV one byte into the reserved area (after the flags byte). */
	if (vector->iv_len + (algorithm == RTE_CRYPTO_AEAD_AES_CCM ? 1u : 0u) > MAX_IV_LEN ||
			vector->msg_len != vector->ct_len ||
			vector->msg_len > env.mbuf_data_room - RTE_PKTMBUF_HEADROOM) {
		stats->skipped_unsupported++;
		debug_vector("SKIP", name, vector, "unsupported message length");
		return 0;
	}
	if (check_aead_capability(vector, algorithm) != 0) {
		stats->skipped_capability++;
		debug_vector("SKIP", name, vector, "PMD capability");
		return 0;
	}

	if (strcmp(vector->result, "valid") == 0) {
		ret = run_aead(vector, algorithm, RTE_CRYPTO_AEAD_OP_ENCRYPT, &output, &digest,
			&status);
		if (ret == -ETIMEDOUT)
			return -ETIMEDOUT;
		if (ret == -EMSGSIZE)
			goto unsupported;
		if (ret != 0 || status != RTE_CRYPTO_OP_STATUS_SUCCESS ||
				(vector->ct_len != 0 &&
				 memcmp(output, vector->ct, vector->ct_len) != 0) ||
				(vector->tag_len != 0 &&
				 memcmp(digest, vector->tag, vector->tag_len) != 0))
			goto failed;
		rte_free(output);
		rte_free(digest);
		output = NULL;
		digest = NULL;
		ret = run_aead(vector, algorithm, RTE_CRYPTO_AEAD_OP_DECRYPT, &output, &digest,
			&status);
		if (ret == -ETIMEDOUT)
			return -ETIMEDOUT;
		if (ret == -EMSGSIZE)
			goto unsupported;
		if (ret != 0 || status != RTE_CRYPTO_OP_STATUS_SUCCESS ||
				(vector->msg_len != 0 &&
				 memcmp(output, vector->msg, vector->msg_len) != 0))
			goto failed;
	} else {
		ret = run_aead(vector, algorithm, RTE_CRYPTO_AEAD_OP_DECRYPT, &output, &digest,
			&status);
		if (ret == -ETIMEDOUT)
			return -ETIMEDOUT;
		if (ret == -EMSGSIZE)
			goto unsupported;
		if (ret != 0 || status != RTE_CRYPTO_OP_STATUS_AUTH_FAILED)
			goto failed;
	}

	stats->passed++;
	rte_free(output);
	rte_free(digest);
	return 0;

unsupported:
	stats->skipped_unsupported++;
	debug_vector("SKIP", name, vector, "message length exceeds mbuf");
	rte_free(output);
	rte_free(digest);
	return 0;

failed:
	if (env.debug)
		printf("FAIL %s tcId=%" JSON_INTEGER_FORMAT " result=%s status=%d ret=%d\n",
			name, vector->tc_id, vector->result, status, ret);
	stats->failed++;
	rte_free(output);
	rte_free(digest);
	return 0;
}

static int
process_aead(json_t *root, enum rte_crypto_aead_algorithm algorithm,
		const char *name, struct app_stats *stats)
{
	json_t *groups = json_object_get(root, "testGroups");
	json_t *group;
	size_t group_index;

	if (!json_is_array(groups))
		return -EINVAL;

	json_array_foreach(groups, group_index, group) {
		json_t *tests = json_object_get(group, "tests");
		json_t *test;
		size_t test_index;

		if (!json_is_array(tests))
			return -EINVAL;
		json_array_foreach(tests, test_index, test) {
			struct wycheproof_data vector;
			int ret = parse_vector(test, &vector);

			if (ret != 0) {
				if (env.debug)
					printf("SKIP %s group=%zu test=%zu: malformed vector\n",
						name, group_index, test_index);
				stats->skipped_unsupported++;
				continue;
			}
			ret = validate_aead_vector(&vector, algorithm, name, stats);
			if (ret == -ETIMEDOUT)
				return -ETIMEDOUT;
			free_vector(&vector);
		}
	}

	return 0;
}

static int
process_hmac(json_t *root, enum rte_crypto_auth_algorithm algorithm,
		const char *name, struct app_stats *stats)
{
	json_t *groups = json_object_get(root, "testGroups");
	json_t *group;
	size_t group_index;

	if (!json_is_array(groups))
		return -EINVAL;
	json_array_foreach(groups, group_index, group) {
		json_t *tests = json_object_get(group, "tests");
		json_t *test;
		size_t test_index;

		if (!json_is_array(tests))
			return -EINVAL;
		json_array_foreach(tests, test_index, test) {
			struct wycheproof_data vector;
			uint8_t *digest = NULL;
			enum rte_crypto_op_status status = RTE_CRYPTO_OP_STATUS_ERROR;
			int ret;

			ret = parse_mac_vector(test, &vector);
			if (ret != 0) {
				if (env.debug)
					printf("SKIP %s group=%zu test=%zu: malformed vector\n",
						name, group_index, test_index);
				stats->skipped_unsupported++;
				continue;
			}
			if (strcmp(vector.result, "acceptable") == 0) {
				stats->skipped_acceptable++;
				debug_vector("SKIP", name, &vector, "acceptable result policy");
			} else if (strcmp(vector.result, "valid") == 0) {
				ret = run_hmac(&vector, algorithm, RTE_CRYPTO_AUTH_OP_GENERATE,
					&digest, &status);
				if (ret == -ENOTSUP) {
					stats->skipped_capability++;
					debug_vector("SKIP", name, &vector, "PMD capability");
				} else if (ret == -ETIMEDOUT) {
					return -ETIMEDOUT;
				} else if (ret == -EMSGSIZE) {
					stats->skipped_unsupported++;
					debug_vector("SKIP", name, &vector,
						"message length exceeds mbuf");
				} else if (ret != 0 || status != RTE_CRYPTO_OP_STATUS_SUCCESS ||
						(vector.tag_len != 0 &&
						 memcmp(digest, vector.tag, vector.tag_len) != 0)) {
					debug_vector("FAIL", name, &vector,
						"generated tag mismatch");
					stats->failed++;
				} else
					stats->passed++;
			} else if (strcmp(vector.result, "invalid") == 0) {
				ret = run_hmac(&vector, algorithm, RTE_CRYPTO_AUTH_OP_VERIFY,
					&digest, &status);
				if (ret == -ENOTSUP) {
					stats->skipped_capability++;
					debug_vector("SKIP", name, &vector, "PMD capability");
				} else if (ret == -ETIMEDOUT) {
					return -ETIMEDOUT;
				} else if (ret == -EMSGSIZE) {
					stats->skipped_unsupported++;
					debug_vector("SKIP", name, &vector,
						"message length exceeds mbuf");
				} else if (ret != 0 || status != RTE_CRYPTO_OP_STATUS_AUTH_FAILED) {
					debug_vector("FAIL", name, &vector,
						"authentication unexpectedly succeeded");
					stats->failed++;
				} else
					stats->passed++;
			} else {
				stats->skipped_unsupported++;
				debug_vector("SKIP", name, &vector, "unknown result");
			}
			rte_free(digest);
			free_vector(&vector);
		}
	}
	return 0;
}

static int
process_gmac(json_t *root, struct app_stats *stats)
{
	json_t *groups = json_object_get(root, "testGroups");
	json_t *group;
	size_t group_index;

	if (!json_is_array(groups))
		return -EINVAL;
	json_array_foreach(groups, group_index, group) {
		json_t *tests = json_object_get(group, "tests");
		json_t *test;
		size_t test_index;

		if (!json_is_array(tests))
			return -EINVAL;
		json_array_foreach(tests, test_index, test) {
			struct wycheproof_data vector;
			uint8_t *digest = NULL;
			enum rte_crypto_op_status status = RTE_CRYPTO_OP_STATUS_ERROR;
			int ret;

			ret = parse_mac_with_iv_vector(test, &vector);
			if (ret != 0) {
				if (env.debug)
					printf("SKIP AES-GMAC group=%zu test=%zu: "
						"malformed vector\n",
						group_index, test_index);
				stats->skipped_unsupported++;
				continue;
			}
			if (vector.iv_len > MAX_IV_LEN) {
				stats->skipped_unsupported++;
				debug_vector("SKIP", "AES-GMAC", &vector, "IV too large");
				rte_free(digest);
				free_vector(&vector);
				continue;
			}
			if (strcmp(vector.result, "valid") == 0)
				ret = run_gmac(&vector, RTE_CRYPTO_AUTH_OP_GENERATE,
					&digest, &status);
			else if (strcmp(vector.result, "invalid") == 0)
				ret = run_gmac(&vector, RTE_CRYPTO_AUTH_OP_VERIFY,
					&digest, &status);
			else if (strcmp(vector.result, "acceptable") == 0) {
				stats->skipped_acceptable++;
				debug_vector("SKIP", "AES-GMAC", &vector,
					"acceptable result policy");
				ret = 0;
			} else {
				stats->skipped_unsupported++;
				debug_vector("SKIP", "AES-GMAC", &vector, "unknown result");
				ret = 0;
			}
			if (ret == -ENOTSUP) {
				stats->skipped_capability++;
				debug_vector("SKIP", "AES-GMAC", &vector, "PMD capability");
			} else if (ret == -ETIMEDOUT) {
				return -ETIMEDOUT;
			} else if (ret == -EMSGSIZE) {
				stats->skipped_unsupported++;
				debug_vector("SKIP", "AES-GMAC", &vector,
					"message length exceeds mbuf");
			} else if ((strcmp(vector.result, "valid") == 0 &&
					(ret != 0 || status != RTE_CRYPTO_OP_STATUS_SUCCESS ||
					 (vector.tag_len != 0 &&
					  memcmp(digest, vector.tag, vector.tag_len) != 0))) ||
					(strcmp(vector.result, "invalid") == 0 &&
					(ret != 0 || status != RTE_CRYPTO_OP_STATUS_AUTH_FAILED))) {
				debug_vector("FAIL", "AES-GMAC", &vector,
					"unexpected operation result");
				stats->failed++;
			} else if (strcmp(vector.result, "valid") == 0 ||
					strcmp(vector.result, "invalid") == 0)
				stats->passed++;
			rte_free(digest);
			free_vector(&vector);
		}
	}
	return 0;
}

struct dsa_group {
	uint8_t *p;
	uint8_t *q;
	uint8_t *g;
	uint8_t *y;
	uint32_t p_len;
	uint32_t q_len;
	uint32_t g_len;
	uint32_t y_len;
	enum rte_crypto_auth_algorithm hash;
	uint32_t digest_len;
};

static int
map_sha(const char *name, enum rte_crypto_auth_algorithm *algo, uint32_t *digest_len)
{
	if (strcmp(name, "SHA-1") == 0) {
		*algo = RTE_CRYPTO_AUTH_SHA1;
		*digest_len = 20;
	} else if (strcmp(name, "SHA-224") == 0) {
		*algo = RTE_CRYPTO_AUTH_SHA224;
		*digest_len = 28;
	} else if (strcmp(name, "SHA-256") == 0) {
		*algo = RTE_CRYPTO_AUTH_SHA256;
		*digest_len = 32;
	} else if (strcmp(name, "SHA-384") == 0) {
		*algo = RTE_CRYPTO_AUTH_SHA384;
		*digest_len = 48;
	} else if (strcmp(name, "SHA-512") == 0) {
		*algo = RTE_CRYPTO_AUTH_SHA512;
		*digest_len = 64;
	} else {
		return -ENOTSUP;
	}
	return 0;
}

/* Compute a plain (keyless) message digest through the cryptodev auth path. */
static int
compute_hash(const uint8_t *msg, uint32_t msg_len, enum rte_crypto_auth_algorithm algo,
		uint8_t *digest, uint32_t digest_len)
{
	struct rte_crypto_sym_xform xform = { 0 };
	struct rte_cryptodev_sym_session *session = NULL;
	struct rte_crypto_op *op = NULL;
	struct rte_crypto_op *completed = NULL;
	struct rte_mbuf *mbuf = NULL;
	const struct rte_cryptodev_symmetric_capability *capability;
	struct rte_cryptodev_sym_capability_idx index = {
		.type = RTE_CRYPTO_SYM_XFORM_AUTH,
		.algo.auth = algo,
	};
	uint8_t *op_digest = NULL;
	int ret = -ENOMEM;

	capability = rte_cryptodev_sym_capability_get(env.hash_dev_id, &index);
	if (capability == NULL || rte_cryptodev_sym_capability_check_auth(capability,
			0, digest_len, 0) != 0)
		return -ENOTSUP;

	xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	xform.auth.algo = algo;
	xform.auth.op = RTE_CRYPTO_AUTH_OP_GENERATE;
	xform.auth.digest_length = digest_len;
	session = rte_cryptodev_sym_session_create(env.hash_dev_id, &xform, env.session_pool);
	if (session == NULL)
		goto out;

	op = rte_crypto_op_alloc(env.op_pool, RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	if (op == NULL)
		goto out;
	mbuf = rte_pktmbuf_alloc(env.mbuf_pool);
	if (mbuf == NULL)
		goto out;
	if (msg_len > rte_pktmbuf_tailroom(mbuf)) {
		ret = -EMSGSIZE;
		goto out;
	}
	if (msg_len != 0)
		memcpy(rte_pktmbuf_append(mbuf, msg_len), msg, msg_len);

	op_digest = rte_malloc(NULL, digest_len, RTE_CACHE_LINE_SIZE);
	if (op_digest == NULL)
		goto out;
	op->sym->m_src = mbuf;
	op->sym->auth.data.offset = 0;
	op->sym->auth.data.length = msg_len;
	op->sym->auth.digest.data = op_digest;
	op->sym->auth.digest.phys_addr = rte_malloc_virt2iova(op_digest);
	rte_crypto_op_attach_sym_session(op, session);
	if (rte_cryptodev_enqueue_burst(env.hash_dev_id, 0, &op, 1) != 1) {
		ret = -EIO;
		goto out;
	}
	completed = dequeue_one(env.hash_dev_id);
	if (completed == NULL)
		/* Device wedged: op still owned by the PMD. Abort without freeing
		 * in-flight state; teardown stops the device.
		 */
		return -ETIMEDOUT;
	if (completed->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		ret = -EIO;
		goto out;
	}
	memcpy(digest, op_digest, digest_len);
	ret = 0;

out:
	rte_free(op_digest);
	rte_pktmbuf_free(mbuf);
	rte_crypto_op_free(op);
	rte_cryptodev_sym_session_free(env.hash_dev_id, session);
	return ret;
}

static int
run_dsa_verify(const struct dsa_group *group, const uint8_t *msg, uint32_t msg_len,
		const uint8_t *r, uint32_t r_len, const uint8_t *s, uint32_t s_len,
		enum rte_crypto_op_status *status)
{
	struct rte_crypto_asym_xform xform = { 0 };
	void *session = NULL;
	struct rte_crypto_op *op = NULL;
	struct rte_crypto_op *completed = NULL;
	struct rte_cryptodev_info dev_info;
	struct rte_cryptodev_asym_capability_idx index = {
		.type = RTE_CRYPTO_ASYM_XFORM_DSA,
	};
	const struct rte_cryptodev_asymmetric_xform_capability *capability;
	uint8_t digest[64];
	int ret;

	if (env.asym_session_pool == NULL || env.asym_op_pool == NULL)
		return -ENOTSUP;
	rte_cryptodev_info_get(env.dev_id, &dev_info);
	if ((dev_info.feature_flags & RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO) == 0)
		return -ENOTSUP;
	capability = rte_cryptodev_asym_capability_get(env.dev_id, &index);
	if (capability == NULL || rte_cryptodev_asym_xform_capability_check_optype(capability,
			RTE_CRYPTO_ASYM_OP_VERIFY) == 0)
		return -ENOTSUP;

	ret = compute_hash(msg, msg_len, group->hash, digest, group->digest_len);
	if (ret != 0)
		return ret;

	xform.xform_type = RTE_CRYPTO_ASYM_XFORM_DSA;
	xform.next = NULL;
	xform.dsa.p.data = group->p;
	xform.dsa.p.length = group->p_len;
	xform.dsa.q.data = group->q;
	xform.dsa.q.length = group->q_len;
	xform.dsa.g.data = group->g;
	xform.dsa.g.length = group->g_len;

	/* Coarse capability check cannot confirm every parameter; treat a
	 * session-create failure as unsupported rather than a hard failure.
	 */
	if (rte_cryptodev_asym_session_create(env.dev_id, &xform, env.asym_session_pool,
			&session) < 0 || session == NULL) {
		ret = -ENOTSUP;
		goto out;
	}
	ret = -ENOMEM;
	op = rte_crypto_op_alloc(env.asym_op_pool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (op == NULL)
		goto out;
	op->asym->dsa.op_type = RTE_CRYPTO_ASYM_OP_VERIFY;
	/* DSA verify operates on the message digest. */
	op->asym->dsa.message.data = digest;
	op->asym->dsa.message.length = group->digest_len;
	op->asym->dsa.r.data = (uint8_t *)(uintptr_t)r;
	op->asym->dsa.r.length = r_len;
	op->asym->dsa.s.data = (uint8_t *)(uintptr_t)s;
	op->asym->dsa.s.length = s_len;
	op->asym->dsa.y.data = group->y;
	op->asym->dsa.y.length = group->y_len;
	rte_crypto_op_attach_asym_session(op, session);
	if (rte_cryptodev_enqueue_burst(env.dev_id, 0, &op, 1) != 1) {
		ret = -EIO;
		goto out;
	}
	completed = dequeue_one(env.dev_id);
	if (completed == NULL)
		/* Device wedged: op still owned by the PMD. Abort without freeing
		 * in-flight state; teardown stops the device.
		 */
		return -ETIMEDOUT;
	*status = completed->status;
	ret = 0;

out:
	rte_crypto_op_free(op);
	if (session != NULL)
		rte_cryptodev_asym_session_free(env.dev_id, session);
	return ret;
}

static int
process_dsa_p1363(json_t *root, const char *name, struct app_stats *stats)
{
	json_t *groups = json_object_get(root, "testGroups");
	json_t *group;
	size_t group_index;

	if (!json_is_array(groups))
		return -EINVAL;
	json_array_foreach(groups, group_index, group) {
		json_t *pub = json_object_get(group, "publicKey");
		json_t *tests = json_object_get(group, "tests");
		struct dsa_group dsa = { 0 };
		const char *sha;
		json_t *test;
		size_t test_index;

		if (!json_is_object(pub) || !json_is_array(tests))
			return -EINVAL;
		sha = json_string_value(json_object_get(group, "sha"));
		if (sha == NULL || map_sha(sha, &dsa.hash, &dsa.digest_len) != 0) {
			stats->skipped_unsupported++;
			continue;
		}
		if (decode_hex(json_object_get(pub, "p"), &dsa.p, &dsa.p_len) != 0 ||
				decode_hex(json_object_get(pub, "q"), &dsa.q, &dsa.q_len) != 0 ||
				decode_hex(json_object_get(pub, "g"), &dsa.g, &dsa.g_len) != 0 ||
				decode_hex(json_object_get(pub, "y"), &dsa.y, &dsa.y_len) != 0) {
			rte_free(dsa.p);
			rte_free(dsa.q);
			rte_free(dsa.g);
			rte_free(dsa.y);
			stats->skipped_unsupported++;
			continue;
		}
		json_array_foreach(tests, test_index, test) {
			struct wycheproof_data vector;
			uint8_t *sig = NULL;
			uint32_t sig_len = 0;
			enum rte_crypto_op_status status = RTE_CRYPTO_OP_STATUS_ERROR;
			json_t *tc_id;
			int ret;

			memset(&vector, 0, sizeof(vector));
			tc_id = json_object_get(test, "tcId");
			vector.result = json_string_value(json_object_get(test, "result"));
			if (!json_is_integer(tc_id) || vector.result == NULL) {
				stats->skipped_unsupported++;
				continue;
			}
			vector.tc_id = json_integer_value(tc_id);
			if (decode_hex(json_object_get(test, "msg"), &vector.msg,
					&vector.msg_len) != 0 ||
					decode_hex(json_object_get(test, "sig"),
						&sig, &sig_len) != 0) {
				rte_free(vector.msg);
				rte_free(sig);
				stats->skipped_unsupported++;
				continue;
			}
			if (strcmp(vector.result, "acceptable") == 0) {
				stats->skipped_acceptable++;
				debug_vector("SKIP", name, &vector, "acceptable result policy");
				rte_free(vector.msg);
				rte_free(sig);
				continue;
			}
			if ((sig_len & 1) != 0 || sig_len == 0) {
				debug_vector("SKIP", name, &vector, "malformed P1363 signature");
				stats->skipped_unsupported++;
				rte_free(vector.msg);
				rte_free(sig);
				continue;
			}
			ret = run_dsa_verify(&dsa, vector.msg, vector.msg_len, sig, sig_len / 2,
				sig + sig_len / 2, sig_len / 2, &status);
			if (ret == -ENOTSUP) {
				stats->skipped_capability++;
				debug_vector("SKIP", name, &vector, "PMD capability");
			} else if (ret == -ETIMEDOUT) {
				return -ETIMEDOUT;
			} else if (ret == -EMSGSIZE) {
				stats->skipped_unsupported++;
				debug_vector("SKIP", name, &vector, "message length exceeds mbuf");
			} else if ((strcmp(vector.result, "valid") == 0 &&
					(ret != 0 || status != RTE_CRYPTO_OP_STATUS_SUCCESS)) ||
					(strcmp(vector.result, "invalid") == 0 &&
					(ret != 0 || status == RTE_CRYPTO_OP_STATUS_SUCCESS))) {
				debug_vector("FAIL", name, &vector,
					"unexpected verification result");
				stats->failed++;
			} else if (strcmp(vector.result, "valid") == 0 ||
					strcmp(vector.result, "invalid") == 0) {
				stats->passed++;
			} else {
				stats->skipped_unsupported++;
				debug_vector("SKIP", name, &vector, "unknown result");
			}
			rte_free(vector.msg);
			rte_free(sig);
		}
		rte_free(dsa.p);
		rte_free(dsa.q);
		rte_free(dsa.g);
		rte_free(dsa.y);
	}
	return 0;
}

static int
map_curve(const char *name, enum rte_crypto_curve_id *curve, uint32_t *bytesize)
{
	if (strcmp(name, "secp224r1") == 0) {
		*curve = RTE_CRYPTO_EC_GROUP_SECP224R1;
		*bytesize = 28;
	} else if (strcmp(name, "secp256r1") == 0) {
		*curve = RTE_CRYPTO_EC_GROUP_SECP256R1;
		*bytesize = 32;
	} else if (strcmp(name, "secp384r1") == 0) {
		*curve = RTE_CRYPTO_EC_GROUP_SECP384R1;
		*bytesize = 48;
	} else if (strcmp(name, "secp521r1") == 0) {
		*curve = RTE_CRYPTO_EC_GROUP_SECP521R1;
		*bytesize = 66;
	} else {
		return -ENOTSUP;
	}
	return 0;
}

/* Left-align an unsigned big-endian value into a fixed-width field. */
static bool
value_equals_padded(const uint8_t *a, uint32_t a_len, const uint8_t *b, uint32_t b_len)
{
	while (a_len > 0 && a[0] == 0) {
		a++;
		a_len--;
	}
	while (b_len > 0 && b[0] == 0) {
		b++;
		b_len--;
	}
	return a_len == b_len && memcmp(a, b, a_len) == 0;
}

/* Left-pad a big-endian value into a fixed-width field: strip leading zeros,
 * reject values wider than the field, zero-pad the remaining prefix.
 */
static int
normalize_operand(const uint8_t *value, uint32_t value_len, uint8_t *out, uint32_t bytesize)
{
	while (value_len > 0 && value[0] == 0) {
		value++;
		value_len--;
	}
	if (value_len > bytesize)
		return -EINVAL;
	memset(out, 0, bytesize);
	memcpy(out + bytesize - value_len, value, value_len);
	return 0;
}

static int
run_ecdh_ecpoint(enum rte_crypto_curve_id curve, uint32_t bytesize,
		const uint8_t *priv, uint32_t priv_len, const uint8_t *pub_x,
		const uint8_t *pub_y, uint8_t **shared_x, uint32_t *shared_len,
		enum rte_crypto_op_status *status)
{
	struct rte_crypto_asym_xform xform = { 0 };
	void *session = NULL;
	struct rte_crypto_op *op = NULL;
	struct rte_crypto_op *completed = NULL;
	struct rte_cryptodev_info dev_info;
	struct rte_cryptodev_asym_capability_idx index = {
		.type = RTE_CRYPTO_ASYM_XFORM_ECDH,
	};
	const struct rte_cryptodev_asymmetric_xform_capability *capability;
	uint8_t *out_x = NULL;
	uint8_t *out_y = NULL;
	uint8_t priv_buf[MAX_EC_BYTESIZE];
	int ret;

	*shared_x = NULL;
	if (env.asym_session_pool == NULL || env.asym_op_pool == NULL)
		return -ENOTSUP;
	rte_cryptodev_info_get(env.dev_id, &dev_info);
	if ((dev_info.feature_flags & RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO) == 0)
		return -ENOTSUP;
	capability = rte_cryptodev_asym_capability_get(env.dev_id, &index);
	if (capability == NULL)
		return -ENOTSUP;
	if (rte_cryptodev_asym_xform_capability_check_optype(capability,
			(enum rte_crypto_asym_op_type)
			RTE_CRYPTO_ASYM_KE_SHARED_SECRET_COMPUTE) == 0)
		return -ENOTSUP;

	if (bytesize > sizeof(priv_buf))
		return -ENOTSUP;
	if (normalize_operand(priv, priv_len, priv_buf, bytesize) != 0)
		return -EINVAL;

	xform.xform_type = RTE_CRYPTO_ASYM_XFORM_ECDH;
	xform.next = NULL;
	xform.ec.curve_id = curve;
	xform.ec.pkey.data = priv_buf;
	xform.ec.pkey.length = bytesize;
	xform.ec.q.x.data = (uint8_t *)(uintptr_t)pub_x;
	xform.ec.q.x.length = bytesize;
	xform.ec.q.y.data = (uint8_t *)(uintptr_t)pub_y;
	xform.ec.q.y.length = bytesize;

	ret = -ENOMEM;
	out_x = rte_zmalloc(NULL, bytesize, 0);
	out_y = rte_zmalloc(NULL, bytesize, 0);
	if (out_x == NULL || out_y == NULL)
		goto out;
	/* Coarse capability check cannot confirm curve support; treat a
	 * session-create failure as unsupported rather than a hard failure.
	 */
	if (rte_cryptodev_asym_session_create(env.dev_id, &xform, env.asym_session_pool,
			&session) < 0 || session == NULL) {
		ret = -ENOTSUP;
		goto out;
	}
	op = rte_crypto_op_alloc(env.asym_op_pool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (op == NULL)
		goto out;
	op->asym->ecdh.ke_type = RTE_CRYPTO_ASYM_KE_SHARED_SECRET_COMPUTE;
	op->asym->ecdh.priv_key.data = priv_buf;
	op->asym->ecdh.priv_key.length = bytesize;
	op->asym->ecdh.pub_key.x.data = (uint8_t *)(uintptr_t)pub_x;
	op->asym->ecdh.pub_key.x.length = bytesize;
	op->asym->ecdh.pub_key.y.data = (uint8_t *)(uintptr_t)pub_y;
	op->asym->ecdh.pub_key.y.length = bytesize;
	op->asym->ecdh.shared_secret.x.data = out_x;
	op->asym->ecdh.shared_secret.x.length = bytesize;
	op->asym->ecdh.shared_secret.y.data = out_y;
	op->asym->ecdh.shared_secret.y.length = bytesize;
	rte_crypto_op_attach_asym_session(op, session);
	if (rte_cryptodev_enqueue_burst(env.dev_id, 0, &op, 1) != 1) {
		ret = -EIO;
		goto out;
	}
	completed = dequeue_one(env.dev_id);
	if (completed == NULL)
		/* Device wedged: op still owned by the PMD. Abort without freeing
		 * in-flight state; teardown stops the device.
		 */
		return -ETIMEDOUT;
	*status = completed->status;
	if (completed->status == RTE_CRYPTO_OP_STATUS_SUCCESS) {
		*shared_x = out_x;
		*shared_len = completed->asym->ecdh.shared_secret.x.length;
		out_x = NULL;
	}
	ret = 0;

out:
	/* The private key was copied into the stack buffer; wipe it. */
	rte_memzero_explicit(priv_buf, sizeof(priv_buf));
	rte_free(out_x);
	rte_free(out_y);
	rte_crypto_op_free(op);
	if (session != NULL)
		rte_cryptodev_asym_session_free(env.dev_id, session);
	return ret;
}

static int
process_ecdh_ecpoint(json_t *root, const char *name, struct app_stats *stats)
{
	json_t *groups = json_object_get(root, "testGroups");
	json_t *group;
	size_t group_index;

	if (!json_is_array(groups))
		return -EINVAL;
	json_array_foreach(groups, group_index, group) {
		json_t *tests = json_object_get(group, "tests");
		enum rte_crypto_curve_id curve;
		uint32_t bytesize;
		const char *curve_name;
		json_t *test;
		size_t test_index;

		if (!json_is_array(tests))
			return -EINVAL;
		curve_name = json_string_value(json_object_get(group, "curve"));
		if (curve_name == NULL || map_curve(curve_name, &curve, &bytesize) != 0) {
			stats->skipped_unsupported++;
			continue;
		}
		json_array_foreach(tests, test_index, test) {
			struct wycheproof_data vector;
			uint8_t *pub = NULL;
			uint8_t *priv = NULL;
			uint8_t *shared = NULL;
			uint8_t *result_x = NULL;
			uint32_t pub_len = 0, priv_len = 0, shared_len = 0, result_len = 0;
			enum rte_crypto_op_status status = RTE_CRYPTO_OP_STATUS_ERROR;
			bool matched;
			json_t *tc_id;
			int ret;

			memset(&vector, 0, sizeof(vector));
			tc_id = json_object_get(test, "tcId");
			vector.result = json_string_value(json_object_get(test, "result"));
			if (!json_is_integer(tc_id) || vector.result == NULL) {
				stats->skipped_unsupported++;
				continue;
			}
			vector.tc_id = json_integer_value(tc_id);
			if (decode_hex(json_object_get(test, "public"), &pub, &pub_len) != 0 ||
					decode_hex(json_object_get(test, "private"), &priv,
						&priv_len) != 0 ||
					decode_hex(json_object_get(test, "shared"), &shared,
						&shared_len) != 0) {
				rte_free(pub);
				rte_free_sensitive(priv);
				rte_free(shared);
				stats->skipped_unsupported++;
				continue;
			}
			if (strcmp(vector.result, "acceptable") == 0) {
				stats->skipped_acceptable++;
				debug_vector("SKIP", name, &vector, "acceptable result policy");
				goto next;
			}
			/* Only uncompressed points (0x04 || x || y) map to the DPDK EC point. */
			if (pub_len != 1 + 2 * bytesize || pub[0] != 0x04) {
				debug_vector("SKIP", name, &vector,
					"non-uncompressed public point");
				stats->skipped_unsupported++;
				goto next;
			}
			/* Invalid-curve rejection cases carry no expected shared secret;
			 * on-curve validation is a separate ECDH PUB_KEY_VERIFY op, not
			 * part of the raw shared-secret compute primitive.
			 */
			if (shared_len == 0) {
				debug_vector("SKIP", name, &vector,
					"no shared secret (point validation out of scope)");
				stats->skipped_unsupported++;
				goto next;
			}
			ret = run_ecdh_ecpoint(curve, bytesize, priv, priv_len, pub + 1,
				pub + 1 + bytesize, &result_x, &result_len, &status);
			if (ret == -ENOTSUP) {
				stats->skipped_capability++;
				debug_vector("SKIP", name, &vector, "PMD capability");
				goto next;
			}
			if (ret == -ETIMEDOUT)
				return -ETIMEDOUT;
			if (ret == -EINVAL) {
				stats->skipped_unsupported++;
				debug_vector("SKIP", name, &vector, "operand wider than curve");
				goto next;
			}
			if (ret != 0) {
				debug_vector("FAIL", name, &vector, "shared-secret computation failed");
				stats->failed++;
				goto next;
			}
			matched = ret == 0 && status == RTE_CRYPTO_OP_STATUS_SUCCESS &&
				value_equals_padded(result_x, result_len, shared, shared_len);
			if (strcmp(vector.result, "valid") == 0) {
				if (ret == 0 && status == RTE_CRYPTO_OP_STATUS_SUCCESS && matched) {
					stats->passed++;
				} else if (ret == 0 && status == RTE_CRYPTO_OP_STATUS_SUCCESS) {
					debug_vector("FAIL", name, &vector, "wrong shared secret");
					stats->failed++;
				} else {
					/* PMD returned an error for a valid vector. */
					debug_vector("FAIL", name, &vector,
						"PMD could not compute");
					stats->failed++;
				}
			} else if (strcmp(vector.result, "invalid") == 0) {
				if (matched) {
					debug_vector("FAIL", name, &vector,
						"produced forbidden secret");
					stats->failed++;
				} else {
					stats->passed++;
				}
			} else {
				stats->skipped_unsupported++;
				debug_vector("SKIP", name, &vector, "unknown result");
			}
next:
			rte_free(result_x);
			rte_free(pub);
			rte_free_sensitive(priv);
			rte_free(shared);
		}
	}
	return 0;
}

struct ecdsa_group {
	uint8_t *wx;
	uint8_t *wy;
	uint32_t wx_len;
	uint32_t wy_len;
	enum rte_crypto_curve_id curve;
	uint32_t bytesize;
	enum rte_crypto_auth_algorithm hash;
	uint32_t digest_len;
};

static int
run_ecdsa_verify(const struct ecdsa_group *group, const uint8_t *msg, uint32_t msg_len,
		const uint8_t *r, uint32_t r_len, const uint8_t *s, uint32_t s_len,
		enum rte_crypto_op_status *status)
{
	struct rte_crypto_asym_xform xform = { 0 };
	void *session = NULL;
	struct rte_crypto_op *op = NULL;
	struct rte_crypto_op *completed = NULL;
	struct rte_cryptodev_info dev_info;
	struct rte_cryptodev_asym_capability_idx index = {
		.type = RTE_CRYPTO_ASYM_XFORM_ECDSA,
	};
	const struct rte_cryptodev_asymmetric_xform_capability *capability;
	uint8_t digest[64];
	uint8_t wx_buf[MAX_EC_BYTESIZE];
	uint8_t wy_buf[MAX_EC_BYTESIZE];
	uint32_t e_len;
	int ret;

	if (env.asym_session_pool == NULL || env.asym_op_pool == NULL)
		return -ENOTSUP;
	rte_cryptodev_info_get(env.dev_id, &dev_info);
	if ((dev_info.feature_flags & RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO) == 0)
		return -ENOTSUP;
	capability = rte_cryptodev_asym_capability_get(env.dev_id, &index);
	if (capability == NULL || rte_cryptodev_asym_xform_capability_check_optype(capability,
			RTE_CRYPTO_ASYM_OP_VERIFY) == 0)
		return -ENOTSUP;

	ret = compute_hash(msg, msg_len, group->hash, digest, group->digest_len);
	if (ret != 0)
		return ret;
	/* ECDSA uses e = leftmost Ln bits of the hash (byte-aligned truncation). */
	e_len = group->digest_len > group->bytesize ? group->bytesize : group->digest_len;

	if (group->bytesize > sizeof(wx_buf))
		return -ENOTSUP;
	if (normalize_operand(group->wx, group->wx_len, wx_buf, group->bytesize) != 0 ||
			normalize_operand(group->wy, group->wy_len, wy_buf, group->bytesize) != 0)
		return -EINVAL;

	xform.xform_type = RTE_CRYPTO_ASYM_XFORM_ECDSA;
	xform.next = NULL;
	xform.ec.curve_id = group->curve;
	xform.ec.q.x.data = wx_buf;
	xform.ec.q.x.length = group->bytesize;
	xform.ec.q.y.data = wy_buf;
	xform.ec.q.y.length = group->bytesize;

	/* Coarse capability check cannot confirm curve support; treat a
	 * session-create failure as unsupported rather than a hard failure.
	 */
	if (rte_cryptodev_asym_session_create(env.dev_id, &xform, env.asym_session_pool,
			&session) < 0 || session == NULL) {
		ret = -ENOTSUP;
		goto out;
	}
	ret = -ENOMEM;
	op = rte_crypto_op_alloc(env.asym_op_pool, RTE_CRYPTO_OP_TYPE_ASYMMETRIC);
	if (op == NULL)
		goto out;
	op->asym->ecdsa.op_type = RTE_CRYPTO_ASYM_OP_VERIFY;
	op->asym->ecdsa.message.data = digest;
	op->asym->ecdsa.message.length = e_len;
	op->asym->ecdsa.r.data = (uint8_t *)(uintptr_t)r;
	op->asym->ecdsa.r.length = r_len;
	op->asym->ecdsa.s.data = (uint8_t *)(uintptr_t)s;
	op->asym->ecdsa.s.length = s_len;
	rte_crypto_op_attach_asym_session(op, session);
	if (rte_cryptodev_enqueue_burst(env.dev_id, 0, &op, 1) != 1) {
		ret = -EIO;
		goto out;
	}
	completed = dequeue_one(env.dev_id);
	if (completed == NULL)
		/* Device wedged: op still owned by the PMD. Abort without freeing
		 * in-flight state; teardown stops the device.
		 */
		return -ETIMEDOUT;
	*status = completed->status;
	ret = 0;

out:
	rte_crypto_op_free(op);
	if (session != NULL)
		rte_cryptodev_asym_session_free(env.dev_id, session);
	return ret;
}

static int
process_ecdsa_p1363(json_t *root, const char *name, struct app_stats *stats)
{
	json_t *groups = json_object_get(root, "testGroups");
	json_t *group;
	size_t group_index;

	if (!json_is_array(groups))
		return -EINVAL;
	json_array_foreach(groups, group_index, group) {
		json_t *pub = json_object_get(group, "publicKey");
		json_t *tests = json_object_get(group, "tests");
		struct ecdsa_group ec = { 0 };
		const char *sha;
		const char *curve_name;
		json_t *test;
		size_t test_index;

		if (!json_is_object(pub) || !json_is_array(tests))
			return -EINVAL;
		sha = json_string_value(json_object_get(group, "sha"));
		curve_name = json_string_value(json_object_get(pub, "curve"));
		if (sha == NULL || curve_name == NULL ||
				map_sha(sha, &ec.hash, &ec.digest_len) != 0 ||
				map_curve(curve_name, &ec.curve, &ec.bytesize) != 0) {
			stats->skipped_unsupported++;
			continue;
		}
		if (decode_hex(json_object_get(pub, "wx"), &ec.wx, &ec.wx_len) != 0 ||
				decode_hex(json_object_get(pub, "wy"), &ec.wy, &ec.wy_len) != 0) {
			rte_free(ec.wx);
			rte_free(ec.wy);
			stats->skipped_unsupported++;
			continue;
		}
		json_array_foreach(tests, test_index, test) {
			struct wycheproof_data vector;
			uint8_t *sig = NULL;
			uint32_t sig_len = 0;
			enum rte_crypto_op_status status = RTE_CRYPTO_OP_STATUS_ERROR;
			json_t *tc_id;
			int ret;

			memset(&vector, 0, sizeof(vector));
			tc_id = json_object_get(test, "tcId");
			vector.result = json_string_value(json_object_get(test, "result"));
			if (!json_is_integer(tc_id) || vector.result == NULL) {
				stats->skipped_unsupported++;
				continue;
			}
			vector.tc_id = json_integer_value(tc_id);
			if (decode_hex(json_object_get(test, "msg"), &vector.msg,
					&vector.msg_len) != 0 ||
					decode_hex(json_object_get(test, "sig"),
						&sig, &sig_len) != 0) {
				rte_free(vector.msg);
				rte_free(sig);
				stats->skipped_unsupported++;
				continue;
			}
			if (strcmp(vector.result, "acceptable") == 0) {
				stats->skipped_acceptable++;
				debug_vector("SKIP", name, &vector, "acceptable result policy");
				rte_free(vector.msg);
				rte_free(sig);
				continue;
			}
			if (sig_len != 2 * ec.bytesize) {
				debug_vector("SKIP", name, &vector,
					"non-canonical P1363 signature size");
				stats->skipped_unsupported++;
				rte_free(vector.msg);
				rte_free(sig);
				continue;
			}
			ret = run_ecdsa_verify(&ec, vector.msg, vector.msg_len, sig, sig_len / 2,
				sig + sig_len / 2, sig_len / 2, &status);
			if (ret == -ENOTSUP) {
				stats->skipped_capability++;
				debug_vector("SKIP", name, &vector, "PMD capability");
			} else if (ret == -ETIMEDOUT) {
				return -ETIMEDOUT;
			} else if (ret == -EMSGSIZE) {
				stats->skipped_unsupported++;
				debug_vector("SKIP", name, &vector, "message length exceeds mbuf");
			} else if (ret == -EINVAL) {
				stats->skipped_unsupported++;
				debug_vector("SKIP", name, &vector, "operand wider than curve");
			} else if (ret != 0) {
				debug_vector("FAIL", name, &vector, "signature verification failed");
				stats->failed++;
			} else if (strcmp(vector.result, "valid") == 0) {
				if (ret == 0 && status == RTE_CRYPTO_OP_STATUS_SUCCESS)
					stats->passed++;
				else {
					debug_vector("FAIL", name, &vector,
						"valid signature rejected");
					stats->failed++;
				}
			} else if (strcmp(vector.result, "invalid") == 0) {
				if (ret == 0 && status == RTE_CRYPTO_OP_STATUS_SUCCESS) {
					debug_vector("FAIL", name, &vector,
						"invalid signature accepted");
					stats->failed++;
				} else {
					stats->passed++;
				}
			} else {
				stats->skipped_unsupported++;
				debug_vector("SKIP", name, &vector, "unknown result");
			}
			rte_free(vector.msg);
			rte_free(sig);
		}
		rte_free(ec.wx);
		rte_free(ec.wy);
	}
	return 0;
}

static int
process_file(const char *path, struct app_stats *stats)
{
	json_error_t error;
	json_t *root;
	const char *algorithm;
	const char *schema;
	int ret;

	root = json_load_file(path, 0, &error);
	if (root == NULL) {
		printf("Cannot load %s: JSON error at line %d: %s\n", path, error.line,
			error.text);
		return -EINVAL;
	}
	algorithm = json_string_value(json_object_get(root, "algorithm"));
	schema = json_string_value(json_object_get(root, "schema"));
	if (algorithm == NULL || schema == NULL) {
		debug_file("SKIP", path, "missing algorithm or schema");
		stats->skipped_unsupported++;
		json_decref(root);
		return 0;
	}
	if (strcmp(algorithm, "AES-GCM") == 0 &&
			strcmp(schema, "aead_test_schema_v1.json") == 0) {
		printf("Processing AES-GCM vectors: %s\n", path);
		ret = process_aead(root, RTE_CRYPTO_AEAD_AES_GCM, "AES-GCM", stats);
	} else if (strcmp(algorithm, "AES-CCM") == 0 &&
			strcmp(schema, "aead_test_schema_v1.json") == 0) {
		printf("Processing AES-CCM vectors: %s\n", path);
		ret = process_aead(root, RTE_CRYPTO_AEAD_AES_CCM, "AES-CCM", stats);
	} else if (strcmp(algorithm, "CHACHA20-POLY1305") == 0 &&
			strcmp(schema, "aead_test_schema_v1.json") == 0) {
		printf("Processing ChaCha20-Poly1305 vectors: %s\n", path);
		ret = process_aead(root, RTE_CRYPTO_AEAD_CHACHA20_POLY1305,
			"ChaCha20-Poly1305", stats);
	} else if (strcmp(algorithm, "SM4-GCM") == 0 &&
			strcmp(schema, "aead_test_schema_v1.json") == 0) {
		printf("Processing SM4-GCM vectors: %s\n", path);
		ret = process_aead(root, RTE_CRYPTO_AEAD_SM4_GCM, "SM4-GCM", stats);
	} else if (strcmp(algorithm, "HMACSHA256") == 0 &&
			strcmp(schema, "mac_test_schema_v1.json") == 0) {
		printf("Processing HMAC-SHA256 vectors: %s\n", path);
		ret = process_hmac(root, RTE_CRYPTO_AUTH_SHA256_HMAC, "HMAC-SHA256", stats);
	} else if (strcmp(algorithm, "HMACSHA1") == 0 &&
			strcmp(schema, "mac_test_schema_v1.json") == 0) {
		printf("Processing HMAC-SHA1 vectors: %s\n", path);
		ret = process_hmac(root, RTE_CRYPTO_AUTH_SHA1_HMAC, "HMAC-SHA1", stats);
	} else if (strcmp(algorithm, "HMACSHA224") == 0 &&
			strcmp(schema, "mac_test_schema_v1.json") == 0) {
		printf("Processing HMAC-SHA224 vectors: %s\n", path);
		ret = process_hmac(root, RTE_CRYPTO_AUTH_SHA224_HMAC, "HMAC-SHA224", stats);
	} else if (strcmp(algorithm, "HMACSHA384") == 0 &&
			strcmp(schema, "mac_test_schema_v1.json") == 0) {
		printf("Processing HMAC-SHA384 vectors: %s\n", path);
		ret = process_hmac(root, RTE_CRYPTO_AUTH_SHA384_HMAC, "HMAC-SHA384", stats);
	} else if (strcmp(algorithm, "HMACSHA512") == 0 &&
			strcmp(schema, "mac_test_schema_v1.json") == 0) {
		printf("Processing HMAC-SHA512 vectors: %s\n", path);
		ret = process_hmac(root, RTE_CRYPTO_AUTH_SHA512_HMAC, "HMAC-SHA512", stats);
	} else if (strcmp(algorithm, "HMACSHA3-224") == 0 &&
			strcmp(schema, "mac_test_schema_v1.json") == 0) {
		printf("Processing HMAC-SHA3-224 vectors: %s\n", path);
		ret = process_hmac(root, RTE_CRYPTO_AUTH_SHA3_224_HMAC, "HMAC-SHA3-224", stats);
	} else if (strcmp(algorithm, "HMACSHA3-256") == 0 &&
			strcmp(schema, "mac_test_schema_v1.json") == 0) {
		printf("Processing HMAC-SHA3-256 vectors: %s\n", path);
		ret = process_hmac(root, RTE_CRYPTO_AUTH_SHA3_256_HMAC, "HMAC-SHA3-256", stats);
	} else if (strcmp(algorithm, "HMACSHA3-384") == 0 &&
			strcmp(schema, "mac_test_schema_v1.json") == 0) {
		printf("Processing HMAC-SHA3-384 vectors: %s\n", path);
		ret = process_hmac(root, RTE_CRYPTO_AUTH_SHA3_384_HMAC, "HMAC-SHA3-384", stats);
	} else if (strcmp(algorithm, "HMACSHA3-512") == 0 &&
			strcmp(schema, "mac_test_schema_v1.json") == 0) {
		printf("Processing HMAC-SHA3-512 vectors: %s\n", path);
		ret = process_hmac(root, RTE_CRYPTO_AUTH_SHA3_512_HMAC, "HMAC-SHA3-512", stats);
	} else if (strcmp(algorithm, "HMACSM3") == 0 &&
			strcmp(schema, "mac_test_schema_v1.json") == 0) {
		printf("Processing HMAC-SM3 vectors: %s\n", path);
		ret = process_hmac(root, RTE_CRYPTO_AUTH_SM3_HMAC, "HMAC-SM3", stats);
	} else if (strcmp(algorithm, "AES-CMAC") == 0 &&
			strcmp(schema, "mac_test_schema_v1.json") == 0) {
		printf("Processing AES-CMAC vectors: %s\n", path);
		ret = process_hmac(root, RTE_CRYPTO_AUTH_AES_CMAC, "AES-CMAC", stats);
	} else if (strcmp(algorithm, "AES-GMAC") == 0 &&
			strcmp(schema, "mac_with_iv_test_schema_v1.json") == 0) {
		printf("Processing AES-GMAC vectors: %s\n", path);
		ret = process_gmac(root, stats);
	} else if (strcmp(algorithm, "DSA") == 0 &&
			strcmp(schema, "dsa_p1363_verify_schema_v1.json") == 0) {
		printf("Processing DSA (P1363) vectors: %s\n", path);
		ret = process_dsa_p1363(root, "DSA", stats);
	} else if (strcmp(algorithm, "ECDH") == 0 &&
			strcmp(schema, "ecdh_ecpoint_test_schema_v1.json") == 0) {
		printf("Processing ECDH (ecpoint) vectors: %s\n", path);
		ret = process_ecdh_ecpoint(root, "ECDH", stats);
	} else if (strcmp(algorithm, "ECDSA") == 0 &&
			strcmp(schema, "ecdsa_p1363_verify_schema_v1.json") == 0) {
		printf("Processing ECDSA (P1363) vectors: %s\n", path);
		ret = process_ecdsa_p1363(root, "ECDSA", stats);
	} else if (strncmp(algorithm, "AES-", strlen("AES-")) == 0 ||
			strncmp(algorithm, "SEED-", strlen("SEED-")) == 0 ||
			strcmp(algorithm, "SM4-CCM") == 0 ||
			strncmp(algorithm, "HMAC", strlen("HMAC")) == 0 ||
			strncmp(algorithm, "RSA", strlen("RSA")) == 0 ||
			strcmp(algorithm, "DSA") == 0 ||
			strcmp(algorithm, "ECDH") == 0 ||
			strcmp(algorithm, "ECDSA") == 0) {
		debug_file("SKIP", path, algorithm);
		stats->skipped_unsupported++;
		ret = 0;
	} else {
		debug_file("SKIP", path, "unsupported vector file");
		stats->skipped_unsupported++;
		ret = 0;
	}
	json_decref(root);
	return ret;
}

static int
process_path(const char *path, struct app_stats *stats)
{
	struct stat path_stat;
	DIR *directory;
	struct dirent *entry;
	int ret = 0;

	if (stat(path, &path_stat) != 0)
		return -errno;
	if (!S_ISDIR(path_stat.st_mode))
		return process_file(path, stats);

	directory = opendir(path);
	if (directory == NULL)
		return -errno;
	while ((entry = readdir(directory)) != NULL) {
		char file_path[PATH_MAX];
		size_t name_length = strlen(entry->d_name);

		if (name_length < 6 || strcmp(entry->d_name + name_length - 5, ".json") != 0)
			continue;
		if (snprintf(file_path, sizeof(file_path), "%s/%s", path, entry->d_name) >=
				(int)sizeof(file_path)) {
			ret = -ENAMETOOLONG;
			break;
		}
		ret = process_file(file_path, stats);
		if (ret != 0)
			break;
	}
	closedir(directory);
	return ret;
}

int
main(int argc, char **argv)
{
	struct app_stats stats = { 0 };
	const char *prgname = argv[0];
	int eal_args;
	int ret;

	eal_args = rte_eal_init(argc, argv);
	if (eal_args < 0)
		rte_exit(EXIT_FAILURE, "Cannot initialize EAL\n");
	argc -= eal_args;
	argv += eal_args;

	ret = parse_args(argc, argv);
	if (ret != 0) {
		usage(prgname);
		ret = EXIT_FAILURE;
		goto out_eal;
	}
	ret = app_init();
	if (ret != 0) {
		printf("Cannot initialize cryptodev %u: %s\n", env.dev_id, rte_strerror(-ret));
		ret = EXIT_FAILURE;
		goto out_eal;
	}

	ret = process_path(env.vectors_path, &stats);
	if (ret != 0)
		printf("Vector processing failed: %s\n", rte_strerror(-ret));
	printf("Summary: passed=%" PRIu64 " failed=%" PRIu64
		" skipped_capability=%" PRIu64 " skipped_acceptable=%" PRIu64
		" skipped_unsupported=%" PRIu64 "\n", stats.passed, stats.failed,
		stats.skipped_capability, stats.skipped_acceptable, stats.skipped_unsupported);
	app_uninit();
	ret = ret == 0 && stats.failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;

out_eal:
	rte_eal_cleanup();
	return ret;
}
