..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016 Intel Corporation.

.. _pdump_library:

The librte_pdump Library
========================

The ``librte_pdump`` library provides a framework for packet capturing in DPDK.
The library does the complete copy of the Rx and Tx mbufs to a new mempool and
hence it slows down the performance of the applications, so it is recommended
to use this library for debugging purposes.

The library uses a generic multi process channel to facilitate communication
between primary and secondary process for enabling/disabling packet capture on
ports.

The library provides the following APIs to initialize the packet capture framework, to enable
or disable the packet capture, and to uninitialize it.

* ``rte_pdump_init()``:
  This API initializes the packet capture framework.

* ``rte_pdump_enable()``:
  This API enables the packet capture on a given port and queue.
  Note: The filter option in the API is a place holder for future enhancements.

* ``rte_pdump_enable_by_deviceid()``:
  This API enables the packet capture on a given device id (``vdev name or pci address``) and queue.
  Note: The filter option in the API is a place holder for future enhancements.

* ``rte_pdump_disable()``:
  This API disables the packet capture on a given port and queue.

* ``rte_pdump_disable_by_deviceid()``:
  This API disables the packet capture on a given device id (``vdev name or pci address``) and queue.

* ``rte_pdump_uninit()``:
  This API uninitializes the packet capture framework.


Operation
---------

The primary process using ``librte_pdump`` is responsible for initializing the packet
capture framework. The packet capture framework, as part of its initialization, creates the
multi process channel to facilitate communication with secondary process, so the
secondary process ``app/pdump`` tool is responsible for enabling and disabling the packet capture on ports.

Implementation Details
----------------------

The library API ``rte_pdump_init()``, initializes the packet capture framework by creating the multi process
channel using ``rte_mp_action_register()`` API. The primary process will listen to secondary process requests
to enable or disable the packet capture over the multi process channel.

The library APIs ``rte_pdump_enable()`` and ``rte_pdump_enable_by_deviceid()`` enables the packet capture.
For the calls to these APIs from secondary process, the library creates the "pdump enable" request and sends
the request to the primary process over the multi process channel. The primary process takes this request
and enables the packet capture by registering the Ethernet RX and TX callbacks for the given port or device_id
and queue combinations. Then the primary process will mirror the packets to the new mempool and enqueue them to
the rte_ring that secondary process have passed to these APIs.

The library APIs ``rte_pdump_disable()`` and ``rte_pdump_disable_by_deviceid()`` disables the packet capture.
For the calls to these APIs from secondary process, the library creates the "pdump disable" request and sends
the request to the primary process over the multi process channel. The primary process takes this request and
disables the packet capture by removing the Ethernet RX and TX callbacks for the given port or device_id and
queue combinations.

The library API ``rte_pdump_uninit()``, uninitializes the packet capture framework by calling ``rte_mp_action_unregister()``
function.


Use Case: Packet Capturing
--------------------------

The DPDK ``app/pdump`` tool is developed based on this library to capture packets in DPDK.
Users can use this as an example to develop their own packet capturing tools.
