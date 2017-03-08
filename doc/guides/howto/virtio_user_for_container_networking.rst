..  BSD LICENSE
    Copyright(c) 2016 Intel Corporation. All rights reserved.
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

.. _virtio_user_for_container_networking:

Virtio_user for Container Networking
====================================

Container becomes more and more popular for strengths, like low overhead, fast
boot-up time, and easy to deploy, etc. How to use DPDK to accelerate container
networking becomes a common question for users. There are two use models of
running DPDK inside containers, as shown in
:numref:`figure_use_models_for_running_dpdk_in_containers`.

.. _figure_use_models_for_running_dpdk_in_containers:

.. figure:: img/use_models_for_running_dpdk_in_containers.*

   Use models of running DPDK inside container

This page will only cover aggregation model.

Overview
--------

The virtual device, virtio-user, with unmodified vhost-user backend, is designed
for high performance user space container networking or inter-process
communication (IPC).

The overview of accelerating container networking by virtio-user is shown
in :numref:`figure_virtio_user_for_container_networking`.

.. _figure_virtio_user_for_container_networking:

.. figure:: img/virtio_user_for_container_networking.*

   Overview of accelerating container networking by virtio-user

Different virtio PCI devices we usually use as a para-virtualization I/O in the
context of QEMU/VM, the basic idea here is to present a kind of virtual devices,
which can be attached and initialized by DPDK. The device emulation layer by
QEMU in VM's context is saved by just registering a new kind of virtual device
in DPDK's ether layer. And to minimize the change, we reuse already-existing
virtio PMD code (driver/net/virtio/).

Virtio, in essence, is a shm-based solution to transmit/receive packets. How is
memory shared? In VM's case, qemu always shares the whole physical layout of VM
to vhost backend. But it's not feasible for a container, as a process, to share
all virtual memory regions to backend. So only those virtual memory regions
(aka, hugepages initialized in DPDK) are sent to backend. It restricts that only
addresses in these areas can be used to transmit or receive packets.

Sample Usage
------------

Here we use Docker as container engine. It also applies to LXC, Rocket with
some minor changes.

#. Compile DPDK.

    .. code-block:: console

        make install RTE_SDK=`pwd` T=x86_64-native-linuxapp-gcc

#. Write a Dockerfile like below.

    .. code-block:: console

	cat <<EOT >> Dockerfile
	FROM ubuntu:latest
	WORKDIR /usr/src/dpdk
	COPY . /usr/src/dpdk
	ENV PATH "$PATH:/usr/src/dpdk/x86_64-native-linuxapp-gcc/app/"
	EOT

#. Build a Docker image.

    .. code-block:: console

	docker build -t dpdk-app-testpmd .

#. Start a testpmd on the host with a vhost-user port.

    .. code-block:: console

        $(testpmd) -l 0-1 -n 4 --socket-mem 1024,1024 \
            --vdev 'eth_vhost0,iface=/tmp/sock0' \
            --file-prefix=host --no-pci -- -i

#. Start a container instance with a virtio-user port.

    .. code-block:: console

        docker run -i -t -v /tmp/sock0:/var/run/usvhost \
            -v /dev/hugepages:/dev/hugepages \
            dpdk-app-testpmd testpmd -l 6-7 -n 4 -m 1024 --no-pci \
            --vdev=virtio_user0,path=/var/run/usvhost \
            --file-prefix=container \
            -- -i --txqflags=0xf00 --disable-hw-vlan

Note: If we run all above setup on the host, it's a shm-based IPC.

Limitations
-----------

We have below limitations in this solution:
 * Cannot work with --huge-unlink option. As we need to reopen the hugepage
   file to share with vhost backend.
 * Cannot work with --no-huge option. Currently, DPDK uses anonymous mapping
   under this option which cannot be reopened to share with vhost backend.
 * Cannot work when there are more than VHOST_MEMORY_MAX_NREGIONS(8) hugepages.
   In another word, do not use 2MB hugepage so far.
 * Applications should not use file name like HUGEFILE_FMT ("%smap_%d"). That
   will bring confusion when sharing hugepage files with backend by name.
 * Root privilege is a must. DPDK resolves physical addresses of hugepages
   which seems not necessary, and some discussions are going on to remove this
   restriction.
