..  BSD LICENSE
    Copyright 2016 Broadcom Limited

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of Broadcom Limited nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

BNXT Poll Mode Driver
=====================

The bnxt poll mode library (**librte_pmd_bnxt**) implements support for:

  * **Broadcom NetXtreme-C®/NetXtreme-E® BCM5730X and BCM574XX family of
    Ethernet Network Controllers**

    These adapters support Standards compliant 10/25/50/100Gbps 30MPPS
    full-duplex throughput.

    Information about the NetXtreme family of adapters can be found in the
    `NetXtreme® Brand section
    <https://www.broadcom.com/products/ethernet-communication-and-switching?technology%5B%5D=88>`_
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
