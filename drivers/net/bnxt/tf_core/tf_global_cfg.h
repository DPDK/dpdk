/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef TF_GLOBAL_CFG_H_
#define TF_GLOBAL_CFG_H_

#include "tf_core.h"
#include "stack.h"

/**
 * The global cfg module provides processing of global cfg types.
 */

/* struct tf; */

/* Internal type not available to user
 * but available internally within Truflow
 */
enum tf_global_config_internal_type {
	TF_GLOBAL_CFG_INTERNAL_PARIF_2_PF = TF_GLOBAL_CFG_TYPE_MAX,
	TF_GLOBAL_CFG_INTERNAL_TYPE_MAX
};

/**
 * Global cfg configuration enumeration.
 */
enum tf_global_cfg_cfg_type {
	/**
	 * No configuration
	 */
	TF_GLOBAL_CFG_CFG_NULL,
	/**
	 * HCAPI 'controlled'
	 */
	TF_GLOBAL_CFG_CFG_HCAPI,
};

/**
 * Global cfg configuration structure, used by the Device to configure
 * how an individual global cfg type is configured in regard to the HCAPI type.
 */
struct tf_global_cfg_cfg {
	/**
	 * Global cfg config controls how the DB for that element is
	 * processed.
	 */
	enum tf_global_cfg_cfg_type cfg_type;

	/**
	 * HCAPI Type for the element. Used for TF to HCAPI type
	 * conversion.
	 */
	uint16_t hcapi_type;
};

/**
 * Global Cfg configuration parameters
 */
struct tf_global_cfg_cfg_parms {
	/**
	 * Number of table types in the configuration array
	 */
	uint16_t num_elements;
	/**
	 * Table Type element configuration array
	 */
	struct tf_global_cfg_cfg *cfg;
};

/**
 * @page global cfg
 *
 * @ref tf_global_cfg_bind
 *
 * @ref tf_global_cfg_unbind
 *
 * @ref tf_global_cfg_set
 *
 * @ref tf_global_cfg_get
 *
 */
/**
 * Initializes the Global Cfg module with the requested DBs. Must be
 * invoked as the first thing before any of the access functions.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] parms
 *   Pointer to Global Cfg configuration parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int
tf_global_cfg_bind(struct tf *tfp,
		   struct tf_global_cfg_cfg_parms *parms);

/**
 * Cleans up the private DBs and releases all the data.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] parms
 *   Pointer to Global Cfg configuration parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int
tf_global_cfg_unbind(struct tf *tfp);

/**
 * Updates the global configuration table
 *
 * [in] tfp
 *   Pointer to TF handle, used for HCAPI communication
 *
 * [in] parms
 *   Pointer to global cfg parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_global_cfg_set(struct tf *tfp,
		      struct tf_global_cfg_parms *parms);

/**
 * Get global configuration
 *
 * [in] tfp
 *   Pointer to TF handle, used for HCAPI communication
 *
 * [in] parms
 *   Pointer to global cfg parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_global_cfg_get(struct tf *tfp,
		      struct tf_global_cfg_parms *parms);

#endif /* TF_GLOBAL_CFG_H */
