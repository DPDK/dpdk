/*
 * Copyright 2008 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 *
 * Copyright (c) 2014, Cisco Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in
 * the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "enic_compat.h"
#include "vnic_rss.h"

void vnic_set_rss_key(union vnic_rss_key *rss_key, u8 *key)
{
	u32 i;
	u32 *p;
	u16 *q;

	for (i = 0; i < 4; ++i) {
		p = (u32 *)(key + (10 * i));
		iowrite32(*p++, &rss_key->key[i].b[0]);
		iowrite32(*p++, &rss_key->key[i].b[4]);
		q = (u16 *)p;
		iowrite32(*q, &rss_key->key[i].b[8]);
	}
}

void vnic_set_rss_cpu(union vnic_rss_cpu *rss_cpu, u8 *cpu)
{
	u32 i;
	u32 *p = (u32 *)cpu;

	for (i = 0; i < 32; ++i)
		iowrite32(*p++, &rss_cpu->cpu[i].b[0]);
}

void vnic_get_rss_key(union vnic_rss_key *rss_key, u8 *key)
{
	u32 i;
	u32 *p;
	u16 *q;

	for (i = 0; i < 4; ++i) {
		p = (u32 *)(key + (10 * i));
		*p++ = ioread32(&rss_key->key[i].b[0]);
		*p++ = ioread32(&rss_key->key[i].b[4]);
		q = (u16 *)p;
		*q = (u16)ioread32(&rss_key->key[i].b[8]);
	}
}

void vnic_get_rss_cpu(union vnic_rss_cpu *rss_cpu, u8 *cpu)
{
	u32 i;
	u32 *p = (u32 *)cpu;

	for (i = 0; i < 32; ++i)
		*p++ = ioread32(&rss_cpu->cpu[i].b[0]);
}
