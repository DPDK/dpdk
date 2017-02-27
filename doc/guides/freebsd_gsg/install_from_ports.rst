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

.. _install_from_ports:

Installing DPDK from the Ports Collection
=========================================

The easiest way to get up and running with the DPDK on FreeBSD is to
install it from the ports collection. Details of getting and using the ports
collection are documented in the
`FreeBSD Handbook <http://www.freebsd.org/doc/en_US.ISO8859-1/books/handbook/index.html>`_.

.. note::

    Testing has been performed using FreeBSD 10.0-RELEASE (x86_64) and requires the
    installation of the kernel sources, which should be included during the
    installation of FreeBSD.

Installing the DPDK FreeBSD Port
--------------------------------

On a system with the ports collection installed in ``/usr/ports``, the DPDK
can be installed using the commands:

.. code-block:: console

    cd /usr/ports/net/dpdk

    make install

After the installation of the DPDK port, instructions will be printed on
how to install the kernel modules required to use the DPDK. A more
complete version of these instructions can be found in the sections
:ref:`loading_contigmem` and :ref:`loading_nic_uio`. Normally, lines like
those below would be added to the file ``/boot/loader.conf``.

.. code-block:: console

    # Reserve 2 x 1G blocks of contiguous memory using contigmem driver:
    hw.contigmem.num_buffers=2
    hw.contigmem.buffer_size=1073741824
    contigmem_load="YES"

    # Identify NIC devices for DPDK apps to use and load nic_uio driver:
    hw.nic_uio.bdfs="2:0:0,2:0:1"
    nic_uio_load="YES"

Compiling and Running the Example Applications
----------------------------------------------

When the DPDK has been installed from the ports collection it installs
its example applications in ``/usr/local/share/dpdk/examples`` - also accessible via
symlink as ``/usr/local/share/examples/dpdk``. These examples can be compiled and
run as described in :ref:`compiling_sample_apps`. In this case, the required
environmental variables should be set as below:

* ``RTE_SDK=/usr/local/share/dpdk``

* ``RTE_TARGET=x86_64-native-bsdapp-clang``

.. note::

   To install a copy of the DPDK compiled using gcc, please download the
   official DPDK package from http://dpdk.org/ and install manually using
   the instructions given in the next chapter, :ref:`building_from_source`

An example application can therefore be copied to a user's home directory and
compiled and run as below:

.. code-block:: console

    export RTE_SDK=/usr/local/share/dpdk

    export RTE_TARGET=x86_64-native-bsdapp-clang

    cp -r /usr/local/share/dpdk/examples/helloworld .

    cd helloworld/

    gmake
      CC main.o
      LD helloworld
      INSTALL-APP helloworld
      INSTALL-MAP helloworld.map

    sudo ./build/helloworld -l 0-3 -n 2

    EAL: Contigmem driver has 2 buffers, each of size 1GB
    EAL: Sysctl reports 8 cpus
    EAL: Detected lcore 0
    EAL: Detected lcore 1
    EAL: Detected lcore 2
    EAL: Detected lcore 3
    EAL: Support maximum 64 logical core(s) by configuration.
    EAL: Detected 4 lcore(s)
    EAL: Setting up physically contiguous memory...
    EAL: Mapped memory segment 1 @ 0x802400000: len 1073741824
    EAL: Mapped memory segment 2 @ 0x842400000: len 1073741824
    EAL: WARNING: clock_gettime cannot use CLOCK_MONOTONIC_RAW and HPET
         is not available - clock timings may be less accurate.
    EAL: TSC frequency is ~3569023 KHz
    EAL: PCI scan found 24 devices
    EAL: Master core 0 is ready (tid=0x802006400)
    EAL: Core 1 is ready (tid=0x802006800)
    EAL: Core 3 is ready (tid=0x802007000)
    EAL: Core 2 is ready (tid=0x802006c00)
    EAL: PCI device 0000:01:00.0 on NUMA socket 0
    EAL:   probe driver: 8086:10fb rte_ixgbe_pmd
    EAL:   PCI memory mapped at 0x80074a000
    EAL:   PCI memory mapped at 0x8007ca000
    EAL: PCI device 0000:01:00.1 on NUMA socket 0
    EAL:   probe driver: 8086:10fb rte_ixgbe_pmd
    EAL:   PCI memory mapped at 0x8007ce000
    EAL:   PCI memory mapped at 0x80084e000
    EAL: PCI device 0000:02:00.0 on NUMA socket 0
    EAL:   probe driver: 8086:10fb rte_ixgbe_pmd
    EAL:   PCI memory mapped at 0x800852000
    EAL:   PCI memory mapped at 0x8008d2000
    EAL: PCI device 0000:02:00.1 on NUMA socket 0
    EAL:   probe driver: 8086:10fb rte_ixgbe_pmd
    EAL:   PCI memory mapped at 0x801b3f000
    EAL:   PCI memory mapped at 0x8008d6000
    hello from core 1
    hello from core 2
    hello from core 3
    hello from core 0

.. note::

   To run a DPDK process as a non-root user, adjust the permissions on
   the ``/dev/contigmem`` and ``/dev/uio device`` nodes as described in section
   :ref:`running_non_root`

.. note::

   For an explanation of the command-line parameters that can be passed to an
   DPDK application, see section :ref:`running_sample_app`.
