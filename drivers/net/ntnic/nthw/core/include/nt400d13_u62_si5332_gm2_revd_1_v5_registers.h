/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Napatech A/S
 */

/*
 * Si5332-GM2 Rev D Configuration Register Export Header File
 *
 * This file represents a series of Skyworks Si5332-GM2 Rev D
 * register writes that can be performed to load a single configuration
 * on a device. It was created by a Skyworks ClockBuilder Pro
 * export tool.
 *
 * Part:		                                       Si5332-GM2 Rev D
 * Design ID:                                          1
 * Includes Pre/Post Download Control Register Writes: Yes
 * Created By:                                         ClockBuilder Pro v4.5 [2022-08-23]
 * Timestamp:                                          2022-10-11 15:44:24 GMT+02:00
 *
 * A complete design report corresponding to this export is included at the end
 * of this header file.
 *
 */

#ifndef SI5332_GM2_REVD_REG_CONFIG_HEADER
#define SI5332_GM2_REVD_REG_CONFIG_HEADER

#define SI5332_GM2_REVD_REG_CONFIG_NUM_REGS 76

typedef struct {
	unsigned int address;	/* 8-bit register address */
	unsigned char value;	/* 8-bit register data */

} si5332_gm2_revd_register_t;

si5332_gm2_revd_register_t const si5332_gm2_revd_registers[SI5332_GM2_REVD_REG_CONFIG_NUM_REGS] = {
	/* Start configuration preamble */
	/*    Set device in Ready mode */
	{ 0x0006, 0x01 },
	/* End configuration preamble */

	/* Start configuration registers */
	{ 0x0017, 0x31 },
	{ 0x0018, 0x00 },
	{ 0x0019, 0x00 },
	{ 0x001A, 0x00 },
	{ 0x001B, 0x00 },
	{ 0x001C, 0x00 },
	{ 0x0021, 0x6A },
	{ 0x0024, 0x01 },
	{ 0x0025, 0x00 },
	{ 0x0026, 0x00 },
	{ 0x0027, 0x50 },
	{ 0x0028, 0x50 },
	{ 0x0029, 0x00 },
	{ 0x002A, 0x00 },
	{ 0x002B, 0x10 },
	{ 0x0036, 0x07 },
	{ 0x0037, 0x80 },
	{ 0x0038, 0x02 },
	{ 0x0039, 0x80 },
	{ 0x003A, 0x05 },
	{ 0x003B, 0x35 },
	{ 0x003C, 0x00 },
	{ 0x0048, 0x00 },
	{ 0x0054, 0x00 },
	{ 0x0060, 0x00 },
	{ 0x0067, 0x19 },
	{ 0x0068, 0x00 },
	{ 0x0069, 0x00 },
	{ 0x006A, 0x00 },
	{ 0x006B, 0x00 },
	{ 0x006C, 0x01 },
	{ 0x0073, 0x00 },
	{ 0x0074, 0x00 },
	{ 0x0075, 0x01 },
	{ 0x007A, 0x07 },
	{ 0x007B, 0x01 },
	{ 0x007C, 0x00 },
	{ 0x007D, 0x00 },
	{ 0x007F, 0x07 },
	{ 0x0080, 0x01 },
	{ 0x0081, 0x00 },
	{ 0x0082, 0x00 },
	{ 0x0089, 0x07 },
	{ 0x008A, 0x01 },
	{ 0x008B, 0x00 },
	{ 0x008C, 0x00 },
	{ 0x008E, 0x07 },
	{ 0x008F, 0x01 },
	{ 0x0090, 0x00 },
	{ 0x0091, 0x00 },
	{ 0x0098, 0x07 },
	{ 0x0099, 0x01 },
	{ 0x009A, 0x00 },
	{ 0x009B, 0x00 },
	{ 0x009D, 0x07 },
	{ 0x009E, 0x01 },
	{ 0x009F, 0x00 },
	{ 0x00A0, 0x00 },
	{ 0x00A7, 0x07 },
	{ 0x00A8, 0x01 },
	{ 0x00A9, 0x00 },
	{ 0x00AA, 0x00 },
	{ 0x00AC, 0x07 },
	{ 0x00AD, 0x01 },
	{ 0x00AE, 0x00 },
	{ 0x00AF, 0x00 },
	{ 0x00B6, 0xDB },
	{ 0x00B7, 0x06 },
	{ 0x00B9, 0x06 },
	{ 0x00BA, 0x5E },
	{ 0x00BB, 0x00 },
	{ 0x00BC, 0x00 },
	{ 0x00BD, 0x00 },
	{ 0x00BE, 0x20 },
	/* End configuration registers */

	/* Start configuration postamble */
	/*    Set device in Active mode */
	{ 0x0006, 0x02 },
	/* End configuration postamble */
};

/*
 * Design Report
 *
 * Overview
 * ========
 * Part:               Si5332EFL Rev D
 * Project File:       P:\Hardware\NT
 * Adapters\NT400D13\design\Clock_syn_design\Si5332-GM2-RevD-1-Project_V5.slabtimeproj Design ID: 1
 * Created By:         ClockBuilder Pro v4.5 [2022-08-23]
 * Timestamp:          2022-10-11 15:44:24 GMT+02:00
 *
 * Design Rule Check
 * =================
 * Errors:
 * - No errors
 *
 * Warnings:
 * - OUT1 [156.25 MHz] and OUT2 [166.625 MHz] may have coupling [1]
 * - OUT5 [166.625 MHz] and OUT6 [156.25 MHz] may have coupling [1]
 *
 * Footnotes:
 * [1] To avoid coupling in outputs, Skyworks recommends the following:
 *
 * - Avoid adjacent frequency values that are close. CBPro uses an output's integration bandwidth
 *   (IBW) to determine whether two adjacent frequencies are too close. An IBW of 20 MHz is used
 *   for frequencies 80 MHz and larger. Lower frequencies will use IBW of OUT/4. CBPro will flag
 *   fundamental coupling and coupling up to the fourth harmonic, where coupling
 *   frequency = Absolute(OUTa*x - OUTb*y) for all combinations of x and y 1 through 4. If any one
 *   of these values is less than or equal to the IBW, the output is flagged as having possible
 *   coupling.
 * - Adjacent frequency values that are integer multiples of one another are okay and these
 *   outputs should be grouped accordingly.
 * - Unused outputs can be used to separate clock outputs that might otherwise interfere
 *   with one another.
 *
 * Skyworks recommends you validate your design's jitter performance using an Evaluation Board.
 * You can request a custom phase noise report for your design from CBPro's design dashboard.
 *
 * Device Grade
 * ============
 * Maximum Output Frequency: 166.625 MHz
 * Frequency Synthesis Mode: Fractional
 * Frequency Plan Grade:     F
 * Minimum Base OPN:         Si5332F*
 *
 * Base       Output Clock         Supported Frequency Synthesis Modes
 * OPN Grade  Frequency Range      (Typical Jitter)
 * ---------  ------------------  --------------------------------------------
 * Si5332E    5 MHz to 334 MHz    Integer (~230 fs) and fractional (~500 fs)
 * Si5332F*   5 MHz to 200 MHz    "
 * Si5332G    5 MHz to 334 MHz    Integer only (~230 fs)
 * Si5332H    5 MHz to 200 MHz    "
 * Si5332L*   5 MHz to 334 MHz    Integer (~230 fs) and fractional (~500 fs)
 *
 * * Based on your calculated frequency plan, a Si5332F grade device is
 * sufficient for your design. For more in-system configuration flexibility
 * (higher frequencies and/or to enable fractional synthesis), consider
 * selecting device grade Si5332E when specifying an ordering part number (OPN)
 * for your application. See the datasheet Ordering Guide for more information.
 *
 * Design
 * ======
 * Base I2C Address: 0x6A
 *
 * Universal Hardware Input Pins:
 *    INPUT1 (P10): None
 *    INPUT2 (P19): None
 *    INPUT3 (P20): None
 *    INPUT4 (P31): None
 *    INPUT5 (P32): None
 *    INPUT6 (P36): None
 *    INPUT7 (P37): None
 *
 * Inputs:
 *    XAXB: 50 MHz
 *          Crystal Mode
 *  CLKIN2: Unused
 *  CLKIN3: Unused
 *
 * Outputs:
 *    OUT0: 156.25 MHz LVDS Fast 1.8 V, Disabled-State: Stop Low
 *          Power-up state: Enabled
 *    OUT1: 156.25 MHz LVDS Fast 1.8 V, Disabled-State: Stop Low
 *          Power-up state: Enabled
 *    OUT2: 166.625 MHz LVDS Fast 1.8 V, Disabled-State: Stop Low
 *          Power-up state: Enabled
 *    OUT3: 166.625 MHz LVDS Fast 1.8 V, Disabled-State: Stop Low
 *          Power-up state: Enabled
 *    OUT4: 166.625 MHz LVDS Fast 1.8 V, Disabled-State: Stop Low
 *          Power-up state: Enabled
 *    OUT5: 166.625 MHz LVDS Fast 1.8 V, Disabled-State: Stop Low
 *          Power-up state: Enabled
 *    OUT6: 156.25 MHz LVDS Fast 1.8 V, Disabled-State: Stop Low
 *          Power-up state: Enabled
 *    OUT7: 156.25 MHz LVDS Fast 1.8 V, Disabled-State: Stop Low
 *          Power-up state: Enabled
 *
 * Frequency Plan
 * ==============
 * Fpfd = 50 MHz
 * Fvco = 2.5 GHz
 *
 * P divider = 1
 * M = 50
 * N dividers:
 *    N0:
 *       Value: 15.0037509377344336... [ 15 + 5/1333 ]
 *       OUT2: 166.625 MHz, Error: 0 ppm
 *       OUT3: 166.625 MHz, Error: 0 ppm
 *       OUT4: 166.625 MHz, Error: 0 ppm
 *       OUT5: 166.625 MHz, Error: 0 ppm
 *    N1:
 *       Unused
 *
 * O dividers:
 *    O0:
 *       Value: 16
 *       OUT0: 156.25 MHz, Error: 0 ppm
 *       OUT1: 156.25 MHz, Error: 0 ppm
 *       OUT6: 156.25 MHz, Error: 0 ppm
 *       OUT7: 156.25 MHz, Error: 0 ppm
 *    O1:
 *       Unused
 *    O2:
 *       Unused
 *    O3:
 *       Unused
 *    O4:
 *       Unused
 *
 * R dividers:
 *    R0 = 1
 *    R1 = 1
 *    R2 = 1
 *    R3 = 1
 *    R4 = 1
 *    R5 = 1
 *    R6 = 1
 *    R7 = 1
 *
 * Estimated Power
 * ===============
 * Total Power: 233 mW, On Chip Power: 220 mW, Tj: 31 °C
 *
 *                    Frequency    Format   Voltage   Current     Power
 *                  -----------  --------  --------  --------  --------
 * VDD                                        1.8 V   30.7 mA     55 mW
 * VDD Dig                                    1.8 V    7.0 mA     13 mW
 * VDD Xtal                                   1.8 V    2.3 mA      4 mW
 * VDDO0
 *            OUT0   156.25 MHz  LVDSFast     1.8 V   11.1 mA     20 mW
 * VDDO1
 *            OUT1   156.25 MHz  LVDSFast     1.8 V   11.1 mA     20 mW
 * VDDO2
 *            OUT2  166.625 MHz  LVDSFast     1.8 V   11.2 mA     20 mW
 *            OUT3  166.625 MHz  LVDSFast     1.8 V   11.2 mA     20 mW
 * VDDO3
 *            OUT4  166.625 MHz  LVDSFast     1.8 V   11.2 mA     20 mW
 *            OUT5  166.625 MHz  LVDSFast     1.8 V   11.2 mA     20 mW
 * VDDO4
 *            OUT6   156.25 MHz  LVDSFast     1.8 V   11.1 mA     20 mW
 * VDDO5
 *            OUT7   156.25 MHz  LVDSFast     1.8 V   11.1 mA     20 mW
 *                                                   --------  --------
 *                                            Total  129.4 mA    233 mW
 *
 * Note:
 *
 * -Tj is junction temperature. Tj must be less than 125 °C (on Si5332-GM2 Revision D) for device
 *  to comply with datasheet specifications. Tj = Ta + Theta_JA*On_Chip_Power.
 * -Overall power includes on-chip power dissipation and adds differential load power dissipation
 *  to estimate total power requirements.
 * -Above are estimates only: power and temperature should be measured on your PCB.
 *
 * Settings
 * ========
 *
 * Location    Setting Name     Decimal Value      Hex Value
 * ----------  ---------------  -----------------  -----------------
 * 0x17[7:0]   DESIGN_ID0       49                 0x31
 * 0x18[7:0]   DESIGN_ID1       0                  0x00
 * 0x19[7:0]   DESIGN_ID2       0                  0x00
 * 0x1A[7:0]   DESIGN_ID3       0                  0x00
 * 0x1B[7:0]   DESIGN_ID4       0                  0x00
 * 0x1C[7:0]   DESIGN_ID5       0                  0x00
 * 0x21[6:0]   I2C_ADDR         106                0x6A
 * 0x24[1:0]   IMUX_SEL         1                  0x1
 * 0x25[1:0]   OMUX0_SEL0       0                  0x0
 * 0x25[6:4]   OMUX0_SEL1       0                  0x0
 * 0x26[1:0]   OMUX1_SEL0       0                  0x0
 * 0x26[6:4]   OMUX1_SEL1       0                  0x0
 * 0x27[1:0]   OMUX2_SEL0       0                  0x0
 * 0x27[6:4]   OMUX2_SEL1       5                  0x5
 * 0x28[1:0]   OMUX3_SEL0       0                  0x0
 * 0x28[6:4]   OMUX3_SEL1       5                  0x5
 * 0x29[1:0]   OMUX4_SEL0       0                  0x0
 * 0x29[6:4]   OMUX4_SEL1       0                  0x0
 * 0x2A[1:0]   OMUX5_SEL0       0                  0x0
 * 0x2A[6:4]   OMUX5_SEL1       0                  0x0
 * 0x2B[7:0]   HSDIV0A_DIV      16                 0x10
 * 0x36[14:0]  ID0A_INTG        1920               0x0780
 * 0x38[14:0]  ID0A_RES         640                0x0280
 * 0x3A[14:0]  ID0A_DEN         1333               0x0535
 * 0x3C[0]     ID0A_SS_ENA      0                  0x0
 * 0x3C[2:1]   ID0A_SS_MODE     0                  0x0
 * 0x48[0]     ID0B_SS_ENA      0                  0x0
 * 0x48[2:1]   ID0B_SS_MODE     0                  0x0
 * 0x54[0]     ID1A_SS_ENA      0                  0x0
 * 0x54[2:1]   ID1A_SS_MODE     0                  0x0
 * 0x60[0]     ID1B_SS_ENA      0                  0x0
 * 0x60[2:1]   ID1B_SS_MODE     0                  0x0
 * 0x67[14:0]  IDPA_INTG        6400               0x1900
 * 0x69[14:0]  IDPA_RES         0                  0x0000
 * 0x6B[14:0]  IDPA_DEN         1                  0x0001
 * 0x73[1:0]   CLKIN_2_CLK_SEL  0                  0x0
 * 0x74[1:0]   CLKIN_3_CLK_SEL  0                  0x0
 * 0x75[4:0]   P_VAL            1                  0x01
 * 0x7A[3:0]   OUT0_MODE        7                  0x7
 * 0x7B[5:0]   OUT0_DIV         1                  0x01
 * 0x7C[2:0]   OUT0_SKEW        0                  0x0
 * 0x7D[0]     OUT0_STOP_HIGHZ  0                  0x0
 * 0x7D[5:4]   OUT0_CMOS_INV    0                  0x0
 * 0x7D[6]     OUT0_DIFF_INV    0                  0x0
 * 0x7F[3:0]   OUT1_MODE        7                  0x7
 * 0x80[5:0]   OUT1_DIV         1                  0x01
 * 0x81[2:0]   OUT1_SKEW        0                  0x0
 * 0x82[0]     OUT1_STOP_HIGHZ  0                  0x0
 * 0x82[5:4]   OUT1_CMOS_INV    0                  0x0
 * 0x82[6]     OUT1_DIFF_INV    0                  0x0
 * 0x89[3:0]   OUT2_MODE        7                  0x7
 * 0x8A[5:0]   OUT2_DIV         1                  0x01
 * 0x8B[2:0]   OUT2_SKEW        0                  0x0
 * 0x8C[0]     OUT2_STOP_HIGHZ  0                  0x0
 * 0x8C[5:4]   OUT2_CMOS_INV    0                  0x0
 * 0x8C[6]     OUT2_DIFF_INV    0                  0x0
 * 0x8E[3:0]   OUT3_MODE        7                  0x7
 * 0x8F[5:0]   OUT3_DIV         1                  0x01
 * 0x90[2:0]   OUT3_SKEW        0                  0x0
 * 0x91[0]     OUT3_STOP_HIGHZ  0                  0x0
 * 0x91[5:4]   OUT3_CMOS_INV    0                  0x0
 * 0x91[6]     OUT3_DIFF_INV    0                  0x0
 * 0x98[3:0]   OUT4_MODE        7                  0x7
 * 0x99[5:0]   OUT4_DIV         1                  0x01
 * 0x9A[2:0]   OUT4_SKEW        0                  0x0
 * 0x9B[0]     OUT4_STOP_HIGHZ  0                  0x0
 * 0x9B[5:4]   OUT4_CMOS_INV    0                  0x0
 * 0x9B[6]     OUT4_DIFF_INV    0                  0x0
 * 0x9D[3:0]   OUT5_MODE        7                  0x7
 * 0x9E[5:0]   OUT5_DIV         1                  0x01
 * 0x9F[2:0]   OUT5_SKEW        0                  0x0
 * 0xA0[0]     OUT5_STOP_HIGHZ  0                  0x0
 * 0xA0[5:4]   OUT5_CMOS_INV    0                  0x0
 * 0xA0[6]     OUT5_DIFF_INV    0                  0x0
 * 0xA7[3:0]   OUT6_MODE        7                  0x7
 * 0xA8[5:0]   OUT6_DIV         1                  0x01
 * 0xA9[2:0]   OUT6_SKEW        0                  0x0
 * 0xAA[0]     OUT6_STOP_HIGHZ  0                  0x0
 * 0xAA[5:4]   OUT6_CMOS_INV    0                  0x0
 * 0xAA[6]     OUT6_DIFF_INV    0                  0x0
 * 0xAC[3:0]   OUT7_MODE        7                  0x7
 * 0xAD[5:0]   OUT7_DIV         1                  0x01
 * 0xAE[2:0]   OUT7_SKEW        0                  0x0
 * 0xAF[0]     OUT7_STOP_HIGHZ  0                  0x0
 * 0xAF[5:4]   OUT7_CMOS_INV    0                  0x0
 * 0xAF[6]     OUT7_DIFF_INV    0                  0x0
 * 0xB6[0]     OUT0_OE          1                  0x1
 * 0xB6[1]     OUT1_OE          1                  0x1
 * 0xB6[3]     OUT2_OE          1                  0x1
 * 0xB6[4]     OUT3_OE          1                  0x1
 * 0xB6[6]     OUT4_OE          1                  0x1
 * 0xB6[7]     OUT5_OE          1                  0x1
 * 0xB7[1]     OUT6_OE          1                  0x1
 * 0xB7[2]     OUT7_OE          1                  0x1
 * 0xB9[0]     XOSC_DIS         0                  0x0
 * 0xB9[1]     IBUF0_DIS        1                  0x1
 * 0xB9[2]     IBUF1_DIS        1                  0x1
 * 0xB9[3]     IMUX_DIS         0                  0x0
 * 0xB9[4]     PDIV_DIS         0                  0x0
 * 0xB9[5]     PLL_DIS          0                  0x0
 * 0xBA[5]     ID0_DIS          0                  0x0
 * 0xBA[6]     ID1_DIS          1                  0x1
 * 0xBA[0]     HSDIV0_DIS       0                  0x0
 * 0xBA[1]     HSDIV1_DIS       1                  0x1
 * 0xBA[2]     HSDIV2_DIS       1                  0x1
 * 0xBA[3]     HSDIV3_DIS       1                  0x1
 * 0xBA[4]     HSDIV4_DIS       1                  0x1
 * 0xBB[0]     OMUX0_DIS        0                  0x0
 * 0xBB[1]     OMUX1_DIS        0                  0x0
 * 0xBB[2]     OMUX2_DIS        0                  0x0
 * 0xBB[3]     OMUX3_DIS        0                  0x0
 * 0xBB[4]     OMUX4_DIS        0                  0x0
 * 0xBB[5]     OMUX5_DIS        0                  0x0
 * 0xBC[0]     OUT0_DIS         0                  0x0
 * 0xBC[1]     OUT1_DIS         0                  0x0
 * 0xBC[3]     OUT2_DIS         0                  0x0
 * 0xBC[4]     OUT3_DIS         0                  0x0
 * 0xBC[6]     OUT4_DIS         0                  0x0
 * 0xBC[7]     OUT5_DIS         0                  0x0
 * 0xBD[1]     OUT6_DIS         0                  0x0
 * 0xBD[2]     OUT7_DIS         0                  0x0
 * 0xBE[7:0]   PLL_MODE         32                 0x20
 *
 *
 */

#endif
