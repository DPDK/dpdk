/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
 */

/**
 * @file
 * getline compat.
 *
 * This module provides getline() and getdelim().
 */

#pragma once

#include <stdio.h>

#ifndef ssize_t
#define ssize_t ptrdiff_t
#endif

ssize_t
getdelim(char **buf, size_t *bufsiz, int delimiter, FILE *fp);

ssize_t
getline(char **buf, size_t *bufsiz, FILE *fp);
