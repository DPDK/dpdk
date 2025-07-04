/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2015-2023 Google, Inc.
 */
#include "gve_version.h"

const char *gve_version_string(void)
{
	static char gve_version[20];
	snprintf(gve_version, sizeof(gve_version), "%s%d.%d.%d",
		GVE_VERSION_PREFIX, GVE_VERSION_MAJOR, GVE_VERSION_MINOR,
		GVE_VERSION_SUB);
	return gve_version;
}
