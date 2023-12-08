/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <stdarg.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_windows.h>

#include "eal_private.h"

#include <dbghelp.h>

#define BACKTRACE_SIZE 256

/* dump the stack of the calling core */
void
rte_dump_stack(void)
{
	PVOID stack_trace[BACKTRACE_SIZE] = {0};
	USHORT frame_num;
	BOOL ret;
	HANDLE process = GetCurrentProcess();

	ret = SymInitialize(process, NULL, TRUE);
	if (!ret) {
		RTE_LOG_WIN32_ERR("SymInitialize()");
		return;
	}

	SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);

	frame_num = RtlCaptureStackBackTrace(0, BACKTRACE_SIZE,
			stack_trace, NULL);

	while (frame_num > 0) {
		DWORD64 address = (DWORD64)(stack_trace[frame_num - 1]);
		DWORD64 sym_disp = 0;
		DWORD error_code = 0, lin_disp;
		char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
		PSYMBOL_INFO symbol_info = (PSYMBOL_INFO)buffer;
		IMAGEHLP_LINE64 line;

		symbol_info->SizeOfStruct = sizeof(SYMBOL_INFO);
		symbol_info->MaxNameLen = MAX_SYM_NAME;
		line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

		ret = SymFromAddr(process, address, &sym_disp, symbol_info);
		if (!ret) {
			error_code = GetLastError();
			if (error_code == ERROR_INVALID_ADDRESS) {
				/* Missing symbols, print message */
				EAL_LOG(ERR,
				    "%d: [<missing_symbols>]", frame_num--);
				continue;
			} else {
				RTE_LOG_WIN32_ERR("SymFromAddr()");
				goto end;
			}
		}

		ret = SymGetLineFromAddr64(process, address, &lin_disp, &line);
		if (!ret) {
			error_code = GetLastError();
			/* If ERROR_INVALID_ADDRESS tag unknown and proceed */
			if (error_code != ERROR_INVALID_ADDRESS) {
				RTE_LOG_WIN32_ERR("SymGetLineFromAddr64()");
				goto end;
			}
		}

		EAL_LOG(ERR,
			"%d: [%s (%s+0x%0llx)[0x%0llX]]", frame_num,
			error_code ? "<unknown>" : line.FileName,
			symbol_info->Name, sym_disp, symbol_info->Address);
		frame_num--;
	}
end:
	ret = SymCleanup(process);
	if (!ret)
		RTE_LOG_WIN32_ERR("SymCleanup()");
}
