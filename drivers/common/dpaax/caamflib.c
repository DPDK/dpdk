/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2020 NXP
 *
 */

#include <rta.h>

/*
 * SEC HW block revision.
 *
 * This *must not be confused with SEC version*:
 * - SEC HW block revision format is "v"
 * - SEC revision format is "x.y"
 */
enum rta_sec_era rta_sec_era;
