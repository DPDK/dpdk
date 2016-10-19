/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#ifndef GTT_REG_ADDR_H
#define GTT_REG_ADDR_H

/* Win 2 */
/* Access:RW   DataWidth:0x20    Chips: BB_A0 BB_B0 K2 */
#define GTT_BAR0_MAP_REG_IGU_CMD                                      0x00f000UL

/* Win 3 */
/* Access:RW   DataWidth:0x20    Chips: BB_A0 BB_B0 K2 */
#define GTT_BAR0_MAP_REG_TSDM_RAM                                     0x010000UL

/* Win 4 */
/* Access:RW   DataWidth:0x20    Chips: BB_A0 BB_B0 K2 */
#define GTT_BAR0_MAP_REG_MSDM_RAM                                     0x011000UL

/* Win 5 */
/* Access:RW   DataWidth:0x20    Chips: BB_A0 BB_B0 K2 */
#define GTT_BAR0_MAP_REG_MSDM_RAM_1024                                0x012000UL

/* Win 6 */
/* Access:RW   DataWidth:0x20    Chips: BB_A0 BB_B0 K2 */
#define GTT_BAR0_MAP_REG_USDM_RAM                                     0x013000UL

/* Win 7 */
/* Access:RW   DataWidth:0x20    Chips: BB_A0 BB_B0 K2 */
#define GTT_BAR0_MAP_REG_USDM_RAM_1024                                0x014000UL

/* Win 8 */
/* Access:RW   DataWidth:0x20    Chips: BB_A0 BB_B0 K2 */
#define GTT_BAR0_MAP_REG_USDM_RAM_2048                                0x015000UL

/* Win 9 */
/* Access:RW   DataWidth:0x20    Chips: BB_A0 BB_B0 K2 */
#define GTT_BAR0_MAP_REG_XSDM_RAM                                     0x016000UL

/* Win 10 */
/* Access:RW   DataWidth:0x20    Chips: BB_A0 BB_B0 K2 */
#define GTT_BAR0_MAP_REG_YSDM_RAM                                     0x017000UL

/* Win 11 */
/* Access:RW   DataWidth:0x20    Chips: BB_A0 BB_B0 K2 */
#define GTT_BAR0_MAP_REG_PSDM_RAM                                     0x018000UL

#endif
