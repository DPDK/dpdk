/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2013-2015 Freescale Semiconductor Inc.
 * Copyright 2017 NXP
 *
 */
#ifndef __FSL_DPMNG_H
#define __FSL_DPMNG_H

/*
 * Management Complex General API
 * Contains general API for the Management Complex firmware
 */

struct fsl_mc_io;

/**
 * Management Complex firmware version information
 */
#define MC_VER_MAJOR 10
#define MC_VER_MINOR 3

/**
 * struct mc_version
 * @major: Major version number: incremented on API compatibility changes
 * @minor: Minor version number: incremented on API additions (that are
 *		backward compatible); reset when major version is incremented
 * @revision: Internal revision number: incremented on implementation changes
 *		and/or bug fixes that have no impact on API
 */
struct mc_version {
	uint32_t major;
	uint32_t minor;
	uint32_t revision;
};

int mc_get_version(struct fsl_mc_io *mc_io,
		   uint32_t cmd_flags,
		   struct mc_version *mc_ver_info);

/**
 * struct mc_platform
 * @svr:	System version (content of platform SVR register)
 * @pvr:	Processor version (content of platform PVR register)
 */
struct mc_soc_version {
	uint32_t svr;
	uint32_t pvr;
};

int mc_get_soc_version(struct fsl_mc_io *mc_io,
		       uint32_t cmd_flags,
		       struct mc_soc_version *mc_platform_info);
#endif /* __FSL_DPMNG_H */
