/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
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
	char *argv_cpy[numargs + 1];
	int i, fd, status;
	char path[32];

	pid_t pid = fork();
	if (pid < 0)
		return -1;
	else if (pid == 0) {
		/* make a copy of the arguments to be passed to exec */
		for (i = 0; i < numargs; i++)
			argv_cpy[i] = strdup(argv[i]);
		argv_cpy[i] = NULL;
		num = numargs;

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
