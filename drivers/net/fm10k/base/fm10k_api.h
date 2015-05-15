/*******************************************************************************

Copyright (c) 2013 - 2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 3. Neither the name of the Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

#ifndef _FM10K_API_H_
#define _FM10K_API_H_

#include "fm10k_pf.h"
#include "fm10k_vf.h"

s32 fm10k_set_mac_type(struct fm10k_hw *hw);
s32 fm10k_reset_hw(struct fm10k_hw *hw);
s32 fm10k_init_hw(struct fm10k_hw *hw);
s32 fm10k_stop_hw(struct fm10k_hw *hw);
s32 fm10k_start_hw(struct fm10k_hw *hw);
s32 fm10k_init_shared_code(struct fm10k_hw *hw);
s32 fm10k_get_bus_info(struct fm10k_hw *hw);
bool fm10k_is_slot_appropriate(struct fm10k_hw *hw);
s32 fm10k_update_vlan(struct fm10k_hw *hw, u32 vid, u8 idx, bool set);
s32 fm10k_read_mac_addr(struct fm10k_hw *hw);
void fm10k_update_hw_stats(struct fm10k_hw *hw, struct fm10k_hw_stats *stats);
void fm10k_rebind_hw_stats(struct fm10k_hw *hw, struct fm10k_hw_stats *stats);
s32 fm10k_configure_dglort_map(struct fm10k_hw *hw,
			       struct fm10k_dglort_cfg *dglort);
void fm10k_set_dma_mask(struct fm10k_hw *hw, u64 dma_mask);
s32 fm10k_get_fault(struct fm10k_hw *hw, int type, struct fm10k_fault *fault);
s32 fm10k_update_uc_addr(struct fm10k_hw *hw, u16 lport,
			  const u8 *mac, u16 vid, bool add, u8 flags);
s32 fm10k_update_mc_addr(struct fm10k_hw *hw, u16 lport,
			 const u8 *mac, u16 vid, bool add);
s32 fm10k_adjust_systime(struct fm10k_hw *hw, s32 ppb);
#endif /* _FM10K_API_H_ */
