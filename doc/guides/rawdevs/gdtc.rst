.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2024 ZTE Corporation

GDTC Rawdev Driver
==================

The ``gdtc`` rawdev driver is an implementation of the rawdev API,
that provides communication between two separate hosts.
This is achieved via using the GDMA controller of Dinghai SoC,
which can be configured through exposed MPF device.

Device Setup
------------

Using the GDTC PMD driver does not require the MPF device
to bind additional user-space IO driver.

Before performing actual data transmission,
it is necessary to call ``rte_rawdev_queue_setup()``
to obtain an available queue ID.

For data transfer, utilize the standard ``rte_rawdev_enqueue_buffers()`` API.
The data transfer status can be queried via ``rte_rawdev_dequeue_buffers()``,
which will return the number of successfully transferred data items.

Initialization
--------------

The ``gdtc`` rawdev driver needs to work in IOVA PA mode.
Consider using ``--iova-mode=pa`` in the EAL options.

Platform Requirement
~~~~~~~~~~~~~~~~~~~~

This PMD is only supported on ZTE Neo Platforms:
- Neo X510/X512
