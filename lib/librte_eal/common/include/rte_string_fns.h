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

/**
 * @file
 *
 * Definitions of warnings for use of various insecure functions
 */

#ifndef _RTE_STRING_FNS_H_
#define _RTE_STRING_FNS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <errno.h>

/**
 * Safer version of snprintf that writes up to buflen characters to
 * the output buffer and ensures that the resultant string is null-terminated,
 * that is, it writes at most buflen-1 actual string characters to buffer. The
 * return value is the number of characters which should be written to the
 * buffer, so string truncation can be detected by the caller by checking if
 * the return value is greater than or equal to the buflen.
 *
 * @param buffer
 *   The buffer into which the output is to be written
 *
 * @param buflen
 *   The size of the output buffer
 *
 * @param format
 *   The format string to be printed to the buffer
 *
 * @return
 *   The number of characters written to the buffer, or if the string has been
 *   truncated, the number of characters which would have been written had the
 *   buffer been sufficiently big.
 *
 */
static inline int
rte_snprintf(char *buffer, int buflen, const char *format, ...)
{
	int len;
	va_list ap;

	if (buffer == NULL && buflen != 0)
		goto einval_error;
	if (format == NULL) {
		if (buflen > 0)
			buffer[0] = '\0';
		goto einval_error;
	}

	va_start(ap, format);
	len = vsnprintf(buffer, buflen, format, ap);
	va_end(ap);
	if (len >= buflen && buflen > 0)
		buffer[buflen - 1] = '\0';

	return len;

einval_error:
	errno = EINVAL;
	return -1;
}


/**
 * Takes string "string" parameter and splits it at character "delim"
 * up to maxtokens-1 times - to give "maxtokens" resulting tokens. Like
 * strtok or strsep functions, this modifies its input string, by replacing
 * instances of "delim" with '\0'. All resultant tokens are returned in the
 * "tokens" array which must have enough entries to hold "maxtokens".
 *
 * @param string
 *   The input string to be split into tokens
 *
 * @param stringlen
 *   The max length of the input buffer
 *
 * @param tokens
 *   The array to hold the pointers to the tokens in the string
 *
 * @param maxtokens
 *   The number of elements in the tokens array. At most, maxtokens-1 splits
 *   of the string will be done.
 *
 * @param delim
 *   The character on which the split of the data will be done
 *
 * @return
 *   The number of tokens in the tokens array.
 */
static inline int
rte_strsplit(char *string, int stringlen,
		char **tokens, int maxtokens, char delim)
{
	int i, tok = 0;
	int tokstart = 1; /* first token is right at start of string */

	if (string == NULL || tokens == NULL)
		goto einval_error;

	for (i = 0; i < stringlen; i++) {
		if (string[i] == '\0' || tok >= maxtokens)
			break;
		if (tokstart) {
			tokstart = 0;
			tokens[tok++] = &string[i];
		}
		if (string[i] == delim) {
			string[i] = '\0';
			tokstart = 1;
		}
	}
	return tok;

einval_error:
	errno = EINVAL;
	return -1;
}

#ifdef __cplusplus
}
#endif


#endif /* RTE_STRING_FNS_H */
