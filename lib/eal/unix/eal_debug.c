/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <rte_debug.h>

#ifdef RTE_BACKTRACE

#include <dlfcn.h>
#include <execinfo.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>

#define BACKTRACE_SIZE 256

/*
 * Convert number to string and return start of string.
 * Note: string does not start at beginning of buffer.
 */
static char *safe_itoa(long val, char *buf, size_t len, unsigned int radix)
{
	char *bp = buf + len;
	static const char hexdigit[] = "0123456789abcdef";

	*--bp = '\0'; /* Null terminate the string */
	do {
		/* if buffer is not big enough, then truncate */
		if (bp == buf)
			return bp;

		*--bp = hexdigit[val % radix];
		val /= radix;
	} while (val != 0);

	return bp;
}

/*
 * Dump the stack of the calling core
 *
 * To be safe in signal handler requires limiting what functions are
 * used in this code since may be called from inside libc or
 * when malloc poll is corrupt.
 *
 * Most of libc is therefore not safe, include RTE_LOG (calls syslog);
 * backtrace_symbols (calls malloc), etc.
 */
void rte_dump_stack(void)
{
	void *func[BACKTRACE_SIZE];
	Dl_info info;
	char buf1[8], buf2[32], buf3[32], buf4[32];
	struct iovec iov[10];
	int i, size;

	size = backtrace(func, BACKTRACE_SIZE);

	for (i = 0; i < size; i++) {
		struct iovec *io = iov;
		char *str;
		uintptr_t base;
		long offset;
		void *pc = func[i];

/*
 * Macro to put string onto set of iovecs.
 * cast is to suppress warnings about lose of const qualifier.
 */
#define PUSH_IOV(io, str) {					\
		(io)->iov_base = (char *)(uintptr_t)str;	\
		(io)->iov_len = strlen(str);			\
		++io; }

		/* output stack frame number */
		str = safe_itoa(i, buf1, sizeof(buf1), 10);
		PUSH_IOV(io, str);	/* iov[0] */
		PUSH_IOV(io, ": ");	/* iov[1] */

		/* Lookup the symbol information */
		if (dladdr(pc, &info) == 0) {
			PUSH_IOV(io, "?? [");
		} else {
			const char *fname;

			if (info.dli_fname && *info.dli_fname)
				fname = info.dli_fname;
			else
				fname = "(vdso)";
			PUSH_IOV(io, fname);	/* iov[2] */
			PUSH_IOV(io, " (");	/* iov[3] */

			if (info.dli_saddr != NULL) {
				PUSH_IOV(io, info.dli_sname);	/* iov[4] */
				base = (uintptr_t)info.dli_saddr;
			} else {
				str = safe_itoa((unsigned long)info.dli_fbase,
					buf3, sizeof(buf3), 16);
				PUSH_IOV(io, str);
				base = (uintptr_t)info.dli_fbase;
			}

			PUSH_IOV(io, "+0x");	/* iov[5] */

			offset = (uintptr_t)pc - base;
			str = safe_itoa(offset, buf4, sizeof(buf4), 16);
			PUSH_IOV(io, str);	/* iov[6] */

			PUSH_IOV(io, ") [");	/* iov[7] */
		}

		str = safe_itoa((unsigned long)pc, buf2, sizeof(buf2), 16);
		PUSH_IOV(io, str);	/* iov[8] */
		PUSH_IOV(io, "]\n");	/* iov[9] */

		if (writev(STDERR_FILENO, iov, io - iov) < 0)
			break;
#undef PUSH_IOV
	}
}

#else /* !RTE_BACKTRACE */

/* stub if not enabled */
void rte_dump_stack(void) { }

#endif /* RTE_BACKTRACE */
