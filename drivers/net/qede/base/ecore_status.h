/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2016 - 2018 Cavium Inc.
 * All rights reserved.
 * www.cavium.com
 */

#ifndef __ECORE_STATUS_H__
#define __ECORE_STATUS_H__

enum _ecore_status_t {
	ECORE_NOENT = -15,
	ECORE_CONN_REFUSED = -14,
	ECORE_CONN_RESET = -13,
	ECORE_UNKNOWN_ERROR  = -12,
	ECORE_NORESOURCES	 = -11,
	ECORE_NODEV   = -10,
	ECORE_ABORTED = -9,
	ECORE_AGAIN   = -8,
	ECORE_NOTIMPL = -7,
	ECORE_EXISTS  = -6,
	ECORE_IO      = -5,
	ECORE_TIMEOUT = -4,
	ECORE_INVAL   = -3,
	ECORE_BUSY    = -2,
	ECORE_NOMEM   = -1,
	ECORE_SUCCESS = 0,
	/* PENDING is not an error and should be positive */
	ECORE_PENDING = 1,
};

#endif /* __ECORE_STATUS_H__ */
