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

.. _linux_gsg_compiling_dpdk:

Compiling the DPDK Target from Source
=====================================

.. note::

    Parts of this process can also be done using the setup script described in
    the :ref:`linux_setup_script` section of this document.

Install the DPDK and Browse Sources
-----------------------------------

First, uncompress the archive and move to the uncompressed DPDK source directory:

.. code-block:: console

    unzip DPDK-<version>.zip
    cd DPDK-<version>

The DPDK is composed of several directories:

*   lib: Source code of DPDK libraries

*   drivers: Source code of DPDK poll-mode drivers

*   app: Source code of DPDK applications (automatic tests)

*   examples: Source code of DPDK application examples

*   config, tools, scripts, mk: Framework-related makefiles, scripts and configuration

Installation of DPDK Target Environments
----------------------------------------

The format of a DPDK target is::

    ARCH-MACHINE-EXECENV-TOOLCHAIN

where:

* ``ARCH`` can be:  ``i686``, ``x86_64``, ``ppc_64``

* ``MACHINE`` can be:  ``native``, ``power8``

* ``EXECENV`` can be:  ``linuxapp``,  ``bsdapp``

* ``TOOLCHAIN`` can be:  ``gcc``,  ``icc``

The targets to be installed depend on the 32-bit and/or 64-bit packages and compilers installed on the host.
Available targets can be found in the DPDK/config directory.
The defconfig\_ prefix should not be used.

.. note::

    Configuration files are provided with the ``RTE_MACHINE`` optimization level set.
    Within the configuration files, the ``RTE_MACHINE`` configuration value is set to native,
    which means that the compiled software is tuned for the platform on which it is built.
    For more information on this setting, and its possible values, see the *DPDK Programmers Guide*.

When using the Intel® C++ Compiler (icc), one of the following commands should be invoked for 64-bit or 32-bit use respectively.
Notice that the shell scripts update the ``$PATH`` variable and therefore should not be performed in the same session.
Also, verify the compiler's installation directory since the path may be different:

.. code-block:: console

    source /opt/intel/bin/iccvars.sh intel64
    source /opt/intel/bin/iccvars.sh ia32

To install and make targets, use the ``make install T=<target>`` command in the top-level DPDK directory.

For example, to compile a 64-bit target using icc, run:

.. code-block:: console

    make install T=x86_64-native-linuxapp-icc

To compile a 32-bit build using gcc, the make command should be:

.. code-block:: console

    make install T=i686-native-linuxapp-gcc

To prepare a target without building it, for example, if the configuration changes need to be made before compilation,
use the ``make config T=<target>`` command:

.. code-block:: console

    make config T=x86_64-native-linuxapp-gcc

.. warning::

    Any kernel modules to be used, e.g. ``igb_uio``, ``kni``, must be compiled with the
    same kernel as the one running on the target.
    If the DPDK is not being built on the target machine,
    the ``RTE_KERNELDIR`` environment variable should be used to point the compilation at a copy of the kernel version to be used on the target machine.

Once the target environment is created, the user may move to the target environment directory and continue to make code changes and re-compile.
The user may also make modifications to the compile-time DPDK configuration by editing the .config file in the build directory.
(This is a build-local copy of the defconfig file from the top- level config directory).

.. code-block:: console

    cd x86_64-native-linuxapp-gcc
    vi .config
    make

In addition, the make clean command can be used to remove any existing compiled files for a subsequent full, clean rebuild of the code.

Browsing the Installed DPDK Environment Target
----------------------------------------------

Once a target is created it contains all libraries, including poll-mode drivers, and header files for the DPDK environment that are required to build customer applications.
In addition, the test and testpmd applications are built under the build/app directory, which may be used for testing.
A kmod  directory is also present that contains kernel modules which may be loaded if needed.

Loading Modules to Enable Userspace IO for DPDK
-----------------------------------------------

To run any DPDK application, a suitable uio module can be loaded into the running kernel.
In many cases, the standard ``uio_pci_generic`` module included in the Linux kernel
can provide the uio capability. This module can be loaded using the command

.. code-block:: console

    sudo modprobe uio_pci_generic

As an alternative to the ``uio_pci_generic``, the DPDK also includes the igb_uio
module which can be found in the kmod subdirectory referred to above. It can
be loaded as shown below:

.. code-block:: console

    sudo modprobe uio
    sudo insmod kmod/igb_uio.ko

.. note::

    For some devices which lack support for legacy interrupts, e.g. virtual function
    (VF) devices, the ``igb_uio`` module may be needed in place of ``uio_pci_generic``.

Since DPDK release 1.7 onward provides VFIO support, use of UIO is optional
for platforms that support using VFIO.

Loading VFIO Module
-------------------

To run an DPDK application and make use of VFIO, the ``vfio-pci`` module must be loaded:

.. code-block:: console

    sudo modprobe vfio-pci

Note that in order to use VFIO, your kernel must support it.
VFIO kernel modules have been included in the Linux kernel since version 3.6.0 and are usually present by default,
however please consult your distributions documentation to make sure that is the case.

Also, to use VFIO, both kernel and BIOS must support and be configured to use IO virtualization (such as Intel® VT-d).

For proper operation of VFIO when running DPDK applications as a non-privileged user, correct permissions should also be set up.
This can be done by using the DPDK setup script (called dpdk-setup.sh and located in the tools directory).

.. _linux_gsg_binding_kernel:

Binding and Unbinding Network Ports to/from the Kernel Modules
--------------------------------------------------------------

As of release 1.4, DPDK applications no longer automatically unbind all supported network ports from the kernel driver in use.
Instead, all ports that are to be used by an DPDK application must be bound to the
``uio_pci_generic``, ``igb_uio`` or ``vfio-pci`` module before the application is run.
Any network ports under Linux* control will be ignored by the DPDK poll-mode drivers and cannot be used by the application.

.. warning::

    The DPDK will, by default, no longer automatically unbind network ports from the kernel driver at startup.
    Any ports to be used by an DPDK application must be unbound from Linux* control and
    bound to the ``uio_pci_generic``, ``igb_uio`` or ``vfio-pci`` module before the application is run.

To bind ports to the ``uio_pci_generic``, ``igb_uio`` or ``vfio-pci`` module for DPDK use,
and then subsequently return ports to Linux* control,
a utility script called dpdk_nic _bind.py is provided in the tools subdirectory.
This utility can be used to provide a view of the current state of the network ports on the system,
and to bind and unbind those ports from the different kernel modules, including the uio and vfio modules.
The following are some examples of how the script can be used.
A full description of the script and its parameters can be obtained by calling the script with the ``--help`` or ``--usage`` options.
Note that the uio or vfio kernel modules to be used, should be loaded into the kernel before
running the ``dpdk-devbind.py`` script.

.. warning::

    Due to the way VFIO works, there are certain limitations to which devices can be used with VFIO.
    Mainly it comes down to how IOMMU groups work.
    Any Virtual Function device can be used with VFIO on its own, but physical devices will require either all ports bound to VFIO,
    or some of them bound to VFIO while others not being bound to anything at all.

    If your device is behind a PCI-to-PCI bridge, the bridge will then be part of the IOMMU group in which your device is in.
    Therefore, the bridge driver should also be unbound from the bridge PCI device for VFIO to work with devices behind the bridge.

.. warning::

    While any user can run the dpdk-devbind.py script to view the status of the network ports,
    binding or unbinding network ports requires root privileges.

To see the status of all network ports on the system:

.. code-block:: console

    ./tools/dpdk-devbind.py --status

    Network devices using DPDK-compatible driver
    ============================================
    0000:82:00.0 '82599EB 10-GbE NIC' drv=uio_pci_generic unused=ixgbe
    0000:82:00.1 '82599EB 10-GbE NIC' drv=uio_pci_generic unused=ixgbe

    Network devices using kernel driver
    ===================================
    0000:04:00.0 'I350 1-GbE NIC' if=em0  drv=igb unused=uio_pci_generic *Active*
    0000:04:00.1 'I350 1-GbE NIC' if=eth1 drv=igb unused=uio_pci_generic
    0000:04:00.2 'I350 1-GbE NIC' if=eth2 drv=igb unused=uio_pci_generic
    0000:04:00.3 'I350 1-GbE NIC' if=eth3 drv=igb unused=uio_pci_generic

    Other network devices
    =====================
    <none>

To bind device ``eth1``,``04:00.1``, to the ``uio_pci_generic`` driver:

.. code-block:: console

    ./tools/dpdk-devbind.py --bind=uio_pci_generic 04:00.1

or, alternatively,

.. code-block:: console

    ./tools/dpdk-devbind.py --bind=uio_pci_generic eth1

To restore device ``82:00.0`` to its original kernel binding:

.. code-block:: console

    ./tools/dpdk-devbind.py --bind=ixgbe 82:00.0
