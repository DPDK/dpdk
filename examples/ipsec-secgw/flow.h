/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */

#ifndef _FLOW_H_
#define _FLOW_H_

#include "parser.h"

void parse_flow_tokens(char **tokens, uint32_t n_tokens,
		       struct parse_status *status);

void flow_init(void);

#endif /* _FLOW_H_ */
