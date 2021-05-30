/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef _TF_MSG_COMMON_H_
#define _TF_MSG_COMMON_H_

/* Communication Mailboxes */
#define TF_CHIMP_MB 0
#define TF_KONG_MB  1

/* Helper to fill in the parms structure */
#define MSG_PREP(parms, mb, type, subtype, req, resp) do {	\
		parms.mailbox = mb;				\
		parms.tf_type = type;				\
		parms.tf_subtype = subtype;			\
		parms.req_size = sizeof(req);			\
		parms.req_data = (uint32_t *)&(req);		\
		parms.resp_size = sizeof(resp);			\
		parms.resp_data = (uint32_t *)&(resp);		\
	} while (0)

#define MSG_PREP_NO_REQ(parms, mb, type, subtype, resp) do {	\
		parms.mailbox = mb;				\
		parms.tf_type = type;				\
		parms.tf_subtype = subtype;			\
		parms.req_size  = 0;				\
		parms.req_data  = NULL;				\
		parms.resp_size = sizeof(resp);			\
		parms.resp_data = (uint32_t *)&(resp);		\
	} while (0)

#define MSG_PREP_NO_RESP(parms, mb, type, subtype, req) do {	\
		parms.mailbox = mb;				\
		parms.tf_type = type;				\
		parms.tf_subtype = subtype;			\
		parms.req_size = sizeof(req);			\
		parms.req_data = (uint32_t *)&(req);		\
		parms.resp_size = 0;				\
		parms.resp_data = NULL;				\
	} while (0)

#endif /* _TF_MSG_COMMON_H_ */
