/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 NXP.
 * Copyright(c) 2017 Intel Corporation.
 * Copyright (c) 2020 Samsung Electronics Co., Ltd All Rights Reserved
 */

#include <ctype.h>
#include <stdlib.h>

#include <rte_cryptodev.h>
#include <dev_driver.h>
#include <rte_telemetry.h>
#include "rte_security.h"
#include "rte_security_driver.h"

/* Macro to check for invalid pointers */
#define RTE_PTR_OR_ERR_RET(ptr, retval) do {	\
	if ((ptr) == NULL)			\
		return retval;			\
} while (0)

/* Macro to check for invalid pointers chains */
#define RTE_PTR_CHAIN3_OR_ERR_RET(p1, p2, p3, retval, last_retval) do {	\
	RTE_PTR_OR_ERR_RET(p1, retval);					\
	RTE_PTR_OR_ERR_RET(p1->p2, retval);				\
	RTE_PTR_OR_ERR_RET(p1->p2->p3, last_retval);			\
} while (0)

#define RTE_SECURITY_DYNFIELD_NAME "rte_security_dynfield_metadata"
#define RTE_SECURITY_OOP_DYNFIELD_NAME "rte_security_oop_dynfield_metadata"

int rte_security_dynfield_offset = -1;
int rte_security_oop_dynfield_offset = -1;

int
rte_security_dynfield_register(void)
{
	static const struct rte_mbuf_dynfield dynfield_desc = {
		.name = RTE_SECURITY_DYNFIELD_NAME,
		.size = sizeof(rte_security_dynfield_t),
		.align = __alignof__(rte_security_dynfield_t),
	};
	rte_security_dynfield_offset =
		rte_mbuf_dynfield_register(&dynfield_desc);
	return rte_security_dynfield_offset;
}

int
rte_security_oop_dynfield_register(void)
{
	static const struct rte_mbuf_dynfield dynfield_desc = {
		.name = RTE_SECURITY_OOP_DYNFIELD_NAME,
		.size = sizeof(rte_security_oop_dynfield_t),
		.align = __alignof__(rte_security_oop_dynfield_t),
	};

	rte_security_oop_dynfield_offset =
		rte_mbuf_dynfield_register(&dynfield_desc);
	return rte_security_oop_dynfield_offset;
}

void *
rte_security_session_create(void *ctx,
			    struct rte_security_session_conf *conf,
			    struct rte_mempool *mp)
{
	struct rte_security_session *sess = NULL;
	struct rte_security_ctx *instance = ctx;
	uint32_t sess_priv_size;

	RTE_PTR_CHAIN3_OR_ERR_RET(instance, ops, session_create, NULL, NULL);
	RTE_PTR_OR_ERR_RET(conf, NULL);
	RTE_PTR_OR_ERR_RET(mp, NULL);

	sess_priv_size = instance->ops->session_get_size(instance->device);
	if (mp->elt_size < (sizeof(struct rte_security_session) + sess_priv_size))
		return NULL;

	if (rte_mempool_get(mp, (void **)&sess))
		return NULL;

	/* Clear session priv data */
	memset(sess->driver_priv_data, 0, sess_priv_size);

	sess->driver_priv_data_iova = rte_mempool_virt2iova(sess) +
			offsetof(struct rte_security_session, driver_priv_data);
	if (instance->ops->session_create(instance->device, conf, sess)) {
		rte_mempool_put(mp, (void *)sess);
		return NULL;
	}
	instance->sess_cnt++;

	return (void *)sess;
}

int
rte_security_session_update(void *ctx, void *sess, struct rte_security_session_conf *conf)
{
	struct rte_security_ctx *instance = ctx;

	RTE_PTR_CHAIN3_OR_ERR_RET(instance, ops, session_update, -EINVAL,
			-ENOTSUP);
	RTE_PTR_OR_ERR_RET(sess, -EINVAL);
	RTE_PTR_OR_ERR_RET(conf, -EINVAL);

	return instance->ops->session_update(instance->device, sess, conf);
}

unsigned int
rte_security_session_get_size(void *ctx)
{
	struct rte_security_ctx *instance = ctx;

	RTE_PTR_CHAIN3_OR_ERR_RET(instance, ops, session_get_size, 0, 0);

	return (sizeof(struct rte_security_session) +
			instance->ops->session_get_size(instance->device));
}

int
rte_security_session_stats_get(void *ctx, void *sess, struct rte_security_stats *stats)
{
	struct rte_security_ctx *instance = ctx;

	RTE_PTR_CHAIN3_OR_ERR_RET(instance, ops, session_stats_get, -EINVAL,
			-ENOTSUP);
	/* Parameter sess can be NULL in case of getting global statistics. */
	RTE_PTR_OR_ERR_RET(stats, -EINVAL);

	return instance->ops->session_stats_get(instance->device, sess, stats);
}

int
rte_security_session_destroy(void *ctx, void *sess)
{
	struct rte_security_ctx *instance = ctx;
	int ret;

	RTE_PTR_CHAIN3_OR_ERR_RET(instance, ops, session_destroy, -EINVAL,
			-ENOTSUP);
	RTE_PTR_OR_ERR_RET(sess, -EINVAL);

	ret = instance->ops->session_destroy(instance->device, sess);
	if (ret != 0)
		return ret;

	rte_mempool_put(rte_mempool_from_obj(sess), (void *)sess);

	if (instance->sess_cnt)
		instance->sess_cnt--;

	return 0;
}

int
rte_security_macsec_sc_create(void *ctx, struct rte_security_macsec_sc *conf)
{
	struct rte_security_ctx *instance = ctx;
	int sc_id;

	RTE_PTR_CHAIN3_OR_ERR_RET(instance, ops, macsec_sc_create, -EINVAL, -ENOTSUP);
	RTE_PTR_OR_ERR_RET(conf, -EINVAL);

	sc_id = instance->ops->macsec_sc_create(instance->device, conf);
	if (sc_id >= 0)
		instance->macsec_sc_cnt++;

	return sc_id;
}

int
rte_security_macsec_sa_create(void *ctx, struct rte_security_macsec_sa *conf)
{
	struct rte_security_ctx *instance = ctx;
	int sa_id;

	RTE_PTR_CHAIN3_OR_ERR_RET(instance, ops, macsec_sa_create, -EINVAL, -ENOTSUP);
	RTE_PTR_OR_ERR_RET(conf, -EINVAL);

	sa_id = instance->ops->macsec_sa_create(instance->device, conf);
	if (sa_id >= 0)
		instance->macsec_sa_cnt++;

	return sa_id;
}

int
rte_security_macsec_sc_destroy(void *ctx, uint16_t sc_id,
			       enum rte_security_macsec_direction dir)
{
	struct rte_security_ctx *instance = ctx;
	int ret;

	RTE_PTR_CHAIN3_OR_ERR_RET(instance, ops, macsec_sc_destroy, -EINVAL, -ENOTSUP);

	ret = instance->ops->macsec_sc_destroy(instance->device, sc_id, dir);
	if (ret != 0)
		return ret;

	if (instance->macsec_sc_cnt)
		instance->macsec_sc_cnt--;

	return 0;
}

int
rte_security_macsec_sa_destroy(void *ctx, uint16_t sa_id,
			       enum rte_security_macsec_direction dir)
{
	struct rte_security_ctx *instance = ctx;
	int ret;

	RTE_PTR_CHAIN3_OR_ERR_RET(instance, ops, macsec_sa_destroy, -EINVAL, -ENOTSUP);

	ret = instance->ops->macsec_sa_destroy(instance->device, sa_id, dir);
	if (ret != 0)
		return ret;

	if (instance->macsec_sa_cnt)
		instance->macsec_sa_cnt--;

	return 0;
}

int
rte_security_macsec_sc_stats_get(void *ctx, uint16_t sc_id,
				 enum rte_security_macsec_direction dir,
				 struct rte_security_macsec_sc_stats *stats)
{
	struct rte_security_ctx *instance = ctx;

	RTE_PTR_CHAIN3_OR_ERR_RET(instance, ops, macsec_sc_stats_get, -EINVAL, -ENOTSUP);
	RTE_PTR_OR_ERR_RET(stats, -EINVAL);

	return instance->ops->macsec_sc_stats_get(instance->device, sc_id, dir, stats);
}

int
rte_security_macsec_sa_stats_get(void *ctx, uint16_t sa_id,
				 enum rte_security_macsec_direction dir,
				 struct rte_security_macsec_sa_stats *stats)
{
	struct rte_security_ctx *instance = ctx;

	RTE_PTR_CHAIN3_OR_ERR_RET(instance, ops, macsec_sa_stats_get, -EINVAL, -ENOTSUP);
	RTE_PTR_OR_ERR_RET(stats, -EINVAL);

	return instance->ops->macsec_sa_stats_get(instance->device, sa_id, dir, stats);
}

int
__rte_security_set_pkt_metadata(void *ctx, void *sess, struct rte_mbuf *m, void *params)
{
	struct rte_security_ctx *instance = ctx;
#ifdef RTE_DEBUG
	RTE_PTR_OR_ERR_RET(sess, -EINVAL);
	RTE_PTR_OR_ERR_RET(instance, -EINVAL);
	RTE_PTR_OR_ERR_RET(instance->ops, -EINVAL);
#endif
	if (*instance->ops->set_pkt_metadata == NULL)
		return -ENOTSUP;
	return instance->ops->set_pkt_metadata(instance->device,
					       sess, m, params);
}

const struct rte_security_capability *
rte_security_capabilities_get(void *ctx)
{
	struct rte_security_ctx *instance = ctx;

	RTE_PTR_CHAIN3_OR_ERR_RET(instance, ops, capabilities_get, NULL, NULL);

	return instance->ops->capabilities_get(instance->device);
}

const struct rte_security_capability *
rte_security_capability_get(void *ctx, struct rte_security_capability_idx *idx)
{
	const struct rte_security_capability *capabilities;
	const struct rte_security_capability *capability;
	struct rte_security_ctx *instance = ctx;
	uint16_t i = 0;

	RTE_PTR_CHAIN3_OR_ERR_RET(instance, ops, capabilities_get, NULL, NULL);
	RTE_PTR_OR_ERR_RET(idx, NULL);

	capabilities = instance->ops->capabilities_get(instance->device);

	if (capabilities == NULL)
		return NULL;

	while ((capability = &capabilities[i++])->action
			!= RTE_SECURITY_ACTION_TYPE_NONE) {
		if (capability->action == idx->action &&
				capability->protocol == idx->protocol) {
			if (idx->protocol == RTE_SECURITY_PROTOCOL_IPSEC) {
				if (capability->ipsec.proto ==
						idx->ipsec.proto &&
					capability->ipsec.mode ==
							idx->ipsec.mode &&
					capability->ipsec.direction ==
							idx->ipsec.direction)
					return capability;
			} else if (idx->protocol == RTE_SECURITY_PROTOCOL_PDCP) {
				if (capability->pdcp.domain ==
							idx->pdcp.domain)
					return capability;
			} else if (idx->protocol ==
						RTE_SECURITY_PROTOCOL_DOCSIS) {
				if (capability->docsis.direction ==
							idx->docsis.direction)
					return capability;
			} else if (idx->protocol ==
						RTE_SECURITY_PROTOCOL_MACSEC) {
				if (idx->macsec.alg == capability->macsec.alg)
					return capability;
			} else if (idx->protocol == RTE_SECURITY_PROTOCOL_TLS_RECORD) {
				if (capability->tls_record.ver == idx->tls_record.ver &&
				    capability->tls_record.type == idx->tls_record.type)
					return capability;
			}
		}
	}

	return NULL;
}

int
rte_security_rx_inject_configure(void *ctx, uint16_t port_id, bool enable)
{
	struct rte_security_ctx *instance = ctx;

	RTE_PTR_OR_ERR_RET(instance, -EINVAL);
	RTE_PTR_OR_ERR_RET(instance->ops, -ENOTSUP);
	RTE_PTR_OR_ERR_RET(instance->ops->rx_inject_configure, -ENOTSUP);

	return instance->ops->rx_inject_configure(instance->device, port_id, enable);
}

uint16_t
rte_security_inb_pkt_rx_inject(void *ctx, struct rte_mbuf **pkts, void **sess,
			       uint16_t nb_pkts)
{
	struct rte_security_ctx *instance = ctx;

	return instance->ops->inb_pkt_rx_inject(instance->device, pkts,
						(struct rte_security_session **)sess, nb_pkts);
}

static int
security_handle_cryptodev_list(const char *cmd __rte_unused,
			       const char *params __rte_unused,
			       struct rte_tel_data *d)
{
	int dev_id;

	if (rte_cryptodev_count() < 1)
		return -1;

	rte_tel_data_start_array(d, RTE_TEL_INT_VAL);
	for (dev_id = 0; dev_id < RTE_CRYPTO_MAX_DEVS; dev_id++)
		if (rte_cryptodev_is_valid_dev(dev_id) &&
		    rte_cryptodev_get_sec_ctx(dev_id))
			rte_tel_data_add_array_int(d, dev_id);

	return 0;
}

#define CRYPTO_CAPS_SZ                                             \
	(RTE_ALIGN_CEIL(sizeof(struct rte_cryptodev_capabilities), \
			sizeof(uint64_t)) /	sizeof(uint64_t))

static int
crypto_caps_array(struct rte_tel_data *d,
		  const struct rte_cryptodev_capabilities *capabilities)
{
	const struct rte_cryptodev_capabilities *dev_caps;
	uint64_t caps_val[CRYPTO_CAPS_SZ];
	unsigned int i = 0, j;

	rte_tel_data_start_array(d, RTE_TEL_UINT_VAL);

	while ((dev_caps = &capabilities[i++])->op !=
	   RTE_CRYPTO_OP_TYPE_UNDEFINED) {
		memset(&caps_val, 0, CRYPTO_CAPS_SZ * sizeof(caps_val[0]));
		rte_memcpy(caps_val, dev_caps, sizeof(capabilities[0]));
		for (j = 0; j < CRYPTO_CAPS_SZ; j++)
			rte_tel_data_add_array_uint(d, caps_val[j]);
	}

	return (i - 1);
}

#define SEC_CAPS_SZ						\
	(RTE_ALIGN_CEIL(sizeof(struct rte_security_capability), \
			sizeof(uint64_t)) /	sizeof(uint64_t))

static int
sec_caps_array(struct rte_tel_data *d,
	       const struct rte_security_capability *capabilities)
{
	const struct rte_security_capability *dev_caps;
	uint64_t caps_val[SEC_CAPS_SZ];
	unsigned int i = 0, j;

	rte_tel_data_start_array(d, RTE_TEL_UINT_VAL);

	while ((dev_caps = &capabilities[i++])->action !=
	   RTE_SECURITY_ACTION_TYPE_NONE) {
		memset(&caps_val, 0, SEC_CAPS_SZ * sizeof(caps_val[0]));
		rte_memcpy(caps_val, dev_caps, sizeof(capabilities[0]));
		for (j = 0; j < SEC_CAPS_SZ; j++)
			rte_tel_data_add_array_uint(d, caps_val[j]);
	}

	return i - 1;
}

static const struct rte_security_capability *
security_capability_by_index(const struct rte_security_capability *capabilities,
			     int index)
{
	const struct rte_security_capability *dev_caps = NULL;
	int i = 0;

	while ((dev_caps = &capabilities[i])->action !=
	   RTE_SECURITY_ACTION_TYPE_NONE) {
		if (i == index)
			return dev_caps;

		++i;
	}

	return NULL;
}

static int
security_capabilities_from_dev_id(int dev_id, const void **caps)
{
	const struct rte_security_capability *capabilities;
	void *sec_ctx;

	if (rte_cryptodev_is_valid_dev(dev_id) == 0)
		return -EINVAL;

	sec_ctx = rte_cryptodev_get_sec_ctx(dev_id);
	RTE_PTR_OR_ERR_RET(sec_ctx, -EINVAL);

	capabilities = rte_security_capabilities_get(sec_ctx);
	RTE_PTR_OR_ERR_RET(capabilities, -EINVAL);

	*caps = capabilities;
	return 0;
}

static int
security_handle_cryptodev_sec_caps(const char *cmd __rte_unused, const char *params,
				   struct rte_tel_data *d)
{
	const struct rte_security_capability *capabilities;
	struct rte_tel_data *sec_caps;
	char *end_param;
	int sec_caps_n;
	int dev_id;
	int rc;

	if (!params || strlen(params) == 0 || !isdigit(*params))
		return -EINVAL;

	dev_id = strtoul(params, &end_param, 0);
	if (*end_param != '\0')
		CDEV_LOG_ERR("Extra parameters passed to command, ignoring");

	rc = security_capabilities_from_dev_id(dev_id, (void *)&capabilities);
	if (rc < 0)
		return rc;

	sec_caps = rte_tel_data_alloc();
	RTE_PTR_OR_ERR_RET(sec_caps, -ENOMEM);

	rte_tel_data_start_dict(d);
	sec_caps_n = sec_caps_array(sec_caps, capabilities);
	rte_tel_data_add_dict_container(d, "sec_caps", sec_caps, 0);
	rte_tel_data_add_dict_int(d, "sec_caps_n", sec_caps_n);

	return 0;
}

static int
security_handle_cryptodev_crypto_caps(const char *cmd __rte_unused, const char *params,
				      struct rte_tel_data *d)
{
	const struct rte_security_capability *capabilities;
	struct rte_tel_data *crypto_caps;
	const char *capa_param;
	int dev_id, capa_id;
	int crypto_caps_n;
	char *end_param;
	int rc;

	if (!params || strlen(params) == 0 || !isdigit(*params))
		return -EINVAL;

	dev_id = strtoul(params, &end_param, 0);
	capa_param = strtok(end_param, ",");
	if (!capa_param || strlen(capa_param) == 0 || !isdigit(*capa_param))
		return -EINVAL;

	capa_id = strtoul(capa_param, &end_param, 0);
	if (*end_param != '\0')
		CDEV_LOG_ERR("Extra parameters passed to command, ignoring");

	rc = security_capabilities_from_dev_id(dev_id, (void *)&capabilities);
	if (rc < 0)
		return rc;

	capabilities = security_capability_by_index(capabilities, capa_id);
	RTE_PTR_OR_ERR_RET(capabilities, -EINVAL);

	crypto_caps = rte_tel_data_alloc();
	RTE_PTR_OR_ERR_RET(crypto_caps, -ENOMEM);

	rte_tel_data_start_dict(d);
	crypto_caps_n = crypto_caps_array(crypto_caps, capabilities->crypto_capabilities);

	rte_tel_data_add_dict_container(d, "crypto_caps", crypto_caps, 0);
	rte_tel_data_add_dict_int(d, "crypto_caps_n", crypto_caps_n);

	return 0;
}

RTE_INIT(security_init_telemetry)
{
	rte_telemetry_register_cmd("/security/cryptodev/list",
		security_handle_cryptodev_list,
		"Returns list of available crypto devices by IDs. No parameters.");

	rte_telemetry_register_cmd("/security/cryptodev/sec_caps",
		security_handle_cryptodev_sec_caps,
		"Returns security capabilities for a cryptodev. Parameters: int dev_id");

	rte_telemetry_register_cmd("/security/cryptodev/crypto_caps",
		security_handle_cryptodev_crypto_caps,
		"Returns crypto capabilities for a security capability. Parameters: int dev_id, sec_cap_id");
}
