..  BSD LICENSE
    Copyright(c) 2016 Red Hat, Inc. All rights reserved.
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


PVP reference benchmark setup using testpmd
===========================================

This guide lists the steps required to setup a PVP benchmark using testpmd as
a simple forwarder between NICs and Vhost interfaces. The goal of this setup
is to have a reference PVP benchmark without using external vSwitches (OVS,
VPP, ...) to make it easier to obtain reproducible results and to facilitate
continuous integration testing.

The guide covers two ways of launching the VM, either by directly calling the
QEMU command line, or by relying on libvirt. It has been tested with DPDK
v16.11 using RHEL7 for both host and guest.


Setup overview
--------------

.. _figure_pvp_2nics:

.. figure:: img/pvp_2nics.*

   PVP setup using 2 NICs

In this diagram, each red arrow represents one logical core. This use-case
requires 6 dedicated logical cores. A forwarding configuration with a single
NIC is also possible, requiring 3 logical cores.


Host setup
----------

In this setup, we isolate 6 cores (from CPU2 to CPU7) on the same NUMA
node. Two cores are assigned to the VM vCPUs running testpmd and four are
assigned to testpmd on the host.


Host tuning
~~~~~~~~~~~

#. On BIOS, disable turbo-boost and hyper-threads.

#. Append these options to Kernel command line:

   .. code-block:: console

      intel_pstate=disable mce=ignore_ce default_hugepagesz=1G hugepagesz=1G hugepages=6 isolcpus=2-7 rcu_nocbs=2-7 nohz_full=2-7 iommu=pt intel_iommu=on

#. Disable hyper-threads at runtime if necessary or if BIOS is not accessible:

   .. code-block:: console

      cat /sys/devices/system/cpu/cpu*[0-9]/topology/thread_siblings_list \
          | sort | uniq \
          | awk -F, '{system("echo 0 > /sys/devices/system/cpu/cpu"$2"/online")}'

#. Disable NMIs:

   .. code-block:: console

      echo 0 > /proc/sys/kernel/nmi_watchdog

#. Exclude isolated CPUs from the writeback cpumask:

   .. code-block:: console

      echo ffffff03 > /sys/bus/workqueue/devices/writeback/cpumask

#. Isolate CPUs from IRQs:

   .. code-block:: console

      clear_mask=0xfc #Isolate CPU2 to CPU7 from IRQs
      for i in /proc/irq/*/smp_affinity
      do
        echo "obase=16;$(( 0x$(cat $i) & ~$clear_mask ))" | bc > $i
      done


Qemu build
~~~~~~~~~~

Build Qemu:

    .. code-block:: console

       git clone git://git.qemu.org/qemu.git
       cd qemu
       mkdir bin
       cd bin
       ../configure --target-list=x86_64-softmmu
       make


DPDK build
~~~~~~~~~~

Build DPDK:

   .. code-block:: console

      git clone git://dpdk.org/dpdk
      cd dpdk
      export RTE_SDK=$PWD
      make install T=x86_64-native-linuxapp-gcc DESTDIR=install


Testpmd launch
~~~~~~~~~~~~~~

#. Assign NICs to DPDK:

   .. code-block:: console

      modprobe vfio-pci
      $RTE_SDK/install/sbin/dpdk-devbind -b vfio-pci 0000:11:00.0 0000:11:00.1

   .. Note::

      The Sandy Bridge family seems to have some IOMMU limitations giving poor
      performance results. To achieve good performance on these machines
      consider using UIO instead.

#. Launch the testpmd application:

   .. code-block:: console

      $RTE_SDK/install/bin/testpmd -l 0,2,3,4,5 --socket-mem=1024 -n 4 \
          --vdev 'net_vhost0,iface=/tmp/vhost-user1' \
          --vdev 'net_vhost1,iface=/tmp/vhost-user2' -- \
          --portmask=f -i --rxq=1 --txq=1 \
          --nb-cores=4 --forward-mode=io

   With this command, isolated CPUs 2 to 5 will be used as lcores for PMD threads.

#. In testpmd interactive mode, set the portlist to obtain the correct port
   chaining:

   .. code-block:: console

      set portlist 0,2,1,3
      start


VM launch
~~~~~~~~~

The VM may be launched either by calling QEMU directly, or by using libvirt.

Qemu way
^^^^^^^^

Launch QEMU with two Virtio-net devices paired to the vhost-user sockets
created by testpmd. Below example uses default Virtio-net options, but options
may be specified, for example to disable mergeable buffers or indirect
descriptors.

   .. code-block:: console

      <QEMU path>/bin/x86_64-softmmu/qemu-system-x86_64 \
          -enable-kvm -cpu host -m 3072 -smp 3 \
          -chardev socket,id=char0,path=/tmp/vhost-user1 \
          -netdev type=vhost-user,id=mynet1,chardev=char0,vhostforce \
          -device virtio-net-pci,netdev=mynet1,mac=52:54:00:02:d9:01,addr=0x10 \
          -chardev socket,id=char1,path=/tmp/vhost-user2 \
          -netdev type=vhost-user,id=mynet2,chardev=char1,vhostforce \
          -device virtio-net-pci,netdev=mynet2,mac=52:54:00:02:d9:02,addr=0x11 \
          -object memory-backend-file,id=mem,size=3072M,mem-path=/dev/hugepages,share=on \
          -numa node,memdev=mem -mem-prealloc \
          -net user,hostfwd=tcp::1002$1-:22 -net nic \
          -qmp unix:/tmp/qmp.socket,server,nowait \
          -monitor stdio <vm_image>.qcow2

You can use this `qmp-vcpu-pin <https://patchwork.kernel.org/patch/9361617/>`_
script to pin vCPUs.

It can be used as follows, for example to pin 3 vCPUs to CPUs 1, 6 and 7,
where isolated CPUs 6 and 7 will be used as lcores for Virtio PMDs:

   .. code-block:: console

      export PYTHONPATH=$PYTHONPATH:<QEMU path>/scripts/qmp
      ./qmp-vcpu-pin -s /tmp/qmp.socket 1 6 7

Libvirt way
^^^^^^^^^^^

Some initial steps are required for libvirt to be able to connect to testpmd's
sockets.

First, SELinux policy needs to be set to permissive, since testpmd is
generally run as root (note, as reboot is required):

   .. code-block:: console

      cat /etc/selinux/config

      # This file controls the state of SELinux on the system.
      # SELINUX= can take one of these three values:
      #     enforcing  - SELinux security policy is enforced.
      #     permissive - SELinux prints warnings instead of enforcing.
      #     disabled   - No SELinux policy is loaded.
      SELINUX=permissive

      # SELINUXTYPE= can take one of three two values:
      #     targeted - Targeted processes are protected,
      #     minimum  - Modification of targeted policy.
      #                Only selected processes are protected.
      #     mls      - Multi Level Security protection.
      SELINUXTYPE=targeted


Also, Qemu needs to be run as root, which has to be specified in
``/etc/libvirt/qemu.conf``:

   .. code-block:: console

      user = "root"

Once the domain created, the following snippet is an extract of he most
important information (hugepages, vCPU pinning, Virtio PCI devices):

   .. code-block:: xml

      <domain type='kvm'>
        <memory unit='KiB'>3145728</memory>
        <currentMemory unit='KiB'>3145728</currentMemory>
        <memoryBacking>
          <hugepages>
            <page size='1048576' unit='KiB' nodeset='0'/>
          </hugepages>
          <locked/>
        </memoryBacking>
        <vcpu placement='static'>3</vcpu>
        <cputune>
          <vcpupin vcpu='0' cpuset='1'/>
          <vcpupin vcpu='1' cpuset='6'/>
          <vcpupin vcpu='2' cpuset='7'/>
          <emulatorpin cpuset='0'/>
        </cputune>
        <numatune>
          <memory mode='strict' nodeset='0'/>
        </numatune>
        <os>
          <type arch='x86_64' machine='pc-i440fx-rhel7.0.0'>hvm</type>
          <boot dev='hd'/>
        </os>
        <cpu mode='host-passthrough'>
          <topology sockets='1' cores='3' threads='1'/>
          <numa>
            <cell id='0' cpus='0-2' memory='3145728' unit='KiB' memAccess='shared'/>
          </numa>
        </cpu>
        <devices>
          <interface type='vhostuser'>
            <mac address='56:48:4f:53:54:01'/>
            <source type='unix' path='/tmp/vhost-user1' mode='client'/>
            <model type='virtio'/>
            <driver name='vhost' rx_queue_size='256' />
            <address type='pci' domain='0x0000' bus='0x00' slot='0x10' function='0x0'/>
          </interface>
          <interface type='vhostuser'>
            <mac address='56:48:4f:53:54:02'/>
            <source type='unix' path='/tmp/vhost-user2' mode='client'/>
            <model type='virtio'/>
            <driver name='vhost' rx_queue_size='256' />
            <address type='pci' domain='0x0000' bus='0x00' slot='0x11' function='0x0'/>
          </interface>
        </devices>
      </domain>


Guest setup
-----------


Guest tuning
~~~~~~~~~~~~

#. Append these options to the Kernel command line:

   .. code-block:: console

      default_hugepagesz=1G hugepagesz=1G hugepages=1 intel_iommu=on iommu=pt isolcpus=1,2 rcu_nocbs=1,2 nohz_full=1,2

#. Disable NMIs:

   .. code-block:: console

      echo 0 > /proc/sys/kernel/nmi_watchdog

#. Exclude isolated CPU1 and CPU2 from the writeback cpumask:

   .. code-block:: console

      echo 1 > /sys/bus/workqueue/devices/writeback/cpumask

#. Isolate CPUs from IRQs:

   .. code-block:: console

      clear_mask=0x6 #Isolate CPU1 and CPU2 from IRQs
      for i in /proc/irq/*/smp_affinity
      do
        echo "obase=16;$(( 0x$(cat $i) & ~$clear_mask ))" | bc > $i
      done


DPDK build
~~~~~~~~~~

Build DPDK:

   .. code-block:: console

      git clone git://dpdk.org/dpdk
      cd dpdk
      export RTE_SDK=$PWD
      make install T=x86_64-native-linuxapp-gcc DESTDIR=install


Testpmd launch
~~~~~~~~~~~~~~

Probe vfio module without iommu:

   .. code-block:: console

      modprobe -r vfio_iommu_type1
      modprobe -r vfio
      modprobe  vfio enable_unsafe_noiommu_mode=1
      cat /sys/module/vfio/parameters/enable_unsafe_noiommu_mode
      modprobe vfio-pci

Bind the virtio-net devices to DPDK:

   .. code-block:: console

      $RTE_SDK/usertools/dpdk-devbind.py -b vfio-pci 0000:00:10.0 0000:00:11.0

Start testpmd:

   .. code-block:: console

      $RTE_SDK/install/bin/testpmd -l 0,1,2 --socket-mem 1024 -n 4 \
          --proc-type auto --file-prefix pg -- \
          --portmask=3 --forward-mode=macswap --port-topology=chained \
          --disable-rss -i --rxq=1 --txq=1 \
          --rxd=256 --txd=256 --nb-cores=2 --auto-start

Results template
----------------

Below template should be used when sharing results:

   .. code-block:: none

      Traffic Generator: <Test equipment (e.g. IXIA, Moongen, ...)>
      Acceptable Loss: <n>%
      Validation run time: <n>min
      Host DPDK version/commit: <version, SHA-1>
      Guest DPDK version/commit: <version, SHA-1>
      Patches applied: <link to patchwork>
      QEMU version/commit: <version>
      Virtio features: <features (e.g. mrg_rxbuf='off', leave empty if default)>
      CPU: <CPU model>, <CPU frequency>
      NIC: <NIC model>
      Result: <n> Mpps
