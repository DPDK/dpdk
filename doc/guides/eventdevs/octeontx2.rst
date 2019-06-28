..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Marvell International Ltd.

OCTEON TX2 SSO Eventdev Driver
===============================

The OCTEON TX2 SSO PMD (**librte_pmd_octeontx2_event**) provides poll mode
eventdev driver support for the inbuilt event device found in the **Marvell OCTEON TX2**
SoC family.

More information about OCTEON TX2 SoC can be found at `Marvell Official Website
<https://www.marvell.com/embedded-processors/infrastructure-processors/>`_.

Features
--------

Features of the OCTEON TX2 SSO PMD are:

- 256 Event queues
- 26 (dual) and 52 (single) Event ports
- HW event scheduler
- Supports 1M flows per event queue
- Flow based event pipelining
- Flow pinning support in flow based event pipelining
- Queue based event pipelining
- Supports ATOMIC, ORDERED, PARALLEL schedule types per flow
- Event scheduling QoS based on event queue priority
- Open system with configurable amount of outstanding events limited only by
  DRAM
- HW accelerated dequeue timeout support to enable power management

Prerequisites and Compilation procedure
---------------------------------------

   See :doc:`../platform/octeontx2` for setup information.

Pre-Installation Configuration
------------------------------

Compile time Config Options
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following option can be modified in the ``config`` file.

- ``CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_EVENTDEV`` (default ``y``)

  Toggle compilation of the ``librte_pmd_octeontx2_event`` driver.

Runtime Config Options
~~~~~~~~~~~~~~~~~~~~~~

- ``Maximum number of in-flight events`` (default ``8192``)

  In **Marvell OCTEON TX2** the max number of in-flight events are only limited
  by DRAM size, the ``xae_cnt`` devargs parameter is introduced to provide
  upper limit for in-flight events.
  For example::

    --dev "0002:0e:00.0,xae_cnt=16384"

- ``Force legacy mode``

  The ``single_ws`` devargs parameter is introduced to force legacy mode i.e
  single workslot mode in SSO and disable the default dual workslot mode.
  For example::

    --dev "0002:0e:00.0,single_ws=1"

Debugging Options
~~~~~~~~~~~~~~~~~

.. _table_octeontx2_event_debug_options:

.. table:: OCTEON TX2 event device debug options

   +---+------------+-------------------------------------------------------+
   | # | Component  | EAL log command                                       |
   +===+============+=======================================================+
   | 1 | SSO        | --log-level='pmd\.event\.octeontx2,8'                 |
   +---+------------+-------------------------------------------------------+
