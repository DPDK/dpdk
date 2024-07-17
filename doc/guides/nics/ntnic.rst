..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2024 Napatech A/S

NTNIC Poll Mode Driver
======================

The NTNIC PMD provides poll mode driver support for Napatech smartNICs.


Design
------

The NTNIC PMD is designed as a pure user-space driver,
and requires no special Napatech kernel modules.

The Napatech smartNIC presents one control PCI device (PF0).
NTNIC PMD accesses smartNIC PF0 via vfio-pci kernel driver.
Access to PF0 for all purposes is exclusive,
so only one process should access it.
The physical ports are located behind PF0 as DPDK port 0 and 1.


Supported NICs
--------------

- NT200A02 2x100G SmartNIC

    - FPGA ID 9563 (Inline Flow Management)

All information about NT200A02 can be found by link below:
https://www.napatech.com/products/nt200a02-smartnic-inline/

Features
--------

- FW version
- Speed capabilities
- Link status (Link update only)
- Unicast MAC filter
- Multicast MAC filter
- Promiscuous mode (Enable only. The device always run promiscuous mode)

Limitations
~~~~~~~~~~~

Linux kernel versions before 5.7 are not supported.
Kernel version 5.7 added vfio-pci support for creating VFs from the PF
which is required for the PMD to use vfio-pci on the PF.
This support has been back-ported to older Linux distributions
and they are also supported.
If vfio-pci is not required, kernel version 4.18 is supported.
