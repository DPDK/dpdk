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
- HW managed event timers support through TIM, with high precision and
  time granularity of 2.5us.
- Up to 256 TIM rings aka event timer adapters.
- Up to 8 rings traversed in parallel.
- HW managed packets enqueued from ethdev to eventdev exposed through event eth
  RX adapter.
- N:1 ethernet device Rx queue to Event queue mapping.
- Lockfree Tx from event eth Tx adapter using ``DEV_TX_OFFLOAD_MT_LOCKFREE``
  capability while maintaining receive packet order.
- Full Rx/Tx offload support defined through ethdev queue config.

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

    -w 0002:0e:00.0,xae_cnt=16384

- ``Force legacy mode``

  The ``single_ws`` devargs parameter is introduced to force legacy mode i.e
  single workslot mode in SSO and disable the default dual workslot mode.
  For example::

    -w 0002:0e:00.0,single_ws=1

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

    -w 0002:0e:00.0,qos=[1-50-50-50]

- ``Selftest``

  The functionality of OCTEON TX2 eventdev can be verified using this option,
  various unit and functional tests are run to verify the sanity.
  The tests are run once the vdev creation is successfully complete.
  For example::

    -w 0002:0e:00.0,selftest=1

- ``TIM disable NPA``

  By default chunks are allocated from NPA then TIM can automatically free
  them when traversing the list of chunks. The ``tim_disable_npa`` devargs
  parameter disables NPA and uses software mempool to manage chunks
  For example::

    -w 0002:0e:00.0,tim_disable_npa=1

- ``TIM modify chunk slots``

  The ``tim_chnk_slots`` devargs can be used to modify number of chunk slots.
  Chunks are used to store event timers, a chunk can be visualised as an array
  where the last element points to the next chunk and rest of them are used to
  store events. TIM traverses the list of chunks and enqueues the event timers
  to SSO. The default value is 255 and the max value is 4095.
  For example::

    -w 0002:0e:00.0,tim_chnk_slots=1023

- ``TIM enable arm/cancel statistics``

  The ``tim_stats_ena`` devargs can be used to enable arm and cancel stats of
  event timer adapter.
  For example::

    -w 0002:0e:00.0,tim_stats_ena=1

- ``TIM limit max rings reserved``

  The ``tim_rings_lmt`` devargs can be used to limit the max number of TIM
  rings i.e. event timer adapter reserved on probe. Since, TIM rings are HW
  resources we can avoid starving other applications by not grabbing all the
  rings.
  For example::

    -w 0002:0e:00.0,tim_rings_lmt=5

- ``TIM ring control internal parameters``

  When using multiple TIM rings the ``tim_ring_ctl`` devargs can be used to
  control each TIM rings internal parameters uniquely. The following dict
  format is expected [ring-chnk_slots-disable_npa-stats_ena]. 0 represents
  default values.
  For Example::

    -w 0002:0e:00.0,tim_ring_ctl=[2-1023-1-0]

Debugging Options
~~~~~~~~~~~~~~~~~

.. _table_octeontx2_event_debug_options:

.. table:: OCTEON TX2 event device debug options

   +---+------------+-------------------------------------------------------+
   | # | Component  | EAL log command                                       |
   +===+============+=======================================================+
   | 1 | SSO        | --log-level='pmd\.event\.octeontx2,8'                 |
   +---+------------+-------------------------------------------------------+
   | 2 | TIM        | --log-level='pmd\.event\.octeontx2\.timer,8'          |
   +---+------------+-------------------------------------------------------+
