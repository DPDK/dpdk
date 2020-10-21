..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2017 NXP


NXP DPAA2 Eventdev Driver
=========================

The dpaa2 eventdev is an implementation of the eventdev API, that provides a
wide range of the eventdev features. The eventdev relies on a dpaa2 hw to
perform event scheduling.

More information can be found at `NXP Official Website
<http://www.nxp.com/products/microcontrollers-and-processors/arm-processors/qoriq-arm-processors:QORIQ-ARM>`_.

Features
--------

The DPAA2 EVENTDEV implements many features in the eventdev API;

- Hardware based event scheduler
- 8 event ports
- 8 event queues
- Parallel flows
- Atomic flows

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


Initialization
--------------

The dpaa2 eventdev is exposed as a vdev device which consists of a set of dpcon
devices and dpci devices. On EAL initialization, dpcon and dpci devices will be
probed and then vdev device can be created from the application code by

* Invoking ``rte_vdev_init("event_dpaa2")`` from the application

* Using ``--vdev="event_dpaa2"`` in the EAL options, which will call
  rte_vdev_init() internally

Example:

.. code-block:: console

   ./your_eventdev_application --vdev="event_dpaa2"

Enabling logs
-------------

For enabling logs, use the following EAL parameter:

.. code-block:: console

   ./your_eventdev_application <EAL args> --log-level=pmd.event.dpaa2,<level>

Using ``eventdev.dpaa2`` as log matching criteria, all Event PMD logs can be
enabled which are lower than logging ``level``.

Limitations
-----------

Platform Requirement
~~~~~~~~~~~~~~~~~~~~

DPAA2 drivers for DPDK can only work on NXP SoCs as listed in the
``Supported DPAA2 SoCs``.

Port-core binding
~~~~~~~~~~~~~~~~~

DPAA2 EVENTDEV can support only one eventport per core.
