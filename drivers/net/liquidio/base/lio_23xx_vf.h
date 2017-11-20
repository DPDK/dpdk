/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
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

#define CN23XX_VF_BUSY_READING_REG_LOOP_COUNT	100000

void cn23xx_vf_ask_pf_to_do_flr(struct lio_device *lio_dev);

int cn23xx_pfvf_handshake(struct lio_device *lio_dev);

int cn23xx_vf_setup_device(struct lio_device  *lio_dev);

void cn23xx_vf_handle_mbox(struct lio_device *lio_dev);
#endif /* _LIO_23XX_VF_H_  */
