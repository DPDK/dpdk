..  BSD LICENSE
    Copyright (C) Cavium, Inc. 2017.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of Cavium, Inc nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

OCTEONTX SSOVF Eventdev Driver
==============================

The OCTEONTX SSOVF PMD (**librte_pmd_octeontx_ssovf**) provides poll mode
eventdev driver support for the inbuilt event device found in the **Cavium OCTEONTX**
SoC family as well as their virtual functions (VF) in SR-IOV context.

More information can be found at `Cavium, Inc Official Website
<http://www.cavium.com/OCTEON-TX_ARM_Processors.html>`_.

Features
--------

Features of the OCTEONTX SSOVF PMD are:

- 64 Event queues
- 32 Event ports
- HW event scheduler
- Supports 1M flows per event queue
- Flow based event pipelining
- Flow pinning support in flow based event pipelining
- Queue based event pipelining
- Supports ATOMIC, ORDERED, PARALLEL schedule types per flow
- Event scheduling QoS based on event queue priority
- Open system with configurable amount of outstanding events
- HW accelerated dequeue timeout support to enable power management
- SR-IOV VF

Supported OCTEONTX SoCs
-----------------------
- CN83xx

Prerequisites
-------------

See :doc: `../platform/octeontx` for setup information.

Pre-Installation Configuration
------------------------------

Config File Options
~~~~~~~~~~~~~~~~~~~

The following options can be modified in the ``config`` file.
Please note that enabling debugging options may affect system performance.

- ``CONFIG_RTE_LIBRTE_PMD_OCTEONTX_SSOVF`` (default ``y``)

  Toggle compilation of the ``librte_pmd_octeontx_ssovf`` driver.

- ``CONFIG_RTE_LIBRTE_PMD_OCTEONTX_SSOVF_DEBUG`` (default ``n``)

  Toggle display of generic debugging messages

Driver Compilation
~~~~~~~~~~~~~~~~~~

To compile the OCTEONTX SSOVF PMD for Linux arm64 gcc target, run the
following ``make`` command:

.. code-block:: console

   cd <DPDK-source-directory>
   make config T=arm64-thunderx-linuxapp-gcc install


Initialization
--------------

The octeontx eventdev is exposed as a vdev device which consists of a set
of SSO group and work-slot PCIe VF devices. On EAL initialization,
SSO PCIe VF devices will be probed and then the vdev device can be created
from the application code, or from the EAL command line based on
the number of probed/bound SSO PCIe VF device to DPDK by

* Invoking ``rte_vdev_init("event_octeontx")`` from the application

* Using ``--vdev="event_octeontx"`` in the EAL options, which will call
  rte_vdev_init() internally

Example:

.. code-block:: console

    ./your_eventdev_application --vdev="event_octeontx"

Limitations
-----------

Burst mode support
~~~~~~~~~~~~~~~~~~

Burst mode is not supported. Dequeue and Enqueue functions accepts only single
event at a time.

Rx adapter support
~~~~~~~~~~~~~~~~~~

When eth_octeontx is used as Rx adapter event schedule type
``RTE_SCHED_TYPE_PARALLEL`` is not supported.
