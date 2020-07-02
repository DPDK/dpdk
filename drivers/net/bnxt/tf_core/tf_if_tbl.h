/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef TF_IF_TBL_TYPE_H_
#define TF_IF_TBL_TYPE_H_

#include "tf_core.h"
#include "stack.h"

/*
 * This is the constant used to define invalid CFA
 * types across all devices.
 */
#define CFA_IF_TBL_TYPE_INVALID 65535

struct tf;

/**
 * The IF Table module provides processing of Internal TF interface table types.
 */

/**
 * IF table configuration enumeration.
 */
enum tf_if_tbl_cfg_type {
	/**
	 * No configuration
	 */
	TF_IF_TBL_CFG_NULL,
	/**
	 * HCAPI 'controlled'
	 */
	TF_IF_TBL_CFG,
};

/**
 * IF table configuration structure, used by the Device to configure
 * how an individual TF type is configured in regard to the HCAPI type.
 */
struct tf_if_tbl_cfg {
	/**
	 * IF table config controls how the DB for that element is
	 * processed.
	 */
	enum tf_if_tbl_cfg_type cfg_type;

	/**
	 * HCAPI Type for the element. Used for TF to HCAPI type
	 * conversion.
	 */
	uint16_t hcapi_type;
};

/**
 * Get HCAPI type parameters for a single element
 */
struct tf_if_tbl_get_hcapi_parms {
	/**
	 * [in] IF Tbl DB Handle
	 */
	void *tbl_db;
	/**
	 * [in] DB Index, indicates which DB entry to perform the
	 * action on.
	 */
	uint16_t db_index;
	/**
	 * [out] Pointer to the hcapi type for the specified db_index
	 */
	uint16_t *hcapi_type;
};

/**
 * Table configuration parameters
 */
struct tf_if_tbl_cfg_parms {
	/**
	 * Number of table types in each of the configuration arrays
	 */
	uint16_t num_elements;
	/**
	 * Table Type element configuration array
	 */
	struct tf_if_tbl_cfg *cfg;
	/**
	 * Shadow table type configuration array
	 */
	struct tf_shadow_if_tbl_cfg *shadow_cfg;
	/**
	 * Boolean controlling the request shadow copy.
	 */
	bool shadow_copy;
};

/**
 * IF Table set parameters
 */
struct tf_if_tbl_set_parms {
	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Type of object to set
	 */
	enum tf_if_tbl_type type;
	/**
	 * [in] Type of HCAPI
	 */
	uint16_t hcapi_type;
	/**
	 * [in] Entry data
	 */
	uint8_t *data;
	/**
	 * [in] Entry size
	 */
	uint16_t data_sz_in_bytes;
	/**
	 * [in] Entry index to write to
	 */
	uint32_t idx;
};

/**
 * IF Table get parameters
 */
struct tf_if_tbl_get_parms {
	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Type of object to get
	 */
	enum tf_if_tbl_type type;
	/**
	 * [in] Type of HCAPI
	 */
	uint16_t hcapi_type;
	/**
	 * [out] Entry data
	 */
	uint8_t *data;
	/**
	 * [out] Entry size
	 */
	uint16_t data_sz_in_bytes;
	/**
	 * [in] Entry index to read
	 */
	uint32_t idx;
};

/**
 * @page if tbl Table
 *
 * @ref tf_if_tbl_bind
 *
 * @ref tf_if_tbl_unbind
 *
 * @ref tf_tbl_set
 *
 * @ref tf_tbl_get
 *
 * @ref tf_tbl_restore
 */
/**
 * Initializes the Table module with the requested DBs. Must be
 * invoked as the first thing before any of the access functions.
 *
 * [in] tfp
 *   Pointer to TF handle, used for HCAPI communication
 *
 * [in] parms
 *   Pointer to Table configuration parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_if_tbl_bind(struct tf *tfp,
		   struct tf_if_tbl_cfg_parms *parms);

/**
 * Cleans up the private DBs and releases all the data.
 *
 * [in] tfp
 *   Pointer to TF handle, used for HCAPI communication
 *
 * [in] parms
 *   Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_if_tbl_unbind(struct tf *tfp);

/**
 * Configures the requested element by sending a firmware request which
 * then installs it into the device internal structures.
 *
 * [in] tfp
 *   Pointer to TF handle, used for HCAPI communication
 *
 * [in] parms
 *   Pointer to Interface Table set parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_if_tbl_set(struct tf *tfp,
		  struct tf_if_tbl_set_parms *parms);

/**
 * Retrieves the requested element by sending a firmware request to get
 * the element.
 *
 * [in] tfp
 *   Pointer to TF handle, used for HCAPI communication
 *
 * [in] parms
 *   Pointer to Table get parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_if_tbl_get(struct tf *tfp,
		  struct tf_if_tbl_get_parms *parms);

#endif /* TF_IF_TBL_TYPE_H */
