..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016 Intel Corporation.

.. _pdump_library:

The librte_pdump Library
========================

The ``librte_pdump`` library provides a framework for packet capturing in DPDK.
The library does the complete copy of the Rx and Tx mbufs to a new mempool and
hence it slows down the performance of the applications, so it is recommended
to use this library for debugging purposes.

The library provides the following APIs to initialize the packet capture framework, to enable
or disable the packet capture, and to uninitialize it:

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

The ``librte_pdump`` library works on a client/server model. The server is responsible for enabling or
disabling the packet capture and the clients are responsible for requesting the enabling or disabling of
the packet capture.

The packet capture framework, as part of its initialization, creates the pthread and the server socket in
the pthread. The application that calls the framework initialization will have the server socket created,
either under the path that the application has passed or under the default path i.e. either ``/var/run/.dpdk`` for
root user or ``~/.dpdk`` for non root user.

Applications that request enabling or disabling of the packet capture will have the client socket created either under
the path that the application has passed or under the default path i.e. either ``/var/run/.dpdk`` for root user or
``~/.dpdk`` for not root user to send the requests to the server. The server socket will listen for client requests for
enabling or disabling the packet capture.


Implementation Details
----------------------

The library API ``rte_pdump_init()``, initializes the packet capture framework by creating the pdump server by calling
``rte_mp_action_register()`` function. The server will listen to the client requests to enable or disable the
packet capture.

The library APIs ``rte_pdump_enable()`` and ``rte_pdump_enable_by_deviceid()`` enables the packet capture.
On each call to these APIs, the library creates a separate client socket, creates the "pdump enable" request and sends
the request to the server. The server that is listening on the socket will take the request and enable the packet capture
by registering the Ethernet RX and TX callbacks for the given port or device_id and queue combinations.
Then the server will mirror the packets to the new mempool and enqueue them to the rte_ring that clients have passed
to these APIs. The server also sends the response back to the client about the status of the request that was processed.
After the response is received from the server, the client socket is closed.

The library APIs ``rte_pdump_disable()`` and ``rte_pdump_disable_by_deviceid()`` disables the packet capture.
On each call to these APIs, the library creates a separate client socket, creates the "pdump disable" request and sends
the request to the server. The server that is listening on the socket will take the request and disable the packet
capture by removing the Ethernet RX and TX callbacks for the given port or device_id and queue combinations. The server
also sends the response back to the client about the status of the request that was processed. After the response is
received from the server, the client socket is closed.

The library API ``rte_pdump_uninit()``, uninitializes the packet capture framework by calling ``rte_mp_action_unregister()``
function.


Use Case: Packet Capturing
--------------------------

The DPDK ``app/pdump`` tool is developed based on this library to capture packets in DPDK.
Users can use this as an example to develop their own packet capturing tools.
