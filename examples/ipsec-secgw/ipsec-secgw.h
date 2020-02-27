/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */
#ifndef _IPSEC_SECGW_H_
#define _IPSEC_SECGW_H_

#include <stdbool.h>

#define NB_SOCKETS 4

/* Port mask to identify the unprotected ports */
extern uint32_t unprotected_port_mask;

/* Index of SA in single mode */
extern uint32_t single_sa_idx;

extern volatile bool force_quit;

static inline uint8_t
is_unprotected_port(uint16_t port_id)
{
	return unprotected_port_mask & (1 << port_id);
}

#endif /* _IPSEC_SECGW_H_ */
