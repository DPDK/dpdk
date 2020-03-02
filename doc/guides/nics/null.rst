..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2020 Intel Corporation.

NULL Poll Mode Driver
=====================

NULL PMD is a simple virtual driver mainly for testing. It always returns success for all packets for Rx/Tx.

On Rx it returns requested number of empty packets (all zero). On Tx it just frees all sent packets.


Usage
-----

.. code-block:: console

   $RTE_TARGET/app/testpmd -l 0-3 -n 4 --vdev net_null0 --vdev net_null1 -- -i


Runtime Config Options
----------------------

- ``copy`` [optional, default disabled]

 It copies data of the packet before Rx/Tx. For Rx it uses another empty dummy mbuf for this.

.. code-block:: console

   $RTE_TARGET/app/testpmd -l 0-3 -n 4 --vdev "net_null0,copy=1" -- -i

- ``size`` [optional, default=64 bytes]

 Custom packet length value to use.r
 If ``copy`` is enabled, this is the length of copy operation.

.. code-block:: console

   $RTE_TARGET/app/testpmd -l 0-3 -n 4 --vdev "net_null0,size=256" -- -i

- ``no-rx`` [optional, default disabled]

 Makes PMD more like ``/dev/null``. On Rx no packets received, on Tx all packets are freed.
 This option can't co-exist with ``copy`` option.
