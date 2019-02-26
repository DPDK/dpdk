/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef TEST_FLOW_CLASSIFY_H_
#define TEST_FLOW_CLASSIFY_H_

/* ACL field definitions for IPv4 5 tuple rule */

enum {
	PROTO_FIELD_IPV4,
	SRC_FIELD_IPV4,
	DST_FIELD_IPV4,
	SRCP_FIELD_IPV4,
	DSTP_FIELD_IPV4,
	NUM_FIELDS_IPV4
};

enum {
	PROTO_INPUT_IPV4,
	SRC_INPUT_IPV4,
	DST_INPUT_IPV4,
	SRCP_DESTP_INPUT_IPV4
};

#endif /* TEST_FLOW_CLASSIFY_H_ */
