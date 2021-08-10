/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2021 Intel Corporation
 */

#ifndef _ICE_PTP_CONSTS_H_
#define _ICE_PTP_CONSTS_H_

/* Constant definitions related to the hardware clock used for PTP 1588
 * features and functionality.
 */
/* Constants defined for the PTP 1588 clock hardware. */

/*
 * struct ice_time_ref_info_e822
 *
 * E822 hardware can use different sources as the reference for the PTP
 * hardware clock. Each clock has different characteristics such as a slightly
 * different frequency, etc.
 *
 * This lookup table defines several constants that depend on the current time
 * reference. See the struct ice_time_ref_info_e822 for information about the
 * meaning of each constant.
 */
const struct ice_time_ref_info_e822 e822_time_ref[NUM_ICE_TIME_REF_FREQ] = {
	/* ICE_TIME_REF_FREQ_25_000 -> 25 MHz */
	{
		/* pll_freq */
		823437500, /* 823.4375 MHz PLL */
		/* nominal_incval */
		0x136e44fabULL,
		/* pps_delay */
		11,
	},

	/* ICE_TIME_REF_FREQ_122_880 -> 122.88 MHz */
	{
		/* pll_freq */
		783360000, /* 783.36 MHz */
		/* nominal_incval */
		0x146cc2177ULL,
		/* pps_delay */
		12,
	},

	/* ICE_TIME_REF_FREQ_125_000 -> 125 MHz */
	{
		/* pll_freq */
		796875000, /* 796.875 MHz */
		/* nominal_incval */
		0x141414141ULL,
		/* pps_delay */
		12,
	},

	/* ICE_TIME_REF_FREQ_153_600 -> 153.6 MHz */
	{
		/* pll_freq */
		816000000, /* 816 MHz */
		/* nominal_incval */
		0x139b9b9baULL,
		/* pps_delay */
		12,
	},

	/* ICE_TIME_REF_FREQ_156_250 -> 156.25 MHz */
	{
		/* pll_freq */
		830078125, /* 830.78125 MHz */
		/* nominal_incval */
		0x134679aceULL,
		/* pps_delay */
		11,
	},

	/* ICE_TIME_REF_FREQ_245_760 -> 245.76 MHz */
	{
		/* pll_freq */
		783360000, /* 783.36 MHz */
		/* nominal_incval */
		0x146cc2177ULL,
		/* pps_delay */
		12,
	},
};

#endif /* _ICE_PTP_CONSTS_H_ */
