/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */
#include <rte_malloc.h>
#include <rte_hash.h>
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

#define GPA_TO_VVA(t, m, a)	((t)(uintptr_t)rte_vhost_gpa_to_vva(m, a))

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
	struct rte_vhost_memory *mem;
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
reach_inhdr(struct vring_desc *head, struct rte_vhost_memory *mem,
		struct vring_desc *desc)
{
	while (desc->flags & VRING_DESC_F_NEXT)
		desc = &head[desc->next];

	return GPA_TO_VVA(struct virtio_crypto_inhdr *, mem, desc->addr);
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

	if (unlikely(left < 0)) {
		VC_LOG_ERR("Incorrect virtio descriptor");
		return -1;
	}

	*cur_desc = &head[desc->next];
	return 0;
}

static int
copy_data(void *dst_data, struct vring_desc *head, struct rte_vhost_memory *mem,
		struct vring_desc **cur_desc, uint32_t size)
{
	struct vring_desc *desc = *cur_desc;
	uint32_t to_copy;
	uint8_t *data = dst_data;
	uint8_t *src;
	int left = size;

	rte_prefetch0(&head[desc->next]);
	to_copy = RTE_MIN(desc->len, (uint32_t)left);
	src = GPA_TO_VVA(uint8_t *, mem, desc->addr);
	rte_memcpy((uint8_t *)data, src, to_copy);
	left -= to_copy;

	while ((desc->flags & VRING_DESC_F_NEXT) && left > 0) {
		desc = &head[desc->next];
		rte_prefetch0(&head[desc->next]);
		to_copy = RTE_MIN(desc->len, (uint32_t)left);
		src = GPA_TO_VVA(uint8_t *, mem, desc->addr);
		rte_memcpy(data + size - left, src, to_copy);
		left -= to_copy;
	}

	if (unlikely(left < 0)) {
		VC_LOG_ERR("Incorrect virtio descriptor");
		return -1;
	}

	*cur_desc = &head[desc->next];

	return 0;
}

static __rte_always_inline void *
get_data_ptr(struct vring_desc *head, struct rte_vhost_memory *mem,
		struct vring_desc **cur_desc, uint32_t size)
{
	void *data;

	data = GPA_TO_VVA(void *, mem, (*cur_desc)->addr);
	if (unlikely(!data)) {
		VC_LOG_ERR("Failed to get object");
		return NULL;
	}

	if (unlikely(move_desc(head, cur_desc, size) < 0))
		return NULL;

	return data;
}

static int
write_back_data(struct rte_crypto_op *op, struct vhost_crypto_data_req *vc_req)
{
	struct rte_mbuf *mbuf = op->sym->m_dst;
	struct vring_desc *head = vc_req->head;
	struct rte_vhost_memory *mem = vc_req->mem;
	struct vring_desc *desc = vc_req->wb_desc;
	int left = vc_req->wb_len;
	uint32_t to_write;
	uint8_t *src_data = mbuf->buf_addr, *dst;

	rte_prefetch0(&head[desc->next]);
	to_write = RTE_MIN(desc->len, (uint32_t)left);
	dst = GPA_TO_VVA(uint8_t *, mem, desc->addr);
	rte_memcpy(dst, src_data, to_write);
	left -= to_write;
	src_data += to_write;

	while ((desc->flags & VRING_DESC_F_NEXT) && left > 0) {
		desc = &head[desc->next];
		rte_prefetch0(&head[desc->next]);
		to_write = RTE_MIN(desc->len, (uint32_t)left);
		dst = GPA_TO_VVA(uint8_t *, mem, desc->addr);
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
	struct vring_desc *head = vc_req->head;
	struct vring_desc *desc = cur_desc;
	struct rte_vhost_memory *mem = vc_req->mem;
	struct rte_mbuf *m_src = op->sym->m_src, *m_dst = op->sym->m_dst;
	uint8_t *iv_data = rte_crypto_op_ctod_offset(op, uint8_t *, IV_OFFSET);
	uint8_t ret = 0;

	/* prepare */
	/* iv */
	if (unlikely(copy_data(iv_data, head, mem, &desc,
			cipher->para.iv_len) < 0)) {
		ret = VIRTIO_CRYPTO_BADMSG;
		goto error_exit;
	}

	m_src->data_len = cipher->para.src_data_len;

	switch (vcrypto->option) {
	case RTE_VHOST_CRYPTO_ZERO_COPY_ENABLE:
		m_src->buf_iova = gpa_to_hpa(vcrypto->dev, desc->addr,
				cipher->para.src_data_len);
		m_src->buf_addr = get_data_ptr(head, mem, &desc,
				cipher->para.src_data_len);
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
		if (unlikely(copy_data(rte_pktmbuf_mtod(m_src, uint8_t *), head,
				mem, &desc, cipher->para.src_data_len))
				< 0) {
			ret = VIRTIO_CRYPTO_BADMSG;
			goto error_exit;
		}
		break;
	default:
		ret = VIRTIO_CRYPTO_BADMSG;
		goto error_exit;
	}

	/* dst */
	desc = find_write_desc(head, desc);
	if (unlikely(!desc)) {
		VC_LOG_ERR("Cannot find write location");
		ret = VIRTIO_CRYPTO_BADMSG;
		goto error_exit;
	}

	switch (vcrypto->option) {
	case RTE_VHOST_CRYPTO_ZERO_COPY_ENABLE:
		m_dst->buf_iova = gpa_to_hpa(vcrypto->dev,
				desc->addr, cipher->para.dst_data_len);
		m_dst->buf_addr = get_data_ptr(head, mem, &desc,
				cipher->para.dst_data_len);
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
		if (unlikely(move_desc(head, &desc, vc_req->wb_len) < 0)) {
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

	vc_req->inhdr = get_data_ptr(head, mem, &desc, INHDR_LEN);
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
	struct vring_desc *head = vc_req->head;
	struct vring_desc *desc = cur_desc;
	struct rte_vhost_memory *mem = vc_req->mem;
	struct rte_mbuf *m_src = op->sym->m_src, *m_dst = op->sym->m_dst;
	uint8_t *iv_data = rte_crypto_op_ctod_offset(op, uint8_t *, IV_OFFSET);
	uint32_t digest_offset;
	void *digest_addr;
	uint8_t ret = 0;

	/* prepare */
	/* iv */
	if (unlikely(copy_data(iv_data, head, mem, &desc,
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
		m_src->buf_addr = get_data_ptr(head, mem, &desc,
				chain->para.src_data_len);
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
		if (unlikely(copy_data(rte_pktmbuf_mtod(m_src, uint8_t *), head,
				mem, &desc, chain->para.src_data_len)) < 0) {
			ret = VIRTIO_CRYPTO_BADMSG;
			goto error_exit;
		}
		break;
	default:
		ret = VIRTIO_CRYPTO_BADMSG;
		goto error_exit;
	}

	/* dst */
	desc = find_write_desc(head, desc);
	if (unlikely(!desc)) {
		VC_LOG_ERR("Cannot find write location");
		ret = VIRTIO_CRYPTO_BADMSG;
		goto error_exit;
	}

	switch (vcrypto->option) {
	case RTE_VHOST_CRYPTO_ZERO_COPY_ENABLE:
		m_dst->buf_iova = gpa_to_hpa(vcrypto->dev,
				desc->addr, chain->para.dst_data_len);
		m_dst->buf_addr = get_data_ptr(head, mem, &desc,
				chain->para.dst_data_len);
		if (unlikely(m_dst->buf_iova == 0 || m_dst->buf_addr == NULL)) {
			VC_LOG_ERR("zero_copy may fail due to cross page data");
			ret = VIRTIO_CRYPTO_ERR;
			goto error_exit;
		}

		op->sym->auth.digest.phys_addr = gpa_to_hpa(vcrypto->dev,
				desc->addr, chain->para.hash_result_len);
		op->sym->auth.digest.data = get_data_ptr(head, mem, &desc,
				chain->para.hash_result_len);
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

		if (unlikely(move_desc(head, &desc,
				chain->para.dst_data_len) < 0)) {
			ret = VIRTIO_CRYPTO_BADMSG;
			goto error_exit;
		}

		if (unlikely(copy_data(digest_addr, head, mem, &desc,
				chain->para.hash_result_len)) < 0) {
			ret = VIRTIO_CRYPTO_BADMSG;
			goto error_exit;
		}

		op->sym->auth.digest.data = digest_addr;
		op->sym->auth.digest.phys_addr = rte_pktmbuf_iova_offset(m_dst,
				digest_offset);
		if (unlikely(move_desc(head, &desc,
				chain->para.hash_result_len) < 0)) {
			ret = VIRTIO_CRYPTO_ERR;
			goto error_exit;
		}
		break;
	default:
		ret = VIRTIO_CRYPTO_BADMSG;
		goto error_exit;
	}

	/* record inhdr */
	vc_req->inhdr = get_data_ptr(head, mem, &desc, INHDR_LEN);
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
		struct vring_desc *head, uint16_t desc_idx,
		struct rte_vhost_memory *mem)
{
	struct vhost_crypto_data_req *vc_req = RTE_PTR_ADD(op->sym->m_src,
			sizeof(struct rte_mbuf));
	struct rte_cryptodev_sym_session *session;
	struct virtio_crypto_op_data_req *req;
	struct virtio_crypto_inhdr *inhdr;
	struct vring_desc *desc = NULL;
	uint64_t session_id;
	int err = 0;

	vc_req->desc_idx = desc_idx;

	if (likely(head->flags & VRING_DESC_F_INDIRECT)) {
		head = GPA_TO_VVA(struct vring_desc *, mem, head->addr);
		if (unlikely(!head))
			return 0;
		desc_idx = 0;
	}

	desc = head;

	vc_req->mem = mem;
	vc_req->head = head;
	vc_req->vq = vq;

	vc_req->zero_copy = vcrypto->option;

	req = get_data_ptr(head, mem, &desc, sizeof(*req));
	if (unlikely(req == NULL)) {
		err = VIRTIO_CRYPTO_ERR;
		VC_LOG_ERR("Invalid descriptor");
		goto error_exit;
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

	inhdr = reach_inhdr(head, mem, desc);
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
