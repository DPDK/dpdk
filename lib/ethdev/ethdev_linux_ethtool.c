/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2024 NVIDIA Corporation & Affiliates
 */

#include <eal_export.h>
#include <rte_bitops.h>

#include "rte_ethdev.h"
#include "ethdev_linux_ethtool.h"

/* Link modes sorted with index as defined in ethtool.
 * Values are speed in Mbps with LSB indicating duplex.
 *
 * The ethtool bits definition should not change as it is a kernel API.
 * Using raw numbers directly avoids checking API availability
 * and allows to compile with new bits included even on an old kernel.
 *
 * The array below is built from bit definitions with this shell command:
 *   sed -rn \
 *     's;.*(ETHTOOL_LINK_MODE_)([0-9]+)([0-9a-zA-Z_]*).*= *([0-9]*).*;[\4] \2 \1\2\3;p' \
 *     /usr/include/linux/ethtool.h |
 *   awk '/_Half_/{$2=$2+1}1' |
 *   awk '{printf "\t%5s = %7s, /\* %s *\/\n", $1, $2, $3}'
 */
static const uint32_t link_modes[] = {
	  [0] =      11, /* ETHTOOL_LINK_MODE_10baseT_Half_BIT */
	  [1] =      10, /* ETHTOOL_LINK_MODE_10baseT_Full_BIT */
	  [2] =     101, /* ETHTOOL_LINK_MODE_100baseT_Half_BIT */
	  [3] =     100, /* ETHTOOL_LINK_MODE_100baseT_Full_BIT */
	  [4] =    1001, /* ETHTOOL_LINK_MODE_1000baseT_Half_BIT */
	  [5] =    1000, /* ETHTOOL_LINK_MODE_1000baseT_Full_BIT */
	 [12] =   10000, /* ETHTOOL_LINK_MODE_10000baseT_Full_BIT */
	 [15] =    2500, /* ETHTOOL_LINK_MODE_2500baseX_Full_BIT */
	 [17] =    1000, /* ETHTOOL_LINK_MODE_1000baseKX_Full_BIT */
	 [18] =   10000, /* ETHTOOL_LINK_MODE_10000baseKX4_Full_BIT */
	 [19] =   10000, /* ETHTOOL_LINK_MODE_10000baseKR_Full_BIT */
	 [20] =   10000, /* ETHTOOL_LINK_MODE_10000baseR_FEC_BIT */
	 [21] =   20000, /* ETHTOOL_LINK_MODE_20000baseMLD2_Full_BIT */
	 [22] =   20000, /* ETHTOOL_LINK_MODE_20000baseKR2_Full_BIT */
	 [23] =   40000, /* ETHTOOL_LINK_MODE_40000baseKR4_Full_BIT */
	 [24] =   40000, /* ETHTOOL_LINK_MODE_40000baseCR4_Full_BIT */
	 [25] =   40000, /* ETHTOOL_LINK_MODE_40000baseSR4_Full_BIT */
	 [26] =   40000, /* ETHTOOL_LINK_MODE_40000baseLR4_Full_BIT */
	 [27] =   56000, /* ETHTOOL_LINK_MODE_56000baseKR4_Full_BIT */
	 [28] =   56000, /* ETHTOOL_LINK_MODE_56000baseCR4_Full_BIT */
	 [29] =   56000, /* ETHTOOL_LINK_MODE_56000baseSR4_Full_BIT */
	 [30] =   56000, /* ETHTOOL_LINK_MODE_56000baseLR4_Full_BIT */
	 [31] =   25000, /* ETHTOOL_LINK_MODE_25000baseCR_Full_BIT */
	 [32] =   25000, /* ETHTOOL_LINK_MODE_25000baseKR_Full_BIT */
	 [33] =   25000, /* ETHTOOL_LINK_MODE_25000baseSR_Full_BIT */
	 [34] =   50000, /* ETHTOOL_LINK_MODE_50000baseCR2_Full_BIT */
	 [35] =   50000, /* ETHTOOL_LINK_MODE_50000baseKR2_Full_BIT */
	 [36] =  100000, /* ETHTOOL_LINK_MODE_100000baseKR4_Full_BIT */
	 [37] =  100000, /* ETHTOOL_LINK_MODE_100000baseSR4_Full_BIT */
	 [38] =  100000, /* ETHTOOL_LINK_MODE_100000baseCR4_Full_BIT */
	 [39] =  100000, /* ETHTOOL_LINK_MODE_100000baseLR4_ER4_Full_BIT */
	 [40] =   50000, /* ETHTOOL_LINK_MODE_50000baseSR2_Full_BIT */
	 [41] =    1000, /* ETHTOOL_LINK_MODE_1000baseX_Full_BIT */
	 [42] =   10000, /* ETHTOOL_LINK_MODE_10000baseCR_Full_BIT */
	 [43] =   10000, /* ETHTOOL_LINK_MODE_10000baseSR_Full_BIT */
	 [44] =   10000, /* ETHTOOL_LINK_MODE_10000baseLR_Full_BIT */
	 [45] =   10000, /* ETHTOOL_LINK_MODE_10000baseLRM_Full_BIT */
	 [46] =   10000, /* ETHTOOL_LINK_MODE_10000baseER_Full_BIT */
	 [47] =    2500, /* ETHTOOL_LINK_MODE_2500baseT_Full_BIT */
	 [48] =    5000, /* ETHTOOL_LINK_MODE_5000baseT_Full_BIT */
	 [52] =   50000, /* ETHTOOL_LINK_MODE_50000baseKR_Full_BIT */
	 [53] =   50000, /* ETHTOOL_LINK_MODE_50000baseSR_Full_BIT */
	 [54] =   50000, /* ETHTOOL_LINK_MODE_50000baseCR_Full_BIT */
	 [55] =   50000, /* ETHTOOL_LINK_MODE_50000baseLR_ER_FR_Full_BIT */
	 [56] =   50000, /* ETHTOOL_LINK_MODE_50000baseDR_Full_BIT */
	 [57] =  100000, /* ETHTOOL_LINK_MODE_100000baseKR2_Full_BIT */
	 [58] =  100000, /* ETHTOOL_LINK_MODE_100000baseSR2_Full_BIT */
	 [59] =  100000, /* ETHTOOL_LINK_MODE_100000baseCR2_Full_BIT */
	 [60] =  100000, /* ETHTOOL_LINK_MODE_100000baseLR2_ER2_FR2_Full_BIT */
	 [61] =  100000, /* ETHTOOL_LINK_MODE_100000baseDR2_Full_BIT */
	 [62] =  200000, /* ETHTOOL_LINK_MODE_200000baseKR4_Full_BIT */
	 [63] =  200000, /* ETHTOOL_LINK_MODE_200000baseSR4_Full_BIT */
	 [64] =  200000, /* ETHTOOL_LINK_MODE_200000baseLR4_ER4_FR4_Full_BIT */
	 [65] =  200000, /* ETHTOOL_LINK_MODE_200000baseDR4_Full_BIT */
	 [66] =  200000, /* ETHTOOL_LINK_MODE_200000baseCR4_Full_BIT */
	 [67] =     100, /* ETHTOOL_LINK_MODE_100baseT1_Full_BIT */
	 [68] =    1000, /* ETHTOOL_LINK_MODE_1000baseT1_Full_BIT */
	 [69] =  400000, /* ETHTOOL_LINK_MODE_400000baseKR8_Full_BIT */
	 [70] =  400000, /* ETHTOOL_LINK_MODE_400000baseSR8_Full_BIT */
	 [71] =  400000, /* ETHTOOL_LINK_MODE_400000baseLR8_ER8_FR8_Full_BIT */
	 [72] =  400000, /* ETHTOOL_LINK_MODE_400000baseDR8_Full_BIT */
	 [73] =  400000, /* ETHTOOL_LINK_MODE_400000baseCR8_Full_BIT */
	 [75] =  100000, /* ETHTOOL_LINK_MODE_100000baseKR_Full_BIT */
	 [76] =  100000, /* ETHTOOL_LINK_MODE_100000baseSR_Full_BIT */
	 [77] =  100000, /* ETHTOOL_LINK_MODE_100000baseLR_ER_FR_Full_BIT */
	 [78] =  100000, /* ETHTOOL_LINK_MODE_100000baseCR_Full_BIT */
	 [79] =  100000, /* ETHTOOL_LINK_MODE_100000baseDR_Full_BIT */
	 [80] =  200000, /* ETHTOOL_LINK_MODE_200000baseKR2_Full_BIT */
	 [81] =  200000, /* ETHTOOL_LINK_MODE_200000baseSR2_Full_BIT */
	 [82] =  200000, /* ETHTOOL_LINK_MODE_200000baseLR2_ER2_FR2_Full_BIT */
	 [83] =  200000, /* ETHTOOL_LINK_MODE_200000baseDR2_Full_BIT */
	 [84] =  200000, /* ETHTOOL_LINK_MODE_200000baseCR2_Full_BIT */
	 [85] =  400000, /* ETHTOOL_LINK_MODE_400000baseKR4_Full_BIT */
	 [86] =  400000, /* ETHTOOL_LINK_MODE_400000baseSR4_Full_BIT */
	 [87] =  400000, /* ETHTOOL_LINK_MODE_400000baseLR4_ER4_FR4_Full_BIT */
	 [88] =  400000, /* ETHTOOL_LINK_MODE_400000baseDR4_Full_BIT */
	 [89] =  400000, /* ETHTOOL_LINK_MODE_400000baseCR4_Full_BIT */
	 [90] =     101, /* ETHTOOL_LINK_MODE_100baseFX_Half_BIT */
	 [91] =     100, /* ETHTOOL_LINK_MODE_100baseFX_Full_BIT */
	 [92] =      10, /* ETHTOOL_LINK_MODE_10baseT1L_Full_BIT */
	 [93] =  800000, /* ETHTOOL_LINK_MODE_800000baseCR8_Full_BIT */
	 [94] =  800000, /* ETHTOOL_LINK_MODE_800000baseKR8_Full_BIT */
	 [95] =  800000, /* ETHTOOL_LINK_MODE_800000baseDR8_Full_BIT */
	 [96] =  800000, /* ETHTOOL_LINK_MODE_800000baseDR8_2_Full_BIT */
	 [97] =  800000, /* ETHTOOL_LINK_MODE_800000baseSR8_Full_BIT */
	 [98] =  800000, /* ETHTOOL_LINK_MODE_800000baseVR8_Full_BIT */
	 [99] =      10, /* ETHTOOL_LINK_MODE_10baseT1S_Full_BIT */
	[100] =      11, /* ETHTOOL_LINK_MODE_10baseT1S_Half_BIT */
	[101] =      11, /* ETHTOOL_LINK_MODE_10baseT1S_P2MP_Half_BIT */
	[102] =      10, /* ETHTOOL_LINK_MODE_10baseT1BRR_Full_BIT */
	[103] =  200000, /* ETHTOOL_LINK_MODE_200000baseCR_Full_BIT */
	[104] =  200000, /* ETHTOOL_LINK_MODE_200000baseKR_Full_BIT */
	[105] =  200000, /* ETHTOOL_LINK_MODE_200000baseDR_Full_BIT */
	[106] =  200000, /* ETHTOOL_LINK_MODE_200000baseDR_2_Full_BIT */
	[107] =  200000, /* ETHTOOL_LINK_MODE_200000baseSR_Full_BIT */
	[108] =  200000, /* ETHTOOL_LINK_MODE_200000baseVR_Full_BIT */
	[109] =  400000, /* ETHTOOL_LINK_MODE_400000baseCR2_Full_BIT */
	[110] =  400000, /* ETHTOOL_LINK_MODE_400000baseKR2_Full_BIT */
	[111] =  400000, /* ETHTOOL_LINK_MODE_400000baseDR2_Full_BIT */
	[112] =  400000, /* ETHTOOL_LINK_MODE_400000baseDR2_2_Full_BIT */
	[113] =  400000, /* ETHTOOL_LINK_MODE_400000baseSR2_Full_BIT */
	[114] =  400000, /* ETHTOOL_LINK_MODE_400000baseVR2_Full_BIT */
	[115] =  800000, /* ETHTOOL_LINK_MODE_800000baseCR4_Full_BIT */
	[116] =  800000, /* ETHTOOL_LINK_MODE_800000baseKR4_Full_BIT */
	[117] =  800000, /* ETHTOOL_LINK_MODE_800000baseDR4_Full_BIT */
	[118] =  800000, /* ETHTOOL_LINK_MODE_800000baseDR4_2_Full_BIT */
	[119] =  800000, /* ETHTOOL_LINK_MODE_800000baseSR4_Full_BIT */
	[120] =  800000, /* ETHTOOL_LINK_MODE_800000baseVR4_Full_BIT */
};

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_link_speed_ethtool)
uint32_t
rte_eth_link_speed_ethtool(enum ethtool_link_mode_bit_indices bit)
{
	uint32_t speed;
	int duplex;

	/* get mode from array */
	if (bit >= RTE_DIM(link_modes))
		return RTE_ETH_LINK_SPEED_AUTONEG;
	speed = link_modes[bit];
	if (speed == 0)
		return RTE_ETH_LINK_SPEED_AUTONEG;
	RTE_BUILD_BUG_ON(RTE_ETH_LINK_SPEED_AUTONEG != 0);

	/* duplex is LSB */
	duplex = (speed & 1) ?
			RTE_ETH_LINK_HALF_DUPLEX :
			RTE_ETH_LINK_FULL_DUPLEX;
	speed &= RTE_GENMASK32(31, 1);

	return rte_eth_speed_bitflag(speed, duplex);
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_link_speed_glink)
uint32_t
rte_eth_link_speed_glink(const uint32_t *bitmap, int8_t nwords)
{
	uint8_t word, bit;
	uint32_t ethdev_bitmap = 0;

	if (nwords < 1)
		return 0;

	for (word = 0; word < nwords; word++) {
		for (bit = 0; bit < 32; bit++) {
			if ((bitmap[word] & RTE_BIT32(bit)) == 0)
				continue;
			ethdev_bitmap |= rte_eth_link_speed_ethtool(word * 32 + bit);
		}
	}

	return ethdev_bitmap;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_link_speed_gset)
uint32_t
rte_eth_link_speed_gset(uint32_t legacy_bitmap)
{
	return rte_eth_link_speed_glink(&legacy_bitmap, 1);
}
