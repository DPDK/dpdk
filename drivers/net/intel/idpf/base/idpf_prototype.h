/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2023 Intel Corporation
 */

#ifndef _IDPF_PROTOTYPE_H_
#define _IDPF_PROTOTYPE_H_

/* Include generic macros and types first */
#include "idpf_osdep.h"
#include "idpf_controlq.h"
#include "idpf_type.h"
#include "idpf_alloc.h"
#include "idpf_devids.h"
#include "idpf_controlq_api.h"
#include "idpf_lan_pf_regs.h"
#include "idpf_lan_vf_regs.h"
#include "idpf_lan_txrx.h"
#include <virtchnl.h>

#define APF

int idpf_init_hw(struct idpf_hw *hw, struct idpf_ctlq_size ctlq_size);
void idpf_deinit_hw(struct idpf_hw *hw);

int idpf_clean_arq_element(struct idpf_hw *hw,
			   struct idpf_arq_event_info *e,
			   u16 *events_pending);
bool idpf_asq_done(struct idpf_hw *hw);
bool idpf_check_asq_alive(struct idpf_hw *hw);

int idpf_get_rss_lut(struct idpf_hw *hw, u16 seid, bool pf_lut,
		     u8 *lut, u16 lut_size);
int idpf_set_rss_lut(struct idpf_hw *hw, u16 seid, bool pf_lut,
		     u8 *lut, u16 lut_size);
int idpf_get_rss_key(struct idpf_hw *hw, u16 seid,
		     struct idpf_get_set_rss_key_data *key);
int idpf_set_rss_key(struct idpf_hw *hw, u16 seid,
		     struct idpf_get_set_rss_key_data *key);

int idpf_set_mac_type(struct idpf_hw *hw);

int idpf_reset(struct idpf_hw *hw);
int idpf_send_msg_to_cp(struct idpf_hw *hw, int v_opcode,
			int v_retval, u8 *msg, u16 msglen);
#endif /* _IDPF_PROTOTYPE_H_ */
