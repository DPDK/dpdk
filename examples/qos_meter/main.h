/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#define FUNC_CONFIG(a,b)
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
