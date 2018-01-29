/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright (c) 2009, Olivier MATZ <zer0@droids-corp.org>
 * All rights reserved.
 */

#ifndef _CMDLINE_VT100_H_
#define _CMDLINE_VT100_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define vt100_bell         "\007"
#define vt100_bs           "\010"
#define vt100_bs_clear     "\010 \010"
#define vt100_tab          "\011"
#define vt100_crnl         "\012\015"
#define vt100_clear_right  "\033[0K"
#define vt100_clear_left   "\033[1K"
#define vt100_clear_down   "\033[0J"
#define vt100_clear_up     "\033[1J"
#define vt100_clear_line   "\033[2K"
#define vt100_clear_screen "\033[2J"
#define vt100_up_arr       "\033\133\101"
#define vt100_down_arr     "\033\133\102"
#define vt100_right_arr    "\033\133\103"
#define vt100_left_arr     "\033\133\104"
#define vt100_multi_right  "\033\133%uC"
#define vt100_multi_left   "\033\133%uD"
#define vt100_suppr        "\033\133\063\176"
#define vt100_home         "\033M\033E"
#define vt100_word_left    "\033\142"
#define vt100_word_right   "\033\146"

/* Result of parsing : it must be synchronized with
 * cmdline_vt100_commands[] in vt100.c */
#define CMDLINE_KEY_UP_ARR 0
#define CMDLINE_KEY_DOWN_ARR 1
#define CMDLINE_KEY_RIGHT_ARR 2
#define CMDLINE_KEY_LEFT_ARR 3
#define CMDLINE_KEY_BKSPACE 4
#define CMDLINE_KEY_RETURN 5
#define CMDLINE_KEY_CTRL_A 6
#define CMDLINE_KEY_CTRL_E 7
#define CMDLINE_KEY_CTRL_K 8
#define CMDLINE_KEY_CTRL_Y 9
#define CMDLINE_KEY_CTRL_C 10
#define CMDLINE_KEY_CTRL_F 11
#define CMDLINE_KEY_CTRL_B 12
#define CMDLINE_KEY_SUPPR 13
#define CMDLINE_KEY_TAB 14
#define CMDLINE_KEY_CTRL_D 15
#define CMDLINE_KEY_CTRL_L 16
#define CMDLINE_KEY_RETURN2 17
#define CMDLINE_KEY_META_BKSPACE 18
#define CMDLINE_KEY_WLEFT 19
#define CMDLINE_KEY_WRIGHT 20
#define CMDLINE_KEY_HELP 21
#define CMDLINE_KEY_CTRL_W 22
#define CMDLINE_KEY_CTRL_P 23
#define CMDLINE_KEY_CTRL_N 24
#define CMDLINE_KEY_META_D 25
#define CMDLINE_KEY_BKSPACE2 26

extern const char *cmdline_vt100_commands[];

enum cmdline_vt100_parser_state {
	CMDLINE_VT100_INIT,
	CMDLINE_VT100_ESCAPE,
	CMDLINE_VT100_ESCAPE_CSI
};

#define CMDLINE_VT100_BUF_SIZE 8
struct cmdline_vt100 {
	uint8_t bufpos;
	char buf[CMDLINE_VT100_BUF_SIZE];
	enum cmdline_vt100_parser_state state;
};

/**
 * Init
 */
void vt100_init(struct cmdline_vt100 *vt);

/**
 * Input a new character.
 * Return -1 if the character is not part of a control sequence
 * Return -2 if c is not the last char of a control sequence
 * Else return the index in vt100_commands[]
 */
int vt100_parser(struct cmdline_vt100 *vt, char c);

#ifdef __cplusplus
}
#endif

#endif
