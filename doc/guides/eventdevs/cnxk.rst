.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2021 Marvell.

Marvell cnxk SSO Eventdev Driver
================================

The SSO PMD (**librte_event_cnxk**) and provides poll mode
eventdev driver support for the inbuilt event device found in the
**Marvell OCTEON cnxk** SoC family.

More information about OCTEON cnxk SoC can be found at `Marvell Official Website
<https://www.marvell.com/embedded-processors/infrastructure-processors/>`_.

Supported OCTEON cnxk SoCs
--------------------------

- CN9XX
- CN10XX

Features
--------

Features of the OCTEON cnxk SSO PMD are:

- 256 Event queues
- 26 (dual) and 52 (single) Event ports on CN9XX
- 52 Event ports on CN10XX
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
- HW managed event timers support through TIM, with high precision and
  time granularity of 2.5us on CN9K and 1us on CN10K.
- Up to 256 TIM rings a.k.a event timer adapters.
- Up to 8 rings traversed in parallel.

Prerequisites and Compilation procedure
---------------------------------------

   See :doc:`../platform/cnxk` for setup information.


Runtime Config Options
----------------------

- ``Maximum number of in-flight events`` (default ``8192``)

  In **Marvell OCTEON cnxk** the max number of in-flight events are only limited
  by DRAM size, the ``xae_cnt`` devargs parameter is introduced to provide
  upper limit for in-flight events.

  For example::

    -a 0002:0e:00.0,xae_cnt=16384

- ``CN9K Getwork mode``

  CN9K ``single_ws`` devargs parameter is introduced to select single workslot
  mode in SSO and disable the default dual workslot mode.

  For example::

    -a 0002:0e:00.0,single_ws=1

- ``CN10K Getwork mode``

  CN10K supports multiple getwork prefetch modes, by default the prefetch
  mode is set to none.

  For example::

    -a 0002:0e:00.0,gw_mode=1

- ``Event Group QoS support``

  SSO GGRPs i.e. queue uses DRAM & SRAM buffers to hold in-flight
  events. By default the buffers are assigned to the SSO GGRPs to
  satisfy minimum HW requirements. SSO is free to assign the remaining
  buffers to GGRPs based on a preconfigured threshold.
  We can control the QoS of SSO GGRP by modifying the above mentioned
  thresholds. GGRPs that have higher importance can be assigned higher
  thresholds than the rest. The dictionary format is as follows
  [Qx-XAQ-TAQ-IAQ][Qz-XAQ-TAQ-IAQ] expressed in percentages, 0 represents
  default.

  For example::

    -a 0002:0e:00.0,qos=[1-50-50-50]

- ``TIM disable NPA``

  By default chunks are allocated from NPA then TIM can automatically free
  them when traversing the list of chunks. The ``tim_disable_npa`` devargs
  parameter disables NPA and uses software mempool to manage chunks

  For example::

    -a 0002:0e:00.0,tim_disable_npa=1

Debugging Options
-----------------

.. _table_octeon_cnxk_event_debug_options:

.. table:: OCTEON cnxk event device debug options

   +---+------------+-------------------------------------------------------+
   | # | Component  | EAL log command                                       |
   +===+============+=======================================================+
   | 1 | SSO        | --log-level='pmd\.event\.cnxk,8'                      |
   +---+------------+-------------------------------------------------------+
   | 2 | TIM        | --log-level='pmd\.event\.cnxk\.timer,8'               |
   +---+------------+-------------------------------------------------------+
