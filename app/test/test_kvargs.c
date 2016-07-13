/*
 * Copyright 2014 6WIND S.A.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * - Neither the name of 6WIND S.A. nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <rte_common.h>
#include <rte_kvargs.h>

#include "test.h"

/* incrementd in handler, to check it is properly called once per
 * key/value association */
static unsigned count;

/* this handler increment the "count" variable at each call and check
 * that the key is "check" and the value is "value%d" */
static int check_handler(const char *key, const char *value,
	__rte_unused void *opaque)
{
	char buf[16];

	/* we check that the value is "check" */
	if (strcmp(key, "check"))
		return -1;

	/* we check that the value is "value$(count)" */
	snprintf(buf, sizeof(buf), "value%d", count);
	if (strncmp(buf, value, sizeof(buf)))
		return -1;

	count ++;
	return 0;
}

/* test a valid case */
static int test_valid_kvargs(void)
{
	struct rte_kvargs *kvlist;
	const char *args;
	const char *valid_keys_list[] = { "foo", "check", NULL };
	const char **valid_keys;

	/* empty args is valid */
	args = "";
	valid_keys = NULL;
	kvlist = rte_kvargs_parse(args, valid_keys);
	if (kvlist == NULL) {
		printf("rte_kvargs_parse() error");
		goto fail;
	}
	rte_kvargs_free(kvlist);

	/* first test without valid_keys */
	args = "foo=1234,check=value0,check=value1";
	valid_keys = NULL;
	kvlist = rte_kvargs_parse(args, valid_keys);
	if (kvlist == NULL) {
		printf("rte_kvargs_parse() error");
		goto fail;
	}
	/* call check_handler() for all entries with key="check" */
	count = 0;
	if (rte_kvargs_process(kvlist, "check", check_handler, NULL) < 0) {
		printf("rte_kvargs_process() error\n");
		rte_kvargs_free(kvlist);
		goto fail;
	}
	if (count != 2) {
		printf("invalid count value %d after rte_kvargs_process(check)\n",
			count);
		rte_kvargs_free(kvlist);
		goto fail;
	}
	count = 0;
	/* call check_handler() for all entries with key="unexistant_key" */
	if (rte_kvargs_process(kvlist, "unexistant_key", check_handler, NULL) < 0) {
		printf("rte_kvargs_process() error\n");
		rte_kvargs_free(kvlist);
		goto fail;
	}
	if (count != 0) {
		printf("invalid count value %d after rte_kvargs_process(unexistant_key)\n",
			count);
		rte_kvargs_free(kvlist);
		goto fail;
	}
	/* count all entries with key="foo" */
	count = rte_kvargs_count(kvlist, "foo");
	if (count != 1) {
		printf("invalid count value %d after rte_kvargs_count(foo)\n",
			count);
		rte_kvargs_free(kvlist);
		goto fail;
	}
	/* count all entries */
	count = rte_kvargs_count(kvlist, NULL);
	if (count != 3) {
		printf("invalid count value %d after rte_kvargs_count(NULL)\n",
			count);
		rte_kvargs_free(kvlist);
		goto fail;
	}
	/* count all entries with key="unexistant_key" */
	count = rte_kvargs_count(kvlist, "unexistant_key");
	if (count != 0) {
		printf("invalid count value %d after rte_kvargs_count(unexistant_key)\n",
			count);
		rte_kvargs_free(kvlist);
		goto fail;
	}
	rte_kvargs_free(kvlist);

	/* second test using valid_keys */
	args = "foo=droids,check=value0,check=value1,check=wrong_value";
	valid_keys = valid_keys_list;
	kvlist = rte_kvargs_parse(args, valid_keys);
	if (kvlist == NULL) {
		printf("rte_kvargs_parse() error");
		goto fail;
	}
	/* call check_handler() on all entries with key="check", it
	 * should fail as the value is not recognized by the handler */
	if (rte_kvargs_process(kvlist, "check", check_handler, NULL) == 0) {
		printf("rte_kvargs_process() is success bu should not\n");
		rte_kvargs_free(kvlist);
		goto fail;
	}
	count = rte_kvargs_count(kvlist, "check");
	if (count != 3) {
		printf("invalid count value %d after rte_kvargs_count(check)\n",
			count);
		rte_kvargs_free(kvlist);
		goto fail;
	}
	rte_kvargs_free(kvlist);

	return 0;

 fail:
	printf("while processing <%s>", args);
	if (valid_keys != NULL && *valid_keys != NULL) {
		printf(" using valid_keys=<%s", *valid_keys);
		while (*(++valid_keys) != NULL)
			printf(",%s", *valid_keys);
		printf(">");
	}
	printf("\n");
	return -1;
}

/* test several error cases */
static int test_invalid_kvargs(void)
{
	struct rte_kvargs *kvlist;
	/* list of argument that should fail */
	const char *args_list[] = {
		"wrong-key=x",     /* key not in valid_keys_list */
		"foo=1,foo=",      /* empty value */
		"foo=1,,foo=2",    /* empty key/value */
		"foo=1,foo",       /* no value */
		"foo=1,=2",        /* no key */
		",=",              /* also test with a smiley */
		NULL };
	const char **args;
	const char *valid_keys_list[] = { "foo", "check", NULL };
	const char **valid_keys = valid_keys_list;

	for (args = args_list; *args != NULL; args++) {

		kvlist = rte_kvargs_parse(*args, valid_keys);
		if (kvlist != NULL) {
			printf("rte_kvargs_parse() returned 0 (but should not)\n");
			rte_kvargs_free(kvlist);
			goto fail;
		}
		return 0;
	}

 fail:
	printf("while processing <%s>", *args);
	if (valid_keys != NULL && *valid_keys != NULL) {
		printf(" using valid_keys=<%s", *valid_keys);
		while (*(++valid_keys) != NULL)
			printf(",%s", *valid_keys);
		printf(">");
	}
	printf("\n");
	return -1;
}

static int
test_kvargs(void)
{
	printf("== test valid case ==\n");
	if (test_valid_kvargs() < 0)
		return -1;
	printf("== test invalid case ==\n");
	if (test_invalid_kvargs() < 0)
		return -1;
	return 0;
}

REGISTER_TEST_COMMAND(kvargs_autotest, test_kvargs);
