/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include <rte_fib.h>
#include <rte_fib6.h>

#include "l3fwd.h"
#include "l3fwd_event.h"
#include "l3fwd_route.h"

/* Main fib processing loop. */
int
fib_main_loop(__rte_unused void *dummy)
{
	return 0;
}

int __rte_noinline
fib_event_main_loop_tx_d(__rte_unused void *dummy)
{
	return 0;
}

int __rte_noinline
fib_event_main_loop_tx_d_burst(__rte_unused void *dummy)
{
	return 0;
}

int __rte_noinline
fib_event_main_loop_tx_q(__rte_unused void *dummy)
{
	return 0;
}

int __rte_noinline
fib_event_main_loop_tx_q_burst(__rte_unused void *dummy)
{
	return 0;
}

/* Function to setup fib. */
void
setup_fib(__rte_unused const int socketid)
{}

/* Return ipv4 fib lookup struct. */
void *
fib_get_ipv4_l3fwd_lookup_struct(__rte_unused const int socketid)
{
	return 0;
}

/* Return ipv6 fib lookup struct. */
void *
fib_get_ipv6_l3fwd_lookup_struct(__rte_unused const int socketid)
{
	return 0;
}
