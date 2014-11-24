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

Compiling the Intel® DPDK Target from Source
============================================

Install the Intel® DPDK and Browse Sources
------------------------------------------

First, uncompress the archive and move to the Intel® DPDK source directory:

.. code-block:: console

    user@host:~ # unzip DPDK-<version>zip
    user@host:~ # cd DPDK-<version>
    user@host:~/DPDK # ls
    app/ config/ examples/ lib/ LICENSE.GPL LICENSE.LGPL Makefile mk/ scripts/ tools/

The Intel® DPDK is composed of several directories:

*   lib: Source code of Intel® DPDK libraries

*   app: Source code of Intel® DPDK applications (automatic tests)

*   examples: Source code of Intel® DPDK applications

*   config, tools, scripts, mk: Framework-related makefiles, scripts and configuration

Installation of the Intel® DPDK Target Environments
---------------------------------------------------

The format of an Intel® DPDK target is:

ARCH-MACHINE-EXECENV-TOOLCHAIN

Where:

*   ARCH is:   x86_64

*   MACHINE is: native

*   EXECENV is: bsdapp

*   TOOLCHAIN is: gcc

The configuration files for the Intel® DPDK targets can be found in the DPDK/config
directory in the form of:

::

    defconfig_ARCH-MACHINE-EXECENV-TOOLCHAIN

.. note::

    Configuration files are provided with the RTE_MACHINE optimization level set.
    Within the configuration files, the RTE_MACHINE configuration value is set
    to native, which means that the compiled software is tuned for the platform
    on which it is built.  For more information on this setting, and its
    possible values, see the *Intel® DPDK Programmers Guide*.

To install and make the target, use gmake install T=<target> CC=gcc48.

For example to compile for FreeBSD* use:

.. code-block:: console

    gmake install T=x86_64-native-bsdapp-gcc CC=gcc48

To prepare a target without building it, for example, if the configuration
changes need to be made before compilation, use the gmake config T=<target> command:

.. code-block:: console

    gmake config T=x86_64-native-bsdapp-gcc CC=gcc48

To build after configuration, change directory to ./x86_64-native-bsdapp-gcc and use:

.. code-block:: console

    gmake CC=gcc48

Browsing the Installed Intel® DPDK Environment Target
-----------------------------------------------------

Once a target is created, it contains all the libraries and header files for the
Intel® DPDK environment that are required to build customer applications.
In addition, the test and testpmd applications are built under the build/app
directory, which may be used for testing.  A kmod directory is also present that
contains the kernel modules to install:

.. code-block:: console

    user@host:~/DPDK # ls x86_64-native-bsdapp-gcc
    app   build    hostapp    include    kmod    lib    Makefile

Loading the Intel® DPDK contigmem Module
----------------------------------------

To run any Intel® DPDK application, the contigmem module must be loaded into the
running kernel. The module is found in the kmod sub-directory of the Intel® DPDK
target directory. The module can be loaded using kldload (assuming that the
current directory is the Intel® DPDK target directory):

.. code-block:: console

    kldload ./kmod/contigmem.ko

It is advisable to include the loading of the contigmem module during the boot
process to avoid issues with potential memory fragmentation during later system
up time.  This can be achieved by copying the module to the /boot/kernel/
directory and placing the following into /boot/loader.conf:

::

    contigmem_load="YES"

.. note::

    The contigmem_load directive should be placed after any definitions of
    hw.contigmem.num_buffers and hw.contigmem.buffer_size if the default values
    are not to be used.

An error such as kldload: can't load ./x86_64-native-bsdapp-gcc/kmod/contigmem.ko:
Exec format error, is generally attributed to not having enough contiguous memory
available and can be verified via dmesg or /var/log/messages:

.. code-block:: console

    kernel: contigmalloc failed for buffer <n>

To avoid this error, reduce the number of buffers or the buffer size.

Loading the Intel® DPDK nic_uio Module
--------------------------------------

After loading the contigmem module, the nic_uio must also be loaded into the
running kernel prior to running any Intel® DPDK application.  This module must
be loaded using the kldload command as shown below (assuming that the current
directory is the Intel® DPDK target directory).

.. code-block:: console

    kldload ./kmod/nic_uio.ko

.. note::

    Currently loaded modules can be seen by using the kldstat command.  A module
    can be removed from the running kernel by using kldunload <module_name>.
    While the nic_uio module can be loaded during boot, the module load order
    cannot be guaranteed and in the case where only some ports are bound to
    nic_uio and others remain in use by the original driver, it is necessary to
    load nic_uio after booting into the kernel, specifically after the original
    driver has been loaded.

To load the module during boot, copy the nic_uio module to /boot/kernel and place the following into /boot/loader.conf:

::

    nic_uio_load="YES"

.. note::

    nic_uio_load="YES" must appear after the contigmem_load directive, if it exists.

Binding Network Ports to the nic_uio Module
-------------------------------------------

By default, the nic_uio module will take ownership of network ports if they are
recognized Intel® DPDK devices and are not owned by another module.

Device ownership can be viewed using the pciconf -l command.

The example below shows four Intel® 82599 network ports under if_ixgbe module ownership.

.. code-block:: console

    user@host:~ # pciconf -l
    ix0@pci0:1:0:0: class=0x020000 card=0x00038086 chip=0x10fb8086 rev=0x01 hdr=0x00
    ix1@pci0:1:0:1: class=0x020000 card=0x00038086 chip=0x10fb8086 rev=0x01 hdr=0x00
    ix2@pci0:2:0:0: class=0x020000 card=0x00038086 chip=0x10fb8086 rev=0x01 hdr=0x00
    ix3@pci0:2:0:1: class=0x020000 card=0x00038086 chip=0x10fb8086 rev=0x01 hdr=0x00

The first column constitutes three components:

#.  Device name: ixN

#.  Unit name:  pci0

#.  Selector (Bus:Device:Function):   1:0:0

Where no driver is associated with a device, the device name will be none.

By default, the FreeBSD* kernel will include built-in drivers for the most common
devices; a kernel rebuild would normally be required to either remove the drivers
or configure them as loadable modules.

To avoid building a custom kernel, the nic_uio module can detach a network port
from its current device driver.  This is achieved by setting the hw.nic_uio.bdfs
kernel environment variable prior to loading nic_uio, as follows:

::

    hw.nic_uio.bdfs="b:d:f,b:d:f,..."

Where a comma separated list of selectors is set, the list must not contain any
whitespace.

For example to re-bind ix2@pci0:2:0:0 and ix3@pci0:2:0: to the nic_uio module
upon loading, use the following command:

.. code-block:: console

    kenv hw.nic_uio.bdfs="2:0:0,2:0:1"

The variable can also be specified during boot by placing the following into
/boot/loader.conf:

::

    hw.nic_uio.bdfs="2:0:0,2:0:1"

To restore the original device binding, it is necessary to reboot FreeBSD* if the
original driver has been compiled into the kernel.

For example to rebind some or all ports to the original driver:

Update or remove the hw.nic_uio.bdfs entry in /boot/loader.conf if specified there
for persistency, then;

.. code-block:: console

    reboot

If rebinding to a driver that is a loadable module, the network port binding can
be reset without rebooting.  This requires the unloading of the nic_uio module
and the original driver.

Update or remove the hw.nic_uio.bdfs entry from /boot/loader.conf if specified
there for persistency.

.. code-block:: console

    kldunload nic_uio

kldunload <original_driver>

.. code-block:: console

    kenv -u hw.nic_uio.bdfs

to remove all network ports from nic_uio and undefined this system variable OR

.. code-block:: console

    kenv hw.nic_uio.bdfs="b:d:f,b:d:f,..."

(to update nic_uio ports)

.. code-block:: console

    kldload <original_driver>
    kldload nic_uio

(if updating the list of associated network ports)
