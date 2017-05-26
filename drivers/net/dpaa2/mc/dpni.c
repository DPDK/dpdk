/*-
 * This file is provided under a dual BSD/GPLv2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 *   BSD LICENSE
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright (c) 2016 NXP.
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
#include <fsl_dpni.h>
#include <fsl_dpni_cmd.h>

int dpni_prepare_key_cfg(const struct dpkg_profile_cfg *cfg,
			 uint8_t *key_cfg_buf)
{
	int i, j;
	int offset = 0;
	int param = 1;
	uint64_t *params = (uint64_t *)key_cfg_buf;

	if (!key_cfg_buf || !cfg)
		return -EINVAL;

	params[0] |= mc_enc(0, 8, cfg->num_extracts);
	params[0] = cpu_to_le64(params[0]);

	if (cfg->num_extracts >= DPKG_MAX_NUM_OF_EXTRACTS)
		return -EINVAL;

	for (i = 0; i < cfg->num_extracts; i++) {
		switch (cfg->extracts[i].type) {
		case DPKG_EXTRACT_FROM_HDR:
			params[param] |= mc_enc(0, 8,
					cfg->extracts[i].extract.from_hdr.prot);
			params[param] |= mc_enc(8, 4,
					cfg->extracts[i].extract.from_hdr.type);
			params[param] |= mc_enc(16, 8,
					cfg->extracts[i].extract.from_hdr.size);
			params[param] |= mc_enc(24, 8,
					cfg->extracts[i].extract.
					from_hdr.offset);
			params[param] |= mc_enc(32, 32,
					cfg->extracts[i].extract.
					from_hdr.field);
			params[param] = cpu_to_le64(params[param]);
			param++;
			params[param] |= mc_enc(0, 8,
					cfg->extracts[i].extract.
					from_hdr.hdr_index);
			break;
		case DPKG_EXTRACT_FROM_DATA:
			params[param] |= mc_enc(16, 8,
					cfg->extracts[i].extract.
					from_data.size);
			params[param] |= mc_enc(24, 8,
					cfg->extracts[i].extract.
					from_data.offset);
			params[param] = cpu_to_le64(params[param]);
			param++;
			break;
		case DPKG_EXTRACT_FROM_PARSE:
			params[param] |= mc_enc(16, 8,
					cfg->extracts[i].extract.
					from_parse.size);
			params[param] |= mc_enc(24, 8,
					cfg->extracts[i].extract.
					from_parse.offset);
			params[param] = cpu_to_le64(params[param]);
			param++;
			break;
		default:
			return -EINVAL;
		}
		params[param] |= mc_enc(
			24, 8, cfg->extracts[i].num_of_byte_masks);
		params[param] |= mc_enc(32, 4, cfg->extracts[i].type);
		params[param] = cpu_to_le64(params[param]);
		param++;
		for (offset = 0, j = 0;
			j < DPKG_NUM_OF_MASKS;
			offset += 16, j++) {
			params[param] |= mc_enc(
				(offset), 8, cfg->extracts[i].masks[j].mask);
			params[param] |= mc_enc(
				(offset + 8), 8,
				cfg->extracts[i].masks[j].offset);
		}
		params[param] = cpu_to_le64(params[param]);
		param++;
	}
	return 0;
}

int dpni_open(struct fsl_mc_io *mc_io,
	      uint32_t cmd_flags,
	      int dpni_id,
	      uint16_t *token)
{
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_OPEN,
					  cmd_flags,
					  0);
	DPNI_CMD_OPEN(cmd, dpni_id);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	*token = MC_CMD_HDR_READ_TOKEN(cmd.header);

	return 0;
}

int dpni_close(struct fsl_mc_io *mc_io,
	       uint32_t cmd_flags,
	       uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_CLOSE,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpni_create(struct fsl_mc_io	*mc_io,
		uint16_t	dprc_token,
		uint32_t	cmd_flags,
		const struct dpni_cfg	*cfg,
		uint32_t	*obj_id)
{
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_CREATE,
					  cmd_flags,
					  dprc_token);
	DPNI_CMD_CREATE(cmd, cfg);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	CMD_CREATE_RSP_GET_OBJ_ID_PARAM0(cmd, *obj_id);

	return 0;
}

int dpni_destroy(struct fsl_mc_io	*mc_io,
		 uint16_t	dprc_token,
		uint32_t	cmd_flags,
		uint32_t	object_id)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_DESTROY,
					  cmd_flags,
					  dprc_token);
	/* set object id to destroy */
	CMD_DESTROY_SET_OBJ_ID_PARAM0(cmd, object_id);
	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpni_set_pools(struct fsl_mc_io *mc_io,
		   uint32_t cmd_flags,
		   uint16_t token,
		   const struct dpni_pools_cfg *cfg)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_POOLS,
					  cmd_flags,
					  token);
	DPNI_CMD_SET_POOLS(cmd, cfg);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpni_enable(struct fsl_mc_io *mc_io,
		uint32_t cmd_flags,
		uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_ENABLE,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpni_disable(struct fsl_mc_io *mc_io,
		 uint32_t cmd_flags,
		 uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_DISABLE,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpni_is_enabled(struct fsl_mc_io *mc_io,
		    uint32_t cmd_flags,
		    uint16_t token,
		    int *en)
{
	struct mc_command cmd = { 0 };
	int err;
	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_IS_ENABLED, cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	DPNI_RSP_IS_ENABLED(cmd, *en);

	return 0;
}

int dpni_reset(struct fsl_mc_io *mc_io,
	       uint32_t cmd_flags,
	       uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_RESET,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpni_get_attributes(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			struct dpni_attr *attr)
{
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_ATTR,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	DPNI_RSP_GET_ATTR(cmd, attr);

	return 0;
}

int dpni_set_errors_behavior(struct fsl_mc_io *mc_io,
			     uint32_t cmd_flags,
			     uint16_t token,
			      struct dpni_error_cfg *cfg)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_ERRORS_BEHAVIOR,
					  cmd_flags,
					  token);
	DPNI_CMD_SET_ERRORS_BEHAVIOR(cmd, cfg);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpni_get_buffer_layout(struct fsl_mc_io *mc_io,
			   uint32_t cmd_flags,
			   uint16_t token,
			   enum dpni_queue_type qtype,
			   struct dpni_buffer_layout *layout)
{
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_BUFFER_LAYOUT,
					  cmd_flags,
					  token);
	DPNI_CMD_GET_BUFFER_LAYOUT(cmd, qtype);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	DPNI_RSP_GET_BUFFER_LAYOUT(cmd, layout);

	return 0;
}

int dpni_set_buffer_layout(struct fsl_mc_io *mc_io,
			   uint32_t cmd_flags,
			      uint16_t token,
			      enum dpni_queue_type qtype,
			      const struct dpni_buffer_layout *layout)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_BUFFER_LAYOUT,
					  cmd_flags,
					  token);
	DPNI_CMD_SET_BUFFER_LAYOUT(cmd, qtype, layout);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpni_set_offload(struct fsl_mc_io *mc_io,
		     uint32_t cmd_flags,
		     uint16_t token,
		     enum dpni_offload type,
		     uint32_t config)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_OFFLOAD,
					  cmd_flags,
					  token);
	DPNI_CMD_SET_OFFLOAD(cmd, type, config);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpni_get_offload(struct fsl_mc_io *mc_io,
		     uint32_t cmd_flags,
		     uint16_t token,
		     enum dpni_offload type,
		     uint32_t *config)
{
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_OFFLOAD,
					  cmd_flags,
					  token);
	DPNI_CMD_GET_OFFLOAD(cmd, type);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	DPNI_RSP_GET_OFFLOAD(cmd, *config);

	return 0;
}

int dpni_get_qdid(struct fsl_mc_io *mc_io,
		  uint32_t cmd_flags,
		  uint16_t token,
		  enum dpni_queue_type qtype,
		  uint16_t *qdid)
{
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_QDID,
					  cmd_flags,
					  token);
	DPNI_CMD_GET_QDID(cmd, qtype);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	DPNI_RSP_GET_QDID(cmd, *qdid);

	return 0;
}

int dpni_set_link_cfg(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      const struct dpni_link_cfg *cfg)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_LINK_CFG,
					  cmd_flags,
					  token);
	DPNI_CMD_SET_LINK_CFG(cmd, cfg);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpni_get_link_state(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			struct dpni_link_state *state)
{
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_LINK_STATE,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	DPNI_RSP_GET_LINK_STATE(cmd, state);

	return 0;
}

int dpni_set_max_frame_length(struct fsl_mc_io *mc_io,
			      uint32_t cmd_flags,
			      uint16_t token,
			      uint16_t max_frame_length)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_MAX_FRAME_LENGTH,
					  cmd_flags,
					  token);
	DPNI_CMD_SET_MAX_FRAME_LENGTH(cmd, max_frame_length);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpni_get_max_frame_length(struct fsl_mc_io *mc_io,
			      uint32_t cmd_flags,
			      uint16_t token,
			      uint16_t *max_frame_length)
{
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_MAX_FRAME_LENGTH,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	DPNI_RSP_GET_MAX_FRAME_LENGTH(cmd, *max_frame_length);

	return 0;
}

int dpni_set_multicast_promisc(struct fsl_mc_io *mc_io,
			       uint32_t cmd_flags,
			       uint16_t token,
			       int en)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_MCAST_PROMISC,
					  cmd_flags,
					  token);
	DPNI_CMD_SET_MULTICAST_PROMISC(cmd, en);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpni_get_multicast_promisc(struct fsl_mc_io *mc_io,
			       uint32_t cmd_flags,
			       uint16_t token,
			       int *en)
{
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_MCAST_PROMISC,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	DPNI_RSP_GET_MULTICAST_PROMISC(cmd, *en);

	return 0;
}

int dpni_set_unicast_promisc(struct fsl_mc_io *mc_io,
			     uint32_t cmd_flags,
			     uint16_t token,
			     int en)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_UNICAST_PROMISC,
					  cmd_flags,
					  token);
	DPNI_CMD_SET_UNICAST_PROMISC(cmd, en);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpni_get_unicast_promisc(struct fsl_mc_io *mc_io,
			     uint32_t cmd_flags,
			     uint16_t token,
			     int *en)
{
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_UNICAST_PROMISC,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	DPNI_RSP_GET_UNICAST_PROMISC(cmd, *en);

	return 0;
}

int dpni_set_primary_mac_addr(struct fsl_mc_io *mc_io,
			      uint32_t cmd_flags,
			      uint16_t token,
			      const uint8_t mac_addr[6])
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_PRIM_MAC,
					  cmd_flags,
					  token);
	DPNI_CMD_SET_PRIMARY_MAC_ADDR(cmd, mac_addr);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpni_get_primary_mac_addr(struct fsl_mc_io *mc_io,
			      uint32_t cmd_flags,
			      uint16_t token,
			      uint8_t mac_addr[6])
{
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_PRIM_MAC,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	DPNI_RSP_GET_PRIMARY_MAC_ADDR(cmd, mac_addr);

	return 0;
}

int dpni_add_mac_addr(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      const uint8_t mac_addr[6])
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_ADD_MAC_ADDR,
					  cmd_flags,
					  token);
	DPNI_CMD_ADD_MAC_ADDR(cmd, mac_addr);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpni_remove_mac_addr(struct fsl_mc_io *mc_io,
			 uint32_t cmd_flags,
			 uint16_t token,
			 const uint8_t mac_addr[6])
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_REMOVE_MAC_ADDR,
					  cmd_flags,
					  token);
	DPNI_CMD_REMOVE_MAC_ADDR(cmd, mac_addr);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpni_clear_mac_filters(struct fsl_mc_io *mc_io,
			   uint32_t cmd_flags,
			   uint16_t token,
			   int unicast,
			   int multicast)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_CLR_MAC_FILTERS,
					  cmd_flags,
					  token);
	DPNI_CMD_CLEAR_MAC_FILTERS(cmd, unicast, multicast);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpni_get_port_mac_addr(struct fsl_mc_io *mc_io,
			   uint32_t cmd_flags,
			   uint16_t token,
			   uint8_t mac_addr[6])
{
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_PORT_MAC_ADDR,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	DPNI_RSP_GET_PORT_MAC_ADDR(cmd, mac_addr);

	return 0;
}

int dpni_enable_vlan_filter(struct fsl_mc_io *mc_io,
			    uint32_t cmd_flags,
			  uint16_t token,
			  int en)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_ENABLE_VLAN_FILTER,
					  cmd_flags,
					  token);
	DPNI_CMD_ENABLE_VLAN_FILTER(cmd, en);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpni_add_vlan_id(struct fsl_mc_io *mc_io,
		     uint32_t cmd_flags,
		     uint16_t token,
		     uint16_t vlan_id)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_ADD_VLAN_ID,
					  cmd_flags,
					  token);
	DPNI_CMD_ADD_VLAN_ID(cmd, vlan_id);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpni_remove_vlan_id(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			uint16_t vlan_id)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_REMOVE_VLAN_ID,
					  cmd_flags,
					  token);
	DPNI_CMD_REMOVE_VLAN_ID(cmd, vlan_id);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpni_clear_vlan_filters(struct fsl_mc_io *mc_io,
			    uint32_t cmd_flags,
			    uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_CLR_VLAN_FILTERS,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpni_set_rx_tc_dist(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			uint8_t tc_id,
			const struct dpni_rx_tc_dist_cfg *cfg)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_RX_TC_DIST,
					  cmd_flags,
					  token);
	DPNI_CMD_SET_RX_TC_DIST(cmd, tc_id, cfg);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpni_set_tx_confirmation_mode(struct fsl_mc_io	*mc_io,
				  uint32_t		cmd_flags,
			    uint16_t		token,
			    enum dpni_confirmation_mode mode)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_TX_CONFIRMATION_MODE,
					  cmd_flags,
					  token);
	DPNI_CMD_SET_TX_CONFIRMATION_MODE(cmd, mode);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpni_set_congestion_notification(
			struct fsl_mc_io	*mc_io,
			uint32_t		cmd_flags,
			uint16_t		token,
			enum dpni_queue_type qtype,
			uint8_t		tc_id,
			const struct dpni_congestion_notification_cfg *cfg)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(
			DPNI_CMDID_SET_CONGESTION_NOTIFICATION,
			cmd_flags,
			token);
	DPNI_CMD_SET_CONGESTION_NOTIFICATION(cmd, qtype, tc_id, cfg);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpni_get_congestion_notification(struct fsl_mc_io	*mc_io,
				     uint32_t		cmd_flags,
					   uint16_t		token,
				     enum dpni_queue_type qtype,
					   uint8_t		tc_id,
				struct dpni_congestion_notification_cfg *cfg)
{
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(
			DPNI_CMDID_GET_CONGESTION_NOTIFICATION,
			cmd_flags,
			token);
	DPNI_CMD_GET_CONGESTION_NOTIFICATION(cmd, qtype, tc_id);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	DPNI_RSP_GET_CONGESTION_NOTIFICATION(cmd, cfg);

	return 0;
}

int dpni_get_api_version(struct fsl_mc_io *mc_io,
			 uint32_t cmd_flags,
			   uint16_t *major_ver,
			   uint16_t *minor_ver)
{
	struct mc_command cmd = { 0 };
	int err;

	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_API_VERSION,
					cmd_flags,
					0);

	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	DPNI_RSP_GET_API_VERSION(cmd, *major_ver, *minor_ver);

	return 0;
}

int dpni_set_queue(struct fsl_mc_io *mc_io,
		   uint32_t cmd_flags,
		     uint16_t token,
		   enum dpni_queue_type qtype,
			 uint8_t tc,
			 uint8_t index,
		   uint8_t options,
		     const struct dpni_queue *queue)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_QUEUE,
					  cmd_flags,
					  token);
	DPNI_CMD_SET_QUEUE(cmd, qtype, tc, index, options, queue);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpni_get_queue(struct fsl_mc_io *mc_io,
		   uint32_t cmd_flags,
		     uint16_t token,
		   enum dpni_queue_type qtype,
			 uint8_t tc,
			 uint8_t index,
		   struct dpni_queue *queue,
		   struct dpni_queue_id *qid)
{
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_QUEUE,
					  cmd_flags,
					  token);
	DPNI_CMD_GET_QUEUE(cmd, qtype, tc, index);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	DPNI_RSP_GET_QUEUE(cmd, queue, qid);

	return 0;
}

int dpni_get_statistics(struct fsl_mc_io *mc_io,
			uint32_t cmd_flags,
			uint16_t token,
			uint8_t page,
			union dpni_statistics *stat)
{
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_STATISTICS,
					  cmd_flags,
					  token);
	DPNI_CMD_GET_STATISTICS(cmd, page);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	DPNI_RSP_GET_STATISTICS(cmd, stat);

	return 0;
}

int dpni_reset_statistics(struct fsl_mc_io *mc_io,
			  uint32_t cmd_flags,
		     uint16_t token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_RESET_STATISTICS,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpni_set_taildrop(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		      uint16_t token,
		      enum dpni_congestion_point cg_point,
		      enum dpni_queue_type q_type,
		      uint8_t tc,
		      uint8_t q_index,
		      struct dpni_taildrop *taildrop)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_SET_TAILDROP,
					  cmd_flags,
					  token);
	DPNI_CMD_SET_TAILDROP(cmd, cg_point, q_type, tc, q_index, taildrop);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}

int dpni_get_taildrop(struct fsl_mc_io *mc_io,
		      uint32_t cmd_flags,
		     uint16_t token,
			 enum dpni_congestion_point cg_point,
			 enum dpni_queue_type q_type,
			 uint8_t tc,
			 uint8_t q_index,
			 struct dpni_taildrop *taildrop)
{
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPNI_CMDID_GET_TAILDROP,
					  cmd_flags,
					  token);
	DPNI_CMD_GET_TAILDROP(cmd, cg_point, q_type, tc, q_index);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	DPNI_RSP_GET_TAILDROP(cmd, taildrop);

	return 0;
}
