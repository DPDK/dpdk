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

#ifndef _PROCESS_H_
#define _PROCESS_H_

#ifdef RTE_EXEC_ENV_BSDAPP
#define self "curproc"
#define exe "file"
#else
#define self "self"
#define exe "exe"
#endif

/*
 * launches a second copy of the test process using the given argv parameters,
 * which should include argv[0] as the process name. To identify in the
 * subprocess the source of the call, the env_value parameter is set in the
 * environment as $RTE_TEST
 */
static inline int
process_dup(const char *const argv[], int numargs, const char *env_value)
{
	int num;
#ifdef RTE_LIBRTE_XEN_DOM0
	char *argv_cpy[numargs + 2];
#else
	char *argv_cpy[numargs + 1];
#endif
	int i, fd, status;
	char path[32];

	pid_t pid = fork();
	if (pid < 0)
		return -1;
	else if (pid == 0) {
		/* make a copy of the arguments to be passed to exec */
		for (i = 0; i < numargs; i++)
			argv_cpy[i] = strdup(argv[i]);
#ifdef RTE_LIBRTE_XEN_DOM0
		argv_cpy[i] = strdup("--xen-dom0");
		argv_cpy[i + 1] = NULL;
		num = numargs + 1;
#else
		argv_cpy[i] = NULL;
		num = numargs;
#endif

		/* close all open file descriptors, check /proc/self/fd to only
		 * call close on open fds. Exclude fds 0, 1 and 2*/
		for (fd = getdtablesize(); fd > 2; fd-- ) {
			snprintf(path, sizeof(path), "/proc/" exe "/fd/%d", fd);
			if (access(path, F_OK) == 0)
				close(fd);
		}
		printf("Running binary with argv[]:");
		for (i = 0; i < num; i++)
			printf("'%s' ", argv_cpy[i]);
		printf("\n");

		/* set the environment variable */
		if (setenv(RECURSIVE_ENV_VAR, env_value, 1) != 0)
			rte_panic("Cannot export environment variable\n");
		if (execv("/proc/" self "/" exe, argv_cpy) < 0)
			rte_panic("Cannot exec\n");
	}
	/* parent process does a wait */
	while (wait(&status) != pid)
		;
	return status;
}

#endif /* _PROCESS_H_ */
