/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 */

#ifndef _NGBE_TYPE_DUMMY_H_
#define _NGBE_TYPE_DUMMY_H_

#ifdef TUP
#elif defined(__GNUC__)
#define TUP(x) x##_unused ngbe_unused
#elif defined(__LCLINT__)
#define TUP(x) x /*@unused@*/
#else
#define TUP(x) x
#endif /*TUP*/
#define TUP0 TUP(p0)
#define TUP1 TUP(p1)
#define TUP2 TUP(p2)
#define TUP3 TUP(p3)
#define TUP4 TUP(p4)
#define TUP5 TUP(p5)
#define TUP6 TUP(p6)
#define TUP7 TUP(p7)
#define TUP8 TUP(p8)
#define TUP9 TUP(p9)

/* struct ngbe_bus_operations */
static inline void ngbe_bus_set_lan_id_dummy(struct ngbe_hw *TUP0)
{
}
/* struct ngbe_rom_operations */
static inline s32 ngbe_rom_init_params_dummy(struct ngbe_hw *TUP0)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_rom_validate_checksum_dummy(struct ngbe_hw *TUP0,
					u16 *TUP1)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline s32 ngbe_mac_acquire_swfw_sync_dummy(struct ngbe_hw *TUP0,
					u32 TUP1)
{
	return NGBE_ERR_OPS_DUMMY;
}
static inline void ngbe_mac_release_swfw_sync_dummy(struct ngbe_hw *TUP0,
					u32 TUP1)
{
}
static inline void ngbe_init_ops_dummy(struct ngbe_hw *hw)
{
	hw->bus.set_lan_id = ngbe_bus_set_lan_id_dummy;
	hw->rom.init_params = ngbe_rom_init_params_dummy;
	hw->rom.validate_checksum = ngbe_rom_validate_checksum_dummy;
	hw->mac.acquire_swfw_sync = ngbe_mac_acquire_swfw_sync_dummy;
	hw->mac.release_swfw_sync = ngbe_mac_release_swfw_sync_dummy;
}

#endif /* _NGBE_TYPE_DUMMY_H_ */

