/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */
#include <rte_malloc.h>
#include <rte_hash.h>
#include <rte_jhash.h>
#include <rte_mbuf.h>
#include <rte_cryptodev.h>

#include "rte_vhost_crypto.h"
#include "vhost.h"
#include "vhost_user.h"
#include "virtio_crypto.h"

#define INHDR_LEN		(sizeof(struct virtio_crypto_inhdr))
#define IV_OFFSET		(sizeof(struct rte_crypto_op) + \
				sizeof(struct rte_crypto_sym_op))

#ifdef RTE_LIBRTE_VHOST_DEBUG
#define VC_LOG_ERR(fmt, args...)				\
	RTE_LOG(ERR, USER1, "[%s] %s() line %u: " fmt "\n",	\
		"Vhost-Crypto",	__func__, __LINE__, ## args)
#define VC_LOG_INFO(fmt, args...)				\
	RTE_LOG(INFO, USER1, "[%s] %s() line %u: " fmt "\n",	\
		"Vhost-Crypto",	__func__, __LINE__, ## args)

#define VC_LOG_DBG(fmt, args...)				\
	RTE_LOG(DEBUG, USER1, "[%s] %s() line %u: " fmt "\n",	\
		"Vhost-Crypto",	__func__, __LINE__, ## args)
#else
#define VC_LOG_ERR(fmt, args...)				\
	RTE_LOG(ERR, USER1, "[VHOST-Crypto]: " fmt "\n", ## args)
#define VC_LOG_INFO(fmt, args...)				\
	RTE_LOG(INFO, USER1, "[VHOST-Crypto]: " fmt "\n", ## args)
#define VC_LOG_DBG(fmt, args...)
#endif

#define VIRTIO_CRYPTO_FEATURES ((1 << VIRTIO_F_NOTIFY_ON_EMPTY) |	\
		(1 << VIRTIO_RING_F_INDIRECT_DESC) |			\
		(1 << VIRTIO_RING_F_EVENT_IDX) |			\
		(1 << VIRTIO_CRYPTO_SERVICE_CIPHER) |			\
		(1 << VIRTIO_CRYPTO_SERVICE_MAC) |			\
		(1 << VIRTIO_NET_F_CTRL_VQ) |				\
		(1 << VHOST_USER_PROTOCOL_F_CONFIG))

#define IOVA_TO_VVA(t, r, a, l, p)					\
	((t)(uintptr_t)vhost_iova_to_vva(r->dev, r->vq, a, l, p))

static int
cipher_algo_transform(uint32_t virtio_cipher_algo,
		enum rte_crypto_cipher_algorithm *algo)
{
	switch (virtio_cipher_algo) {
	case VIRTIO_CRYPTO_CIPHER_AES_CBC:
		*algo = RTE_CRYPTO_CIPHER_AES_CBC;
		break;
	case VIRTIO_CRYPTO_CIPHER_AES_CTR:
		*algo = RTE_CRYPTO_CIPHER_AES_CTR;
		break;
	case VIRTIO_CRYPTO_CIPHER_DES_ECB:
		*algo = -VIRTIO_CRYPTO_NOTSUPP;
		break;
	case VIRTIO_CRYPTO_CIPHER_DES_CBC:
		*algo = RTE_CRYPTO_CIPHER_DES_CBC;
		break;
	case VIRTIO_CRYPTO_CIPHER_3DES_ECB:
		*algo = RTE_CRYPTO_CIPHER_3DES_ECB;
		break;
	case VIRTIO_CRYPTO_CIPHER_3DES_CBC:
		*algo = RTE_CRYPTO_CIPHER_3DES_CBC;
		break;
	case VIRTIO_CRYPTO_CIPHER_3DES_CTR:
		*algo = RTE_CRYPTO_CIPHER_3DES_CTR;
		break;
	case VIRTIO_CRYPTO_CIPHER_KASUMI_F8:
		*algo = RTE_CRYPTO_CIPHER_KASUMI_F8;
		break;
	case VIRTIO_CRYPTO_CIPHER_SNOW3G_UEA2:
		*algo = RTE_CRYPTO_CIPHER_SNOW3G_UEA2;
		break;
	case VIRTIO_CRYPTO_CIPHER_AES_F8:
		*algo = RTE_CRYPTO_CIPHER_AES_F8;
		break;
	case VIRTIO_CRYPTO_CIPHER_AES_XTS:
		*algo = RTE_CRYPTO_CIPHER_AES_XTS;
		break;
	case VIRTIO_CRYPTO_CIPHER_ZUC_EEA3:
		*algo = RTE_CRYPTO_CIPHER_ZUC_EEA3;
		break;
	default:
		return -VIRTIO_CRYPTO_BADMSG;
		break;
	}

	return 0;
}

static int
auth_algo_transform(uint32_t virtio_auth_algo,
		enum rte_crypto_auth_algorithm *algo)
{
	switch (virtio_auth_algo) {
	case VIRTIO_CRYPTO_NO_MAC:
		*algo = RTE_CRYPTO_AUTH_NULL;
		break;
	case VIRTIO_CRYPTO_MAC_HMAC_MD5:
		*algo = RTE_CRYPTO_AUTH_MD5_HMAC;
		break;
	case VIRTIO_CRYPTO_MAC_HMAC_SHA1:
		*algo = RTE_CRYPTO_AUTH_SHA1_HMAC;
		break;
	case VIRTIO_CRYPTO_MAC_HMAC_SHA_224:
		*algo = RTE_CRYPTO_AUTH_SHA224_HMAC;
		break;
	case VIRTIO_CRYPTO_MAC_HMAC_SHA_256:
		*algo = RTE_CRYPTO_AUTH_SHA256_HMAC;
		break;
	case VIRTIO_CRYPTO_MAC_HMAC_SHA_384:
		*algo = RTE_CRYPTO_AUTH_SHA384_HMAC;
		break;
	case VIRTIO_CRYPTO_MAC_HMAC_SHA_512:
		*algo = RTE_CRYPTO_AUTH_SHA512_HMAC;
		break;
	case VIRTIO_CRYPTO_MAC_CMAC_AES:
		*algo = RTE_CRYPTO_AUTH_AES_CMAC;
		break;
	case VIRTIO_CRYPTO_MAC_KASUMI_F9:
		*algo = RTE_CRYPTO_AUTH_KASUMI_F9;
		break;
	case VIRTIO_CRYPTO_MAC_SNOW3G_UIA2:
		*algo = RTE_CRYPTO_AUTH_SNOW3G_UIA2;
		break;
	case VIRTIO_CRYPTO_MAC_GMAC_AES:
		*algo = RTE_CRYPTO_AUTH_AES_GMAC;
		break;
	case VIRTIO_CRYPTO_MAC_CBCMAC_AES:
		*algo = RTE_CRYPTO_AUTH_AES_CBC_MAC;
		break;
	case VIRTIO_CRYPTO_MAC_XCBC_AES:
		*algo = RTE_CRYPTO_AUTH_AES_XCBC_MAC;
		break;
	case VIRTIO_CRYPTO_MAC_CMAC_3DES:
	case VIRTIO_CRYPTO_MAC_GMAC_TWOFISH:
	case VIRTIO_CRYPTO_MAC_CBCMAC_KASUMI_F9:
		return -VIRTIO_CRYPTO_NOTSUPP;
	default:
		return -VIRTIO_CRYPTO_BADMSG;
	}

	return 0;
}

static int get_iv_len(enum rte_crypto_cipher_algorithm algo)
{
	int len;

	switch (algo) {
	case RTE_CRYPTO_CIPHER_3DES_CBC:
		len = 8;
		break;
	case RTE_CRYPTO_CIPHER_3DES_CTR:
		len = 8;
		break;
	case RTE_CRYPTO_CIPHER_3DES_ECB:
		len = 8;
		break;
	case RTE_CRYPTO_CIPHER_AES_CBC:
		len = 16;
		break;

	/* TODO: add common algos */

	default:
		len = -1;
		break;
	}

	return len;
}

/**
 * vhost_crypto struct is used to maintain a number of virtio_cryptos and
 * one DPDK crypto device that deals with all crypto workloads. It is declared
 * here and defined in vhost_crypto.c
 */
struct vhost_crypto {
	/** Used to lookup DPDK Cryptodev Session based on VIRTIO crypto
	 *  session ID.
	 */
	struct rte_hash *session_map;
	struct rte_mempool *mbuf_pool;
	struct rte_mempool *sess_pool;
	struct rte_mempool *sess_priv_pool;
	struct rte_mempool *wb_pool;

	/** DPDK cryptodev ID */
	uint8_t cid;
	uint16_t nb_qps;

	uint64_t last_session_id;

	uint64_t cache_session_id;
	struct rte_cryptodev_sym_session *cache_session;
	/** socket id for the device */
	int socket_id;

	struct virtio_net *dev;

	uint8_t option;
} __rte_cache_aligned;

struct vhost_crypto_writeback_data {
	uint8_t *src;
	uint8_t *dst;
	uint64_t len;
	struct vhost_crypto_writeback_data *next;
};

struct vhost_crypto_data_req {
	struct vring_desc *head;
	struct virtio_net *dev;
	struct virtio_crypto_inhdr *inhdr;
	struct vhost_virtqueue *vq;
	struct vhost_crypto_writeback_data *wb;
	struct rte_mempool *wb_pool;
	uint16_t desc_idx;
	uint16_t len;
	uint16_t zero_copy;
};

static int
transform_cipher_param(struct rte_crypto_sym_xform *xform,
		VhostUserCryptoSessionParam *param)
{
	int ret;

	ret = cipher_algo_transform(param->cipher_algo, &xform->cipher.algo);
	if (unlikely(ret < 0))
		return ret;

	if (param->cipher_key_len > VHOST_USER_CRYPTO_MAX_CIPHER_KEY_LENGTH) {
		VC_LOG_DBG("Invalid cipher key length\n");
		return -VIRTIO_CRYPTO_BADMSG;
	}

	xform->type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	xform->cipher.key.length = param->cipher_key_len;
	if (xform->cipher.key.length > 0)
		xform->cipher.key.data = param->cipher_key_buf;
	if (param->dir == VIRTIO_CRYPTO_OP_ENCRYPT)
		xform->cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;
	else if (param->dir == VIRTIO_CRYPTO_OP_DECRYPT)
		xform->cipher.op = RTE_CRYPTO_CIPHER_OP_DECRYPT;
	else {
		VC_LOG_DBG("Bad operation type");
		return -VIRTIO_CRYPTO_BADMSG;
	}

	ret = get_iv_len(xform->cipher.algo);
	if (unlikely(ret < 0))
		return ret;
	xform->cipher.iv.length = (uint16_t)ret;
	xform->cipher.iv.offset = IV_OFFSET;
	return 0;
}

static int
transform_chain_param(struct rte_crypto_sym_xform *xforms,
		VhostUserCryptoSessionParam *param)
{
	struct rte_crypto_sym_xform *xform_cipher, *xform_auth;
	int ret;

	switch (param->chaining_dir) {
	case VIRTIO_CRYPTO_SYM_ALG_CHAIN_ORDER_HASH_THEN_CIPHER:
		xform_auth = xforms;
		xform_cipher = xforms->next;
		xform_cipher->cipher.op = RTE_CRYPTO_CIPHER_OP_DECRYPT;
		xform_auth->auth.op = RTE_CRYPTO_AUTH_OP_VERIFY;
		break;
	case VIRTIO_CRYPTO_SYM_ALG_CHAIN_ORDER_CIPHER_THEN_HASH:
		xform_cipher = xforms;
		xform_auth = xforms->next;
		xform_cipher->cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;
		xform_auth->auth.op = RTE_CRYPTO_AUTH_OP_GENERATE;
		break;
	default:
		return -VIRTIO_CRYPTO_BADMSG;
	}

	/* cipher */
	ret = cipher_algo_transform(param->cipher_algo,
			&xform_cipher->cipher.algo);
	if (unlikely(ret < 0))
		return ret;

	if (param->cipher_key_len > VHOST_USER_CRYPTO_MAX_CIPHER_KEY_LENGTH) {
		VC_LOG_DBG("Invalid cipher key length\n");
		return -VIRTIO_CRYPTO_BADMSG;
	}

	xform_cipher->type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	xform_cipher->cipher.key.length = param->cipher_key_len;
	xform_cipher->cipher.key.data = param->cipher_key_buf;
	ret = get_iv_len(xform_cipher->cipher.algo);
	if (unlikely(ret < 0))
		return ret;
	xform_cipher->cipher.iv.length = (uint16_t)ret;
	xform_cipher->cipher.iv.offset = IV_OFFSET;

	/* auth */
	xform_auth->type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ret = auth_algo_transform(param->hash_algo, &xform_auth->auth.algo);
	if (unlikely(ret < 0))
		return ret;

	if (param->auth_key_len > VHOST_USER_CRYPTO_MAX_HMAC_KEY_LENGTH) {
		VC_LOG_DBG("Invalid auth key length\n");
		return -VIRTIO_CRYPTO_BADMSG;
	}

	xform_auth->auth.digest_length = param->digest_len;
	xform_auth->auth.key.length = param->auth_key_len;
	xform_auth->auth.key.data = param->auth_key_buf;

	return 0;
}

static void
vhost_crypto_create_sess(struct vhost_crypto *vcrypto,
		VhostUserCryptoSessionParam *sess_param)
{
	struct rte_crypto_sym_xform xform1 = {0}, xform2 = {0};
	struct rte_cryptodev_sym_session *session;
	int ret;

	switch (sess_param->op_type) {
	case VIRTIO_CRYPTO_SYM_OP_NONE:
	case VIRTIO_CRYPTO_SYM_OP_CIPHER:
		ret = transform_cipher_param(&xform1, sess_param);
		if (unlikely(ret)) {
			VC_LOG_ERR("Error transform session msg (%i)", ret);
			sess_param->session_id = ret;
			return;
		}
		break;
	case VIRTIO_CRYPTO_SYM_OP_ALGORITHM_CHAINING:
		if (unlikely(sess_param->hash_mode !=
				VIRTIO_CRYPTO_SYM_HASH_MODE_AUTH)) {
			sess_param->session_id = -VIRTIO_CRYPTO_NOTSUPP;
			VC_LOG_ERR("Error transform session message (%i)",
					-VIRTIO_CRYPTO_NOTSUPP);
			return;
		}

		xform1.next = &xform2;

		ret = transform_chain_param(&xform1, sess_param);
		if (unlikely(ret)) {
			VC_LOG_ERR("Error transform session message (%i)", ret);
			sess_param->session_id = ret;
			return;
		}

		break;
	default:
		VC_LOG_ERR("Algorithm not yet supported");
		sess_param->session_id = -VIRTIO_CRYPTO_NOTSUPP;
		return;
	}

	session = rte_cryptodev_sym_session_create(vcrypto->sess_pool);
	if (!session) {
		VC_LOG_ERR("Failed to create session");
		sess_param->session_id = -VIRTIO_CRYPTO_ERR;
		return;
	}

	if (rte_cryptodev_sym_session_init(vcrypto->cid, session, &xform1,
			vcrypto->sess_priv_pool) < 0) {
		VC_LOG_ERR("Failed to initialize session");
		sess_param->session_id = -VIRTIO_CRYPTO_ERR;
		return;
	}

	/* insert hash to map */
	if (rte_hash_add_key_data(vcrypto->session_map,
			&vcrypto->last_session_id, session) < 0) {
		VC_LOG_ERR("Failed to insert session to hash table");

		if (rte_cryptodev_sym_session_clear(vcrypto->cid, session) < 0)
			VC_LOG_ERR("Failed to clear session");
		else {
			if (rte_cryptodev_sym_session_free(session) < 0)
				VC_LOG_ERR("Failed to free session");
		}
		sess_param->session_id = -VIRTIO_CRYPTO_ERR;
		return;
	}

	VC_LOG_INFO("Session %"PRIu64" created for vdev %i.",
			vcrypto->last_session_id, vcrypto->dev->vid);

	sess_param->session_id = vcrypto->last_session_id;
	vcrypto->last_session_id++;
}

static int
vhost_crypto_close_sess(struct vhost_crypto *vcrypto, uint64_t session_id)
{
	struct rte_cryptodev_sym_session *session;
	uint64_t sess_id = session_id;
	int ret;

	ret = rte_hash_lookup_data(vcrypto->session_map, &sess_id,
			(void **)&session);

	if (unlikely(ret < 0)) {
		VC_LOG_ERR("Failed to delete session %"PRIu64".", session_id);
		return -VIRTIO_CRYPTO_INVSESS;
	}

	if (rte_cryptodev_sym_session_clear(vcrypto->cid, session) < 0) {
		VC_LOG_DBG("Failed to clear session");
		return -VIRTIO_CRYPTO_ERR;
	}

	if (rte_cryptodev_sym_session_free(session) < 0) {
		VC_LOG_DBG("Failed to free session");
		return -VIRTIO_CRYPTO_ERR;
	}

	if (rte_hash_del_key(vcrypto->session_map, &sess_id) < 0) {
		VC_LOG_DBG("Failed to delete session from hash table.");
		return -VIRTIO_CRYPTO_ERR;
	}

	VC_LOG_INFO("Session %"PRIu64" deleted for vdev %i.", sess_id,
			vcrypto->dev->vid);

	return 0;
}

static enum rte_vhost_msg_result
vhost_crypto_msg_post_handler(int vid, void *msg)
{
	struct virtio_net *dev = get_device(vid);
	struct vhost_crypto *vcrypto;
	VhostUserMsg *vmsg = msg;
	enum rte_vhost_msg_result ret = RTE_VHOST_MSG_RESULT_OK;

	if (dev == NULL) {
		VC_LOG_ERR("Invalid vid %i", vid);
		return RTE_VHOST_MSG_RESULT_ERR;
	}

	vcrypto = dev->extern_data;
	if (vcrypto == NULL) {
		VC_LOG_ERR("Cannot find required data, is it initialized?");
		return RTE_VHOST_MSG_RESULT_ERR;
	}

	switch (vmsg->request.master) {
	case VHOST_USER_CRYPTO_CREATE_SESS:
		vhost_crypto_create_sess(vcrypto,
				&vmsg->payload.crypto_session);
		vmsg->fd_num = 0;
		ret = RTE_VHOST_MSG_RESULT_REPLY;
		break;
	case VHOST_USER_CRYPTO_CLOSE_SESS:
		if (vhost_crypto_close_sess(vcrypto, vmsg->payload.u64))
			ret = RTE_VHOST_MSG_RESULT_ERR;
		break;
	default:
		ret = RTE_VHOST_MSG_RESULT_NOT_HANDLED;
		break;
	}

	return ret;
}

static __rte_always_inline struct vring_desc *
find_write_desc(struct vring_desc *head, struct vring_desc *desc,
		uint32_t *nb_descs, uint32_t vq_size)
{
	if (desc->flags & VRING_DESC_F_WRITE)
		return desc;

	while (desc->flags & VRING_DESC_F_NEXT) {
		if (unlikely(*nb_descs == 0 || desc->next >= vq_size))
			return NULL;
		(*nb_descs)--;

		desc = &head[desc->next];
		if (desc->flags & VRING_DESC_F_WRITE)
			return desc;
	}

	return NULL;
}

static struct virtio_crypto_inhdr *
reach_inhdr(struct vhost_crypto_data_req *vc_req, struct vring_desc *desc,
		uint32_t *nb_descs, uint32_t vq_size)
{
	uint64_t dlen;
	struct virtio_crypto_inhdr *inhdr;

	while (desc->flags & VRING_DESC_F_NEXT) {
		if (unlikely(*nb_descs == 0 || desc->next >= vq_size))
			return NULL;
		(*nb_descs)--;
		desc = &vc_req->head[desc->next];
	}

	dlen = desc->len;
	inhdr = IOVA_TO_VVA(struct virtio_crypto_inhdr *, vc_req, desc->addr,
			&dlen, VHOST_ACCESS_WO);
	if (unlikely(!inhdr || dlen != desc->len))
		return NULL;

	return inhdr;
}

static __rte_always_inline int
move_desc(struct vring_desc *head, struct vring_desc **cur_desc,
		uint32_t size, uint32_t *nb_descs, uint32_t vq_size)
{
	struct vring_desc *desc = *cur_desc;
	int left = size - desc->len;

	while ((desc->flags & VRING_DESC_F_NEXT) && left > 0) {
		if (unlikely(*nb_descs == 0 || desc->next >= vq_size))
			return -1;

		desc = &head[desc->next];
		rte_prefetch0(&head[desc->next]);
		left -= desc->len;
		if (left > 0)
			(*nb_descs)--;
	}

	if (unlikely(left > 0))
		return -1;

	if (unlikely(*nb_descs == 0))
		*cur_desc = NULL;
	else {
		if (unlikely(desc->next >= vq_size))
			return -1;
		*cur_desc = &head[desc->next];
	}

	return 0;
}

static __rte_always_inline void *
get_data_ptr(struct vhost_crypto_data_req *vc_req, struct vring_desc *cur_desc,
		uint8_t perm)
{
	void *data;
	uint64_t dlen = cur_desc->len;

	data = IOVA_TO_VVA(void *, vc_req, cur_desc->addr, &dlen, perm);
	if (unlikely(!data || dlen != cur_desc->len)) {
		VC_LOG_ERR("Failed to map object");
		return NULL;
	}

	return data;
}

static int
copy_data(void *dst_data, struct vhost_crypto_data_req *vc_req,
		struct vring_desc **cur_desc, uint32_t size,
		uint32_t *nb_descs, uint32_t vq_size)
{
	struct vring_desc *desc = *cur_desc;
	uint64_t remain, addr, dlen, len;
	uint32_t to_copy;
	uint8_t *data = dst_data;
	uint8_t *src;
	int left = size;

	to_copy = RTE_MIN(desc->len, (uint32_t)left);
	dlen = to_copy;
	src = IOVA_TO_VVA(uint8_t *, vc_req, desc->addr, &dlen,
			VHOST_ACCESS_RO);
	if (unlikely(!src || !dlen))
		return -1;

	rte_memcpy((uint8_t *)data, src, dlen);
	data += dlen;

	if (unlikely(dlen < to_copy)) {
		remain = to_copy - dlen;
		addr = desc->addr + dlen;

		while (remain) {
			len = remain;
			src = IOVA_TO_VVA(uint8_t *, vc_req, addr, &len,
					VHOST_ACCESS_RO);
			if (unlikely(!src || !len)) {
				VC_LOG_ERR("Failed to map descriptor");
				return -1;
			}

			rte_memcpy(data, src, len);
			addr += len;
			remain -= len;
			data += len;
		}
	}

	left -= to_copy;

	while ((desc->flags & VRING_DESC_F_NEXT) && left > 0) {
		if (unlikely(*nb_descs == 0 || desc->next >= vq_size)) {
			VC_LOG_ERR("Invalid descriptors");
			return -1;
		}
		(*nb_descs)--;

		desc = &vc_req->head[desc->next];
		rte_prefetch0(&vc_req->head[desc->next]);
		to_copy = RTE_MIN(desc->len, (uint32_t)left);
		dlen = desc->len;
		src = IOVA_TO_VVA(uint8_t *, vc_req, desc->addr, &dlen,
				VHOST_ACCESS_RO);
		if (unlikely(!src || !dlen)) {
			VC_LOG_ERR("Failed to map descriptor");
			return -1;
		}

		rte_memcpy(data, src, dlen);
		data += dlen;

		if (unlikely(dlen < to_copy)) {
			remain = to_copy - dlen;
			addr = desc->addr + dlen;

			while (remain) {
				len = remain;
				src = IOVA_TO_VVA(uint8_t *, vc_req, addr, &len,
						VHOST_ACCESS_RO);
				if (unlikely(!src || !len)) {
					VC_LOG_ERR("Failed to map descriptor");
					return -1;
				}

				rte_memcpy(data, src, len);
				addr += len;
				remain -= len;
				data += len;
			}
		}

		left -= to_copy;
	}

	if (unlikely(left > 0)) {
		VC_LOG_ERR("Incorrect virtio descriptor");
		return -1;
	}

	if (unlikely(*nb_descs == 0))
		*cur_desc = NULL;
	else {
		if (unlikely(desc->next >= vq_size))
			return -1;
		*cur_desc = &vc_req->head[desc->next];
	}

	return 0;
}

static void
write_back_data(struct vhost_crypto_data_req *vc_req)
{
	struct vhost_crypto_writeback_data *wb_data = vc_req->wb, *wb_last;

	while (wb_data) {
		rte_memcpy(wb_data->dst, wb_data->src, wb_data->len);
		wb_last = wb_data;
		wb_data = wb_data->next;
		rte_mempool_put(vc_req->wb_pool, wb_last);
	}
}

static void
free_wb_data(struct vhost_crypto_writeback_data *wb_data,
		struct rte_mempool *mp)
{
	while (wb_data->next != NULL)
		free_wb_data(wb_data->next, mp);

	rte_mempool_put(mp, wb_data);
}

/**
 * The function will allocate a vhost_crypto_writeback_data linked list
 * containing the source and destination data pointers for the write back
 * operation after dequeued from Cryptodev PMD queues.
 *
 * @param vc_req
 *   The vhost crypto data request pointer
 * @param cur_desc
 *   The pointer of the current in use descriptor pointer. The content of
 *   cur_desc is expected to be updated after the function execution.
 * @param end_wb_data
 *   The last write back data element to be returned. It is used only in cipher
 *   and hash chain operations.
 * @param src
 *   The source data pointer
 * @param offset
 *   The offset to both source and destination data. For source data the offset
 *   is the number of bytes between src and start point of cipher operation. For
 *   destination data the offset is the number of bytes from *cur_desc->addr
 *   to the point where the src will be written to.
 * @param write_back_len
 *   The size of the write back length.
 * @return
 *   The pointer to the start of the write back data linked list.
 */
static struct vhost_crypto_writeback_data *
prepare_write_back_data(struct vhost_crypto_data_req *vc_req,
		struct vring_desc **cur_desc,
		struct vhost_crypto_writeback_data **end_wb_data,
		uint8_t *src,
		uint32_t offset,
		uint64_t write_back_len,
		uint32_t *nb_descs, uint32_t vq_size)
{
	struct vhost_crypto_writeback_data *wb_data, *head;
	struct vring_desc *desc = *cur_desc;
	uint64_t dlen;
	uint8_t *dst;
	int ret;

	ret = rte_mempool_get(vc_req->wb_pool, (void **)&head);
	if (unlikely(ret < 0)) {
		VC_LOG_ERR("no memory");
		goto error_exit;
	}

	wb_data = head;

	if (likely(desc->len > offset)) {
		wb_data->src = src + offset;
		dlen = desc->len;
		dst = IOVA_TO_VVA(uint8_t *, vc_req, desc->addr,
			&dlen, VHOST_ACCESS_RW) + offset;
		if (unlikely(!dst || dlen != desc->len)) {
			VC_LOG_ERR("Failed to map descriptor");
			goto error_exit;
		}

		wb_data->dst = dst;
		wb_data->len = desc->len - offset;
		write_back_len -= wb_data->len;
		src += offset + wb_data->len;
		offset = 0;

		if (unlikely(write_back_len)) {
			ret = rte_mempool_get(vc_req->wb_pool,
					(void **)&(wb_data->next));
			if (unlikely(ret < 0)) {
				VC_LOG_ERR("no memory");
				goto error_exit;
			}

			wb_data = wb_data->next;
		} else
			wb_data->next = NULL;
	} else
		offset -= desc->len;

	while (write_back_len) {
		if (unlikely(*nb_descs == 0 || desc->next >= vq_size)) {
			VC_LOG_ERR("Invalid descriptors");
			goto error_exit;
		}
		(*nb_descs)--;

		desc = &vc_req->head[desc->next];
		if (unlikely(!(desc->flags & VRING_DESC_F_WRITE))) {
			VC_LOG_ERR("incorrect descriptor");
			goto error_exit;
		}

		if (desc->len <= offset) {
			offset -= desc->len;
			continue;
		}

		dlen = desc->len;
		dst = IOVA_TO_VVA(uint8_t *, vc_req, desc->addr, &dlen,
				VHOST_ACCESS_RW) + offset;
		if (unlikely(dst == NULL || dlen != desc->len)) {
			VC_LOG_ERR("Failed to map descriptor");
			goto error_exit;
		}

		wb_data->src = src;
		wb_data->dst = dst;
		wb_data->len = RTE_MIN(desc->len - offset, write_back_len);
		write_back_len -= wb_data->len;
		src += wb_data->len;
		offset = 0;

		if (write_back_len) {
			ret = rte_mempool_get(vc_req->wb_pool,
					(void **)&(wb_data->next));
			if (unlikely(ret < 0)) {
				VC_LOG_ERR("no memory");
				goto error_exit;
			}

			wb_data = wb_data->next;
		} else
			wb_data->next = NULL;
	}

	if (unlikely(*nb_descs == 0))
		*cur_desc = NULL;
	else {
		if (unlikely(desc->next >= vq_size))
			goto error_exit;
		*cur_desc = &vc_req->head[desc->next];
	}

	*end_wb_data = wb_data;

	return head;

error_exit:
	if (head)
		free_wb_data(head, vc_req->wb_pool);

	return NULL;
}

static uint8_t
prepare_sym_cipher_op(struct vhost_crypto *vcrypto, struct rte_crypto_op *op,
		struct vhost_crypto_data_req *vc_req,
		struct virtio_crypto_cipher_data_req *cipher,
		struct vring_desc *cur_desc,
		uint32_t *nb_descs, uint32_t vq_size)
{
	struct vring_desc *desc = cur_desc;
	struct vhost_crypto_writeback_data *ewb = NULL;
	struct rte_mbuf *m_src = op->sym->m_src, *m_dst = op->sym->m_dst;
	uint8_t *iv_data = rte_crypto_op_ctod_offset(op, uint8_t *, IV_OFFSET);
	uint8_t ret = 0;

	/* prepare */
	/* iv */
	if (unlikely(copy_data(iv_data, vc_req, &desc, cipher->para.iv_len,
			nb_descs, vq_size) < 0)) {
		ret = VIRTIO_CRYPTO_BADMSG;
		goto error_exit;
	}

	m_src->data_len = cipher->para.src_data_len;

	switch (vcrypto->option) {
	case RTE_VHOST_CRYPTO_ZERO_COPY_ENABLE:
		m_src->buf_iova = gpa_to_hpa(vcrypto->dev, desc->addr,
				cipher->para.src_data_len);
		m_src->buf_addr = get_data_ptr(vc_req, desc, VHOST_ACCESS_RO);
		if (unlikely(m_src->buf_iova == 0 ||
				m_src->buf_addr == NULL)) {
			VC_LOG_ERR("zero_copy may fail due to cross page data");
			ret = VIRTIO_CRYPTO_ERR;
			goto error_exit;
		}

		if (unlikely(move_desc(vc_req->head, &desc,
				cipher->para.src_data_len, nb_descs,
				vq_size) < 0)) {
			VC_LOG_ERR("Incorrect descriptor");
			ret = VIRTIO_CRYPTO_ERR;
			goto error_exit;
		}

		break;
	case RTE_VHOST_CRYPTO_ZERO_COPY_DISABLE:
		vc_req->wb_pool = vcrypto->wb_pool;

		if (unlikely(cipher->para.src_data_len >
				RTE_MBUF_DEFAULT_BUF_SIZE)) {
			VC_LOG_ERR("Not enough space to do data copy");
			ret = VIRTIO_CRYPTO_ERR;
			goto error_exit;
		}
		if (unlikely(copy_data(rte_pktmbuf_mtod(m_src, uint8_t *),
				vc_req, &desc, cipher->para.src_data_len,
				nb_descs, vq_size) < 0)) {
			ret = VIRTIO_CRYPTO_BADMSG;
			goto error_exit;
		}
		break;
	default:
		ret = VIRTIO_CRYPTO_BADMSG;
		goto error_exit;
	}

	/* dst */
	desc = find_write_desc(vc_req->head, desc, nb_descs, vq_size);
	if (unlikely(!desc)) {
		VC_LOG_ERR("Cannot find write location");
		ret = VIRTIO_CRYPTO_BADMSG;
		goto error_exit;
	}

	switch (vcrypto->option) {
	case RTE_VHOST_CRYPTO_ZERO_COPY_ENABLE:
		m_dst->buf_iova = gpa_to_hpa(vcrypto->dev,
				desc->addr, cipher->para.dst_data_len);
		m_dst->buf_addr = get_data_ptr(vc_req, desc, VHOST_ACCESS_RW);
		if (unlikely(m_dst->buf_iova == 0 || m_dst->buf_addr == NULL)) {
			VC_LOG_ERR("zero_copy may fail due to cross page data");
			ret = VIRTIO_CRYPTO_ERR;
			goto error_exit;
		}

		if (unlikely(move_desc(vc_req->head, &desc,
				cipher->para.dst_data_len,
				nb_descs, vq_size) < 0)) {
			VC_LOG_ERR("Incorrect descriptor");
			ret = VIRTIO_CRYPTO_ERR;
			goto error_exit;
		}

		m_dst->data_len = cipher->para.dst_data_len;
		break;
	case RTE_VHOST_CRYPTO_ZERO_COPY_DISABLE:
		vc_req->wb = prepare_write_back_data(vc_req, &desc, &ewb,
				rte_pktmbuf_mtod(m_src, uint8_t *), 0,
				cipher->para.dst_data_len, nb_descs, vq_size);
		if (unlikely(vc_req->wb == NULL)) {
			ret = VIRTIO_CRYPTO_ERR;
			goto error_exit;
		}

		break;
	default:
		ret = VIRTIO_CRYPTO_BADMSG;
		goto error_exit;
	}

	/* src data */
	op->type = RTE_CRYPTO_OP_TYPE_SYMMETRIC;
	op->sess_type = RTE_CRYPTO_OP_WITH_SESSION;

	op->sym->cipher.data.offset = 0;
	op->sym->cipher.data.length = cipher->para.src_data_len;

	vc_req->inhdr = get_data_ptr(vc_req, desc, VHOST_ACCESS_WO);
	if (unlikely(vc_req->inhdr == NULL)) {
		ret = VIRTIO_CRYPTO_BADMSG;
		goto error_exit;
	}

	vc_req->inhdr->status = VIRTIO_CRYPTO_OK;
	vc_req->len = cipher->para.dst_data_len + INHDR_LEN;

	return 0;

error_exit:
	if (vc_req->wb)
		free_wb_data(vc_req->wb, vc_req->wb_pool);

	vc_req->len = INHDR_LEN;
	return ret;
}

static uint8_t
prepare_sym_chain_op(struct vhost_crypto *vcrypto, struct rte_crypto_op *op,
		struct vhost_crypto_data_req *vc_req,
		struct virtio_crypto_alg_chain_data_req *chain,
		struct vring_desc *cur_desc,
		uint32_t *nb_descs, uint32_t vq_size)
{
	struct vring_desc *desc = cur_desc, *digest_desc;
	struct vhost_crypto_writeback_data *ewb = NULL, *ewb2 = NULL;
	struct rte_mbuf *m_src = op->sym->m_src, *m_dst = op->sym->m_dst;
	uint8_t *iv_data = rte_crypto_op_ctod_offset(op, uint8_t *, IV_OFFSET);
	uint32_t digest_offset;
	void *digest_addr;
	uint8_t ret = 0;

	/* prepare */
	/* iv */
	if (unlikely(copy_data(iv_data, vc_req, &desc,
			chain->para.iv_len, nb_descs, vq_size) < 0)) {
		ret = VIRTIO_CRYPTO_BADMSG;
		goto error_exit;
	}

	m_src->data_len = chain->para.src_data_len;

	switch (vcrypto->option) {
	case RTE_VHOST_CRYPTO_ZERO_COPY_ENABLE:
		m_dst->data_len = chain->para.dst_data_len;

		m_src->buf_iova = gpa_to_hpa(vcrypto->dev, desc->addr,
				chain->para.src_data_len);
		m_src->buf_addr = get_data_ptr(vc_req, desc, VHOST_ACCESS_RO);
		if (unlikely(m_src->buf_iova == 0 || m_src->buf_addr == NULL)) {
			VC_LOG_ERR("zero_copy may fail due to cross page data");
			ret = VIRTIO_CRYPTO_ERR;
			goto error_exit;
		}

		if (unlikely(move_desc(vc_req->head, &desc,
				chain->para.src_data_len,
				nb_descs, vq_size) < 0)) {
			VC_LOG_ERR("Incorrect descriptor");
			ret = VIRTIO_CRYPTO_ERR;
			goto error_exit;
		}
		break;
	case RTE_VHOST_CRYPTO_ZERO_COPY_DISABLE:
		vc_req->wb_pool = vcrypto->wb_pool;

		if (unlikely(chain->para.src_data_len >
				RTE_MBUF_DEFAULT_BUF_SIZE)) {
			VC_LOG_ERR("Not enough space to do data copy");
			ret = VIRTIO_CRYPTO_ERR;
			goto error_exit;
		}
		if (unlikely(copy_data(rte_pktmbuf_mtod(m_src, uint8_t *),
				vc_req, &desc, chain->para.src_data_len,
				nb_descs, vq_size) < 0)) {
			ret = VIRTIO_CRYPTO_BADMSG;
			goto error_exit;
		}

		break;
	default:
		ret = VIRTIO_CRYPTO_BADMSG;
		goto error_exit;
	}

	/* dst */
	desc = find_write_desc(vc_req->head, desc, nb_descs, vq_size);
	if (unlikely(!desc)) {
		VC_LOG_ERR("Cannot find write location");
		ret = VIRTIO_CRYPTO_BADMSG;
		goto error_exit;
	}

	switch (vcrypto->option) {
	case RTE_VHOST_CRYPTO_ZERO_COPY_ENABLE:
		m_dst->buf_iova = gpa_to_hpa(vcrypto->dev,
				desc->addr, chain->para.dst_data_len);
		m_dst->buf_addr = get_data_ptr(vc_req, desc, VHOST_ACCESS_RW);
		if (unlikely(m_dst->buf_iova == 0 || m_dst->buf_addr == NULL)) {
			VC_LOG_ERR("zero_copy may fail due to cross page data");
			ret = VIRTIO_CRYPTO_ERR;
			goto error_exit;
		}

		if (unlikely(move_desc(vc_req->head, &desc,
				chain->para.dst_data_len,
				nb_descs, vq_size) < 0)) {
			VC_LOG_ERR("Incorrect descriptor");
			ret = VIRTIO_CRYPTO_ERR;
			goto error_exit;
		}

		op->sym->auth.digest.phys_addr = gpa_to_hpa(vcrypto->dev,
				desc->addr, chain->para.hash_result_len);
		op->sym->auth.digest.data = get_data_ptr(vc_req, desc,
				VHOST_ACCESS_RW);
		if (unlikely(op->sym->auth.digest.phys_addr == 0)) {
			VC_LOG_ERR("zero_copy may fail due to cross page data");
			ret = VIRTIO_CRYPTO_ERR;
			goto error_exit;
		}

		if (unlikely(move_desc(vc_req->head, &desc,
				chain->para.hash_result_len,
				nb_descs, vq_size) < 0)) {
			VC_LOG_ERR("Incorrect descriptor");
			ret = VIRTIO_CRYPTO_ERR;
			goto error_exit;
		}

		break;
	case RTE_VHOST_CRYPTO_ZERO_COPY_DISABLE:
		vc_req->wb = prepare_write_back_data(vc_req, &desc, &ewb,
				rte_pktmbuf_mtod(m_src, uint8_t *),
				chain->para.cipher_start_src_offset,
				chain->para.dst_data_len -
				chain->para.cipher_start_src_offset,
				nb_descs, vq_size);
		if (unlikely(vc_req->wb == NULL)) {
			ret = VIRTIO_CRYPTO_ERR;
			goto error_exit;
		}

		digest_offset = m_src->data_len;
		digest_addr = rte_pktmbuf_mtod_offset(m_src, void *,
				digest_offset);
		digest_desc = desc;

		/** create a wb_data for digest */
		ewb->next = prepare_write_back_data(vc_req, &desc, &ewb2,
				digest_addr, 0, chain->para.hash_result_len,
				nb_descs, vq_size);
		if (unlikely(ewb->next == NULL)) {
			ret = VIRTIO_CRYPTO_ERR;
			goto error_exit;
		}

		if (unlikely(copy_data(digest_addr, vc_req, &digest_desc,
				chain->para.hash_result_len,
				nb_descs, vq_size) < 0)) {
			ret = VIRTIO_CRYPTO_BADMSG;
			goto error_exit;
		}

		op->sym->auth.digest.data = digest_addr;
		op->sym->auth.digest.phys_addr = rte_pktmbuf_iova_offset(m_src,
				digest_offset);
		break;
	default:
		ret = VIRTIO_CRYPTO_BADMSG;
		goto error_exit;
	}

	/* record inhdr */
	vc_req->inhdr = get_data_ptr(vc_req, desc, VHOST_ACCESS_WO);
	if (unlikely(vc_req->inhdr == NULL)) {
		ret = VIRTIO_CRYPTO_BADMSG;
		goto error_exit;
	}

	vc_req->inhdr->status = VIRTIO_CRYPTO_OK;

	op->type = RTE_CRYPTO_OP_TYPE_SYMMETRIC;
	op->sess_type = RTE_CRYPTO_OP_WITH_SESSION;

	op->sym->cipher.data.offset = chain->para.cipher_start_src_offset;
	op->sym->cipher.data.length = chain->para.src_data_len -
			chain->para.cipher_start_src_offset;

	op->sym->auth.data.offset = chain->para.hash_start_src_offset;
	op->sym->auth.data.length = chain->para.len_to_hash;

	vc_req->len = chain->para.dst_data_len + chain->para.hash_result_len +
			INHDR_LEN;
	return 0;

error_exit:
	if (vc_req->wb)
		free_wb_data(vc_req->wb, vc_req->wb_pool);
	vc_req->len = INHDR_LEN;
	return ret;
}

/**
 * Process on descriptor
 */
static __rte_always_inline int
vhost_crypto_process_one_req(struct vhost_crypto *vcrypto,
		struct vhost_virtqueue *vq, struct rte_crypto_op *op,
		struct vring_desc *head, uint16_t desc_idx)
{
	struct vhost_crypto_data_req *vc_req = rte_mbuf_to_priv(op->sym->m_src);
	struct rte_cryptodev_sym_session *session;
	struct virtio_crypto_op_data_req *req, tmp_req;
	struct virtio_crypto_inhdr *inhdr;
	struct vring_desc *desc = NULL;
	uint64_t session_id;
	uint64_t dlen;
	uint32_t nb_descs = vq->size;
	int err = 0;

	vc_req->desc_idx = desc_idx;
	vc_req->dev = vcrypto->dev;
	vc_req->vq = vq;

	if (likely(head->flags & VRING_DESC_F_INDIRECT)) {
		dlen = head->len;
		nb_descs = dlen / sizeof(struct vring_desc);
		/* drop invalid descriptors */
		if (unlikely(nb_descs > vq->size))
			return -1;
		desc = IOVA_TO_VVA(struct vring_desc *, vc_req, head->addr,
				&dlen, VHOST_ACCESS_RO);
		if (unlikely(!desc || dlen != head->len))
			return -1;
		desc_idx = 0;
		head = desc;
	} else {
		desc = head;
	}

	vc_req->head = head;
	vc_req->zero_copy = vcrypto->option;

	req = get_data_ptr(vc_req, desc, VHOST_ACCESS_RO);
	if (unlikely(req == NULL)) {
		switch (vcrypto->option) {
		case RTE_VHOST_CRYPTO_ZERO_COPY_ENABLE:
			err = VIRTIO_CRYPTO_BADMSG;
			VC_LOG_ERR("Invalid descriptor");
			goto error_exit;
		case RTE_VHOST_CRYPTO_ZERO_COPY_DISABLE:
			req = &tmp_req;
			if (unlikely(copy_data(req, vc_req, &desc, sizeof(*req),
					&nb_descs, vq->size) < 0)) {
				err = VIRTIO_CRYPTO_BADMSG;
				VC_LOG_ERR("Invalid descriptor");
				goto error_exit;
			}
			break;
		default:
			err = VIRTIO_CRYPTO_ERR;
			VC_LOG_ERR("Invalid option");
			goto error_exit;
		}
	} else {
		if (unlikely(move_desc(vc_req->head, &desc,
				sizeof(*req), &nb_descs, vq->size) < 0)) {
			VC_LOG_ERR("Incorrect descriptor");
			goto error_exit;
		}
	}

	switch (req->header.opcode) {
	case VIRTIO_CRYPTO_CIPHER_ENCRYPT:
	case VIRTIO_CRYPTO_CIPHER_DECRYPT:
		session_id = req->header.session_id;

		/* one branch to avoid unnecessary table lookup */
		if (vcrypto->cache_session_id != session_id) {
			err = rte_hash_lookup_data(vcrypto->session_map,
					&session_id, (void **)&session);
			if (unlikely(err < 0)) {
				err = VIRTIO_CRYPTO_ERR;
				VC_LOG_ERR("Failed to find session %"PRIu64,
						session_id);
				goto error_exit;
			}

			vcrypto->cache_session = session;
			vcrypto->cache_session_id = session_id;
		}

		session = vcrypto->cache_session;

		err = rte_crypto_op_attach_sym_session(op, session);
		if (unlikely(err < 0)) {
			err = VIRTIO_CRYPTO_ERR;
			VC_LOG_ERR("Failed to attach session to op");
			goto error_exit;
		}

		switch (req->u.sym_req.op_type) {
		case VIRTIO_CRYPTO_SYM_OP_NONE:
			err = VIRTIO_CRYPTO_NOTSUPP;
			break;
		case VIRTIO_CRYPTO_SYM_OP_CIPHER:
			err = prepare_sym_cipher_op(vcrypto, op, vc_req,
					&req->u.sym_req.u.cipher, desc,
					&nb_descs, vq->size);
			break;
		case VIRTIO_CRYPTO_SYM_OP_ALGORITHM_CHAINING:
			err = prepare_sym_chain_op(vcrypto, op, vc_req,
					&req->u.sym_req.u.chain, desc,
					&nb_descs, vq->size);
			break;
		}
		if (unlikely(err != 0)) {
			VC_LOG_ERR("Failed to process sym request");
			goto error_exit;
		}
		break;
	default:
		VC_LOG_ERR("Unsupported symmetric crypto request type %u",
				req->header.opcode);
		goto error_exit;
	}

	return 0;

error_exit:

	inhdr = reach_inhdr(vc_req, desc, &nb_descs, vq->size);
	if (likely(inhdr != NULL))
		inhdr->status = (uint8_t)err;

	return -1;
}

static __rte_always_inline struct vhost_virtqueue *
vhost_crypto_finalize_one_request(struct rte_crypto_op *op,
		struct vhost_virtqueue *old_vq)
{
	struct rte_mbuf *m_src = op->sym->m_src;
	struct rte_mbuf *m_dst = op->sym->m_dst;
	struct vhost_crypto_data_req *vc_req = rte_mbuf_to_priv(m_src);
	uint16_t desc_idx;

	if (unlikely(!vc_req)) {
		VC_LOG_ERR("Failed to retrieve vc_req");
		return NULL;
	}

	if (old_vq && (vc_req->vq != old_vq))
		return vc_req->vq;

	desc_idx = vc_req->desc_idx;

	if (unlikely(op->status != RTE_CRYPTO_OP_STATUS_SUCCESS))
		vc_req->inhdr->status = VIRTIO_CRYPTO_ERR;
	else {
		if (vc_req->zero_copy == 0)
			write_back_data(vc_req);
	}

	vc_req->vq->used->ring[desc_idx].id = desc_idx;
	vc_req->vq->used->ring[desc_idx].len = vc_req->len;

	rte_mempool_put(m_src->pool, (void *)m_src);

	if (m_dst)
		rte_mempool_put(m_dst->pool, (void *)m_dst);

	return vc_req->vq;
}

static __rte_always_inline uint16_t
vhost_crypto_complete_one_vm_requests(struct rte_crypto_op **ops,
		uint16_t nb_ops, int *callfd)
{
	uint16_t processed = 1;
	struct vhost_virtqueue *vq, *tmp_vq;

	if (unlikely(nb_ops == 0))
		return 0;

	vq = vhost_crypto_finalize_one_request(ops[0], NULL);
	if (unlikely(vq == NULL))
		return 0;
	tmp_vq = vq;

	while ((processed < nb_ops)) {
		tmp_vq = vhost_crypto_finalize_one_request(ops[processed],
				tmp_vq);

		if (unlikely(vq != tmp_vq))
			break;

		processed++;
	}

	*callfd = vq->callfd;

	*(volatile uint16_t *)&vq->used->idx += processed;

	return processed;
}

int
rte_vhost_crypto_create(int vid, uint8_t cryptodev_id,
		struct rte_mempool *sess_pool,
		struct rte_mempool *sess_priv_pool,
		int socket_id)
{
	struct virtio_net *dev = get_device(vid);
	struct rte_hash_parameters params = {0};
	struct vhost_crypto *vcrypto;
	char name[128];
	int ret;

	if (!dev) {
		VC_LOG_ERR("Invalid vid %i", vid);
		return -EINVAL;
	}

	ret = rte_vhost_driver_set_features(dev->ifname,
			VIRTIO_CRYPTO_FEATURES);
	if (ret < 0) {
		VC_LOG_ERR("Error setting features");
		return -1;
	}

	vcrypto = rte_zmalloc_socket(NULL, sizeof(*vcrypto),
			RTE_CACHE_LINE_SIZE, socket_id);
	if (!vcrypto) {
		VC_LOG_ERR("Insufficient memory");
		return -ENOMEM;
	}

	vcrypto->sess_pool = sess_pool;
	vcrypto->sess_priv_pool = sess_priv_pool;
	vcrypto->cid = cryptodev_id;
	vcrypto->cache_session_id = UINT64_MAX;
	vcrypto->last_session_id = 1;
	vcrypto->dev = dev;
	vcrypto->option = RTE_VHOST_CRYPTO_ZERO_COPY_DISABLE;

	snprintf(name, 127, "HASH_VHOST_CRYPT_%u", (uint32_t)vid);
	params.name = name;
	params.entries = VHOST_CRYPTO_SESSION_MAP_ENTRIES;
	params.hash_func = rte_jhash;
	params.key_len = sizeof(uint64_t);
	params.socket_id = socket_id;
	vcrypto->session_map = rte_hash_create(&params);
	if (!vcrypto->session_map) {
		VC_LOG_ERR("Failed to creath session map");
		ret = -ENOMEM;
		goto error_exit;
	}

	snprintf(name, 127, "MBUF_POOL_VM_%u", (uint32_t)vid);
	vcrypto->mbuf_pool = rte_pktmbuf_pool_create(name,
			VHOST_CRYPTO_MBUF_POOL_SIZE, 512,
			sizeof(struct vhost_crypto_data_req),
			RTE_MBUF_DEFAULT_DATAROOM * 2 + RTE_PKTMBUF_HEADROOM,
			rte_socket_id());
	if (!vcrypto->mbuf_pool) {
		VC_LOG_ERR("Failed to creath mbuf pool");
		ret = -ENOMEM;
		goto error_exit;
	}

	snprintf(name, 127, "WB_POOL_VM_%u", (uint32_t)vid);
	vcrypto->wb_pool = rte_mempool_create(name,
			VHOST_CRYPTO_MBUF_POOL_SIZE,
			sizeof(struct vhost_crypto_writeback_data),
			128, 0, NULL, NULL, NULL, NULL,
			rte_socket_id(), 0);
	if (!vcrypto->wb_pool) {
		VC_LOG_ERR("Failed to creath mempool");
		ret = -ENOMEM;
		goto error_exit;
	}

	dev->extern_data = vcrypto;
	dev->extern_ops.pre_msg_handle = NULL;
	dev->extern_ops.post_msg_handle = vhost_crypto_msg_post_handler;

	return 0;

error_exit:
	if (vcrypto->session_map)
		rte_hash_free(vcrypto->session_map);
	if (vcrypto->mbuf_pool)
		rte_mempool_free(vcrypto->mbuf_pool);

	rte_free(vcrypto);

	return ret;
}

int
rte_vhost_crypto_free(int vid)
{
	struct virtio_net *dev = get_device(vid);
	struct vhost_crypto *vcrypto;

	if (unlikely(dev == NULL)) {
		VC_LOG_ERR("Invalid vid %i", vid);
		return -EINVAL;
	}

	vcrypto = dev->extern_data;
	if (unlikely(vcrypto == NULL)) {
		VC_LOG_ERR("Cannot find required data, is it initialized?");
		return -ENOENT;
	}

	rte_hash_free(vcrypto->session_map);
	rte_mempool_free(vcrypto->mbuf_pool);
	rte_mempool_free(vcrypto->wb_pool);
	rte_free(vcrypto);

	dev->extern_data = NULL;
	dev->extern_ops.pre_msg_handle = NULL;
	dev->extern_ops.post_msg_handle = NULL;

	return 0;
}

int
rte_vhost_crypto_set_zero_copy(int vid, enum rte_vhost_crypto_zero_copy option)
{
	struct virtio_net *dev = get_device(vid);
	struct vhost_crypto *vcrypto;

	if (unlikely(dev == NULL)) {
		VC_LOG_ERR("Invalid vid %i", vid);
		return -EINVAL;
	}

	if (unlikely((uint32_t)option >=
				RTE_VHOST_CRYPTO_MAX_ZERO_COPY_OPTIONS)) {
		VC_LOG_ERR("Invalid option %i", option);
		return -EINVAL;
	}

	vcrypto = (struct vhost_crypto *)dev->extern_data;
	if (unlikely(vcrypto == NULL)) {
		VC_LOG_ERR("Cannot find required data, is it initialized?");
		return -ENOENT;
	}

	if (vcrypto->option == (uint8_t)option)
		return 0;

	if (!(rte_mempool_full(vcrypto->mbuf_pool)) ||
			!(rte_mempool_full(vcrypto->wb_pool))) {
		VC_LOG_ERR("Cannot update zero copy as mempool is not full");
		return -EINVAL;
	}

	if (option == RTE_VHOST_CRYPTO_ZERO_COPY_DISABLE) {
		char name[128];

		snprintf(name, 127, "WB_POOL_VM_%u", (uint32_t)vid);
		vcrypto->wb_pool = rte_mempool_create(name,
				VHOST_CRYPTO_MBUF_POOL_SIZE,
				sizeof(struct vhost_crypto_writeback_data),
				128, 0, NULL, NULL, NULL, NULL,
				rte_socket_id(), 0);
		if (!vcrypto->wb_pool) {
			VC_LOG_ERR("Failed to creath mbuf pool");
			return -ENOMEM;
		}
	} else {
		rte_mempool_free(vcrypto->wb_pool);
		vcrypto->wb_pool = NULL;
	}

	vcrypto->option = (uint8_t)option;

	return 0;
}

uint16_t
rte_vhost_crypto_fetch_requests(int vid, uint32_t qid,
		struct rte_crypto_op **ops, uint16_t nb_ops)
{
	struct rte_mbuf *mbufs[VHOST_CRYPTO_MAX_BURST_SIZE * 2];
	struct virtio_net *dev = get_device(vid);
	struct vhost_crypto *vcrypto;
	struct vhost_virtqueue *vq;
	uint16_t avail_idx;
	uint16_t start_idx;
	uint16_t count;
	uint16_t i = 0;

	if (unlikely(dev == NULL)) {
		VC_LOG_ERR("Invalid vid %i", vid);
		return 0;
	}

	if (unlikely(qid >= VHOST_MAX_QUEUE_PAIRS)) {
		VC_LOG_ERR("Invalid qid %u", qid);
		return 0;
	}

	vcrypto = (struct vhost_crypto *)dev->extern_data;
	if (unlikely(vcrypto == NULL)) {
		VC_LOG_ERR("Cannot find required data, is it initialized?");
		return 0;
	}

	vq = dev->virtqueue[qid];

	avail_idx = *((volatile uint16_t *)&vq->avail->idx);
	start_idx = vq->last_used_idx;
	count = avail_idx - start_idx;
	count = RTE_MIN(count, VHOST_CRYPTO_MAX_BURST_SIZE);
	count = RTE_MIN(count, nb_ops);

	if (unlikely(count == 0))
		return 0;

	/* for zero copy, we need 2 empty mbufs for src and dst, otherwise
	 * we need only 1 mbuf as src and dst
	 */
	switch (vcrypto->option) {
	case RTE_VHOST_CRYPTO_ZERO_COPY_ENABLE:
		if (unlikely(rte_mempool_get_bulk(vcrypto->mbuf_pool,
				(void **)mbufs, count * 2) < 0)) {
			VC_LOG_ERR("Insufficient memory");
			return 0;
		}

		for (i = 0; i < count; i++) {
			uint16_t used_idx = (start_idx + i) & (vq->size - 1);
			uint16_t desc_idx = vq->avail->ring[used_idx];
			struct vring_desc *head = &vq->desc[desc_idx];
			struct rte_crypto_op *op = ops[i];

			op->sym->m_src = mbufs[i * 2];
			op->sym->m_dst = mbufs[i * 2 + 1];
			op->sym->m_src->data_off = 0;
			op->sym->m_dst->data_off = 0;

			if (unlikely(vhost_crypto_process_one_req(vcrypto, vq,
					op, head, desc_idx) < 0))
				break;
		}

		if (unlikely(i < count))
			rte_mempool_put_bulk(vcrypto->mbuf_pool,
					(void **)&mbufs[i * 2],
					(count - i) * 2);

		break;

	case RTE_VHOST_CRYPTO_ZERO_COPY_DISABLE:
		if (unlikely(rte_mempool_get_bulk(vcrypto->mbuf_pool,
				(void **)mbufs, count) < 0)) {
			VC_LOG_ERR("Insufficient memory");
			return 0;
		}

		for (i = 0; i < count; i++) {
			uint16_t used_idx = (start_idx + i) & (vq->size - 1);
			uint16_t desc_idx = vq->avail->ring[used_idx];
			struct vring_desc *head = &vq->desc[desc_idx];
			struct rte_crypto_op *op = ops[i];

			op->sym->m_src = mbufs[i];
			op->sym->m_dst = NULL;
			op->sym->m_src->data_off = 0;

			if (unlikely(vhost_crypto_process_one_req(vcrypto, vq,
					op, head, desc_idx) < 0))
				break;
		}

		if (unlikely(i < count))
			rte_mempool_put_bulk(vcrypto->mbuf_pool,
					(void **)&mbufs[i],
					count - i);

		break;

	}

	vq->last_used_idx += i;

	return i;
}

uint16_t
rte_vhost_crypto_finalize_requests(struct rte_crypto_op **ops,
		uint16_t nb_ops, int *callfds, uint16_t *nb_callfds)
{
	struct rte_crypto_op **tmp_ops = ops;
	uint16_t count = 0, left = nb_ops;
	int callfd;
	uint16_t idx = 0;

	while (left) {
		count = vhost_crypto_complete_one_vm_requests(tmp_ops, left,
				&callfd);
		if (unlikely(count == 0))
			break;

		tmp_ops = &tmp_ops[count];
		left -= count;

		callfds[idx++] = callfd;

		if (unlikely(idx >= VIRTIO_CRYPTO_MAX_NUM_BURST_VQS)) {
			VC_LOG_ERR("Too many vqs");
			break;
		}
	}

	*nb_callfds = idx;

	return nb_ops - left;
}
