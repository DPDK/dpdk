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

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/queue.h>

#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>

#include "test.h"

#define RTE_LOGTYPE_TESTAPP1 RTE_LOGTYPE_USER1
#define RTE_LOGTYPE_TESTAPP2 RTE_LOGTYPE_USER2

/*
 * Logs
 * ====
 *
 * - Enable log types.
 * - Set log level.
 * - Send logs with different types and levels, some should not be displayed.
 */

static int
test_logs(void)
{
	/* enable these logs type */
	rte_set_log_type(RTE_LOGTYPE_TESTAPP1, 1);
	rte_set_log_type(RTE_LOGTYPE_TESTAPP2, 1);

	/* log in debug level */
	rte_set_log_level(RTE_LOG_DEBUG);
	RTE_LOG(DEBUG, TESTAPP1, "this is a debug level message\n");
	RTE_LOG(INFO, TESTAPP1, "this is a info level message\n");
	RTE_LOG(WARNING, TESTAPP1, "this is a warning level message\n");

	/* log in info level */
	rte_set_log_level(RTE_LOG_INFO);
	RTE_LOG(DEBUG, TESTAPP2, "debug level message (not displayed)\n");
	RTE_LOG(INFO, TESTAPP2, "this is a info level message\n");
	RTE_LOG(WARNING, TESTAPP2, "this is a warning level message\n");

	/* disable one log type */
	rte_set_log_type(RTE_LOGTYPE_TESTAPP2, 0);

	/* log in debug level */
	rte_set_log_level(RTE_LOG_DEBUG);
	RTE_LOG(DEBUG, TESTAPP1, "this is a debug level message\n");
	RTE_LOG(DEBUG, TESTAPP2, "debug level message (not displayed)\n");

	rte_log_dump_history(stdout);

	return 0;
}

static struct test_command logs_cmd = {
	.command = "logs_autotest",
	.callback = test_logs,
};
REGISTER_TEST_COMMAND(logs_cmd);
