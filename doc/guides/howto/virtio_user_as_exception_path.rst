..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016 Intel Corporation.

.. _virtio_user_as_exception_path:

Virtio_user as Exception Path
=============================

.. note::

   This solution is only applicable to Linux systems.

The virtual device, virtio-user, was originally introduced with the vhost-user
backend as a high performance solution for IPC (Inter-Process Communication)
and user space container networking.

Beyond this originally intended use,
virtio-user can be used in conjunction with the vhost-kernel backend
as a solution for dealing with exception path packets
which need to be injected into the Linux kernel for processing there.
In this regard, virtio-user and vhost in kernel space are an alternative to DPDK KNI
for transferring packets between a DPDK packet processing application and the kernel stack.

This solution has a number of advantages over alternatives such as KNI:

*   Maintenance

    All kernel modules needed by this solution, vhost and vhost-net (kernel),
    are upstreamed and extensively used.

*   Features

    vhost-net is designed to be a networking solution, and, as such,
    has lots of networking related features,
    such as multi queue support, TSO, multi-segment buffer support, etc.

*   Performance

    Similar to KNI, this solution would use one or more kthreads
    to send/receive packets to/from user space DPDK applications,
    which minimises the impact on the polling DPDK threads.

The overview of an application using virtio-user as exception path is shown
in :numref:`figure_virtio_user_as_exception_path`.

.. _figure_virtio_user_as_exception_path:

.. figure:: img/virtio_user_as_exception_path.*

   Overview of a DPDK app using virtio-user as exception path


Example Usage With Testpmd
---------------------------

.. note::

   These instructions assume that the vhost/vhost-net kernel modules are available
   and have already been loaded into the running kernel.
   It also assumes that the DPDK virtio driver has not been disabled in the DPDK build.

To run a simple test of virtio-user as exception path using testpmd:

#. Compile DPDK and bind a NIC to vfio-pci as documented in :ref:`linux_gsg_linux_drivers`.

   This physical NIC is for communicating with the outside world,
   and serves as a packet source in this example.

#. Run testpmd to forward packets from NIC to kernel,
   passing in a suitable list of logical cores to run on  (``-l`` parameter),
   and optionally the PCI address of the physical NIC to use (``-a`` parameter).
   The virtio-user device for interfacing to the kernel is specified via a ``--vdev`` argument,
   taking the parameters described below.

   .. code-block:: console

      /path/to/dpdk-testpmd -l <cores> -a <pci BDF> \
          --vdev=virtio_user0,path=/dev/vhost-net,queues=1,queue_size=1024

   ``path``
     The path to the kernel vhost-net device.

   ``queue_size``
     256 by default. To avoid shortage of descriptors, we can increase it to 1024.

   ``queues``
     Number of virt-queues. Each queue will be served by a kthread.

#. Once testpmd is running, a new network interface - called ``tap0`` by default -
   will be present on the system.
   This should be configured with an IP address and then enabled for use:

   .. code-block:: console

      ip addr add 192.168.1.1/24 dev tap0
      ip link set dev tap0 up

#. To observe packet forwarding through the kernel,
   a second testpmd instance can be run on the system,
   taking packets from the kernel using an ``af_packet`` socket on the ``tap0`` interface.

   .. code-block:: console

      /path/to/dpdk-testpmd -l <cores> --vdev=net_af_packet0,iface=tap0 --in-memory --no-pci

   When running this instance,
   we can use ``--in-memory`` flag to avoid hugepage naming conflicts with the previous instance,
   and we also use ``--no-pci`` flag to only use the ``af_packet`` interface
   for all traffic forwarding.

#. Running traffic into the system through the NIC should see that traffic returned back again,
   having been forwarded through both testpmd instances.
   This can be confirmed by checking the testpmd statistics on testpmd exit.

For more advanced use of virtio-user with testpmd in this scenario,
some other more advanced options may also be used.
For example:

* ``--tx-offloads=0x02c``

  This testpmd option enables Tx offloads for UDP and TCP checksum on transmit,
  as well as TCP TSO support.
  The list of the offload flag values can be seen in header
  `rte_ethdev.h <https://doc.dpdk.org/api/rte__ethdev_8h.html>`_.

* ``--enable-lro``

  This testpmd option is used to negotiate VIRTIO_NET_F_GUEST_TSO4 and
  VIRTIO_NET_F_GUEST_TSO6 feature so that large packets from the kernel can be
  transmitted to the DPDK application and further TSOed by physical NIC.
  If unsupported by the physical NIC, errors may be reported by testpmd with this option.

* Enabling Rx checksum offloads for physical port:

  Within testpmd, you can enable and disable offloads on a per-port basis,
  rather than enabling them for both ports.
  For the physical NIC, it may be desirable to enable checksum offload on packet Rx.
  This may be done as below, if testpmd is run with ``-i`` flag for interactive mode.

   .. code-block:: console

      testpmd> port stop 0
      testpmd> port config 0 rx_offload tcp_cksum on
      testpmd> port config 0 rx_offload udp_cksum on
      testpmd> port start 0

* Multiple queue support

  Better performance may be achieved by using multiple queues,
  so that multiple kernel threads are handling the traffic on the kernel side.
  For example, to use 2 queues on both NIC and virtio ports,
  while also enabling TX offloads and LRO support:

  .. code-block:: console

     /path/to/dpdk-testpmd --vdev=virtio_user0,path=/dev/vhost-net,queues=2,queue_size=1024 -- \
         -i --tx-offloads=0x002c --enable-lro --txq=2 --rxq=2 --txd=1024 --rxd=1024

