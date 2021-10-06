/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2013-2015 Freescale Semiconductor Inc.
 * Copyright 2018-2021 NXP
 *
 */
#ifndef __FSL_DPOPR_H_
#define __FSL_DPOPR_H_

/** @addtogroup dpopr Data Path Order Restoration API
 * Contains initialization APIs and runtime APIs for the Order Restoration
 * @{
 */

/** Order Restoration properties */

/**
 * Create a new Order Point Record option
 */
#define OPR_OPT_CREATE 0x1
/**
 * Retire an existing Order Point Record option
 */
#define OPR_OPT_RETIRE 0x2
/**
 * Assign an existing Order Point Record to a queue
 */
#define OPR_OPT_ASSIGN 0x4
/**
 * struct opr_cfg - Structure representing OPR configuration
 * @oprrws: Order point record (OPR) restoration window size (0 to 5)
 *			0 - Window size is 32 frames.
 *			1 - Window size is 64 frames.
 *			2 - Window size is 128 frames.
 *			3 - Window size is 256 frames.
 *			4 - Window size is 512 frames.
 *			5 - Window size is 1024 frames.
 *@oa: OPR auto advance NESN window size (0 disabled, 1 enabled)
 *@olws: OPR acceptable late arrival window size (0 to 3)
 *			0 - Disabled. Late arrivals are always rejected.
 *			1 - Window size is 32 frames.
 *			2 - Window size is the same as the OPR restoration
 *			window size configured in the OPRRWS field.
 *			3 - Window size is 8192 frames.
 *			Late arrivals are always accepted.
 *@oeane: Order restoration list (ORL) resource exhaustion
 *			advance NESN enable (0 disabled, 1 enabled)
 *@oloe: OPR loose ordering enable (0 disabled, 1 enabled)
 */
struct opr_cfg {
	uint8_t oprrws;
	uint8_t oa;
	uint8_t olws;
	uint8_t oeane;
	uint8_t oloe;
};

/**
 * struct opr_qry - Structure representing OPR configuration
 * @enable: Enabled state
 * @rip: Retirement In Progress
 * @ndsn: Next dispensed sequence number
 * @nesn: Next expected sequence number
 * @ea_hseq: Early arrival head sequence number
 * @hseq_nlis: HSEQ not last in sequence
 * @ea_tseq: Early arrival tail sequence number
 * @tseq_nlis: TSEQ not last in sequence
 * @ea_tptr: Early arrival tail pointer
 * @ea_hptr: Early arrival head pointer
 * @opr_id: Order Point Record ID
 * @opr_vid: Order Point Record Virtual ID
 */
struct opr_qry {
	char enable;
	char rip;
	uint16_t ndsn;
	uint16_t nesn;
	uint16_t ea_hseq;
	char hseq_nlis;
	uint16_t ea_tseq;
	char tseq_nlis;
	uint16_t ea_tptr;
	uint16_t ea_hptr;
	uint16_t opr_id;
	uint16_t opr_vid;
};

#endif /* __FSL_DPOPR_H_ */
