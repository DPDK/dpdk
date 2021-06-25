.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2021 Marvell.

Marvell cnxk Crypto Poll Mode Driver
====================================

The cnxk crypto poll mode driver provides support for offloading
cryptographic operations to cryptographic accelerator units on the
**Marvell OCTEON cnxk** SoC family.

The cnxk crypto PMD code is organized into different sets of files.
The file names starting with cn9k and cn10k provides support for CN9XX
and CN10XX respectively. The common code between the SoCs is present
in file names starting with cnxk.

More information about OCTEON cnxk SoCs may be obtained from `<https://www.marvell.com>`_

Supported OCTEON cnxk SoCs
--------------------------

- CN9XX
- CN10XX

Installation
------------

The OCTEON cnxk crypto PMD may be compiled natively on an OCTEON cnxk platform
or cross-compiled on an x86 platform.

Refer to :doc:`../platform/cnxk` for instructions to build your DPDK
application.

.. note::

   The OCTEON cnxk crypto PMD uses services from the kernel mode OCTEON cnxk
   crypto PF driver in linux. This driver is included in the OCTEON TX SDK.

Initialization
--------------

``CN9K Initialization``

List the CPT PF devices available on cn9k platform:

.. code-block:: console

    lspci -d:a0fd

``a0fd`` is the CPT PF device id. You should see output similar to:

.. code-block:: console

    0002:10:00.0 Class 1080: Device 177d:a0fd

Set ``sriov_numvfs`` on the CPT PF device, to create a VF:

.. code-block:: console

    echo 1 > /sys/bus/pci/devices/0002:10:00.0/sriov_numvfs

Bind the CPT VF device to the vfio_pci driver:

.. code-block:: console

    cd <dpdk directory>
    ./usertools/dpdk-devbind.py -u 0002:10:00.1
    ./usertools/dpdk-devbind.py -b vfio-pci 0002:10.00.1

.. note::

    * For CN98xx SoC, it is recommended to use even and odd DBDF VFs to achieve
      higher performance as even VF uses one crypto engine and odd one uses
      another crypto engine.

    * Ensure that sufficient huge pages are available for your application::

         dpdk-hugepages.py --setup 4G --pagesize 512M

      Refer to :ref:`linux_gsg_hugepages` for more details.

``CN10K Initialization``

List the CPT PF devices available on cn10k platform:

.. code-block:: console

    lspci -d:a0f2

``a0f2`` is the CPT PF device id. You should see output similar to:

.. code-block:: console

    0002:20:00.0 Class 1080: Device 177d:a0f2

Set ``sriov_numvfs`` on the CPT PF device, to create a VF:

.. code-block:: console

    echo 1 > /sys/bus/pci/devices/0002:20:00.0/sriov_numvfs

Bind the CPT VF device to the vfio_pci driver:

.. code-block:: console

    cd <dpdk directory>
    ./usertools/dpdk-devbind.py -u 0002:20:00.1
    ./usertools/dpdk-devbind.py -b vfio-pci 0002:20:00.1

Debugging Options
-----------------

.. _table_octeon_cnxk_crypto_debug_options:

.. table:: OCTEON cnxk crypto PMD debug options

    +---+------------+-------------------------------------------------------+
    | # | Component  | EAL log command                                       |
    +===+============+=======================================================+
    | 1 | CPT        | --log-level='pmd\.crypto\.cnxk,8'                     |
    +---+------------+-------------------------------------------------------+

Limitations
-----------

Multiple lcores may not operate on the same crypto queue pair. The lcore that
enqueues to a queue pair is the one that must dequeue from it.
