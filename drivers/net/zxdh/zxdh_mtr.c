/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#include <bus_pci_driver.h>
#include <rte_ethdev.h>
#include <rte_mtr_driver.h>
#include <rte_mempool.h>

#include "zxdh_logs.h"
#include "zxdh_mtr.h"
#include "zxdh_msg.h"
#include "zxdh_ethdev.h"
#include "zxdh_tables.h"

#define ZXDH_SHARE_FLOW_MAX       2048
#define ZXDH_HW_PROFILE_MAX       512
#define ZXDH_MAX_MTR_PROFILE_NUM  ZXDH_HW_PROFILE_MAX
#define ZXDH_PORT_MTR_FID_BASE    8192

/*  Maximum value of srTCM metering parameters, unit_step: 64kb
 *  61K~400000000(400G) bps, uint 64Kbps CBS/EBS/PBS max bucket depth 128MB
 *  PPS: 1pps~600Mpps
 */
#define ZXDH_SRTCM_CIR_MIN_BPS  (61 * (1ULL << 10))
#define ZXDH_SRTCM_CIR_MAX_BPS  (400 * (1ULL << 30))
#define ZXDH_SRTCM_EBS_MAX_B    (128 * (1ULL << 20))
#define ZXDH_SRTCM_CBS_MAX_B    (128 * (1ULL << 20))
#define ZXDH_TRTCM_PBS_MAX_B    (128 * (1ULL << 20))
#define ZXDH_TRTCM_PIR_MAX_BPS  (400 * (1ULL << 30))
#define ZXDH_TRTCM_PIR_MIN_BPS  (61 * (1ULL << 10))

#define ZXDH_SRTCM_CIR_MIN_PPS  (1)
#define ZXDH_SRTCM_CIR_MAX_PPS  (200 * (1ULL << 20))
#define ZXDH_SRTCM_CBS_MAX_P    (8192)
#define ZXDH_SRTCM_EBS_MAX_P    (8192)
#define ZXDH_TRTCM_PBS_MAX_P    (8192)
#define ZXDH_TRTCM_PIR_MIN_PPS  (1)
#define ZXDH_TRTCM_PIR_MAX_PPS  (200 * (1ULL << 20))

#define ZXDH_MP_ALLOC_OBJ_FUNC(mp, obj) rte_mempool_get(mp, (void **)&(obj))
#define ZXDH_MP_FREE_OBJ_FUNC(mp, obj) rte_mempool_put(mp, obj)

#define ZXDH_VFUNC_ACTIVE_BIT  11
#define ZXDH_VFUNC_NUM_MASK    0xff
#define ZXDH_GET_OWNER_PF_VPORT(vport) \
	(((vport) & ~(ZXDH_VFUNC_NUM_MASK)) & (~(1 << ZXDH_VFUNC_ACTIVE_BIT)))

enum ZXDH_PLCR_CD {
	ZXDH_PLCR_CD_SRTCM = 0,
	ZXDH_PLCR_CD_TRTCM,
	ZXDH_PLCR_CD_MEF101,
};
enum ZXDH_PLCR_CM {
	ZXDH_PLCR_CM_BLIND = 0,
	ZXDH_PLCR_CM_AWARE,
};
enum ZXDH_PLCR_CF {
	ZXDH_PLCR_CF_UNOVERFLOW = 0,
	ZXDH_PLCR_CF_OVERFLOW,
};

int
zxdh_hw_profile_ref(uint16_t hw_profile_id)
{
	if (hw_profile_id >= HW_PROFILE_MAX)
		return  -1;

	rte_spinlock_lock(&g_mtr_res.hw_plcr_res_lock);
	g_mtr_res.hw_profile_refcnt[hw_profile_id]++;
	rte_spinlock_unlock(&g_mtr_res.hw_plcr_res_lock);
	return 0;
}

static struct zxdh_meter_policy
*zxdh_mtr_policy_find_by_id(struct zxdh_mtr_policy_list *mtr_policy_list,
	uint16_t policy_id, uint16_t dpdk_portid)
{
	struct zxdh_meter_policy *mtr_policy = NULL;

	TAILQ_FOREACH(mtr_policy, mtr_policy_list, next) {
		if (policy_id == mtr_policy->policy_id &&
			dpdk_portid == mtr_policy->dpdk_port_id)
			return mtr_policy;
	}
	return NULL;
}

static int
zxdh_policy_validate_actions(const struct rte_flow_action *actions[RTE_COLORS],
	struct rte_mtr_error *error)
{
	if (!actions[RTE_COLOR_RED] || actions[RTE_COLOR_RED]->type != RTE_FLOW_ACTION_TYPE_DROP)
		return -rte_mtr_error_set(error, ENOTSUP, RTE_MTR_ERROR_TYPE_METER_POLICY, NULL,
			"Red color only supports drop action.");
	return 0;
}

static int
mtr_hw_stats_get(struct zxdh_hw *hw, uint8_t direction, struct zxdh_hw_mtr_stats *hw_mtr_stats)
{
	union zxdh_virport_num v_port = hw->vport;
	uint32_t stat_baseaddr = (direction == ZXDH_EGRESS)
		? ZXDH_MTR_STATS_EGRESS_BASE
		: ZXDH_MTR_STATS_INGRESS_BASE;
	uint32_t idx = zxdh_vport_to_vfid(v_port) + stat_baseaddr;
	struct zxdh_dtb_shared_data *dtb_sd = &hw->dev_sd->dtb_sd;

	int ret = zxdh_np_dtb_stats_get(hw->dev_id,
		dtb_sd->queueid, ZXDH_STAT_128_MODE,
		idx, (uint32_t *)hw_mtr_stats);

	if (ret) {
		PMD_DRV_LOG(ERR, "get vport 0x%x (vfid 0x%x) dir %u stats failed",
				v_port.vport,
				hw->vfid,
				direction);
		return ret;
	}
	PMD_DRV_LOG(INFO, "get vport 0x%x (vfid 0x%x) dir %u stats",
			v_port.vport,
			hw->vfid,
			direction);
	return 0;
}

static int
zxdh_mtr_stats_get(struct rte_eth_dev *dev, int dir, struct zxdh_mtr_stats *mtr_stats)
{
	struct zxdh_hw_mtr_stats hw_mtr_stat = {0};
	struct zxdh_hw *hw = dev->data->dev_private;
	int ret = mtr_hw_stats_get(hw, dir, &hw_mtr_stat);

	if (ret) {
		PMD_DRV_LOG(ERR, "port %u dir %u get mtr stats failed", hw->vport.vport, dir);
		return ret;
	}
	mtr_stats->n_bytes_dropped =
		(uint64_t)(rte_le_to_cpu_32(hw_mtr_stat.n_bytes_dropped_hi)) << 32 |
		rte_le_to_cpu_32(hw_mtr_stat.n_bytes_dropped_lo);
	mtr_stats->n_pkts_dropped =
		(uint64_t)(rte_le_to_cpu_32(hw_mtr_stat.n_pkts_dropped_hi)) << 32 |
		rte_le_to_cpu_32(hw_mtr_stat.n_pkts_dropped_lo);

	return 0;
}

static int
zxdh_meter_cap_get(struct rte_eth_dev *dev __rte_unused,
	struct rte_mtr_capabilities *cap,
	struct rte_mtr_error *error __rte_unused)
{
	struct rte_mtr_capabilities capa = {
		.n_max = ZXDH_MAX_MTR_NUM,
		.n_shared_max = ZXDH_SHARE_FLOW_MAX,
		.meter_srtcm_rfc2697_n_max = ZXDH_MAX_MTR_PROFILE_NUM,
		.meter_trtcm_rfc2698_n_max = ZXDH_MAX_MTR_PROFILE_NUM,
		.color_aware_srtcm_rfc2697_supported = 1,
		.color_aware_trtcm_rfc2698_supported = 1,
		.meter_rate_max = ZXDH_SRTCM_CIR_MAX_BPS,
		.meter_policy_n_max = ZXDH_MAX_POLICY_NUM,
		.srtcm_rfc2697_byte_mode_supported   = 1,
		.srtcm_rfc2697_packet_mode_supported = 1,
		.trtcm_rfc2698_byte_mode_supported   = 1,
		.trtcm_rfc2698_packet_mode_supported = 1,
		.stats_mask = RTE_MTR_STATS_N_PKTS_DROPPED | RTE_MTR_STATS_N_BYTES_DROPPED,
	};

	*cap = capa;
	return 0;
}

static int
zxdh_mtr_profile_validate(uint32_t meter_profile_id,
		struct rte_mtr_meter_profile *profile,
		struct rte_mtr_error *error)
{
	uint64_t cir_min, cir_max, cbs_max, ebs_max, pir_min, pir_max, pbs_max;

	if (profile == NULL || meter_profile_id >= ZXDH_MAX_MTR_PROFILE_NUM) {
		return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_METER_PROFILE, NULL,
			"Meter profile param id invalid or null");
	}

	if (profile->packet_mode == 0) {
		cir_min = ZXDH_SRTCM_CIR_MIN_BPS / 8;
		cir_max = ZXDH_SRTCM_CIR_MAX_BPS / 8;
		cbs_max = ZXDH_SRTCM_CBS_MAX_B;
		ebs_max = ZXDH_SRTCM_EBS_MAX_B;
		pir_min = ZXDH_TRTCM_PIR_MIN_BPS / 8;
		pir_max = ZXDH_TRTCM_PIR_MAX_BPS / 8;
		pbs_max = ZXDH_TRTCM_PBS_MAX_B;
	} else {
		cir_min = ZXDH_SRTCM_CIR_MIN_PPS;
		cir_max = ZXDH_SRTCM_CIR_MAX_PPS;
		cbs_max = ZXDH_SRTCM_CBS_MAX_P;
		ebs_max = ZXDH_SRTCM_EBS_MAX_P;
		pir_min = ZXDH_TRTCM_PIR_MIN_PPS;
		pir_max = ZXDH_TRTCM_PIR_MAX_PPS;
		pbs_max = ZXDH_TRTCM_PBS_MAX_P;
	}
	if (profile->alg == RTE_MTR_SRTCM_RFC2697) {
		if (profile->srtcm_rfc2697.cir >= cir_min &&
			profile->srtcm_rfc2697.cir < cir_max &&
			profile->srtcm_rfc2697.cbs < cbs_max &&
			profile->srtcm_rfc2697.cbs > 0 &&
			profile->srtcm_rfc2697.ebs > 0 &&
			profile->srtcm_rfc2697.ebs < ebs_max) {
			goto check_exist;
		} else {
			return -rte_mtr_error_set
					(error, ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_PROFILE,
					NULL,
					"Invalid metering parameters");
		}
	} else if (profile->alg == RTE_MTR_TRTCM_RFC2698) {
		if (profile->trtcm_rfc2698.cir >= cir_min &&
			profile->trtcm_rfc2698.cir < cir_max &&
			profile->trtcm_rfc2698.cbs < cbs_max &&
			profile->trtcm_rfc2698.cbs > 0 &&
			profile->trtcm_rfc2698.pir >= pir_min &&
			profile->trtcm_rfc2698.pir < pir_max &&
			profile->trtcm_rfc2698.cir < profile->trtcm_rfc2698.pir &&
			profile->trtcm_rfc2698.pbs > 0 &&
			profile->trtcm_rfc2698.pbs < pbs_max)
			goto check_exist;
		else
			return -rte_mtr_error_set(error, ENOTSUP,
				RTE_MTR_ERROR_TYPE_METER_PROFILE,
				NULL,
				"Invalid metering parameters");
	} else {
		return -rte_mtr_error_set(error, ENOTSUP,
			RTE_MTR_ERROR_TYPE_METER_PROFILE,
			NULL,
			"algorithm not supported");
	}

check_exist:
	return 0;
}

static struct zxdh_meter_profile
*zxdh_mtr_profile_find_by_id(struct zxdh_mtr_profile_list *mpl,
		uint32_t meter_profile_id, uint16_t dpdk_portid)
{
	struct zxdh_meter_profile *mp = NULL;

	TAILQ_FOREACH(mp, mpl, next) {
		if (meter_profile_id == mp->meter_profile_id && mp->dpdk_port_id == dpdk_portid)
			return mp;
	}
	return NULL;
}

static struct zxdh_meter_profile
*zxdh_mtr_profile_res_alloc(struct rte_mempool *mtr_profile_mp)
{
	struct zxdh_meter_profile *meter_profile = NULL;

	if (ZXDH_MP_ALLOC_OBJ_FUNC(mtr_profile_mp, meter_profile) != 0)
		return NULL;

	return meter_profile;
}

static struct zxdh_meter_policy
*zxdh_mtr_policy_res_alloc(struct rte_mempool *mtr_policy_mp)
{
	struct zxdh_meter_policy *policy = NULL;

	rte_mempool_get(mtr_policy_mp, (void **)&policy);
	PMD_DRV_LOG(INFO, "policy %p", policy);
	return policy;
}

static int
zxdh_hw_profile_free_direct(struct rte_eth_dev *dev, ZXDH_PROFILE_TYPE car_type,
		uint16_t hw_profile_id, struct rte_mtr_error *error)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	uint16_t vport = hw->vport.vport;
	int ret = zxdh_np_car_profile_id_delete(hw->dev_id, vport, car_type,
			(uint64_t)hw_profile_id);
	if (ret) {
		PMD_DRV_LOG(ERR, "port %u free hw profile %u failed", vport, hw_profile_id);
		return -rte_mtr_error_set(error, ENOTSUP, RTE_MTR_ERROR_TYPE_METER_PROFILE_ID, NULL,
			"Meter free profile failed");
	}

	return 0;
}

int
zxdh_hw_profile_alloc_direct(struct rte_eth_dev *dev, ZXDH_PROFILE_TYPE car_type,
		uint64_t *hw_profile_id, struct rte_mtr_error *error)
{
	uint64_t profile_id = HW_PROFILE_MAX;
	struct zxdh_hw *hw = dev->data->dev_private;
	uint16_t vport = hw->vport.vport;
	int ret = zxdh_np_car_profile_id_add(hw->dev_id, vport, car_type, &profile_id);

	if (ret) {
		PMD_DRV_LOG(ERR, "port %u alloc hw profile failed", vport);
		return -rte_mtr_error_set(error, ENOTSUP, RTE_MTR_ERROR_TYPE_METER_PROFILE_ID, NULL,
			"Meter offload alloc profile failed");
	}
	*hw_profile_id = profile_id;
	if (*hw_profile_id == ZXDH_HW_PROFILE_MAX) {
		return -rte_mtr_error_set(error, ENOTSUP, RTE_MTR_ERROR_TYPE_METER_PROFILE_ID, NULL,
			"Meter offload alloc profile id invalid");
	}

	return 0;
}

static uint16_t
zxdh_hw_profile_free(struct rte_eth_dev *dev, uint8_t car_type,
		uint16_t hw_profile_id, struct rte_mtr_error *error)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	int ret = 0;

	if (hw->is_pf) {
		ret = zxdh_hw_profile_free_direct(dev, car_type, (uint64_t)hw_profile_id, error);
	} else {
		struct zxdh_msg_info msg_info = {0};
		uint8_t zxdh_msg_reply_info[ZXDH_ST_SZ_BYTES(msg_reply_info)] = {0};
		struct zxdh_plcr_profile_free *zxdh_plcr_profile_free =
			&msg_info.data.zxdh_plcr_profile_free;

		zxdh_plcr_profile_free->profile_id = hw_profile_id;
		zxdh_plcr_profile_free->car_type = car_type;
		zxdh_msg_head_build(hw, ZXDH_PLCR_CAR_PROFILE_ID_DELETE, &msg_info);
		ret = zxdh_vf_send_msg_to_pf(dev, &msg_info,
			ZXDH_MSG_HEAD_LEN + sizeof(struct zxdh_plcr_profile_free),
			zxdh_msg_reply_info, ZXDH_ST_SZ_BYTES(msg_reply_info));

		if (ret)
			return -rte_mtr_error_set(error, ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_PROFILE_ID, NULL,
					"Meter free  profile failed ");
	}

	return ret;
}

static int
zxdh_hw_profile_alloc(struct rte_eth_dev *dev, uint64_t *hw_profile_id,
		struct rte_mtr_error *error)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	int ret = 0;

	if (hw->is_pf) {
		ret = zxdh_hw_profile_alloc_direct(dev, CAR_A, hw_profile_id, error);
	} else {
		struct zxdh_msg_info msg_info = {0};
		uint8_t zxdh_msg_reply_info[ZXDH_ST_SZ_BYTES(msg_reply_info)] = {0};
		struct zxdh_plcr_profile_add  *zxdh_plcr_profile_add =
			&msg_info.data.zxdh_plcr_profile_add;
		void *reply_body_addr =
			ZXDH_ADDR_OF(msg_reply_info, zxdh_msg_reply_info, reply_body);
		void *mtr_profile_info_addr =
			ZXDH_ADDR_OF(msg_reply_body, reply_body_addr, mtr_profile_info);

		zxdh_plcr_profile_add->car_type = CAR_A;
		zxdh_msg_head_build(hw, ZXDH_PLCR_CAR_PROFILE_ID_ADD, &msg_info);
		ret = zxdh_vf_send_msg_to_pf(dev, &msg_info,
			ZXDH_MSG_HEAD_LEN + sizeof(struct zxdh_plcr_profile_add),
			zxdh_msg_reply_info, ZXDH_ST_SZ_BYTES(msg_reply_info));

		if (ret) {
			PMD_DRV_LOG(ERR,
				"Failed to send msg: port 0x%x msg type ZXDH_PLCR_CAR_PROFILE_ID_ADD ",
				hw->vport.vport);

			return -rte_mtr_error_set(error, ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_PROFILE_ID, NULL,
					"Meter offload alloc profile  id msg failed ");
		}
		*hw_profile_id = ZXDH_GET(mtr_profile_info, mtr_profile_info_addr, profile_id);
		if (*hw_profile_id == ZXDH_HW_PROFILE_MAX) {
			return -rte_mtr_error_set(error, ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_PROFILE_ID, NULL,
					"Meter offload alloc profile  id invalid  ");
		}
	}

	return ret;
}

int
zxdh_hw_profile_unref(struct rte_eth_dev *dev,
	uint8_t car_type,
	uint16_t hw_profile_id,
	struct rte_mtr_error *error)
{
	if (hw_profile_id >= ZXDH_HW_PROFILE_MAX)
		return -1;

	rte_spinlock_lock(&g_mtr_res.hw_plcr_res_lock);
	if (g_mtr_res.hw_profile_refcnt[hw_profile_id] == 0) {
		PMD_DRV_LOG(ERR, "del hw profile id %d  but ref 0", hw_profile_id);
		rte_spinlock_unlock(&g_mtr_res.hw_plcr_res_lock);
		return -1;
	}
	if (--g_mtr_res.hw_profile_refcnt[hw_profile_id] == 0) {
		PMD_DRV_LOG(INFO, "del hw profile id %d ", hw_profile_id);
		zxdh_hw_profile_free(dev, car_type, hw_profile_id, error);
	}
	rte_spinlock_unlock(&g_mtr_res.hw_plcr_res_lock);
	return 0;
}

static int
zxdh_mtr_hw_counter_query(struct rte_eth_dev *dev,
	bool clear,
	bool dir,
	struct zxdh_mtr_stats *mtr_stats,
	struct rte_mtr_error *error)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	int ret = 0;

	if (hw->is_pf) {
		ret = zxdh_mtr_stats_get(dev, dir, mtr_stats);
		if (ret) {
			PMD_DRV_LOG(ERR,
				"ZXDH_PORT_METER_STAT_GET port %u dir %d failed",
				hw->vport.vport,
				dir);

			return -rte_mtr_error_set(error, ENOTSUP, RTE_MTR_ERROR_TYPE_STATS, NULL, "Failed to bind plcr flow.");
		}
	} else { /* send msg to pf */
		struct zxdh_msg_info msg_info = {0};
		uint8_t zxdh_msg_reply_info[ZXDH_ST_SZ_BYTES(msg_reply_info)] = {0};
		struct zxdh_mtr_stats_query *zxdh_mtr_stats_query =
				&msg_info.data.zxdh_mtr_stats_query;

		zxdh_mtr_stats_query->direction = dir;
		zxdh_mtr_stats_query->is_clr = !!clear;
		zxdh_msg_head_build(hw, ZXDH_PORT_METER_STAT_GET, &msg_info);
		ret = zxdh_vf_send_msg_to_pf(dev,
			&msg_info,
			sizeof(msg_info),
			zxdh_msg_reply_info,
			ZXDH_ST_SZ_BYTES(msg_reply_info));

		if (ret) {
			PMD_DRV_LOG(ERR,
				"Failed to send msg: port 0x%x msg type ZXDH_PORT_METER_STAT_GET",
				hw->vport.vport);
			return -rte_mtr_error_set(error, ENOTSUP, RTE_MTR_ERROR_TYPE_STATS, NULL, "Meter offload alloc profile failed");
		}
		void *reply_body_addr =
			ZXDH_ADDR_OF(msg_reply_info, zxdh_msg_reply_info, reply_body);
		void *hw_mtr_stats_addr =
			ZXDH_ADDR_OF(msg_reply_body, reply_body_addr, hw_mtr_stats);
		struct zxdh_mtr_stats *hw_mtr_stats = (struct zxdh_mtr_stats *)hw_mtr_stats_addr;

		mtr_stats->n_bytes_dropped = hw_mtr_stats->n_bytes_dropped;
		mtr_stats->n_pkts_dropped = hw_mtr_stats->n_pkts_dropped;
	}

	return ret;
}


static void
zxdh_mtr_profile_res_free(struct rte_eth_dev *dev,
		struct rte_mempool *mtr_profile_mp,
		struct zxdh_meter_profile *meter_profile,
		struct rte_mtr_error *error)
{
	if (meter_profile->ref_cnt == 0) {
		ZXDH_MP_FREE_OBJ_FUNC(mtr_profile_mp, meter_profile);
		return;
	}
	if (meter_profile->ref_cnt == 1) {
		meter_profile->ref_cnt--;
		zxdh_hw_profile_unref(dev, CAR_A, meter_profile->hw_profile_id, error);

		TAILQ_REMOVE(&zxdh_shared_data->meter_profile_list, meter_profile, next);
		ZXDH_MP_FREE_OBJ_FUNC(mtr_profile_mp, meter_profile);
	} else {
		PMD_DRV_LOG(INFO,
			"profile %d ref %d is busy",
			meter_profile->meter_profile_id,
			meter_profile->ref_cnt);
	}
}

static uint16_t
zxdh_check_hw_profile_exist(struct zxdh_mtr_profile_list *mpl,
	struct rte_mtr_meter_profile *profile,
	uint16_t hw_profile_owner_vport)
{
	struct zxdh_meter_profile *mp;

	TAILQ_FOREACH(mp, mpl, next) {
		if ((memcmp(profile, &mp->profile, sizeof(struct rte_mtr_meter_profile)) == 0) &&
			hw_profile_owner_vport == mp->hw_profile_owner_vport) {
			return mp->hw_profile_id;
		}
	}
	return ZXDH_HW_PROFILE_MAX;
}

static void
zxdh_plcr_param_build(struct rte_mtr_meter_profile *profile,
		void *plcr_param, uint16_t profile_id)
{
	if (profile->packet_mode == 0) {
		ZXDH_STAT_CAR_PROFILE_CFG_T *p_car_byte_profile_cfg =
			(ZXDH_STAT_CAR_PROFILE_CFG_T *)plcr_param;

		p_car_byte_profile_cfg->profile_id = profile_id;
		p_car_byte_profile_cfg->pkt_sign = profile->packet_mode;
		p_car_byte_profile_cfg->cf = ZXDH_PLCR_CF_UNOVERFLOW;
		p_car_byte_profile_cfg->cm = ZXDH_PLCR_CM_BLIND;
		if (profile->alg == RTE_MTR_SRTCM_RFC2697) {
			p_car_byte_profile_cfg->cd  = ZXDH_PLCR_CD_SRTCM;
			p_car_byte_profile_cfg->cir = profile->srtcm_rfc2697.cir * 8 / 1000;
			p_car_byte_profile_cfg->cbs = profile->srtcm_rfc2697.cbs;
			p_car_byte_profile_cfg->ebs = profile->srtcm_rfc2697.ebs;
		} else {
			p_car_byte_profile_cfg->cd  = ZXDH_PLCR_CD_TRTCM;
			p_car_byte_profile_cfg->cir = profile->trtcm_rfc2698.cir * 8 / 1000;
			p_car_byte_profile_cfg->cbs = profile->trtcm_rfc2698.cbs;
			p_car_byte_profile_cfg->eir = (profile->trtcm_rfc2698.pir -
				profile->trtcm_rfc2698.cir) * 8 / 1000;
			p_car_byte_profile_cfg->ebs =
				profile->trtcm_rfc2698.pbs - profile->trtcm_rfc2698.cbs;
		}
	} else {
		ZXDH_STAT_CAR_PKT_PROFILE_CFG_T *p_car_pkt_profile_cfg =
			(ZXDH_STAT_CAR_PKT_PROFILE_CFG_T *)plcr_param;

		p_car_pkt_profile_cfg->profile_id = profile_id;
		p_car_pkt_profile_cfg->pkt_sign = profile->packet_mode;

		if (profile->alg == RTE_MTR_SRTCM_RFC2697) {
			p_car_pkt_profile_cfg->cir = profile->srtcm_rfc2697.cir;
			p_car_pkt_profile_cfg->cbs = profile->srtcm_rfc2697.cbs;
		} else {
			p_car_pkt_profile_cfg->cir = profile->trtcm_rfc2698.cir;
			p_car_pkt_profile_cfg->cbs = profile->trtcm_rfc2698.cbs;
		}
	}
}

static int
zxdh_hw_profile_config_direct(struct rte_eth_dev *dev __rte_unused,
	ZXDH_PROFILE_TYPE car_type,
	uint16_t hw_profile_id,
	struct zxdh_meter_profile *mp,
	struct rte_mtr_error *error)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	int ret = zxdh_np_car_profile_cfg_set(hw->dev_id,
		mp->hw_profile_owner_vport,
		car_type, mp->profile.packet_mode,
		(uint32_t)hw_profile_id, &mp->plcr_param);
	if (ret) {
		PMD_DRV_LOG(ERR, " config hw profile %u failed", hw_profile_id);
		return -rte_mtr_error_set(error, ENOTSUP, RTE_MTR_ERROR_TYPE_METER_PROFILE, NULL,
			"Meter offload cfg profile failed");
	}

	return 0;
}

static int zxdh_hw_profile_config(struct rte_eth_dev *dev, uint16_t hw_profile_id,
	struct zxdh_meter_profile *mp, struct rte_mtr_error *error)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	int ret = 0;

	if (hw->is_pf) {
		ret = zxdh_hw_profile_config_direct(dev, CAR_A, hw_profile_id, mp, error);
	} else {
		struct zxdh_msg_info msg_info = {0};
		uint8_t zxdh_msg_reply_info[ZXDH_ST_SZ_BYTES(msg_reply_info)] = {0};
		struct zxdh_plcr_profile_cfg *zxdh_plcr_profile_cfg =
			&msg_info.data.zxdh_plcr_profile_cfg;

		zxdh_plcr_profile_cfg->car_type = CAR_A;
		zxdh_plcr_profile_cfg->packet_mode = mp->profile.packet_mode;
		zxdh_plcr_profile_cfg->hw_profile_id = hw_profile_id;
		rte_memcpy(&zxdh_plcr_profile_cfg->plcr_param,
			&mp->plcr_param,
			sizeof(zxdh_plcr_profile_cfg->plcr_param));

		zxdh_msg_head_build(hw, ZXDH_PLCR_CAR_PROFILE_CFG_SET, &msg_info);
		ret = zxdh_vf_send_msg_to_pf(dev,
			&msg_info,
			ZXDH_MSG_HEAD_LEN + sizeof(struct zxdh_plcr_profile_cfg),
			zxdh_msg_reply_info,
			ZXDH_ST_SZ_BYTES(msg_reply_info)
		);
		if (ret) {
			PMD_DRV_LOG(ERR,
				"Failed msg: port 0x%x msg type ZXDH_PLCR_CAR_PROFILE_CFG_SET ",
				hw->vport.vport);

			return -rte_mtr_error_set(error, ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_PROFILE, NULL,
					"Meter offload cfg profile failed ");
		}
	}

	return ret;
}

static int
zxdh_mtr_profile_offload(struct rte_eth_dev *dev, struct zxdh_meter_profile *mp,
		struct rte_mtr_meter_profile *profile, struct rte_mtr_error *error)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	uint16_t hw_profile_owner_vport = ZXDH_GET_OWNER_PF_VPORT(hw->vport.vport);

	mp->hw_profile_owner_vport = hw_profile_owner_vport;
	uint64_t hw_profile_id =
		zxdh_check_hw_profile_exist(&zxdh_shared_data->meter_profile_list,
			profile,
			hw_profile_owner_vport);

	if (hw_profile_id == ZXDH_HW_PROFILE_MAX) {
		uint32_t ret = zxdh_hw_profile_alloc(dev, &hw_profile_id, error);

		if (ret) {
			PMD_DRV_LOG(ERR, "hw_profile alloc fail");
			return ret;
		}

		zxdh_plcr_param_build(profile, &mp->plcr_param, hw_profile_id);
		ret = zxdh_hw_profile_config(dev, hw_profile_id, mp, error);
		if (ret) {
			PMD_DRV_LOG(ERR, "zxdh_hw_profile_config fail");
			hw_profile_id = ZXDH_HW_PROFILE_MAX;
			return ret;
		}
	}
	zxdh_hw_profile_ref(hw_profile_id);
	mp->hw_profile_id = hw_profile_id;

	return 0;
}

static int
zxdh_meter_profile_add(struct rte_eth_dev *dev,
				uint32_t meter_profile_id,
				struct rte_mtr_meter_profile *profile,
				struct rte_mtr_error *error)
{
	struct zxdh_meter_profile *mp;
	int ret;

	ret = zxdh_mtr_profile_validate(meter_profile_id, profile, error);
	if (ret)
		return -rte_mtr_error_set(error, ENOMEM,
					RTE_MTR_ERROR_TYPE_METER_PROFILE,
					NULL, "meter profile validate failed");
	mp = zxdh_mtr_profile_find_by_id(&zxdh_shared_data->meter_profile_list,
		meter_profile_id,
		dev->data->port_id);

	if (mp)
		return -rte_mtr_error_set(error, EEXIST,
					  RTE_MTR_ERROR_TYPE_METER_PROFILE,
					  NULL,
					  "meter profile is exists");

	mp = zxdh_mtr_profile_res_alloc(zxdh_shared_data->mtr_profile_mp);
	if (mp == NULL)
		return -rte_mtr_error_set(error, ENOMEM,
					RTE_MTR_ERROR_TYPE_METER_PROFILE,
					NULL, "Meter profile res memory alloc failed.");

	memset(mp, 0, sizeof(struct zxdh_meter_profile));

	mp->meter_profile_id = meter_profile_id;
	mp->dpdk_port_id = dev->data->port_id;
	mp->hw_profile_id = UINT16_MAX;
	rte_memcpy(&mp->profile, profile, sizeof(struct rte_mtr_meter_profile));

	ret = zxdh_mtr_profile_offload(dev, mp, profile, error);
	if (ret) {
		PMD_DRV_LOG(ERR,
			" port %d profile id  %d offload failed  ",
			dev->data->port_id,
			meter_profile_id);
		goto error;
	}

	TAILQ_INSERT_TAIL(&zxdh_shared_data->meter_profile_list, mp, next);
	PMD_DRV_LOG(DEBUG,
		"add profile id %d mp %p  mp->ref_cnt %d",
		meter_profile_id,
		mp,
		mp->ref_cnt);

	mp->ref_cnt++;

	return 0;
error:
	zxdh_mtr_profile_res_free(dev, zxdh_shared_data->mtr_profile_mp, mp, error);
	return ret;
}

static int
zxdh_meter_profile_delete(struct rte_eth_dev *dev,
				uint32_t meter_profile_id,
				struct rte_mtr_error *error)
{
	struct zxdh_meter_profile *mp;

	mp = zxdh_mtr_profile_find_by_id(&zxdh_shared_data->meter_profile_list,
		meter_profile_id,
		dev->data->port_id);

	if (mp == NULL) {
		PMD_DRV_LOG(ERR, "del profile id %d  unfind ", meter_profile_id);
		return -rte_mtr_error_set(error, ENOENT,
						RTE_MTR_ERROR_TYPE_METER_PROFILE,
						&meter_profile_id,
						 "Meter profile id is not exists.");
	}
	zxdh_mtr_profile_res_free(dev, zxdh_shared_data->mtr_profile_mp, mp, error);

	return 0;
}

static int
zxdh_meter_policy_add(struct rte_eth_dev *dev,
		uint32_t policy_id,
		struct rte_mtr_meter_policy_params *policy,
		struct rte_mtr_error *error)
{
	int ret = 0;
	struct zxdh_meter_policy *mtr_policy = NULL;

	if (policy_id >= ZXDH_MAX_POLICY_NUM)
		return -rte_mtr_error_set(error, ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
					NULL, "policy ID is invalid. ");
	mtr_policy = zxdh_mtr_policy_find_by_id(&zxdh_shared_data->mtr_policy_list,
				policy_id,
				dev->data->port_id);

	if (mtr_policy)
		return -rte_mtr_error_set(error, EEXIST,
					RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
					NULL, "policy ID  exists. ");
	ret = zxdh_policy_validate_actions(policy->actions, error);
	if (ret) {
		return -rte_mtr_error_set(error, ENOTSUP,
				RTE_MTR_ERROR_TYPE_METER_POLICY,
				NULL, "  only supports def action.");
	}

	mtr_policy = zxdh_mtr_policy_res_alloc(zxdh_shared_data->mtr_policy_mp);
	if (mtr_policy == NULL) {
		return -rte_mtr_error_set(error, ENOMEM,
					RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
					NULL, "Meter policy res memory alloc  failed.");
	}
	/* Fill profile info. */
	memset(mtr_policy, 0, sizeof(struct zxdh_meter_policy));
	mtr_policy->policy_id = policy_id;
	mtr_policy->dpdk_port_id = dev->data->port_id;
	rte_memcpy(&mtr_policy->policy, policy, sizeof(struct rte_mtr_meter_policy_params));
	/* Add to list. */
	TAILQ_INSERT_TAIL(&zxdh_shared_data->mtr_policy_list, mtr_policy, next);
	mtr_policy->ref_cnt++;
	PMD_DRV_LOG(INFO, "allic policy id %d ok %p ", mtr_policy->policy_id, mtr_policy);
	return 0;
}

static int
zxdh_meter_policy_delete(struct rte_eth_dev *dev,
	uint32_t policy_id,
	struct rte_mtr_error *error)
{
	struct zxdh_meter_policy *mtr_policy = NULL;

	if (policy_id >= ZXDH_MAX_POLICY_NUM)
		return -rte_mtr_error_set(error, ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
					NULL, "policy ID is invalid. ");
	mtr_policy = zxdh_mtr_policy_find_by_id(&zxdh_shared_data->mtr_policy_list,
		policy_id, dev->data->port_id);

	if (mtr_policy && mtr_policy->ref_cnt == 1) {
		TAILQ_REMOVE(&zxdh_shared_data->mtr_policy_list, mtr_policy, next);
		MP_FREE_OBJ_FUNC(zxdh_shared_data->mtr_policy_mp, mtr_policy);
	} else {
		if (mtr_policy) {
			PMD_DRV_LOG(INFO,
				" policy id %d ref %d is busy ",
				mtr_policy->policy_id,
				mtr_policy->ref_cnt);
		} else {
			PMD_DRV_LOG(ERR, " policy id %d  is not exist ", policy_id);
			return -rte_mtr_error_set(error, ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
					NULL, "policy ID is  not exist. ");
		}
	}
	return 0;
}

static int
zxdh_meter_validate(uint32_t meter_id,
			struct rte_mtr_params *params,
			struct rte_mtr_error *error)
{
	/* Meter params must not be NULL. */
	if (params == NULL)
		return -rte_mtr_error_set(error, EINVAL,
					RTE_MTR_ERROR_TYPE_MTR_PARAMS,
					NULL, "Meter object params null.");
	/* Previous meter color is not supported. */
	if (params->use_prev_mtr_color)
		return -rte_mtr_error_set(error, EINVAL,
					RTE_MTR_ERROR_TYPE_MTR_PARAMS,
					NULL,
					"Previous meter color not supported");
	if (meter_id > ZXDH_MAX_MTR_NUM / 2) {
		return -rte_mtr_error_set(error, EINVAL,
				RTE_MTR_ERROR_TYPE_MTR_PARAMS,
				NULL,
				" meter id exceed 1024 unsupported");
	}
	return 0;
}

static int
zxdh_check_port_mtr_bind(struct rte_eth_dev *dev, uint32_t dir)
{
	struct zxdh_mtr_object *mtr_obj = NULL;

	TAILQ_FOREACH(mtr_obj, &zxdh_shared_data->mtr_list, next) {
		if (mtr_obj->direction != dir)
			continue;
		if (mtr_obj->port_id == dev->data->port_id) {
			PMD_DRV_LOG(INFO,
				"port %d dir %d already bind meter %d",
				dev->data->port_id,
				dir,
				mtr_obj->meter_id);
			return -1;
		}
	}

	return 0;
}

static struct zxdh_mtr_object
*zxdh_mtr_obj_alloc(struct rte_mempool *mtr_mp)
{
	struct zxdh_mtr_object *mtr_obj = NULL;

	if (ZXDH_MP_ALLOC_OBJ_FUNC(mtr_mp, mtr_obj) != 0)
		return NULL;

	return mtr_obj;
}

static uint32_t dir_to_mtr_mode[] = {
	ZXDH_PORT_EGRESS_METER_EN_OFF_FLAG,
	ZXDH_PORT_INGRESS_METER_EN_OFF_FLAG
};

static int
zxdh_set_mtr_enable(struct rte_eth_dev *dev, uint8_t dir, bool enable, struct rte_mtr_error *error)
{
	struct zxdh_hw *priv = dev->data->dev_private;
	struct zxdh_port_attr_table port_attr = {0};
	int ret = 0;

	if (priv->is_pf) {
		ret = zxdh_get_port_attr(priv, priv->vport.vport, &port_attr);
		port_attr.egress_meter_enable = enable;
		ret = zxdh_set_port_attr(priv, priv->vport.vport, &port_attr);
		if (ret) {
			PMD_DRV_LOG(ERR, "%s set port attr failed", __func__);
			return -ret;
		}
	} else {
		struct zxdh_msg_info msg_info = {0};
		struct zxdh_port_attr_set_msg *attr_msg = &msg_info.data.port_attr_msg;

		attr_msg->mode  = dir_to_mtr_mode[dir];
		attr_msg->value = enable;
		zxdh_msg_head_build(priv, ZXDH_PORT_ATTRS_SET, &msg_info);
		ret = zxdh_vf_send_msg_to_pf(dev, &msg_info,
			sizeof(struct zxdh_msg_head) + sizeof(struct zxdh_port_attr_set_msg),
			NULL, 0);
	}
	if (ret) {
		PMD_DRV_LOG(ERR, " port %d  mtr enable failed", priv->port_id);
		return -rte_mtr_error_set(error, EEXIST,
					RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
					"Meter  enable failed.");
	}
	if (dir == ZXDH_INGRESS)
		priv->i_mtr_en = !!enable;
	else
		priv->e_mtr_en = !!enable;

	return ret;
}

static void
zxdh_meter_build_actions(struct zxdh_meter_action *mtr_action,
		struct rte_mtr_params *params)
{
	mtr_action->stats_mask = params->stats_mask;
	mtr_action->action[RTE_COLOR_RED] = ZXDH_MTR_POLICER_ACTION_DROP;
}

static int
zxdh_hw_plcrflow_config(struct rte_eth_dev *dev, uint16_t hw_flow_id,
		struct zxdh_mtr_object *mtr, struct rte_mtr_error *error)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	int ret = 0;

	if (hw->is_pf) {
		uint64_t hw_profile_id = (uint64_t)mtr->profile->hw_profile_id;

		ret = zxdh_np_stat_car_queue_cfg_set(hw->dev_id, CAR_A,
			hw_flow_id, 1, mtr->enable, hw_profile_id);

		if (ret) {
			PMD_DRV_LOG(ERR, "dpp_stat_car_queue_cfg_set failed flowid %d  profile id %d",
							hw_flow_id, mtr->profile->hw_profile_id);
			return -rte_mtr_error_set(error, ENOTSUP,
					RTE_MTR_ERROR_TYPE_MTR_PARAMS,
					NULL, "Failed to  bind  plcr flow.");
			;
		}
	} else {
		struct zxdh_msg_info msg_info = {0};
		uint8_t zxdh_msg_reply_info[ZXDH_ST_SZ_BYTES(msg_reply_info)] = {0};
		struct zxdh_plcr_flow_cfg *zxdh_plcr_flow_cfg = &msg_info.data.zxdh_plcr_flow_cfg;

		zxdh_plcr_flow_cfg->car_type = CAR_A;
		zxdh_plcr_flow_cfg->flow_id = hw_flow_id;
		zxdh_plcr_flow_cfg->drop_flag = 1;
		zxdh_plcr_flow_cfg->plcr_en = mtr->enable;
		zxdh_plcr_flow_cfg->profile_id = mtr->profile->hw_profile_id;
		zxdh_msg_head_build(hw, ZXDH_PLCR_CAR_QUEUE_CFG_SET, &msg_info);
		ret = zxdh_vf_send_msg_to_pf(dev, &msg_info,
			ZXDH_MSG_HEAD_LEN + sizeof(struct zxdh_plcr_flow_cfg),
			zxdh_msg_reply_info,
			ZXDH_ST_SZ_BYTES(msg_reply_info));
		if (ret) {
			PMD_DRV_LOG(ERR,
				"Failed msg: port 0x%x msg type ZXDH_PLCR_CAR_QUEUE_CFG_SET ",
				hw->vport.vport);
			return -rte_mtr_error_set(error, ENOTSUP,
					RTE_MTR_ERROR_TYPE_MTR_PARAMS,
					NULL, "Failed to  bind  plcr flow.");
		}
	}

	return ret;
}

static void
zxdh_mtr_obj_free(struct rte_eth_dev *dev, struct zxdh_mtr_object *mtr_obj)
{
	struct zxdh_mtr_list *mtr_list = &zxdh_shared_data->mtr_list;
	struct rte_mempool *mtr_mp = zxdh_shared_data->mtr_mp;

	PMD_DRV_LOG(INFO, "free port %d dir %d meter %d  mtr refcnt:%d ....",
		dev->data->port_id, mtr_obj->direction, mtr_obj->meter_id, mtr_obj->mtr_ref_cnt);

	if (mtr_obj->policy)
		mtr_obj->policy->ref_cnt--;

	if (mtr_obj->profile)
		mtr_obj->profile->ref_cnt--;

	PMD_DRV_LOG(INFO,
		"free port %d dir %d meter %d  profile refcnt:%d ",
		dev->data->port_id,
		mtr_obj->direction,
		mtr_obj->meter_id,
		mtr_obj->profile ? mtr_obj->profile->ref_cnt : 0);

	if (--mtr_obj->mtr_ref_cnt == 0) {
		PMD_DRV_LOG(INFO, "rm  mtr %p refcnt:%d ....", mtr_obj, mtr_obj->mtr_ref_cnt);
		TAILQ_REMOVE(mtr_list, mtr_obj, next);
		MP_FREE_OBJ_FUNC(mtr_mp, mtr_obj);
	}
}

static int
zxdh_mtr_flow_offlad(struct rte_eth_dev *dev,
	struct zxdh_mtr_object *mtr,
	struct rte_mtr_error *error)
{
	uint16_t hw_flow_id;

	hw_flow_id = mtr->vfid * 2 + ZXDH_PORT_MTR_FID_BASE + mtr->direction;
	return zxdh_hw_plcrflow_config(dev, hw_flow_id, mtr, error);
}

static struct zxdh_mtr_object *
zxdh_mtr_find(uint32_t meter_id, uint16_t dpdk_portid)
{
	struct zxdh_mtr_list *mtr_list = &zxdh_shared_data->mtr_list;
	struct zxdh_mtr_object *mtr = NULL;

	TAILQ_FOREACH(mtr, mtr_list, next) {
		PMD_DRV_LOG(INFO,
			"mtrlist head %p  mtr %p mtr->meterid %d to find mtrid %d",
			TAILQ_FIRST(mtr_list),
			mtr,
			mtr->meter_id,
			meter_id
		);

		if (meter_id == mtr->meter_id && dpdk_portid == mtr->port_id)
			return mtr;
	}
	return NULL;
}

static int
zxdh_meter_create(struct rte_eth_dev *dev, uint32_t meter_id,
				struct rte_mtr_params *params, int shared,
				struct rte_mtr_error *error)
{
	struct zxdh_hw *priv = dev->data->dev_private;
	struct zxdh_mtr_list *mtr_list = &zxdh_shared_data->mtr_list;
	struct zxdh_mtr_object *mtr;
	struct zxdh_meter_profile *mtr_profile;
	struct zxdh_meter_policy *mtr_policy;
	uint8_t dir = 0;
	int ret;

	if (shared)
		return -rte_mtr_error_set(error, ENOTSUP,
					RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
					"Meter share is not supported");

	ret = zxdh_meter_validate(meter_id, params, error);
	if (ret)
		return ret;

	if (zxdh_check_port_mtr_bind(dev, dir))
		return -rte_mtr_error_set(error, EEXIST,
				RTE_MTR_ERROR_TYPE_MTR_ID, NULL,
				"Meter object already bind to dev.");

	mtr_profile = zxdh_mtr_profile_find_by_id(&zxdh_shared_data->meter_profile_list,
			params->meter_profile_id,
			dev->data->port_id
	);

	if (mtr_profile == NULL)
		return -rte_mtr_error_set(error, EEXIST,
					RTE_MTR_ERROR_TYPE_METER_PROFILE, &params->meter_profile_id,
					"Meter profile object is not exists.");
	mtr_profile->ref_cnt++;
	mtr_policy = zxdh_mtr_policy_find_by_id(&zxdh_shared_data->mtr_policy_list,
			params->meter_policy_id,
			dev->data->port_id);

	if (mtr_policy == NULL) {
		ret = -rte_mtr_error_set(error, EEXIST,
					RTE_MTR_ERROR_TYPE_METER_PROFILE, &params->meter_policy_id,
					"Meter policy object is not exists.");
		mtr_profile->ref_cnt--;
		return ret;
	}
	mtr_policy->ref_cnt++;

	mtr = zxdh_mtr_obj_alloc(zxdh_shared_data->mtr_mp);
	if (mtr == NULL) {
		ret = -rte_mtr_error_set(error, ENOMEM,
					RTE_MTR_ERROR_TYPE_MTR_PARAMS, NULL,
					"Memory alloc failed for meter.");
		mtr_policy->ref_cnt--;
		mtr_profile->ref_cnt--;
		return ret;
	}
	memset(mtr, 0, sizeof(struct zxdh_mtr_object));

	mtr->meter_id = meter_id;
	mtr->profile = mtr_profile;

	zxdh_meter_build_actions(&mtr->mtr_action, params);
	TAILQ_INSERT_TAIL(mtr_list, mtr, next);
	mtr->enable = !!params->meter_enable;
	mtr->shared = !!shared;
	mtr->mtr_ref_cnt++;
	mtr->vfid = priv->vfid;
	mtr->port_id = dev->data->port_id;
	mtr->policy = mtr_policy;
	mtr->direction = !!dir;
	if (params->meter_enable) {
		ret = zxdh_mtr_flow_offlad(dev, mtr, error);
		if (ret)
			goto error;
	}
	ret = zxdh_set_mtr_enable(dev, mtr->direction, 1, error);
	if (ret)
		goto error;
	return ret;
error:
	zxdh_mtr_obj_free(dev, mtr);
	return ret;
}

static int
zxdh_meter_destroy(struct rte_eth_dev *dev, uint32_t meter_id,
			struct rte_mtr_error *error)
{
	struct zxdh_mtr_object *mtr;

	mtr = zxdh_mtr_find(meter_id, dev->data->port_id);
	if (mtr == NULL)
		return -rte_mtr_error_set(error, EEXIST,
					  RTE_MTR_ERROR_TYPE_MTR_ID,
					  NULL, "Meter object id not valid.");
	mtr->enable = 0;
	zxdh_set_mtr_enable(dev, mtr->direction, 0, error);

	if (zxdh_mtr_flow_offlad(dev, mtr, error))
		return -1;

	zxdh_mtr_obj_free(dev, mtr);
	return 0;
}

void
zxdh_mtr_policy_res_free(struct rte_mempool *mtr_policy_mp, struct zxdh_meter_policy *policy)
{
	PMD_DRV_LOG(INFO, "to free policy %d  ref  %d  ", policy->policy_id,  policy->ref_cnt);

	if (--policy->ref_cnt == 0) {
		TAILQ_REMOVE(&zxdh_shared_data->mtr_policy_list, policy, next);
		MP_FREE_OBJ_FUNC(mtr_policy_mp, policy);
	}
}

static int
zxdh_mtr_stats_read(struct rte_eth_dev *dev,
								uint32_t mtr_id,
								struct rte_mtr_stats *stats,
								uint64_t *stats_mask,
								int clear,
								struct rte_mtr_error *error)
{
	struct zxdh_mtr_stats mtr_stat = {0};
	struct zxdh_mtr_object *mtr = NULL;
	int ret = 0;
	/* Meter object must exist. */
	mtr = zxdh_mtr_find(mtr_id, dev->data->port_id);
	if (mtr == NULL)
		return -rte_mtr_error_set(error, ENOENT,
					RTE_MTR_ERROR_TYPE_MTR_ID,
					NULL, "Meter object id not valid.");
	*stats_mask = RTE_MTR_STATS_N_BYTES_DROPPED | RTE_MTR_STATS_N_PKTS_DROPPED;
	memset(&mtr_stat, 0, sizeof(mtr_stat));
	ret = zxdh_mtr_hw_counter_query(dev, clear, mtr->direction, &mtr_stat, error);
	if (ret)
		goto error;
	stats->n_bytes_dropped = mtr_stat.n_bytes_dropped;
	stats->n_pkts_dropped = mtr_stat.n_pkts_dropped;

	return 0;
error:
	return -rte_mtr_error_set(error, ret, RTE_MTR_ERROR_TYPE_STATS, NULL,
				"Failed to read meter drop counters.");
}

static const struct rte_mtr_ops zxdh_mtr_ops = {
	.capabilities_get = zxdh_meter_cap_get,
	.meter_profile_add = zxdh_meter_profile_add,
	.meter_profile_delete = zxdh_meter_profile_delete,
	.create = zxdh_meter_create,
	.destroy = zxdh_meter_destroy,
	.stats_read = zxdh_mtr_stats_read,
	.meter_policy_add = zxdh_meter_policy_add,
	.meter_policy_delete = zxdh_meter_policy_delete,
};

int
zxdh_meter_ops_get(struct rte_eth_dev *dev __rte_unused, void *arg)
{
	*(const struct rte_mtr_ops **)arg = &zxdh_mtr_ops;
	return 0;
}

void
zxdh_mtr_release(struct rte_eth_dev *dev)
{
	struct zxdh_hw *priv = dev->data->dev_private;
	struct zxdh_meter_profile *profile;
	struct rte_mtr_error error = {0};
	struct zxdh_mtr_object *mtr_obj;

	RTE_TAILQ_FOREACH(mtr_obj, &zxdh_shared_data->mtr_list, next) {
		if (mtr_obj->port_id == priv->port_id)
			zxdh_mtr_obj_free(dev, mtr_obj);
	}


	RTE_TAILQ_FOREACH(profile, &zxdh_shared_data->meter_profile_list, next) {
		if (profile->dpdk_port_id == priv->port_id)
			zxdh_mtr_profile_res_free(dev,
				zxdh_shared_data->mtr_profile_mp,
				profile,
				&error
			);
	}

	struct zxdh_meter_policy *policy;

	RTE_TAILQ_FOREACH(policy, &zxdh_shared_data->mtr_policy_list, next) {
		if (policy->dpdk_port_id == priv->port_id)
			zxdh_mtr_policy_res_free(zxdh_shared_data->mtr_policy_mp, policy);
	}
}
