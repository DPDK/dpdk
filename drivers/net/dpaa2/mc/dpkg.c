/*-
 * This file is provided under a dual BSD/GPLv2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 *   BSD LICENSE
 *
 * Copyright 2017 NXP
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
#include <fsl_dpkg.h>

/**
 * dpkg_prepare_key_cfg() - function prepare extract parameters
 * @cfg: defining a full Key Generation profile (rule)
 * @key_cfg_buf: Zeroed 256 bytes of memory before mapping it to DMA
 *
 * This function has to be called before the following functions:
 *	- dpni_set_rx_tc_dist()
 *	- dpni_set_qos_table()
 *	- dpkg_prepare_key_cfg()
 */
int
dpkg_prepare_key_cfg(const struct dpkg_profile_cfg *cfg, uint8_t *key_cfg_buf)
{
	int i, j;
	struct dpni_ext_set_rx_tc_dist *dpni_ext;
	struct dpni_dist_extract *extr;

	if (cfg->num_extracts > DPKG_MAX_NUM_OF_EXTRACTS)
		return -EINVAL;

	dpni_ext = (struct dpni_ext_set_rx_tc_dist *)key_cfg_buf;
	dpni_ext->num_extracts = cfg->num_extracts;

	for (i = 0; i < cfg->num_extracts; i++) {
		extr = &dpni_ext->extracts[i];

		switch (cfg->extracts[i].type) {
		case DPKG_EXTRACT_FROM_HDR:
			extr->prot = cfg->extracts[i].extract.from_hdr.prot;
			dpkg_set_field(extr->efh_type, EFH_TYPE,
				       cfg->extracts[i].extract.from_hdr.type);
			extr->size = cfg->extracts[i].extract.from_hdr.size;
			extr->offset = cfg->extracts[i].extract.from_hdr.offset;
			extr->field = cpu_to_le32(
				cfg->extracts[i].extract.from_hdr.field);
			extr->hdr_index =
				cfg->extracts[i].extract.from_hdr.hdr_index;
			break;
		case DPKG_EXTRACT_FROM_DATA:
			extr->size = cfg->extracts[i].extract.from_data.size;
			extr->offset =
				cfg->extracts[i].extract.from_data.offset;
			break;
		case DPKG_EXTRACT_FROM_PARSE:
			extr->size = cfg->extracts[i].extract.from_parse.size;
			extr->offset =
				cfg->extracts[i].extract.from_parse.offset;
			break;
		default:
			return -EINVAL;
		}

		extr->num_of_byte_masks = cfg->extracts[i].num_of_byte_masks;
		dpkg_set_field(extr->extract_type, EXTRACT_TYPE,
			       cfg->extracts[i].type);

		for (j = 0; j < DPKG_NUM_OF_MASKS; j++) {
			extr->masks[j].mask = cfg->extracts[i].masks[j].mask;
			extr->masks[j].offset =
				cfg->extracts[i].masks[j].offset;
		}
	}

	return 0;
}
