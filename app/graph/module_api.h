/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#ifndef APP_GRAPH_MODULE_API_H
#define APP_GRAPH_MODULE_API_H

#include <stdint.h>
#include <stdbool.h>

#include "cli.h"
#include "conn.h"
#include "utils.h"
/*
 * Externs
 */
extern volatile bool force_quit;

bool app_graph_exit(void);
#endif
