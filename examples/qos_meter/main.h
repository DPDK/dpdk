/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _MAIN_H_
#define _MAIN_H_

enum policer_action {
        GREEN = e_RTE_METER_GREEN,
        YELLOW = e_RTE_METER_YELLOW,
        RED = e_RTE_METER_RED,
        DROP = 3,
};

enum policer_action policer_table[e_RTE_METER_COLORS][e_RTE_METER_COLORS] =
{
	{ GREEN, RED, RED},
	{ DROP, YELLOW, RED},
	{ DROP, DROP, RED}
};

#if APP_MODE == APP_MODE_FWD

#define FUNC_METER(a,b,c,d) color, flow_id=flow_id, pkt_len=pkt_len, time=time
#define FUNC_CONFIG(a, b) 0
#define PARAMS	app_srtcm_params
#define FLOW_METER int

#elif APP_MODE == APP_MODE_SRTCM_COLOR_BLIND

#define FUNC_METER(a,b,c,d) rte_meter_srtcm_color_blind_check(a,b,c)
#define FUNC_CONFIG   rte_meter_srtcm_config
#define PARAMS        app_srtcm_params
#define FLOW_METER    struct rte_meter_srtcm

#elif (APP_MODE == APP_MODE_SRTCM_COLOR_AWARE)

#define FUNC_METER    rte_meter_srtcm_color_aware_check
#define FUNC_CONFIG   rte_meter_srtcm_config
#define PARAMS        app_srtcm_params
#define FLOW_METER    struct rte_meter_srtcm

#elif (APP_MODE == APP_MODE_TRTCM_COLOR_BLIND)

#define FUNC_METER(a,b,c,d) rte_meter_trtcm_color_blind_check(a,b,c)
#define FUNC_CONFIG  rte_meter_trtcm_config
#define PARAMS       app_trtcm_params
#define FLOW_METER   struct rte_meter_trtcm

#elif (APP_MODE == APP_MODE_TRTCM_COLOR_AWARE)

#define FUNC_METER   rte_meter_trtcm_color_aware_check
#define FUNC_CONFIG  rte_meter_trtcm_config
#define PARAMS       app_trtcm_params
#define FLOW_METER   struct rte_meter_trtcm

#else
#error Invalid value for APP_MODE
#endif




#endif /* _MAIN_H_ */
