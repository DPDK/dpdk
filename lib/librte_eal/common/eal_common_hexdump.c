/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
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
