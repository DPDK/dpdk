/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#include "cnxk_ml_utils.h"

void
cnxk_ml_print_line(FILE *fp, int len)
{
	int i;

	for (i = 0; i < len; i++)
		fprintf(fp, "-");
	fprintf(fp, "\n");
}
