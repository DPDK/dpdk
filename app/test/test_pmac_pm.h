/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2013 Intel Corporation. All rights reserved.
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
 * 
 */

#ifndef TEST_PMAC_PM_H_
#define TEST_PMAC_PM_H_

/**
 * PATTERNS
 */

/*
 * clean ASCII patterns
 */
#define MAX_PATTERN_LEN 256
const char clean_patterns[][MAX_PATTERN_LEN] = {
		"Command completed",
		"Bad command or filename",
		"1 file(s) copied",
		"Invalid URL",
		"Index of /cgi-bin/",
		"HTTP/1.1 403",
		"subject=FrEaK+SERVER",
		"friend_nickname=",
		"/scripts/WWPMsg.dll",
		"body=JJ+BackDoor+-+v",

		"subject=Insurrection+Page",
		"backtrust.com",
		"User-Agent:",
		"fromemail=y3k",
		"www.kornputers.com",
		"# Nova CGI Notification Script",
		/* test NOT converting to binary */
		"|23|",
		/* test adding same pattern twice - should result in two matches */
		"fromemail=y3k",
};

/*
 * mixed ASCII-binary patterns
 */
const char mixed_patterns[][MAX_PATTERN_LEN] = {
		"1 file|28|s|29| copied",
		"User-Agent|3A|",
		"|23|",				/* gives # */
		"|7C 32 33 7C 00|",	/* gives |23| */
		/* test converter not converting stuff erroneously */
		"www.kornputers.com",
};

/*
 * P1 patterns
 * (P1 is used when having only one pattern)
 */
const char P1_patterns[][MAX_PATTERN_LEN] = {
		"1111aA111111",
};



/**
 * BUFFERS TO LOOK PATTERNS IN
 */
#define MAX_MATCH_COUNT 3

struct pm_test_buffer {
	/* test string */
	const char string[MAX_PATTERN_LEN];
	/* strings that should be found marching.
	 * for our test we allow no more than 3 matches.
	 * the number is completely arbitrary */
	const char * matched_str[MAX_MATCH_COUNT];
	/* number of matches with and without case sensitivity */
	uint8_t n_matches;
	uint8_t n_matches_with_case_sense;
};

struct pm_test_buffer clean_buffers[] = {
		{"abcCommand completedcde",
				{clean_patterns[0], NULL, NULL},
				1,	1},
		{"jsljelkwlefwe|23|igu5o0",
				{clean_patterns[16], NULL, NULL},
				1,	1},
		{"Invalid URLwww.kOrnpUterS.comfRiEnD_niCKname=",
				{clean_patterns[3], clean_patterns[14], clean_patterns[7]},
				3,	1},
		{"HTTP/1.1 404",
				{NULL, NULL, NULL},
				0,	0},
		{"abcfrOmemail=y3kcde",
				{clean_patterns[13], clean_patterns[17], NULL},
				2,	0},
		{"FANOUTsubject=FrEaK+SERVERFANOUT",
				{clean_patterns[6], NULL, NULL},
				1,	1},
		{"Bad command or filenam",
				{NULL, NULL, NULL},
				0,	0},
		{"Bad command or filename",
				{clean_patterns[1], NULL, NULL},
				1,	1},
		{"845hyut8hji51 FILE(S) COPIED934ui45",
				{clean_patterns[2], NULL, NULL},
				1,	0},
		{"HTTP/1.1 403IndEx of /cgi-bin/",
				{clean_patterns[5], clean_patterns[4], NULL},
				2,	1},
		{"mail.php?subject=Mail&body=JJ+BackDoor+-+v&id=2357874",
				{clean_patterns[9], NULL, NULL},
				1,	1},
		{"/var/www/site/scripts/WWPMsg.dll",
				{clean_patterns[8], NULL, NULL},
				1,	1},
		{"backtrust.com/mail.cgi?subject=Insurrection+Page&body=JJ+BackDoor+-+v",
				{clean_patterns[11], clean_patterns[10], clean_patterns[9]},
				3,	3},
		{"User-Agent: Mozilla/6.0 (Windows NT 6.2; WOW64; rv:16.0.1)",
				{clean_patterns[12], NULL, NULL},
				1,	1},
		{"User-agent: Mozilla/6.0 (Windows NT 6.2; WOW64; rv:16.0.1)",
				{clean_patterns[12], NULL, NULL},
				1,	0},
		{"http://www.kornputers.com/index.php",
				{clean_patterns[14], NULL, NULL},
				1,	1},
		{"\r\n# Nova CGI Notification Script",
				{clean_patterns[15], NULL, NULL},
				1,	1},
		{"\r\n# Nova CGI Notification Scrupt",
				{NULL, NULL, NULL},
				0,	0},
		{"User Agent: Mozilla/6.0 (Windows NT 6.2; WOW64; rv:16.0.1)",
				{NULL, NULL, NULL},
				0,	0},
		{"abcfromemail=y3dcde",
				{NULL, NULL, NULL},
				0,	0},
};

struct pm_test_buffer mixed_buffers[] = {
		{"jsljelkwlefwe|23|igu5o0",
				{mixed_patterns[3], NULL, NULL},
				1,	1},
		{"User-Agent:",
				{mixed_patterns[1], NULL, NULL},
				1,	1},
		{"User-Agent#",
				{mixed_patterns[2], NULL, NULL},
				1,	1},
		{"User-agEnt:",
				{mixed_patterns[1], NULL, NULL},
				1,	0},
		{"www.kornputers.com",
				{mixed_patterns[4], NULL, NULL},
				1,	1},
		{"1 file(s) copied from www.kornputers.com ",
				{mixed_patterns[0], mixed_patterns[4], NULL},
				2,	2},
		{"www.kornputers.com: 1 File(s) Copied",
				{mixed_patterns[4], mixed_patterns[0], NULL},
				2,	1},
		{"1 file(s) copied",
				{mixed_patterns[0], NULL, NULL},
				1,	1},
		{"1 file(s) copie",
				{NULL, NULL, NULL},
				0,	0},
		{"1 file(s) copieD",
				{mixed_patterns[0], NULL, NULL},
				1,	0},
		{"iwrhf34890yuhh *Y89#9ireirgf",
				{mixed_patterns[2], NULL, NULL},
				1,	1},
};

struct pm_test_buffer P1_buffers[] = {
		{"1111aA111111",
				{P1_patterns[0], NULL, NULL},
				1,	1},
		{"1111Aa111111",
				{P1_patterns[0], NULL, NULL},
				1,	0},
		{"1111aA11111",
				{NULL, NULL, NULL},
				0,	0},
		{"1111aB11111",
				{NULL, NULL, NULL},
				0,	0},
		{"1111aA11112",
				{NULL, NULL, NULL},
				0,	0},
		{"1111aA1111111111aA111111",
				{P1_patterns[0], P1_patterns[0], NULL},
				2,	2},
};


#endif /* TEST_PMAC_PM_H_ */
