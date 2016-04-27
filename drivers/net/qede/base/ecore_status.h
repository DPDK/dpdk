/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#ifndef __ECORE_STATUS_H__
#define __ECORE_STATUS_H__

enum _ecore_status_t {
	ECORE_UNKNOWN_ERROR = -12,
	ECORE_NORESOURCES = -11,
	ECORE_NODEV = -10,
	ECORE_ABORTED = -9,
	ECORE_AGAIN = -8,
	ECORE_NOTIMPL = -7,
	ECORE_EXISTS = -6,
	ECORE_IO = -5,
	ECORE_TIMEOUT = -4,
	ECORE_INVAL = -3,
	ECORE_BUSY = -2,
	ECORE_NOMEM = -1,
	ECORE_SUCCESS = 0,
	/* PENDING is not an error and should be positive */
	ECORE_PENDING = 1,
};

#endif /* __ECORE_STATUS_H__ */
