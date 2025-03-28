.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2023 Mucse IC Design Ltd.

RNP Poll Mode driver
====================

The RNP ethdev PMD (**librte_net_rnp**) provides poll mode ethdev driver
support for the inbuilt network device found in the **Mucse RNP**

More information can be found at `Mucse, Official Website <https://mucse.com/en/pro/pro.aspx>`_.


Supported Chipsets and NICs
---------------------------

- MUCSE Ethernet Controller N10 Series for 10GbE or 40GbE (Dual-port)


Chip Basic Overview
-------------------

N10 has two functions, each function support multiple ports (1 to 8),
which is different of normal PCIe network card (one PF for each port).

.. _figure_mucse_nic:

.. figure:: img/mucse_nic_port.*

   rnp Mucse NIC port.


Prerequisites and Pre-conditions
--------------------------------

#. Prepare the system as recommended by DPDK suite.

#. Bind the intended N10 device to ``igb_uio`` or ``vfio-pci`` module.

Now DPDK system is ready to detect N10 ports.


Limitations or Known issues
---------------------------

X86-32, BSD, Armv7, RISC-V, Windows, are not supported yet.
