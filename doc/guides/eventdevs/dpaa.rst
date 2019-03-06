.. SPDX-License-Identifier:        BSD-3-Clause
   Copyright 2017 NXP

NXP DPAA Eventdev Driver
=========================

The dpaa eventdev is an implementation of the eventdev API, that provides a
wide range of the eventdev features. The eventdev relies on a dpaa based
platform to perform event scheduling.

More information can be found at `NXP Official Website
<http://www.nxp.com/products/microcontrollers-and-processors/arm-processors/qoriq-arm-processors:QORIQ-ARM>`_.

Features
--------

The DPAA EVENTDEV implements many features in the eventdev API;

- Hardware based event scheduler
- 4 event ports
- 4 event queues
- Parallel flows
- Atomic flows

Supported DPAA SoCs
--------------------

- LS1046A/LS1026A
- LS1043A/LS1023A

Prerequisites
-------------

See :doc:`../platform/dpaa` for setup information

Currently supported by DPDK:

- NXP SDK **2.0+** or LSDK **18.09+**
- Supported architectures:  **arm64 LE**.

- Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup the basic DPDK environment.

Pre-Installation Configuration
------------------------------

Config File Options
~~~~~~~~~~~~~~~~~~~

The following options can be modified in the ``config`` file.
Please note that enabling debugging options may affect system performance.

- ``CONFIG_RTE_LIBRTE_PMD_DPAA_EVENTDEV`` (default ``y``)

  Toggle compilation of the ``librte_pmd_dpaa_event`` driver.

Driver Compilation
~~~~~~~~~~~~~~~~~~

To compile the DPAA EVENTDEV PMD for Linux arm64 gcc target, run the
following ``make`` command:

.. code-block:: console

   cd <DPDK-source-directory>
   make config T=arm64-dpaa-linux-gcc install

Initialization
--------------

The dpaa eventdev is exposed as a vdev device which consists of a set of channels
and queues. On EAL initialization, dpaa components will be
probed and then vdev device can be created from the application code by

* Invoking ``rte_vdev_init("event_dpaa1")`` from the application

* Using ``--vdev="event_dpaa1"`` in the EAL options, which will call
  rte_vdev_init() internally

Example:

.. code-block:: console

    ./your_eventdev_application --vdev="event_dpaa1"

* Use dev arg option ``disable_intr=1`` to disable the interrupt mode

Limitations
-----------

1. DPAA eventdev can not work with DPAA PUSH mode queues configured for ethdev.
   Please configure export DPAA_NUM_PUSH_QUEUES=0

Platform Requirement
~~~~~~~~~~~~~~~~~~~~

DPAA drivers for DPDK can only work on NXP SoCs as listed in the
``Supported DPAA SoCs``.

Port-core Binding
~~~~~~~~~~~~~~~~~

DPAA EVENTDEV driver requires event port 'x' to be used on core 'x'.
