..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2024 ZTE Corporation.

ZXDH Poll Mode Driver
=====================

The ZXDH PMD (**librte_net_zxdh**) provides poll mode driver support
for 25/100 Gbps ZXDH NX Series Ethernet Controller
based on the ZTE Ethernet Controller E310/E312.

More information on
`ZXDH NX Series Ethernet Controller NICs
<https://enterprise.zte.com.cn/sup-detail.html?id=271&suptype=1>`_.


Features
--------

Features of the ZXDH PMD are:

- Multi arch support: x86_64, ARMv8.
- Multiple queues for Tx and Rx
- SR-IOV VF
- Scatter and gather for Tx and Rx
- Link auto-negotiation
- Link state information
- Set link down or up
- Unicast MAC filter
- Multicast MAC filter
- Promiscuous mode
- Multicast mode
- VLAN filter and VLAN offload
- VLAN stripping and inserting
- QINQ stripping and inserting
- Receive Side Scaling (RSS)
- Port hardware statistics
- MTU update
- Jumbo frames
- Inner and Outer Checksum offload
- Hardware LRO
- Hardware TSO for generic IP or UDP tunnel, including VXLAN
- Extended statistics query
- Ingress meter support
- Flow API


Driver compilation and testing
------------------------------

Refer to the document :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
for details.


Limitations or Known issues
---------------------------

X86-32, Power8, ARMv7, RISC-V-32, Windows and BSD are not supported yet.
