..  BSD LICENSE
    Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

.. _kni:

Kernel NIC Interface
====================

The DPDK Kernel NIC Interface (KNI) allows userspace applications access to the Linux* control plane.

The benefits of using the DPDK KNI are:

*   Faster than existing Linux TUN/TAP interfaces
    (by eliminating system calls and copy_to_user()/copy_from_user() operations.

*   Allows management of DPDK ports using standard Linux net tools such as ethtool, ifconfig and tcpdump.

*   Allows an interface with the kernel network stack.

The components of an application using the DPDK Kernel NIC Interface are shown in :numref:`figure_kernel_nic_intf`.

.. _figure_kernel_nic_intf:

.. figure:: img/kernel_nic_intf.*

   Components of a DPDK KNI Application


The DPDK KNI Kernel Module
--------------------------

The KNI kernel loadable module provides support for two types of devices:

*   A Miscellaneous device (/dev/kni) that:

    *   Creates net devices (via ioctl  calls).

    *   Maintains a kernel thread context shared by all KNI instances
        (simulating the RX side of the net driver).

    *   For single kernel thread mode, maintains a kernel thread context shared by all KNI instances
        (simulating the RX side of the net driver).

    *   For multiple kernel thread mode, maintains a kernel thread context for each KNI instance
        (simulating the RX side of the net driver).

*   Net device:

    *   Net functionality provided by implementing several operations such as netdev_ops,
        header_ops, ethtool_ops that are defined by struct net_device,
        including support for DPDK mbufs and FIFOs.

    *   The interface name is provided from userspace.

    *   The MAC address can be the real NIC MAC address or random.

KNI Creation and Deletion
-------------------------

The KNI interfaces are created by a DPDK application dynamically.
The interface name and FIFO details are provided by the application through an ioctl call
using the rte_kni_device_info struct which contains:

*   The interface name.

*   Physical addresses of the corresponding memzones for the relevant FIFOs.

*   Mbuf mempool details, both physical and virtual (to calculate the offset for mbuf pointers).

*   PCI information.

*   Core affinity.

Refer to rte_kni_common.h in the DPDK source code for more details.

The physical addresses will be re-mapped into the kernel address space and stored in separate KNI contexts.

The affinity of kernel RX thread (both single and multi-threaded modes) is controlled by force_bind and
core_id config parameters.

The KNI interfaces can be deleted by a DPDK application dynamically after being created.
Furthermore, all those KNI interfaces not deleted will be deleted on the release operation
of the miscellaneous device (when the DPDK application is closed).

DPDK mbuf Flow
--------------

To minimize the amount of DPDK code running in kernel space, the mbuf mempool is managed in userspace only.
The kernel module will be aware of mbufs,
but all mbuf allocation and free operations will be handled by the DPDK application only.

:numref:`figure_pkt_flow_kni` shows a typical scenario with packets sent in both directions.

.. _figure_pkt_flow_kni:

.. figure:: img/pkt_flow_kni.*

   Packet Flow via mbufs in the DPDK KNI


Use Case: Ingress
-----------------

On the DPDK RX side, the mbuf is allocated by the PMD in the RX thread context.
This thread will enqueue the mbuf in the rx_q FIFO.
The KNI thread will poll all KNI active devices for the rx_q.
If an mbuf is dequeued, it will be converted to a sk_buff and sent to the net stack via netif_rx().
The dequeued mbuf must be freed, so the same pointer is sent back in the free_q FIFO.

The RX thread, in the same main loop, polls this FIFO and frees the mbuf after dequeuing it.

Use Case: Egress
----------------

For packet egress the DPDK application must first enqueue several mbufs to create an mbuf cache on the kernel side.

The packet is received from the Linux net stack, by calling the kni_net_tx() callback.
The mbuf is dequeued (without waiting due the cache) and filled with data from sk_buff.
The sk_buff is then freed and the mbuf sent in the tx_q FIFO.

The DPDK TX thread dequeues the mbuf and sends it to the PMD (via rte_eth_tx_burst()).
It then puts the mbuf back in the cache.

Ethtool
-------

Ethtool is a Linux-specific tool with corresponding support in the kernel
where each net device must register its own callbacks for the supported operations.
The current implementation uses the igb/ixgbe modified Linux drivers for ethtool support.
Ethtool is not supported in i40e and VMs (VF or EM devices).

Link state and MTU change
-------------------------

Link state and MTU change are network interface specific operations usually done via ifconfig.
The request is initiated from the kernel side (in the context of the ifconfig process)
and handled by the user space DPDK application.
The application polls the request, calls the application handler and returns the response back into the kernel space.

The application handlers can be registered upon interface creation or explicitly registered/unregistered in runtime.
This provides flexibility in multiprocess scenarios
(where the KNI is created in the primary process but the callbacks are handled in the secondary one).
The constraint is that a single process can register and handle the requests.
