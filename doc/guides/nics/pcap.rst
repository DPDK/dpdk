.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2010-2015 Intel Corporation.

Pcap Poll Mode Driver
=====================

The pcap-based PMD (**librte_net_pcap**) reads and writes packets using the pcap library,
both from files on disk and from physical NIC devices using standard kernel drivers.

For more information about the pcap library, see the
`libpcap documentation <https://www.tcpdump.org/manpages/pcap.3pcap.html>`_.

.. note::

   The pcap-based PMD requires the libpcap development files to be installed.
   This applies to all supported operating systems: Linux, FreeBSD, and Windows.


Using the Driver from the EAL Command Line
------------------------------------------

DPDK allows pseudo-Ethernet devices, as the pcap driver,
to be created at application startup time during EAL initialization.

To do so, pass the ``--vdev=net_pcap0`` parameter to the EAL.
This parameter accepts options to allocate and use pcap-based Ethernet
transparently by the application.
This can be used, for example, for testing on a virtual machine
where there are no Ethernet ports.

The device name must start with the ``net_pcap`` prefix followed by numbers or letters.
The name must be unique for each device.
Each device can have multiple stream options and multiple devices can be used.
Multiple device definitions can be specified using multiple ``--vdev`` arguments.
Device name and stream options must be separated by commas as shown below:

.. code-block:: console

   dpdk-testpmd -l 0-3 \
      --vdev 'net_pcap0,stream_opt0=..,stream_opt1=..' \
      --vdev='net_pcap1,stream_opt0=..'

Device Streams
~~~~~~~~~~~~~~

Stream definitions can be combined as long as one of the following two rules is met:

* A device is provided with two different streams - reception and transmission.
* A device is provided with one network interface name used for reading and writing packets.

The stream types are:

``rx_pcap``

   Defines a reception stream based on a pcap file.
   The driver reads each packet within the given pcap file
   as if it was receiving it from the wire.
   The value is a path to a valid pcap file::

      rx_pcap=/path/to/file.pcap

``tx_pcap``

   Defines a transmission stream based on a pcap file.
   The driver writes each received packet to the given pcap file.
   The file is overwritten if it already exists and it is created if it does not.
   The value is a path to a pcap file::

      tx_pcap=/path/to/file.pcap

``rx_iface``

   Defines a reception stream based on a network interface name.
   The driver reads packets from the given interface
   using the kernel driver for that interface.
   The driver captures both the incoming and outgoing packets on that interface.
   The value is an interface name::

      rx_iface=eth0

``rx_iface_in``

   Defines a reception stream based on a network interface name.
   The driver reads packets from the given interface
   using the kernel driver for that interface.
   The driver captures only the incoming packets on that interface.
   The value is an interface name::

      rx_iface_in=eth0

``tx_iface``

   Defines a transmission stream based on a network interface name.
   The driver sends packets to the given interface
   using the kernel driver for that interface.
   The value is an interface name::

      tx_iface=eth0

``iface``

   Defines a device mapping a network interface.
   The driver both reads and writes packets from and to the given interface.
   The value is an interface name::

      iface=eth0

Multi-queue Support
~~~~~~~~~~~~~~~~~~~

The pcap PMD supports multiple receive and transmit queues.
The number of receive queues is determined
by the number of ``rx_pcap`` or ``rx_iface`` arguments provided.
Similarly, the number of transmit queues is determined
by the number of ``tx_pcap`` or ``tx_iface`` arguments.

Using the same file for multiple queues is not supported
because the underlying pcap library
does not support concurrent access to a single file handle.

Runtime Config Options
~~~~~~~~~~~~~~~~~~~~~~

* Use pcap interface physical MAC

  When the ``iface=`` configuration is set,
  the selected interface's physical MAC address can be used.
  This can be done with the ``phy_mac`` devarg, for example::

     --vdev 'net_pcap0,iface=eth0,phy_mac=1'

* Use the Rx pcap file to infinitely receive packets

  When the ``rx_pcap=`` configuration is set,
  the selected pcap file can be used for basic performance testing.
  This can be done with the ``infinite_rx`` devarg, for example::

     --vdev 'net_pcap0,rx_pcap=file_rx.pcap,infinite_rx=1'

  When this mode is used, it is recommended to drop all packets on transmit
  by not providing a ``tx_pcap`` or ``tx_iface``.

  This option is device-wide,
  so all queues on a device will either have this enabled or disabled.
  This option should only be provided once per device.

* Drop all packets on transmit

  To drop all packets on transmit for a device,
  do not provide a ``tx_pcap`` or ``tx_iface``, for example::

     --vdev 'net_pcap0,rx_pcap=file_rx.pcap'

  In this case, one Tx drop queue is created for each Rx queue on that device.

* Receive no packets on Rx

  To run without receiving any packets on Rx,
  do not provide a ``rx_pcap`` or ``rx_iface``, for example::

     --vdev 'net_pcap0,tx_pcap=file_tx.pcap'

  In this case, one dummy Rx queue is created for each Tx queue argument passed.

Examples of Usage
~~~~~~~~~~~~~~~~~

Read packets from one pcap file and write them to another:

.. code-block:: console

   dpdk-testpmd -l 0-3 \
      --vdev 'net_pcap0,rx_pcap=file_rx.pcap,tx_pcap=file_tx.pcap' \
      -- --port-topology=chained

Read packets from a network interface and write them to a pcap file:

.. code-block:: console

   dpdk-testpmd -l 0-3 \
      --vdev 'net_pcap0,rx_iface=eth0,tx_pcap=file_tx.pcap' \
      -- --port-topology=chained

Read packets from a pcap file and write them to a network interface:

.. code-block:: console

   dpdk-testpmd -l 0-3 \
      --vdev 'net_pcap0,rx_pcap=file_rx.pcap,tx_iface=eth1' \
      -- --port-topology=chained

Forward packets through 2 network interfaces:

.. code-block:: console

   dpdk-testpmd -l 0-3 \
      --vdev 'net_pcap0,iface=eth0' --vdev='net_pcap1,iface=eth1'

Enable 2 Tx queues on a network interface:

.. code-block:: console

   dpdk-testpmd -l 0-3 \
      --vdev 'net_pcap0,rx_iface=eth1,tx_iface=eth1,tx_iface=eth1' \
      -- --txq 2

Read only incoming packets from a network interface
and write them back to the same network interface:

.. code-block:: console

   dpdk-testpmd -l 0-3 \
      --vdev 'net_pcap0,rx_iface_in=eth1,tx_iface=eth1'

Using Pcap-based PMD with the testpmd Application
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

One of the first things that testpmd does before starting to forward packets
is to flush the Rx streams by reading the first 512 packets on every Rx stream
and discarding them.
When using a pcap-based PMD, this behavior can be turned off
using the ``--no-flush-rx`` option:

.. code-block:: console

   --no-flush-rx

This option is also available in the runtime command line:

.. code-block:: console

   set flush_rx on/off

It is useful for the case where the ``rx_pcap`` is being used
and no packets are meant to be discarded.
Otherwise, the first 512 packets from the input pcap file
will be discarded by the Rx flushing operation.

.. code-block:: console

   dpdk-testpmd -l 0-3 \
      --vdev 'net_pcap0,rx_pcap=file_rx.pcap,tx_pcap=file_tx.pcap' \
      -- --port-topology=chained --no-flush-rx

.. note::

   The network interface provided to the PMD should be up.
   The PMD will return an error if the interface is down,
   and the PMD itself won't change the status of the external network interface.
