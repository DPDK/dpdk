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

System Requirements
===================

This chapter describes the packages required to compile the Intel® DPDK.

Compilationofthe Intel® DPDK
----------------------------

.. note::

    The Intel® DPDK and its applications requires the GNU make system (gmake)
    and the GNU Compiler Collection (gcc) to build on FreeBSD*.  The
    installation of these tools is covered in this section.

**Required Tools:**

.. note::

    Testing has been performed using FreeBSD* 9.2-RELEASE (x86_64), FreeBSD*
    10.0-RELEASE (x86_64) and requires the installation of the kernel sources,
    which should be included during the installation of FreeBSD*.  The Intel®
    DPDK also requires the use of FreeBSD* ports to compile and function.

To use the FreeBSD* ports system, it is required to update and extract the FreeBSD*
ports tree by issuing the following commands:

.. code-block:: console

    root@host:~ # portsnap fetch
    root@host:~ # portsnap extract

If the environment requires proxies for external communication, these can be set
using:

.. code-block:: console

    root@host:~ # setenv http_proxy <my_proxy_host>:<port>
    root@host:~ # setenv ftp_proxy <my_proxy_host>:<port>

The FreeBSD* ports below need to be installed prior to building the Intel® DPDK.
In general these can be installed using the following set of commands:

#.  cd /usr/ports/<port_location>

#.  make config-recursive

#.  make install

#.  make clean

Each port location can be found using:

.. code-block:: console

    user@host:~ # whereis <port_name>

The ports required and their locations are as follows:

*   dialog4ports

*   /usr/ports/ports-mgmt/dialog4ports

*   gcc: version 4.8 is recommended

*   /usr/ports/lang/gcc48

*   Ensure that CPU_OPTS is selected (default is OFF)

*   GNU make(gmake)

*   Installed automatically with gcc48

*   coreutils

*   /usr/ports/sysutils/coreutils

*   libexecinfo  (Not required for FreeBSD* 10)

*   /usr/src/contrib/libexecinfo

When running the make config-recursive command, a dialog may be presented to the
user. For the installation of the Intel® DPDK, the default options were used.

.. note::

    To avoid multiple dialogs being presented to the user during make install,
    it is advisable before running the make install command to re-run the
    make config -recursive command until no more dialogs are seen.

Running Intel® DPDK Applications
--------------------------------

To run an Intel® DPDK application, physically contiguous memory is required.
In the absence of non-transparent superpages, the included sources for the
contigmem kernel module provides the ability to present contiguous blocks of
memory for the Intel® DPDK to use.  Section 3.4, “Loading the Intel® DPDK
Contigmem Module” on page 8 for details on the loading of this module.

Using Intel® DPDK Contigmem Module
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The amount of physically contiguous memory along with the number of physically
contiguous blocks can be set at runtime and prior to module loading using:

.. code-block:: console

    root@host:~ # kenv hw.contigmem.num_buffers=n
    root@host:~ # kenv hw.contigmem.buffer_size=m

The kernel environment variables can also be specified during boot by placing the
following in /boot/loader.conf:

::

    hw.contigmem.num_buffers=n hw.contigmem.buffer_size=m

The variables can be inspected using the following command:

.. code-block:: console

    root@host:~ # sysctl -a hw.contigmem

Where n is the number of blocks and m is the size in bytes of each area of
contiguous memory.  A default of two buffers of size 1073741824 bytes (1 Gigabyte)
each is set during module load if they are not specified in the environment.

.. note::

    The /boot/loader.conf file may not exist, but can be created as a root user
    and should be given permissions as follows:

.. code-block:: console

        root@host:~ # chmod 644 /boot/loader.conf
