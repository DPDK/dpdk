/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#include <string.h>

#include <rte_eal.h>
#include <bus_pci_driver.h>

#include "../octeontx_logs.h"
#include "octeontx_io.h"
#include "octeontx_pkivf.h"


struct octeontx_pkivf {
	uint8_t		*bar0;
	uint8_t		status;
	uint16_t	domain;
	uint16_t	vfid;
};

struct octeontx_pki_vf_ctl_s {
	struct octeontx_pkivf pki[PKI_VF_MAX];
};

static struct octeontx_pki_vf_ctl_s pki_vf_ctl;

int
octeontx_pki_port_open(int port)
{
	uint16_t global_domain = octeontx_get_global_domain();
	struct octeontx_mbox_hdr hdr;
	pki_port_type_t port_type;
	int i, res;

	/* Check if atleast one PKI vf is in application domain. */
	for (i = 0; i < PKI_VF_MAX; i++) {
		if (pki_vf_ctl.pki[i].domain != global_domain)
			continue;
		break;
	}

	if (i == PKI_VF_MAX)
		return -ENODEV;

	port_type.port_type = OCTTX_PORT_TYPE_NET;
	hdr.coproc = OCTEONTX_PKI_COPROC;
	hdr.msg = MBOX_PKI_PORT_OPEN;
	hdr.vfid = port;

	res = octeontx_mbox_send(&hdr, &port_type, sizeof(pki_port_type_t),
				 NULL, 0);
	if (res < 0)
		return -EACCES;
	return res;
}

int
octeontx_pki_port_hash_config(int port, pki_hash_cfg_t *hash_cfg)
{
	struct octeontx_mbox_hdr hdr;
	int res;

	pki_hash_cfg_t h_cfg = *(pki_hash_cfg_t *)hash_cfg;
	int len = sizeof(pki_hash_cfg_t);

	hdr.coproc = OCTEONTX_PKI_COPROC;
	hdr.msg = MBOX_PKI_PORT_HASH_CONFIG;
	hdr.vfid = port;

	res = octeontx_mbox_send(&hdr, &h_cfg, len, NULL, 0);
	if (res < 0)
		return -EACCES;

	return res;
}

int
octeontx_pki_port_pktbuf_config(int port, pki_pktbuf_cfg_t *buf_cfg)
{
	struct octeontx_mbox_hdr hdr;
	int res;

	pki_pktbuf_cfg_t b_cfg = *(pki_pktbuf_cfg_t *)buf_cfg;
	int len = sizeof(pki_pktbuf_cfg_t);

	hdr.coproc = OCTEONTX_PKI_COPROC;
	hdr.msg = MBOX_PKI_PORT_PKTBUF_CONFIG;
	hdr.vfid = port;

	res = octeontx_mbox_send(&hdr, &b_cfg, len, NULL, 0);
	if (res < 0)
		return -EACCES;
	return res;
}

int
octeontx_pki_port_create_qos(int port, pki_qos_cfg_t *qos_cfg)
{
	struct octeontx_mbox_hdr hdr;
	int res;

	pki_qos_cfg_t q_cfg = *(pki_qos_cfg_t *)qos_cfg;
	int len = sizeof(pki_qos_cfg_t);

	hdr.coproc = OCTEONTX_PKI_COPROC;
	hdr.msg = MBOX_PKI_PORT_CREATE_QOS;
	hdr.vfid = port;

	res = octeontx_mbox_send(&hdr, &q_cfg, len, NULL, 0);
	if (res < 0)
		return -EACCES;

	return res;
}


int
octeontx_pki_port_errchk_config(int port, pki_errchk_cfg_t *cfg)
{
	struct octeontx_mbox_hdr hdr;
	int res;

	pki_errchk_cfg_t e_cfg;
	e_cfg = *((pki_errchk_cfg_t *)(cfg));
	int len = sizeof(pki_errchk_cfg_t);

	hdr.coproc = OCTEONTX_PKI_COPROC;
	hdr.msg = MBOX_PKI_PORT_ERRCHK_CONFIG;
	hdr.vfid = port;

	res = octeontx_mbox_send(&hdr, &e_cfg, len, NULL, 0);
	if (res < 0)
		return -EACCES;

	return res;
}

int
octeontx_pki_port_vlan_fltr_config(int port,
				   pki_port_vlan_filter_config_t *fltr_cfg)
{
	struct octeontx_mbox_hdr hdr;
	int res;

	pki_port_vlan_filter_config_t cfg = *fltr_cfg;
	int len = sizeof(pki_port_vlan_filter_config_t);

	hdr.coproc = OCTEONTX_PKI_COPROC;
	hdr.msg = MBOX_PKI_PORT_VLAN_FILTER_CONFIG;
	hdr.vfid = port;

	res = octeontx_mbox_send(&hdr, &cfg, len, NULL, 0);
	if (res < 0)
		return -EACCES;
	return res;
}

int
octeontx_pki_port_vlan_fltr_entry_config(int port,
				   pki_port_vlan_filter_entry_config_t *e_cfg)
{
	struct octeontx_mbox_hdr hdr;
	int res;

	pki_port_vlan_filter_entry_config_t cfg = *e_cfg;
	int len = sizeof(pki_port_vlan_filter_entry_config_t);

	hdr.coproc = OCTEONTX_PKI_COPROC;
	hdr.msg = MBOX_PKI_PORT_VLAN_FILTER_ENTRY_CONFIG;
	hdr.vfid = port;

	res = octeontx_mbox_send(&hdr, &cfg, len, NULL, 0);
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
	struct octeontx_pkivf *res;
	static uint8_t vf_cnt;
	uint16_t domain;
	uint16_t vfid;
	uint8_t *bar0;
	uint64_t val;

	RTE_SET_USED(pci_drv);
	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (pci_dev->mem_resource[0].addr == NULL) {
		octeontx_log_err("PKI Empty bar[0] %p",
				 pci_dev->mem_resource[0].addr);
		return -ENODEV;
	}

	bar0 = pci_dev->mem_resource[0].addr;
	val = octeontx_read64(bar0);
	domain = val & 0xffff;
	vfid = (val >> 16) & 0xffff;

	if (unlikely(vfid >= PKI_VF_MAX)) {
		octeontx_log_err("pki: Invalid vfid %d", vfid);
		return -EINVAL;
	}

	res = &pki_vf_ctl.pki[vf_cnt++];
	res->vfid = vfid;
	res->domain = domain;
	res->bar0 = bar0;

	octeontx_log_dbg("PKI Domain=%d vfid=%d", res->domain, res->vfid);
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
