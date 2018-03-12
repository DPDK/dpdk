/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rte_string_fns.h>

#include <cmdline_cirbuf.h>

#include "test_cmdline.h"

/* different length strings */
#define CIRBUF_STR_HEAD " HEAD"
#define CIRBUF_STR_TAIL "TAIL"

/* miscellaneous tests - they make bullseye happy */
static int
test_cirbuf_string_misc(void)
{
	struct cirbuf cb;
	char buf[CMDLINE_TEST_BUFSIZE];
	char tmp[CMDLINE_TEST_BUFSIZE];

	/* initialize buffers */
	memset(buf, 0, sizeof(buf));
	memset(tmp, 0, sizeof(tmp));

	/*
	 * initialize circular buffer
	 */
	if (cirbuf_init(&cb, buf, 0, sizeof(buf)) < 0) {
		printf("Error: failed to initialize circular buffer!\n");
		return -1;
	}

	/*
	 * add strings to head and tail, but read only tail
	 * this results in read operation that does not transcend
	 * from buffer end to buffer beginning (in other words,
	 * strlen <= cb->maxlen - cb->end)
	 */

	/* add string to head */
	if (cirbuf_add_buf_head(&cb, CIRBUF_STR_HEAD, sizeof(CIRBUF_STR_HEAD))
			!= (sizeof(CIRBUF_STR_HEAD))) {
		printf("Error: failed to add string to head!\n");
		return -1;
	}
	/* add string to tail */
	if (cirbuf_add_buf_tail(&cb, CIRBUF_STR_TAIL, sizeof(CIRBUF_STR_TAIL))
			!= (sizeof(CIRBUF_STR_TAIL))) {
		printf("Error: failed to add string to head!\n");
		return -1;
	}
	/* read string from tail */
	if (cirbuf_get_buf_tail(&cb, tmp, sizeof(CIRBUF_STR_TAIL))
			!= (sizeof(CIRBUF_STR_TAIL))) {
		printf("Error: failed to get string from tail!\n");
		return -1;
	}
	/* verify string */
	if (strncmp(tmp, CIRBUF_STR_TAIL, sizeof(CIRBUF_STR_TAIL)) != 0) {
		printf("Error: tail strings do not match!\n");
		return -1;
	}
	/* clear buffers */
	memset(tmp, 0, sizeof(tmp));
	memset(buf, 0, sizeof(buf));



	/*
	 * add a string to buffer when start/end is at end of buffer
	 */

	/*
	 * reinitialize circular buffer with start at the end of cirbuf
	 */
	if (cirbuf_init(&cb, buf, CMDLINE_TEST_BUFSIZE - 2, sizeof(buf)) < 0) {
		printf("Error: failed to reinitialize circular buffer!\n");
		return -1;
	}


	/* add string to tail */
	if (cirbuf_add_buf_tail(&cb, CIRBUF_STR_TAIL, sizeof(CIRBUF_STR_TAIL))
			!= (sizeof(CIRBUF_STR_TAIL))) {
		printf("Error: failed to add string to tail!\n");
		return -1;
	}
	/* read string from tail */
	if (cirbuf_get_buf_tail(&cb, tmp, sizeof(CIRBUF_STR_TAIL))
			!= (sizeof(CIRBUF_STR_TAIL))) {
		printf("Error: failed to get string from tail!\n");
		return -1;
	}
	/* verify string */
	if (strncmp(tmp, CIRBUF_STR_TAIL, sizeof(CIRBUF_STR_TAIL)) != 0) {
		printf("Error: tail strings do not match!\n");
		return -1;
	}
	/* clear tmp buffer */
	memset(tmp, 0, sizeof(tmp));


	/* add string to head */
	if (cirbuf_add_buf_head(&cb, CIRBUF_STR_HEAD, sizeof(CIRBUF_STR_HEAD))
			!= (sizeof(CIRBUF_STR_HEAD))) {
		printf("Error: failed to add string to head!\n");
		return -1;
	}
	/* read string from tail */
	if (cirbuf_get_buf_head(&cb, tmp, sizeof(CIRBUF_STR_HEAD))
			!= (sizeof(CIRBUF_STR_HEAD))) {
		printf("Error: failed to get string from head!\n");
		return -1;
	}
	/* verify string */
	if (strncmp(tmp, CIRBUF_STR_HEAD, sizeof(CIRBUF_STR_HEAD)) != 0) {
		printf("Error: headstrings do not match!\n");
		return -1;
	}

	return 0;
}

/* test adding and deleting strings */
static int
test_cirbuf_string_add_del(void)
{
	struct cirbuf cb;
	char buf[CMDLINE_TEST_BUFSIZE];
	char tmp[CMDLINE_TEST_BUFSIZE];

	/* initialize buffers */
	memset(buf, 0, sizeof(buf));
	memset(tmp, 0, sizeof(tmp));

	/*
	 * initialize circular buffer
	 */
	if (cirbuf_init(&cb, buf, 0, sizeof(buf)) < 0) {
		printf("Error: failed to initialize circular buffer!\n");
		return -1;
	}

	/* add string to head */
	if (cirbuf_add_buf_head(&cb, CIRBUF_STR_HEAD, sizeof(CIRBUF_STR_HEAD))
			!= (sizeof(CIRBUF_STR_HEAD))) {
		printf("Error: failed to add string to head!\n");
		return -1;
	}
	/* read string from head */
	if (cirbuf_get_buf_head(&cb, tmp, sizeof(CIRBUF_STR_HEAD))
			!= (sizeof(CIRBUF_STR_HEAD))) {
		printf("Error: failed to get string from head!\n");
		return -1;
	}
	/* verify string */
	if (strncmp(tmp, CIRBUF_STR_HEAD, sizeof(CIRBUF_STR_HEAD)) != 0) {
		printf("Error: head strings do not match!\n");
		return -1;
	}
	/* clear tmp buffer */
	memset(tmp, 0, sizeof(tmp));
	/* read string from tail */
	if (cirbuf_get_buf_tail(&cb, tmp, sizeof(CIRBUF_STR_HEAD))
			!= (sizeof(CIRBUF_STR_HEAD))) {
		printf("Error: failed to get string from head!\n");
		return -1;
	}
	/* verify string */
	if (strncmp(tmp, CIRBUF_STR_HEAD, sizeof(CIRBUF_STR_HEAD)) != 0) {
		printf("Error: head strings do not match!\n");
		return -1;
	}
	/* delete string from head*/
	if (cirbuf_del_buf_head(&cb, sizeof(CIRBUF_STR_HEAD)) < 0) {
		printf("Error: failed to delete string from head!\n");
		return -1;
	}
	/* verify string was deleted */
	if (cirbuf_del_head_safe(&cb) == 0) {
		printf("Error: buffer should have been empty!\n");
		return -1;
	}
	/* clear tmp buffer */
	memset(tmp, 0, sizeof(tmp));



	/*
	 * reinitialize circular buffer
	 */
	memset(buf, 0, sizeof(buf));
	if (cirbuf_init(&cb, buf, 0, sizeof(buf)) < 0) {
		printf("Error: failed to reinitialize circular buffer!\n");
		return -1;
	}

	/* add string to tail */
	if (cirbuf_add_buf_tail(&cb, CIRBUF_STR_TAIL, sizeof(CIRBUF_STR_TAIL))
			!= (sizeof(CIRBUF_STR_TAIL))) {
		printf("Error: failed to add string to tail!\n");
		return -1;
	}
	/* get string from tail */
	if (cirbuf_get_buf_tail(&cb, tmp, sizeof(CIRBUF_STR_TAIL))
			!= (sizeof(CIRBUF_STR_TAIL))) {
		printf("Error: failed to get string from tail!\n");
		return -1;
	}
	/* verify string */
	if (strncmp(tmp, CIRBUF_STR_TAIL, sizeof(CIRBUF_STR_TAIL)) != 0) {
		printf("Error: tail strings do not match!\n");
		return -1;
	}
	/* clear tmp buffer */
	memset(tmp, 0, sizeof(tmp));
	/* get string from head */
	if (cirbuf_get_buf_head(&cb, tmp, sizeof(CIRBUF_STR_TAIL))
			!= (sizeof(CIRBUF_STR_TAIL))) {
		printf("Error: failed to get string from tail!\n");
		return -1;
	}
	/* verify string */
	if (strncmp(tmp, CIRBUF_STR_TAIL, sizeof(CIRBUF_STR_TAIL)) != 0) {
		printf("Error: tail strings do not match!\n");
		return -1;
	}
	/* delete string from tail */
	if (cirbuf_del_buf_tail(&cb, sizeof(CIRBUF_STR_TAIL)) < 0) {
		printf("Error: failed to delete string from tail!\n");
		return -1;
	}
	/* verify string was deleted */
	if (cirbuf_del_tail_safe(&cb) == 0) {
		printf("Error: buffer should have been empty!\n");
		return -1;
	}

	return 0;
}

/* test adding from head and deleting from tail, and vice versa */
static int
test_cirbuf_string_add_del_reverse(void)
{
	struct cirbuf cb;
	char buf[CMDLINE_TEST_BUFSIZE];
	char tmp[CMDLINE_TEST_BUFSIZE];

	/* initialize buffers */
	memset(buf, 0, sizeof(buf));
	memset(tmp, 0, sizeof(tmp));

	/*
	 * initialize circular buffer
	 */
	if (cirbuf_init(&cb, buf, 0, sizeof(buf)) < 0) {
		printf("Error: failed to initialize circular buffer!\n");
		return -1;
	}

	/* add string to head */
	if (cirbuf_add_buf_head(&cb, CIRBUF_STR_HEAD, sizeof(CIRBUF_STR_HEAD))
			!= (sizeof(CIRBUF_STR_HEAD))) {
		printf("Error: failed to add string to head!\n");
		return -1;
	}
	/* delete string from tail */
	if (cirbuf_del_buf_tail(&cb, sizeof(CIRBUF_STR_HEAD)) < 0) {
		printf("Error: failed to delete string from tail!\n");
		return -1;
	}
	/* verify string was deleted */
	if (cirbuf_del_tail_safe(&cb) == 0) {
		printf("Error: buffer should have been empty!\n");
		return -1;
	}
	/* clear tmp buffer */
	memset(tmp, 0, sizeof(tmp));

	/*
	 * reinitialize circular buffer
	 */
	memset(buf, 0, sizeof(buf));
	if (cirbuf_init(&cb, buf, 0, sizeof(buf)) < 0) {
		printf("Error: failed to reinitialize circular buffer!\n");
		return -1;
	}

	/* add string to tail */
	if (cirbuf_add_buf_tail(&cb, CIRBUF_STR_TAIL, sizeof(CIRBUF_STR_TAIL))
			!= (sizeof(CIRBUF_STR_TAIL))) {
		printf("Error: failed to add string to tail!\n");
		return -1;
	}
	/* delete string from head */
	if (cirbuf_del_buf_head(&cb, sizeof(CIRBUF_STR_TAIL)) < 0) {
		printf("Error: failed to delete string from head!\n");
		return -1;
	}
	/* verify string was deleted */
	if (cirbuf_del_head_safe(&cb) == 0) {
		printf("Error: buffer should have been empty!\n");
		return -1;
	}

	return 0;
}

/* try to write more than available */
static int
test_cirbuf_string_add_boundaries(void)
{
	struct cirbuf cb;
	char buf[CMDLINE_TEST_BUFSIZE];
	unsigned i;

	/* initialize buffers */
	memset(buf, 0, sizeof(buf));

	/*
	 * initialize circular buffer
	 */
	if (cirbuf_init(&cb, buf, 0, sizeof(buf)) < 0) {
		printf("Error: failed to initialize circular buffer!\n");
		return -1;
	}

	/* fill the buffer from tail */
	for (i = 0; i < CMDLINE_TEST_BUFSIZE - sizeof(CIRBUF_STR_TAIL) + 1; i++)
		cirbuf_add_tail_safe(&cb, 't');

	/* try adding a string to tail */
	if (cirbuf_add_buf_tail(&cb, CIRBUF_STR_TAIL, sizeof(CIRBUF_STR_TAIL))
			> 0) {
		printf("Error: buffer should have been full!\n");
		return -1;
	}
	/* try adding a string to head */
	if (cirbuf_add_buf_head(&cb, CIRBUF_STR_TAIL, sizeof(CIRBUF_STR_TAIL))
			> 0) {
		printf("Error: buffer should have been full!\n");
		return -1;
	}

	/*
	 * reinitialize circular buffer
	 */
	memset(buf, 0, sizeof(buf));
	if (cirbuf_init(&cb, buf, 0, sizeof(buf)) < 0) {
		printf("Error: failed to reinitialize circular buffer!\n");
		return -1;
	}

	/* fill the buffer from head */
	for (i = 0; i < CMDLINE_TEST_BUFSIZE - sizeof(CIRBUF_STR_HEAD) + 1; i++)
		cirbuf_add_head_safe(&cb, 'h');

	/* try adding a string to head */
	if (cirbuf_add_buf_head(&cb, CIRBUF_STR_HEAD, sizeof(CIRBUF_STR_HEAD))
			> 0) {
		printf("Error: buffer should have been full!\n");
		return -1;
	}
	/* try adding a string to tail */
	if (cirbuf_add_buf_tail(&cb, CIRBUF_STR_HEAD, sizeof(CIRBUF_STR_HEAD))
			> 0) {
		printf("Error: buffer should have been full!\n");
		return -1;
	}

	return 0;
}

/* try to read/delete more than written */
static int
test_cirbuf_string_get_del_boundaries(void)
{
	struct cirbuf cb;
	char buf[CMDLINE_TEST_BUFSIZE];
	char tmp[CMDLINE_TEST_BUFSIZE];

	/* initialize buffers */
	memset(buf, 0, sizeof(buf));
	memset(tmp, 0, sizeof(tmp));

	/*
	 * initialize circular buffer
	 */
	if (cirbuf_init(&cb, buf, 0, sizeof(buf)) < 0) {
		printf("Error: failed to initialize circular buffer!\n");
		return -1;
	}


	/* add string to head */
	if (cirbuf_add_buf_head(&cb, CIRBUF_STR_HEAD, sizeof(CIRBUF_STR_HEAD))
				!= (sizeof(CIRBUF_STR_HEAD))) {
		printf("Error: failed to add string to head!\n");
		return -1;
	}
	/* read more than written (head) */
	if (cirbuf_get_buf_head(&cb, tmp, sizeof(CIRBUF_STR_HEAD) + 1)
			!= sizeof(CIRBUF_STR_HEAD)) {
		printf("Error: unexpected result when reading too much data!\n");
		return -1;
	}
	/* read more than written (tail) */
	if (cirbuf_get_buf_tail(&cb, tmp, sizeof(CIRBUF_STR_HEAD) + 1)
			!= sizeof(CIRBUF_STR_HEAD)) {
		printf("Error: unexpected result when reading too much data!\n");
		return -1;
	}
	/* delete more than written (head) */
	if (cirbuf_del_buf_head(&cb, sizeof(CIRBUF_STR_HEAD) + 1) == 0) {
		printf("Error: unexpected result when deleting too much data!\n");
		return -1;
	}
	/* delete more than written (tail) */
	if (cirbuf_del_buf_tail(&cb, sizeof(CIRBUF_STR_HEAD) + 1) == 0) {
		printf("Error: unexpected result when deleting too much data!\n");
		return -1;
	}

	/*
	 * reinitialize circular buffer
	 */
	memset(buf, 0, sizeof(buf));
	if (cirbuf_init(&cb, buf, 0, sizeof(buf)) < 0) {
		printf("Error: failed to reinitialize circular buffer!\n");
		return -1;
	}

	/* add string to tail */
	if (cirbuf_add_buf_tail(&cb, CIRBUF_STR_TAIL, sizeof(CIRBUF_STR_TAIL))
				!= (sizeof(CIRBUF_STR_TAIL))) {
		printf("Error: failed to add string to tail!\n");
		return -1;
	}
	/* read more than written (tail) */
	if (cirbuf_get_buf_tail(&cb, tmp, sizeof(CIRBUF_STR_TAIL) + 1)
			!= sizeof(CIRBUF_STR_TAIL)) {
		printf("Error: unexpected result when reading too much data!\n");
		return -1;
	}
	/* read more than written (head) */
	if (cirbuf_get_buf_head(&cb, tmp, sizeof(CIRBUF_STR_TAIL) + 1)
			!= sizeof(CIRBUF_STR_TAIL)) {
		printf("Error: unexpected result when reading too much data!\n");
		return -1;
	}
	/* delete more than written (tail) */
	if (cirbuf_del_buf_tail(&cb, sizeof(CIRBUF_STR_TAIL) + 1) == 0) {
		printf("Error: unexpected result when deleting too much data!\n");
		return -1;
	}
	/* delete more than written (head) */
	if (cirbuf_del_buf_tail(&cb, sizeof(CIRBUF_STR_TAIL) + 1) == 0) {
		printf("Error: unexpected result when deleting too much data!\n");
		return -1;
	}

	return 0;
}

/* try to read/delete less than written */
static int
test_cirbuf_string_get_del_partial(void)
{
	struct cirbuf cb;
	char buf[CMDLINE_TEST_BUFSIZE];
	char tmp[CMDLINE_TEST_BUFSIZE];
	char tmp2[CMDLINE_TEST_BUFSIZE];

	/* initialize buffers */
	memset(buf, 0, sizeof(buf));
	memset(tmp, 0, sizeof(tmp));
	memset(tmp2, 0, sizeof(tmp));

	strlcpy(tmp2, CIRBUF_STR_HEAD, sizeof(tmp2));

	/*
	 * initialize circular buffer
	 */
	if (cirbuf_init(&cb, buf, 0, sizeof(buf)) < 0) {
		printf("Error: failed to initialize circular buffer!\n");
		return -1;
	}

	/* add string to head */
	if (cirbuf_add_buf_head(&cb, CIRBUF_STR_HEAD, sizeof(CIRBUF_STR_HEAD))
				!= (sizeof(CIRBUF_STR_HEAD))) {
		printf("Error: failed to add string to head!\n");
		return -1;
	}
	/* read less than written (head) */
	if (cirbuf_get_buf_head(&cb, tmp, sizeof(CIRBUF_STR_HEAD) - 1)
			!= sizeof(CIRBUF_STR_HEAD) - 1) {
		printf("Error: unexpected result when reading from head!\n");
		return -1;
	}
	/* verify string */
	if (strncmp(tmp, tmp2, sizeof(CIRBUF_STR_HEAD) - 1) != 0) {
		printf("Error: strings mismatch!\n");
		return -1;
	}
	memset(tmp, 0, sizeof(tmp));
	/* read less than written (tail) */
	if (cirbuf_get_buf_tail(&cb, tmp, sizeof(CIRBUF_STR_HEAD) - 1)
			!= sizeof(CIRBUF_STR_HEAD) - 1) {
		printf("Error: unexpected result when reading from tail!\n");
		return -1;
	}
	/* verify string */
	if (strncmp(tmp, &tmp2[1], sizeof(CIRBUF_STR_HEAD) - 1) != 0) {
		printf("Error: strings mismatch!\n");
		return -1;
	}

	/*
	 * verify correct deletion
	 */

	/* clear buffer */
	memset(tmp, 0, sizeof(tmp));

	/* delete less than written (head) */
	if (cirbuf_del_buf_head(&cb, 1) != 0) {
		printf("Error: delete from head failed!\n");
		return -1;
	}
	/* read from head */
	if (cirbuf_get_buf_head(&cb, tmp, sizeof(CIRBUF_STR_HEAD) - 1)
			!= sizeof(CIRBUF_STR_HEAD) - 1) {
		printf("Error: unexpected result when reading from head!\n");
		return -1;
	}
	/* since we deleted from head, first char should be deleted */
	if (strncmp(tmp, &tmp2[1], sizeof(CIRBUF_STR_HEAD) - 1) != 0) {
		printf("Error: strings mismatch!\n");
		return -1;
	}
	/* clear buffer */
	memset(tmp, 0, sizeof(tmp));

	/* delete less than written (tail) */
	if (cirbuf_del_buf_tail(&cb, 1) != 0) {
		printf("Error: delete from tail failed!\n");
		return -1;
	}
	/* read from tail */
	if (cirbuf_get_buf_tail(&cb, tmp, sizeof(CIRBUF_STR_HEAD) - 2)
			!= sizeof(CIRBUF_STR_HEAD) - 2) {
		printf("Error: unexpected result when reading from head!\n");
		return -1;
	}
	/* since we deleted from tail, last char should be deleted */
	if (strncmp(tmp, &tmp2[1], sizeof(CIRBUF_STR_HEAD) - 2) != 0) {
		printf("Error: strings mismatch!\n");
		return -1;
	}

	return 0;
}

/* test cmdline_cirbuf char add/del functions */
static int
test_cirbuf_char_add_del(void)
{
	struct cirbuf cb;
	char buf[CMDLINE_TEST_BUFSIZE];
	char tmp[CMDLINE_TEST_BUFSIZE];

	/* clear buffer */
	memset(buf, 0, sizeof(buf));
	memset(tmp, 0, sizeof(tmp));

	/*
	 * initialize circular buffer
	 */
	if (cirbuf_init(&cb, buf, 0, sizeof(buf)) < 0) {
		printf("Error: failed to initialize circular buffer!\n");
		return -1;
	}

	/*
	 * try to delete something from cirbuf. since it's empty,
	 * these should fail.
	 */
	if (cirbuf_del_head_safe(&cb) == 0) {
		printf("Error: deleting from empty cirbuf head succeeded!\n");
		return -1;
	}
	if (cirbuf_del_tail_safe(&cb) == 0) {
		printf("Error: deleting from empty cirbuf tail succeeded!\n");
		return -1;
	}

	/*
	 * add, verify and delete. these should pass.
	 */
	if (cirbuf_add_head_safe(&cb,'h') < 0) {
		printf("Error: adding to cirbuf head failed!\n");
		return -1;
	}
	if (cirbuf_get_head(&cb) != 'h') {
		printf("Error: wrong head content!\n");
		return -1;
	}
	if (cirbuf_del_head_safe(&cb) < 0) {
		printf("Error: deleting from cirbuf head failed!\n");
		return -1;
	}
	if (cirbuf_add_tail_safe(&cb,'t') < 0) {
		printf("Error: adding to cirbuf tail failed!\n");
		return -1;
	}
	if (cirbuf_get_tail(&cb) != 't') {
		printf("Error: wrong tail content!\n");
		return -1;
	}
	if (cirbuf_del_tail_safe(&cb) < 0) {
		printf("Error: deleting from cirbuf tail failed!\n");
		return -1;
	}
	/* do the same for unsafe versions. those are void. */
	cirbuf_add_head(&cb,'h');
	if (cirbuf_get_head(&cb) != 'h') {
		printf("Error: wrong head content!\n");
		return -1;
	}
	cirbuf_del_head(&cb);

	/* test if char has been deleted. we can't call cirbuf_get_head
	 * because it's unsafe, but we can call cirbuf_get_buf_head.
	 */
	if (cirbuf_get_buf_head(&cb, tmp, 1) > 0) {
		printf("Error: buffer should have been empty!\n");
		return -1;
	}

	cirbuf_add_tail(&cb,'t');
	if (cirbuf_get_tail(&cb) != 't') {
		printf("Error: wrong tail content!\n");
		return -1;
	}
	cirbuf_del_tail(&cb);

	/* test if char has been deleted. we can't call cirbuf_get_tail
	 * because it's unsafe, but we can call cirbuf_get_buf_tail.
	 */
	if (cirbuf_get_buf_tail(&cb, tmp, 1) > 0) {
		printf("Error: buffer should have been empty!\n");
		return -1;
	}

	return 0;
}

/* test filling up buffer with chars */
static int
test_cirbuf_char_fill(void)
{
	struct cirbuf cb;
	char buf[CMDLINE_TEST_BUFSIZE];
	unsigned i;

	/* clear buffer */
	memset(buf, 0, sizeof(buf));

	/*
	 * initialize circular buffer
	 */
	if (cirbuf_init(&cb, buf, 0, sizeof(buf)) < 0) {
		printf("Error: failed to initialize circular buffer!\n");
		return -1;
	}

	/*
	 * fill the buffer from head or tail, verify contents, test boundaries
	 * and clear the buffer
	 */

	/* fill the buffer from tail */
	for (i = 0; i < CMDLINE_TEST_BUFSIZE; i++)
		cirbuf_add_tail_safe(&cb, 't');
	/* verify that contents of the buffer are what they are supposed to be */
	for (i = 0; i < sizeof(buf); i++) {
		if (buf[i] != 't') {
			printf("Error: wrong content in buffer!\n");
			return -1;
		}
	}
	/* try to add to a full buffer from tail */
	if (cirbuf_add_tail_safe(&cb, 't') == 0) {
		printf("Error: buffer should have been full!\n");
		return -1;
	}
	/* try to add to a full buffer from head */
	if (cirbuf_add_head_safe(&cb, 'h') == 0) {
		printf("Error: buffer should have been full!\n");
		return -1;
	}
	/* delete buffer from tail */
	for(i = 0; i < CMDLINE_TEST_BUFSIZE; i++)
		cirbuf_del_tail_safe(&cb);
	/* try to delete from an empty buffer */
	if (cirbuf_del_tail_safe(&cb) >= 0) {
		printf("Error: buffer should have been empty!\n");
		return -1;
	}

	/* fill the buffer from head */
	for (i = 0; i < CMDLINE_TEST_BUFSIZE; i++)
		cirbuf_add_head_safe(&cb, 'h');
	/* verify that contents of the buffer are what they are supposed to be */
	for (i = 0; i < sizeof(buf); i++) {
		if (buf[i] != 'h') {
			printf("Error: wrong content in buffer!\n");
			return -1;
		}
	}
	/* try to add to a full buffer from head */
	if (cirbuf_add_head_safe(&cb,'h') >= 0) {
		printf("Error: buffer should have been full!\n");
		return -1;
	}
	/* try to add to a full buffer from tail */
	if (cirbuf_add_tail_safe(&cb, 't') == 0) {
		printf("Error: buffer should have been full!\n");
		return -1;
	}
	/* delete buffer from head */
	for(i = 0; i < CMDLINE_TEST_BUFSIZE; i++)
		cirbuf_del_head_safe(&cb);
	/* try to delete from an empty buffer */
	if (cirbuf_del_head_safe(&cb) >= 0) {
		printf("Error: buffer should have been empty!\n");
		return -1;
	}

	/*
	 * fill the buffer from both head and tail, with alternating characters,
	 * verify contents and clear the buffer
	 */

	/* fill half of buffer from tail */
	for (i = 0; i < CMDLINE_TEST_BUFSIZE / 2; i++)
		cirbuf_add_tail_safe(&cb, (char) (i % 2 ? 't' : 'T'));
	/* fill other half of the buffer from head */
	for (i = 0; i < CMDLINE_TEST_BUFSIZE / 2; i++)
		cirbuf_add_head_safe(&cb, (char) (i % 2 ? 'H' : 'h')); /* added in reverse */

	/* verify that contents of the buffer are what they are supposed to be */
	for (i = 0; i < sizeof(buf) / 2; i++) {
		if (buf[i] != (char) (i % 2 ? 't' : 'T')) {
			printf("Error: wrong content in buffer at %u!\n", i);
			return -1;
		}
	}
	for (i = sizeof(buf) / 2; i < sizeof(buf); i++) {
		if (buf[i] != (char) (i % 2 ? 'h' : 'H')) {
			printf("Error: wrong content in buffer %u!\n", i);
			return -1;
		}
	}

	return 0;
}

/* test left alignment */
static int
test_cirbuf_align_left(void)
{
#define HALF_OFFSET CMDLINE_TEST_BUFSIZE / 2
#define SMALL_OFFSET HALF_OFFSET / 2
/* resulting buffer lengths for each of the test cases */
#define LEN1 HALF_OFFSET - SMALL_OFFSET - 1
#define LEN2 HALF_OFFSET + SMALL_OFFSET + 2
#define LEN3 HALF_OFFSET - SMALL_OFFSET
#define LEN4 HALF_OFFSET + SMALL_OFFSET - 1

	struct cirbuf cb;
	char buf[CMDLINE_TEST_BUFSIZE];
	char tmp[CMDLINE_TEST_BUFSIZE];
	unsigned i;

	/*
	 * align left when start < end and start in left half
	 */

	/*
	 * initialize circular buffer
	 */
	memset(buf, 0, sizeof(buf));
	if (cirbuf_init(&cb, buf, 0, sizeof(buf)) < 0) {
		printf("Error: failed to initialize circular buffer!\n");
		return -1;
	}

	/* push end into left half */
	for (i = 0; i < HALF_OFFSET - 1; i++)
		cirbuf_add_tail_safe(&cb, 't');

	/* push start into left half < end */
	for (i = 0; i < SMALL_OFFSET; i++)
		cirbuf_del_head_safe(&cb);

	/* align */
	if (cirbuf_align_left(&cb) < 0) {
		printf("Error: alignment failed!\n");
		return -1;
	}

	/* verify result */
	if (cb.start != 0 || cb.len != LEN1 || cb.end != cb.len - 1) {
		printf("Error: buffer alignment is wrong!\n");
		return -1;
	}

	/*
	 * align left when start > end and start in left half
	 */

	/*
	 * reinitialize circular buffer
	 */
	memset(buf, 0, sizeof(buf));
	if (cirbuf_init(&cb, buf, 0, sizeof(buf)) < 0) {
		printf("Error: failed to reinitialize circular buffer!\n");
		return -1;
	}

	/* push start into left half */
	for (i = 0; i < HALF_OFFSET + 2; i++)
		cirbuf_add_head_safe(&cb, 'h');

	/* push end into left half > start */
	for (i = 0; i < SMALL_OFFSET; i++)
		cirbuf_add_tail_safe(&cb, 't');

	/* align */
	if (cirbuf_align_left(&cb) < 0) {
		printf("Error: alignment failed!\n");
		return -1;
	}

	/* verify result */
	if (cb.start != 0 || cb.len != LEN2 || cb.end != cb.len - 1) {
		printf("Error: buffer alignment is wrong!");
		return -1;
	}

	/*
	 * align left when start < end and start in right half
	 */

	/*
	 * reinitialize circular buffer
	 */
	memset(buf, 0, sizeof(buf));
	if (cirbuf_init(&cb, buf, 0, sizeof(buf)) < 0) {
		printf("Error: failed to reinitialize circular buffer!\n");
		return -1;
	}

	/* push start into the right half */
	for (i = 0; i < HALF_OFFSET; i++)
		cirbuf_add_head_safe(&cb, 'h');

	/* push end into left half > start */
	for (i = 0; i < SMALL_OFFSET; i++)
		cirbuf_del_tail_safe(&cb);

	/* align */
	if (cirbuf_align_left(&cb) < 0) {
		printf("Error: alignment failed!\n");
		return -1;
	}

	/* verify result */
	if (cb.start != 0 || cb.len != LEN3 || cb.end != cb.len - 1) {
		printf("Error: buffer alignment is wrong!");
		return -1;
	}

	/*
	 * align left when start > end and start in right half
	 */

	/*
	 * reinitialize circular buffer
	 */
	memset(buf, 0, sizeof(buf));
	if (cirbuf_init(&cb, buf, 0, sizeof(buf)) < 0) {
		printf("Error: failed to reinitialize circular buffer!\n");
		return -1;
	}

	/* push start into the right half */
	for (i = 0; i < HALF_OFFSET - 1; i++)
		cirbuf_add_head_safe(&cb, 'h');

	/* push end into left half < start */
	for (i = 0; i < SMALL_OFFSET; i++)
		cirbuf_add_tail_safe(&cb, 't');

	/* align */
	if (cirbuf_align_left(&cb) < 0) {
		printf("Error: alignment failed!\n");
		return -1;
	}

	/* verify result */
	if (cb.start != 0 || cb.len != LEN4 ||
			cb.end != cb.len - 1) {
		printf("Error: buffer alignment is wrong!");
		return -1;
	}

	/*
	 * Verify that alignment doesn't corrupt data
	 */

	/*
	 * reinitialize circular buffer
	 */
	memset(buf, 0, sizeof(buf));
	if (cirbuf_init(&cb, buf, 0, sizeof(buf)) < 0) {
		printf("Error: failed to reinitialize circular buffer!\n");
		return -1;
	}

	/* add string to tail and head */
	if (cirbuf_add_buf_head(&cb, CIRBUF_STR_HEAD,
			sizeof(CIRBUF_STR_HEAD)) < 0 || cirbuf_add_buf_tail(&cb,
					CIRBUF_STR_TAIL, sizeof(CIRBUF_STR_TAIL)) < 0) {
		printf("Error: failed to add strings!\n");
		return -1;
	}

	/* align */
	if (cirbuf_align_left(&cb) < 0) {
		printf("Error: alignment failed!\n");
		return -1;
	}

	/* get string from head */
	if (cirbuf_get_buf_head(&cb, tmp,
			sizeof(CIRBUF_STR_HEAD) + sizeof(CIRBUF_STR_TAIL)) < 0) {
		printf("Error: failed to read string from head!\n");
		return -1;
	}

	/* verify string */
	if (strncmp(tmp, CIRBUF_STR_HEAD "\0" CIRBUF_STR_TAIL,
			sizeof(CIRBUF_STR_HEAD) + sizeof(CIRBUF_STR_TAIL)) != 0) {
		printf("Error: strings mismatch!\n");
		return -1;
	}

	/* reset tmp buffer */
	memset(tmp, 0, sizeof(tmp));

	/* get string from tail */
	if (cirbuf_get_buf_tail(&cb, tmp,
			sizeof(CIRBUF_STR_HEAD) + sizeof(CIRBUF_STR_TAIL)) < 0) {
		printf("Error: failed to read string from head!\n");
		return -1;
	}

	/* verify string */
	if (strncmp(tmp, CIRBUF_STR_HEAD "\0" CIRBUF_STR_TAIL,
			sizeof(CIRBUF_STR_HEAD) + sizeof(CIRBUF_STR_TAIL)) != 0) {
		printf("Error: strings mismatch!\n");
		return -1;
	}

	return 0;
}

/* test right alignment */
static int
test_cirbuf_align_right(void)
{
#define END_OFFSET CMDLINE_TEST_BUFSIZE - 1
	struct cirbuf cb;
	char buf[CMDLINE_TEST_BUFSIZE];
	char tmp[CMDLINE_TEST_BUFSIZE];
	unsigned i;


	/*
	 * align right when start < end and start in left half
	 */

	/*
	 * initialize circular buffer
	 */
	memset(buf, 0, sizeof(buf));
	if (cirbuf_init(&cb, buf, 0, sizeof(buf)) < 0) {
		printf("Error: failed to initialize circular buffer!\n");
		return -1;
	}

	/* push end into left half */
	for (i = 0; i < HALF_OFFSET - 1; i++)
		cirbuf_add_tail_safe(&cb, 't');

	/* push start into left half < end */
	for (i = 0; i < SMALL_OFFSET; i++)
		cirbuf_del_head_safe(&cb);

	/* align */
	cirbuf_align_right(&cb);

	/* verify result */
	if (cb.start != END_OFFSET || cb.len != LEN1 || cb.end != cb.len - 2) {
		printf("Error: buffer alignment is wrong!\n");
		return -1;
	}

	/*
	 * align right when start > end and start in left half
	 */

	/*
	 * reinitialize circular buffer
	 */
	memset(buf, 0, sizeof(buf));
	if (cirbuf_init(&cb, buf, 0, sizeof(buf)) < 0) {
		printf("Error: failed to reinitialize circular buffer!\n");
		return -1;
	}

	/* push start into left half */
	for (i = 0; i < HALF_OFFSET + 2; i++)
		cirbuf_add_head_safe(&cb, 'h');

	/* push end into left half > start */
	for (i = 0; i < SMALL_OFFSET; i++)
		cirbuf_add_tail_safe(&cb, 't');

	/* align */
	cirbuf_align_right(&cb);

	/* verify result */
	if (cb.start != END_OFFSET || cb.len != LEN2 || cb.end != cb.len - 2) {
		printf("Error: buffer alignment is wrong!");
		return -1;
	}

	/*
	 * align right when start < end and start in right half
	 */

	/*
	 * reinitialize circular buffer
	 */
	memset(buf, 0, sizeof(buf));
	if (cirbuf_init(&cb, buf, 0, sizeof(buf)) < 0) {
		printf("Error: failed to reinitialize circular buffer!\n");
		return -1;
	}

	/* push start into the right half */
	for (i = 0; i < HALF_OFFSET; i++)
		cirbuf_add_head_safe(&cb, 'h');

	/* push end into left half > start */
	for (i = 0; i < SMALL_OFFSET; i++)
		cirbuf_del_tail_safe(&cb);

	/* align */
	cirbuf_align_right(&cb);

	/* verify result */
	if (cb.end != END_OFFSET || cb.len != LEN3 || cb.start != cb.end - cb.len + 1) {
		printf("Error: buffer alignment is wrong!");
		return -1;
	}

	/*
	 * align right when start > end and start in right half
	 */

	/*
	 * reinitialize circular buffer
	 */
	memset(buf, 0, sizeof(buf));
	if (cirbuf_init(&cb, buf, 0, sizeof(buf)) < 0) {
		printf("Error: failed to reinitialize circular buffer!\n");
		return -1;
	}

	/* push start into the right half */
	for (i = 0; i < HALF_OFFSET - 1; i++)
		cirbuf_add_head_safe(&cb, 'h');

	/* push end into left half < start */
	for (i = 0; i < SMALL_OFFSET; i++)
		cirbuf_add_tail_safe(&cb, 't');

	/* align */
	cirbuf_align_right(&cb);

	/* verify result */
	if (cb.end != END_OFFSET || cb.len != LEN4 || cb.start != cb.end - cb.len + 1) {
		printf("Error: buffer alignment is wrong!");
		return -1;
	}

	/*
	 * Verify that alignment doesn't corrupt data
	 */

	/*
	 * reinitialize circular buffer
	 */
	memset(buf, 0, sizeof(buf));
	if (cirbuf_init(&cb, buf, 0, sizeof(buf)) < 0) {
		printf("Error: failed to reinitialize circular buffer!\n");
		return -1;
	}

	/* add string to tail and head */
	if (cirbuf_add_buf_tail(&cb, CIRBUF_STR_TAIL,
			sizeof(CIRBUF_STR_TAIL)) < 0 || cirbuf_add_buf_head(&cb,
					CIRBUF_STR_HEAD, sizeof(CIRBUF_STR_HEAD)) < 0) {
		printf("Error: failed to add strings!\n");
		return -1;
	}

	/* align */
	if (cirbuf_align_right(&cb) < 0) {
		printf("Error: alignment failed!\n");
		return -1;
	}

	/* get string from head */
	if (cirbuf_get_buf_head(&cb, tmp,
			sizeof(CIRBUF_STR_HEAD) + sizeof(CIRBUF_STR_TAIL)) < 0) {
		printf("Error: failed to read string from head!\n");
		return -1;
	}

	/* verify string */
	if (strncmp(tmp, CIRBUF_STR_HEAD "\0" CIRBUF_STR_TAIL,
			sizeof(CIRBUF_STR_HEAD) + sizeof(CIRBUF_STR_TAIL)) != 0) {
		printf("Error: strings mismatch!\n");
		return -1;
	}

	/* reset tmp buffer */
	memset(tmp, 0, sizeof(tmp));

	/* get string from tail */
	if (cirbuf_get_buf_tail(&cb, tmp,
			sizeof(CIRBUF_STR_HEAD) + sizeof(CIRBUF_STR_TAIL)) < 0) {
		printf("Error: failed to read string from head!\n");
		return -1;
	}
	/* verify string */
	if (strncmp(tmp, CIRBUF_STR_HEAD "\0" CIRBUF_STR_TAIL,
			sizeof(CIRBUF_STR_HEAD) + sizeof(CIRBUF_STR_TAIL)) != 0) {
		printf("Error: strings mismatch!\n");
		return -1;
	}

	return 0;
}

/* call functions with invalid parameters */
int
test_cirbuf_invalid_param(void)
{
	struct cirbuf cb;
	char buf[CMDLINE_TEST_BUFSIZE];

	/* null cirbuf */
	if (cirbuf_init(0, buf, 0, sizeof(buf)) == 0)
		return -1;
	/* null buffer */
	if (cirbuf_init(&cb, 0, 0, sizeof(buf)) == 0)
		return -1;
	/* null cirbuf */
	if (cirbuf_add_head_safe(0, 'h') == 0)
		return -1;
	if (cirbuf_add_tail_safe(0, 't') == 0)
		return -1;
	if (cirbuf_del_head_safe(0) == 0)
		return -1;
	if (cirbuf_del_tail_safe(0) == 0)
		return -1;
	/* null buffer */
	if (cirbuf_add_buf_head(&cb, 0, 0) == 0)
		return -1;
	if (cirbuf_add_buf_tail(&cb, 0, 0) == 0)
		return -1;
	/* null cirbuf */
	if (cirbuf_add_buf_head(0, buf, 0) == 0)
		return -1;
	if (cirbuf_add_buf_tail(0, buf, 0) == 0)
		return -1;
	/* null size */
	if (cirbuf_add_buf_head(&cb, buf, 0) == 0)
		return -1;
	if (cirbuf_add_buf_tail(&cb, buf, 0) == 0)
		return -1;
	/* null cirbuf */
	if (cirbuf_del_buf_head(0, 0) == 0)
		return -1;
	if (cirbuf_del_buf_tail(0, 0) == 0)
		return -1;
	/* null size */
	if (cirbuf_del_buf_head(&cb, 0) == 0)
		return -1;
	if (cirbuf_del_buf_tail(&cb, 0) == 0)
		return -1;
	/* null cirbuf */
	if (cirbuf_get_buf_head(0, 0, 0) == 0)
		return -1;
	if (cirbuf_get_buf_tail(0, 0, 0) == 0)
		return -1;
	/* null buffer */
	if (cirbuf_get_buf_head(&cb, 0, 0) == 0)
		return -1;
	if (cirbuf_get_buf_tail(&cb, 0, 0) == 0)
		return -1;
	/* null size, this is valid but should return 0 */
	if (cirbuf_get_buf_head(&cb, buf, 0) != 0)
		return -1;
	if (cirbuf_get_buf_tail(&cb, buf, 0) != 0)
		return -1;
	/* null cirbuf */
	if (cirbuf_align_left(0) == 0)
		return -1;
	if (cirbuf_align_right(0) == 0)
		return -1;

	return 0;
}

/* test cmdline_cirbuf char functions */
int
test_cirbuf_char(void)
{
	int ret;

	ret = test_cirbuf_char_add_del();
	if (ret < 0)
		return -1;

	ret = test_cirbuf_char_fill();
	if (ret < 0)
		return -1;

	return 0;
}

/* test cmdline_cirbuf string functions */
int
test_cirbuf_string(void)
{
	if (test_cirbuf_string_add_del() < 0)
		return -1;

	if (test_cirbuf_string_add_del_reverse() < 0)
		return -1;

	if (test_cirbuf_string_add_boundaries() < 0)
		return -1;

	if (test_cirbuf_string_get_del_boundaries() < 0)
		return -1;

	if (test_cirbuf_string_get_del_partial() < 0)
		return -1;

	if (test_cirbuf_string_misc() < 0)
		return -1;

	return 0;
}

/* test cmdline_cirbuf align functions */
int
test_cirbuf_align(void)
{
	if (test_cirbuf_align_left() < 0)
		return -1;
	if (test_cirbuf_align_right() < 0)
		return -1;
	return 0;
}
