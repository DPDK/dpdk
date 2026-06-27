/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

 #include <stdlib.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <ethdev_driver.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_string_fns.h>

#include "sxe2_mp.h"
#include "sxe2_stats.h"
#include "sxe2_common_log.h"

static RTE_ATOMIC(uint16_t)primary_ethdev_cnt;
static RTE_ATOMIC(uint16_t)secondary_ethdev_cnt;
static const struct rte_memzone *sxe2_mp_mz;

static int32_t sxe2_mp_acquire_token(void);
static void sxe2_mp_release_token(void);

static int32_t sxe2_mp_primary_handle(const struct rte_mp_msg *mp_msg,
				   const void *peer);

static int32_t sxe2_mp_secondary_handle(const struct rte_mp_msg *mp_msg,
					 const void *peer);

static int32_t
sxe2_mp_primary_handle(const struct rte_mp_msg *mp_msg, const void *peer)
{
	struct rte_mp_msg reply;
	const struct sxe2_mp_param *param =
			(const struct sxe2_mp_param *)mp_msg->param;
	struct sxe2_mp_param *reply_param = (struct sxe2_mp_param *)reply.param;
	struct rte_eth_dev *dev;
	int32_t ret = 0;
	struct sxe2_mp_shared_data *mz_data;
	int32_t send_reply = 0;
	int32_t cnt = 0;

	if (!rte_eth_dev_is_valid_port(param->port_id)) {
		PMD_LOG_ERR(DRV, "primary process: invalid port_id %u",
				param->port_id);
		ret = -EINVAL;
		goto out;
	}

	dev = &rte_eth_devices[param->port_id];
	sxe2_mp_mz = rte_memzone_lookup(SXE2_MP_MZ_NAME);
	if (sxe2_mp_mz == NULL) {
		PMD_LOG_ERR(DRV, "Failed to lookup memzone %s", SXE2_MP_MZ_NAME);
		ret = -ENOENT;
		goto out;
	}

	mz_data = (struct sxe2_mp_shared_data *)sxe2_mp_mz->addr;
	send_reply = 1;

	memset(&reply, 0, sizeof(reply));
	(void)strlcpy(reply.name, SXE2_MP_NAME, sizeof(reply.name));
	reply.len_param = sizeof(*reply_param);

	switch (param->type) {
	case SXE2_MP_REQ_GET_STATS:
		ret = sxe2_stats_info_get(dev,
					  &mz_data->payload.stats_blk.stats,
					  &mz_data->payload.stats_blk.qstats);
		break;
	case SXE2_MP_REQ_GET_XSTATS:
		cnt = sxe2_xstats_info_get(dev,
				mz_data->payload.xstats_blk.xstats,
				SXE2_MP_MAX_XSTATS);

		if (cnt >= 0) {
			mz_data->payload.xstats_blk.xstats_num = (uint32_t)cnt;
			ret = 0;
		} else {
			mz_data->payload.xstats_blk.xstats_num = 0;
			ret = cnt;
		}
		break;
	case SXE2_MP_REQ_RESET_STATS:
		ret = sxe2_stats_hw_reset(dev);
		break;
	default:
		PMD_LOG_ERR(DRV, "primary process: unrecognized msg type: %d",
				param->type);
		send_reply = false;
		ret = -EINVAL;
		goto out;
	}
out:
	if (!send_reply)
		return ret;

	reply_param->result = ret;
	reply_param->type = param->type;
	reply_param->port_id = param->port_id;

	return rte_mp_reply(&reply, peer);
}

static int32_t
sxe2_mp_secondary_handle(const struct rte_mp_msg *mp_msg __rte_unused,
			 const void *peer __rte_unused)
{
	PMD_LOG_WARN(DRV, "the secondary process handler should not be called");
	return 0;
}

static int32_t
sxe2_mp_init_primary(__rte_unused struct rte_eth_dev *dev)
{
	int32_t ret;

	if (sxe2_mp_mz == NULL) {
		sxe2_mp_mz = rte_memzone_reserve(SXE2_MP_MZ_NAME,
				sizeof(struct sxe2_mp_shared_data),
				rte_socket_id(), 0);
		if (sxe2_mp_mz == NULL && rte_errno != EEXIST) {
			PMD_LOG_ERR(DRV, "Failed to reserve memzone %s, error: %d",
					SXE2_MP_MZ_NAME, -rte_errno);
			ret = -rte_errno;
			goto out;
		}

		sxe2_mp_mz = rte_memzone_lookup(SXE2_MP_MZ_NAME);
		if (sxe2_mp_mz == NULL) {
			PMD_LOG_ERR(DRV, "Failed to lookup memzone %s", SXE2_MP_MZ_NAME);
			ret = -ENOENT;
			goto out;
		}

		struct sxe2_mp_shared_data *mz =
			(struct sxe2_mp_shared_data *)sxe2_mp_mz->addr;
		rte_atomic_store_explicit(&mz->in_use, 0, rte_memory_order_release);
	}

	ret = rte_mp_action_register(SXE2_MP_NAME, sxe2_mp_primary_handle);
	if (ret && rte_errno == ENOTSUP) {
		PMD_LOG_INFO(DRV, "Primary not support IPC.");
		ret = 0;
		goto out;
	} else if (ret && rte_errno != EEXIST) {
		PMD_LOG_ERR(DRV, "Failed to register MP primary handle, error: %d",
			-rte_errno);
		goto out;
	}

	rte_atomic_fetch_add_explicit(&primary_ethdev_cnt, 1, rte_memory_order_relaxed);

	ret = 0;
out:
	return ret;
}

static int32_t
sxe2_mp_init_secondary(__rte_unused struct rte_eth_dev *dev)
{
	int32_t ret;

	sxe2_mp_mz = rte_memzone_lookup(SXE2_MP_MZ_NAME);
	if (sxe2_mp_mz == NULL) {
		PMD_LOG_ERR(DRV, "Failed to lookup memzone %s", SXE2_MP_MZ_NAME);
		ret = -ENOENT;
		goto out;
	}

	ret = rte_mp_action_register(SXE2_MP_NAME, sxe2_mp_secondary_handle);
	if (ret && rte_errno != EEXIST) {
		PMD_LOG_ERR(DRV, "Failed to register MP secondary handle, error: %d",
				-rte_errno);
		goto out;
	}

	rte_atomic_fetch_add_explicit(&secondary_ethdev_cnt, 1, rte_memory_order_relaxed);

	ret = 0;
out:
	return ret;
}

int32_t
sxe2_mp_init(struct rte_eth_dev *dev)
{
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		return sxe2_mp_init_primary(dev);
	else
		return sxe2_mp_init_secondary(dev);
}

void
sxe2_mp_uninit(__rte_unused struct rte_eth_dev *dev)
{
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		if (rte_atomic_fetch_sub_explicit(&primary_ethdev_cnt, 1,
			rte_memory_order_acq_rel) == 1) {
			rte_mp_action_unregister(SXE2_MP_NAME);
			if (sxe2_mp_mz != NULL) {
				rte_memzone_free(sxe2_mp_mz);
				sxe2_mp_mz = NULL;
			}
		}
	} else {
		if (rte_atomic_fetch_sub_explicit(&secondary_ethdev_cnt, 1,
			rte_memory_order_acq_rel) == 1)
			rte_mp_action_unregister(SXE2_MP_NAME);
	}
}

static int32_t sxe2_mp_acquire_token(void)
{
	struct sxe2_mp_shared_data *mz;
	uint16_t expected;

	if (sxe2_mp_mz == NULL)
		return -EINVAL;

	mz = (struct sxe2_mp_shared_data *)sxe2_mp_mz->addr;

	for (int32_t i = 0; i < SXE2_MP_MAX_SPIN; i++) {
		expected = 0;
		if (rte_atomic_compare_exchange_strong_explicit(&mz->in_use, &expected, 1,
			rte_memory_order_acquire, rte_memory_order_relaxed))
			return 0;

		rte_pause();
	}
	return -EBUSY;
}

static void sxe2_mp_release_token(void)
{
	struct sxe2_mp_shared_data *mz;

	if (sxe2_mp_mz == NULL)
		return;

	mz = (struct sxe2_mp_shared_data *)sxe2_mp_mz->addr;
	rte_atomic_store_explicit(&mz->in_use, 0, rte_memory_order_release);
}

int32_t sxe2_mp_request_simple(struct rte_eth_dev *dev,
			   enum sxe2_mp_req_type type,
			   int32_t *result_out)
{
	struct rte_mp_msg msg;
	struct rte_mp_reply reply = { 0 };
	struct timespec ts = { .tv_sec = SXE2_MP_MSG_TIMEOUT, .tv_nsec = 0 };
	struct sxe2_mp_param *param = (struct sxe2_mp_param *)msg.param;
	struct sxe2_mp_shared_data *mz_data;
	int32_t ret = 0;

	mz_data = (struct sxe2_mp_shared_data *)sxe2_mp_mz->addr;

	memset(&mz_data->payload, 0, sizeof(mz_data->payload));
	memset(&msg, 0, sizeof(msg));
	(void)strlcpy(msg.name, SXE2_MP_NAME, sizeof(msg.name));
	msg.len_param = sizeof(*param);
	param->type = type;
	param->port_id = dev->data->port_id;

	ret = rte_mp_request_sync(&msg, &reply, &ts);
	if (ret != 0) {
		PMD_LOG_ERR(DRV,
				"IPC request(type=%d) failed for port %u: %s",
				type, dev->data->port_id, rte_strerror(rte_errno));
		ret = -rte_errno;
		goto out;
	}

	if (reply.nb_received == 0) {
		PMD_LOG_ERR(DRV, "No response received from primary for type=%d, port %u",
			type, dev->data->port_id);
		ret = -EINVAL;
		goto out;
	}

	*result_out = ((struct sxe2_mp_param *)reply.msgs[0].param)->result;

out:
	if (reply.msgs != NULL)
		free(reply.msgs);

	return ret;
}

int32_t sxe2_mp_req_get_stats(struct rte_eth_dev *dev,
		      struct rte_eth_stats *stats,
		      struct eth_queue_stats *qstats)
{
	struct sxe2_mp_shared_data *mz_data;
	int32_t mp_ret;
	int32_t ret;
	int32_t token_acquired = 0;

	if (sxe2_mp_mz == NULL)
		return -EINVAL;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		PMD_LOG_WARN(DRV, "Primary process direct execution for port %u",
			     dev->data->port_id);
		return sxe2_stats_info_get(dev, stats, qstats);
	}

	int32_t token_ret = sxe2_mp_acquire_token();
	if (token_ret != 0)
		return token_ret;
	token_acquired = 1;

	mp_ret = sxe2_mp_request_simple(dev, SXE2_MP_REQ_GET_STATS, &ret);
	if (mp_ret != 0) {
		ret = mp_ret;
		goto out;
	}

	if (ret != 0) {
		PMD_LOG_ERR(DRV, "Primary failed to exec request (type=%d), result: %d for port %u",
			    SXE2_MP_REQ_GET_STATS, ret, dev->data->port_id);
		goto out;
	}

	mz_data = (struct sxe2_mp_shared_data *)sxe2_mp_mz->addr;
	memcpy(stats, &mz_data->payload.stats_blk.stats, sizeof(*stats));
	memcpy(qstats, &mz_data->payload.stats_blk.qstats, sizeof(*qstats));
	PMD_LOG_DEBUG(DRV, "sxe2_mp: stats received via IPC for port %u",
			  dev->data->port_id);
	ret = 0;
out:
	if (token_acquired)
		sxe2_mp_release_token();
	return ret;
}

int32_t sxe2_mp_req_get_xstats(struct rte_eth_dev *dev,
			   struct rte_eth_xstat *xstats, uint32_t usr_cnt)
{
	struct sxe2_mp_shared_data *mz_data;
	int32_t ret;
	int32_t mp_ret;
	int32_t token_acquired = 0;

	if (sxe2_mp_mz == NULL)
		return -EINVAL;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		PMD_LOG_WARN(DRV, "Primary process direct execution for port %u",
			     dev->data->port_id);
		return sxe2_xstats_info_get(dev, xstats, usr_cnt);
	}

	int32_t token_ret = sxe2_mp_acquire_token();
	if (token_ret != 0)
		return token_ret;
	token_acquired = 1;

	mp_ret = sxe2_mp_request_simple(dev, SXE2_MP_REQ_GET_XSTATS, &ret);
	if (mp_ret != 0) {
		ret = mp_ret;
		goto out;
	}

	if (ret != 0) {
		PMD_LOG_ERR(DRV, "Primary failed to exec request (type=%d), result: %d for port %u",
			    SXE2_MP_REQ_GET_XSTATS, ret, dev->data->port_id);
		goto out;
	}

	mz_data = (struct sxe2_mp_shared_data *)sxe2_mp_mz->addr;
	if (usr_cnt < mz_data->payload.xstats_blk.xstats_num) {
		PMD_LOG_ERR(DRV, "user usr_cnt:%u less than xstats cnt:%u.",
			    usr_cnt, mz_data->payload.xstats_blk.xstats_num);
		ret = (int32_t)mz_data->payload.xstats_blk.xstats_num;
		goto out;
	}

	memcpy(xstats, mz_data->payload.xstats_blk.xstats,
		   mz_data->payload.xstats_blk.xstats_num *
			   sizeof(struct rte_eth_xstat));
	ret = (int32_t)mz_data->payload.xstats_blk.xstats_num;

	PMD_LOG_DEBUG(DRV,
			  "xstats received via IPC for port %u (cnt=%d)",
			  dev->data->port_id, ret);
out:
	if (token_acquired)
		sxe2_mp_release_token();
	return ret;
}

int32_t
sxe2_mp_req_reset_stats(struct rte_eth_dev *dev)
{
	int32_t mp_ret;
	int32_t ret = 0;

	if (sxe2_mp_mz == NULL)
		return -EINVAL;

	mp_ret = sxe2_mp_request_simple(dev, SXE2_MP_REQ_RESET_STATS, &ret);
	if (mp_ret != 0)
		return mp_ret;

	if (ret != 0) {
		PMD_LOG_ERR(DRV,
				"Primary failed SXE2_MP_REQ_RESET_STATS, result: %d for port %u",
				ret, dev->data->port_id);
		return ret;
	}
	return 0;
}
