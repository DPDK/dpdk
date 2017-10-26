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
#include <string.h>

#include <rte_eal.h>
#include <rte_bus_pci.h>

#include "octeontx_pkivf.h"

int
octeontx_pki_port_open(int port)
{
	struct octeontx_mbox_hdr hdr;
	int res;

	hdr.coproc = OCTEONTX_PKI_COPROC;
	hdr.msg = MBOX_PKI_PORT_OPEN;
	hdr.vfid = port;

	res = octeontx_ssovf_mbox_send(&hdr, NULL, 0, NULL, 0);
	if (res < 0)
		return -EACCES;
	return res;
}

int
octeontx_pki_port_hash_config(int port, pki_hash_cfg_t *hash_cfg)
{
	struct octeontx_mbox_hdr hdr;
	int res;

	mbox_pki_hash_cfg_t h_cfg = *(mbox_pki_hash_cfg_t *)hash_cfg;
	int len = sizeof(mbox_pki_hash_cfg_t);

	hdr.coproc = OCTEONTX_PKI_COPROC;
	hdr.msg = MBOX_PKI_PORT_HASH_CONFIG;
	hdr.vfid = port;

	res = octeontx_ssovf_mbox_send(&hdr, &h_cfg, len, NULL, 0);
	if (res < 0)
		return -EACCES;

	return res;
}

int
octeontx_pki_port_pktbuf_config(int port, pki_pktbuf_cfg_t *buf_cfg)
{
	struct octeontx_mbox_hdr hdr;
	int res;

	mbox_pki_pktbuf_cfg_t b_cfg = *(mbox_pki_pktbuf_cfg_t *)buf_cfg;
	int len = sizeof(mbox_pki_pktbuf_cfg_t);

	hdr.coproc = OCTEONTX_PKI_COPROC;
	hdr.msg = MBOX_PKI_PORT_PKTBUF_CONFIG;
	hdr.vfid = port;

	res = octeontx_ssovf_mbox_send(&hdr, &b_cfg, len, NULL, 0);
	if (res < 0)
		return -EACCES;
	return res;
}

int
octeontx_pki_port_create_qos(int port, pki_qos_cfg_t *qos_cfg)
{
	struct octeontx_mbox_hdr hdr;
	int res;

	mbox_pki_qos_cfg_t q_cfg = *(mbox_pki_qos_cfg_t *)qos_cfg;
	int len = sizeof(mbox_pki_qos_cfg_t);

	hdr.coproc = OCTEONTX_PKI_COPROC;
	hdr.msg = MBOX_PKI_PORT_CREATE_QOS;
	hdr.vfid = port;

	res = octeontx_ssovf_mbox_send(&hdr, &q_cfg, len, NULL, 0);
	if (res < 0)
		return -EACCES;

	return res;
}


int
octeontx_pki_port_errchk_config(int port, pki_errchk_cfg_t *cfg)
{
	struct octeontx_mbox_hdr hdr;
	int res;

	mbox_pki_errcheck_cfg_t e_cfg;
	e_cfg = *((mbox_pki_errcheck_cfg_t *)(cfg));
	int len = sizeof(mbox_pki_errcheck_cfg_t);

	hdr.coproc = OCTEONTX_PKI_COPROC;
	hdr.msg = MBOX_PKI_PORT_ERRCHK_CONFIG;
	hdr.vfid = port;

	res = octeontx_ssovf_mbox_send(&hdr, &e_cfg, len, NULL, 0);
	if (res < 0)
		return -EACCES;

	return res;
}

#define PCI_VENDOR_ID_CAVIUM               0x177D
#define PCI_DEVICE_ID_OCTEONTX_PKI_VF      0xA0DD

/* PKIVF pcie device */
static int
pkivf_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	RTE_SET_USED(pci_drv);
	RTE_SET_USED(pci_dev);

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	return 0;
}

static const struct rte_pci_id pci_pkivf_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
				PCI_DEVICE_ID_OCTEONTX_PKI_VF)
	},
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver pci_pkivf = {
	.id_table = pci_pkivf_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = pkivf_probe,
};

RTE_PMD_REGISTER_PCI(octeontx_pkivf, pci_pkivf);
