/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#ifndef __IRO_H__
#define __IRO_H__

/* Ystorm flow control mode. Use enum fw_flow_ctrl_mode */
#define YSTORM_FLOW_CONTROL_MODE_OFFSET		(IRO[0].base)
#define YSTORM_FLOW_CONTROL_MODE_SIZE		(IRO[0].size)
/* Tstorm port statistics */
#define TSTORM_PORT_STAT_OFFSET(port_id) \
(IRO[1].base + ((port_id) * IRO[1].m1))
#define TSTORM_PORT_STAT_SIZE			(IRO[1].size)
/* Ustorm VF-PF Channel ready flag */
#define USTORM_VF_PF_CHANNEL_READY_OFFSET(vf_id) \
(IRO[3].base + ((vf_id) * IRO[3].m1))
#define USTORM_VF_PF_CHANNEL_READY_SIZE		(IRO[3].size)
/* Ustorm Final flr cleanup ack */
#define USTORM_FLR_FINAL_ACK_OFFSET(pf_id) \
(IRO[4].base + ((pf_id) * IRO[4].m1))
#define USTORM_FLR_FINAL_ACK_SIZE		(IRO[4].size)
/* Ustorm Event ring consumer */
#define USTORM_EQE_CONS_OFFSET(pf_id) \
(IRO[5].base + ((pf_id) * IRO[5].m1))
#define USTORM_EQE_CONS_SIZE			(IRO[5].size)
/* Ustorm Common Queue ring consumer */
#define USTORM_COMMON_QUEUE_CONS_OFFSET(global_queue_id) \
(IRO[6].base + ((global_queue_id) * IRO[6].m1))
#define USTORM_COMMON_QUEUE_CONS_SIZE		(IRO[6].size)
/* Xstorm Integration Test Data */
#define XSTORM_INTEG_TEST_DATA_OFFSET		(IRO[7].base)
#define XSTORM_INTEG_TEST_DATA_SIZE		(IRO[7].size)
/* Ystorm Integration Test Data */
#define YSTORM_INTEG_TEST_DATA_OFFSET		(IRO[8].base)
#define YSTORM_INTEG_TEST_DATA_SIZE		(IRO[8].size)
/* Pstorm Integration Test Data */
#define PSTORM_INTEG_TEST_DATA_OFFSET		(IRO[9].base)
#define PSTORM_INTEG_TEST_DATA_SIZE		(IRO[9].size)
/* Tstorm Integration Test Data */
#define TSTORM_INTEG_TEST_DATA_OFFSET		(IRO[10].base)
#define TSTORM_INTEG_TEST_DATA_SIZE		(IRO[10].size)
/* Mstorm Integration Test Data */
#define MSTORM_INTEG_TEST_DATA_OFFSET		(IRO[11].base)
#define MSTORM_INTEG_TEST_DATA_SIZE		(IRO[11].size)
/* Ustorm Integration Test Data */
#define USTORM_INTEG_TEST_DATA_OFFSET		(IRO[12].base)
#define USTORM_INTEG_TEST_DATA_SIZE		(IRO[12].size)
/* Mstorm queue statistics */
#define MSTORM_QUEUE_STAT_OFFSET(stat_counter_id) \
(IRO[17].base + ((stat_counter_id) * IRO[17].m1))
#define MSTORM_QUEUE_STAT_SIZE			(IRO[17].size)
/* Mstorm producers */
#define MSTORM_PRODS_OFFSET(queue_id) \
(IRO[18].base + ((queue_id) * IRO[18].m1))
#define MSTORM_PRODS_SIZE			(IRO[18].size)
/* TPA agregation timeout in us resolution (on ASIC) */
#define MSTORM_TPA_TIMEOUT_US_OFFSET		(IRO[19].base)
#define MSTORM_TPA_TIMEOUT_US_SIZE		(IRO[19].size)
/* Ustorm queue statistics */
#define USTORM_QUEUE_STAT_OFFSET(stat_counter_id) \
(IRO[20].base + ((stat_counter_id) * IRO[20].m1))
#define USTORM_QUEUE_STAT_SIZE (IRO[20].size)
/* Ustorm queue zone */
#define USTORM_ETH_QUEUE_ZONE_OFFSET(queue_id) \
(IRO[21].base + ((queue_id) * IRO[21].m1))
#define USTORM_ETH_QUEUE_ZONE_SIZE		(IRO[21].size)
/* Pstorm queue statistics */
#define PSTORM_QUEUE_STAT_OFFSET(stat_counter_id) \
(IRO[22].base + ((stat_counter_id) * IRO[22].m1))
#define PSTORM_QUEUE_STAT_SIZE			(IRO[22].size)
/* Tstorm last parser message */
#define TSTORM_ETH_PRS_INPUT_OFFSET		(IRO[23].base)
#define TSTORM_ETH_PRS_INPUT_SIZE		(IRO[23].size)
/* Tstorm Eth limit Rx rate */
#define ETH_RX_RATE_LIMIT_OFFSET(pf_id) \
(IRO[24].base + ((pf_id) * IRO[24].m1))
#define ETH_RX_RATE_LIMIT_SIZE			(IRO[24].size)
/* Ystorm queue zone */
#define YSTORM_ETH_QUEUE_ZONE_OFFSET(queue_id) \
(IRO[25].base + ((queue_id) * IRO[25].m1))
#define YSTORM_ETH_QUEUE_ZONE_SIZE		(IRO[25].size)
/* Ystorm cqe producer */
#define YSTORM_TOE_CQ_PROD_OFFSET(rss_id) \
(IRO[26].base + ((rss_id) * IRO[26].m1))
#define YSTORM_TOE_CQ_PROD_SIZE			(IRO[26].size)
/* Ustorm cqe producer */
#define USTORM_TOE_CQ_PROD_OFFSET(rss_id) \
(IRO[27].base + ((rss_id) * IRO[27].m1))
#define USTORM_TOE_CQ_PROD_SIZE			(IRO[27].size)
/* Ustorm grq producer */
#define USTORM_TOE_GRQ_PROD_OFFSET(pf_id) \
(IRO[28].base + ((pf_id) * IRO[28].m1))
#define USTORM_TOE_GRQ_PROD_SIZE		(IRO[28].size)
/* Tstorm cmdq-cons of given command queue-id */
#define TSTORM_SCSI_CMDQ_CONS_OFFSET(cmdq_queue_id) \
(IRO[29].base + ((cmdq_queue_id) * IRO[29].m1))
#define TSTORM_SCSI_CMDQ_CONS_SIZE		(IRO[29].size)
#define TSTORM_SCSI_BDQ_EXT_PROD_OFFSET(func_id, bdq_id) \
(IRO[30].base + ((func_id) * IRO[30].m1) + ((bdq_id) * IRO[30].m2))
#define TSTORM_SCSI_BDQ_EXT_PROD_SIZE		(IRO[30].size)
/* Mstorm rq-cons of given queue-id */
#define MSTORM_SCSI_RQ_CONS_OFFSET(rq_queue_id) \
(IRO[31].base + ((rq_queue_id) * IRO[31].m1))
#define MSTORM_SCSI_RQ_CONS_SIZE		(IRO[31].size)
/* Mstorm bdq-external-producer of given BDQ function ID, BDqueue-id */
#define MSTORM_SCSI_BDQ_EXT_PROD_OFFSET(func_id, bdq_id) \
(IRO[32].base + ((func_id) * IRO[32].m1) + ((bdq_id) * IRO[32].m2))
#define MSTORM_SCSI_BDQ_EXT_PROD_SIZE		(IRO[32].size)

#endif /* __IRO_H__ */
