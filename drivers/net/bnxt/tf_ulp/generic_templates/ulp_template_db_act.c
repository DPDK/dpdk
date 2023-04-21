/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#include "ulp_template_db_enum.h"
#include "ulp_template_db_field.h"
#include "ulp_template_struct.h"
#include "ulp_template_db_tbl.h"

/*
 * Action signature table:
 * maps hash id to ulp_act_match_list[] index
 */
uint16_t ulp_act_sig_tbl[BNXT_ULP_ACT_SIG_TBL_MAX_SZ] = {
	[BNXT_ULP_ACT_HID_0000] = 1,
	[BNXT_ULP_ACT_HID_0008] = 2,
	[BNXT_ULP_ACT_HID_2000] = 3,
	[BNXT_ULP_ACT_HID_1988] = 4,
	[BNXT_ULP_ACT_HID_0080] = 5,
	[BNXT_ULP_ACT_HID_3988] = 6,
	[BNXT_ULP_ACT_HID_1a08] = 7,
	[BNXT_ULP_ACT_HID_0010] = 8,
	[BNXT_ULP_ACT_HID_0040] = 9,
	[BNXT_ULP_ACT_HID_0050] = 10,
	[BNXT_ULP_ACT_HID_0018] = 11,
	[BNXT_ULP_ACT_HID_2010] = 12,
	[BNXT_ULP_ACT_HID_1998] = 13,
	[BNXT_ULP_ACT_HID_0090] = 14,
	[BNXT_ULP_ACT_HID_3998] = 15,
	[BNXT_ULP_ACT_HID_1a18] = 16,
	[BNXT_ULP_ACT_HID_32ea] = 17,
	[BNXT_ULP_ACT_HID_32f2] = 18,
	[BNXT_ULP_ACT_HID_52ea] = 19,
	[BNXT_ULP_ACT_HID_4c72] = 20,
	[BNXT_ULP_ACT_HID_336a] = 21,
	[BNXT_ULP_ACT_HID_6c72] = 22,
	[BNXT_ULP_ACT_HID_4cf2] = 23,
	[BNXT_ULP_ACT_HID_32fa] = 24,
	[BNXT_ULP_ACT_HID_3302] = 25,
	[BNXT_ULP_ACT_HID_52fa] = 26,
	[BNXT_ULP_ACT_HID_4c82] = 27,
	[BNXT_ULP_ACT_HID_337a] = 28,
	[BNXT_ULP_ACT_HID_6c82] = 29,
	[BNXT_ULP_ACT_HID_4d02] = 30,
	[BNXT_ULP_ACT_HID_0808] = 31,
	[BNXT_ULP_ACT_HID_1008] = 32,
	[BNXT_ULP_ACT_HID_1808] = 33,
	[BNXT_ULP_ACT_HID_0818] = 34,
	[BNXT_ULP_ACT_HID_1018] = 35,
	[BNXT_ULP_ACT_HID_1818] = 36,
	[BNXT_ULP_ACT_HID_0880] = 37,
	[BNXT_ULP_ACT_HID_1080] = 38,
	[BNXT_ULP_ACT_HID_1880] = 39,
	[BNXT_ULP_ACT_HID_0890] = 40,
	[BNXT_ULP_ACT_HID_1090] = 41,
	[BNXT_ULP_ACT_HID_1890] = 42,
	[BNXT_ULP_ACT_HID_3af2] = 43,
	[BNXT_ULP_ACT_HID_42f2] = 44,
	[BNXT_ULP_ACT_HID_4af2] = 45,
	[BNXT_ULP_ACT_HID_3b02] = 46,
	[BNXT_ULP_ACT_HID_4302] = 47,
	[BNXT_ULP_ACT_HID_4b02] = 48,
	[BNXT_ULP_ACT_HID_3b6a] = 49,
	[BNXT_ULP_ACT_HID_436a] = 50,
	[BNXT_ULP_ACT_HID_4b6a] = 51,
	[BNXT_ULP_ACT_HID_3b7a] = 52,
	[BNXT_ULP_ACT_HID_437a] = 53,
	[BNXT_ULP_ACT_HID_4b7a] = 54,
	[BNXT_ULP_ACT_HID_640d] = 55,
	[BNXT_ULP_ACT_HID_641d] = 56,
	[BNXT_ULP_ACT_HID_071a] = 57,
	[BNXT_ULP_ACT_HID_0800] = 58,
	[BNXT_ULP_ACT_HID_1000] = 59,
	[BNXT_ULP_ACT_HID_1800] = 60,
	[BNXT_ULP_ACT_HID_0810] = 61,
	[BNXT_ULP_ACT_HID_1010] = 62,
	[BNXT_ULP_ACT_HID_1810] = 63,
	[BNXT_ULP_ACT_HID_1110] = 64,
	[BNXT_ULP_ACT_HID_4420] = 65,
	[BNXT_ULP_ACT_HID_2220] = 66,
	[BNXT_ULP_ACT_HID_0c84] = 67,
	[BNXT_ULP_ACT_HID_3f94] = 68,
	[BNXT_ULP_ACT_HID_3330] = 69,
	[BNXT_ULP_ACT_HID_50a4] = 70,
	[BNXT_ULP_ACT_HID_1910] = 71,
	[BNXT_ULP_ACT_HID_4c20] = 72,
	[BNXT_ULP_ACT_HID_2a20] = 73,
	[BNXT_ULP_ACT_HID_1484] = 74,
	[BNXT_ULP_ACT_HID_4794] = 75,
	[BNXT_ULP_ACT_HID_3b30] = 76,
	[BNXT_ULP_ACT_HID_58a4] = 77,
	[BNXT_ULP_ACT_HID_2110] = 78,
	[BNXT_ULP_ACT_HID_5420] = 79,
	[BNXT_ULP_ACT_HID_3220] = 80,
	[BNXT_ULP_ACT_HID_1c84] = 81,
	[BNXT_ULP_ACT_HID_4f94] = 82,
	[BNXT_ULP_ACT_HID_4330] = 83,
	[BNXT_ULP_ACT_HID_60a4] = 84,
	[BNXT_ULP_ACT_HID_2910] = 85,
	[BNXT_ULP_ACT_HID_5c20] = 86,
	[BNXT_ULP_ACT_HID_3a20] = 87,
	[BNXT_ULP_ACT_HID_2484] = 88,
	[BNXT_ULP_ACT_HID_5794] = 89,
	[BNXT_ULP_ACT_HID_4b30] = 90,
	[BNXT_ULP_ACT_HID_68a4] = 91,
	[BNXT_ULP_ACT_HID_1120] = 92,
	[BNXT_ULP_ACT_HID_4430] = 93,
	[BNXT_ULP_ACT_HID_2230] = 94,
	[BNXT_ULP_ACT_HID_0c94] = 95,
	[BNXT_ULP_ACT_HID_3fa4] = 96,
	[BNXT_ULP_ACT_HID_3340] = 97,
	[BNXT_ULP_ACT_HID_50b4] = 98,
	[BNXT_ULP_ACT_HID_1920] = 99,
	[BNXT_ULP_ACT_HID_4c30] = 100,
	[BNXT_ULP_ACT_HID_2a30] = 101,
	[BNXT_ULP_ACT_HID_1494] = 102,
	[BNXT_ULP_ACT_HID_47a4] = 103,
	[BNXT_ULP_ACT_HID_3b40] = 104,
	[BNXT_ULP_ACT_HID_58b4] = 105,
	[BNXT_ULP_ACT_HID_2120] = 106,
	[BNXT_ULP_ACT_HID_5430] = 107,
	[BNXT_ULP_ACT_HID_3230] = 108,
	[BNXT_ULP_ACT_HID_1c94] = 109,
	[BNXT_ULP_ACT_HID_4fa4] = 110,
	[BNXT_ULP_ACT_HID_4340] = 111,
	[BNXT_ULP_ACT_HID_60b4] = 112,
	[BNXT_ULP_ACT_HID_2920] = 113,
	[BNXT_ULP_ACT_HID_5c30] = 114,
	[BNXT_ULP_ACT_HID_3a30] = 115,
	[BNXT_ULP_ACT_HID_2494] = 116,
	[BNXT_ULP_ACT_HID_57a4] = 117,
	[BNXT_ULP_ACT_HID_4b40] = 118,
	[BNXT_ULP_ACT_HID_68b4] = 119,
	[BNXT_ULP_ACT_HID_2a98] = 120,
	[BNXT_ULP_ACT_HID_5da8] = 121,
	[BNXT_ULP_ACT_HID_3ba8] = 122,
	[BNXT_ULP_ACT_HID_260c] = 123,
	[BNXT_ULP_ACT_HID_591c] = 124,
	[BNXT_ULP_ACT_HID_6a2c] = 125,
	[BNXT_ULP_ACT_HID_2aa8] = 126,
	[BNXT_ULP_ACT_HID_5db8] = 127,
	[BNXT_ULP_ACT_HID_3bb8] = 128,
	[BNXT_ULP_ACT_HID_261c] = 129,
	[BNXT_ULP_ACT_HID_592c] = 130,
	[BNXT_ULP_ACT_HID_6a3c] = 131,
	[BNXT_ULP_ACT_HID_3298] = 132,
	[BNXT_ULP_ACT_HID_65a8] = 133,
	[BNXT_ULP_ACT_HID_43a8] = 134,
	[BNXT_ULP_ACT_HID_2e0c] = 135,
	[BNXT_ULP_ACT_HID_611c] = 136,
	[BNXT_ULP_ACT_HID_722c] = 137,
	[BNXT_ULP_ACT_HID_32a8] = 138,
	[BNXT_ULP_ACT_HID_65b8] = 139,
	[BNXT_ULP_ACT_HID_43b8] = 140,
	[BNXT_ULP_ACT_HID_2e1c] = 141,
	[BNXT_ULP_ACT_HID_612c] = 142,
	[BNXT_ULP_ACT_HID_723c] = 143,
	[BNXT_ULP_ACT_HID_3a98] = 144,
	[BNXT_ULP_ACT_HID_6da8] = 145,
	[BNXT_ULP_ACT_HID_4ba8] = 146,
	[BNXT_ULP_ACT_HID_360c] = 147,
	[BNXT_ULP_ACT_HID_691c] = 148,
	[BNXT_ULP_ACT_HID_7a2c] = 149,
	[BNXT_ULP_ACT_HID_3aa8] = 150,
	[BNXT_ULP_ACT_HID_6db8] = 151,
	[BNXT_ULP_ACT_HID_4bb8] = 152,
	[BNXT_ULP_ACT_HID_361c] = 153,
	[BNXT_ULP_ACT_HID_692c] = 154,
	[BNXT_ULP_ACT_HID_7a3c] = 155,
	[BNXT_ULP_ACT_HID_4298] = 156,
	[BNXT_ULP_ACT_HID_75a8] = 157,
	[BNXT_ULP_ACT_HID_53a8] = 158,
	[BNXT_ULP_ACT_HID_3e0c] = 159,
	[BNXT_ULP_ACT_HID_711c] = 160,
	[BNXT_ULP_ACT_HID_0670] = 161,
	[BNXT_ULP_ACT_HID_42a8] = 162,
	[BNXT_ULP_ACT_HID_75b8] = 163,
	[BNXT_ULP_ACT_HID_53b8] = 164,
	[BNXT_ULP_ACT_HID_3e1c] = 165,
	[BNXT_ULP_ACT_HID_712c] = 166,
	[BNXT_ULP_ACT_HID_0680] = 167,
	[BNXT_ULP_ACT_HID_3aea] = 168,
	[BNXT_ULP_ACT_HID_42ea] = 169,
	[BNXT_ULP_ACT_HID_4aea] = 170,
	[BNXT_ULP_ACT_HID_3afa] = 171,
	[BNXT_ULP_ACT_HID_42fa] = 172,
	[BNXT_ULP_ACT_HID_4afa] = 173,
	[BNXT_ULP_ACT_HID_43fa] = 174,
	[BNXT_ULP_ACT_HID_770a] = 175,
	[BNXT_ULP_ACT_HID_550a] = 176,
	[BNXT_ULP_ACT_HID_3f6e] = 177,
	[BNXT_ULP_ACT_HID_727e] = 178,
	[BNXT_ULP_ACT_HID_661a] = 179,
	[BNXT_ULP_ACT_HID_07d2] = 180,
	[BNXT_ULP_ACT_HID_4bfa] = 181,
	[BNXT_ULP_ACT_HID_034e] = 182,
	[BNXT_ULP_ACT_HID_5d0a] = 183,
	[BNXT_ULP_ACT_HID_476e] = 184,
	[BNXT_ULP_ACT_HID_7a7e] = 185,
	[BNXT_ULP_ACT_HID_6e1a] = 186,
	[BNXT_ULP_ACT_HID_0fd2] = 187,
	[BNXT_ULP_ACT_HID_53fa] = 188,
	[BNXT_ULP_ACT_HID_0b4e] = 189,
	[BNXT_ULP_ACT_HID_650a] = 190,
	[BNXT_ULP_ACT_HID_4f6e] = 191,
	[BNXT_ULP_ACT_HID_06c2] = 192,
	[BNXT_ULP_ACT_HID_761a] = 193,
	[BNXT_ULP_ACT_HID_17d2] = 194,
	[BNXT_ULP_ACT_HID_5bfa] = 195,
	[BNXT_ULP_ACT_HID_134e] = 196,
	[BNXT_ULP_ACT_HID_6d0a] = 197,
	[BNXT_ULP_ACT_HID_576e] = 198,
	[BNXT_ULP_ACT_HID_0ec2] = 199,
	[BNXT_ULP_ACT_HID_025e] = 200,
	[BNXT_ULP_ACT_HID_1fd2] = 201,
	[BNXT_ULP_ACT_HID_440a] = 202,
	[BNXT_ULP_ACT_HID_771a] = 203,
	[BNXT_ULP_ACT_HID_551a] = 204,
	[BNXT_ULP_ACT_HID_3f7e] = 205,
	[BNXT_ULP_ACT_HID_728e] = 206,
	[BNXT_ULP_ACT_HID_662a] = 207,
	[BNXT_ULP_ACT_HID_07e2] = 208,
	[BNXT_ULP_ACT_HID_4c0a] = 209,
	[BNXT_ULP_ACT_HID_035e] = 210,
	[BNXT_ULP_ACT_HID_5d1a] = 211,
	[BNXT_ULP_ACT_HID_477e] = 212,
	[BNXT_ULP_ACT_HID_7a8e] = 213,
	[BNXT_ULP_ACT_HID_6e2a] = 214,
	[BNXT_ULP_ACT_HID_0fe2] = 215,
	[BNXT_ULP_ACT_HID_540a] = 216,
	[BNXT_ULP_ACT_HID_0b5e] = 217,
	[BNXT_ULP_ACT_HID_651a] = 218,
	[BNXT_ULP_ACT_HID_4f7e] = 219,
	[BNXT_ULP_ACT_HID_06d2] = 220,
	[BNXT_ULP_ACT_HID_762a] = 221,
	[BNXT_ULP_ACT_HID_17e2] = 222,
	[BNXT_ULP_ACT_HID_5c0a] = 223,
	[BNXT_ULP_ACT_HID_135e] = 224,
	[BNXT_ULP_ACT_HID_6d1a] = 225,
	[BNXT_ULP_ACT_HID_577e] = 226,
	[BNXT_ULP_ACT_HID_0ed2] = 227,
	[BNXT_ULP_ACT_HID_026e] = 228,
	[BNXT_ULP_ACT_HID_1fe2] = 229,
	[BNXT_ULP_ACT_HID_5d82] = 230,
	[BNXT_ULP_ACT_HID_14d6] = 231,
	[BNXT_ULP_ACT_HID_6e92] = 232,
	[BNXT_ULP_ACT_HID_58f6] = 233,
	[BNXT_ULP_ACT_HID_104a] = 234,
	[BNXT_ULP_ACT_HID_215a] = 235,
	[BNXT_ULP_ACT_HID_5d92] = 236,
	[BNXT_ULP_ACT_HID_14e6] = 237,
	[BNXT_ULP_ACT_HID_6ea2] = 238,
	[BNXT_ULP_ACT_HID_5906] = 239,
	[BNXT_ULP_ACT_HID_105a] = 240,
	[BNXT_ULP_ACT_HID_216a] = 241,
	[BNXT_ULP_ACT_HID_6582] = 242,
	[BNXT_ULP_ACT_HID_1cd6] = 243,
	[BNXT_ULP_ACT_HID_7692] = 244,
	[BNXT_ULP_ACT_HID_60f6] = 245,
	[BNXT_ULP_ACT_HID_184a] = 246,
	[BNXT_ULP_ACT_HID_295a] = 247,
	[BNXT_ULP_ACT_HID_6592] = 248,
	[BNXT_ULP_ACT_HID_1ce6] = 249,
	[BNXT_ULP_ACT_HID_76a2] = 250,
	[BNXT_ULP_ACT_HID_6106] = 251,
	[BNXT_ULP_ACT_HID_185a] = 252,
	[BNXT_ULP_ACT_HID_296a] = 253,
	[BNXT_ULP_ACT_HID_6d82] = 254,
	[BNXT_ULP_ACT_HID_24d6] = 255,
	[BNXT_ULP_ACT_HID_02d6] = 256,
	[BNXT_ULP_ACT_HID_68f6] = 257,
	[BNXT_ULP_ACT_HID_204a] = 258,
	[BNXT_ULP_ACT_HID_315a] = 259,
	[BNXT_ULP_ACT_HID_6d92] = 260,
	[BNXT_ULP_ACT_HID_24e6] = 261,
	[BNXT_ULP_ACT_HID_02e6] = 262,
	[BNXT_ULP_ACT_HID_6906] = 263,
	[BNXT_ULP_ACT_HID_205a] = 264,
	[BNXT_ULP_ACT_HID_316a] = 265,
	[BNXT_ULP_ACT_HID_7582] = 266,
	[BNXT_ULP_ACT_HID_2cd6] = 267,
	[BNXT_ULP_ACT_HID_0ad6] = 268,
	[BNXT_ULP_ACT_HID_70f6] = 269,
	[BNXT_ULP_ACT_HID_284a] = 270,
	[BNXT_ULP_ACT_HID_395a] = 271,
	[BNXT_ULP_ACT_HID_7592] = 272,
	[BNXT_ULP_ACT_HID_2ce6] = 273,
	[BNXT_ULP_ACT_HID_0ae6] = 274,
	[BNXT_ULP_ACT_HID_7106] = 275,
	[BNXT_ULP_ACT_HID_285a] = 276,
	[BNXT_ULP_ACT_HID_396a] = 277,
	[BNXT_ULP_ACT_HID_0020] = 278,
	[BNXT_ULP_ACT_HID_0030] = 279,
	[BNXT_ULP_ACT_HID_65d4] = 280,
	[BNXT_ULP_ACT_HID_65e4] = 281,
	[BNXT_ULP_ACT_HID_330a] = 282,
	[BNXT_ULP_ACT_HID_331a] = 283,
	[BNXT_ULP_ACT_HID_1cfe] = 284,
	[BNXT_ULP_ACT_HID_1d0e] = 285,
	[BNXT_ULP_ACT_HID_1474] = 286,
	[BNXT_ULP_ACT_HID_4838] = 287,
	[BNXT_ULP_ACT_HID_6458] = 288,
	[BNXT_ULP_ACT_HID_1c68] = 289,
	[BNXT_ULP_ACT_HID_6c34] = 290,
	[BNXT_ULP_ACT_HID_5d08] = 291,
	[BNXT_ULP_ACT_HID_5d10] = 292,
	[BNXT_ULP_ACT_HID_5d20] = 293,
	[BNXT_ULP_ACT_HID_2e18] = 294,
	[BNXT_ULP_ACT_HID_29d4] = 295,
	[BNXT_ULP_ACT_HID_7690] = 296,
	[BNXT_ULP_ACT_HID_47a0] = 297,
	[BNXT_ULP_ACT_HID_435c] = 298,
	[BNXT_ULP_ACT_HID_5d18] = 299,
	[BNXT_ULP_ACT_HID_2e28] = 300,
	[BNXT_ULP_ACT_HID_29e4] = 301,
	[BNXT_ULP_ACT_HID_76a0] = 302,
	[BNXT_ULP_ACT_HID_47b0] = 303,
	[BNXT_ULP_ACT_HID_436c] = 304,
	[BNXT_ULP_ACT_HID_1436] = 305,
	[BNXT_ULP_ACT_HID_143e] = 306,
	[BNXT_ULP_ACT_HID_144e] = 307,
	[BNXT_ULP_ACT_HID_6102] = 308,
	[BNXT_ULP_ACT_HID_5cbe] = 309,
	[BNXT_ULP_ACT_HID_2dbe] = 310,
	[BNXT_ULP_ACT_HID_7a8a] = 311,
	[BNXT_ULP_ACT_HID_7646] = 312,
	[BNXT_ULP_ACT_HID_1446] = 313,
	[BNXT_ULP_ACT_HID_6112] = 314,
	[BNXT_ULP_ACT_HID_5cce] = 315,
	[BNXT_ULP_ACT_HID_2dce] = 316,
	[BNXT_ULP_ACT_HID_7a9a] = 317,
	[BNXT_ULP_ACT_HID_7656] = 318,
	[BNXT_ULP_ACT_HID_6508] = 319,
	[BNXT_ULP_ACT_HID_6d08] = 320,
	[BNXT_ULP_ACT_HID_7508] = 321,
	[BNXT_ULP_ACT_HID_6518] = 322,
	[BNXT_ULP_ACT_HID_6d18] = 323,
	[BNXT_ULP_ACT_HID_7518] = 324,
	[BNXT_ULP_ACT_HID_6e18] = 325,
	[BNXT_ULP_ACT_HID_256c] = 326,
	[BNXT_ULP_ACT_HID_036c] = 327,
	[BNXT_ULP_ACT_HID_698c] = 328,
	[BNXT_ULP_ACT_HID_20e0] = 329,
	[BNXT_ULP_ACT_HID_31f0] = 330,
	[BNXT_ULP_ACT_HID_7618] = 331,
	[BNXT_ULP_ACT_HID_2d6c] = 332,
	[BNXT_ULP_ACT_HID_0b6c] = 333,
	[BNXT_ULP_ACT_HID_718c] = 334,
	[BNXT_ULP_ACT_HID_28e0] = 335,
	[BNXT_ULP_ACT_HID_39f0] = 336,
	[BNXT_ULP_ACT_HID_025c] = 337,
	[BNXT_ULP_ACT_HID_356c] = 338,
	[BNXT_ULP_ACT_HID_136c] = 339,
	[BNXT_ULP_ACT_HID_798c] = 340,
	[BNXT_ULP_ACT_HID_30e0] = 341,
	[BNXT_ULP_ACT_HID_41f0] = 342,
	[BNXT_ULP_ACT_HID_0a5c] = 343,
	[BNXT_ULP_ACT_HID_3d6c] = 344,
	[BNXT_ULP_ACT_HID_1b6c] = 345,
	[BNXT_ULP_ACT_HID_05d0] = 346,
	[BNXT_ULP_ACT_HID_38e0] = 347,
	[BNXT_ULP_ACT_HID_49f0] = 348,
	[BNXT_ULP_ACT_HID_6e28] = 349,
	[BNXT_ULP_ACT_HID_257c] = 350,
	[BNXT_ULP_ACT_HID_037c] = 351,
	[BNXT_ULP_ACT_HID_699c] = 352,
	[BNXT_ULP_ACT_HID_20f0] = 353,
	[BNXT_ULP_ACT_HID_3200] = 354,
	[BNXT_ULP_ACT_HID_7628] = 355,
	[BNXT_ULP_ACT_HID_2d7c] = 356,
	[BNXT_ULP_ACT_HID_0b7c] = 357,
	[BNXT_ULP_ACT_HID_719c] = 358,
	[BNXT_ULP_ACT_HID_28f0] = 359,
	[BNXT_ULP_ACT_HID_3a00] = 360,
	[BNXT_ULP_ACT_HID_026c] = 361,
	[BNXT_ULP_ACT_HID_357c] = 362,
	[BNXT_ULP_ACT_HID_137c] = 363,
	[BNXT_ULP_ACT_HID_799c] = 364,
	[BNXT_ULP_ACT_HID_30f0] = 365,
	[BNXT_ULP_ACT_HID_4200] = 366,
	[BNXT_ULP_ACT_HID_0a6c] = 367,
	[BNXT_ULP_ACT_HID_3d7c] = 368,
	[BNXT_ULP_ACT_HID_1b7c] = 369,
	[BNXT_ULP_ACT_HID_05e0] = 370,
	[BNXT_ULP_ACT_HID_38f0] = 371,
	[BNXT_ULP_ACT_HID_4a00] = 372,
	[BNXT_ULP_ACT_HID_0be4] = 373,
	[BNXT_ULP_ACT_HID_3ef4] = 374,
	[BNXT_ULP_ACT_HID_1cf4] = 375,
	[BNXT_ULP_ACT_HID_0758] = 376,
	[BNXT_ULP_ACT_HID_3a68] = 377,
	[BNXT_ULP_ACT_HID_4b78] = 378,
	[BNXT_ULP_ACT_HID_0bf4] = 379,
	[BNXT_ULP_ACT_HID_3f04] = 380,
	[BNXT_ULP_ACT_HID_1d04] = 381,
	[BNXT_ULP_ACT_HID_0768] = 382,
	[BNXT_ULP_ACT_HID_3a78] = 383,
	[BNXT_ULP_ACT_HID_4b88] = 384,
	[BNXT_ULP_ACT_HID_46f4] = 385,
	[BNXT_ULP_ACT_HID_24f4] = 386,
	[BNXT_ULP_ACT_HID_0f58] = 387,
	[BNXT_ULP_ACT_HID_13e4] = 388,
	[BNXT_ULP_ACT_HID_4268] = 389,
	[BNXT_ULP_ACT_HID_5378] = 390,
	[BNXT_ULP_ACT_HID_13f4] = 391,
	[BNXT_ULP_ACT_HID_4704] = 392,
	[BNXT_ULP_ACT_HID_2504] = 393,
	[BNXT_ULP_ACT_HID_0f68] = 394,
	[BNXT_ULP_ACT_HID_4278] = 395,
	[BNXT_ULP_ACT_HID_5388] = 396,
	[BNXT_ULP_ACT_HID_1be4] = 397,
	[BNXT_ULP_ACT_HID_4ef4] = 398,
	[BNXT_ULP_ACT_HID_2cf4] = 399,
	[BNXT_ULP_ACT_HID_1758] = 400,
	[BNXT_ULP_ACT_HID_4a68] = 401,
	[BNXT_ULP_ACT_HID_5b78] = 402,
	[BNXT_ULP_ACT_HID_1bf4] = 403,
	[BNXT_ULP_ACT_HID_4f04] = 404,
	[BNXT_ULP_ACT_HID_2d04] = 405,
	[BNXT_ULP_ACT_HID_1768] = 406,
	[BNXT_ULP_ACT_HID_4a78] = 407,
	[BNXT_ULP_ACT_HID_5b88] = 408,
	[BNXT_ULP_ACT_HID_23e4] = 409,
	[BNXT_ULP_ACT_HID_56f4] = 410,
	[BNXT_ULP_ACT_HID_34f4] = 411,
	[BNXT_ULP_ACT_HID_1f58] = 412,
	[BNXT_ULP_ACT_HID_5268] = 413,
	[BNXT_ULP_ACT_HID_6378] = 414,
	[BNXT_ULP_ACT_HID_23f4] = 415,
	[BNXT_ULP_ACT_HID_5704] = 416,
	[BNXT_ULP_ACT_HID_3504] = 417,
	[BNXT_ULP_ACT_HID_1f68] = 418,
	[BNXT_ULP_ACT_HID_5278] = 419,
	[BNXT_ULP_ACT_HID_6388] = 420,
	[BNXT_ULP_ACT_HID_1c36] = 421,
	[BNXT_ULP_ACT_HID_2436] = 422,
	[BNXT_ULP_ACT_HID_2c36] = 423,
	[BNXT_ULP_ACT_HID_1c46] = 424,
	[BNXT_ULP_ACT_HID_2446] = 425,
	[BNXT_ULP_ACT_HID_2c46] = 426,
	[BNXT_ULP_ACT_HID_2546] = 427,
	[BNXT_ULP_ACT_HID_5856] = 428,
	[BNXT_ULP_ACT_HID_3656] = 429,
	[BNXT_ULP_ACT_HID_20ba] = 430,
	[BNXT_ULP_ACT_HID_53ca] = 431,
	[BNXT_ULP_ACT_HID_64da] = 432,
	[BNXT_ULP_ACT_HID_2d46] = 433,
	[BNXT_ULP_ACT_HID_6056] = 434,
	[BNXT_ULP_ACT_HID_3e56] = 435,
	[BNXT_ULP_ACT_HID_28ba] = 436,
	[BNXT_ULP_ACT_HID_5bca] = 437,
	[BNXT_ULP_ACT_HID_6cda] = 438,
	[BNXT_ULP_ACT_HID_3546] = 439,
	[BNXT_ULP_ACT_HID_6856] = 440,
	[BNXT_ULP_ACT_HID_4656] = 441,
	[BNXT_ULP_ACT_HID_30ba] = 442,
	[BNXT_ULP_ACT_HID_63ca] = 443,
	[BNXT_ULP_ACT_HID_74da] = 444,
	[BNXT_ULP_ACT_HID_3d46] = 445,
	[BNXT_ULP_ACT_HID_7056] = 446,
	[BNXT_ULP_ACT_HID_4e56] = 447,
	[BNXT_ULP_ACT_HID_38ba] = 448,
	[BNXT_ULP_ACT_HID_6bca] = 449,
	[BNXT_ULP_ACT_HID_011e] = 450,
	[BNXT_ULP_ACT_HID_2556] = 451,
	[BNXT_ULP_ACT_HID_5866] = 452,
	[BNXT_ULP_ACT_HID_3666] = 453,
	[BNXT_ULP_ACT_HID_20ca] = 454,
	[BNXT_ULP_ACT_HID_53da] = 455,
	[BNXT_ULP_ACT_HID_64ea] = 456,
	[BNXT_ULP_ACT_HID_2d56] = 457,
	[BNXT_ULP_ACT_HID_6066] = 458,
	[BNXT_ULP_ACT_HID_3e66] = 459,
	[BNXT_ULP_ACT_HID_28ca] = 460,
	[BNXT_ULP_ACT_HID_5bda] = 461,
	[BNXT_ULP_ACT_HID_6cea] = 462,
	[BNXT_ULP_ACT_HID_3556] = 463,
	[BNXT_ULP_ACT_HID_6866] = 464,
	[BNXT_ULP_ACT_HID_4666] = 465,
	[BNXT_ULP_ACT_HID_30ca] = 466,
	[BNXT_ULP_ACT_HID_63da] = 467,
	[BNXT_ULP_ACT_HID_74ea] = 468,
	[BNXT_ULP_ACT_HID_3d56] = 469,
	[BNXT_ULP_ACT_HID_7066] = 470,
	[BNXT_ULP_ACT_HID_4e66] = 471,
	[BNXT_ULP_ACT_HID_38ca] = 472,
	[BNXT_ULP_ACT_HID_6bda] = 473,
	[BNXT_ULP_ACT_HID_012e] = 474,
	[BNXT_ULP_ACT_HID_3ece] = 475,
	[BNXT_ULP_ACT_HID_71de] = 476,
	[BNXT_ULP_ACT_HID_4fde] = 477,
	[BNXT_ULP_ACT_HID_3a42] = 478,
	[BNXT_ULP_ACT_HID_6d52] = 479,
	[BNXT_ULP_ACT_HID_02a6] = 480,
	[BNXT_ULP_ACT_HID_3ede] = 481,
	[BNXT_ULP_ACT_HID_71ee] = 482,
	[BNXT_ULP_ACT_HID_4fee] = 483,
	[BNXT_ULP_ACT_HID_3a52] = 484,
	[BNXT_ULP_ACT_HID_6d62] = 485,
	[BNXT_ULP_ACT_HID_02b6] = 486,
	[BNXT_ULP_ACT_HID_79de] = 487,
	[BNXT_ULP_ACT_HID_57de] = 488,
	[BNXT_ULP_ACT_HID_4242] = 489,
	[BNXT_ULP_ACT_HID_46ce] = 490,
	[BNXT_ULP_ACT_HID_7552] = 491,
	[BNXT_ULP_ACT_HID_0aa6] = 492,
	[BNXT_ULP_ACT_HID_46de] = 493,
	[BNXT_ULP_ACT_HID_79ee] = 494,
	[BNXT_ULP_ACT_HID_57ee] = 495,
	[BNXT_ULP_ACT_HID_4252] = 496,
	[BNXT_ULP_ACT_HID_7562] = 497,
	[BNXT_ULP_ACT_HID_0ab6] = 498,
	[BNXT_ULP_ACT_HID_4ece] = 499,
	[BNXT_ULP_ACT_HID_0622] = 500,
	[BNXT_ULP_ACT_HID_5fde] = 501,
	[BNXT_ULP_ACT_HID_4a42] = 502,
	[BNXT_ULP_ACT_HID_0196] = 503,
	[BNXT_ULP_ACT_HID_12a6] = 504,
	[BNXT_ULP_ACT_HID_4ede] = 505,
	[BNXT_ULP_ACT_HID_0632] = 506,
	[BNXT_ULP_ACT_HID_5fee] = 507,
	[BNXT_ULP_ACT_HID_4a52] = 508,
	[BNXT_ULP_ACT_HID_01a6] = 509,
	[BNXT_ULP_ACT_HID_12b6] = 510,
	[BNXT_ULP_ACT_HID_56ce] = 511,
	[BNXT_ULP_ACT_HID_0e22] = 512,
	[BNXT_ULP_ACT_HID_67de] = 513,
	[BNXT_ULP_ACT_HID_5242] = 514,
	[BNXT_ULP_ACT_HID_0996] = 515,
	[BNXT_ULP_ACT_HID_1aa6] = 516,
	[BNXT_ULP_ACT_HID_56de] = 517,
	[BNXT_ULP_ACT_HID_0e32] = 518,
	[BNXT_ULP_ACT_HID_67ee] = 519,
	[BNXT_ULP_ACT_HID_5252] = 520,
	[BNXT_ULP_ACT_HID_09a6] = 521,
	[BNXT_ULP_ACT_HID_1ab6] = 522,
	[BNXT_ULP_ACT_HID_31d0] = 523,
	[BNXT_ULP_ACT_HID_31e0] = 524,
	[BNXT_ULP_ACT_HID_39d0] = 525,
	[BNXT_ULP_ACT_HID_39e0] = 526,
	[BNXT_ULP_ACT_HID_41d0] = 527,
	[BNXT_ULP_ACT_HID_41e0] = 528,
	[BNXT_ULP_ACT_HID_49d0] = 529,
	[BNXT_ULP_ACT_HID_49e0] = 530,
	[BNXT_ULP_ACT_HID_64ba] = 531,
	[BNXT_ULP_ACT_HID_64ca] = 532,
	[BNXT_ULP_ACT_HID_6cba] = 533,
	[BNXT_ULP_ACT_HID_6cca] = 534,
	[BNXT_ULP_ACT_HID_74ba] = 535,
	[BNXT_ULP_ACT_HID_74ca] = 536,
	[BNXT_ULP_ACT_HID_00fe] = 537,
	[BNXT_ULP_ACT_HID_010e] = 538,
	[BNXT_ULP_ACT_HID_331c] = 539,
	[BNXT_ULP_ACT_HID_332c] = 540,
	[BNXT_ULP_ACT_HID_6706] = 541,
	[BNXT_ULP_ACT_HID_6716] = 542,
	[BNXT_ULP_ACT_HID_1b6d] = 543,
	[BNXT_ULP_ACT_HID_1b7d] = 544,
	[BNXT_ULP_ACT_HID_641a] = 545
};

/* Array for the act matcher list */
struct bnxt_ulp_act_match_info ulp_act_match_list[] = {
	[1] = {
	.act_hid = BNXT_ULP_ACT_HID_0000,
	.act_pattern_id = 0,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[2] = {
	.act_hid = BNXT_ULP_ACT_HID_0008,
	.act_pattern_id = 1,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[3] = {
	.act_hid = BNXT_ULP_ACT_HID_2000,
	.act_pattern_id = 2,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[4] = {
	.act_hid = BNXT_ULP_ACT_HID_1988,
	.act_pattern_id = 3,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[5] = {
	.act_hid = BNXT_ULP_ACT_HID_0080,
	.act_pattern_id = 4,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[6] = {
	.act_hid = BNXT_ULP_ACT_HID_3988,
	.act_pattern_id = 5,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[7] = {
	.act_hid = BNXT_ULP_ACT_HID_1a08,
	.act_pattern_id = 6,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[8] = {
	.act_hid = BNXT_ULP_ACT_HID_0010,
	.act_pattern_id = 7,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[9] = {
	.act_hid = BNXT_ULP_ACT_HID_0040,
	.act_pattern_id = 8,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_METER |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[10] = {
	.act_hid = BNXT_ULP_ACT_HID_0050,
	.act_pattern_id = 9,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_METER |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[11] = {
	.act_hid = BNXT_ULP_ACT_HID_0018,
	.act_pattern_id = 10,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[12] = {
	.act_hid = BNXT_ULP_ACT_HID_2010,
	.act_pattern_id = 11,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[13] = {
	.act_hid = BNXT_ULP_ACT_HID_1998,
	.act_pattern_id = 12,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[14] = {
	.act_hid = BNXT_ULP_ACT_HID_0090,
	.act_pattern_id = 13,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[15] = {
	.act_hid = BNXT_ULP_ACT_HID_3998,
	.act_pattern_id = 14,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[16] = {
	.act_hid = BNXT_ULP_ACT_HID_1a18,
	.act_pattern_id = 15,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[17] = {
	.act_hid = BNXT_ULP_ACT_HID_32ea,
	.act_pattern_id = 16,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[18] = {
	.act_hid = BNXT_ULP_ACT_HID_32f2,
	.act_pattern_id = 17,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[19] = {
	.act_hid = BNXT_ULP_ACT_HID_52ea,
	.act_pattern_id = 18,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[20] = {
	.act_hid = BNXT_ULP_ACT_HID_4c72,
	.act_pattern_id = 19,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[21] = {
	.act_hid = BNXT_ULP_ACT_HID_336a,
	.act_pattern_id = 20,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[22] = {
	.act_hid = BNXT_ULP_ACT_HID_6c72,
	.act_pattern_id = 21,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[23] = {
	.act_hid = BNXT_ULP_ACT_HID_4cf2,
	.act_pattern_id = 22,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[24] = {
	.act_hid = BNXT_ULP_ACT_HID_32fa,
	.act_pattern_id = 23,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[25] = {
	.act_hid = BNXT_ULP_ACT_HID_3302,
	.act_pattern_id = 24,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[26] = {
	.act_hid = BNXT_ULP_ACT_HID_52fa,
	.act_pattern_id = 25,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[27] = {
	.act_hid = BNXT_ULP_ACT_HID_4c82,
	.act_pattern_id = 26,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[28] = {
	.act_hid = BNXT_ULP_ACT_HID_337a,
	.act_pattern_id = 27,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[29] = {
	.act_hid = BNXT_ULP_ACT_HID_6c82,
	.act_pattern_id = 28,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[30] = {
	.act_hid = BNXT_ULP_ACT_HID_4d02,
	.act_pattern_id = 29,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[31] = {
	.act_hid = BNXT_ULP_ACT_HID_0808,
	.act_pattern_id = 30,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[32] = {
	.act_hid = BNXT_ULP_ACT_HID_1008,
	.act_pattern_id = 31,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[33] = {
	.act_hid = BNXT_ULP_ACT_HID_1808,
	.act_pattern_id = 32,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[34] = {
	.act_hid = BNXT_ULP_ACT_HID_0818,
	.act_pattern_id = 33,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[35] = {
	.act_hid = BNXT_ULP_ACT_HID_1018,
	.act_pattern_id = 34,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[36] = {
	.act_hid = BNXT_ULP_ACT_HID_1818,
	.act_pattern_id = 35,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[37] = {
	.act_hid = BNXT_ULP_ACT_HID_0880,
	.act_pattern_id = 36,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[38] = {
	.act_hid = BNXT_ULP_ACT_HID_1080,
	.act_pattern_id = 37,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[39] = {
	.act_hid = BNXT_ULP_ACT_HID_1880,
	.act_pattern_id = 38,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[40] = {
	.act_hid = BNXT_ULP_ACT_HID_0890,
	.act_pattern_id = 39,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[41] = {
	.act_hid = BNXT_ULP_ACT_HID_1090,
	.act_pattern_id = 40,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[42] = {
	.act_hid = BNXT_ULP_ACT_HID_1890,
	.act_pattern_id = 41,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[43] = {
	.act_hid = BNXT_ULP_ACT_HID_3af2,
	.act_pattern_id = 42,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[44] = {
	.act_hid = BNXT_ULP_ACT_HID_42f2,
	.act_pattern_id = 43,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[45] = {
	.act_hid = BNXT_ULP_ACT_HID_4af2,
	.act_pattern_id = 44,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[46] = {
	.act_hid = BNXT_ULP_ACT_HID_3b02,
	.act_pattern_id = 45,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[47] = {
	.act_hid = BNXT_ULP_ACT_HID_4302,
	.act_pattern_id = 46,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[48] = {
	.act_hid = BNXT_ULP_ACT_HID_4b02,
	.act_pattern_id = 47,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[49] = {
	.act_hid = BNXT_ULP_ACT_HID_3b6a,
	.act_pattern_id = 48,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[50] = {
	.act_hid = BNXT_ULP_ACT_HID_436a,
	.act_pattern_id = 49,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[51] = {
	.act_hid = BNXT_ULP_ACT_HID_4b6a,
	.act_pattern_id = 50,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[52] = {
	.act_hid = BNXT_ULP_ACT_HID_3b7a,
	.act_pattern_id = 51,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[53] = {
	.act_hid = BNXT_ULP_ACT_HID_437a,
	.act_pattern_id = 52,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[54] = {
	.act_hid = BNXT_ULP_ACT_HID_4b7a,
	.act_pattern_id = 53,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_VXLAN_DECAP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[55] = {
	.act_hid = BNXT_ULP_ACT_HID_640d,
	.act_pattern_id = 0,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED |
		BNXT_ULP_ACT_BIT_SAMPLE |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 2
	},
	[56] = {
	.act_hid = BNXT_ULP_ACT_HID_641d,
	.act_pattern_id = 1,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED |
		BNXT_ULP_ACT_BIT_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 2
	},
	[57] = {
	.act_hid = BNXT_ULP_ACT_HID_071a,
	.act_pattern_id = 2,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DELETE |
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 2
	},
	[58] = {
	.act_hid = BNXT_ULP_ACT_HID_0800,
	.act_pattern_id = 0,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[59] = {
	.act_hid = BNXT_ULP_ACT_HID_1000,
	.act_pattern_id = 1,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[60] = {
	.act_hid = BNXT_ULP_ACT_HID_1800,
	.act_pattern_id = 2,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[61] = {
	.act_hid = BNXT_ULP_ACT_HID_0810,
	.act_pattern_id = 3,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[62] = {
	.act_hid = BNXT_ULP_ACT_HID_1010,
	.act_pattern_id = 4,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[63] = {
	.act_hid = BNXT_ULP_ACT_HID_1810,
	.act_pattern_id = 5,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[64] = {
	.act_hid = BNXT_ULP_ACT_HID_1110,
	.act_pattern_id = 6,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[65] = {
	.act_hid = BNXT_ULP_ACT_HID_4420,
	.act_pattern_id = 7,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[66] = {
	.act_hid = BNXT_ULP_ACT_HID_2220,
	.act_pattern_id = 8,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[67] = {
	.act_hid = BNXT_ULP_ACT_HID_0c84,
	.act_pattern_id = 9,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[68] = {
	.act_hid = BNXT_ULP_ACT_HID_3f94,
	.act_pattern_id = 10,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[69] = {
	.act_hid = BNXT_ULP_ACT_HID_3330,
	.act_pattern_id = 11,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[70] = {
	.act_hid = BNXT_ULP_ACT_HID_50a4,
	.act_pattern_id = 12,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[71] = {
	.act_hid = BNXT_ULP_ACT_HID_1910,
	.act_pattern_id = 13,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[72] = {
	.act_hid = BNXT_ULP_ACT_HID_4c20,
	.act_pattern_id = 14,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[73] = {
	.act_hid = BNXT_ULP_ACT_HID_2a20,
	.act_pattern_id = 15,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[74] = {
	.act_hid = BNXT_ULP_ACT_HID_1484,
	.act_pattern_id = 16,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[75] = {
	.act_hid = BNXT_ULP_ACT_HID_4794,
	.act_pattern_id = 17,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[76] = {
	.act_hid = BNXT_ULP_ACT_HID_3b30,
	.act_pattern_id = 18,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[77] = {
	.act_hid = BNXT_ULP_ACT_HID_58a4,
	.act_pattern_id = 19,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[78] = {
	.act_hid = BNXT_ULP_ACT_HID_2110,
	.act_pattern_id = 20,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[79] = {
	.act_hid = BNXT_ULP_ACT_HID_5420,
	.act_pattern_id = 21,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[80] = {
	.act_hid = BNXT_ULP_ACT_HID_3220,
	.act_pattern_id = 22,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[81] = {
	.act_hid = BNXT_ULP_ACT_HID_1c84,
	.act_pattern_id = 23,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[82] = {
	.act_hid = BNXT_ULP_ACT_HID_4f94,
	.act_pattern_id = 24,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[83] = {
	.act_hid = BNXT_ULP_ACT_HID_4330,
	.act_pattern_id = 25,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[84] = {
	.act_hid = BNXT_ULP_ACT_HID_60a4,
	.act_pattern_id = 26,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[85] = {
	.act_hid = BNXT_ULP_ACT_HID_2910,
	.act_pattern_id = 27,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[86] = {
	.act_hid = BNXT_ULP_ACT_HID_5c20,
	.act_pattern_id = 28,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[87] = {
	.act_hid = BNXT_ULP_ACT_HID_3a20,
	.act_pattern_id = 29,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[88] = {
	.act_hid = BNXT_ULP_ACT_HID_2484,
	.act_pattern_id = 30,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[89] = {
	.act_hid = BNXT_ULP_ACT_HID_5794,
	.act_pattern_id = 31,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[90] = {
	.act_hid = BNXT_ULP_ACT_HID_4b30,
	.act_pattern_id = 32,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[91] = {
	.act_hid = BNXT_ULP_ACT_HID_68a4,
	.act_pattern_id = 33,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[92] = {
	.act_hid = BNXT_ULP_ACT_HID_1120,
	.act_pattern_id = 34,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[93] = {
	.act_hid = BNXT_ULP_ACT_HID_4430,
	.act_pattern_id = 35,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[94] = {
	.act_hid = BNXT_ULP_ACT_HID_2230,
	.act_pattern_id = 36,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[95] = {
	.act_hid = BNXT_ULP_ACT_HID_0c94,
	.act_pattern_id = 37,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[96] = {
	.act_hid = BNXT_ULP_ACT_HID_3fa4,
	.act_pattern_id = 38,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[97] = {
	.act_hid = BNXT_ULP_ACT_HID_3340,
	.act_pattern_id = 39,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[98] = {
	.act_hid = BNXT_ULP_ACT_HID_50b4,
	.act_pattern_id = 40,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[99] = {
	.act_hid = BNXT_ULP_ACT_HID_1920,
	.act_pattern_id = 41,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[100] = {
	.act_hid = BNXT_ULP_ACT_HID_4c30,
	.act_pattern_id = 42,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[101] = {
	.act_hid = BNXT_ULP_ACT_HID_2a30,
	.act_pattern_id = 43,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[102] = {
	.act_hid = BNXT_ULP_ACT_HID_1494,
	.act_pattern_id = 44,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[103] = {
	.act_hid = BNXT_ULP_ACT_HID_47a4,
	.act_pattern_id = 45,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[104] = {
	.act_hid = BNXT_ULP_ACT_HID_3b40,
	.act_pattern_id = 46,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[105] = {
	.act_hid = BNXT_ULP_ACT_HID_58b4,
	.act_pattern_id = 47,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[106] = {
	.act_hid = BNXT_ULP_ACT_HID_2120,
	.act_pattern_id = 48,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[107] = {
	.act_hid = BNXT_ULP_ACT_HID_5430,
	.act_pattern_id = 49,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[108] = {
	.act_hid = BNXT_ULP_ACT_HID_3230,
	.act_pattern_id = 50,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[109] = {
	.act_hid = BNXT_ULP_ACT_HID_1c94,
	.act_pattern_id = 51,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[110] = {
	.act_hid = BNXT_ULP_ACT_HID_4fa4,
	.act_pattern_id = 52,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[111] = {
	.act_hid = BNXT_ULP_ACT_HID_4340,
	.act_pattern_id = 53,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[112] = {
	.act_hid = BNXT_ULP_ACT_HID_60b4,
	.act_pattern_id = 54,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[113] = {
	.act_hid = BNXT_ULP_ACT_HID_2920,
	.act_pattern_id = 55,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[114] = {
	.act_hid = BNXT_ULP_ACT_HID_5c30,
	.act_pattern_id = 56,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[115] = {
	.act_hid = BNXT_ULP_ACT_HID_3a30,
	.act_pattern_id = 57,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[116] = {
	.act_hid = BNXT_ULP_ACT_HID_2494,
	.act_pattern_id = 58,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[117] = {
	.act_hid = BNXT_ULP_ACT_HID_57a4,
	.act_pattern_id = 59,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[118] = {
	.act_hid = BNXT_ULP_ACT_HID_4b40,
	.act_pattern_id = 60,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[119] = {
	.act_hid = BNXT_ULP_ACT_HID_68b4,
	.act_pattern_id = 61,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[120] = {
	.act_hid = BNXT_ULP_ACT_HID_2a98,
	.act_pattern_id = 62,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[121] = {
	.act_hid = BNXT_ULP_ACT_HID_5da8,
	.act_pattern_id = 63,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[122] = {
	.act_hid = BNXT_ULP_ACT_HID_3ba8,
	.act_pattern_id = 64,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[123] = {
	.act_hid = BNXT_ULP_ACT_HID_260c,
	.act_pattern_id = 65,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[124] = {
	.act_hid = BNXT_ULP_ACT_HID_591c,
	.act_pattern_id = 66,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[125] = {
	.act_hid = BNXT_ULP_ACT_HID_6a2c,
	.act_pattern_id = 67,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[126] = {
	.act_hid = BNXT_ULP_ACT_HID_2aa8,
	.act_pattern_id = 68,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[127] = {
	.act_hid = BNXT_ULP_ACT_HID_5db8,
	.act_pattern_id = 69,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[128] = {
	.act_hid = BNXT_ULP_ACT_HID_3bb8,
	.act_pattern_id = 70,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[129] = {
	.act_hid = BNXT_ULP_ACT_HID_261c,
	.act_pattern_id = 71,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[130] = {
	.act_hid = BNXT_ULP_ACT_HID_592c,
	.act_pattern_id = 72,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[131] = {
	.act_hid = BNXT_ULP_ACT_HID_6a3c,
	.act_pattern_id = 73,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[132] = {
	.act_hid = BNXT_ULP_ACT_HID_3298,
	.act_pattern_id = 74,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[133] = {
	.act_hid = BNXT_ULP_ACT_HID_65a8,
	.act_pattern_id = 75,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[134] = {
	.act_hid = BNXT_ULP_ACT_HID_43a8,
	.act_pattern_id = 76,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[135] = {
	.act_hid = BNXT_ULP_ACT_HID_2e0c,
	.act_pattern_id = 77,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[136] = {
	.act_hid = BNXT_ULP_ACT_HID_611c,
	.act_pattern_id = 78,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[137] = {
	.act_hid = BNXT_ULP_ACT_HID_722c,
	.act_pattern_id = 79,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[138] = {
	.act_hid = BNXT_ULP_ACT_HID_32a8,
	.act_pattern_id = 80,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[139] = {
	.act_hid = BNXT_ULP_ACT_HID_65b8,
	.act_pattern_id = 81,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[140] = {
	.act_hid = BNXT_ULP_ACT_HID_43b8,
	.act_pattern_id = 82,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[141] = {
	.act_hid = BNXT_ULP_ACT_HID_2e1c,
	.act_pattern_id = 83,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[142] = {
	.act_hid = BNXT_ULP_ACT_HID_612c,
	.act_pattern_id = 84,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[143] = {
	.act_hid = BNXT_ULP_ACT_HID_723c,
	.act_pattern_id = 85,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[144] = {
	.act_hid = BNXT_ULP_ACT_HID_3a98,
	.act_pattern_id = 86,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[145] = {
	.act_hid = BNXT_ULP_ACT_HID_6da8,
	.act_pattern_id = 87,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[146] = {
	.act_hid = BNXT_ULP_ACT_HID_4ba8,
	.act_pattern_id = 88,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[147] = {
	.act_hid = BNXT_ULP_ACT_HID_360c,
	.act_pattern_id = 89,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[148] = {
	.act_hid = BNXT_ULP_ACT_HID_691c,
	.act_pattern_id = 90,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[149] = {
	.act_hid = BNXT_ULP_ACT_HID_7a2c,
	.act_pattern_id = 91,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[150] = {
	.act_hid = BNXT_ULP_ACT_HID_3aa8,
	.act_pattern_id = 92,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[151] = {
	.act_hid = BNXT_ULP_ACT_HID_6db8,
	.act_pattern_id = 93,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[152] = {
	.act_hid = BNXT_ULP_ACT_HID_4bb8,
	.act_pattern_id = 94,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[153] = {
	.act_hid = BNXT_ULP_ACT_HID_361c,
	.act_pattern_id = 95,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[154] = {
	.act_hid = BNXT_ULP_ACT_HID_692c,
	.act_pattern_id = 96,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[155] = {
	.act_hid = BNXT_ULP_ACT_HID_7a3c,
	.act_pattern_id = 97,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[156] = {
	.act_hid = BNXT_ULP_ACT_HID_4298,
	.act_pattern_id = 98,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[157] = {
	.act_hid = BNXT_ULP_ACT_HID_75a8,
	.act_pattern_id = 99,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[158] = {
	.act_hid = BNXT_ULP_ACT_HID_53a8,
	.act_pattern_id = 100,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[159] = {
	.act_hid = BNXT_ULP_ACT_HID_3e0c,
	.act_pattern_id = 101,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[160] = {
	.act_hid = BNXT_ULP_ACT_HID_711c,
	.act_pattern_id = 102,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[161] = {
	.act_hid = BNXT_ULP_ACT_HID_0670,
	.act_pattern_id = 103,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[162] = {
	.act_hid = BNXT_ULP_ACT_HID_42a8,
	.act_pattern_id = 104,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[163] = {
	.act_hid = BNXT_ULP_ACT_HID_75b8,
	.act_pattern_id = 105,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[164] = {
	.act_hid = BNXT_ULP_ACT_HID_53b8,
	.act_pattern_id = 106,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[165] = {
	.act_hid = BNXT_ULP_ACT_HID_3e1c,
	.act_pattern_id = 107,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[166] = {
	.act_hid = BNXT_ULP_ACT_HID_712c,
	.act_pattern_id = 108,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[167] = {
	.act_hid = BNXT_ULP_ACT_HID_0680,
	.act_pattern_id = 109,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[168] = {
	.act_hid = BNXT_ULP_ACT_HID_3aea,
	.act_pattern_id = 110,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[169] = {
	.act_hid = BNXT_ULP_ACT_HID_42ea,
	.act_pattern_id = 111,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[170] = {
	.act_hid = BNXT_ULP_ACT_HID_4aea,
	.act_pattern_id = 112,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[171] = {
	.act_hid = BNXT_ULP_ACT_HID_3afa,
	.act_pattern_id = 113,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[172] = {
	.act_hid = BNXT_ULP_ACT_HID_42fa,
	.act_pattern_id = 114,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[173] = {
	.act_hid = BNXT_ULP_ACT_HID_4afa,
	.act_pattern_id = 115,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[174] = {
	.act_hid = BNXT_ULP_ACT_HID_43fa,
	.act_pattern_id = 116,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[175] = {
	.act_hid = BNXT_ULP_ACT_HID_770a,
	.act_pattern_id = 117,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[176] = {
	.act_hid = BNXT_ULP_ACT_HID_550a,
	.act_pattern_id = 118,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[177] = {
	.act_hid = BNXT_ULP_ACT_HID_3f6e,
	.act_pattern_id = 119,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[178] = {
	.act_hid = BNXT_ULP_ACT_HID_727e,
	.act_pattern_id = 120,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[179] = {
	.act_hid = BNXT_ULP_ACT_HID_661a,
	.act_pattern_id = 121,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[180] = {
	.act_hid = BNXT_ULP_ACT_HID_07d2,
	.act_pattern_id = 122,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[181] = {
	.act_hid = BNXT_ULP_ACT_HID_4bfa,
	.act_pattern_id = 123,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[182] = {
	.act_hid = BNXT_ULP_ACT_HID_034e,
	.act_pattern_id = 124,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[183] = {
	.act_hid = BNXT_ULP_ACT_HID_5d0a,
	.act_pattern_id = 125,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[184] = {
	.act_hid = BNXT_ULP_ACT_HID_476e,
	.act_pattern_id = 126,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[185] = {
	.act_hid = BNXT_ULP_ACT_HID_7a7e,
	.act_pattern_id = 127,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[186] = {
	.act_hid = BNXT_ULP_ACT_HID_6e1a,
	.act_pattern_id = 128,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[187] = {
	.act_hid = BNXT_ULP_ACT_HID_0fd2,
	.act_pattern_id = 129,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[188] = {
	.act_hid = BNXT_ULP_ACT_HID_53fa,
	.act_pattern_id = 130,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[189] = {
	.act_hid = BNXT_ULP_ACT_HID_0b4e,
	.act_pattern_id = 131,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[190] = {
	.act_hid = BNXT_ULP_ACT_HID_650a,
	.act_pattern_id = 132,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[191] = {
	.act_hid = BNXT_ULP_ACT_HID_4f6e,
	.act_pattern_id = 133,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[192] = {
	.act_hid = BNXT_ULP_ACT_HID_06c2,
	.act_pattern_id = 134,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[193] = {
	.act_hid = BNXT_ULP_ACT_HID_761a,
	.act_pattern_id = 135,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[194] = {
	.act_hid = BNXT_ULP_ACT_HID_17d2,
	.act_pattern_id = 136,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[195] = {
	.act_hid = BNXT_ULP_ACT_HID_5bfa,
	.act_pattern_id = 137,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[196] = {
	.act_hid = BNXT_ULP_ACT_HID_134e,
	.act_pattern_id = 138,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[197] = {
	.act_hid = BNXT_ULP_ACT_HID_6d0a,
	.act_pattern_id = 139,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[198] = {
	.act_hid = BNXT_ULP_ACT_HID_576e,
	.act_pattern_id = 140,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[199] = {
	.act_hid = BNXT_ULP_ACT_HID_0ec2,
	.act_pattern_id = 141,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[200] = {
	.act_hid = BNXT_ULP_ACT_HID_025e,
	.act_pattern_id = 142,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[201] = {
	.act_hid = BNXT_ULP_ACT_HID_1fd2,
	.act_pattern_id = 143,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[202] = {
	.act_hid = BNXT_ULP_ACT_HID_440a,
	.act_pattern_id = 144,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[203] = {
	.act_hid = BNXT_ULP_ACT_HID_771a,
	.act_pattern_id = 145,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[204] = {
	.act_hid = BNXT_ULP_ACT_HID_551a,
	.act_pattern_id = 146,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[205] = {
	.act_hid = BNXT_ULP_ACT_HID_3f7e,
	.act_pattern_id = 147,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[206] = {
	.act_hid = BNXT_ULP_ACT_HID_728e,
	.act_pattern_id = 148,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[207] = {
	.act_hid = BNXT_ULP_ACT_HID_662a,
	.act_pattern_id = 149,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[208] = {
	.act_hid = BNXT_ULP_ACT_HID_07e2,
	.act_pattern_id = 150,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[209] = {
	.act_hid = BNXT_ULP_ACT_HID_4c0a,
	.act_pattern_id = 151,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[210] = {
	.act_hid = BNXT_ULP_ACT_HID_035e,
	.act_pattern_id = 152,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[211] = {
	.act_hid = BNXT_ULP_ACT_HID_5d1a,
	.act_pattern_id = 153,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[212] = {
	.act_hid = BNXT_ULP_ACT_HID_477e,
	.act_pattern_id = 154,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[213] = {
	.act_hid = BNXT_ULP_ACT_HID_7a8e,
	.act_pattern_id = 155,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[214] = {
	.act_hid = BNXT_ULP_ACT_HID_6e2a,
	.act_pattern_id = 156,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[215] = {
	.act_hid = BNXT_ULP_ACT_HID_0fe2,
	.act_pattern_id = 157,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[216] = {
	.act_hid = BNXT_ULP_ACT_HID_540a,
	.act_pattern_id = 158,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[217] = {
	.act_hid = BNXT_ULP_ACT_HID_0b5e,
	.act_pattern_id = 159,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[218] = {
	.act_hid = BNXT_ULP_ACT_HID_651a,
	.act_pattern_id = 160,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[219] = {
	.act_hid = BNXT_ULP_ACT_HID_4f7e,
	.act_pattern_id = 161,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[220] = {
	.act_hid = BNXT_ULP_ACT_HID_06d2,
	.act_pattern_id = 162,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[221] = {
	.act_hid = BNXT_ULP_ACT_HID_762a,
	.act_pattern_id = 163,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[222] = {
	.act_hid = BNXT_ULP_ACT_HID_17e2,
	.act_pattern_id = 164,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[223] = {
	.act_hid = BNXT_ULP_ACT_HID_5c0a,
	.act_pattern_id = 165,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[224] = {
	.act_hid = BNXT_ULP_ACT_HID_135e,
	.act_pattern_id = 166,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[225] = {
	.act_hid = BNXT_ULP_ACT_HID_6d1a,
	.act_pattern_id = 167,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[226] = {
	.act_hid = BNXT_ULP_ACT_HID_577e,
	.act_pattern_id = 168,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[227] = {
	.act_hid = BNXT_ULP_ACT_HID_0ed2,
	.act_pattern_id = 169,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[228] = {
	.act_hid = BNXT_ULP_ACT_HID_026e,
	.act_pattern_id = 170,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[229] = {
	.act_hid = BNXT_ULP_ACT_HID_1fe2,
	.act_pattern_id = 171,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[230] = {
	.act_hid = BNXT_ULP_ACT_HID_5d82,
	.act_pattern_id = 172,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[231] = {
	.act_hid = BNXT_ULP_ACT_HID_14d6,
	.act_pattern_id = 173,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[232] = {
	.act_hid = BNXT_ULP_ACT_HID_6e92,
	.act_pattern_id = 174,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[233] = {
	.act_hid = BNXT_ULP_ACT_HID_58f6,
	.act_pattern_id = 175,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[234] = {
	.act_hid = BNXT_ULP_ACT_HID_104a,
	.act_pattern_id = 176,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[235] = {
	.act_hid = BNXT_ULP_ACT_HID_215a,
	.act_pattern_id = 177,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[236] = {
	.act_hid = BNXT_ULP_ACT_HID_5d92,
	.act_pattern_id = 178,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[237] = {
	.act_hid = BNXT_ULP_ACT_HID_14e6,
	.act_pattern_id = 179,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[238] = {
	.act_hid = BNXT_ULP_ACT_HID_6ea2,
	.act_pattern_id = 180,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[239] = {
	.act_hid = BNXT_ULP_ACT_HID_5906,
	.act_pattern_id = 181,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[240] = {
	.act_hid = BNXT_ULP_ACT_HID_105a,
	.act_pattern_id = 182,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[241] = {
	.act_hid = BNXT_ULP_ACT_HID_216a,
	.act_pattern_id = 183,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[242] = {
	.act_hid = BNXT_ULP_ACT_HID_6582,
	.act_pattern_id = 184,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[243] = {
	.act_hid = BNXT_ULP_ACT_HID_1cd6,
	.act_pattern_id = 185,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[244] = {
	.act_hid = BNXT_ULP_ACT_HID_7692,
	.act_pattern_id = 186,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[245] = {
	.act_hid = BNXT_ULP_ACT_HID_60f6,
	.act_pattern_id = 187,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[246] = {
	.act_hid = BNXT_ULP_ACT_HID_184a,
	.act_pattern_id = 188,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[247] = {
	.act_hid = BNXT_ULP_ACT_HID_295a,
	.act_pattern_id = 189,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[248] = {
	.act_hid = BNXT_ULP_ACT_HID_6592,
	.act_pattern_id = 190,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[249] = {
	.act_hid = BNXT_ULP_ACT_HID_1ce6,
	.act_pattern_id = 191,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[250] = {
	.act_hid = BNXT_ULP_ACT_HID_76a2,
	.act_pattern_id = 192,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[251] = {
	.act_hid = BNXT_ULP_ACT_HID_6106,
	.act_pattern_id = 193,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[252] = {
	.act_hid = BNXT_ULP_ACT_HID_185a,
	.act_pattern_id = 194,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[253] = {
	.act_hid = BNXT_ULP_ACT_HID_296a,
	.act_pattern_id = 195,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[254] = {
	.act_hid = BNXT_ULP_ACT_HID_6d82,
	.act_pattern_id = 196,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[255] = {
	.act_hid = BNXT_ULP_ACT_HID_24d6,
	.act_pattern_id = 197,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[256] = {
	.act_hid = BNXT_ULP_ACT_HID_02d6,
	.act_pattern_id = 198,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[257] = {
	.act_hid = BNXT_ULP_ACT_HID_68f6,
	.act_pattern_id = 199,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[258] = {
	.act_hid = BNXT_ULP_ACT_HID_204a,
	.act_pattern_id = 200,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[259] = {
	.act_hid = BNXT_ULP_ACT_HID_315a,
	.act_pattern_id = 201,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[260] = {
	.act_hid = BNXT_ULP_ACT_HID_6d92,
	.act_pattern_id = 202,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[261] = {
	.act_hid = BNXT_ULP_ACT_HID_24e6,
	.act_pattern_id = 203,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[262] = {
	.act_hid = BNXT_ULP_ACT_HID_02e6,
	.act_pattern_id = 204,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[263] = {
	.act_hid = BNXT_ULP_ACT_HID_6906,
	.act_pattern_id = 205,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[264] = {
	.act_hid = BNXT_ULP_ACT_HID_205a,
	.act_pattern_id = 206,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[265] = {
	.act_hid = BNXT_ULP_ACT_HID_316a,
	.act_pattern_id = 207,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[266] = {
	.act_hid = BNXT_ULP_ACT_HID_7582,
	.act_pattern_id = 208,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[267] = {
	.act_hid = BNXT_ULP_ACT_HID_2cd6,
	.act_pattern_id = 209,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[268] = {
	.act_hid = BNXT_ULP_ACT_HID_0ad6,
	.act_pattern_id = 210,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[269] = {
	.act_hid = BNXT_ULP_ACT_HID_70f6,
	.act_pattern_id = 211,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[270] = {
	.act_hid = BNXT_ULP_ACT_HID_284a,
	.act_pattern_id = 212,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[271] = {
	.act_hid = BNXT_ULP_ACT_HID_395a,
	.act_pattern_id = 213,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[272] = {
	.act_hid = BNXT_ULP_ACT_HID_7592,
	.act_pattern_id = 214,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[273] = {
	.act_hid = BNXT_ULP_ACT_HID_2ce6,
	.act_pattern_id = 215,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[274] = {
	.act_hid = BNXT_ULP_ACT_HID_0ae6,
	.act_pattern_id = 216,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[275] = {
	.act_hid = BNXT_ULP_ACT_HID_7106,
	.act_pattern_id = 217,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[276] = {
	.act_hid = BNXT_ULP_ACT_HID_285a,
	.act_pattern_id = 218,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[277] = {
	.act_hid = BNXT_ULP_ACT_HID_396a,
	.act_pattern_id = 219,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 3
	},
	[278] = {
	.act_hid = BNXT_ULP_ACT_HID_0020,
	.act_pattern_id = 0,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_RSS |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 4
	},
	[279] = {
	.act_hid = BNXT_ULP_ACT_HID_0030,
	.act_pattern_id = 1,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_RSS |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 4
	},
	[280] = {
	.act_hid = BNXT_ULP_ACT_HID_65d4,
	.act_pattern_id = 2,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_QUEUE |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 4
	},
	[281] = {
	.act_hid = BNXT_ULP_ACT_HID_65e4,
	.act_pattern_id = 3,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_QUEUE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 4
	},
	[282] = {
	.act_hid = BNXT_ULP_ACT_HID_330a,
	.act_pattern_id = 4,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_RSS |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 4
	},
	[283] = {
	.act_hid = BNXT_ULP_ACT_HID_331a,
	.act_pattern_id = 5,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_RSS |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 4
	},
	[284] = {
	.act_hid = BNXT_ULP_ACT_HID_1cfe,
	.act_pattern_id = 6,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_QUEUE |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 4
	},
	[285] = {
	.act_hid = BNXT_ULP_ACT_HID_1d0e,
	.act_pattern_id = 7,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_QUEUE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 4
	},
	[286] = {
	.act_hid = BNXT_ULP_ACT_HID_1474,
	.act_pattern_id = 0,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_METER_PROFILE |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 5
	},
	[287] = {
	.act_hid = BNXT_ULP_ACT_HID_4838,
	.act_pattern_id = 1,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_METER |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 5
	},
	[288] = {
	.act_hid = BNXT_ULP_ACT_HID_6458,
	.act_pattern_id = 2,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DELETE |
		BNXT_ULP_ACT_BIT_METER_PROFILE |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 5
	},
	[289] = {
	.act_hid = BNXT_ULP_ACT_HID_1c68,
	.act_pattern_id = 3,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DELETE |
		BNXT_ULP_ACT_BIT_SHARED_METER |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 5
	},
	[290] = {
	.act_hid = BNXT_ULP_ACT_HID_6c34,
	.act_pattern_id = 4,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_UPDATE |
		BNXT_ULP_ACT_BIT_SHARED_METER |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 5
	},
	[291] = {
	.act_hid = BNXT_ULP_ACT_HID_5d08,
	.act_pattern_id = 0,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[292] = {
	.act_hid = BNXT_ULP_ACT_HID_5d10,
	.act_pattern_id = 1,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[293] = {
	.act_hid = BNXT_ULP_ACT_HID_5d20,
	.act_pattern_id = 2,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[294] = {
	.act_hid = BNXT_ULP_ACT_HID_2e18,
	.act_pattern_id = 3,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_VLAN_PCP |
		BNXT_ULP_ACT_BIT_SET_VLAN_VID |
		BNXT_ULP_ACT_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[295] = {
	.act_hid = BNXT_ULP_ACT_HID_29d4,
	.act_pattern_id = 4,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_VLAN_VID |
		BNXT_ULP_ACT_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[296] = {
	.act_hid = BNXT_ULP_ACT_HID_7690,
	.act_pattern_id = 5,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[297] = {
	.act_hid = BNXT_ULP_ACT_HID_47a0,
	.act_pattern_id = 6,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_VLAN_PCP |
		BNXT_ULP_ACT_BIT_SET_VLAN_VID |
		BNXT_ULP_ACT_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[298] = {
	.act_hid = BNXT_ULP_ACT_HID_435c,
	.act_pattern_id = 7,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_VLAN_VID |
		BNXT_ULP_ACT_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[299] = {
	.act_hid = BNXT_ULP_ACT_HID_5d18,
	.act_pattern_id = 8,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[300] = {
	.act_hid = BNXT_ULP_ACT_HID_2e28,
	.act_pattern_id = 9,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_VLAN_PCP |
		BNXT_ULP_ACT_BIT_SET_VLAN_VID |
		BNXT_ULP_ACT_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[301] = {
	.act_hid = BNXT_ULP_ACT_HID_29e4,
	.act_pattern_id = 10,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_VLAN_VID |
		BNXT_ULP_ACT_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[302] = {
	.act_hid = BNXT_ULP_ACT_HID_76a0,
	.act_pattern_id = 11,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[303] = {
	.act_hid = BNXT_ULP_ACT_HID_47b0,
	.act_pattern_id = 12,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_VLAN_PCP |
		BNXT_ULP_ACT_BIT_SET_VLAN_VID |
		BNXT_ULP_ACT_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[304] = {
	.act_hid = BNXT_ULP_ACT_HID_436c,
	.act_pattern_id = 13,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_VLAN_VID |
		BNXT_ULP_ACT_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[305] = {
	.act_hid = BNXT_ULP_ACT_HID_1436,
	.act_pattern_id = 14,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[306] = {
	.act_hid = BNXT_ULP_ACT_HID_143e,
	.act_pattern_id = 15,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[307] = {
	.act_hid = BNXT_ULP_ACT_HID_144e,
	.act_pattern_id = 16,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DROP |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[308] = {
	.act_hid = BNXT_ULP_ACT_HID_6102,
	.act_pattern_id = 17,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_VLAN_PCP |
		BNXT_ULP_ACT_BIT_SET_VLAN_VID |
		BNXT_ULP_ACT_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[309] = {
	.act_hid = BNXT_ULP_ACT_HID_5cbe,
	.act_pattern_id = 18,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_VLAN_VID |
		BNXT_ULP_ACT_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[310] = {
	.act_hid = BNXT_ULP_ACT_HID_2dbe,
	.act_pattern_id = 19,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[311] = {
	.act_hid = BNXT_ULP_ACT_HID_7a8a,
	.act_pattern_id = 20,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_VLAN_PCP |
		BNXT_ULP_ACT_BIT_SET_VLAN_VID |
		BNXT_ULP_ACT_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[312] = {
	.act_hid = BNXT_ULP_ACT_HID_7646,
	.act_pattern_id = 21,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_VLAN_VID |
		BNXT_ULP_ACT_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[313] = {
	.act_hid = BNXT_ULP_ACT_HID_1446,
	.act_pattern_id = 22,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[314] = {
	.act_hid = BNXT_ULP_ACT_HID_6112,
	.act_pattern_id = 23,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_VLAN_PCP |
		BNXT_ULP_ACT_BIT_SET_VLAN_VID |
		BNXT_ULP_ACT_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[315] = {
	.act_hid = BNXT_ULP_ACT_HID_5cce,
	.act_pattern_id = 24,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_VLAN_VID |
		BNXT_ULP_ACT_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[316] = {
	.act_hid = BNXT_ULP_ACT_HID_2dce,
	.act_pattern_id = 25,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[317] = {
	.act_hid = BNXT_ULP_ACT_HID_7a9a,
	.act_pattern_id = 26,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_VLAN_PCP |
		BNXT_ULP_ACT_BIT_SET_VLAN_VID |
		BNXT_ULP_ACT_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[318] = {
	.act_hid = BNXT_ULP_ACT_HID_7656,
	.act_pattern_id = 27,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_VLAN_VID |
		BNXT_ULP_ACT_BIT_PUSH_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 6
	},
	[319] = {
	.act_hid = BNXT_ULP_ACT_HID_6508,
	.act_pattern_id = 0,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[320] = {
	.act_hid = BNXT_ULP_ACT_HID_6d08,
	.act_pattern_id = 1,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[321] = {
	.act_hid = BNXT_ULP_ACT_HID_7508,
	.act_pattern_id = 2,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[322] = {
	.act_hid = BNXT_ULP_ACT_HID_6518,
	.act_pattern_id = 3,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[323] = {
	.act_hid = BNXT_ULP_ACT_HID_6d18,
	.act_pattern_id = 4,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[324] = {
	.act_hid = BNXT_ULP_ACT_HID_7518,
	.act_pattern_id = 5,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[325] = {
	.act_hid = BNXT_ULP_ACT_HID_6e18,
	.act_pattern_id = 6,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[326] = {
	.act_hid = BNXT_ULP_ACT_HID_256c,
	.act_pattern_id = 7,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[327] = {
	.act_hid = BNXT_ULP_ACT_HID_036c,
	.act_pattern_id = 8,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[328] = {
	.act_hid = BNXT_ULP_ACT_HID_698c,
	.act_pattern_id = 9,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[329] = {
	.act_hid = BNXT_ULP_ACT_HID_20e0,
	.act_pattern_id = 10,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[330] = {
	.act_hid = BNXT_ULP_ACT_HID_31f0,
	.act_pattern_id = 11,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[331] = {
	.act_hid = BNXT_ULP_ACT_HID_7618,
	.act_pattern_id = 12,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[332] = {
	.act_hid = BNXT_ULP_ACT_HID_2d6c,
	.act_pattern_id = 13,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[333] = {
	.act_hid = BNXT_ULP_ACT_HID_0b6c,
	.act_pattern_id = 14,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[334] = {
	.act_hid = BNXT_ULP_ACT_HID_718c,
	.act_pattern_id = 15,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[335] = {
	.act_hid = BNXT_ULP_ACT_HID_28e0,
	.act_pattern_id = 16,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[336] = {
	.act_hid = BNXT_ULP_ACT_HID_39f0,
	.act_pattern_id = 17,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[337] = {
	.act_hid = BNXT_ULP_ACT_HID_025c,
	.act_pattern_id = 18,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[338] = {
	.act_hid = BNXT_ULP_ACT_HID_356c,
	.act_pattern_id = 19,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[339] = {
	.act_hid = BNXT_ULP_ACT_HID_136c,
	.act_pattern_id = 20,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[340] = {
	.act_hid = BNXT_ULP_ACT_HID_798c,
	.act_pattern_id = 21,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[341] = {
	.act_hid = BNXT_ULP_ACT_HID_30e0,
	.act_pattern_id = 22,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[342] = {
	.act_hid = BNXT_ULP_ACT_HID_41f0,
	.act_pattern_id = 23,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[343] = {
	.act_hid = BNXT_ULP_ACT_HID_0a5c,
	.act_pattern_id = 24,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[344] = {
	.act_hid = BNXT_ULP_ACT_HID_3d6c,
	.act_pattern_id = 25,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[345] = {
	.act_hid = BNXT_ULP_ACT_HID_1b6c,
	.act_pattern_id = 26,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[346] = {
	.act_hid = BNXT_ULP_ACT_HID_05d0,
	.act_pattern_id = 27,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[347] = {
	.act_hid = BNXT_ULP_ACT_HID_38e0,
	.act_pattern_id = 28,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[348] = {
	.act_hid = BNXT_ULP_ACT_HID_49f0,
	.act_pattern_id = 29,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[349] = {
	.act_hid = BNXT_ULP_ACT_HID_6e28,
	.act_pattern_id = 30,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[350] = {
	.act_hid = BNXT_ULP_ACT_HID_257c,
	.act_pattern_id = 31,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[351] = {
	.act_hid = BNXT_ULP_ACT_HID_037c,
	.act_pattern_id = 32,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[352] = {
	.act_hid = BNXT_ULP_ACT_HID_699c,
	.act_pattern_id = 33,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[353] = {
	.act_hid = BNXT_ULP_ACT_HID_20f0,
	.act_pattern_id = 34,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[354] = {
	.act_hid = BNXT_ULP_ACT_HID_3200,
	.act_pattern_id = 35,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[355] = {
	.act_hid = BNXT_ULP_ACT_HID_7628,
	.act_pattern_id = 36,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[356] = {
	.act_hid = BNXT_ULP_ACT_HID_2d7c,
	.act_pattern_id = 37,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[357] = {
	.act_hid = BNXT_ULP_ACT_HID_0b7c,
	.act_pattern_id = 38,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[358] = {
	.act_hid = BNXT_ULP_ACT_HID_719c,
	.act_pattern_id = 39,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[359] = {
	.act_hid = BNXT_ULP_ACT_HID_28f0,
	.act_pattern_id = 40,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[360] = {
	.act_hid = BNXT_ULP_ACT_HID_3a00,
	.act_pattern_id = 41,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[361] = {
	.act_hid = BNXT_ULP_ACT_HID_026c,
	.act_pattern_id = 42,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[362] = {
	.act_hid = BNXT_ULP_ACT_HID_357c,
	.act_pattern_id = 43,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[363] = {
	.act_hid = BNXT_ULP_ACT_HID_137c,
	.act_pattern_id = 44,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[364] = {
	.act_hid = BNXT_ULP_ACT_HID_799c,
	.act_pattern_id = 45,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[365] = {
	.act_hid = BNXT_ULP_ACT_HID_30f0,
	.act_pattern_id = 46,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[366] = {
	.act_hid = BNXT_ULP_ACT_HID_4200,
	.act_pattern_id = 47,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[367] = {
	.act_hid = BNXT_ULP_ACT_HID_0a6c,
	.act_pattern_id = 48,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[368] = {
	.act_hid = BNXT_ULP_ACT_HID_3d7c,
	.act_pattern_id = 49,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[369] = {
	.act_hid = BNXT_ULP_ACT_HID_1b7c,
	.act_pattern_id = 50,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[370] = {
	.act_hid = BNXT_ULP_ACT_HID_05e0,
	.act_pattern_id = 51,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[371] = {
	.act_hid = BNXT_ULP_ACT_HID_38f0,
	.act_pattern_id = 52,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[372] = {
	.act_hid = BNXT_ULP_ACT_HID_4a00,
	.act_pattern_id = 53,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[373] = {
	.act_hid = BNXT_ULP_ACT_HID_0be4,
	.act_pattern_id = 54,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[374] = {
	.act_hid = BNXT_ULP_ACT_HID_3ef4,
	.act_pattern_id = 55,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[375] = {
	.act_hid = BNXT_ULP_ACT_HID_1cf4,
	.act_pattern_id = 56,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[376] = {
	.act_hid = BNXT_ULP_ACT_HID_0758,
	.act_pattern_id = 57,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[377] = {
	.act_hid = BNXT_ULP_ACT_HID_3a68,
	.act_pattern_id = 58,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[378] = {
	.act_hid = BNXT_ULP_ACT_HID_4b78,
	.act_pattern_id = 59,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[379] = {
	.act_hid = BNXT_ULP_ACT_HID_0bf4,
	.act_pattern_id = 60,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[380] = {
	.act_hid = BNXT_ULP_ACT_HID_3f04,
	.act_pattern_id = 61,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[381] = {
	.act_hid = BNXT_ULP_ACT_HID_1d04,
	.act_pattern_id = 62,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[382] = {
	.act_hid = BNXT_ULP_ACT_HID_0768,
	.act_pattern_id = 63,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[383] = {
	.act_hid = BNXT_ULP_ACT_HID_3a78,
	.act_pattern_id = 64,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[384] = {
	.act_hid = BNXT_ULP_ACT_HID_4b88,
	.act_pattern_id = 65,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[385] = {
	.act_hid = BNXT_ULP_ACT_HID_46f4,
	.act_pattern_id = 66,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[386] = {
	.act_hid = BNXT_ULP_ACT_HID_24f4,
	.act_pattern_id = 67,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[387] = {
	.act_hid = BNXT_ULP_ACT_HID_0f58,
	.act_pattern_id = 68,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[388] = {
	.act_hid = BNXT_ULP_ACT_HID_13e4,
	.act_pattern_id = 69,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[389] = {
	.act_hid = BNXT_ULP_ACT_HID_4268,
	.act_pattern_id = 70,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[390] = {
	.act_hid = BNXT_ULP_ACT_HID_5378,
	.act_pattern_id = 71,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[391] = {
	.act_hid = BNXT_ULP_ACT_HID_13f4,
	.act_pattern_id = 72,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[392] = {
	.act_hid = BNXT_ULP_ACT_HID_4704,
	.act_pattern_id = 73,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[393] = {
	.act_hid = BNXT_ULP_ACT_HID_2504,
	.act_pattern_id = 74,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[394] = {
	.act_hid = BNXT_ULP_ACT_HID_0f68,
	.act_pattern_id = 75,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[395] = {
	.act_hid = BNXT_ULP_ACT_HID_4278,
	.act_pattern_id = 76,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[396] = {
	.act_hid = BNXT_ULP_ACT_HID_5388,
	.act_pattern_id = 77,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[397] = {
	.act_hid = BNXT_ULP_ACT_HID_1be4,
	.act_pattern_id = 78,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[398] = {
	.act_hid = BNXT_ULP_ACT_HID_4ef4,
	.act_pattern_id = 79,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[399] = {
	.act_hid = BNXT_ULP_ACT_HID_2cf4,
	.act_pattern_id = 80,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[400] = {
	.act_hid = BNXT_ULP_ACT_HID_1758,
	.act_pattern_id = 81,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[401] = {
	.act_hid = BNXT_ULP_ACT_HID_4a68,
	.act_pattern_id = 82,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[402] = {
	.act_hid = BNXT_ULP_ACT_HID_5b78,
	.act_pattern_id = 83,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[403] = {
	.act_hid = BNXT_ULP_ACT_HID_1bf4,
	.act_pattern_id = 84,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[404] = {
	.act_hid = BNXT_ULP_ACT_HID_4f04,
	.act_pattern_id = 85,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[405] = {
	.act_hid = BNXT_ULP_ACT_HID_2d04,
	.act_pattern_id = 86,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[406] = {
	.act_hid = BNXT_ULP_ACT_HID_1768,
	.act_pattern_id = 87,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[407] = {
	.act_hid = BNXT_ULP_ACT_HID_4a78,
	.act_pattern_id = 88,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[408] = {
	.act_hid = BNXT_ULP_ACT_HID_5b88,
	.act_pattern_id = 89,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[409] = {
	.act_hid = BNXT_ULP_ACT_HID_23e4,
	.act_pattern_id = 90,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[410] = {
	.act_hid = BNXT_ULP_ACT_HID_56f4,
	.act_pattern_id = 91,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[411] = {
	.act_hid = BNXT_ULP_ACT_HID_34f4,
	.act_pattern_id = 92,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[412] = {
	.act_hid = BNXT_ULP_ACT_HID_1f58,
	.act_pattern_id = 93,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[413] = {
	.act_hid = BNXT_ULP_ACT_HID_5268,
	.act_pattern_id = 94,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[414] = {
	.act_hid = BNXT_ULP_ACT_HID_6378,
	.act_pattern_id = 95,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[415] = {
	.act_hid = BNXT_ULP_ACT_HID_23f4,
	.act_pattern_id = 96,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[416] = {
	.act_hid = BNXT_ULP_ACT_HID_5704,
	.act_pattern_id = 97,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[417] = {
	.act_hid = BNXT_ULP_ACT_HID_3504,
	.act_pattern_id = 98,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[418] = {
	.act_hid = BNXT_ULP_ACT_HID_1f68,
	.act_pattern_id = 99,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[419] = {
	.act_hid = BNXT_ULP_ACT_HID_5278,
	.act_pattern_id = 100,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[420] = {
	.act_hid = BNXT_ULP_ACT_HID_6388,
	.act_pattern_id = 101,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[421] = {
	.act_hid = BNXT_ULP_ACT_HID_1c36,
	.act_pattern_id = 102,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[422] = {
	.act_hid = BNXT_ULP_ACT_HID_2436,
	.act_pattern_id = 103,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[423] = {
	.act_hid = BNXT_ULP_ACT_HID_2c36,
	.act_pattern_id = 104,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[424] = {
	.act_hid = BNXT_ULP_ACT_HID_1c46,
	.act_pattern_id = 105,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[425] = {
	.act_hid = BNXT_ULP_ACT_HID_2446,
	.act_pattern_id = 106,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[426] = {
	.act_hid = BNXT_ULP_ACT_HID_2c46,
	.act_pattern_id = 107,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[427] = {
	.act_hid = BNXT_ULP_ACT_HID_2546,
	.act_pattern_id = 108,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[428] = {
	.act_hid = BNXT_ULP_ACT_HID_5856,
	.act_pattern_id = 109,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[429] = {
	.act_hid = BNXT_ULP_ACT_HID_3656,
	.act_pattern_id = 110,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[430] = {
	.act_hid = BNXT_ULP_ACT_HID_20ba,
	.act_pattern_id = 111,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[431] = {
	.act_hid = BNXT_ULP_ACT_HID_53ca,
	.act_pattern_id = 112,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[432] = {
	.act_hid = BNXT_ULP_ACT_HID_64da,
	.act_pattern_id = 113,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[433] = {
	.act_hid = BNXT_ULP_ACT_HID_2d46,
	.act_pattern_id = 114,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[434] = {
	.act_hid = BNXT_ULP_ACT_HID_6056,
	.act_pattern_id = 115,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[435] = {
	.act_hid = BNXT_ULP_ACT_HID_3e56,
	.act_pattern_id = 116,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[436] = {
	.act_hid = BNXT_ULP_ACT_HID_28ba,
	.act_pattern_id = 117,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[437] = {
	.act_hid = BNXT_ULP_ACT_HID_5bca,
	.act_pattern_id = 118,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[438] = {
	.act_hid = BNXT_ULP_ACT_HID_6cda,
	.act_pattern_id = 119,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[439] = {
	.act_hid = BNXT_ULP_ACT_HID_3546,
	.act_pattern_id = 120,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[440] = {
	.act_hid = BNXT_ULP_ACT_HID_6856,
	.act_pattern_id = 121,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[441] = {
	.act_hid = BNXT_ULP_ACT_HID_4656,
	.act_pattern_id = 122,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[442] = {
	.act_hid = BNXT_ULP_ACT_HID_30ba,
	.act_pattern_id = 123,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[443] = {
	.act_hid = BNXT_ULP_ACT_HID_63ca,
	.act_pattern_id = 124,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[444] = {
	.act_hid = BNXT_ULP_ACT_HID_74da,
	.act_pattern_id = 125,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[445] = {
	.act_hid = BNXT_ULP_ACT_HID_3d46,
	.act_pattern_id = 126,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[446] = {
	.act_hid = BNXT_ULP_ACT_HID_7056,
	.act_pattern_id = 127,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[447] = {
	.act_hid = BNXT_ULP_ACT_HID_4e56,
	.act_pattern_id = 128,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[448] = {
	.act_hid = BNXT_ULP_ACT_HID_38ba,
	.act_pattern_id = 129,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[449] = {
	.act_hid = BNXT_ULP_ACT_HID_6bca,
	.act_pattern_id = 130,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[450] = {
	.act_hid = BNXT_ULP_ACT_HID_011e,
	.act_pattern_id = 131,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[451] = {
	.act_hid = BNXT_ULP_ACT_HID_2556,
	.act_pattern_id = 132,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[452] = {
	.act_hid = BNXT_ULP_ACT_HID_5866,
	.act_pattern_id = 133,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[453] = {
	.act_hid = BNXT_ULP_ACT_HID_3666,
	.act_pattern_id = 134,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[454] = {
	.act_hid = BNXT_ULP_ACT_HID_20ca,
	.act_pattern_id = 135,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[455] = {
	.act_hid = BNXT_ULP_ACT_HID_53da,
	.act_pattern_id = 136,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[456] = {
	.act_hid = BNXT_ULP_ACT_HID_64ea,
	.act_pattern_id = 137,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[457] = {
	.act_hid = BNXT_ULP_ACT_HID_2d56,
	.act_pattern_id = 138,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[458] = {
	.act_hid = BNXT_ULP_ACT_HID_6066,
	.act_pattern_id = 139,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[459] = {
	.act_hid = BNXT_ULP_ACT_HID_3e66,
	.act_pattern_id = 140,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[460] = {
	.act_hid = BNXT_ULP_ACT_HID_28ca,
	.act_pattern_id = 141,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[461] = {
	.act_hid = BNXT_ULP_ACT_HID_5bda,
	.act_pattern_id = 142,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[462] = {
	.act_hid = BNXT_ULP_ACT_HID_6cea,
	.act_pattern_id = 143,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[463] = {
	.act_hid = BNXT_ULP_ACT_HID_3556,
	.act_pattern_id = 144,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[464] = {
	.act_hid = BNXT_ULP_ACT_HID_6866,
	.act_pattern_id = 145,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[465] = {
	.act_hid = BNXT_ULP_ACT_HID_4666,
	.act_pattern_id = 146,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[466] = {
	.act_hid = BNXT_ULP_ACT_HID_30ca,
	.act_pattern_id = 147,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[467] = {
	.act_hid = BNXT_ULP_ACT_HID_63da,
	.act_pattern_id = 148,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[468] = {
	.act_hid = BNXT_ULP_ACT_HID_74ea,
	.act_pattern_id = 149,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[469] = {
	.act_hid = BNXT_ULP_ACT_HID_3d56,
	.act_pattern_id = 150,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[470] = {
	.act_hid = BNXT_ULP_ACT_HID_7066,
	.act_pattern_id = 151,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[471] = {
	.act_hid = BNXT_ULP_ACT_HID_4e66,
	.act_pattern_id = 152,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[472] = {
	.act_hid = BNXT_ULP_ACT_HID_38ca,
	.act_pattern_id = 153,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[473] = {
	.act_hid = BNXT_ULP_ACT_HID_6bda,
	.act_pattern_id = 154,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[474] = {
	.act_hid = BNXT_ULP_ACT_HID_012e,
	.act_pattern_id = 155,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[475] = {
	.act_hid = BNXT_ULP_ACT_HID_3ece,
	.act_pattern_id = 156,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[476] = {
	.act_hid = BNXT_ULP_ACT_HID_71de,
	.act_pattern_id = 157,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[477] = {
	.act_hid = BNXT_ULP_ACT_HID_4fde,
	.act_pattern_id = 158,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[478] = {
	.act_hid = BNXT_ULP_ACT_HID_3a42,
	.act_pattern_id = 159,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[479] = {
	.act_hid = BNXT_ULP_ACT_HID_6d52,
	.act_pattern_id = 160,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[480] = {
	.act_hid = BNXT_ULP_ACT_HID_02a6,
	.act_pattern_id = 161,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[481] = {
	.act_hid = BNXT_ULP_ACT_HID_3ede,
	.act_pattern_id = 162,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[482] = {
	.act_hid = BNXT_ULP_ACT_HID_71ee,
	.act_pattern_id = 163,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[483] = {
	.act_hid = BNXT_ULP_ACT_HID_4fee,
	.act_pattern_id = 164,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[484] = {
	.act_hid = BNXT_ULP_ACT_HID_3a52,
	.act_pattern_id = 165,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[485] = {
	.act_hid = BNXT_ULP_ACT_HID_6d62,
	.act_pattern_id = 166,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[486] = {
	.act_hid = BNXT_ULP_ACT_HID_02b6,
	.act_pattern_id = 167,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[487] = {
	.act_hid = BNXT_ULP_ACT_HID_79de,
	.act_pattern_id = 168,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[488] = {
	.act_hid = BNXT_ULP_ACT_HID_57de,
	.act_pattern_id = 169,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[489] = {
	.act_hid = BNXT_ULP_ACT_HID_4242,
	.act_pattern_id = 170,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[490] = {
	.act_hid = BNXT_ULP_ACT_HID_46ce,
	.act_pattern_id = 171,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[491] = {
	.act_hid = BNXT_ULP_ACT_HID_7552,
	.act_pattern_id = 172,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[492] = {
	.act_hid = BNXT_ULP_ACT_HID_0aa6,
	.act_pattern_id = 173,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[493] = {
	.act_hid = BNXT_ULP_ACT_HID_46de,
	.act_pattern_id = 174,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[494] = {
	.act_hid = BNXT_ULP_ACT_HID_79ee,
	.act_pattern_id = 175,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[495] = {
	.act_hid = BNXT_ULP_ACT_HID_57ee,
	.act_pattern_id = 176,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[496] = {
	.act_hid = BNXT_ULP_ACT_HID_4252,
	.act_pattern_id = 177,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[497] = {
	.act_hid = BNXT_ULP_ACT_HID_7562,
	.act_pattern_id = 178,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[498] = {
	.act_hid = BNXT_ULP_ACT_HID_0ab6,
	.act_pattern_id = 179,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[499] = {
	.act_hid = BNXT_ULP_ACT_HID_4ece,
	.act_pattern_id = 180,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[500] = {
	.act_hid = BNXT_ULP_ACT_HID_0622,
	.act_pattern_id = 181,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[501] = {
	.act_hid = BNXT_ULP_ACT_HID_5fde,
	.act_pattern_id = 182,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[502] = {
	.act_hid = BNXT_ULP_ACT_HID_4a42,
	.act_pattern_id = 183,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[503] = {
	.act_hid = BNXT_ULP_ACT_HID_0196,
	.act_pattern_id = 184,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[504] = {
	.act_hid = BNXT_ULP_ACT_HID_12a6,
	.act_pattern_id = 185,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[505] = {
	.act_hid = BNXT_ULP_ACT_HID_4ede,
	.act_pattern_id = 186,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[506] = {
	.act_hid = BNXT_ULP_ACT_HID_0632,
	.act_pattern_id = 187,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[507] = {
	.act_hid = BNXT_ULP_ACT_HID_5fee,
	.act_pattern_id = 188,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[508] = {
	.act_hid = BNXT_ULP_ACT_HID_4a52,
	.act_pattern_id = 189,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[509] = {
	.act_hid = BNXT_ULP_ACT_HID_01a6,
	.act_pattern_id = 190,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[510] = {
	.act_hid = BNXT_ULP_ACT_HID_12b6,
	.act_pattern_id = 191,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[511] = {
	.act_hid = BNXT_ULP_ACT_HID_56ce,
	.act_pattern_id = 192,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[512] = {
	.act_hid = BNXT_ULP_ACT_HID_0e22,
	.act_pattern_id = 193,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[513] = {
	.act_hid = BNXT_ULP_ACT_HID_67de,
	.act_pattern_id = 194,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[514] = {
	.act_hid = BNXT_ULP_ACT_HID_5242,
	.act_pattern_id = 195,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[515] = {
	.act_hid = BNXT_ULP_ACT_HID_0996,
	.act_pattern_id = 196,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[516] = {
	.act_hid = BNXT_ULP_ACT_HID_1aa6,
	.act_pattern_id = 197,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[517] = {
	.act_hid = BNXT_ULP_ACT_HID_56de,
	.act_pattern_id = 198,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[518] = {
	.act_hid = BNXT_ULP_ACT_HID_0e32,
	.act_pattern_id = 199,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[519] = {
	.act_hid = BNXT_ULP_ACT_HID_67ee,
	.act_pattern_id = 200,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[520] = {
	.act_hid = BNXT_ULP_ACT_HID_5252,
	.act_pattern_id = 201,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[521] = {
	.act_hid = BNXT_ULP_ACT_HID_09a6,
	.act_pattern_id = 202,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[522] = {
	.act_hid = BNXT_ULP_ACT_HID_1ab6,
	.act_pattern_id = 203,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_DEC_TTL |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_ACT_BIT_SET_IPV4_SRC |
		BNXT_ULP_ACT_BIT_SET_IPV4_DST |
		BNXT_ULP_ACT_BIT_SET_TP_SRC |
		BNXT_ULP_ACT_BIT_SET_TP_DST |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 7
	},
	[523] = {
	.act_hid = BNXT_ULP_ACT_HID_31d0,
	.act_pattern_id = 0,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_VXLAN_ENCAP |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 8
	},
	[524] = {
	.act_hid = BNXT_ULP_ACT_HID_31e0,
	.act_pattern_id = 1,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_VXLAN_ENCAP |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 8
	},
	[525] = {
	.act_hid = BNXT_ULP_ACT_HID_39d0,
	.act_pattern_id = 2,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_VXLAN_ENCAP |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 8
	},
	[526] = {
	.act_hid = BNXT_ULP_ACT_HID_39e0,
	.act_pattern_id = 3,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_VXLAN_ENCAP |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 8
	},
	[527] = {
	.act_hid = BNXT_ULP_ACT_HID_41d0,
	.act_pattern_id = 4,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_VXLAN_ENCAP |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 8
	},
	[528] = {
	.act_hid = BNXT_ULP_ACT_HID_41e0,
	.act_pattern_id = 5,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_VXLAN_ENCAP |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 8
	},
	[529] = {
	.act_hid = BNXT_ULP_ACT_HID_49d0,
	.act_pattern_id = 6,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_VXLAN_ENCAP |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 8
	},
	[530] = {
	.act_hid = BNXT_ULP_ACT_HID_49e0,
	.act_pattern_id = 7,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_VXLAN_ENCAP |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 8
	},
	[531] = {
	.act_hid = BNXT_ULP_ACT_HID_64ba,
	.act_pattern_id = 8,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_VXLAN_ENCAP |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 8
	},
	[532] = {
	.act_hid = BNXT_ULP_ACT_HID_64ca,
	.act_pattern_id = 9,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_VXLAN_ENCAP |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 8
	},
	[533] = {
	.act_hid = BNXT_ULP_ACT_HID_6cba,
	.act_pattern_id = 10,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_VXLAN_ENCAP |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 8
	},
	[534] = {
	.act_hid = BNXT_ULP_ACT_HID_6cca,
	.act_pattern_id = 11,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_VXLAN_ENCAP |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 8
	},
	[535] = {
	.act_hid = BNXT_ULP_ACT_HID_74ba,
	.act_pattern_id = 12,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_VXLAN_ENCAP |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 8
	},
	[536] = {
	.act_hid = BNXT_ULP_ACT_HID_74ca,
	.act_pattern_id = 13,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_VXLAN_ENCAP |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 8
	},
	[537] = {
	.act_hid = BNXT_ULP_ACT_HID_00fe,
	.act_pattern_id = 14,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_VXLAN_ENCAP |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 8
	},
	[538] = {
	.act_hid = BNXT_ULP_ACT_HID_010e,
	.act_pattern_id = 15,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_SET_MAC_SRC |
		BNXT_ULP_ACT_BIT_SET_MAC_DST |
		BNXT_ULP_ACT_BIT_VXLAN_ENCAP |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 8
	},
	[539] = {
	.act_hid = BNXT_ULP_ACT_HID_331c,
	.act_pattern_id = 0,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_VF_TO_VF |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 9
	},
	[540] = {
	.act_hid = BNXT_ULP_ACT_HID_332c,
	.act_pattern_id = 1,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_VF_TO_VF |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 9
	},
	[541] = {
	.act_hid = BNXT_ULP_ACT_HID_6706,
	.act_pattern_id = 2,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_VF_TO_VF |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 9
	},
	[542] = {
	.act_hid = BNXT_ULP_ACT_HID_6716,
	.act_pattern_id = 3,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_ACT_BIT_VF_TO_VF |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 9
	},
	[543] = {
	.act_hid = BNXT_ULP_ACT_HID_1b6d,
	.act_pattern_id = 0,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED |
		BNXT_ULP_ACT_BIT_SAMPLE |
		BNXT_ULP_ACT_BIT_VF_TO_VF |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 10
	},
	[544] = {
	.act_hid = BNXT_ULP_ACT_HID_1b7d,
	.act_pattern_id = 1,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_SHARED |
		BNXT_ULP_ACT_BIT_SAMPLE |
		BNXT_ULP_ACT_BIT_VF_TO_VF |
		BNXT_ULP_ACT_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 10
	},
	[545] = {
	.act_hid = BNXT_ULP_ACT_HID_641a,
	.act_pattern_id = 2,
	.app_sig = 0,
	.act_sig = { .bits =
		BNXT_ULP_ACT_BIT_DELETE |
		BNXT_ULP_ACT_BIT_SHARED_SAMPLE |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 10
	}
};
