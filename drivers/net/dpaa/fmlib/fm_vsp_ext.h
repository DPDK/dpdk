/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2012 Freescale Semiconductor, Inc
 * Copyright 2019-2020 NXP
 */

/*
 * @File          fm_vsp_ext.h
 *
 * @Description   FM Virtual Storage-Profile
 */
#ifndef __FM_VSP_EXT_H
#define __FM_VSP_EXT_H
#include "ncsw_ext.h"
#include "fm_ext.h"
#include "net_ext.h"

typedef struct t_fm_vsp_params {
	t_handle	h_fm;
			/**< A handle to the FM object this VSP related to */
	t_fm_ext_pools	ext_buf_pools;
			/**< Which external buffer pools are used (up to
			 * FM_PORT_MAX_NUM_OF_EXT_POOLS), and their sizes.
			 * Parameter associated with Rx / OP port
			 */
	uint16_t	liodn_offset;	/**< VSP's LIODN offset */
	struct {
		e_fm_port_type	port_type; /**< Port type */
		uint8_t	port_id;           /**< Port Id - relative to type */
	} port_params;
	uint8_t	relative_profile_id;
			/**< VSP Id - relative to VSP's range defined in
			 * relevant FM object
			 */
} t_fm_vsp_params;

typedef struct ioc_fm_vsp_params_t {
	struct t_fm_vsp_params vsp_params;
	void		*id;		/**< return value */
} ioc_fm_vsp_params_t;

typedef struct t_fm_port_vspalloc_params {
	uint8_t     num_of_profiles;
		/**< Number of Virtual Storage Profiles; must be a power of 2 */
	uint8_t     dflt_relative_id;
	/**< The default Virtual-Storage-Profile-id dedicated to Rx/OP port. The
	 * same default Virtual-Storage-Profile-id will be for coupled Tx port
	 * if relevant function called for Rx port
	 */
} t_fm_port_vspalloc_params;

typedef struct ioc_fm_port_vsp_alloc_params_t {
	struct t_fm_port_vspalloc_params params;
	void	*p_fm_tx_port;
	/**< Handle to coupled Tx Port; not relevant for OP port. */
} ioc_fm_port_vsp_alloc_params_t;

typedef struct ioc_fm_buffer_prefix_content_t {
	uint16_t priv_data_size;
		/**< Number of bytes to be left at the beginning of the external
		 * buffer; Note that the private-area will start from the base
		 * of the buffer address.
		 */
	bool pass_prs_result;
			/**< TRUE to pass the parse result to/from the FM; User
			 * may use fm_port_get_buffer_prs_result() in order to
			 * get the parser-result from a buffer.
			 */
	bool pass_time_stamp;
			/**< TRUE to pass the timeStamp to/from the FM User may
			 * use fm_port_get_buffer_time_stamp() in order to get
			 * the parser-result from a buffer.
			 */
	bool pass_hash_result;
			/**< TRUE to pass the KG hash result to/from the FM User
			 * may use fm_port_get_buffer_hash_result() in order to
			 * get the parser-result from a buffer.
			 */
	bool pass_all_other_pcd_info;
			/**< Add all other Internal-Context information: AD,
			 * hash-result, key, etc.
			 */
	uint16_t data_align;
			/**< 0 to use driver's default alignment [64],
			 * other value for selecting a data alignment (must be a
			 * power of 2); if write optimization is used, must be
			 * >= 16.
			 */
	uint8_t manip_extra_space;
			/**< Maximum extra size needed
			 * (insertion-size minus removal-size);
			 * Note that this field impacts the size of the
			 * buffer-prefix (i.e. it pushes the data offset);
			 * This field is irrelevant if DPAA_VERSION==10
			 */
} ioc_fm_buffer_prefix_content_t;

typedef struct ioc_fm_buffer_prefix_content_params_t {
	void    *p_fm_vsp;
	ioc_fm_buffer_prefix_content_t fm_buffer_prefix_content;
} ioc_fm_buffer_prefix_content_params_t;

uint32_t fm_port_vsp_alloc(t_handle h_fm_port,
			  t_fm_port_vspalloc_params *p_params);

t_handle fm_vsp_config(t_fm_vsp_params *p_fm_vsp_params);

uint32_t fm_vsp_init(t_handle h_fm_vsp);

uint32_t fm_vsp_free(t_handle h_fm_vsp);

uint32_t fm_vsp_config_buffer_prefix_content(t_handle h_fm_vsp,
		t_fm_buffer_prefix_content *p_fm_buffer_prefix_content);

#define FM_PORT_IOC_VSP_ALLOC \
	_IOW(FM_IOC_TYPE_BASE, FM_PORT_IOC_NUM(38), \
	ioc_fm_port_vsp_alloc_params_t)

#define FM_IOC_VSP_CONFIG \
	_IOWR(FM_IOC_TYPE_BASE, FM_IOC_NUM(8), ioc_fm_vsp_params_t)

#define FM_IOC_VSP_INIT	\
	_IOW(FM_IOC_TYPE_BASE, FM_IOC_NUM(9), ioc_fm_obj_t)

#define FM_IOC_VSP_FREE	\
	_IOW(FM_IOC_TYPE_BASE, FM_IOC_NUM(10), ioc_fm_obj_t)

#define FM_IOC_VSP_CONFIG_BUFFER_PREFIX_CONTENT \
	_IOW(FM_IOC_TYPE_BASE, FM_IOC_NUM(12), \
	ioc_fm_buffer_prefix_content_params_t)

#endif /* __FM_VSP_EXT_H */
