/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium Inc. 2017. All rights reserved.
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
 *     * Neither the name of Cavium networks nor the names of its
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

#ifndef	__OCTEONTX_PKI_H__
#define	__OCTEONTX_PKI_H__

#include <stdint.h>

#include <rte_pmd_octeontx_ssovf.h>

#define OCTEONTX_PKI_COPROC                     5

/* PKI messages */

#define MBOX_PKI_PORT_OPEN			1
#define MBOX_PKI_PORT_START			2
#define MBOX_PKI_PORT_STOP			3
#define MBOX_PKI_PORT_CLOSE			4

/* Interface types: */
enum {
	OCTTX_PORT_TYPE_NET, /* Network interface ports */
	OCTTX_PORT_TYPE_INT, /* CPU internal interface ports */
	OCTTX_PORT_TYPE_PCI, /* DPI/PCIe interface ports */
	OCTTX_PORT_TYPE_MAX
};

/* pki port config */
typedef struct mbox_pki_port_type {
	uint8_t port_type;
} mbox_pki_port_t;

int octeontx_pki_port_open(int port);
int octeontx_pki_port_close(int port);
int octeontx_pki_port_start(int port);
int octeontx_pki_port_stop(int port);

#endif /* __OCTEONTX_PKI_H__ */
