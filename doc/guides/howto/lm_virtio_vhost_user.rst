..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016 Intel Corporation.

Live Migration of VM with Virtio on host running vhost_user
===========================================================

Overview
--------

Live Migration of a VM with DPDK Virtio PMD on a host which is
running the Vhost sample application (vhost-switch) and using the DPDK PMD (ixgbe or i40e).

The Vhost sample application uses VMDQ so SRIOV must be disabled on the NIC's.

The following sections show an example of how to do this migration.

Test Setup
----------

To test the Live Migration two servers with identical operating systems installed are used.
KVM and QEMU is also required on the servers.

QEMU 2.5 is required for Live Migration of a VM with vhost_user running on the hosts.

In this example, the servers have Niantic and or Fortville NIC's installed.
The NIC's on both servers are connected to a switch
which is also connected to the traffic generator.

The switch is configured to broadcast traffic on all the NIC ports.

The ip address of host_server_1 is 10.237.212.46

The ip address of host_server_2 is 10.237.212.131

.. _figure_lm_vhost_user:

.. figure:: img/lm_vhost_user.*

Live Migration steps
--------------------

The sample scripts mentioned in the steps below can be found in the
:ref:`Sample host scripts <lm_virtio_vhost_user_host_scripts>` and
:ref:`Sample VM scripts <lm_virtio_vhost_user_vm_scripts>` sections.

On host_server_1: Terminal 1
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Setup DPDK on host_server_1

.. code-block:: console

   cd /root/dpdk/host_scripts
   ./setup_dpdk_on_host.sh

On host_server_1: Terminal 2
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Bind the Niantic or Fortville NIC to igb_uio on host_server_1.

For Fortville NIC.

.. code-block:: console

   cd /root/dpdk/usertools
   ./dpdk-devbind.py -b igb_uio 0000:02:00.0

For Niantic NIC.

.. code-block:: console

   cd /root/dpdk/usertools
   ./dpdk-devbind.py -b igb_uio 0000:09:00.0

On host_server_1: Terminal 3
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For Fortville and Niantic NIC's reset SRIOV and run the
vhost_user sample application (vhost-switch) on host_server_1.

.. code-block:: console

   cd /root/dpdk/host_scripts
   ./reset_vf_on_212_46.sh
   ./run_vhost_switch_on_host.sh

On host_server_1: Terminal 1
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Start the VM on host_server_1

.. code-block:: console

   ./vm_virtio_vhost_user.sh

On host_server_1: Terminal 4
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Connect to the QEMU monitor on host_server_1.

.. code-block:: console

   cd /root/dpdk/host_scripts
   ./connect_to_qemu_mon_on_host.sh
   (qemu)

On host_server_1: Terminal 1
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**In VM on host_server_1:**

Setup DPDK in the VM and run testpmd in the VM.

.. code-block:: console

   cd /root/dpdk/vm_scripts
   ./setup_dpdk_in_vm.sh
   ./run_testpmd_in_vm.sh

   testpmd> show port info all
   testpmd> set fwd mac retry
   testpmd> start tx_first
   testpmd> show port stats all

Virtio traffic is seen at P1 and P2.

On host_server_2: Terminal 1
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set up DPDK on the host_server_2.

.. code-block:: console

   cd /root/dpdk/host_scripts
   ./setup_dpdk_on_host.sh

On host_server_2: Terminal 2
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Bind the Niantic or Fortville NIC to igb_uio on host_server_2.

For Fortville NIC.

.. code-block:: console

   cd /root/dpdk/usertools
   ./dpdk-devbind.py -b igb_uio 0000:03:00.0

For Niantic NIC.

.. code-block:: console

   cd /root/dpdk/usertools
   ./dpdk-devbind.py -b igb_uio 0000:06:00.0

On host_server_2: Terminal 3
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For Fortville and Niantic NIC's reset SRIOV, and run
the vhost_user sample application on host_server_2.

.. code-block:: console

   cd /root/dpdk/host_scripts
   ./reset_vf_on_212_131.sh
   ./run_vhost_switch_on_host.sh

On host_server_2: Terminal 1
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Start the VM on host_server_2.

.. code-block:: console

   ./vm_virtio_vhost_user_migrate.sh

On host_server_2: Terminal 4
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Connect to the QEMU monitor on host_server_2.

.. code-block:: console

   cd /root/dpdk/host_scripts
   ./connect_to_qemu_mon_on_host.sh
   (qemu) info status
   VM status: paused (inmigrate)
   (qemu)

On host_server_1: Terminal 4
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Check that switch is up before migrating the VM.

.. code-block:: console

   (qemu) migrate tcp:10.237.212.131:5555
   (qemu) info status
   VM status: paused (postmigrate)

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

On host_server_2: Terminal 4
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**In QEMU monitor on host_server_2**

.. code-block:: console

   (qemu) info status
   VM status: running

On host_server_2: Terminal 1
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**In VM on host_server_2:**

.. code-block:: console

   testomd> show port info all
   testpmd> show port stats all

Virtio traffic is seen at P0 and P1.


.. _lm_virtio_vhost_user_host_scripts:

Sample host scripts
-------------------

reset_vf_on_212_46.sh
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: sh

   #!/bin/sh
   # This script is run on the host 10.237.212.46 to reset SRIOV

   # BDF for Fortville NIC is 0000:02:00.0
   cat /sys/bus/pci/devices/0000\:02\:00.0/max_vfs
   echo 0 > /sys/bus/pci/devices/0000\:02\:00.0/max_vfs
   cat /sys/bus/pci/devices/0000\:02\:00.0/max_vfs

   # BDF for Niantic NIC is 0000:09:00.0
   cat /sys/bus/pci/devices/0000\:09\:00.0/max_vfs
   echo 0 > /sys/bus/pci/devices/0000\:09\:00.0/max_vfs
   cat /sys/bus/pci/devices/0000\:09\:00.0/max_vfs

vm_virtio_vhost_user.sh
~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: sh

   #/bin/sh
   # Script for use with vhost_user sample application
   # The host system has 8 cpu's (0-7)

   # Path to KVM tool
   KVM_PATH="/usr/bin/qemu-system-x86_64"

   # Guest Disk image
   DISK_IMG="/home/user/disk_image/virt1_sml.disk"

   # Number of guest cpus
   VCPUS_NR="6"

   # Memory
   MEM=1024

   VIRTIO_OPTIONS="csum=off,gso=off,guest_tso4=off,guest_tso6=off,guest_ecn=off"

   # Socket Path
   SOCKET_PATH="/root/dpdk/host_scripts/usvhost"

   taskset -c 2-7 $KVM_PATH \
    -enable-kvm \
    -m $MEM \
    -smp $VCPUS_NR \
    -object memory-backend-file,id=mem,size=1024M,mem-path=/mnt/huge,share=on \
    -numa node,memdev=mem,nodeid=0 \
    -cpu host \
    -name VM1 \
    -no-reboot \
    -net none \
    -vnc none \
    -nographic \
    -hda $DISK_IMG \
    -chardev socket,id=chr0,path=$SOCKET_PATH \
    -netdev type=vhost-user,id=net1,chardev=chr0,vhostforce \
    -device virtio-net-pci,netdev=net1,mac=CC:BB:BB:BB:BB:BB,$VIRTIO_OPTIONS \
    -chardev socket,id=chr1,path=$SOCKET_PATH \
    -netdev type=vhost-user,id=net2,chardev=chr1,vhostforce \
    -device virtio-net-pci,netdev=net2,mac=DD:BB:BB:BB:BB:BB,$VIRTIO_OPTIONS \
    -monitor telnet::3333,server,nowait

connect_to_qemu_mon_on_host.sh
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: sh

   #!/bin/sh
   # This script is run on both hosts when the VM is up,
   # to connect to the Qemu Monitor.

   telnet 0 3333

reset_vf_on_212_131.sh
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: sh

   #!/bin/sh
   # This script is run on the host 10.237.212.131 to reset SRIOV

   # BDF for Ninatic NIC is 0000:06:00.0
   cat /sys/bus/pci/devices/0000\:06\:00.0/max_vfs
   echo 0 > /sys/bus/pci/devices/0000\:06\:00.0/max_vfs
   cat /sys/bus/pci/devices/0000\:06\:00.0/max_vfs

   # BDF for Fortville NIC is 0000:03:00.0
   cat /sys/bus/pci/devices/0000\:03\:00.0/max_vfs
   echo 0 > /sys/bus/pci/devices/0000\:03\:00.0/max_vfs
   cat /sys/bus/pci/devices/0000\:03\:00.0/max_vfs

vm_virtio_vhost_user_migrate.sh
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: sh

   #/bin/sh
   # Script for use with vhost user sample application
   # The host system has 8 cpu's (0-7)

   # Path to KVM tool
   KVM_PATH="/usr/bin/qemu-system-x86_64"

   # Guest Disk image
   DISK_IMG="/home/user/disk_image/virt1_sml.disk"

   # Number of guest cpus
   VCPUS_NR="6"

   # Memory
   MEM=1024

   VIRTIO_OPTIONS="csum=off,gso=off,guest_tso4=off,guest_tso6=off,guest_ecn=off"

   # Socket Path
   SOCKET_PATH="/root/dpdk/host_scripts/usvhost"

   taskset -c 2-7 $KVM_PATH \
    -enable-kvm \
    -m $MEM \
    -smp $VCPUS_NR \
    -object memory-backend-file,id=mem,size=1024M,mem-path=/mnt/huge,share=on \
    -numa node,memdev=mem,nodeid=0 \
    -cpu host \
    -name VM1 \
    -no-reboot \
    -net none \
    -vnc none \
    -nographic \
    -hda $DISK_IMG \
    -chardev socket,id=chr0,path=$SOCKET_PATH \
    -netdev type=vhost-user,id=net1,chardev=chr0,vhostforce \
    -device virtio-net-pci,netdev=net1,mac=CC:BB:BB:BB:BB:BB,$VIRTIO_OPTIONS \
    -chardev socket,id=chr1,path=$SOCKET_PATH \
    -netdev type=vhost-user,id=net2,chardev=chr1,vhostforce \
    -device virtio-net-pci,netdev=net2,mac=DD:BB:BB:BB:BB:BB,$VIRTIO_OPTIONS \
    -incoming tcp:0:5555 \
    -monitor telnet::3333,server,nowait

.. _lm_virtio_vhost_user_vm_scripts:

Sample VM scripts
-----------------

setup_dpdk_virtio_in_vm.sh
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: sh

   #!/bin/sh
   # this script matches the vm_virtio_vhost_user script
   # virtio port is 03
   # virtio port is 04

   cat  /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
   echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
   cat  /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

   ifconfig -a
   /root/dpdk/usertools/dpdk-devbind.py --status

   rmmod virtio-pci

   modprobe uio
   insmod /root/dpdk/x86_64-default-linuxapp-gcc/kmod/igb_uio.ko

   /root/dpdk/usertools/dpdk-devbind.py -b igb_uio 0000:00:03.0
   /root/dpdk/usertools/dpdk-devbind.py -b igb_uio 0000:00:04.0

   /root/dpdk/usertools/dpdk-devbind.py --status

run_testpmd_in_vm.sh
~~~~~~~~~~~~~~~~~~~~

.. code-block:: sh

   #!/bin/sh
   # Run testpmd for use with vhost_user sample app.
   # test system has 8 cpus (0-7), use cpus 2-7 for VM

   /root/dpdk/x86_64-default-linuxapp-gcc/app/testpmd \
   -l 0-5 -n 4 --socket-mem 350 -- --burst=64 --i
