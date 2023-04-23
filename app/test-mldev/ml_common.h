/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#ifndef ML_COMMON_H
#define ML_COMMON_H

#include <stdio.h>

#define CLNRM "\x1b[0m"
#define CLRED "\x1b[31m"
#define CLGRN "\x1b[32m"
#define CLYEL "\x1b[33m"

#define ML_STR_FMT 20

#define ml_err(fmt, args...) fprintf(stderr, CLRED "error: %s() " fmt CLNRM "\n", __func__, ##args)

#define ml_info(fmt, args...) fprintf(stdout, CLYEL "" fmt CLNRM "\n", ##args)

#define ml_dump(str, fmt, val...) printf("\t%-*s : " fmt "\n", ML_STR_FMT, str, ##val)

#define ml_dump_begin(str) printf("\t%-*s :\n\t{\n", ML_STR_FMT, str)

#define ml_dump_list(str, id, val) printf("\t%*s[%2u] : %s\n", ML_STR_FMT - 4, str, id, val)

#define ml_dump_end printf("\b\t}\n\n")

static inline void
ml_print_line(uint16_t len)
{
	uint16_t i;

	for (i = 0; i < len; i++)
		printf("-");

	printf("\n");
}

#endif /* ML_COMMON_H */
