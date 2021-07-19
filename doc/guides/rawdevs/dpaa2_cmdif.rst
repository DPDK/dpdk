..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 NXP

NXP DPAA2 CMDIF Driver
======================

The DPAA2 CMDIF is an implementation of the rawdev API, that provides
communication between the GPP and AIOP (Firmware). This is achieved
via using the DPCI devices exposed by MC for GPP <--> AIOP interaction.

More information can be found at `NXP Official Website
<http://www.nxp.com/products/microcontrollers-and-processors/arm-processors/qoriq-arm-processors:QORIQ-ARM>`_.

Features
--------

The DPAA2 CMDIF implements following features in the rawdev API;

- Getting the object ID of the device (DPCI) using attributes
- I/O to and from the AIOP device using DPCI

Supported DPAA2 SoCs
--------------------

- LS2084A/LS2044A
- LS2088A/LS2048A
- LS1088A/LS1048A

Prerequisites
-------------

See :doc:`../platform/dpaa2` for setup information

- Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup the basic DPDK environment.

.. note::

   Some part of fslmc bus code (mc flib - object library) routines are
   dual licensed (BSD & GPLv2).


Enabling logs
-------------

For enabling logs, use the following EAL parameter:

.. code-block:: console

   ./your_cmdif_application <EAL args> --log-level=pmd.raw.dpaa2.cmdif,<level>

Using ``pmd.raw.dpaa2.cmdif`` as log matching criteria, all Event PMD logs can be
enabled which are lower than logging ``level``.


Initialization
--------------

The DPAA2 CMDIF is exposed as a vdev device which consists of dpci devices.
On EAL initialization, dpci devices will be probed and then vdev device
can be created from the application code by

* Invoking ``rte_vdev_init("dpaa2_dpci")`` from the application

* Using ``--vdev="dpaa2_dpci"`` in the EAL options, which will call
  rte_vdev_init() internally

Example:

.. code-block:: console

    ./your_cmdif_application <EAL args> --vdev="dpaa2_dpci"

Platform Requirement
~~~~~~~~~~~~~~~~~~~~

DPAA2 drivers for DPDK can only work on NXP SoCs as listed in the
``Supported DPAA2 SoCs``.
