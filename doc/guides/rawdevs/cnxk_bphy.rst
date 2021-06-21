..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2021 Marvell.

Marvell CNXK BPHY Driver
========================

CN10K/CN9K Fusion product families offer an internal BPHY unit which provides
set of hardware accelerators for performing baseband related operations.
Connectivity to the outside world happens through a block called RFOE which is
backed by ethernet I/O block called CGX or RPM (depending on the chip version).
RFOE stands for Radio Frequency Over Ethernet and provides support for
IEEE 1904.3 (RoE) standard.

Device Setup
------------

The BPHY CGX/RPM devices will need to be bound to a user-space IO driver for
use. The script ``dpdk-devbind.py`` script included with DPDK can be used to
view the state of the devices and to bind them to a suitable DPDK-supported
kernel driver. When querying the status of the devices, they will appear under
the category of "Misc (rawdev) devices", i.e. the command
``dpdk-devbind.py --status-dev misc`` can be used to see the state of those
devices alone.

Before performing actual data transfer one needs to first retrieve number of
available queues with ``rte_rawdev_queue_count()`` and capacity of each
using ``rte_rawdev_queue_conf_get()``.
