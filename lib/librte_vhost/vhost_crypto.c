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
		(1 << VIRTIO_NET_F_CTRL_VQ))

#define IOVA_TO_VVA(t, r, a, l, p)					\
	((t)(uintptr_t)vhost_iova_to_vva(r->dev, r->vq, a, l, p))

static int
cipher_algo_transform(uint32_t virtio_cipher_algo)
{
	int ret;

	switch (virtio_cipher_algo) {
	case VIRTIO_CRYPTO_CIPHER_AES_CBC:
		ret = RTE_CRYPTO_CIPHER_AES_CBC;
		break;
	case VIRTIO_CRYPTO_CIPHER_AES_CTR:
		ret = RTE_CRYPTO_CIPHER_AES_CTR;
		break;
	case VIRTIO_CRYPTO_CIPHER_DES_ECB:
		ret = -VIRTIO_CRYPTO_NOTSUPP;
		break;
	case VIRTIO_CRYPTO_CIPHER_DES_CBC:
		ret = RTE_CRYPTO_CIPHER_DES_CBC;
		break;
	case VIRTIO_CRYPTO_CIPHER_3DES_ECB:
		ret = RTE_CRYPTO_CIPHER_3DES_ECB;
		break;
	case VIRTIO_CRYPTO_CIPHER_3DES_CBC:
		ret = RTE_CRYPTO_CIPHER_3DES_CBC;
		break;
	case VIRTIO_CRYPTO_CIPHER_3DES_CTR:
		ret = RTE_CRYPTO_CIPHER_3DES_CTR;
		break;
	case VIRTIO_CRYPTO_CIPHER_KASUMI_F8:
		ret = RTE_CRYPTO_CIPHER_KASUMI_F8;
		break;
	case VIRTIO_CRYPTO_CIPHER_SNOW3G_UEA2:
		ret = RTE_CRYPTO_CIPHER_SNOW3G_UEA2;
		break;
	case VIRTIO_CRYPTO_CIPHER_AES_F8:
		ret = RTE_CRYPTO_CIPHER_AES_F8;
		break;
	case VIRTIO_CRYPTO_CIPHER_AES_XTS:
		ret = RTE_CRYPTO_CIPHER_AES_XTS;
		break;
	case VIRTIO_CRYPTO_CIPHER_ZUC_EEA3:
		ret = RTE_CRYPTO_CIPHER_ZUC_EEA3;
		break;
	default:
		ret = -VIRTIO_CRYPTO_BADMSG;
		break;
	}

	return ret;
}

static int
auth_algo_transform(uint32_t virtio_auth_algo)
{
	int ret;

	switch (virtio_auth_algo) {

	case VIRTIO_CRYPTO_NO_MAC:
		ret = RTE_CRYPTO_AUTH_NULL;
		break;
	case VIRTIO_CRYPTO_MAC_HMAC_MD5:
		ret = RTE_CRYPTO_AUTH_MD5_HMAC;
		break;
	case VIRTIO_CRYPTO_MAC_HMAC_SHA1:
		ret = RTE_CRYPTO_AUTH_SHA1_HMAC;
		break;
	case VIRTIO_CRYPTO_MAC_HMAC_SHA_224:
		ret = RTE_CRYPTO_AUTH_SHA224_HMAC;
		break;
	case VIRTIO_CRYPTO_MAC_HMAC_SHA_256:
		ret = RTE_CRYPTO_AUTH_SHA256_HMAC;
		break;
	case VIRTIO_CRYPTO_MAC_HMAC_SHA_384:
		ret = RTE_CRYPTO_AUTH_SHA384_HMAC;
		break;
	case VIRTIO_CRYPTO_MAC_HMAC_SHA_512:
		ret = RTE_CRYPTO_AUTH_SHA512_HMAC;
		break;
	case VIRTIO_CRYPTO_MAC_CMAC_3DES:
		ret = -VIRTIO_CRYPTO_NOTSUPP;
		break;
	case VIRTIO_CRYPTO_MAC_CMAC_AES:
		ret = RTE_CRYPTO_AUTH_AES_CMAC;
		break;
	case VIRTIO_CRYPTO_MAC_KASUMI_F9:
		ret = RTE_CRYPTO_AUTH_KASUMI_F9;
		break;
	case VIRTIO_CRYPTO_MAC_SNOW3G_UIA2:
		ret = RTE_CRYPTO_AUTH_SNOW3G_UIA2;
		break;
	case VIRTIO_CRYPTO_MAC_GMAC_AES:
		ret = RTE_CRYPTO_AUTH_AES_GMAC;
		break;
	case VIRTIO_CRYPTO_MAC_GMAC_TWOFISH:
		ret = -VIRTIO_CRYPTO_NOTSUPP;
		break;
	case VIRTIO_CRYPTO_MAC_CBCMAC_AES:
		ret = RTE_CRYPTO_AUTH_AES_CBC_MAC;
		break;
	case VIRTIO_CRYPTO_MAC_CBCMAC_KASUMI_F9:
		ret = -VIRTIO_CRYPTO_NOTSUPP;
		break;
	case VIRTIO_CRYPTO_MAC_XCBC_AES:
		ret = RTE_CRYPTO_AUTH_AES_XCBC_MAC;
		break;
	default:
		ret = -VIRTIO_CRYPTO_BADMSG;
		break;
	}

	return ret;
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

struct vhost_crypto_data_req {
	struct vring_desc *head;
	struct virtio_net *dev;
	struct virtio_crypto_inhdr *inhdr;
	struct vhost_virtqueue *vq;
	struct vring_desc *wb_desc;
	uint16_t wb_len;
	uint16_t desc_idx;
	uint16_t len;
	uint16_t zero_copy;
};

static int
transform_cipher_param(struct rte_crypto_sym_xform *xform,
		VhostUserCryptoSessionParam *param)
{
	int ret;

	ret = cipher_algo_transform(param->cipher_algo);
	if (unlikely(ret < 0))
		return ret;

	xform->type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	xform->cipher.algo = (uint32_t)ret;
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
	ret = cipher_algo_transform(param->cipher_algo);
	if (unlikely(ret < 0))
		return ret;
	xform_cipher->type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	xform_cipher->cipher.algo = (uint32_t)ret;
	xform_cipher->cipher.key.length = param->cipher_key_len;
	xform_cipher->cipher.key.data = param->cipher_key_buf;
	ret = get_iv_len(xform_cipher->cipher.algo);
	if (unlikely(ret < 0))
		return ret;
	xform_cipher->cipher.iv.length = (uint16_t)ret;
	xform_cipher->cipher.iv.offset = IV_OFFSET;

	/* auth */
	xform_auth->type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ret = auth_algo_transform(param->hash_algo);
	if (unlikely(ret < 0))
		return ret;
	xform_auth->auth.algo = (uint32_t)ret;
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
			vcrypto->sess_pool) < 0) {
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

static int
vhost_crypto_msg_post_handler(int vid, void *msg, uint32_t *require_reply)
{
	struct virtio_net *dev = get_device(vid);
	struct vhost_crypto *vcrypto;
	VhostUserMsg *vmsg = msg;
	int ret = 0;

	if (dev == NULL || require_reply == NULL) {
		VC_LOG_ERR("Invalid vid %i", vid);
		return -EINVAL;
	}

	vcrypto = dev->extern_data;
	if (vcrypto == NULL) {
		VC_LOG_ERR("Cannot find required data, is it initialized?");
		return -ENOENT;
	}

	*require_reply = 0;

	if (vmsg->request.master == VHOST_USER_CRYPTO_CREATE_SESS) {
		vhost_crypto_create_sess(vcrypto,
				&vmsg->payload.crypto_session);
		*require_reply = 1;
	} else if (vmsg->request.master == VHOST_USER_CRYPTO_CLOSE_SESS)
		ret = vhost_crypto_close_sess(vcrypto, vmsg->payload.u64);
	else
		ret = -EINVAL;

	return ret;
}

static __rte_always_inline struct vring_desc *
find_write_desc(struct vring_desc *head, struct vring_desc *desc)
{
	if (desc->flags & VRING_DESC_F_WRITE)
		return desc;

	while (desc->flags & VRING_DESC_F_NEXT) {
		desc = &head[desc->next];
		if (desc->flags & VRING_DESC_F_WRITE)
			return desc;
	}

	return NULL;
}

static struct virtio_crypto_inhdr *
reach_inhdr(struct vhost_crypto_data_req *vc_req, struct vring_desc *desc)
{
	uint64_t dlen;
	struct virtio_crypto_inhdr *inhdr;

	while (desc->flags & VRING_DESC_F_NEXT)
		desc = &vc_req->head[desc->next];

	dlen = desc->len;
	inhdr = IOVA_TO_VVA(struct virtio_crypto_inhdr *, vc_req, desc->addr,
			&dlen, VHOST_ACCESS_WO);
	if (unlikely(!inhdr || dlen != desc->len))
		return NULL;

	return inhdr;
}

static __rte_always_inline int
move_desc(struct vring_desc *head, struct vring_desc **cur_desc,
		uint32_t size)
{
	struct vring_desc *desc = *cur_desc;
	int left = size;

	rte_prefetch0(&head[desc->next]);
	left -= desc->len;

	while ((desc->flags & VRING_DESC_F_NEXT) && left > 0) {
		desc = &head[desc->next];
		rte_prefetch0(&head[desc->next]);
		left -= desc->len;
	}

	if (unlikely(left > 0)) {
		VC_LOG_ERR("Incorrect virtio descriptor");
		return -1;
	}

	*cur_desc = &head[desc->next];
	return 0;
}

static int
copy_data(void *dst_data, struct vhost_crypto_data_req *vc_req,
		struct vring_desc **cur_desc, uint32_t size)
{
	struct vring_desc *desc = *cur_desc;
	uint64_t remain, addr, dlen, len;
	uint32_t to_copy;
	uint8_t *data = dst_data;
	uint8_t *src;
	int left = size;

	rte_prefetch0(&vc_req->head[desc->next]);
	to_copy = RTE_MIN(desc->len, (uint32_t)left);
	dlen = to_copy;
	src = IOVA_TO_VVA(uint8_t *, vc_req, desc->addr, &dlen,
			VHOST_ACCESS_RO);
	if (unlikely(!src || !dlen)) {
		VC_LOG_ERR("Failed to map descriptor");
		return -1;
	}

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

	*cur_desc = &vc_req->head[desc->next];

	return 0;
}

static __rte_always_inline void *
get_data_ptr(struct vhost_crypto_data_req *vc_req, struct vring_desc **cur_desc,
		uint32_t size, uint8_t perm)
{
	void *data;
	uint64_t dlen = (*cur_desc)->len;

	data = IOVA_TO_VVA(void *, vc_req, (*cur_desc)->addr, &dlen, perm);
	if (unlikely(!data || dlen != (*cur_desc)->len)) {
		VC_LOG_ERR("Failed to map object");
		return NULL;
	}

	if (unlikely(move_desc(vc_req->head, cur_desc, size) < 0))
		return NULL;

	return data;
}

static int
write_back_data(struct rte_crypto_op *op, struct vhost_crypto_data_req *vc_req)
{
	struct rte_mbuf *mbuf = op->sym->m_dst;
	struct vring_desc *head = vc_req->head;
	struct vring_desc *desc = vc_req->wb_desc;
	int left = vc_req->wb_len;
	uint32_t to_write;
	uint8_t *src_data = mbuf->buf_addr, *dst;
	uint64_t dlen;

	rte_prefetch0(&head[desc->next]);
	to_write = RTE_MIN(desc->len, (uint32_t)left);
	dlen = desc->len;
	dst = IOVA_TO_VVA(uint8_t *, vc_req, desc->addr, &dlen,
			VHOST_ACCESS_RW);
	if (unlikely(!dst || dlen != desc->len)) {
		VC_LOG_ERR("Failed to map descriptor");
		return -1;
	}

	rte_memcpy(dst, src_data, to_write);
	left -= to_write;
	src_data += to_write;

	while ((desc->flags & VRING_DESC_F_NEXT) && left > 0) {
		desc = &head[desc->next];
		rte_prefetch0(&head[desc->next]);
		to_write = RTE_MIN(desc->len, (uint32_t)left);
		dlen = desc->len;
		dst = IOVA_TO_VVA(uint8_t *, vc_req, desc->addr, &dlen,
				VHOST_ACCESS_RW);
		if (unlikely(!dst || dlen != desc->len)) {
			VC_LOG_ERR("Failed to map descriptor");
			return -1;
		}

		rte_memcpy(dst, src_data, to_write);
		left -= to_write;
		src_data += to_write;
	}

	if (unlikely(left < 0)) {
		VC_LOG_ERR("Incorrect virtio descriptor");
		return -1;
	}

	return 0;
}

static uint8_t
prepare_sym_cipher_op(struct vhost_crypto *vcrypto, struct rte_crypto_op *op,
		struct vhost_crypto_data_req *vc_req,
		struct virtio_crypto_cipher_data_req *cipher,
		struct vring_desc *cur_desc)
{
	struct vring_desc *desc = cur_desc;
	struct rte_mbuf *m_src = op->sym->m_src, *m_dst = op->sym->m_dst;
	uint8_t *iv_data = rte_crypto_op_ctod_offset(op, uint8_t *, IV_OFFSET);
	uint8_t ret = 0;

	/* prepare */
	/* iv */
	if (unlikely(copy_data(iv_data, vc_req, &desc,
			cipher->para.iv_len) < 0)) {
		ret = VIRTIO_CRYPTO_BADMSG;
		goto error_exit;
	}

	m_src->data_len = cipher->para.src_data_len;

	switch (vcrypto->option) {
	case RTE_VHOST_CRYPTO_ZERO_COPY_ENABLE:
		m_src->buf_iova = gpa_to_hpa(vcrypto->dev, desc->addr,
				cipher->para.src_data_len);
		m_src->buf_addr = get_data_ptr(vc_req, &desc,
				cipher->para.src_data_len, VHOST_ACCESS_RO);
		if (unlikely(m_src->buf_iova == 0 ||
				m_src->buf_addr == NULL)) {
			VC_LOG_ERR("zero_copy may fail due to cross page data");
			ret = VIRTIO_CRYPTO_ERR;
			goto error_exit;
		}
		break;
	case RTE_VHOST_CRYPTO_ZERO_COPY_DISABLE:
		if (unlikely(cipher->para.src_data_len >
				RTE_MBUF_DEFAULT_BUF_SIZE)) {
			VC_LOG_ERR("Not enough space to do data copy");
			ret = VIRTIO_CRYPTO_ERR;
			goto error_exit;
		}
		if (unlikely(copy_data(rte_pktmbuf_mtod(m_src, uint8_t *),
				vc_req, &desc, cipher->para.src_data_len)
				< 0)) {
			ret = VIRTIO_CRYPTO_BADMSG;
			goto error_exit;
		}
		break;
	default:
		ret = VIRTIO_CRYPTO_BADMSG;
		goto error_exit;
	}

	/* dst */
	desc = find_write_desc(vc_req->head, desc);
	if (unlikely(!desc)) {
		VC_LOG_ERR("Cannot find write location");
		ret = VIRTIO_CRYPTO_BADMSG;
		goto error_exit;
	}

	switch (vcrypto->option) {
	case RTE_VHOST_CRYPTO_ZERO_COPY_ENABLE:
		m_dst->buf_iova = gpa_to_hpa(vcrypto->dev,
				desc->addr, cipher->para.dst_data_len);
		m_dst->buf_addr = get_data_ptr(vc_req, &desc,
				cipher->para.dst_data_len, VHOST_ACCESS_RW);
		if (unlikely(m_dst->buf_iova == 0 || m_dst->buf_addr == NULL)) {
			VC_LOG_ERR("zero_copy may fail due to cross page data");
			ret = VIRTIO_CRYPTO_ERR;
			goto error_exit;
		}

		m_dst->data_len = cipher->para.dst_data_len;
		break;
	case RTE_VHOST_CRYPTO_ZERO_COPY_DISABLE:
		vc_req->wb_desc = desc;
		vc_req->wb_len = cipher->para.dst_data_len;
		if (unlikely(move_desc(vc_req->head, &desc,
				vc_req->wb_len) < 0)) {
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

	vc_req->inhdr = get_data_ptr(vc_req, &desc, INHDR_LEN, VHOST_ACCESS_WO);
	if (unlikely(vc_req->inhdr == NULL)) {
		ret = VIRTIO_CRYPTO_BADMSG;
		goto error_exit;
	}

	vc_req->inhdr->status = VIRTIO_CRYPTO_OK;
	vc_req->len = cipher->para.dst_data_len + INHDR_LEN;

	return 0;

error_exit:
	vc_req->len = INHDR_LEN;
	return ret;
}

static uint8_t
prepare_sym_chain_op(struct vhost_crypto *vcrypto, struct rte_crypto_op *op,
		struct vhost_crypto_data_req *vc_req,
		struct virtio_crypto_alg_chain_data_req *chain,
		struct vring_desc *cur_desc)
{
	struct vring_desc *desc = cur_desc;
	struct rte_mbuf *m_src = op->sym->m_src, *m_dst = op->sym->m_dst;
	uint8_t *iv_data = rte_crypto_op_ctod_offset(op, uint8_t *, IV_OFFSET);
	uint32_t digest_offset;
	void *digest_addr;
	uint8_t ret = 0;

	/* prepare */
	/* iv */
	if (unlikely(copy_data(iv_data, vc_req, &desc,
			chain->para.iv_len) < 0)) {
		ret = VIRTIO_CRYPTO_BADMSG;
		goto error_exit;
	}

	m_src->data_len = chain->para.src_data_len;
	m_dst->data_len = chain->para.dst_data_len;

	switch (vcrypto->option) {
	case RTE_VHOST_CRYPTO_ZERO_COPY_ENABLE:
		m_src->buf_iova = gpa_to_hpa(vcrypto->dev, desc->addr,
				chain->para.src_data_len);
		m_src->buf_addr = get_data_ptr(vc_req, &desc,
				chain->para.src_data_len, VHOST_ACCESS_RO);
		if (unlikely(m_src->buf_iova == 0 || m_src->buf_addr == NULL)) {
			VC_LOG_ERR("zero_copy may fail due to cross page data");
			ret = VIRTIO_CRYPTO_ERR;
			goto error_exit;
		}
		break;
	case RTE_VHOST_CRYPTO_ZERO_COPY_DISABLE:
		if (unlikely(chain->para.src_data_len >
				RTE_MBUF_DEFAULT_BUF_SIZE)) {
			VC_LOG_ERR("Not enough space to do data copy");
			ret = VIRTIO_CRYPTO_ERR;
			goto error_exit;
		}
		if (unlikely(copy_data(rte_pktmbuf_mtod(m_src, uint8_t *),
				vc_req, &desc, chain->para.src_data_len)) < 0) {
			ret = VIRTIO_CRYPTO_BADMSG;
			goto error_exit;
		}
		break;
	default:
		ret = VIRTIO_CRYPTO_BADMSG;
		goto error_exit;
	}

	/* dst */
	desc = find_write_desc(vc_req->head, desc);
	if (unlikely(!desc)) {
		VC_LOG_ERR("Cannot find write location");
		ret = VIRTIO_CRYPTO_BADMSG;
		goto error_exit;
	}

	switch (vcrypto->option) {
	case RTE_VHOST_CRYPTO_ZERO_COPY_ENABLE:
		m_dst->buf_iova = gpa_to_hpa(vcrypto->dev,
				desc->addr, chain->para.dst_data_len);
		m_dst->buf_addr = get_data_ptr(vc_req, &desc,
				chain->para.dst_data_len, VHOST_ACCESS_RW);
		if (unlikely(m_dst->buf_iova == 0 || m_dst->buf_addr == NULL)) {
			VC_LOG_ERR("zero_copy may fail due to cross page data");
			ret = VIRTIO_CRYPTO_ERR;
			goto error_exit;
		}

		op->sym->auth.digest.phys_addr = gpa_to_hpa(vcrypto->dev,
				desc->addr, chain->para.hash_result_len);
		op->sym->auth.digest.data = get_data_ptr(vc_req, &desc,
				chain->para.hash_result_len, VHOST_ACCESS_RW);
		if (unlikely(op->sym->auth.digest.phys_addr == 0)) {
			VC_LOG_ERR("zero_copy may fail due to cross page data");
			ret = VIRTIO_CRYPTO_ERR;
			goto error_exit;
		}
		break;
	case RTE_VHOST_CRYPTO_ZERO_COPY_DISABLE:
		digest_offset = m_dst->data_len;
		digest_addr = rte_pktmbuf_mtod_offset(m_dst, void *,
				digest_offset);

		vc_req->wb_desc = desc;
		vc_req->wb_len = m_dst->data_len + chain->para.hash_result_len;

		if (unlikely(move_desc(vc_req->head, &desc,
				chain->para.dst_data_len) < 0)) {
			ret = VIRTIO_CRYPTO_BADMSG;
			goto error_exit;
		}

		if (unlikely(copy_data(digest_addr, vc_req, &desc,
				chain->para.hash_result_len)) < 0) {
			ret = VIRTIO_CRYPTO_BADMSG;
			goto error_exit;
		}

		op->sym->auth.digest.data = digest_addr;
		op->sym->auth.digest.phys_addr = rte_pktmbuf_iova_offset(m_dst,
				digest_offset);
		break;
	default:
		ret = VIRTIO_CRYPTO_BADMSG;
		goto error_exit;
	}

	/* record inhdr */
	vc_req->inhdr = get_data_ptr(vc_req, &desc, INHDR_LEN, VHOST_ACCESS_WO);
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
	struct vhost_crypto_data_req *vc_req = RTE_PTR_ADD(op->sym->m_src,
			sizeof(struct rte_mbuf));
	struct rte_cryptodev_sym_session *session;
	struct virtio_crypto_op_data_req *req, tmp_req;
	struct virtio_crypto_inhdr *inhdr;
	struct vring_desc *desc = NULL;
	uint64_t session_id;
	uint64_t dlen;
	int err = 0;

	vc_req->desc_idx = desc_idx;
	vc_req->dev = vcrypto->dev;
	vc_req->vq = vq;

	if (likely(head->flags & VRING_DESC_F_INDIRECT)) {
		dlen = head->len;
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

	req = get_data_ptr(vc_req, &desc, sizeof(*req), VHOST_ACCESS_RO);
	if (unlikely(req == NULL)) {
		switch (vcrypto->option) {
		case RTE_VHOST_CRYPTO_ZERO_COPY_ENABLE:
			err = VIRTIO_CRYPTO_BADMSG;
			VC_LOG_ERR("Invalid descriptor");
			goto error_exit;
		case RTE_VHOST_CRYPTO_ZERO_COPY_DISABLE:
			req = &tmp_req;
			if (unlikely(copy_data(req, vc_req, &desc, sizeof(*req))
					< 0)) {
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
					&req->u.sym_req.u.cipher, desc);
			break;
		case VIRTIO_CRYPTO_SYM_OP_ALGORITHM_CHAINING:
			err = prepare_sym_chain_op(vcrypto, op, vc_req,
					&req->u.sym_req.u.chain, desc);
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

	inhdr = reach_inhdr(vc_req, desc);
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
	struct vhost_crypto_data_req *vc_req = RTE_PTR_ADD(m_src,
			sizeof(struct rte_mbuf));
	uint16_t desc_idx;
	int ret = 0;

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
		if (vc_req->zero_copy == 0) {
			ret = write_back_data(op, vc_req);
			if (unlikely(ret != 0))
				vc_req->inhdr->status = VIRTIO_CRYPTO_ERR;
		}
	}

	vc_req->vq->used->ring[desc_idx].id = desc_idx;
	vc_req->vq->used->ring[desc_idx].len = vc_req->len;

	rte_mempool_put(m_dst->pool, (void *)m_dst);
	rte_mempool_put(m_src->pool, (void *)m_src);

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

int __rte_experimental
rte_vhost_crypto_create(int vid, uint8_t cryptodev_id,
		struct rte_mempool *sess_pool, int socket_id)
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

int __rte_experimental
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
	rte_free(vcrypto);

	dev->extern_data = NULL;
	dev->extern_ops.pre_msg_handle = NULL;
	dev->extern_ops.post_msg_handle = NULL;

	return 0;
}

int __rte_experimental
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

	if (!(rte_mempool_full(vcrypto->mbuf_pool))) {
		VC_LOG_ERR("Cannot update zero copy as mempool is not full");
		return -EINVAL;
	}

	vcrypto->option = (uint8_t)option;

	return 0;
}

uint16_t __rte_experimental
rte_vhost_crypto_fetch_requests(int vid, uint32_t qid,
		struct rte_crypto_op **ops, uint16_t nb_ops)
{
	struct rte_mbuf *mbufs[VHOST_CRYPTO_MAX_BURST_SIZE * 2];
	struct virtio_net *dev = get_device(vid);
	struct vhost_crypto *vcrypto;
	struct vhost_virtqueue *vq;
	uint16_t avail_idx;
	uint16_t start_idx;
	uint16_t required;
	uint16_t count;
	uint16_t i;

	if (unlikely(dev == NULL)) {
		VC_LOG_ERR("Invalid vid %i", vid);
		return -EINVAL;
	}

	if (unlikely(qid >= VHOST_MAX_QUEUE_PAIRS)) {
		VC_LOG_ERR("Invalid qid %u", qid);
		return -EINVAL;
	}

	vcrypto = (struct vhost_crypto *)dev->extern_data;
	if (unlikely(vcrypto == NULL)) {
		VC_LOG_ERR("Cannot find required data, is it initialized?");
		return -ENOENT;
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
	required = count * 2;
	if (unlikely(rte_mempool_get_bulk(vcrypto->mbuf_pool, (void **)mbufs,
			required) < 0)) {
		VC_LOG_ERR("Insufficient memory");
		return -ENOMEM;
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

		if (unlikely(vhost_crypto_process_one_req(vcrypto, vq, op, head,
				desc_idx)) < 0)
			break;
	}

	vq->last_used_idx += i;

	return i;
}

uint16_t __rte_experimental
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
