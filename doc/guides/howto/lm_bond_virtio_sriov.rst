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

Live Migration of VM with SR-IOV VF
===================================

Overview
--------

It is not possible to migrate a Virtual Machine which has an SR-IOV Virtual Function (VF).

To get around this problem the bonding PMD is used.

The following sections show an example of how to do this.

Test Setup
----------

A bonded device is created in the VM.
The virtio and VF PMD's are added as slaves to the bonded device.
The VF is set as the primary slave of the bonded device.

A bridge must be set up on the Host connecting the tap device, which is the
backend of the Virtio device and the Physical Function (PF) device.

To test the Live Migration two servers with identical operating systems installed are used.
KVM and Qemu 2.3 is also required on the servers.

In this example, the servers have Niantic and or Fortville NIC's installed.
The NIC's on both servers are connected to a switch
which is also connected to the traffic generator.

The switch is configured to broadcast traffic on all the NIC ports.
A :ref:`Sample switch configuration <lm_bond_virtio_sriov_switch_conf>`
can be found in this section.

The host is running the Kernel PF driver (ixgbe or i40e).

The ip address of host_server_1 is 10.237.212.46

The ip address of host_server_2 is 10.237.212.131

.. _figure_lm_bond_virtio_sriov:

.. figure:: img/lm_bond_virtio_sriov.*

Live Migration steps
--------------------

The sample scripts mentioned in the steps below can be found in the
:ref:`Sample host scripts <lm_bond_virtio_sriov_host_scripts>` and
:ref:`Sample VM scripts <lm_bond_virtio_sriov_vm_scripts>` sections.

On host_server_1: Terminal 1
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: console

   cd /root/dpdk/host_scripts
   ./setup_vf_on_212_46.sh

For Fortville NIC

.. code-block:: console

   ./vm_virtio_vf_i40e_212_46.sh

For Niantic NIC

.. code-block:: console

   ./vm_virtio_vf_one_212_46.sh

On host_server_1: Terminal 2
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: console

   cd /root/dpdk/host_scripts
   ./setup_bridge_on_212_46.sh
   ./connect_to_qemu_mon_on_host.sh
   (qemu)

On host_server_1: Terminal 1
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**In VM on host_server_1:**

.. code-block:: console

   cd /root/dpdk/vm_scripts
   ./setup_dpdk_in_vm.sh
   ./run_testpmd_bonding_in_vm.sh

   testpmd> show port info all

The ``mac_addr`` command only works with kernel PF for Niantic

.. code-block:: console

   testpmd> mac_addr add port 1 vf 0 AA:BB:CC:DD:EE:FF

The syntax of the ``testpmd`` command is:

Create bonded device (mode) (socket).

Mode 1 is active backup.

Virtio is port 0 (P0).

VF is port 1 (P1).

Bonding is port 2 (P2).

.. code-block:: console

   testpmd> create bonded device 1 0
   Created new bonded device eth_bond_testpmd_0 on (port 2).
   testpmd> add bonding slave 0 2
   testpmd> add bonding slave 1 2
   testpmd> show bonding config 2

The syntax of the ``testpmd`` command is:

set bonding primary (slave id) (port id)

Set primary to P1 before starting bonding port.

.. code-block:: console

   testpmd> set bonding primary 1 2
   testpmd> show bonding config 2
   testpmd> port start 2
   Port 2: 02:09:C0:68:99:A5
   Checking link statuses...
   Port 0 Link Up - speed 10000 Mbps - full-duplex
   Port 1 Link Up - speed 10000 Mbps - full-duplex
   Port 2 Link Up - speed 10000 Mbps - full-duplex

   testpmd> show bonding config 2

Primary is now P1. There are 2 active slaves.

Use P2 only for forwarding.

.. code-block:: console

   testpmd> set portlist 2
   testpmd> show config fwd
   testpmd> set fwd mac
   testpmd> start
   testpmd> show bonding config 2

Primary is now P1. There are 2 active slaves.

.. code-block:: console

   testpmd> show port stats all

VF traffic is seen at P1 and P2.

.. code-block:: console

   testpmd> clear port stats all
   testpmd> set bonding primary 0 2
   testpmd> remove bonding slave 1 2
   testpmd> show bonding config 2

Primary is now P0. There is 1 active slave.

.. code-block:: console

   testpmd> clear port stats all
   testpmd> show port stats all

No VF traffic is seen at P0 and P2, VF MAC address still present.

.. code-block:: console

   testpmd> port stop 1
   testpmd> port close 1

Port close should remove VF MAC address, it does not remove perm_addr.

The ``mac_addr`` command only works with the kernel PF for Niantic.

.. code-block:: console

   testpmd> mac_addr remove 1 AA:BB:CC:DD:EE:FF
   testpmd> port detach 1
   Port '0000:00:04.0' is detached. Now total ports is 2
   testpmd> show port stats all

No VF traffic is seen at P0 and P2.

On host_server_1: Terminal 2
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: console

   (qemu) device_del vf1


On host_server_1: Terminal 1
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**In VM on host_server_1:**

.. code-block:: console

   testpmd> show bonding config 2

Primary is now P0. There is 1 active slave.

.. code-block:: console

   testpmd> show port info all
   testpmd> show port stats all

On host_server_2: Terminal 1
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: console

   cd /root/dpdk/host_scripts
   ./setup_vf_on_212_131.sh
   ./vm_virtio_one_migrate.sh

On host_server_2: Terminal 2
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: console

   ./setup_bridge_on_212_131.sh
   ./connect_to_qemu_mon_on_host.sh
   (qemu) info status
   VM status: paused (inmigrate)
   (qemu)

On host_server_1: Terminal 2
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Check that the switch is up before migrating.

.. code-block:: console

   (qemu) migrate tcp:10.237.212.131:5555
   (qemu) info status
   VM status: paused (postmigrate)

For the Niantic NIC.

.. code-block:: console

   (qemu) info migrate
   capabilities: xbzrle: off rdma-pin-all: off auto-converge: off zero-blocks: off
   Migration status: completed
   total time: 11834 milliseconds
   downtime: 18 milliseconds
   setup: 3 milliseconds
   transferred ram: 389137 kbytes
   throughput: 269.49 mbps
   remaining ram: 0 kbytes
   total ram: 1590088 kbytes
   duplicate: 301620 pages
   skipped: 0 pages
   normal: 96433 pages
   normal bytes: 385732 kbytes
   dirty sync count: 2
   (qemu) quit

For the Fortville NIC.

.. code-block:: console

   (qemu) info migrate
   capabilities: xbzrle: off rdma-pin-all: off auto-converge: off zero-blocks: off
   Migration status: completed
   total time: 11619 milliseconds
   downtime: 5 milliseconds
   setup: 7 milliseconds
   transferred ram: 379699 kbytes
   throughput: 267.82 mbps
   remaining ram: 0 kbytes
   total ram: 1590088 kbytes
   duplicate: 303985 pages
   skipped: 0 pages
   normal: 94073 pages
   normal bytes: 376292 kbytes
   dirty sync count: 2
   (qemu) quit

On host_server_2: Terminal 1
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**In VM on host_server_2:**

   Hit Enter key. This brings the user to the testpmd prompt.

.. code-block:: console

   testpmd>

On host_server_2: Terminal 2
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: console

   (qemu) info status
   VM status: running

For the Niantic NIC.

.. code-block:: console

   (qemu) device_add pci-assign,host=06:10.0,id=vf1

For the Fortville NIC.

.. code-block:: console

   (qemu) device_add pci-assign,host=03:02.0,id=vf1

On host_server_2: Terminal 1
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**In VM on host_server_2:**

.. code-block:: console

   testomd> show port info all
   testpmd> show port stats all
   testpmd> show bonding config 2
   testpmd> port attach 0000:00:04.0
   Port 1 is attached.
   Now total ports is 3
   Done

   testpmd> port start 1

The ``mac_addr`` command only works with the Kernel PF for Niantic.

.. code-block:: console

   testpmd> mac_addr add port 1 vf 0 AA:BB:CC:DD:EE:FF
   testpmd> show port stats all.
   testpmd> show config fwd
   testpmd> show bonding config 2
   testpmd> add bonding slave 1 2
   testpmd> set bonding primary 1 2
   testpmd> show bonding config 2
   testpmd> show port stats all

VF traffic is seen at P1 (VF) and P2 (Bonded device).

.. code-block:: console

   testpmd> remove bonding slave 0 2
   testpmd> show bonding config 2
   testpmd> port stop 0
   testpmd> port close 0
   testpmd> port detach 0
   Port '0000:00:03.0' is detached. Now total ports is 2

   testpmd> show port info all
   testpmd> show config fwd
   testpmd> show port stats all

VF traffic is seen at P1 (VF) and P2 (Bonded device).

.. _lm_bond_virtio_sriov_host_scripts:

Sample host scripts
-------------------

setup_vf_on_212_46.sh
~~~~~~~~~~~~~~~~~~~~~
Set up Virtual Functions on host_server_1

.. code-block:: sh

   #!/bin/sh
   # This script is run on the host 10.237.212.46 to setup the VF

   # set up Niantic VF
   cat /sys/bus/pci/devices/0000\:09\:00.0/sriov_numvfs
   echo 1 > /sys/bus/pci/devices/0000\:09\:00.0/sriov_numvfs
   cat /sys/bus/pci/devices/0000\:09\:00.0/sriov_numvfs
   rmmod ixgbevf

   # set up Fortville VF
   cat /sys/bus/pci/devices/0000\:02\:00.0/sriov_numvfs
   echo 1 > /sys/bus/pci/devices/0000\:02\:00.0/sriov_numvfs
   cat /sys/bus/pci/devices/0000\:02\:00.0/sriov_numvfs
   rmmod i40evf

vm_virtio_vf_one_212_46.sh
~~~~~~~~~~~~~~~~~~~~~~~~~~

Setup Virtual Machine on host_server_1

.. code-block:: sh

   #!/bin/sh

   # Path to KVM tool
   KVM_PATH="/usr/bin/qemu-system-x86_64"

   # Guest Disk image
   DISK_IMG="/home/username/disk_image/virt1_sml.disk"

   # Number of guest cpus
   VCPUS_NR="4"

   # Memory
   MEM=1536

   taskset -c 1-5 $KVM_PATH \
    -enable-kvm \
    -m $MEM \
    -smp $VCPUS_NR \
    -cpu host \
    -name VM1 \
    -no-reboot \
    -net none \
    -vnc none -nographic \
    -hda $DISK_IMG \
    -netdev type=tap,id=net1,script=no,downscript=no,ifname=tap1 \
    -device virtio-net-pci,netdev=net1,mac=CC:BB:BB:BB:BB:BB \
    -device pci-assign,host=09:10.0,id=vf1 \
    -monitor telnet::3333,server,nowait

setup_bridge_on_212_46.sh
~~~~~~~~~~~~~~~~~~~~~~~~~

Setup bridge on host_server_1

.. code-block:: sh

   #!/bin/sh
   # This script is run on the host 10.237.212.46 to setup the bridge
   # for the Tap device and the PF device.
   # This enables traffic to go from the PF to the Tap to the Virtio PMD in the VM.

   # ens3f0 is the Niantic NIC
   # ens6f0 is the Fortville NIC

   ifconfig ens3f0 down
   ifconfig tap1 down
   ifconfig ens6f0 down
   ifconfig virbr0 down

   brctl show virbr0
   brctl addif virbr0 ens3f0
   brctl addif virbr0 ens6f0
   brctl addif virbr0 tap1
   brctl show virbr0

   ifconfig ens3f0 up
   ifconfig tap1 up
   ifconfig ens6f0 up
   ifconfig virbr0 up

connect_to_qemu_mon_on_host.sh
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: sh

   #!/bin/sh
   # This script is run on both hosts when the VM is up,
   # to connect to the Qemu Monitor.

   telnet 0 3333

setup_vf_on_212_131.sh
~~~~~~~~~~~~~~~~~~~~~~

Set up Virtual Functions on host_server_2

.. code-block:: sh

   #!/bin/sh
   # This script is run on the host 10.237.212.131 to setup the VF

   # set up Niantic VF
   cat /sys/bus/pci/devices/0000\:06\:00.0/sriov_numvfs
   echo 1 > /sys/bus/pci/devices/0000\:06\:00.0/sriov_numvfs
   cat /sys/bus/pci/devices/0000\:06\:00.0/sriov_numvfs
   rmmod ixgbevf

   # set up Fortville VF
   cat /sys/bus/pci/devices/0000\:03\:00.0/sriov_numvfs
   echo 1 > /sys/bus/pci/devices/0000\:03\:00.0/sriov_numvfs
   cat /sys/bus/pci/devices/0000\:03\:00.0/sriov_numvfs
   rmmod i40evf

vm_virtio_one_migrate.sh
~~~~~~~~~~~~~~~~~~~~~~~~

Setup Virtual Machine on host_server_2

.. code-block:: sh

   #!/bin/sh
   # Start the VM on host_server_2 with the same parameters except without the VF
   # parameters, as the VM on host_server_1, in migration-listen mode
   # (-incoming tcp:0:5555)

   # Path to KVM tool
   KVM_PATH="/usr/bin/qemu-system-x86_64"

   # Guest Disk image
   DISK_IMG="/home/username/disk_image/virt1_sml.disk"

   # Number of guest cpus
   VCPUS_NR="4"

   # Memory
   MEM=1536

   taskset -c 1-5 $KVM_PATH \
    -enable-kvm \
    -m $MEM \
    -smp $VCPUS_NR \
    -cpu host \
    -name VM1 \
    -no-reboot \
    -net none \
    -vnc none -nographic \
    -hda $DISK_IMG \
    -netdev type=tap,id=net1,script=no,downscript=no,ifname=tap1 \
    -device virtio-net-pci,netdev=net1,mac=CC:BB:BB:BB:BB:BB \
    -incoming tcp:0:5555 \
    -monitor telnet::3333,server,nowait

setup_bridge_on_212_131.sh
~~~~~~~~~~~~~~~~~~~~~~~~~~

Setup bridge on host_server_2

.. code-block:: sh

   #!/bin/sh
   # This script is run on the host to setup the bridge
   # for the Tap device and the PF device.
   # This enables traffic to go from the PF to the Tap to the Virtio PMD in the VM.

   # ens4f0 is the Niantic NIC
   # ens5f0 is the Fortville NIC

   ifconfig ens4f0 down
   ifconfig tap1 down
   ifconfig ens5f0 down
   ifconfig virbr0 down

   brctl show virbr0
   brctl addif virbr0 ens4f0
   brctl addif virbr0 ens5f0
   brctl addif virbr0 tap1
   brctl show virbr0

   ifconfig ens4f0 up
   ifconfig tap1 up
   ifconfig ens5f0 up
   ifconfig virbr0 up

.. _lm_bond_virtio_sriov_vm_scripts:

Sample VM scripts
-----------------

setup_dpdk_in_vm.sh
~~~~~~~~~~~~~~~~~~~

Set up DPDK in the Virtual Machine

.. code-block:: sh

   #!/bin/sh
   # this script matches the vm_virtio_vf_one script
   # virtio port is 03
   # vf port is 04

   cat  /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
   echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
   cat  /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

   ifconfig -a
   /root/dpdk/tools/dpdk_nic_bind.py --status

   rmmod virtio-pci ixgbevf

   modprobe uio
   insmod /root/dpdk/x86_64-default-linuxapp-gcc/kmod/igb_uio.ko

   /root/dpdk/tools/dpdk_nic_bind.py -b igb_uio 0000:00:03.0
   /root/dpdk/tools/dpdk_nic_bind.py -b igb_uio 0000:00:04.0

   /root/dpdk/tools/dpdk_nic_bind.py --status

run_testpmd_bonding_in_vm.sh
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Run testpmd in the Virtual Machine.

.. code-block:: sh

   #!/bin/sh
   # Run testpmd in the VM

   # The test system has 8 cpus (0-7), use cpus 2-7 for VM
   # Use taskset -pc <core number> <thread_id>

   # use for bonding of virtio and vf tests in VM

   /root/dpdk/x86_64-default-linuxapp-gcc/app/testpmd \
   -c f -n 4 --socket-mem 350 --  --i --port-topology=chained

.. _lm_bond_virtio_sriov_switch_conf:

Sample switch configuration
---------------------------

The Intel switch is used to connect the traffic generator to the
NIC's on host_server_1 and host_server_2.

In order to run the switch configuration two console windows are required.

Log in as root in both windows.

TestPointShared, run_switch.sh and load /root/switch_config must be executed
in the sequence below.

On Switch: Terminal 1
~~~~~~~~~~~~~~~~~~~~~

run TestPointShared

.. code-block:: console

   /usr/bin/TestPointShared

On Switch: Terminal 2
~~~~~~~~~~~~~~~~~~~~~

execute run_switch.sh

.. code-block:: console

   /root/run_switch.sh

On Switch: Terminal 1
~~~~~~~~~~~~~~~~~~~~~

load switch configuration

.. code-block:: console

   load /root/switch_config

Sample switch configuration script
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``/root/switch_config`` script:

.. code-block:: sh

   # TestPoint History
   show port 1,5,9,13,17,21,25
   set port 1,5,9,13,17,21,25 up
   show port 1,5,9,13,17,21,25
   del acl 1
   create acl 1
   create acl-port-set
   create acl-port-set
   add port port-set 1 0
   add port port-set 5,9,13,17,21,25 1
   create acl-rule 1 1
   add acl-rule condition 1 1 port-set 1
   add acl-rule action 1 1 redirect 1
   apply acl
   create vlan 1000
   add vlan port 1000 1,5,9,13,17,21,25
   set vlan tagging 1000 1,5,9,13,17,21,25 tag
   set switch config flood_ucast fwd
   show port stats all 1,5,9,13,17,21,25
