..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2015 Intel Corporation.

Poll Mode Driver for Emulated Virtio NIC
========================================

Virtio is a para-virtualization framework initiated by IBM, and supported by KVM hypervisor.
In the Data Plane Development Kit (DPDK),
we provide a virtio Poll Mode Driver (PMD) as a software solution, comparing to SRIOV hardware solution,

for fast guest VM to guest VM communication and guest VM to host communication.

Vhost is a kernel acceleration module for virtio qemu backend.
The DPDK extends kni to support vhost raw socket interface,
which enables vhost to directly read/ write packets from/to a physical port.
With this enhancement, virtio could achieve quite promising performance.

For basic qemu-KVM installation and other Intel EM poll mode driver in guest VM,
please refer to Chapter "Driver for VM Emulated Devices".

In this chapter, we will demonstrate usage of virtio PMD driver with two backends,
standard qemu vhost back end and vhost kni back end.

Virtio Implementation in DPDK
-----------------------------

For details about the virtio spec, refer to Virtio PCI Card Specification written by Rusty Russell.

As a PMD, virtio provides packet reception and transmission callbacks virtio_recv_pkts and virtio_xmit_pkts.

In virtio_recv_pkts, index in range [vq->vq_used_cons_idx , vq->vq_ring.used->idx) in vring is available for virtio to burst out.

In virtio_xmit_pkts, same index range in vring is available for virtio to clean.
Virtio will enqueue to be transmitted packets into vring, advance the vq->vq_ring.avail->idx,
and then notify the host back end if necessary.

Features and Limitations of virtio PMD
--------------------------------------

In this release, the virtio PMD driver provides the basic functionality of packet reception and transmission.

*   It supports merge-able buffers per packet when receiving packets and scattered buffer per packet
    when transmitting packets. The packet size supported is from 64 to 1518.

*   It supports multicast packets and promiscuous mode.

*   The descriptor number for the Rx/Tx queue is hard-coded to be 256 by qemu 2.7 and below.
    If given a different descriptor number by the upper application,
    the virtio PMD generates a warning and fall back to the hard-coded value.
    Rx queue size can be configurable and up to 1024 since qemu 2.8 and above. Rx queue size is 256
    by default. Tx queue size is still hard-coded to be 256.

*   Features of mac/vlan filter are supported, negotiation with vhost/backend are needed to support them.
    When backend can't support vlan filter, virtio app on guest should not enable vlan filter in order
    to make sure the virtio port is configured correctly. E.g. do not specify '--enable-hw-vlan' in testpmd
    command line.

*   "RTE_PKTMBUF_HEADROOM" should be defined
    no less than "sizeof(struct virtio_net_hdr_mrg_rxbuf)", which is 12 bytes when mergeable or
    "VIRTIO_F_VERSION_1" is set.
    no less than "sizeof(struct virtio_net_hdr)", which is 10 bytes, when using non-mergeable.

*   Virtio does not support runtime configuration.

*   Virtio supports Link State interrupt.

*   Virtio supports Rx interrupt (so far, only support 1:1 mapping for queue/interrupt).

*   Virtio supports software vlan stripping and inserting.

*   Virtio supports using port IO to get PCI resource when uio/igb_uio module is not available.

Prerequisites
-------------

The following prerequisites apply:

*   In the BIOS, turn VT-x and VT-d on

*   Linux kernel with KVM module; vhost module loaded and ioeventfd supported.
    Qemu standard backend without vhost support isn't tested, and probably isn't supported.

Virtio with kni vhost Back End
------------------------------

This section demonstrates kni vhost back end example setup for Phy-VM Communication.

.. _figure_host_vm_comms:

.. figure:: img/host_vm_comms.*

   Host2VM Communication Example Using kni vhost Back End


Host2VM communication example

#.  Load the kni kernel module:

    .. code-block:: console

        insmod rte_kni.ko

    Other basic DPDK preparations like hugepage enabling, uio port binding are not listed here.
    Please refer to the *DPDK Getting Started Guide* for detailed instructions.

#.  Launch the kni user application:

    .. code-block:: console

        examples/kni/build/app/kni -l 0-3 -n 4 -- -p 0x1 -P --config="(0,1,3)"

    This command generates one network device vEth0 for physical port.
    If specify more physical ports, the generated network device will be vEth1, vEth2, and so on.

    For each physical port, kni creates two user threads.
    One thread loops to fetch packets from the physical NIC port into the kni receive queue.
    The other user thread loops to send packets in the kni transmit queue.

    For each physical port, kni also creates a kernel thread that retrieves packets from the kni receive queue,
    place them onto kni's raw socket's queue and wake up the vhost kernel thread to exchange packets with the virtio virt queue.

    For more details about kni, please refer to :ref:`kni`.

#.  Enable the kni raw socket functionality for the specified physical NIC port,
    get the generated file descriptor and set it in the qemu command line parameter.
    Always remember to set ioeventfd_on and vhost_on.

    Example:

    .. code-block:: console

        echo 1 > /sys/class/net/vEth0/sock_en
        fd=`cat /sys/class/net/vEth0/sock_fd`
        exec qemu-system-x86_64 -enable-kvm -cpu host \
        -m 2048 -smp 4 -name dpdk-test1-vm1 \
        -drive file=/data/DPDKVMS/dpdk-vm.img \
        -netdev tap, fd=$fd,id=mynet_kni, script=no,vhost=on \
        -device virtio-net-pci,netdev=mynet_kni,bus=pci.0,addr=0x3,ioeventfd=on \
        -vnc:1 -daemonize

    In the above example, virtio port 0 in the guest VM will be associated with vEth0, which in turns corresponds to a physical port,
    which means received packets come from vEth0, and transmitted packets is sent to vEth0.

#.  In the guest, bind the virtio device to the uio_pci_generic kernel module and start the forwarding application.
    When the virtio port in guest bursts Rx, it is getting packets from the
    raw socket's receive queue.
    When the virtio port bursts Tx, it is sending packet to the tx_q.

    .. code-block:: console

        modprobe uio
        echo 512 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
        modprobe uio_pci_generic
        python usertools/dpdk-devbind.py -b uio_pci_generic 00:03.0

    We use testpmd as the forwarding application in this example.

    .. figure:: img/console.*

       Running testpmd

#.  Use IXIA packet generator to inject a packet stream into the KNI physical port.

    The packet reception and transmission flow path is:

    IXIA packet generator->82599 PF->KNI Rx queue->KNI raw socket queue->Guest
    VM virtio port 0 Rx burst->Guest VM virtio port 0 Tx burst-> KNI Tx queue
    ->82599 PF-> IXIA packet generator

Virtio with qemu virtio Back End
--------------------------------

.. _figure_host_vm_comms_qemu:

.. figure:: img/host_vm_comms_qemu.*

   Host2VM Communication Example Using qemu vhost Back End


.. code-block:: console

    qemu-system-x86_64 -enable-kvm -cpu host -m 2048 -smp 2 -mem-path /dev/
    hugepages -mem-prealloc
    -drive file=/data/DPDKVMS/dpdk-vm1
    -netdev tap,id=vm1_p1,ifname=tap0,script=no,vhost=on
    -device virtio-net-pci,netdev=vm1_p1,bus=pci.0,addr=0x3,ioeventfd=on
    -device pci-assign,host=04:10.1 \

In this example, the packet reception flow path is:

    IXIA packet generator->82599 PF->Linux Bridge->TAP0's socket queue-> Guest
    VM virtio port 0 Rx burst-> Guest VM 82599 VF port1 Tx burst-> IXIA packet
    generator

The packet transmission flow is:

    IXIA packet generator-> Guest VM 82599 VF port1 Rx burst-> Guest VM virtio
    port 0 Tx burst-> tap -> Linux Bridge->82599 PF-> IXIA packet generator


Virtio PMD Rx/Tx Callbacks
--------------------------

Virtio driver has 4 Rx callbacks and 3 Tx callbacks.

Rx callbacks:

#. ``virtio_recv_pkts``:
   Regular version without mergeable Rx buffer support.

#. ``virtio_recv_mergeable_pkts``:
   Regular version with mergeable Rx buffer support.

#. ``virtio_recv_pkts_vec``:
   Vector version without mergeable Rx buffer support, also fixes the available
   ring indexes and uses vector instructions to optimize performance.

#. ``virtio_recv_mergeable_pkts_inorder``:
   In-order version with mergeable Rx buffer support.

Tx callbacks:

#. ``virtio_xmit_pkts``:
   Regular version.

#. ``virtio_xmit_pkts_simple``:
   Vector version fixes the available ring indexes to optimize performance.

#. ``virtio_xmit_pkts_inorder``:
   In-order version.

By default, the non-vector callbacks are used:

*   For Rx: If mergeable Rx buffers is disabled then ``virtio_recv_pkts`` is
    used; otherwise ``virtio_recv_mergeable_pkts``.

*   For Tx: ``virtio_xmit_pkts``.


Vector callbacks will be used when:

*   ``txmode.offloads`` is set to ``0x0``, which implies:

    *   Single segment is specified.

    *   No offload support is needed.

*   Mergeable Rx buffers is disabled.

The corresponding callbacks are:

*   For Rx: ``virtio_recv_pkts_vec``.

*   For Tx: ``virtio_xmit_pkts_simple``.


Example of using the vector version of the virtio poll mode driver in
``testpmd``::

   testpmd -l 0-2 -n 4 -- -i --tx-offloads=0x0 --rxq=1 --txq=1 --nb-cores=1

In-order callbacks only work on simulated virtio user vdev.

*   For Rx: If mergeable Rx buffers is enabled and in-order is enabled then
    ``virtio_xmit_pkts_inorder`` is used.

*   For Tx: If in-order is enabled then ``virtio_xmit_pkts_inorder`` is used.

Interrupt mode
--------------

.. _virtio_interrupt_mode:

There are three kinds of interrupts from a virtio device over PCI bus: config
interrupt, Rx interrupts, and Tx interrupts. Config interrupt is used for
notification of device configuration changes, especially link status (lsc).
Interrupt mode is translated into Rx interrupts in the context of DPDK.

.. Note::

   Virtio PMD already has support for receiving lsc from qemu when the link
   status changes, especially when vhost user disconnects. However, it fails
   to do that if the VM is created by qemu 2.6.2 or below, since the
   capability to detect vhost user disconnection is introduced in qemu 2.7.0.

Prerequisites for Rx interrupts
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To support Rx interrupts,
#. Check if guest kernel supports VFIO-NOIOMMU:

    Linux started to support VFIO-NOIOMMU since 4.8.0. Make sure the guest
    kernel is compiled with:

    .. code-block:: console

        CONFIG_VFIO_NOIOMMU=y

#. Properly set msix vectors when starting VM:

    Enable multi-queue when starting VM, and specify msix vectors in qemu
    cmdline. (N+1) is the minimum, and (2N+2) is mostly recommended.

    .. code-block:: console

        $(QEMU) ... -device virtio-net-pci,mq=on,vectors=2N+2 ...

#. In VM, insert vfio module in NOIOMMU mode:

    .. code-block:: console

        modprobe vfio enable_unsafe_noiommu_mode=1
        modprobe vfio-pci

#. In VM, bind the virtio device with vfio-pci:

    .. code-block:: console

        python usertools/dpdk-devbind.py -b vfio-pci 00:03.0

Example
~~~~~~~

Here we use l3fwd-power as an example to show how to get started.

    Example:

    .. code-block:: console

        $ l3fwd-power -l 0-1 -- -p 1 -P --config="(0,0,1)" \
                                               --no-numa --parse-ptype


Virtio PMD arguments
--------------------

The user can specify below argument in devargs.

#.  ``vdpa``:

    A virtio device could also be driven by vDPA (vhost data path acceleration)
    driver, and works as a HW vhost backend. This argument is used to specify
    a virtio device needs to work in vDPA mode.
    (Default: 0 (disabled))

#. ``mrg_rxbuf``:

    It is used to enable virtio device mergeable Rx buffer feature.
    (Default: 1 (enabled))

#. ``in_order``:

    It is used to enable virtio device in-order feature.
    (Default: 1 (enabled))
