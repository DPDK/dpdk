..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016 Intel Corporation.

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
            -- -i

Note: If we run all above setup on the host, it's a shm-based IPC.

Limitations
-----------

We have below limitations in this solution:
 * Cannot work with --huge-unlink option. As we need to reopen the hugepage
   file to share with vhost backend.
 * Cannot work with --no-huge option. Currently, DPDK uses anonymous mapping
   under this option which cannot be reopened to share with vhost backend.
 * Cannot work when there are more than VHOST_MEMORY_MAX_NREGIONS(8) hugepages.
   If you have more regions (especially when 2MB hugepages are used), the option,
   --single-file-segments, can help to reduce the number of shared files.
 * Applications should not use file name like HUGEFILE_FMT ("%smap_%d"). That
   will bring confusion when sharing hugepage files with backend by name.
 * Root privilege is a must. DPDK resolves physical addresses of hugepages
   which seems not necessary, and some discussions are going on to remove this
   restriction.
