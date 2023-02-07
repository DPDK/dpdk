..  SPDX-License-Identifier: BSD-3-Clause
    Copyright (c) 2022 Marvell.

Marvell cnxk Machine Learning Poll Mode Driver
==============================================

The cnxk ML poll mode driver provides support for offloading
Machine Learning inference operations to Machine Learning accelerator units
on the **Marvell OCTEON cnxk** SoC family.

The cnxk ML PMD code is organized into multiple files with all file names
starting with cn10k, providing support for CN106XX and CN106XXS.

More information about OCTEON cnxk SoCs may be obtained from `<https://www.marvell.com>`_

Supported OCTEON cnxk SoCs
--------------------------

- CN106XX
- CN106XXS

Features
--------

The OCTEON cnxk ML PMD provides support for the following set of operations:


Installation
------------

The OCTEON cnxk ML PMD may be compiled natively on an OCTEON cnxk platform
or cross-compiled on an x86 platform.

Refer to :doc:`../platform/cnxk` for instructions to build your DPDK application.


Initialization
--------------

List the ML PF devices available on cn10k platform:

.. code-block:: console

   lspci -d:a092

``a092`` is the ML device PF id. You should see output similar to:

.. code-block:: console

   0000:00:10.0 System peripheral: Cavium, Inc. Device a092

Bind the ML PF device to the vfio_pci driver:

.. code-block:: console

   cd <dpdk directory>
   usertools/dpdk-devbind.py -u 0000:00:10.0
   usertools/dpdk-devbind.py -b vfio-pci 0000:00:10.0


Debugging Options
-----------------

.. _table_octeon_cnxk_ml_debug_options:

.. table:: OCTEON cnxk ML PMD debug options

   +---+------------+-------------------------------------------------------+
   | # | Component  | EAL log command                                       |
   +===+============+=======================================================+
   | 1 | ML         | --log-level='pmd\.ml\.cnxk,8'                         |
   +---+------------+-------------------------------------------------------+
