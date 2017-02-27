
..  BSD LICENSE
    Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
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


Vhost Sample Application
========================

The vhost sample application demonstrates integration of the Data Plane
Development Kit (DPDK) with the Linux* KVM hypervisor by implementing the
vhost-net offload API. The sample application performs simple packet
switching between virtual machines based on Media Access Control (MAC)
address or Virtual Local Area Network (VLAN) tag. The splitting of Ethernet
traffic from an external switch is performed in hardware by the Virtual
Machine Device Queues (VMDQ) and Data Center Bridging (DCB) features of
the Intel® 82599 10 Gigabit Ethernet Controller.

Testing steps
-------------

This section shows the steps how to test a typical PVP case with this
vhost-switch sample, whereas packets are received from the physical NIC
port first and enqueued to the VM's Rx queue. Through the guest testpmd's
default forwarding mode (io forward), those packets will be put into
the Tx queue. The vhost-switch example, in turn, gets the packets and
puts back to the same physical NIC port.

Build
~~~~~

Follow the *Getting Started Guide for Linux* on generic info about
environment setup and building DPDK from source.

In this example, you need build DPDK both on the host and inside guest.
Also, you need build this example.

.. code-block:: console

    export RTE_SDK=/path/to/dpdk_source
    export RTE_TARGET=x86_64-native-linuxapp-gcc

    cd ${RTE_SDK}/examples/vhost
    make


Start the vswitch example
~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: console

        ./vhost-switch -l 0-3 -n 4 --socket-mem 1024  \
             -- --socket-file /tmp/sock0 --client \
             ...

Check the `Parameters`_ section for the explanations on what do those
parameters mean.

.. _vhost_app_run_vm:

Start the VM
~~~~~~~~~~~~

.. code-block:: console

    qemu-system-x86_64 -machine accel=kvm -cpu host \
        -m $mem -object memory-backend-file,id=mem,size=$mem,mem-path=/dev/hugepages,share=on \
                -mem-prealloc -numa node,memdev=mem \
        \
        -chardev socket,id=char1,path=/tmp/sock0,server \
        -netdev type=vhost-user,id=hostnet1,chardev=char1  \
        -device virtio-net-pci,netdev=hostnet1,id=net1,mac=52:54:00:00:00:14 \
        ...

.. note::
    For basic vhost-user support, QEMU 2.2 (or above) is required. For
    some specific features, a higher version might be need. Such as
    QEMU 2.7 (or above) for the reconnect feature.

.. _vhost_app_run_dpdk_inside_guest:

Run testpmd inside guest
~~~~~~~~~~~~~~~~~~~~~~~~

Make sure you have DPDK built inside the guest. Also make sure the
corresponding virtio-net PCI device is bond to a uio driver, which
could be done by:

.. code-block:: console

   modprobe uio_pci_generic
   $RTE_SDK/usertools/dpdk-devbind.py -b=uio_pci_generic 0000:00:04.0

Then start testpmd for packet forwarding testing.

.. code-block:: console

    ./x86_64-native-gcc/app/testpmd -l 0-1 -- -i
    > start tx_first

Inject packets
--------------

While a virtio-net is connected to vhost-switch, a VLAN tag starts with
1000 is assigned to it. So make sure configure your packet generator
with the right MAC and VLAN tag, you should be able to see following
log from the vhost-switch console. It means you get it work::

    VHOST_DATA: (0) mac 52:54:00:00:00:14 and vlan 1000 registered


.. _vhost_app_parameters:

Parameters
----------

**--socket-file path**
Specifies the vhost-user socket file path.

**--client**
DPDK vhost-user will act as the client mode when such option is given.
In the client mode, QEMU will create the socket file. Otherwise, DPDK
will create it. Put simply, it's the server to create the socket file.


**--vm2vm mode**
The vm2vm parameter sets the mode of packet switching between guests in
the host.

- 0 disables vm2vm, impling that VM's packets will always go to the NIC port.
- 1 means a normal mac lookup packet routing.
- 2 means hardware mode packet forwarding between guests, it allows packets
  go to the NIC port, hardware L2 switch will determine which guest the
  packet should forward to or need send to external, which bases on the
  packet destination MAC address and VLAN tag.

**--mergeable 0|1**
Set 0/1 to disable/enable the mergeable Rx feature. It's disabled by default.

**--stats interval**
The stats parameter controls the printing of virtio-net device statistics.
The parameter specifies an interval (in unit of seconds) to print statistics,
with an interval of 0 seconds disabling statistics.

**--rx-retry 0|1**
The rx-retry option enables/disables enqueue retries when the guests Rx queue
is full. This feature resolves a packet loss that is observed at high data
rates, by allowing it to delay and retry in the receive path. This option is
enabled by default.

**--rx-retry-num num**
The rx-retry-num option specifies the number of retries on an Rx burst, it
takes effect only when rx retry is enabled.  The default value is 4.

**--rx-retry-delay msec**
The rx-retry-delay option specifies the timeout (in micro seconds) between
retries on an RX burst, it takes effect only when rx retry is enabled. The
default value is 15.

**--dequeue-zero-copy**
Dequeue zero copy will be enabled when this option is given.

**--vlan-strip 0|1**
VLAN strip option is removed, because different NICs have different behaviors
when disabling VLAN strip. Such feature, which heavily depends on hardware,
should be removed from this example to reduce confusion. Now, VLAN strip is
enabled and cannot be disabled.

Common Issues
-------------

* QEMU fails to allocate memory on hugetlbfs, with an error like the
  following::

      file_ram_alloc: can't mmap RAM pages: Cannot allocate memory

  When running QEMU the above error indicates that it has failed to allocate
  memory for the Virtual Machine on the hugetlbfs. This is typically due to
  insufficient hugepages being free to support the allocation request. The
  number of free hugepages can be checked as follows:

  .. code-block:: console

      cat /sys/kernel/mm/hugepages/hugepages-<pagesize>/nr_hugepages

  The command above indicates how many hugepages are free to support QEMU's
  allocation request.

* vhost-user will not work with QEMU without the ``-mem-prealloc`` option

  The current implementation works properly only when the guest memory is
  pre-allocated.

* vhost-user will not work with a QEMU version without shared memory mapping:

  Make sure ``share=on`` QEMU option is given.

* Failed to build DPDK in VM

  Make sure "-cpu host" QEMU option is given.
