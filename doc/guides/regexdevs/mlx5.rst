.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2020 Mellanox Technologies, Ltd

.. include:: <isonum.txt>

NVIDIA MLX5 RegEx Driver
========================

.. note::

   NVIDIA acquired Mellanox Technologies in 2020.
   The DPDK documentation and code might still include instances
   of or references to Mellanox trademarks (like BlueField and ConnectX)
   that are now NVIDIA trademarks.

The mlx5 RegEx (Regular Expression) driver library
(**librte_regex_mlx5**) provides support for **NVIDIA BlueField-2**,
and **NVIDIA BlueField-3** families of 25/50/100/200 Gb/s adapters.

Design
------

This PMD is configuring the RegEx HW engine.
For the PMD to work, the application must supply
a precompiled rule file in rof2 format.

See :doc:`../../platform/mlx5` guide for more design details.

Features
--------

- Multi segments mbuf support.

Configuration
-------------

See :ref:`mlx5 common compilation <mlx5_common_compilation>`,
:ref:`mlx5 firmware configuration <mlx5_firmware_config>`,
and :ref:`mlx5 common driver options <mlx5_common_driver_options>`.


Supported NICs
--------------

* NVIDIA\ |reg| BlueField-2 SmartNIC
* NVIDIA\ |reg| BlueField-3 SmartNIC

Prerequisites
-------------

- BlueField-2 or BlueField-3 running NVIDIA supported kernel.
- Enable the RegEx capabilities using system call from the BlueField-2 or BlueField-3.
- Official support is not yet released.


Limitations
-----------

- The firmware version must be greater than 24.31.0364 for BlueField-2
  and 32.36.xxxx for BlueField-3.
