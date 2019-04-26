..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016 Intel Corporation.

.. _virtio_user_as_exceptional_path:

Virtio_user as Exceptional Path
===============================

The virtual device, virtio-user, was originally introduced with vhost-user
backend, as a high performance solution for IPC (Inter-Process Communication)
and user space container networking.

Virtio_user with vhost-kernel backend is a solution for exceptional path,
such as KNI which exchanges packets with kernel networking stack. This
solution is very promising in:

*   Maintenance

    All kernel modules needed by this solution, vhost and vhost-net (kernel),
    are upstreamed and extensively used kernel module.

*   Features

    vhost-net is born to be a networking solution, which has lots of networking
    related features, like multi queue, tso, multi-seg mbuf, etc.

*   Performance

    similar to KNI, this solution would use one or more kthreads to
    send/receive packets to/from user space DPDK applications, which has little
    impact on user space polling thread (except that it might enter into kernel
    space to wake up those kthreads if necessary).

The overview of an application using virtio-user as exceptional path is shown
in :numref:`figure_virtio_user_as_exceptional_path`.

.. _figure_virtio_user_as_exceptional_path:

.. figure:: img/virtio_user_as_exceptional_path.*

   Overview of a DPDK app using virtio-user as exceptional path


Sample Usage
------------

As a prerequisite, the vhost/vhost-net kernel CONFIG should be chosen before
compiling the kernel and those kernel modules should be inserted.

#.  Compile DPDK and bind a physical NIC to igb_uio/uio_pci_generic/vfio-pci.

    This physical NIC is for communicating with outside.

#.  Run testpmd.

    .. code-block:: console

        $(testpmd) -l 2-3 -n 4 \
		--vdev=virtio_user0,path=/dev/vhost-net,queue_size=1024 \
		-- -i --tx-offloads=0x0000002c --enable-lro \
		--txd=1024 --rxd=1024

    This command runs testpmd with two ports, one physical NIC to communicate
    with outside, and one virtio-user to communicate with kernel.

* ``--enable-lro``

    This is used to negotiate VIRTIO_NET_F_GUEST_TSO4 and
    VIRTIO_NET_F_GUEST_TSO6 feature so that large packets from kernel can be
    transmitted to DPDK application and further TSOed by physical NIC.

* ``queue_size``

    256 by default. To avoid shortage of descriptors, we can increase it to 1024.

* ``queues``

    Number of multi-queues. Each queue will be served by a kthread. For example:

    .. code-block:: console

        $(testpmd) -l 2-3 -n 4 \
		--vdev=virtio_user0,path=/dev/vhost-net,queues=2,queue_size=1024 \
		-- -i --tx-offloads=0x0000002c --enable-lro \
		--txq=2 --rxq=2 --txd=1024 --rxd=1024

#. Enable Rx checksum offloads in testpmd:

    .. code-block:: console

        (testpmd) port stop 0
        (testpmd) port config 0 rx_offload tcp_cksum on
        (testpmd) port config 0 rx_offload udp_cksum on
        (testpmd) port start 0

#. Start testpmd:

    .. code-block:: console

        (testpmd) start

#.  Configure IP address and start tap:

    .. code-block:: console

        ifconfig tap0 1.1.1.1/24 up

.. note::

    The tap device will be named tap0, tap1, etc, by kernel.

Then, all traffic from physical NIC can be forwarded into kernel stack, and all
traffic on the tap0 can be sent out from physical NIC.

Limitations
-----------

This solution is only available on Linux systems.
