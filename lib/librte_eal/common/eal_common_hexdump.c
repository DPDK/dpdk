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
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <rte_hexdump.h>
#include <rte_string_fns.h>

#define LINE_LEN 128

/**************************************************************************//**
*
* rte_hexdump - Dump out memory in a special hex dump format.
*
* DESCRIPTION
* Dump out the message buffer in a special hex dump output format with characters
* printed for each line of 16 hex values.
*
* RETURNS: N/A
*
* SEE ALSO:
*/

void
rte_hexdump(FILE *f, const char * title, const void * buf, unsigned int len)
{
    unsigned int i, out, ofs;
    const unsigned char *data = buf;
    char line[LINE_LEN];    /* space needed 8+16*3+3+16 == 75 */

    fprintf(f, "%s at [%p], len=%u\n", (title)? title  : "  Dump data", data, len);
    ofs = 0;
    while (ofs < len) {
        /* format the line in the buffer, then use printf to output to screen */
        out = snprintf(line, LINE_LEN, "%08X:", ofs);
        for (i = 0; ((ofs + i) < len) && (i < 16); i++)
            out += snprintf(line+out, LINE_LEN - out, " %02X", (data[ofs+i] & 0xff));
        for(; i <= 16; i++)
            out += snprintf(line+out, LINE_LEN - out, " | ");
        for(i = 0; (ofs < len) && (i < 16); i++, ofs++) {
            unsigned char c = data[ofs];
            if ( (c < ' ') || (c > '~'))
                c = '.';
            out += snprintf(line+out, LINE_LEN - out, "%c", c);
        }
        fprintf(f, "%s\n", line);
    }
    fflush(f);
}

/**************************************************************************//**
*
* rte_memdump - Dump out memory in hex bytes with colons.
*
* DESCRIPTION
* Dump out the message buffer in hex bytes with colons xx:xx:xx:xx:...
*
* RETURNS: N/A
*
* SEE ALSO:
*/

void
rte_memdump(FILE *f, const char * title, const void * buf, unsigned int len)
{
    unsigned int i, out;
    const unsigned char *data = buf;
    char line[LINE_LEN];

    if ( title )
	fprintf(f, "%s: ", title);

    line[0] = '\0';
    for (i = 0, out = 0; i < len; i++) {
	// Make sure we do not overrun the line buffer length.
		if ( out >= (LINE_LEN - 4) ) {
			fprintf(f, "%s", line);
			out = 0;
			line[out] = '\0';
		}
		out += snprintf(line+out, LINE_LEN - out, "%02x%s",
				(data[i] & 0xff), ((i+1) < len)? ":" : "");
    }
    if ( out > 0 )
	fprintf(f, "%s", line);
    fprintf(f, "\n");

    fflush(f);
}
