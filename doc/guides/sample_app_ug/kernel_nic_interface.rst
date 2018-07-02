..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

Kernel NIC Interface Sample Application
=======================================

The Kernel NIC Interface (KNI) is a DPDK control plane solution that
allows userspace applications to exchange packets with the kernel networking stack.
To accomplish this, DPDK userspace applications use an IOCTL call
to request the creation of a KNI virtual device in the Linux* kernel.
The IOCTL call provides interface information and the DPDK's physical address space,
which is re-mapped into the kernel address space by the KNI kernel loadable module
that saves the information to a virtual device context.
The DPDK creates FIFO queues for packet ingress and egress
to the kernel module for each device allocated.

The KNI kernel loadable module is a standard net driver,
which upon receiving the IOCTL call access the DPDK's FIFO queue to
receive/transmit packets from/to the DPDK userspace application.
The FIFO queues contain pointers to data packets in the DPDK. This:

*   Provides a faster mechanism to interface with the kernel net stack and eliminates system calls

*   Facilitates the DPDK using standard Linux* userspace net tools (tcpdump, ftp, and so on)

*   Eliminate the copy_to_user and copy_from_user operations on packets.

The Kernel NIC Interface sample application is a simple example that demonstrates the use
of the DPDK to create a path for packets to go through the Linux* kernel.
This is done by creating one or more kernel net devices for each of the DPDK ports.
The application allows the use of standard Linux tools (ethtool, ifconfig, tcpdump) with the DPDK ports and
also the exchange of packets between the DPDK application and the Linux* kernel.

Overview
--------

The Kernel NIC Interface sample application uses two threads in user space for each physical NIC port being used,
and allocates one or more KNI device for each physical NIC port with kernel module's support.
For a physical NIC port, one thread reads from the port and writes to KNI devices,
and another thread reads from KNI devices and writes the data unmodified to the physical NIC port.
It is recommended to configure one KNI device for each physical NIC port.
If configured with more than one KNI devices for a physical NIC port,
it is just for performance testing, or it can work together with VMDq support in future.

The packet flow through the Kernel NIC Interface application is as shown in the following figure.

.. _figure_kernel_nic:

.. figure:: img/kernel_nic.*

   Kernel NIC Application Packet Flow

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``kni`` sub-directory.

.. note::

        This application is intended as a linuxapp only.

Loading the Kernel Module
-------------------------

Loading the KNI kernel module without any parameter is the typical way a DPDK application
gets packets into and out of the kernel net stack.
This way, only one kernel thread is created for all KNI devices for packet receiving in kernel side:

.. code-block:: console

    #insmod rte_kni.ko

Pinning the kernel thread to a specific core can be done using a taskset command such as following:

.. code-block:: console

    #taskset -p 100000 `pgrep --fl kni_thread | awk '{print $1}'`

This command line tries to pin the specific kni_thread on the 20th lcore (lcore numbering starts at 0),
which means it needs to check if that lcore is available on the board.
This command must be sent after the application has been launched, as insmod does not start the kni thread.

For optimum performance,
the lcore in the mask must be selected to be on the same socket as the lcores used in the KNI application.

To provide flexibility of performance, the kernel module of the KNI,
located in the kmod sub-directory of the DPDK target directory,
can be loaded with parameter of kthread_mode as follows:

*   #insmod rte_kni.ko kthread_mode=single

    This mode will create only one kernel thread for all KNI devices for packet receiving in kernel side.
    By default, it is in this single kernel thread mode.
    It can set core affinity for this kernel thread by using Linux command taskset.

*   #insmod rte_kni.ko kthread_mode =multiple

    This mode will create a kernel thread for each KNI device for packet receiving in kernel side.
    The core affinity of each kernel thread is set when creating the KNI device.
    The lcore ID for each kernel thread is provided in the command line of launching the application.
    Multiple kernel thread mode can provide scalable higher performance.

To measure the throughput in a loopback mode, the kernel module of the KNI,
located in the kmod sub-directory of the DPDK target directory,
can be loaded with parameters as follows:

*   #insmod rte_kni.ko lo_mode=lo_mode_fifo

    This loopback mode will involve ring enqueue/dequeue operations in kernel space.

*   #insmod rte_kni.ko lo_mode=lo_mode_fifo_skb

    This loopback mode will involve ring enqueue/dequeue operations and sk buffer copies in kernel space.

Running the Application
-----------------------

The application requires a number of command line options:

.. code-block:: console

    kni [EAL options] -- -P -p PORTMASK --config="(port,lcore_rx,lcore_tx[,lcore_kthread,...])[,port,lcore_rx,lcore_tx[,lcore_kthread,...]]"

Where:

*   -P: Set all ports to promiscuous mode so that packets are accepted regardless of the packet's Ethernet MAC destination address.
    Without this option, only packets with the Ethernet MAC destination address set to the Ethernet address of the port are accepted.

*   -p PORTMASK: Hexadecimal bitmask of ports to configure.

*   --config="(port,lcore_rx, lcore_tx[,lcore_kthread, ...]) [, port,lcore_rx, lcore_tx[,lcore_kthread, ...]]":
    Determines which lcores of RX, TX, kernel thread are mapped to which ports.

Refer to *DPDK Getting Started Guide* for general information on running applications and the Environment Abstraction Layer (EAL) options.

The -c coremask or -l corelist parameter of the EAL options should include the lcores indicated by the lcore_rx and lcore_tx,
but does not need to include lcores indicated by lcore_kthread as they are used to pin the kernel thread on.
The -p PORTMASK parameter should include the ports indicated by the port in --config, neither more nor less.

The lcore_kthread in --config can be configured none, one or more lcore IDs.
In multiple kernel thread mode, if configured none, a KNI device will be allocated for each port,
while no specific lcore affinity will be set for its kernel thread.
If configured one or more lcore IDs, one or more KNI devices will be allocated for each port,
while specific lcore affinity will be set for its kernel thread.
In single kernel thread mode, if configured none, a KNI device will be allocated for each port.
If configured one or more lcore IDs,
one or more KNI devices will be allocated for each port while
no lcore affinity will be set as there is only one kernel thread for all KNI devices.

For example, to run the application with two ports served by six lcores, one lcore of RX, one lcore of TX,
and one lcore of kernel thread for each port:

.. code-block:: console

    ./build/kni -l 4-7 -n 4 -- -P -p 0x3 --config="(0,4,6,8),(1,5,7,9)"

KNI Operations
--------------

Once the KNI application is started, one can use different Linux* commands to manage the net interfaces.
If more than one KNI devices configured for a physical port,
only the first KNI device will be paired to the physical device.
Operations on other KNI devices will not affect the physical port handled in user space application.

Assigning an IP address:

.. code-block:: console

    #ifconfig vEth0_0 192.168.0.1

Displaying the NIC registers:

.. code-block:: console

    #ethtool -d vEth0_0

Dumping the network traffic:

.. code-block:: console

    #tcpdump -i vEth0_0

Change the MAC address:

.. code-block:: console

    #ifconfig vEth0_0 hw ether 0C:01:02:03:04:08

When the DPDK userspace application is closed, all the KNI devices are deleted from Linux*.

Explanation
-----------

The following sections provide some explanation of code.

Initialization
~~~~~~~~~~~~~~

Setup of mbuf pool, driver and queues is similar to the setup done in the :doc:`l2_forward_real_virtual`..
In addition, one or more kernel NIC interfaces are allocated for each
of the configured ports according to the command line parameters.

The code for allocating the kernel NIC interfaces for a specific port is
in the function ``kni_alloc``.

The other step in the initialization process that is unique to this sample application
is the association of each port with lcores for RX, TX and kernel threads.

*   One lcore to read from the port and write to the associated one or more KNI devices

*   Another lcore to read from one or more KNI devices and write to the port

*   Other lcores for pinning the kernel threads on one by one

This is done by using the ``kni_port_params_array[]`` array, which is indexed by the port ID.
The code is in the function ``parse_config``.

Packet Forwarding
~~~~~~~~~~~~~~~~~

After the initialization steps are completed, the main_loop() function is run on each lcore.
This function first checks the lcore_id against the user provided lcore_rx and lcore_tx
to see if this lcore is reading from or writing to kernel NIC interfaces.

For the case that reads from a NIC port and writes to the kernel NIC interfaces (``kni_ingress``),
the packet reception is the same as in L2 Forwarding sample application
(see :ref:`l2_fwd_app_rx_tx_packets`).
The packet transmission is done by sending mbufs into the kernel NIC interfaces by rte_kni_tx_burst().
The KNI library automatically frees the mbufs after the kernel successfully copied the mbufs.

For the other case that reads from kernel NIC interfaces
and writes to a physical NIC port (``kni_egress``),
packets are retrieved by reading mbufs from kernel NIC interfaces by ``rte_kni_rx_burst()``.
The packet transmission is the same as in the L2 Forwarding sample application
(see :ref:`l2_fwd_app_rx_tx_packets`).

Callbacks for Kernel Requests
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To execute specific PMD operations in user space requested by some Linux* commands,
callbacks must be implemented and filled in the struct rte_kni_ops structure.
Currently, setting a new MTU, change in MAC address, configuring promiscusous mode and
configuring the network interface(up/down) re supported.
Default implementation for following is available in rte_kni library.
Application may choose to not implement following callbacks:

- ``config_mac_address``
- ``config_promiscusity``
