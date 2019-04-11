/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _MAIN_H_
#define _MAIN_H_

enum policer_action {
		GREEN = RTE_COLOR_GREEN,
		YELLOW = RTE_COLOR_YELLOW,
		RED = RTE_COLOR_RED,
		DROP = 3,
};

enum policer_action policer_table[RTE_COLORS][RTE_COLORS] =
{
	{ GREEN, RED, RED},
	{ DROP, YELLOW, RED},
	{ DROP, DROP, RED}
};

#if APP_MODE == APP_MODE_FWD

#define FUNC_METER(m, p, time, pkt_len, pkt_color)	\
({							\
	void *mp = m;					\
	void *pp = p;					\
	mp = mp;					\
	pp = pp;					\
	time = time;					\
	pkt_len = pkt_len;				\
	pkt_color;					\
})
#define FUNC_CONFIG(a, b) 0
#define FLOW_METER int
#define PROFILE	app_srtcm_profile

#elif APP_MODE == APP_MODE_SRTCM_COLOR_BLIND

#define FUNC_METER(m, p, time, pkt_len, pkt_color)	\
	rte_meter_srtcm_color_blind_check(m, p, time, pkt_len)
#define FUNC_CONFIG   rte_meter_srtcm_config
#define FLOW_METER    struct rte_meter_srtcm
#define PROFILE       app_srtcm_profile

#elif (APP_MODE == APP_MODE_SRTCM_COLOR_AWARE)

#define FUNC_METER    rte_meter_srtcm_color_aware_check
#define FUNC_CONFIG   rte_meter_srtcm_config
#define FLOW_METER    struct rte_meter_srtcm
#define PROFILE       app_srtcm_profile

#elif (APP_MODE == APP_MODE_TRTCM_COLOR_BLIND)

#define FUNC_METER(m, p, time, pkt_len, pkt_color)	\
	rte_meter_trtcm_color_blind_check(m, p, time, pkt_len)
#define FUNC_CONFIG  rte_meter_trtcm_config
#define FLOW_METER   struct rte_meter_trtcm
#define PROFILE      app_trtcm_profile

#elif (APP_MODE == APP_MODE_TRTCM_COLOR_AWARE)

#define FUNC_METER rte_meter_trtcm_color_aware_check
#define FUNC_CONFIG  rte_meter_trtcm_config
#define FLOW_METER   struct rte_meter_trtcm
#define PROFILE      app_trtcm_profile

#else
#error Invalid value for APP_MODE
#endif

#endif /* _MAIN_H_ */
