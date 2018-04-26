..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Intel Corporation

BBDEV null Poll Mode Driver
============================

The (**baseband_null**) is a bbdev poll mode driver which provides a minimal
implementation of a software bbdev device. As a null device it does not modify
the data in the mbuf on which the bbdev operation is to operate and it only
works for operation type ``RTE_BBDEV_OP_NONE``.

When a burst of mbufs is submitted to a *bbdev null PMD* for processing then
each mbuf in the burst will be enqueued in an internal buffer ring to be
collected on a dequeue call.


Limitations
-----------

* In-place operations for Turbo encode and decode are not supported

Installation
------------

The *bbdev null PMD* is enabled and built by default in both the Linux and
FreeBSD builds.

Initialization
--------------

To use the PMD in an application, user must:

- Call ``rte_vdev_init("baseband_null")`` within the application.

- Use ``--vdev="baseband_null"`` in the EAL options, which will call ``rte_vdev_init()`` internally.

The following parameters (all optional) can be provided in the previous two calls:

* ``socket_id``: Specify the socket where the memory for the device is going to be allocated
  (by default, *socket_id* will be the socket where the core that is creating the PMD is running on).

* ``max_nb_queues``: Specify the maximum number of queues in the device (default is ``RTE_MAX_LCORE``).

Example:
~~~~~~~~

.. code-block:: console

    ./test-bbdev.py -e="--vdev=baseband_null,socket_id=0,max_nb_queues=8"
