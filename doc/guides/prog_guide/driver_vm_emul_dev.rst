..  BSD LICENSE
    Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

Driver for VM Emulated Devices
==============================

The DPDK EM poll mode driver supports the following emulated devices:

*   qemu-kvm emulated Intel® 82540EM Gigabit Ethernet Controller (qemu e1000 device)

*   VMware* emulated Intel® 82545EM Gigabit Ethernet Controller

*   VMware emulated Intel® 8274L Gigabit Ethernet Controller.

Validated Hypervisors
---------------------

The validated hypervisors are:

*   KVM (Kernel Virtual Machine) with Qemu, version 0.14.0

*   KVM (Kernel Virtual Machine) with Qemu, version 0.15.1

*   VMware ESXi 5.0, Update 1

Recommended Guest Operating System in Virtual Machine
-----------------------------------------------------

The recommended guest operating system in a virtualized environment is:

*   Fedora* 18 (64-bit)

For supported kernel versions, refer to the *DPDK Release Notes*.

Setting Up a KVM Virtual Machine
--------------------------------

The following describes a target environment:

*   Host Operating System: Fedora 14

*   Hypervisor: KVM (Kernel Virtual Machine) with Qemu version, 0.14.0

*   Guest Operating System: Fedora 14

*   Linux Kernel Version: Refer to the DPDK Getting Started Guide

*   Target Applications: testpmd

The setup procedure is as follows:

#.  Download qemu-kvm-0.14.0 from
    `http://sourceforge.net/projects/kvm/files/qemu-kvm/ <http://sourceforge.net/projects/kvm/files/qemu-kvm/>`_
    and install it in the Host OS using the following steps:

    When using a recent kernel (2.6.25+) with kvm modules included:

    .. code-block:: console

        tar xzf qemu-kvm-release.tar.gz cd qemu-kvm-release
        ./configure --prefix=/usr/local/kvm
        make
        sudo make install
        sudo /sbin/modprobe kvm-intel

    When using an older kernel or a kernel from a distribution without the kvm modules,
    you must download (from the same link), compile and install the modules yourself:

    .. code-block:: console

        tar xjf kvm-kmod-release.tar.bz2
        cd kvm-kmod-release
        ./configure
        make
        sudo make install
        sudo /sbin/modprobe kvm-intel

    Note that qemu-kvm installs in the /usr/local/bin directory.

    For more details about KVM configuration and usage, please refer to:
    `http://www.linux-kvm.org/page/HOWTO1 <http://www.linux-kvm.org/page/HOWTO1>`_.

#.  Create a Virtual Machine and install Fedora 14 on the Virtual Machine.
    This is referred to as the Guest Operating System (Guest OS).

#.  Start the Virtual Machine with at least one emulated e1000 device.

    .. note::

        The Qemu provides several choices for the emulated network device backend.
        Most commonly used is a TAP networking backend that uses a TAP networking device in the host.
        For more information about Qemu supported networking backends and different options for configuring networking at Qemu,
        please refer to:

        — `http://www.linux-kvm.org/page/Networking <http://www.linux-kvm.org/page/Networking>`_

        — `http://wiki.qemu.org/Documentation/Networking <http://wiki.qemu.org/Documentation/Networking>`_

        — `http://qemu.weilnetz.de/qemu-doc.html <http://qemu.weilnetz.de/qemu-doc.html>`_

        For example, to start a VM with two emulated e1000 devices, issue the following command:

        .. code-block:: console

            /usr/local/kvm/bin/qemu-system-x86_64 -cpu host -smp 4 -hda qemu1.raw -m 1024
            -net nic,model=e1000,vlan=1,macaddr=DE:AD:1E:00:00:01
            -net tap,vlan=1,ifname=tapvm01,script=no,downscript=no
            -net nic,model=e1000,vlan=2,macaddr=DE:AD:1E:00:00:02
            -net tap,vlan=2,ifname=tapvm02,script=no,downscript=no

        where:

        — -m = memory to assign

        — -smp = number of smp cores

        — -hda = virtual disk image

        This command starts a new virtual machine with two emulated 82540EM devices,
        backed up with two TAP networking host interfaces, tapvm01 and tapvm02.

        .. code-block:: console

            # ip tuntap show
            tapvm01: tap
            tapvm02: tap

#.  Configure your TAP networking interfaces using ip/ifconfig tools.

#.  Log in to the guest OS and check that the expected emulated devices exist:

    .. code-block:: console

        # lspci -d 8086:100e
        00:04.0 Ethernet controller: Intel Corporation 82540EM Gigabit Ethernet Controller (rev 03)
        00:05.0 Ethernet controller: Intel Corporation 82540EM Gigabit Ethernet Controller (rev 03)

#.  Install the DPDK and run testpmd.

Known Limitations of Emulated Devices
-------------------------------------

The following are known limitations:

#.  The Qemu e1000 RX path does not support multiple descriptors/buffers per packet.
    Therefore, rte_mbuf should be big enough to hold the whole packet.
    For example, to allow testpmd to receive jumbo frames, use the following:

    testpmd [options] -- --mbuf-size=<your-max-packet-size>

#.  Qemu e1000 does not validate the checksum of incoming packets.
