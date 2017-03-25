/*
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Cavium, Inc.. All rights reserved.
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
 *     * Neither the name of Cavium, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER(S) OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LIO_23XX_VF_H_
#define _LIO_23XX_VF_H_

#include <stdio.h>

#include "lio_struct.h"

static const struct lio_config default_cn23xx_conf	= {
	.card_type				= LIO_23XX,
	.card_name				= LIO_23XX_NAME,
	/** IQ attributes */
	.iq					= {
		.max_iqs			= CN23XX_CFG_IO_QUEUES,
		.pending_list_size		=
			(CN23XX_MAX_IQ_DESCRIPTORS * CN23XX_CFG_IO_QUEUES),
		.instr_type			= OCTEON_64BYTE_INSTR,
	},

	/** OQ attributes */
	.oq					= {
		.max_oqs			= CN23XX_CFG_IO_QUEUES,
		.info_ptr			= OCTEON_OQ_INFOPTR_MODE,
		.refill_threshold		= CN23XX_OQ_REFIL_THRESHOLD,
	},

	.num_nic_ports				= CN23XX_DEFAULT_NUM_PORTS,
	.num_def_rx_descs			= CN23XX_MAX_OQ_DESCRIPTORS,
	.num_def_tx_descs			= CN23XX_MAX_IQ_DESCRIPTORS,
	.def_rx_buf_size			= CN23XX_OQ_BUF_SIZE,
};

static inline const struct lio_config *
lio_get_conf(struct lio_device *lio_dev)
{
	const struct lio_config *default_lio_conf = NULL;

	/* check the LIO Device model & return the corresponding lio
	 * configuration
	 */
	default_lio_conf = &default_cn23xx_conf;

	if (default_lio_conf == NULL) {
		lio_dev_err(lio_dev, "Configuration verification failed\n");
		return NULL;
	}

	return default_lio_conf;
}

/** Turns off the input and output queues for the device
 *  @param lio_dev which device io queues to disable
 */
int cn23xx_vf_set_io_queues_off(struct lio_device *lio_dev);

#define CN23XX_VF_BUSY_READING_REG_LOOP_COUNT	100000

void cn23xx_vf_ask_pf_to_do_flr(struct lio_device *lio_dev);

int cn23xx_pfvf_handshake(struct lio_device *lio_dev);

int cn23xx_vf_setup_device(struct lio_device  *lio_dev);

void cn23xx_vf_handle_mbox(struct lio_device *lio_dev);
#endif /* _LIO_23XX_VF_H_  */
