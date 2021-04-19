..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2021 Intel Corporation.

Toeplitz Hash Library
=====================

DPDK provides a Toeplitz Hash Library
to calculate the Toeplitz hash function and to use its properties.
The Toeplitz hash function is commonly used in a wide range of NICs
to calculate the RSS hash sum to spread the traffic among the queues.

.. _figure_rss_queue_assign:

.. figure:: img/rss_queue_assign.*

   RSS queue assignment example


Toeplitz hash function API
--------------------------

There are two functions that provide calculation of the Toeplitz hash sum:

* ``rte_softrss()``
* ``rte_softrss_be()``

Both of these functions take the parameters:

* A pointer to the tuple, containing fields extracted from the packet.
* A length of this tuple counted in double words.
* A pointer to the RSS hash key corresponding to the one installed on the NIC.

Both functions expect the tuple to be in "host" byte order
and a multiple of 4 bytes in length.
The ``rte_softrss()`` function expects the ``rss_key``
to be exactly the same as the one installed on the NIC.
The ``rte_softrss_be`` function is a faster implementation,
but it expects ``rss_key`` to be converted to the host byte order.
