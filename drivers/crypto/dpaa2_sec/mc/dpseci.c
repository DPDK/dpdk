/*-
 * This file is provided under a dual BSD/GPLv2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 *   BSD LICENSE
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright 2016 NXP.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the above-listed copyright holders nor the
 * names of any contributors may be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 *   GPL LICENSE SUMMARY
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <fsl_mc_sys.h>
#include <fsl_mc_cmd.h>
#include <fsl_dpseci.h>
#include <fsl_dpseci_cmd.h>

int
dpseci_open(struct fsl_mc_io *mc_io,
	    uint32_t cmd_flags,
	    int dpseci_id,
	    uint16_t *token)
{
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_OPEN,
					  cmd_flags,
					  0);
	DPSECI_CMD_OPEN(cmd, dpseci_id);

	/* send command to mc */
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	*token = MC_CMD_HDR_READ_TOKEN(cmd.header);

	return 0;
}

int
dpseci_close(struct fsl_mc_io *mc_io,
	     uint32_t cmd_flags,
	     uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_CLOSE,
					  cmd_flags,
					  token);

	/* send command to mc */
	return mc_send_command(mc_io, &cmd);
}

int
dpseci_create(struct fsl_mc_io *mc_io,
	      uint16_t dprc_token,
	      uint32_t cmd_flags,
	      const struct dpseci_cfg *cfg,
	      uint32_t *obj_id)
{
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_CREATE,
					  cmd_flags,
					  dprc_token);
	DPSECI_CMD_CREATE(cmd, cfg);

	/* send command to mc */
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	CMD_CREATE_RSP_GET_OBJ_ID_PARAM0(cmd, *obj_id);

	return 0;
}

int
dpseci_destroy(struct fsl_mc_io	*mc_io,
	       uint16_t	dprc_token,
	       uint32_t	cmd_flags,
	       uint32_t	object_id)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_DESTROY,
					  cmd_flags,
					  dprc_token);
	/* set object id to destroy */
	CMD_DESTROY_SET_OBJ_ID_PARAM0(cmd, object_id);
	/* send command to mc */
	return mc_send_command(mc_io, &cmd);
}

int
dpseci_enable(struct fsl_mc_io *mc_io,
	      uint32_t cmd_flags,
	      uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_ENABLE,
					  cmd_flags,
					  token);

	/* send command to mc */
	return mc_send_command(mc_io, &cmd);
}

int
dpseci_disable(struct fsl_mc_io *mc_io,
	       uint32_t cmd_flags,
	       uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_DISABLE,
					  cmd_flags,
					  token);

	/* send command to mc */
	return mc_send_command(mc_io, &cmd);
}

int
dpseci_is_enabled(struct fsl_mc_io *mc_io,
		  uint32_t cmd_flags,
		  uint16_t token,
		  int *en)
{
	struct mc_command cmd = { 0 };
	int err;
	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_IS_ENABLED,
					  cmd_flags,
					  token);

	/* send command to mc */
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	DPSECI_RSP_IS_ENABLED(cmd, *en);

	return 0;
}

int
dpseci_reset(struct fsl_mc_io *mc_io,
	     uint32_t cmd_flags,
	     uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_RESET,
					  cmd_flags,
					  token);

	/* send command to mc */
	return mc_send_command(mc_io, &cmd);
}

int
dpseci_get_irq(struct fsl_mc_io *mc_io,
	       uint32_t cmd_flags,
	       uint16_t token,
	       uint8_t irq_index,
	       int *type,
	       struct dpseci_irq_cfg *irq_cfg)
{
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_GET_IRQ,
					  cmd_flags,
					  token);
	DPSECI_CMD_GET_IRQ(cmd, irq_index);

	/* send command to mc */
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	DPSECI_RSP_GET_IRQ(cmd, *type, irq_cfg);

	return 0;
}

int
dpseci_set_irq(struct fsl_mc_io *mc_io,
	       uint32_t cmd_flags,
	       uint16_t token,
	       uint8_t irq_index,
	       struct dpseci_irq_cfg *irq_cfg)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_SET_IRQ,
					  cmd_flags,
					  token);
	DPSECI_CMD_SET_IRQ(cmd, irq_index, irq_cfg);

	/* send command to mc */
	return mc_send_command(mc_io, &cmd);
}

int
dpseci_get_irq_enable(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      uint8_t irq_index,
		      uint8_t *en)
{
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_GET_IRQ_ENABLE,
					  cmd_flags,
					  token);
	DPSECI_CMD_GET_IRQ_ENABLE(cmd, irq_index);

	/* send command to mc */
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	DPSECI_RSP_GET_IRQ_ENABLE(cmd, *en);

	return 0;
}

int
dpseci_set_irq_enable(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      uint8_t irq_index,
		      uint8_t en)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_SET_IRQ_ENABLE,
					  cmd_flags,
					  token);
	DPSECI_CMD_SET_IRQ_ENABLE(cmd, irq_index, en);

	/* send command to mc */
	return mc_send_command(mc_io, &cmd);
}

int
dpseci_get_irq_mask(struct fsl_mc_io *mc_io,
		    uint32_t cmd_flags,
		    uint16_t token,
		    uint8_t irq_index,
		    uint32_t *mask)
{
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_GET_IRQ_MASK,
					  cmd_flags,
					  token);
	DPSECI_CMD_GET_IRQ_MASK(cmd, irq_index);

	/* send command to mc */
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	DPSECI_RSP_GET_IRQ_MASK(cmd, *mask);

	return 0;
}

int
dpseci_set_irq_mask(struct fsl_mc_io *mc_io,
		    uint32_t cmd_flags,
		    uint16_t token,
		    uint8_t irq_index,
		    uint32_t mask)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_SET_IRQ_MASK,
					  cmd_flags,
					  token);
	DPSECI_CMD_SET_IRQ_MASK(cmd, irq_index, mask);

	/* send command to mc */
	return mc_send_command(mc_io, &cmd);
}

int
dpseci_get_irq_status(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      uint8_t irq_index,
		      uint32_t *status)
{
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_GET_IRQ_STATUS,
					  cmd_flags,
					  token);
	DPSECI_CMD_GET_IRQ_STATUS(cmd, irq_index, *status);

	/* send command to mc */
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	DPSECI_RSP_GET_IRQ_STATUS(cmd, *status);

	return 0;
}

int
dpseci_clear_irq_status(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			uint8_t irq_index,
			uint32_t status)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_CLEAR_IRQ_STATUS,
					  cmd_flags,
					  token);
	DPSECI_CMD_CLEAR_IRQ_STATUS(cmd, irq_index, status);

	/* send command to mc */
	return mc_send_command(mc_io, &cmd);
}

int
dpseci_get_attributes(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      struct dpseci_attr *attr)
{
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_GET_ATTR,
					  cmd_flags,
					  token);

	/* send command to mc */
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	DPSECI_RSP_GET_ATTR(cmd, attr);

	return 0;
}

int
dpseci_set_rx_queue(struct fsl_mc_io *mc_io,
		    uint32_t cmd_flags,
		    uint16_t token,
		    uint8_t queue,
		    const struct dpseci_rx_queue_cfg *cfg)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_SET_RX_QUEUE,
					  cmd_flags,
					  token);
	DPSECI_CMD_SET_RX_QUEUE(cmd, queue, cfg);

	/* send command to mc */
	return mc_send_command(mc_io, &cmd);
}

int
dpseci_get_rx_queue(struct fsl_mc_io *mc_io,
		    uint32_t cmd_flags,
		    uint16_t token,
		    uint8_t queue,
		    struct dpseci_rx_queue_attr *attr)
{
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_GET_RX_QUEUE,
					  cmd_flags,
					  token);
	DPSECI_CMD_GET_RX_QUEUE(cmd, queue);

	/* send command to mc */
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	DPSECI_RSP_GET_RX_QUEUE(cmd, attr);

	return 0;
}

int
dpseci_get_tx_queue(struct fsl_mc_io *mc_io,
		    uint32_t cmd_flags,
		    uint16_t token,
		    uint8_t queue,
		    struct dpseci_tx_queue_attr *attr)
{
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_GET_TX_QUEUE,
					  cmd_flags,
					  token);
	DPSECI_CMD_GET_TX_QUEUE(cmd, queue);

	/* send command to mc */
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	DPSECI_RSP_GET_TX_QUEUE(cmd, attr);

	return 0;
}

int
dpseci_get_sec_attr(struct fsl_mc_io *mc_io,
		    uint32_t cmd_flags,
		    uint16_t token,
		    struct dpseci_sec_attr *attr)
{
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_GET_SEC_ATTR,
					  cmd_flags,
					  token);

	/* send command to mc */
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	DPSECI_RSP_GET_SEC_ATTR(cmd, attr);

	return 0;
}

int
dpseci_get_sec_counters(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			struct dpseci_sec_counters *counters)
{
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_GET_SEC_COUNTERS,
					  cmd_flags,
					  token);

	/* send command to mc */
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	DPSECI_RSP_GET_SEC_COUNTERS(cmd, counters);

	return 0;
}

int
dpseci_get_api_version(struct fsl_mc_io *mc_io,
		       uint32_t cmd_flags,
		       uint16_t *major_ver,
		       uint16_t *minor_ver)
{
	struct mc_command cmd = { 0 };
	int err;

	cmd.header = mc_encode_cmd_header(DPSECI_CMDID_GET_API_VERSION,
					cmd_flags,
					0);

	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	DPSECI_RSP_GET_API_VERSION(cmd, *major_ver, *minor_ver);

	return 0;
}
