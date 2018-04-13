..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2016-2018 Broadcom

BNXT Poll Mode Driver
=====================

The bnxt poll mode library (**librte_pmd_bnxt**) implements support for:

  * **Broadcom NetXtreme-C®/NetXtreme-E®/NetXtreme-S®
    BCM5730X / BCM574XX / BCM58000 family of Ethernet Network Controllers**

    These adapters support Standards compliant 10/25/50/100Gbps 30MPPS
    full-duplex throughput.

    Information about the NetXtreme family of adapters can be found in the
    `NetXtreme® Brand section
    <https://www.broadcom.com/products/ethernet-connectivity/controllers/>`_
    of the `Broadcom website <http://www.broadcom.com/>`_.

  * **Broadcom StrataGX® BCM5871X Series of Communucations Processors**

    These ARM based processors target a broad range of networking applications
    including virtual CPE (vCPE) and NFV appliances, 10G service routers and
    gateways, control plane processing for Ethernet switches and network
    attached storage (NAS).

    Information about the StrataGX family of adapters can be found in the
    `StrataGX® BCM5871X Series section
    <http://www.broadcom.com/products/enterprise-and-network-processors/processors/bcm58712>`_
    of the `Broadcom website <http://www.broadcom.com/>`_.

Limitations
-----------

With the current driver, allocated mbufs must be large enough to hold
the entire received frame.  If the mbufs are not large enough, the
packets will be dropped.  This is most limiting when jumbo frames are
used.
