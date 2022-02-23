..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2019 Mellanox Technologies, Ltd

.. include:: <isonum.txt>

MLX5 vDPA Driver
================

The mlx5 vDPA (vhost data path acceleration) driver library
(**librte_vdpa_mlx5**) provides support for **Mellanox ConnectX-6**,
**Mellanox ConnectX-6 Dx** and **Mellanox BlueField** families of
10/25/40/50/100/200 Gb/s adapters as well as their virtual functions (VF) in
SR-IOV context.

.. note::

   This driver is enabled automatically when using "meson" build system which
   will detect dependencies.

See :doc:`../../platform/mlx5` guide for design details,
and which PMDs can be combined with vDPA PMD.

Supported NICs
--------------

* Mellanox\ |reg| ConnectX\ |reg|-6 200G MCX654106A-HCAT (2x200G)
* Mellanox\ |reg| ConnectX\ |reg|-6 Dx EN 25G MCX621102AN-ADAT (2x25G)
* Mellanox\ |reg| ConnectX\ |reg|-6 Dx EN 100G MCX623106AN-CDAT (2x100G)
* Mellanox\ |reg| ConnectX\ |reg|-6 Dx EN 200G MCX623105AN-VDAT (1x200G)
* Mellanox\ |reg| BlueField SmartNIC 25G MBF1M332A-ASCAT (2x25G)

Prerequisites
-------------

- Mellanox OFED version: **5.0**
  See :ref:`mlx5 common prerequisites <mlx5_linux_prerequisites>` for more details.

Run-time configuration
~~~~~~~~~~~~~~~~~~~~~~

Driver options
^^^^^^^^^^^^^^

Please refer to :ref:`mlx5 common options <mlx5_common_driver_options>`
for an additional list of options shared with other mlx5 drivers.

- ``event_mode`` parameter [int]

  - 0, Completion queue scheduling will be managed by a timer thread which
    automatically adjusts its delays to the coming traffic rate.

  - 1, Completion queue scheduling will be managed by a timer thread with fixed
    delay time.

  - 2, Completion queue scheduling will be managed by interrupts. Each CQ burst
    arms the CQ in order to get an interrupt event in the next traffic burst.

  - Default mode is 1.

- ``event_us`` parameter [int]

  Per mode micro-seconds parameter - relevant only for event mode 0 and 1:

  - 0, A nonzero value to set timer step in micro-seconds. The timer thread
    dynamic delay change steps according to this value. Default value is 1us.

  - 1, A value to set fixed timer delay in micro-seconds. Default value is 0us.

- ``no_traffic_time`` parameter [int]

  A nonzero value defines the traffic off time, in polling cycle time units,
  that moves the driver to no-traffic mode. In this mode the polling is stopped
  and interrupts are configured to the device in order to notify traffic for the
  driver. Default value is 16.

- ``event_core`` parameter [int]

  CPU core number to set polling thread affinity to, default to control plane
  cpu.

- ``hw_latency_mode`` parameter [int]

  The completion queue moderation mode:

  - 0, HW default.

  - 1, Latency is counted from the first packet completion report.

  - 2, Latency is counted from the last packet completion.

- ``hw_max_latency_us`` parameter [int]

  - 1 - 4095, The maximum time in microseconds that packet completion report
    can be delayed.

  - 0, HW default.

- ``hw_max_pending_comp`` parameter [int]

  - 1 - 65535, The maximum number of pending packets completions in an HW queue.

  - 0, HW default.


Error handling
^^^^^^^^^^^^^^

Upon potential hardware errors, mlx5 PMD try to recover, give up if failed 3
times in 3 seconds, virtq will be put in disable state. User should check log
to get error information, or query vdpa statistics counter to know error type
and count report.
