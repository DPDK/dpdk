..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2024 ZTE Corporation.

ZXDH Poll Mode Driver
=====================

The ZXDH PMD (**librte_net_zxdh**) provides poll mode driver support
for 25/100 Gbps ZXDH NX Series Ethernet Controller
based on the ZTE Ethernet Controller E310/E312.

- Learn about ZXDH NX Series Ethernet Controller NICs using
  `<https://enterprise.zte.com.cn/sup-detail.html?id=271&suptype=1>`_.


Features
--------

Features of the ZXDH PMD are:

- Multi arch support: x86_64, ARMv8.


Driver compilation and testing
------------------------------

Refer to the document :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
for details.


Limitations or Known issues
---------------------------

Datapath and some operations are not supported and will be provided later.
X86-32, Power8, ARMv7, RISC-V, Windows and BSD are not supported yet.
