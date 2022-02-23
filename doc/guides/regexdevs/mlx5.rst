.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2020 Mellanox Technologies, Ltd

.. include:: <isonum.txt>

MLX5 RegEx Driver
=================

The mlx5 RegEx (Regular Expression) driver library
(**librte_regex_mlx5**) provides support for **Mellanox BlueField-2**
families of 25/50/100/200 Gb/s adapters.

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

* Mellanox\ |reg| BlueField-2 SmartNIC

Prerequisites
-------------

- BlueField-2 running Mellanox supported kernel.
- Enable the RegEx capabilities using system call from the BlueField-2.
- Official support is not yet released.


Limitations
-----------

- The firmware version must be greater than XX.31.0364
