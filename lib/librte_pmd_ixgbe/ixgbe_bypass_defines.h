/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _IXGBE_BYPASS_DEFINES_H_
#define _IXGBE_BYPASS_DEFINES_H_

#ifdef RTE_NIC_BYPASS

#define msleep(x)             rte_delay_us(x*1000)
#define usleep_range(min, max) rte_delay_us(min)

#define BYPASS_PAGE_CTL0	0x00000000
#define BYPASS_PAGE_CTL1	0x40000000
#define BYPASS_PAGE_CTL2	0x80000000
#define BYPASS_PAGE_M		0xc0000000
#define BYPASS_WE		0x20000000

#define BYPASS_AUTO	0x0
#define BYPASS_NOP	0x0
#define BYPASS_NORM	0x1
#define BYPASS_BYPASS	0x2
#define BYPASS_ISOLATE	0x3

#define BYPASS_EVENT_MAIN_ON	0x1
#define BYPASS_EVENT_AUX_ON	0x2
#define BYPASS_EVENT_MAIN_OFF	0x3
#define BYPASS_EVENT_AUX_OFF	0x4
#define BYPASS_EVENT_WDT_TO	0x5
#define BYPASS_EVENT_USR	0x6

#define BYPASS_MODE_OFF_M	0x00000003
#define BYPASS_STATUS_OFF_M	0x0000000c
#define BYPASS_AUX_ON_M		0x00000030
#define BYPASS_MAIN_ON_M	0x000000c0
#define BYPASS_MAIN_OFF_M	0x00000300
#define BYPASS_AUX_OFF_M	0x00000c00
#define BYPASS_WDTIMEOUT_M	0x00003000
#define BYPASS_WDT_ENABLE_M	0x00004000
#define BYPASS_WDT_VALUE_M	0x00070000

#define BYPASS_MODE_OFF_SHIFT	0
#define BYPASS_STATUS_OFF_SHIFT	2
#define BYPASS_AUX_ON_SHIFT	4
#define BYPASS_MAIN_ON_SHIFT	6
#define BYPASS_MAIN_OFF_SHIFT	8
#define BYPASS_AUX_OFF_SHIFT	10
#define BYPASS_WDTIMEOUT_SHIFT	12
#define BYPASS_WDT_ENABLE_SHIFT	14
#define BYPASS_WDT_TIME_SHIFT	16

#define BYPASS_WDT_1	0x0
#define BYPASS_WDT_1_5	0x1
#define BYPASS_WDT_2	0x2
#define BYPASS_WDT_3	0x3
#define BYPASS_WDT_4	0x4
#define BYPASS_WDT_8	0x5
#define BYPASS_WDT_16	0x6
#define BYPASS_WDT_32	0x7
#define BYPASS_WDT_OFF	0xffff

#define BYPASS_WDT_MASK	0x7

#define BYPASS_CTL1_TIME_M	0x01ffffff
#define BYPASS_CTL1_VALID_M	0x02000000
#define BYPASS_CTL1_OFFTRST_M	0x04000000
#define BYPASS_CTL1_WDT_PET_M	0x08000000

#define BYPASS_CTL1_VALID	0x02000000
#define BYPASS_CTL1_OFFTRST	0x04000000
#define BYPASS_CTL1_WDT_PET	0x08000000

#define BYPASS_CTL2_DATA_M	0x000000ff
#define BYPASS_CTL2_OFFSET_M	0x0000ff00
#define BYPASS_CTL2_RW_M	0x00010000
#define BYPASS_CTL2_HEAD_M	0x0ff00000

#define BYPASS_CTL2_OFFSET_SHIFT	8
#define BYPASS_CTL2_HEAD_SHIFT		20

#define BYPASS_CTL2_RW		0x00010000

enum ixgbe_state_t {
	__IXGBE_TESTING,
	__IXGBE_RESETTING,
	__IXGBE_DOWN,
	__IXGBE_SERVICE_SCHED,
	__IXGBE_IN_SFP_INIT,
	__IXGBE_IN_BYPASS_LOW,
	__IXGBE_IN_BYPASS_HIGH,
	__IXGBE_IN_BYPASS_LOG,
};

#define BYPASS_MAX_LOGS		43
#define BYPASS_LOG_SIZE		5
#define BYPASS_LOG_LINE_SIZE	37

#define BYPASS_EEPROM_VER_ADD	0x02

#define BYPASS_LOG_TIME_M	0x01ffffff
#define BYPASS_LOG_TIME_VALID_M	0x02000000
#define BYPASS_LOG_HEAD_M	0x04000000
#define BYPASS_LOG_CLEAR_M	0x08000000
#define BYPASS_LOG_EVENT_M	0xf0000000
#define BYPASS_LOG_ACTION_M	0x03

#define BYPASS_LOG_EVENT_SHIFT	28
#define BYPASS_LOG_CLEAR_SHIFT	24 /* bit offset */
#define IXGBE_DEV_TO_ADPATER(dev) \
	((struct ixgbe_adapter*)(dev->data->dev_private))

/* extractions from ixgbe_phy.h */
#define	IXGBE_I2C_EEPROM_DEV_ADDR2	0xA2

#define IXGBE_SFF_SFF_8472_SWAP		0x5C
#define IXGBE_SFF_SFF_8472_COMP		0x5E
#define IXGBE_SFF_SFF_8472_OSCB		0x6E
#define IXGBE_SFF_SFF_8472_ESCB		0x76

#define IXGBE_SFF_SOFT_RS_SELECT_MASK	0x8
#define IXGBE_SFF_SOFT_RS_SELECT_10G	0x8
#define IXGBE_SFF_SOFT_RS_SELECT_1G	0x0

/* extractions from ixgbe_type.h */
#define IXGBE_DEV_ID_82599_BYPASS	0x155D

#define IXGBE_BYPASS_FW_WRITE_FAILURE	-35

#endif /* RTE_NIC_BYPASS */

#endif /* _IXGBE_BYPASS_DEFINES_H_ */
