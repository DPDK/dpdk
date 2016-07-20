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

In future release, we will also make enhancement to vhost backend,
releasing peak performance of virtio PMD driver.

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

*   The descriptor number for the Rx/Tx queue is hard-coded to be 256 by qemu.
    If given a different descriptor number by the upper application,
    the virtio PMD generates a warning and fall back to the hard-coded value.

*   Features of mac/vlan filter are supported, negotiation with vhost/backend are needed to support them.
    When backend can't support vlan filter, virtio app on guest should disable vlan filter to make sure
    the virtio port is configured correctly. E.g. specify '--disable-hw-vlan' in testpmd command line.

*   RTE_PKTMBUF_HEADROOM should be defined larger than sizeof(struct virtio_net_hdr), which is 10 bytes.

*   Virtio does not support runtime configuration.

*   Virtio supports Link State interrupt.

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

        examples/kni/build/app/kni -c 0xf -n 4 -- -p 0x1 -P --config="(0,1,3)"

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
        python tools/dpdk-devbind.py -b uio_pci_generic 00:03.0

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

Virtio driver has 3 Rx callbacks and 2 Tx callbacks.

Rx callbacks:

#. ``virtio_recv_pkts``:
   Regular version without mergeable Rx buffer support.

#. ``virtio_recv_mergeable_pkts``:
   Regular version with mergeable Rx buffer support.

#. ``virtio_recv_pkts_vec``:
   Vector version without mergeable Rx buffer support, also fixes the available
   ring indexes and uses vector instructions to optimize performance.

Tx callbacks:

#. ``virtio_xmit_pkts``:
   Regular version.

#. ``virtio_xmit_pkts_simple``:
   Vector version fixes the available ring indexes to optimize performance.


By default, the non-vector callbacks are used:

*   For Rx: If mergeable Rx buffers is disabled then ``virtio_recv_pkts`` is
    used; otherwise ``virtio_recv_mergeable_pkts``.

*   For Tx: ``virtio_xmit_pkts``.


Vector callbacks will be used when:

*   ``txq_flags`` is set to ``VIRTIO_SIMPLE_FLAGS`` (0xF01), which implies:

    *   Single segment is specified.

    *   No offload support is needed.

*   Mergeable Rx buffers is disabled.

The corresponding callbacks are:

*   For Rx: ``virtio_recv_pkts_vec``.

*   For Tx: ``virtio_xmit_pkts_simple``.


Example of using the vector version of the virtio poll mode driver in
``testpmd``::

   testpmd -c 0x7 -n 4 -- -i --txqflags=0xF01 --rxq=1 --txq=1 --nb-cores=1
