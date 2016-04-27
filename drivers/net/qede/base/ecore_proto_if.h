/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#ifndef __ECORE_PROTO_IF_H__
#define __ECORE_PROTO_IF_H__

/*
 * PF parameters (according to personality/protocol)
 */

struct ecore_eth_pf_params {
	/* The following parameters are used during HW-init
	 * and these parameters need to be passed as arguments
	 * to update_pf_params routine invoked before slowpath start
	 */
	u16 num_cons;
};

struct ecore_pf_params {
	struct ecore_eth_pf_params eth_pf_params;
};

#endif
