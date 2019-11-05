..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 NXP

NXP DPAA2 QDMA Driver
=====================

The DPAA2 QDMA is an implementation of the rawdev API, that provide means
to initiate a DMA transaction from CPU. The initiated DMA is performed
without CPU being involved in the actual DMA transaction. This is achieved
via using the DPDMAI device exposed by MC.

More information can be found at `NXP Official Website
<http://www.nxp.com/products/microcontrollers-and-processors/arm-processors/qoriq-arm-processors:QORIQ-ARM>`_.

Features
--------

The DPAA2 QDMA implements following features in the rawdev API;

- Supports issuing DMA of data within memory without hogging CPU while
  performing DMA operation.
- Supports configuring to optionally get status of the DMA translation on
  per DMA operation basis.

Supported DPAA2 SoCs
--------------------

- LX2160A
- LS2084A/LS2044A
- LS2088A/LS2048A
- LS1088A/LS1048A

Prerequisites
-------------

See :doc:`../platform/dpaa2` for setup information

Currently supported by DPDK:

- NXP SDK **19.09+**.
- MC Firmware version **10.18.0** and higher.
- Supported architectures:  **arm64 LE**.

- Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup the basic DPDK environment.

.. note::

   Some part of fslmc bus code (mc flib - object library) routines are
   dual licensed (BSD & GPLv2).

Pre-Installation Configuration
------------------------------

Config File Options
~~~~~~~~~~~~~~~~~~~

The following options can be modified in the ``config`` file.

- ``CONFIG_RTE_LIBRTE_PMD_DPAA2_QDMA_RAWDEV`` (default ``y``)

  Toggle compilation of the ``lrte_pmd_dpaa2_qdma`` driver.

Enabling logs
-------------

For enabling logs, use the following EAL parameter:

.. code-block:: console

   ./your_qdma_application <EAL args> --log-level=pmd.raw.dpaa2.qdma,<level>

Using ``pmd.raw.dpaa2.qdma`` as log matching criteria, all Event PMD logs can be
enabled which are lower than logging ``level``.

Driver Compilation
~~~~~~~~~~~~~~~~~~

To compile the DPAA2 QDMA PMD for Linux arm64 gcc target, run the
following ``make`` command:

.. code-block:: console

   cd <DPDK-source-directory>
   make config T=arm64-dpaa-linux-gcc install

Initialization
--------------

The DPAA2 QDMA is exposed as a vdev device which consists of dpdmai devices.
On EAL initialization, dpdmai devices will be probed and populated into the
rawdevices. The rawdev ID of the device can be obtained using

* Invoking ``rte_rawdev_get_dev_id("dpdmai.x")`` from the application
  where x is the object ID of the DPDMAI object created by MC. Use can
  use this index for further rawdev function calls.

Platform Requirement
~~~~~~~~~~~~~~~~~~~~

DPAA2 drivers for DPDK can only work on NXP SoCs as listed in the
``Supported DPAA2 SoCs``.
