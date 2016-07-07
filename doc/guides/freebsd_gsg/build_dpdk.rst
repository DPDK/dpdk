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

.. _building_from_source:

Compiling the DPDK Target from Source
=====================================

System Requirements
-------------------

The DPDK and its applications require the GNU make system (gmake)
to build on FreeBSD. Optionally, gcc may also be used in place of clang
to build the DPDK, in which case it too must be installed prior to
compiling the DPDK. The installation of these tools is covered in this
section.

Compiling the DPDK requires the FreeBSD kernel sources, which should be
included during the installation of FreeBSD on the development platform.
The DPDK also requires the use of FreeBSD ports to compile and function.

To use the FreeBSD ports system, it is required to update and extract the FreeBSD
ports tree by issuing the following commands:

.. code-block:: console

    portsnap fetch
    portsnap extract

If the environment requires proxies for external communication, these can be set
using:

.. code-block:: console

    setenv http_proxy <my_proxy_host>:<port>
    setenv ftp_proxy <my_proxy_host>:<port>

The FreeBSD ports below need to be installed prior to building the DPDK.
In general these can be installed using the following set of commands::

   cd /usr/ports/<port_location>

   make config-recursive

   make install

   make clean

Each port location can be found using::

   whereis <port_name>

The ports required and their locations are as follows:

* dialog4ports: ``/usr/ports/ports-mgmt/dialog4ports``

* GNU make(gmake): ``/usr/ports/devel/gmake``

* coreutils: ``/usr/ports/sysutils/coreutils``

For compiling and using the DPDK with gcc, the compiler must be installed
from the ports collection:

* gcc: version 4.8 is recommended ``/usr/ports/lang/gcc48``.
  Ensure that ``CPU_OPTS`` is selected (default is OFF).

When running the make config-recursive command, a dialog may be presented to the
user. For the installation of the DPDK, the default options were used.

.. note::

    To avoid multiple dialogs being presented to the user during make install,
    it is advisable before running the make install command to re-run the
    make config-recursive command until no more dialogs are seen.


Install the DPDK and Browse Sources
-----------------------------------

First, uncompress the archive and move to the DPDK source directory:

.. code-block:: console

    unzip DPDK-<version>.zip
    cd DPDK-<version>

    ls
    app/ config/ examples/ lib/ LICENSE.GPL LICENSE.LGPL Makefile
    mk/ scripts/ tools/

The DPDK is composed of several directories:

*   lib: Source code of DPDK libraries

*   app: Source code of DPDK applications (automatic tests)

*   examples: Source code of DPDK applications

*   config, tools, scripts, mk: Framework-related makefiles, scripts and configuration

Installation of the DPDK Target Environments
--------------------------------------------

The format of a DPDK target is::

   ARCH-MACHINE-EXECENV-TOOLCHAIN

Where:

* ``ARCH`` is: ``x86_64``

* ``MACHINE`` is: ``native``

* ``EXECENV`` is: ``bsdapp``

* ``TOOLCHAIN`` is: ``gcc`` | ``clang``

The configuration files for the DPDK targets can be found in the DPDK/config
directory in the form of::

    defconfig_ARCH-MACHINE-EXECENV-TOOLCHAIN

.. note::

   Configuration files are provided with the ``RTE_MACHINE`` optimization level set.
   Within the configuration files, the ``RTE_MACHINE`` configuration value is set
   to native, which means that the compiled software is tuned for the platform
   on which it is built.  For more information on this setting, and its
   possible values, see the *DPDK Programmers Guide*.

To make the target, use ``gmake install T=<target>``.

For example to compile for FreeBSD use:

.. code-block:: console

    gmake install T=x86_64-native-bsdapp-clang

.. note::

   If the compiler binary to be used does not correspond to that given in the
   TOOLCHAIN part of the target, the compiler command may need to be explicitly
   specified. For example, if compiling for gcc, where the gcc binary is called
   gcc4.8, the command would need to be ``gmake install T=<target> CC=gcc4.8``.

Browsing the Installed DPDK Environment Target
----------------------------------------------

Once a target is created, it contains all the libraries and header files for the
DPDK environment that are required to build customer applications.
In addition, the test and testpmd applications are built under the build/app
directory, which may be used for testing.  A kmod directory is also present that
contains the kernel modules to install:

.. code-block:: console

    ls x86_64-native-bsdapp-gcc

    app build include kmod lib Makefile


.. _loading_contigmem:

Loading the DPDK contigmem Module
---------------------------------

To run a DPDK application, physically contiguous memory is required.
In the absence of non-transparent superpages, the included sources for the
contigmem kernel module provides the ability to present contiguous blocks of
memory for the DPDK to use. The contigmem module must be loaded into the
running kernel before any DPDK is run.  The module is found in the kmod
sub-directory of the DPDK target directory.

The amount of physically contiguous memory along with the number of physically
contiguous blocks to be reserved by the module can be set at runtime prior to
module loading using:

.. code-block:: console

    kenv hw.contigmem.num_buffers=n
    kenv hw.contigmem.buffer_size=m

The kernel environment variables can also be specified during boot by placing the
following in ``/boot/loader.conf``::

    hw.contigmem.num_buffers=n hw.contigmem.buffer_size=m

The variables can be inspected using the following command:

.. code-block:: console

    sysctl -a hw.contigmem

Where n is the number of blocks and m is the size in bytes of each area of
contiguous memory.  A default of two buffers of size 1073741824 bytes (1 Gigabyte)
each is set during module load if they are not specified in the environment.

The module can then be loaded using kldload (assuming that the current directory
is the DPDK target directory):

.. code-block:: console

    kldload ./kmod/contigmem.ko

It is advisable to include the loading of the contigmem module during the boot
process to avoid issues with potential memory fragmentation during later system
up time.  This can be achieved by copying the module to the ``/boot/kernel/``
directory and placing the following into ``/boot/loader.conf``::

    contigmem_load="YES"

.. note::

    The contigmem_load directive should be placed after any definitions of
    ``hw.contigmem.num_buffers`` and ``hw.contigmem.buffer_size`` if the default values
    are not to be used.

An error such as:

.. code-block:: console

    kldload: can't load ./x86_64-native-bsdapp-gcc/kmod/contigmem.ko:
             Exec format error

is generally attributed to not having enough contiguous memory
available and can be verified via dmesg or ``/var/log/messages``:

.. code-block:: console

    kernel: contigmalloc failed for buffer <n>

To avoid this error, reduce the number of buffers or the buffer size.

.. _loading_nic_uio:

Loading the DPDK nic_uio Module
-------------------------------

After loading the contigmem module, the ``nic_uio`` module must also be loaded into the
running kernel prior to running any DPDK application.  This module must
be loaded using the kldload command as shown below (assuming that the current
directory is the DPDK target directory).

.. code-block:: console

    kldload ./kmod/nic_uio.ko

.. note::

    If the ports to be used are currently bound to a existing kernel driver
    then the ``hw.nic_uio.bdfs sysctl`` value will need to be set before loading the
    module. Setting this value is described in the next section below.

Currently loaded modules can be seen by using the ``kldstat`` command and a module
can be removed from the running kernel by using ``kldunload <module_name>``.

To load the module during boot, copy the ``nic_uio`` module to ``/boot/kernel``
and place the following into ``/boot/loader.conf``::

    nic_uio_load="YES"

.. note::

    ``nic_uio_load="YES"`` must appear after the contigmem_load directive, if it exists.

By default, the ``nic_uio`` module will take ownership of network ports if they are
recognized DPDK devices and are not owned by another module. However, since
the FreeBSD kernel includes support, either built-in, or via a separate driver
module, for most network card devices, it is likely that the ports to be used are
already bound to a driver other than ``nic_uio``. The following sub-section describe
how to query and modify the device ownership of the ports to be used by
DPDK applications.

.. _binding_network_ports:

Binding Network Ports to the nic_uio Module
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Device ownership can be viewed using the pciconf -l command. The example below shows
four Intel® 82599 network ports under ``if_ixgbe`` module ownership.

.. code-block:: console

    pciconf -l
    ix0@pci0:1:0:0: class=0x020000 card=0x00038086 chip=0x10fb8086 rev=0x01 hdr=0x00
    ix1@pci0:1:0:1: class=0x020000 card=0x00038086 chip=0x10fb8086 rev=0x01 hdr=0x00
    ix2@pci0:2:0:0: class=0x020000 card=0x00038086 chip=0x10fb8086 rev=0x01 hdr=0x00
    ix3@pci0:2:0:1: class=0x020000 card=0x00038086 chip=0x10fb8086 rev=0x01 hdr=0x00

The first column constitutes three components:

#. Device name: ``ixN``

#. Unit name: ``pci0``

#. Selector (Bus:Device:Function): ``1:0:0``

Where no driver is associated with a device, the device name will be ``none``.

By default, the FreeBSD kernel will include built-in drivers for the most common
devices; a kernel rebuild would normally be required to either remove the drivers
or configure them as loadable modules.

To avoid building a custom kernel, the ``nic_uio`` module can detach a network port
from its current device driver. This is achieved by setting the ``hw.nic_uio.bdfs``
kernel environment variable prior to loading ``nic_uio``, as follows::

    hw.nic_uio.bdfs="b:d:f,b:d:f,..."

Where a comma separated list of selectors is set, the list must not contain any
whitespace.

For example to re-bind ``ix2@pci0:2:0:0`` and ``ix3@pci0:2:0:1`` to the ``nic_uio`` module
upon loading, use the following command::

    kenv hw.nic_uio.bdfs="2:0:0,2:0:1"

The variable can also be specified during boot by placing the following into
``/boot/loader.conf``, before the previously-described ``nic_uio_load`` line - as
shown::

    hw.nic_uio.bdfs="2:0:0,2:0:1"
    nic_uio_load="YES"

Binding Network Ports Back to their Original Kernel Driver
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If the original driver for a network port has been compiled into the kernel,
it is necessary to reboot FreeBSD to restore the original device binding. Before
doing so, update or remove the ``hw.nic_uio.bdfs`` in ``/boot/loader.conf``.

If rebinding to a driver that is a loadable module, the network port binding can
be reset without rebooting. To do so, unload both the target kernel module and the
``nic_uio`` module, modify or clear the ``hw.nic_uio.bdfs`` kernel environment (kenv)
value, and reload the two drivers - first the original kernel driver, and then
the ``nic_uio driver``. Note: the latter does not need to be reloaded unless there are
ports that are still to be bound to it.

Example commands to perform these steps are shown below:

.. code-block:: console

    kldunload nic_uio
    kldunload <original_driver>

    # To clear the value completely:
    kenv -u hw.nic_uio.bdfs

    # To update the list of ports to bind:
    kenv hw.nic_uio.bdfs="b:d:f,b:d:f,..."

    kldload <original_driver>

    kldload nic_uio  # optional
