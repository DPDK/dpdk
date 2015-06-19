#ifndef _RTE_RTM_H_
#define _RTE_RTM_H_ 1

/*
 * Copyright (c) 2012,2013 Intel Corporation
 * Author: Andi Kleen
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/* Official RTM intrinsics interface matching gcc/icc, but works
   on older gcc compatible compilers and binutils. */


#ifdef __cplusplus
extern "C" {
#endif


#define RTE_XBEGIN_STARTED		(~0u)
#define RTE_XABORT_EXPLICIT		(1 << 0)
#define RTE_XABORT_RETRY		(1 << 1)
#define RTE_XABORT_CONFLICT		(1 << 2)
#define RTE_XABORT_CAPACITY		(1 << 3)
#define RTE_XABORT_DEBUG		(1 << 4)
#define RTE_XABORT_NESTED		(1 << 5)
#define RTE_XABORT_CODE(x)		(((x) >> 24) & 0xff)

static __attribute__((__always_inline__)) inline
unsigned int rte_xbegin(void)
{
	unsigned int ret = RTE_XBEGIN_STARTED;

	asm volatile(".byte 0xc7,0xf8 ; .long 0" : "+a" (ret) :: "memory");
	return ret;
}

static __attribute__((__always_inline__)) inline
void rte_xend(void)
{
	 asm volatile(".byte 0x0f,0x01,0xd5" ::: "memory");
}

static __attribute__((__always_inline__)) inline
void rte_xabort(const unsigned int status)
{
	asm volatile(".byte 0xc6,0xf8,%P0" :: "i" (status) : "memory");
}

static __attribute__((__always_inline__)) inline
int rte_xtest(void)
{
	unsigned char out;

	asm volatile(".byte 0x0f,0x01,0xd6 ; setnz %0" :
		"=r" (out) :: "memory");
	return out;
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_RTM_H_ */
