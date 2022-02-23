.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2020 Mellanox Technologies, Ltd

.. include:: <isonum.txt>

MLX5 RegEx driver
=================

The MLX5 RegEx (Regular Expression) driver library
(**librte_regex_mlx5**) provides support for **Mellanox BlueField-2**
families of 25/50/100/200 Gb/s adapters.

Design
------

This PMD is configuring the RegEx HW engine.
For the PMD to work, the application must supply
a precompiled rule file in rof2 format.

The PMD uses libibverbs and libmlx5 to access the device firmware
or directly the hardware components.
There are different levels of objects and bypassing abilities
to get the best performances:

- Verbs is a complete high-level generic API
- Direct Verbs is a device-specific API
- DevX allows to access firmware objects

Enabling librte_regex_mlx5 causes DPDK applications to be linked against
libibverbs.

Mellanox mlx5 pci device can be probed by number of different pci devices,
for example net / vDPA / RegEx. To select the RegEx PMD ``class=regex`` should
be specified as device parameter. The RegEx device can be probed and used with
other Mellanox devices, by adding more options in the class.
For example: ``class=net:regex`` will probe both the net PMD and the RegEx PMD.

Features
--------

- Multi segments mbuf support.

Supported NICs
--------------

* Mellanox\ |reg| BlueField-2 SmartNIC

Prerequisites
-------------

- BlueField-2 running Mellanox supported kernel.
- Enable the RegEx capabilities using system call from the BlueField-2.
- Official support is not yet released.

Limitations
-----------

- The firmware version must be greater than XX.31.0364

Run-time configuration
~~~~~~~~~~~~~~~~~~~~~~

- **ethtool** operations on related kernel interfaces also affect the PMD.
